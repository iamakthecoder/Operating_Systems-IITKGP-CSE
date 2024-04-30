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

/* 
   Struct definition for message type 1. 
   Represents a message containing a message type and a process ID.
*/
typedef struct message1
{
    long mtype; // Type of the message
    int pid;    // Process ID
} Msg1;

/* 
   Struct definition for message type 3. 
   Represents a message containing a message type, process ID, and page frame.
*/
typedef struct message3
{
    long mtype;       // Type of the message
    int pid;          // Process ID
    int page_frame;   // Page frame
} Msg3;

// Initialize an array to store the reference string.
char refstr[10010];

int main(int argc, char *argv[]){
    /*
       Check if the correct number of command-line arguments are provided.
       If not, print an error message and exit the program.
    */
    if (argc != 4)
    {
        printf("Provide these three arguments in order: <Reference String> <Message Queue 1 ID> <Message Queue 3 ID>\n");
        exit(1);
    }

    // Fill the array with null characters to ensure it's empty.
    memset(refstr, 0, sizeof(refstr));
    // Copy the reference string provided as a command-line argument into the array.
    strcpy(refstr, argv[1]);
    // Convert the provided Message Queue 1 ID argument to an integer.
    int msg_id1 = atoi(argv[2]);
    // Convert the provided Message Queue 3 ID argument to an integer.
    int msg_id3 = atoi(argv[3]);

    // Get the process ID of the current process.
    int pid = getpid();

    // Print a message indicating that the process has started, along with its process ID.
    printf("Process (pid: %d) has started\n", pid);

    // Initialize a message of type Msg1.
    Msg1 msg1;
    // Set the message type to 1.
    msg1.mtype = 1;
    // Set the process ID in the message to the current process ID.
    msg1.pid = pid;
    // Send the process ID to Message Queue 1 (ready queue).
    msgsnd(msg_id1, (void *)&msg1, sizeof(Msg1), 0);

    // Receive a message from Message Queue 1 targeted specifically to this process ID.
    msgrcv(msg_id1, (void *)&msg1, sizeof(Msg1), pid, 0);


    /*
       Initialize a message of type Msg3.
       Declare loop control variables i and prev_i.
    */
    Msg3 msg3;
    int i = 0;
    int prev_i = -1;

    // Iterate through the reference string until the end.
    while (refstr[i] != '\0')
    {
        // Store the current index as the previous index.
        prev_i = i;

        // Initialize a variable to store the page number.
        int page = 0;
        
        // Extract the page number from the reference string.
        while (refstr[i] != '.' && refstr[i] != '\0')
        {
            page = page * 10 + (refstr[i] - '0');
            i++;
        }
        // Move to the next character after the '.' or reach the end of the string.
        i++;

        // Set the message type, process ID, and page frame in Msg3.
        msg3.mtype = 1;
        msg3.pid = pid;
        msg3.page_frame = page;
        
        // Send the message containing page information to Message Queue 3 (mmu).
        msgsnd(msg_id3, (void *)&msg3, sizeof(Msg3), 0);

        // Receive a response from Message Queue 3 regarding the page frame allocation.
        msgrcv(msg_id3, (void *)&msg3, sizeof(Msg3), pid, 0);

        if (msg3.page_frame == -2)
        {// If the sent page is invalid
            printf("Process with pid %d -> Illegal Page Number - Terminating\n", pid);
            exit(1);
        }
        else if (msg3.page_frame == -1)
        {// If the sent page led to page fault
            printf("Process with pid %d -> Page Fault - Waiting for page to be loaded\n", pid);

            // Wait for a message indicating that the page has been loaded.
            msgrcv(msg_id1, (void *)&msg1, sizeof(Msg1), pid, 0);
            // Reset the index to the position before encountering the page fault.
            i = prev_i;
        }
        else
        {
            // Print a message indicating successful page frame allocation.
            printf("Process with pid %d -> Frame %d allocated for page %d\n", pid, msg3.page_frame, page);
        }
    }

    // Print a message indicating successful allocation of all frames.
    printf("Process with pid %d -> Got all frames correctly\n", pid);
    // Send a termination message to Message Queue 3 (mmu)
    msg3.mtype = 1;
    msg3.pid = pid;
    msg3.page_frame = -9;
    msgsnd(msg_id3, (void *)&msg3, sizeof(Msg3), 0);
    // Print a message indicating process termination.
    printf("Process with pid %d -> Terminating\n", (int)pid);



    return 0;
}