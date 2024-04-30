#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>


void consumption_loop(int consumer_no, int M[]){
    int item_cnt = 0;
    int checksum = 0;
    while(M[0]!=-1){
        int flag = 0;
        while(M[0]!=consumer_no){
            //wait
            if(M[0]==-1){
                flag = 1;
                break;
            }
        }
        if(flag)break;

        #ifdef VERBOSE
            printf("\t\t\t\t\tConsumer %d reads %d\n", consumer_no, M[1]);
        #endif

        item_cnt++;
        checksum += M[1];
        M[0] = 0;
    }

    printf("\t\t\t\t\tConsumer %d has read %d items: Checksum = %d\n", consumer_no, item_cnt, checksum);
}


int main(){
    srand(time(NULL));

    int n;
    printf("Enter the value of n (no. of consumers): ");
    scanf("%d", &n);

    int t;
    printf("Enter the value of t : ");
    scanf("%d", &t);

    int shmid = shmget(IPC_PRIVATE, 2*sizeof(int), 0777|IPC_CREAT);
    if (shmid == -1) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }
    void *temp = shmat(shmid, NULL, 0);
    if(temp==(void *)-1){
        perror("shmat failure");
        exit(EXIT_FAILURE);
    }
    int *M = (int *)temp;
    M[0]=0;

    for(int i=1; i<=n; i++){
        if(fork()==0){
            int consumer_no = i;
            printf("\t\t\t\t\tConsumer %d is alive\n", consumer_no);

            consumption_loop(consumer_no, M);
            if(shmdt(M)==-1){
                perror("shmdt failure");
                exit(EXIT_FAILURE);
            }
            exit(0);
        }
        else{
            continue;
        }
    }
    printf("Producer is alive\n");
    int item_cnt[n];
    int checksum[n];
    for(int i=0; i<n; i++){
        item_cnt[i]=0;
        checksum[i]=0;
    }

    for(int i=0; i<t; i++){
        int rand_no = rand()%900 + 100;
        int cons_no = rand()%n + 1;
        while(M[0]!=0){
            //wait
        }

        M[0] = cons_no;
        #ifdef SLEEP
            usleep(5);
        #endif
        M[1] = rand_no;
        #ifdef VERBOSE
            printf("Producer produces %d for Consumer %d\n", rand_no, cons_no);
        #endif

        item_cnt[cons_no-1]++;
        checksum[cons_no-1]+=rand_no;
    }
    while(M[0]!=0);
    M[0] = -1;
    printf("Producer has produced %d items\n", t);
    for(int i=0; i<n; i++){
        wait(0);
    }

    for(int i=0; i<n; i++){
        printf("%d items for Consumer %d: Checksum = %d\n", item_cnt[i], i+1, checksum[i]);
    }

    if(shmdt(M)==-1){
        perror("shmdt failure");
        exit(EXIT_FAILURE);
    }
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl failure");
        exit(EXIT_FAILURE);
    }

    return 0;
}