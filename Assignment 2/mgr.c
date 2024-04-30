#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#define MAXLEN 50

void print_help(){
    char help_msg[] = "Command : Action\n c : Continue a suspended job\n h : Print this help message\n k : Kill a suspended job\n p : Print the process table\n q : Quit\n r : Run a new job\n";
    printf("%s", help_msg);
}

int running_job_pid = -1;

void signal_handler(int signo){
    if(running_job_pid==-1){
        printf("\nmgr> ");
        fflush(stdout);
        return;
    }
    if(signo==SIGINT){
        kill(running_job_pid, SIGINT);
    }
    if(signo==SIGTSTP){
        kill(running_job_pid, SIGTSTP);
    }
}

char* process_status[] = {"SELF", "FINISHED", "TERMINATED", "SUSPENDED", "KILLED"};

typedef struct Process{
    pid_t pid;
    pid_t pgid;
    int status;
    char name[MAXLEN];
} process;


process PT[11];

int job_id=0;

void set_new_process(pid_t pid, pid_t pgid, int status, char name[]){
    PT[job_id].pid = pid;
    PT[job_id].pgid = pgid;
    PT[job_id].status = status;
    sprintf(PT[job_id].name, name);
    job_id++;
}

void print_process_table(){
    printf("%-7s%-7s%-7s%-15s%-10s\n", "NO", "PID", "PGID", "STATUS", "NAME");
    for(int i=0; i<job_id; i++){
        printf("%-7d%-7u%-7u%-15s%-10s\n", i, PT[i].pid, PT[i].pgid, process_status[PT[i].status], PT[i].name);
    }
}

int temp[11];

void kill_remaining_jobs(){
    for(int i=0; i<job_id; i++){
        if(PT[i].status!=3)continue;
        kill(PT[i].pid, SIGKILL);
        PT[i].status = 4;
    }
}

int main(){

    srand((unsigned int)time(NULL));


    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);


    set_new_process(getpid(), getpgid(0), 0, "mgr");

    char input='a';

    while(input!='q'){
        printf("\nmgr> ");
        scanf("%c", &input);
        getchar();

        if(input=='h'){
            print_help();
            continue;
        }

        if(input=='p'){
            print_process_table();
            continue;
        }

        if(input=='r'){
            if(job_id==11){
                printf("Process table is full. Quittng...\n");
                kill_remaining_jobs();
                exit(1);
            }


            int pid_new = fork();
            running_job_pid = pid_new;
            char arg[2] = {'A' + rand()%26,'\0'};
            if(pid_new<0){
                perror("Fork failed.");
                kill_remaining_jobs();
                exit(1);
            }
            if(pid_new==0){
                setpgid(0,0);
                execl("job", "job", arg, NULL);
            }
            else{
                setpgid(pid_new,0);
                int status;
                waitpid(pid_new,&status,WUNTRACED);
                running_job_pid = -1;
                int new_process_status = 0;
                if(WIFEXITED(status)){
                    new_process_status = 1;
                }
                else if(WIFSIGNALED(status)){
                    new_process_status = 2;
                }
                else if(WIFSTOPPED(status)){
                    new_process_status = 3;
                }

                char new_name[] = "job A";
                new_name[4] = arg[0];
                set_new_process(pid_new, pid_new, new_process_status, new_name);
            }

            continue;
        }


        if(input == 'c'){
            int num_suspended = 0;

            for(int i=0; i<job_id; i++){
                if(PT[i].status==3){
                    temp[num_suspended++] = i;
                }
            }

            if(num_suspended==0)continue;

            printf("Suspended jobs : ");
            for(int i=0; i<num_suspended; i++){
                printf("%d, ", temp[i]);
            }

            printf("\b\b (Pick one): ");
            int op_num;
            scanf("%d", &op_num);
            getchar();
            int flag = 0;
            for(int i=0; i<num_suspended; i++){
                if(temp[i]==op_num){
                    flag=1;
                    break;
                }
            }

            if(flag==0){
                printf("Not a valid input!\n");
                continue;
            }

            running_job_pid = PT[op_num].pid;

            kill(running_job_pid, SIGCONT);
            int status;
            waitpid(running_job_pid, &status, WUNTRACED);
            running_job_pid = -1;
            int new_process_status = 0;
            if(WIFEXITED(status)){
                new_process_status = 1;
            }
            else if(WIFSIGNALED(status)){
                new_process_status = 2;
            }
            else if(WIFSTOPPED(status)){
                new_process_status = 3;
            }
            PT[op_num].status = new_process_status;

            continue;
        }

        if(input == 'k'){
            int num_killed = 0;

            for(int i=0; i<job_id; i++){
                if(PT[i].status==3){
                    temp[num_killed++] = i;
                }
            }

            if(num_killed==0)continue;

            printf("Suspended jobs : ");
            for(int i=0; i<num_killed; i++){
                printf("%d, ", temp[i]);
            }

            printf("\b\b (Pick one): ");
            int op_num;
            scanf("%d", &op_num);
            getchar();
            int flag = 0;
            for(int i=0; i<num_killed; i++){
                if(temp[i]==op_num){
                    flag=1;
                    break;
                }
            }

            if(flag==0){
                printf("Not a valid input!\n");
                continue;
            }

            running_job_pid = PT[op_num].pid;

            kill(running_job_pid, SIGKILL);

            PT[op_num].status = 4;

            continue;
        }

        if(input!='q'){
            printf("Enter a valid input\n");
            continue;
        }

    }


    
    kill_remaining_jobs();
    exit(0);


    return 0;
}