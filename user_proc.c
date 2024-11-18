// user_proc.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>

#define MSG_KEY 5678
#define MAX_RESOURCES 10

// Message structure
struct Message {
    long mtype;
    int processID;
    int action;  // 0 = request, 1 = release, 2 = terminate
    int resourceID;
};

int main() {
    srand(getpid());  // Seed random number generator with process ID for variability

    int msgid = msgget(MSG_KEY, 0666);
    if (msgid == -1) {
        perror("Failed to access message queue");
        exit(1);
    }

    struct Message msg;
    msg.mtype = 1;
    msg.processID = getpid();

    // Simulate resource requests/releases
    for (int i = 0; i < 5; i++) {
        msg.action = rand() % 2;             // 0 = request, 1 = release
        msg.resourceID = rand() % MAX_RESOURCES;

        // Send the message to OSS
        if (msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
            perror("Failed to send message");
            exit(1);
        }

        printf("Process %d %s resource %d\n", getpid(),
               msg.action == 0 ? "requesting" : "releasing", msg.resourceID);
        sleep(1);  // Sleep to simulate time between actions
    }

    // Send termination message
    msg.action = 2;  // Termination action
    if (msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
        perror("Failed to send termination message");
        exit(1);
    }

    printf("Process %d finished\n", getpid());
    return 0;
}

