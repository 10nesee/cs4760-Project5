// oss.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>

#define SHM_KEY 1234  // Shared memory key

struct Clock {
    int seconds;
    int nanoseconds;
};

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

    // Increment clock in a loop for demonstration
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

