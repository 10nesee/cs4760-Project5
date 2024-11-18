// oss.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#define SHM_KEY 1234
#define MSG_KEY 5678
#define MAX_RESOURCES 10
#define INSTANCES_PER_RESOURCE 20
#define MAX_CHILDREN 18

// Clock structure
struct Clock {
    int seconds;
    int nanoseconds;
};

// Resource Descriptor structure
struct ResourceDescriptor {
    int total;                    // Total instances of the resource
    int available;                // Available instances
    int allocated[MAX_CHILDREN];  // Track allocated instances for each process
};

// Message structure for communication
struct Message {
    long mtype;
    int processID;
    int action;      // 0 = request, 1 = release, 2 = terminate
    int resourceID;
    bool blocked;    // Indicates if the process is blocked
};

// Global variables
struct ResourceDescriptor resources[MAX_RESOURCES];
FILE *logFile;
int verbose = 0;  // Verbose logging flag

// Statistics counters
int totalRequests = 0;
int totalReleases = 0;
int totalTerminations = 0;
int totalDeadlockResolutions = 0;

// Function declarations
void initializeResources();
void logEvent(const char *format, ...);
void logResourceTable(struct Clock *clock);
void incrementClock(struct Clock *clock, int nano_increment);
void detectAndResolveDeadlock(int msgid);
void logFinalStatistics();  // New function for final statistics logging

// Function to initialize resources
void initializeResources() {
    for (int i = 0; i < MAX_RESOURCES; i++) {
        resources[i].total = INSTANCES_PER_RESOURCE;
        resources[i].available = INSTANCES_PER_RESOURCE;
        for (int j = 0; j < MAX_CHILDREN; j++) {
            resources[i].allocated[j] = 0;
        }
    }
    printf("Resources initialized: Each resource has %d instances.\n", INSTANCES_PER_RESOURCE);
}

// Logging function with optional verbose mode
void logEvent(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);  // Write to log file
    if (verbose) {
        vfprintf(stdout, format, args);  // Optionally print to console if verbose is enabled
    }
    va_end(args);
}

// Log resource and process tables every half second
void logResourceTable(struct Clock *clock) {
    logEvent("\nTime %d:%d - Resource Table\n", clock->seconds, clock->nanoseconds);
    logEvent("Resource | Available | Allocated (per process)\n");
    for (int i = 0; i < MAX_RESOURCES; i++) {
        logEvent("R%d       | %d         | ", i, resources[i].available);
        for (int j = 0; j < MAX_CHILDREN; j++) {
            logEvent("%d ", resources[i].allocated[j]);
        }
        logEvent("\n");
    }
    logEvent("\n");
}

// Function to increment the clock
void incrementClock(struct Clock *clock, int nano_increment) {
    clock->nanoseconds += nano_increment;
    if (clock->nanoseconds >= 1000000000) {
        clock->seconds += 1;
        clock->nanoseconds -= 1000000000;
    }
}

// Function to detect and resolve deadlock
void detectAndResolveDeadlock(int msgid) {
    struct Message msg;
    bool deadlock = false;
    int processID_to_terminate = -1;

    for (int i = 0; i < MAX_CHILDREN; i++) {
        for (int j = 0; j < MAX_RESOURCES; j++) {
            if (resources[j].allocated[i] > 0 && resources[j].available < resources[j].total) {
                deadlock = true;
                processID_to_terminate = i;
                break;
            }
        }
        if (deadlock) {
            break;
        }
    }

    if (deadlock && processID_to_terminate != -1) {
        logEvent("Deadlock detected! Terminating process %d to resolve deadlock\n", processID_to_terminate);
        totalDeadlockResolutions++;  // Increment deadlock resolution counter

        msg.mtype = 1;
        msg.action = 2;
        msg.processID = processID_to_terminate;

        if (msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
            perror("Failed to send termination message");
        }
    }
}

// Log final statistics at the end of the simulation
void logFinalStatistics() {
    logEvent("\n--- Simulation Summary ---\n");
    logEvent("Total resource requests: %d\n", totalRequests);
    logEvent("Total resource releases: %d\n", totalReleases);
    logEvent("Total process terminations: %d\n", totalTerminations);
    logEvent("Total deadlock resolutions: %d\n", totalDeadlockResolutions);
    logEvent("--------------------------\n");
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        }
    }

    logFile = fopen("oss_log.txt", "w");
    if (logFile == NULL) {
        perror("Failed to open log file");
        exit(1);
    }

    int shmid = shmget(SHM_KEY, sizeof(struct Clock), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Failed to create shared memory segment");
        exit(1);
    }
    struct Clock *clock = (struct Clock *)shmat(shmid, NULL, 0);
    if (clock == (void *)-1) {
        perror("Failed to attach shared memory segment");
        exit(1);
    }

    clock->seconds = 0;
    clock->nanoseconds = 0;

    initializeResources();

    int msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("Failed to create message queue");
        exit(1);
    }

    pid_t pids[MAX_CHILDREN];
    int childCount = 0;
    for (int i = 0; i < MAX_CHILDREN; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("Failed to fork process");
            exit(1);
        } else if (pids[i] == 0) {
            execl("./user_proc", "user_proc", NULL);
            perror("Failed to exec user_proc");
            exit(1);
        } else {
            childCount++;
        }
    }

    struct Message msg;
    int lastLogTime = 0;

    while (1) {
        incrementClock(clock, 500000000);  // Increment by 0.5 seconds

        if (clock->seconds > lastLogTime) {
            logResourceTable(clock);  // Log every half second
            detectAndResolveDeadlock(msgid);  // Check for deadlock every second
            lastLogTime = clock->seconds;
        }

        if (msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 0, 0) == -1) {
            perror("Failed to receive message");
            break;
        }

        if (msg.action == 0) {  // Resource request
            totalRequests++;
            logEvent("OSS: Process %d requesting resource %d\n", msg.processID, msg.resourceID);
        } else if (msg.action == 1) {  // Resource release
            totalReleases++;
            logEvent("OSS: Process %d releasing resource %d\n", msg.processID, msg.resourceID);
        } else if (msg.action == 2) {  // Process termination
            totalTerminations++;
            logEvent("OSS: Process %d is terminating\n", msg.processID);
        }
    }

    for (int i = 0; i < childCount; i++) {
        waitpid(pids[i], NULL, 0);
    }

    logFinalStatistics();  // Log final summary statistics

    shmdt(clock);
    shmctl(shmid, IPC_RMID, NULL);
    msgctl(msgid, IPC_RMID, NULL);
    fclose(logFile);

    return 0;
}

