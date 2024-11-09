// oss.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>

#define SHM_KEY 1234            // Shared memory key for clock
#define MAX_RESOURCES 10        // Total number of resources
#define INSTANCES_PER_RESOURCE 20 // Instances per resource

// Clock structure
struct Clock {
    int seconds;
    int nanoseconds;
};

// Resource Descriptor structure
struct ResourceDescriptor {
    int total;                    // Total instances of the resource
    int available;                // Available instances
    int allocated[MAX_RESOURCES]; // Track allocated instances for each process
};

// Array to store all resource descriptors
struct ResourceDescriptor resources[MAX_RESOURCES];

// Function to initialize resources
void initializeResources() {
    for (int i = 0; i < MAX_RESOURCES; i++) {
        resources[i].total = INSTANCES_PER_RESOURCE;
        resources[i].available = INSTANCES_PER_RESOURCE;

        // Initialize allocated instances for each process to zero
        for (int j = 0; j < MAX_RESOURCES; j++) {
            resources[i].allocated[j] = 0;
        }
    }
    printf("Resources initialized: Each resource has %d instances.\n", INSTANCES_PER_RESOURCE);
}

// Function to increment the clock
void incrementClock(struct Clock *clock, int nano_increment) {
    clock->nanoseconds += nano_increment;
    if (clock->nanoseconds >= 1000000000) {
        clock->seconds += 1;
        clock->nanoseconds -= 1000000000;
    }
}

int main() {
    int shmid;
    struct Clock *clock;

    // Create shared memory for the clock
    shmid = shmget(SHM_KEY, sizeof(struct Clock), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Failed to create shared memory segment");
        exit(1);
    }

    // Attach to the shared memory
    clock = (struct Clock *)shmat(shmid, NULL, 0);
    if (clock == (void *)-1) {
        perror("Failed to attach shared memory segment");
        exit(1);
    }

    // Initialize the clock
    clock->seconds = 0;
    clock->nanoseconds = 0;

    // Initialize resources
    initializeResources();

    // Increment clock and display resource 
    for (int i = 0; i < 10; i++) {
        incrementClock(clock, 100000000);
        printf("Clock: %d seconds, %d nanoseconds\n", clock->seconds, clock->nanoseconds);
        sleep(1);
    }

    // Detach and remove shared memory
    shmdt(clock);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}

