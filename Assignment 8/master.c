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

// Define probability of illegal address
#define PROB_ILLEGAL_ADDR 0.1

// Maximum virtual address space and maximum number of processes
#define MAX_VIRTUAL_ADDR_SPACE 25
#define MAX_PROCESSES 100

// Define P() and V() macros for semaphore operations
#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

// Structure for Shared Memory 1 (SM1)
typedef struct SM1
{
    int pid;                    // Process ID
    int mi;                     // Number of required pages

    // Page table structure:
    // pagetable[page][0] -> frame allocated
    // pagetable[page][1] -> valid bit
    // pagetable[page][2] -> timestamp
    int pagetable[MAX_VIRTUAL_ADDR_SPACE][3];

    int total_page_faults;      // Total number of page faults
    int total_illegal_access;   // Total number of illegal accesses
} SM1;

// Global variables initialization
int sched_pid = -1;
int mmu_pid = -1;
int pid_mmu = -1;
int sm1_id = -1, sm2_id = -1;
SM1 *sm1 = NULL;
int *sm2 = NULL;
int msg_id1 = -1, msg_id2 = -1, msg_id3 = -1;
int sync_sem = -1;

// Function to cleanup resources on exit
void cleanup_on_exit() {
    if (sched_pid > 0) kill(sched_pid, SIGINT);
    if (pid_mmu > 0) kill(pid_mmu, SIGINT);
    if (sm1 != NULL) shmdt(sm1);
    if (sm2 != NULL) shmdt(sm2);
    if (sm1_id > 0) shmctl(sm1_id, IPC_RMID, NULL);
    if (sm2_id > 0) shmctl(sm2_id, IPC_RMID, NULL);
    if (msg_id1 > 0) msgctl(msg_id1, IPC_RMID, NULL);
    if (msg_id2 > 0) msgctl(msg_id2, IPC_RMID, NULL);
    if (msg_id3 > 0) msgctl(msg_id3, IPC_RMID, NULL);
    if (sync_sem > 0) semctl(sync_sem, 0, IPC_RMID, 0);
}

// Signal handler function
void sighand(int signo) {
    if (signo == SIGINT || signo == SIGQUIT) {
        cleanup_on_exit();
        exit(1);
    }
}

// Structure for message type 2
typedef struct message2 {
    long mtype; // Message type
    int pid;    // Process ID
} Msg2;



int main(){

    // Set up signal handlers for SIGINT and SIGQUIT
    signal(SIGINT, sighand);
    signal(SIGQUIT, sighand);

    // Initialize random number generator with current time
    srand(time(0));

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

    // Variables for input
    int k, m, f;
    printf("Enter the number of processes (Max. value %d): ", MAX_PROCESSES);
    scanf("%d", &k);
    printf("Enter the Virtual Address space size (Max. value %d): ", MAX_VIRTUAL_ADDR_SPACE);
    scanf("%d", &m);
    printf("Enter the Physical Address space size: ");
    scanf("%d", &f);

    // Create shared memory for page tables of k processes
    key_t key = ftok("mmu.c", 'P');
    sm1_id = shmget(key, k * sizeof(SM1), IPC_CREAT | 0666);
    sm1 = (SM1 *)shmat(sm1_id, NULL, 0);

    // Create shared memory for free frames list
    key = ftok("mmu.c", 'F');
    sm2_id = shmget(key, (f + 1) * sizeof(int), IPC_CREAT | 0666);
    sm2 = (int *)shmat(sm2_id, NULL, 0);

    // Create a semaphore for synchronization between master and scheduler
    //Scheduler notifies master when all the processes are terminated
    key = ftok("mmu.c", 'S'); // Generate a key for the semaphore
    sync_sem = semget(key, 1, IPC_CREAT | 0666); // Create the semaphore
    semctl(sync_sem, 0, SETVAL, 0); // Initialize the semaphore value to 0

    // Create Message Queue 1 for communication between scheduler and process
    key = ftok("master.c", '1');
    msg_id1 = msgget(key, IPC_CREAT | 0666);

    // Create Message Queue 2 for communication between scheduler and mmu
    key = ftok("master.c", '2');
    msg_id2 = msgget(key, IPC_CREAT | 0666);

    // Create Message Queue 3 for communication between mmu and process
    key = ftok("master.c", '3');
    msg_id3 = msgget(key, IPC_CREAT | 0666);


    // Initialize total_page_faults and total_illegal_access to 0 for each process
    for (int i = 0; i < k; i++) {
        sm1[i].total_page_faults = 0;
        sm1[i].total_illegal_access = 0;

        // Initialize page table entries for each process
        for (int j = 0; j < m; j++) {
            sm1[i].pagetable[j][0] = -1;      // No frame allocated
            sm1[i].pagetable[j][1] = 0;       // Invalid
            sm1[i].pagetable[j][2] = INT_MAX; // Timestamp
        }
    }


    // Initialize the frames: 1 means free, 0 means occupied, -1 means end of list
    for (int i = 0; i < f; i++) {
        sm2[i] = 1;
    }
    sm2[f] = -1;


    // Convert the number of processes (k) to a string
    char k_str[15];
    sprintf(k_str, "%d", k);

    // Convert shared memory IDs to strings
    char sm1_id_str[15], sm2_id_str[15];
    sprintf(sm1_id_str, "%d", sm1_id);
    sprintf(sm2_id_str, "%d", sm2_id);

    // Convert message queue IDs to strings
    char msg_id1_str[15], msg_id2_str[15], msg_id3_str[15];
    sprintf(msg_id1_str, "%d", msg_id1);
    sprintf(msg_id2_str, "%d", msg_id2);
    sprintf(msg_id3_str, "%d", msg_id3);


    // Create the scheduler process
    sched_pid = fork(); // Fork a child process
    if (sched_pid == 0) { // If this is the child process
        // Execute the 'sched' program with necessary arguments
        execl("./sched", "./sched", msg_id1_str, msg_id2_str, k_str, NULL);
        // If execl fails, print an error message and exit
        printf("Error in running 'sched' process...\n");
        exit(1);
    }

    // Create the memory management unit (mmu) process
    mmu_pid = fork(); // Fork another child process
    if (mmu_pid == 0) { // If this is the child process
        // Execute the 'mmu' program in a new xterm window with necessary arguments
        execlp("xterm", "xterm", "-T", "Memory Management Unit", "-e", "./mmu", msg_id2_str, msg_id3_str, sm1_id_str, sm2_id_str, NULL);
        // If execlp fails, print an error message and exit
        printf("Error in running 'mmu' process in xterm...\n");
        exit(1);
    }

    // Receive a message from Message Queue 2 (from mmu) to get mmu_pid
    Msg2 msg2;
    msgrcv(msg_id2, (void *)&msg2, sizeof(Msg2), 100, 0);
    pid_mmu = msg2.pid; // Assign the received PID from the message to pid_mmu

    // Array to store reference pages for each process
    int ref_pages[MAX_PROCESSES][MAX_VIRTUAL_ADDR_SPACE * 11];

    // Array to store reference strings for each process
    char reference_str[MAX_PROCESSES][MAX_VIRTUAL_ADDR_SPACE * 110];

    // Initialize reference_str to all zeros
    memset(reference_str, 0, sizeof(reference_str));

    // Loop through each process
    for (int i = 0; i < k; i++) {
        // Generate random number of pages between 1 to m for each process
        sm1[i].mi = (rand() % m) + 1;

        int ref_str_num_pages = 2 * sm1[i].mi + (rand() % (8 * sm1[i].mi + 1));

        // Generate reference string for each process
        for (int j = 0; j < ref_str_num_pages; j++) {
            if ((float)rand() / RAND_MAX < PROB_ILLEGAL_ADDR && m > sm1[i].mi) {
                ref_pages[i][j] = sm1[i].mi + (rand() % (m - sm1[i].mi)); // Generate illegal address with probability
            } else {
                ref_pages[i][j] = rand() % sm1[i].mi; // Generate random page within process's address space
            }  
        }

        char temp_buff[15];
        for (int j = 0; j < ref_str_num_pages; j++) {
            memset(temp_buff, 0, sizeof(temp_buff));
            sprintf(temp_buff, "%d.", ref_pages[i][j]);
            strcat(reference_str[i], temp_buff); // Concatenate each page number to the reference string
        }
    }

    // Create processes
    for (int i = 0; i < k; i++) {
        usleep(250000); // Sleep to stagger process creation

        // Fork a child process
        int pid = fork();
        if (pid == 0) { // If this is the child process
            sm1[i].pid = getpid(); // Set the PID for the process

            // Execute the 'process' program with necessary arguments
            execl("./process", "./process", reference_str[i], msg_id1_str, msg_id3_str, NULL);
            
            // If execl fails, print an error message and exit
            printf("Error in running 'process', quitting this child process...\n");
            exit(1);
        }
    }


    // Wait till scheduler notifies that all the processes have terminated
    P(sync_sem);

    // Print a message indicating that the master received notification from scheduler
    printf("Master received notification from scheduler as all the processes are completed.\n");

    // Clean up resources
    if (sched_pid > 0) kill(sched_pid, SIGINT); // Terminate scheduler process
    if (pid_mmu > 0) kill(pid_mmu, SIGINT); // Terminate memory management unit process
    if (sm1 != NULL) shmdt(sm1); // Detach shared memory segment for page tables
    if (sm2 != NULL) shmdt(sm2); // Detach shared memory segment for free frames list
    if (sm1_id > 0) shmctl(sm1_id, IPC_RMID, NULL); // Remove shared memory segment for page tables
    if (sm2_id > 0) shmctl(sm2_id, IPC_RMID, NULL); // Remove shared memory segment for free frames list
    if (msg_id1 > 0) msgctl(msg_id1, IPC_RMID, NULL); // Remove message queue 1
    if (msg_id2 > 0) msgctl(msg_id2, IPC_RMID, NULL); // Remove message queue 2
    if (msg_id3 > 0) msgctl(msg_id3, IPC_RMID, NULL); // Remove message queue 3
    if (sync_sem > 0) semctl(sync_sem, 0, IPC_RMID, 0); // Remove synchronization semaphore

    // Exit the program
    return 0;

}