#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char* argv[]){
    //mode=1: Supervisor mode
    //mode=2: Command-input mode
    //mode=3: Execute-command mode
    int mode = 1;
    if(argc!=1){
        mode = -1;
        if(argv[1][0]=='C'){
            mode = 2;
        }
        if(argv[1][0]=='E'){
            mode = 3;
        }
    }

    if(mode==-1){
        exit(0);
    }

    if(mode==1){ //SUPERVISOR MODE
        printf("+++ CSE in supervisor mode: Started\n");
        int p1[2];
        int p2[2];
        pipe(p1);
        pipe(p2);
        printf("+++ CSE in supervisor mode: pfd = [%d %d]\n", p1[0], p1[1]);

        int pid1 = fork();
        if(pid1==0){
            printf("+++ CSE in supervisor mode: Forking first child in command-input mode\n");
            char buff1[5];
            char buff2[5];
            char buff3[5];
            char buff4[5];
            sprintf(buff1, "%d", p1[0]);
            sprintf(buff2, "%d", p1[1]);
            sprintf(buff3, "%d", p2[0]);
            sprintf(buff4, "%d", p2[1]);
            execlp("xterm", "xterm", "-T", "\"First Child\"", "-e", "./CSE", "C", buff1, buff2, buff3, buff4, NULL);
        }

        int pid2 = fork();
        if(pid2==0){
            printf("+++ CSE in supervisor mode: Forking second child in execute mode\n");
            char buff1[5];
            char buff2[5];
            char buff3[5];
            char buff4[5];
            sprintf(buff1, "%d", p1[0]);
            sprintf(buff2, "%d", p1[1]);
            sprintf(buff3, "%d", p2[0]);
            sprintf(buff4, "%d", p2[1]);
            execlp("xterm", "xterm", "-T", "\"Second Child\"", "-e", "./CSE", "E", buff1, buff2, buff3, buff4, NULL);
        }
        

        int w1 = wait(0);
        if(w1==pid1){
            printf(" +++ CSE in supervisor mode: First child terminated\n");
        }
        else if(w1==pid2){
            printf(" +++ CSE in supervisor mode: Second child terminated\n");

        }

        int w2 = wait(0);
        if(w2==pid1){
            printf(" +++ CSE in supervisor mode: First child terminated\n");
        }
        else if(w2==pid2){
            printf(" +++ CSE in supervisor mode: Second child terminated\n");

        }


        exit(EXIT_SUCCESS);
       
    }

    //C mode or E mode....
    int state = 0;
    while(1){

        if(mode==2){ //C mode...
            int exit_flag = 0;

            int stdout_orig = dup(1);
            close(1);
            
            int writep;
            if(state){
                sscanf(argv[5], "%d", &writep);
            }
            else{
                sscanf(argv[3], "%d", &writep);
            }

            int x = dup(writep);
            if(x!=1)exit(0);

            char buff[50];

            while(1){
                fputs("Enter command> ", stderr);

                buff[0]='\0';
                scanf("%[^\n]s", buff);
                getchar();

                printf("%s\n", buff);
                fflush(stdout);

                if(strcmp(buff, "swaprole")==0){
                    break;
                }

                if(strcmp(buff, "exit")==0){
                    exit_flag = 1;
                    break;
                }

            }

            close(1);
            dup(stdout_orig);
            close(stdout_orig);
            if(exit_flag) exit(0);
            state=1-state;
            mode = 3;
            continue;

        }
        

        if(mode==3){ // E mode...
            int exit_flag = 0;

            int stdin_orig = dup(0);
            close(0);

            int readp;
            if(state){
                sscanf(argv[4], "%d", &readp);
            }
            else{
                sscanf(argv[2], "%d", &readp);
            }

            int x = dup(readp);
            if(x!=0)exit(0);

            char buff[50];
            while(1){
                fputs("Command received> ", stderr);

                buff[0]='\0';
                scanf("%[^\n]s", buff);
                getchar();

                printf("%s", buff);
                fflush(stdout);

                printf("\n");
                fflush(stdout);

                if(strcmp(buff, "swaprole")==0){
                    break;
                }

                if(strcmp(buff, "exit")==0){
                    exit_flag = 1;
                    break;
                }

                const char delimiters[] = " ";
                char *token;
                char *args[10];

                int i=0;
                token = strtok(buff, delimiters);
                while (token != NULL) {
                    args[i++] = token;
                    token = strtok(NULL, delimiters);
                }
                args[i] = NULL;

                pid_t pid_new = fork();
                if(pid_new==0){
                    close(0);
                    dup(stdin_orig);
                    close(stdin_orig);
                    int err = execvp(args[0], args);
                    if(err==-1){
                        printf("*** Unable to execute command\n");
                        exit(1);
                    }
                }
                else{
                    int status;
                    waitpid(pid_new, &status, 0);
                }
                
                
            }

            close(0);
            dup(stdin_orig);
            close(stdin_orig);
            if(exit_flag)exit(0);
            state=1-state;
            mode = 2;
            continue;
        }
    }


    return 0;
}