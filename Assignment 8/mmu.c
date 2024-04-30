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

// Define constants for maximum virtual address space and maximum processes
#define MAX_VIRTUAL_ADDR_SPACE 25
#define MAX_PROCESSES 100

// Structure for process memory information
typedef struct SM1 {
    int pid;                    // Process id
    int mi;                     // Number of required pages

    // Page table:
    // pagetable[page][0] -> frame allocated
    // pagetable[page][1] -> valid bit
    // pagetable[page][2] -> timestamp
    int pagetable[MAX_VIRTUAL_ADDR_SPACE][3];

    int total_page_faults;      // Total number of page faults
    int total_illegal_access;   // Total number of illegal accesses
} SM1;

// Structure for message type 2
typedef struct message2 {
    long mtype; // Message type
    int pid;    // Process id
} Msg2;

// Structure for message type 3
typedef struct message3 {
    long mtype;     // Message type
    int pid;        // Process id
    int page_frame; // Page frame
} Msg3;

// Global variables
int total_num_processes = 0;
SM1 *sm1 = NULL;
int *sm2 = NULL;
int fd = -1;

// Signal handler function
void sig_handler(int signo) {
    char buff[1001];
    printf("*********************************************\n");
    // Print process information
    if (sm1 != NULL) {
        for (int i = 0; i < total_num_processes; i++) {
            printf("=> Process no. %d (pid: %d):-\n", i + 1, sm1[i].pid);
            printf("\t-Total no. of page faults: %d\n", sm1[i].total_page_faults);
            printf("\t-Total no. of invalid page references: %d\n", sm1[i].total_illegal_access);
        }
    }
    // Write process information to file descriptor if open
    if (fd != -1) {
        sprintf(buff, "*********************************************\n");
        write(fd, buff, strlen(buff));
        for (int i = 0; i < total_num_processes; i++) {
            sprintf(buff, "=> Process no. %d (pid: %d):-\n", i + 1, sm1[i].pid);
            write(fd, buff, strlen(buff));
            sprintf(buff, "\t-Total no. of page faults: %d\n", sm1[i].total_page_faults);
            write(fd, buff, strlen(buff));
            sprintf(buff, "\t-Total no. of invalid page references: %d\n", sm1[i].total_illegal_access);
            write(fd, buff, strlen(buff));
        }
    }
    char c;
    scanf("%c", &c); // Wait for input before exiting
    exit(0); // Exit the program
}


int main(int argc, char *argv[]){
    // Check if the correct number of command-line arguments are provided
    if (argc != 5){
        printf("Provide these four arguments in order: <Message Queue 2 ID> <Message Queue 3 ID> <Shared Memory 1 ID> <Shared Memory 2 ID>\n");
        exit(1);  // Exit program if arguments are not provided correctly
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

    // Set up signal handler for SIGINT
    signal(SIGINT, sig_handler);

    // Open file descriptor for writing results
    fd = open("result.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    char buff[1001];

    // Convert command-line arguments to integers
    int msg_id2 = atoi(argv[1]); // Message Queue 2 ID
    int msg_id3 = atoi(argv[2]); // Message Queue 3 ID
    int shm_id1 = atoi(argv[3]); // Shared Memory 1 ID
    int shm_id2 = atoi(argv[4]); // Shared Memory 2 ID

    // Attach shared memory segments
    sm1 = (SM1 *)shmat(shm_id1, NULL, 0);
    sm2 = (int *)shmat(shm_id2, NULL, 0);

    // Initialize message structures
    Msg2 msg2;
    Msg3 msg3;

    // Set message type and process id for message 2
    msg2.mtype = 100;
    msg2.pid = getpid();
    msgsnd(msg_id2, (void *)&msg2, sizeof(Msg2), 0);


    int timestamp = 0;
    while (1) {
        // Wait for message from process
        msgrcv(msg_id3, (void *)&msg3, sizeof(Msg3), 1, 0);
        timestamp++; // Increment timestamp

        // Find process index corresponding to the received process ID
        int process_idx = 0;
        while (sm1[process_idx].pid != msg3.pid && process_idx < MAX_PROCESSES) {
            process_idx++;
        }
        // Check if process ID is not found in SM1
        if (process_idx == MAX_PROCESSES) {
            printf("=> MMU received a process pid which it could not find in SM1.\n");
            sprintf(buff, "=> MMU received a process pid which it could not find in SM1.\n");
            write(fd, buff, strlen(buff));
            exit(1); // Exit the program
        }

        // Print global ordering information
        printf("Global ordering - (Timestamp %d, Process %d, Page %d)\n", timestamp, process_idx + 1, msg3.page_frame);
        sprintf(buff, "Global ordering - (Timestamp %d, Process %d, Page %d)\n", timestamp, process_idx + 1, msg3.page_frame);
        write(fd, buff, strlen(buff));

        int page = msg3.page_frame;
        if (page == -9) {
            // Handle process termination
            for (int i = 0; i < sm1[process_idx].mi; i++) {
                if (sm1[process_idx].pagetable[i][0] != -1) {
                    sm2[sm1[process_idx].pagetable[i][0]] = 1;
                    sm1[process_idx].pagetable[i][0] = -1;
                    sm1[process_idx].pagetable[i][1] = 0;
                    sm1[process_idx].pagetable[i][2] = INT_MAX;
                }
            }
            // Increment total number of processes
            total_num_processes++;
            // Send termination message to message queue 2 (to scheduler)
            msg2.mtype = 2;
            msg2.pid = msg3.pid;
            msgsnd(msg_id2, (void *)&msg2, sizeof(Msg2), 0);
        } else if (page >= sm1[process_idx].mi) {
            // Handle illegal page reference
            sm1[process_idx].total_illegal_access++;
            printf("Invalid Page Reference - (Process %d, Page %d)\n", process_idx + 1, page);
            sprintf(buff, "Invalid Page Reference - (Process %d, Page %d)\n", process_idx + 1, page);
            write(fd, buff, strlen(buff));
            // Increment total number of processes
            total_num_processes++;
            // Send invalid page reference message to process
            msg3.mtype = msg3.pid;
            msg3.page_frame = -2;
            msgsnd(msg_id3, (void *)&msg3, sizeof(Msg3), 0);
            // Free frames and send termination message to message queue 2 (to scheduler)
            for (int i = 0; i < sm1[process_idx].mi; i++) {
                if (sm1[process_idx].pagetable[i][0] != -1) {
                    sm2[sm1[process_idx].pagetable[i][0]] = 1;
                    sm1[process_idx].pagetable[i][0] = -1;
                    sm1[process_idx].pagetable[i][1] = 0;
                    sm1[process_idx].pagetable[i][2] = INT_MAX;
                }
            }
            msg2.mtype = 2;
            msg2.pid = msg3.pid;
            msgsnd(msg_id2, (void *)&msg2, sizeof(Msg2), 0);
        } else if (sm1[process_idx].pagetable[page][0] != -1 && sm1[process_idx].pagetable[page][1] == 1) {
            // Handle page hit
            sm1[process_idx].pagetable[page][2] = timestamp;
            // Send message with page frame to process (through message queue 3)
            msg3.mtype = msg3.pid;
            msg3.page_frame = sm1[process_idx].pagetable[page][0];
            msgsnd(msg_id3, (void *)&msg3, sizeof(Msg3), 0);
        } else {
            // Handle page fault
            sm1[process_idx].total_page_faults++;
            // Send message indicating page fault to process (through message queue 3)
            msg3.mtype = msg3.pid;
            msg3.page_frame = -1;
            msgsnd(msg_id3, (void *)&msg3, sizeof(Msg3), 0);
            // Print page fault sequence
            printf("Page fault sequence - (Process %d, Page %d)\n", process_idx + 1, page);
            sprintf(buff, "Page fault sequence - (Process %d, Page %d)\n", process_idx + 1, page);
            write(fd, buff, strlen(buff));
            // Find a free frame for page allocation
            int frame = 0;
            while (sm2[frame] != -1) {
                if (sm2[frame] == 1) {
                    sm2[frame] = 0;
                    break;
                }
                frame++;
            }
            // Allocate page to the frame if free frame found
            if (sm2[frame] != -1) {
                sm1[process_idx].pagetable[page][0] = frame;
                sm1[process_idx].pagetable[page][1] = 1;
                sm1[process_idx].pagetable[page][2] = timestamp;
                // Send message to scheduler indicating page fault handled for the process and to enqueue it to the ready queue
                msg2.mtype = 1;
                msg2.pid = msg3.pid;
                msgsnd(msg_id2, (void *)&msg2, sizeof(Msg2), 0);
            } else {
                // Perform LRU replacement if no free frame is available
                int min_time = INT_MAX;
                int idx = -1;
                int req_page = -1;
                for (int i = 0; i < sm1[process_idx].mi; i++) {
                    if (sm1[process_idx].pagetable[i][0] != -1 && sm1[process_idx].pagetable[i][2] < min_time) {
                        min_time = sm1[process_idx].pagetable[i][2];
                        idx = i;
                        req_page = sm1[process_idx].pagetable[i][0];
                    }
                }
                // If the least recently used page is found
                if (idx != -1) {
                    sm1[process_idx].pagetable[page][0] = req_page;
                    sm1[process_idx].pagetable[page][1] = 1;
                    sm1[process_idx].pagetable[page][2] = timestamp;
                    sm1[process_idx].pagetable[idx][0] = -1;
                    sm1[process_idx].pagetable[idx][1] = 0;
                    sm1[process_idx].pagetable[idx][2] = INT_MAX;
                    // Send the message to scheduler indicating page fault handled and to enqueue the process to the ready queue
                    msg2.mtype = 1;
                    msg2.pid = msg3.pid;
                    msgsnd(msg_id2, (void *)&msg2, sizeof(Msg2), 0);
                } else {
                    // If no available page for replacement
                    //Send the message to scheduler to enqueue the process to ready queue to try handling the page fault later
                    msg2.mtype = 1;
                    msg2.pid = msg3.pid;
                    msgsnd(msg_id2, (void *)&msg2, sizeof(Msg2), 0);
                    printf("No page available for LRU replacement - (Process %d, Page %d)\n", process_idx + 1, page);
                    // sprintf(buff, "No page available for LRU replacement - (Process %d, Page %d)\n", process_idx + 1, page);
                    // write(fd, buff, strlen(buff));
                }
            }
        }
    }


    return 0;
}