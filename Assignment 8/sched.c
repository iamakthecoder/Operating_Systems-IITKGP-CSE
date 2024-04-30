#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>

// Define P() and V() macros for semaphore operations
#define P(s) semop(s, &pop, 1) //for semaphore 'wait' operation
#define V(s) semop(s, &vop, 1) //for semaphore 'signal' operation

// Structure for message type 1
typedef struct message1 {
    long mtype; // Message type
    int pid;    // Process ID
} Msg1;

// Structure for message type 2
typedef struct message2 {
    long mtype; // Message type
    int pid;    // Process ID
} Msg2;


int main(int argc, char *argv[]){
    // Check if the correct number of command-line arguments are provided
    if (argc != 4) {
        printf("Provide these three arguments in order: <Message Queue 1 ID> <Message Queue 2 ID> <No. of Processes>\n");
        exit(1); // Exit program if arguments are not provided correctly
    }

    // Structure for semaphore 'wait' operation
    struct sembuf pop;
    pop.sem_flg = 0;
    pop.sem_num = 0;
    pop.sem_op = -1;
    // Structure for semaphore 'signal' operation
    struct sembuf vop;
    vop.sem_flg = 0;
    vop.sem_num = 0;
    vop.sem_op = 1;

    // Get the process ID
    int pid = getpid();

    // Convert command-line arguments to integers
    int msg_id1 = atoi(argv[1]); // Message Queue 1 ID
    int msg_id2 = atoi(argv[2]); // Message Queue 2 ID
    int k = atoi(argv[3]); // Number of Processes

    // Generate a key for the synchronization semaphore
    key_t key = ftok("mmu.c", 'S');
    // Get the synchronization semaphore
    int sync_sem = semget(key, 1, 0666);


    // Declare message variables
    Msg1 msg1;
    Msg2 msg2; 

    // Loop until all processes are scheduled
    while (k > 0) {
        // Wait for a message from process to schedule itself
        msgrcv(msg_id1, (void *)&msg1, sizeof(Msg1), 1, 0);

        // Print scheduling message
        printf("\t***Scheduling Process with pid: %d\n", msg1.pid);

        // Signal the process to start itself
        msg1.mtype = msg1.pid;
        msgsnd(msg_id1, (void *)&msg1, sizeof(Msg1), 0);

        // Wait for message from mmu
        msgrcv(msg_id2, (void *)&msg2, sizeof(Msg2), 0, 0);

        // Check the type of message from mmu
        if (msg2.mtype == 1) {
            printf("\t***Process with pid %d added to the end of the ready queue\n", msg2.pid);
            // Send message to process to indicate it's added to the ready queue
            msg1.mtype = 1;
            msg1.pid = msg2.pid;
            msgsnd(msg_id1, (void *)&msg1, sizeof(Msg1), 0);
        } else if (msg2.mtype == 2) {
            printf("\t***Process with pid %d terminated.\n", msg2.pid);
            k--; // Decrement the number of processes
        }
    }

    // Print exit message
    printf("\t***Scheduler exiting: all processes are completed.\n");

    // Signal synchronization semaphore to indicate completion to master
    V(sync_sem);


    return 0;
}