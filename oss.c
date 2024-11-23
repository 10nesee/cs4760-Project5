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
#include <getopt.h>

#define SHM_KEY 1234
#define MSG_KEY 5678
#define MAX_RESOURCES 10
#define INSTANCES_PER_RESOURCE 20
#define MAX_CHILDREN 18
#define MAX_LOG_LINES 10000

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
struct Clock *clock;
int shmid = -1, msgid = -1;
FILE *logFile = NULL;
int verbose = 0;  // Verbose logging flag
int totalLogLines = 0;  // Track total log lines

// Command-line options
int maxProcesses = 18;  // Default max processes
int intervalMs = 1000;  // Default interval for launching children
char logFileName[256] = "oss_log.txt";  // Default log file

// Function declarations
void initializeResources();
void logEvent(const char *format, ...);
void logResourceTable();
void incrementClock(int nano_increment);
void detectAndResolveDeadlock();
void cleanupResources();
void signalHandler(int signo);
void printHelpMessage();

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

// Logging function with line limit and optional verbose mode
void logEvent(const char *format, ...) {
    if (totalLogLines >= MAX_LOG_LINES) return;

    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);  // Write to log file
    if (verbose) {
        vfprintf(stdout, format, args);  // Optionally print to console if verbose is enabled
    }
    va_end(args);

    totalLogLines++;
}

// Function to log resource table
void logResourceTable() {
    logEvent("\n--- Resource Table ---\n");
    logEvent("Resource | Available | Allocated (per process)\n");
    for (int i = 0; i < MAX_RESOURCES; i++) {
        logEvent("R%d       | %d         | ", i, resources[i].available);
        for (int j = 0; j < MAX_CHILDREN; j++) {
            logEvent("%d ", resources[i].allocated[j]);
        }
        logEvent("\n");
    }
    logEvent("-----------------------\n");
}

// Function to increment the clock
void incrementClock(int nano_increment) {
    clock->nanoseconds += nano_increment;
    if (clock->nanoseconds >= 1000000000) {
        clock->seconds += 1;
        clock->nanoseconds -= 1000000000;
    }
}

// Deadlock detection and resolution function
void detectAndResolveDeadlock() {
    logEvent("Checking for deadlocks at time %d:%d...\n", clock->seconds, clock->nanoseconds);

    // Simplified deadlock detection logic
    bool deadlock = false;
    for (int i = 0; i < MAX_CHILDREN; i++) {
        for (int j = 0; j < MAX_RESOURCES; j++) {
            if (resources[j].allocated[i] > 0 && resources[j].available == 0) {
                deadlock = true;
                logEvent("Deadlock detected. Terminating process %d.\n", i);
                // Free resources
                for (int k = 0; k < MAX_RESOURCES; k++) {
                    resources[k].available += resources[k].allocated[i];
                    resources[k].allocated[i] = 0;
                }
                break;
            }
        }
        if (deadlock) break;
    }
    if (!deadlock) {
        logEvent("No deadlock detected.\n");
    }
}

// Cleanup function
void cleanupResources() {
    if (shmid != -1) {
        shmdt(clock);
        shmctl(shmid, IPC_RMID, NULL);
    }
    if (msgid != -1) {
        msgctl(msgid, IPC_RMID, NULL);
    }
    if (logFile) {
        fclose(logFile);
    }
    printf("Resources cleaned up successfully.\n");
}

// Signal handler for cleanup on termination
void signalHandler(int signo) {
    logEvent("Signal %d received. Cleaning up resources and exiting.\n", signo);
    cleanupResources();
    exit(0);
}

// Print help message
void printHelpMessage() {
    printf("Usage: oss [OPTIONS]\n");
    printf("Options:\n");
    printf("  -h              Display this help message\n");
    printf("  -n proc         Maximum number of processes (default: 18)\n");
    printf("  -i interval     Interval in milliseconds to launch children (default: 1000ms)\n");
    printf("  -f logfile      Log file path (default: oss_log.txt)\n");
    printf("  -v              Enable verbose logging\n");
}

// Main function
int main(int argc, char *argv[]) {
    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "hn:i:f:v")) != -1) {
        switch (opt) {
            case 'h':
                printHelpMessage();
                exit(0);
            case 'n':
                maxProcesses = atoi(optarg);
                break;
            case 'i':
                intervalMs = atoi(optarg);
                break;
            case 'f':
                strncpy(logFileName, optarg, sizeof(logFileName) - 1);
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                fprintf(stderr, "Unknown option. Use -h for help.\n");
                exit(1);
        }
    }

    // Open log file
    logFile = fopen(logFileName, "w");
    if (logFile == NULL) {
        perror("Failed to open log file");
        exit(1);
    }

    // Set up shared memory
    shmid = shmget(SHM_KEY, sizeof(struct Clock), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Failed to create shared memory segment");
        cleanupResources();
        exit(1);
    }
    clock = (struct Clock *)shmat(shmid, NULL, 0);
    if (clock == (void *)-1) {
        perror("Failed to attach shared memory segment");
        cleanupResources();
        exit(1);
    }
    clock->seconds = 0;
    clock->nanoseconds = 0;

    // Set up message queue
    msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("Failed to create message queue");
        cleanupResources();
        exit(1);
    }

    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Initialize resources
    initializeResources();

    int totalGenerated = 0;  // Track total generated processes
    int activeChildren = 0;  // Track active processes

    // Main simulation loop
    while (totalGenerated < maxProcesses || activeChildren > 0) {
        incrementClock(500000000);  // Increment clock by 0.5 seconds

        // Launch a new child process if conditions allow
        if (totalGenerated < maxProcesses && activeChildren < MAX_CHILDREN) {
            pid_t pid = fork();
            if (pid == -1) {
                perror("Failed to fork process");
            } else if (pid == 0) {
                execl("./user_proc", "user_proc", NULL);
                perror("Failed to exec user_proc");
                exit(1);
            } else {
                totalGenerated++;
                activeChildren++;
                logEvent("OSS: Launched process %d (total: %d)\n", pid, totalGenerated);
            }
        }

        // Check for deadlock
        detectAndResolveDeadlock();

        // Wait for child termination
        int status;
        pid_t childPid = waitpid(-1, &status, WNOHANG);
        if (childPid > 0) {
            activeChildren--;
            logEvent("OSS: Process %d terminated. Active children: %d\n", childPid, activeChildren);
        }

        // Log resource table every second
        logResourceTable();

        // Sleep for the specified interval
        usleep(intervalMs * 1000);  // Convert ms to microseconds
    }

    logEvent("OSS: All processes have terminated. Exiting...\n");
    cleanupResources();
    return 0;
}

