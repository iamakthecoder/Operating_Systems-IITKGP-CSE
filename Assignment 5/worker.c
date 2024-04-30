#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>

#define P(s) semop(s, &pop, 1)  /* pop is the structure we pass for doing
				   the P(s) operation */
#define V(s) semop(s, &vop, 1)  /* vop is the structure we pass for doing
				   the V(s) operation */

int main(int argc, char* argv[]){

    int n,i;
    sscanf(argv[1], "%d", &n);
    sscanf(argv[2], "%d", &i);

    struct sembuf pop, vop ;
    pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1 ;

    size_t sz;
    key_t key;
    // shared memory segment for A
    if ((key = ftok("graph.txt", 'R')) == -1) {
        perror("ftok");
        exit(1);
    }
    int shmid_A;
    sz = sizeof(int)*n*n;
    if ((shmid_A = shmget(key, sz, IPC_CREAT | 0777)) < 0) {
        perror("shmget");
        exit(1);
    }
    int (*A)[n];
    if ((A = shmat(shmid_A, NULL, 0)) == (void *) -1) {
        perror("shmat");
        exit(1);
    }

    // shared memory segment for T
    if ((key = ftok("graph.txt", 'T')) == -1) {
        perror("ftok");
        exit(1);
    }
    int shmid_T;
    sz = sizeof(int)*n;
    if ((shmid_T = shmget(key, sz, IPC_CREAT | 0777)) < 0) {
        perror("shmget");
        exit(1);
    }
    int *T;
    if ((T = shmat(shmid_T, NULL, 0)) == (void *) -1) {
        perror("shmat");
        exit(1);
    }

    // shared memory segment for idx
    if ((key = ftok("graph.txt", 'I')) == -1) {
        perror("ftok");
        exit(1);
    }
    int shmid_idx;
    sz = sizeof(int);
    if ((shmid_idx = shmget(key, sz, IPC_CREAT | 0777)) < 0) {
        perror("shmget");
        exit(1);
    }
    int *idx;
    if ((idx = shmat(shmid_idx, NULL, 0)) == (void *) -1) {
        perror("shmat");
        exit(1);
    }


    //semaphore mtx
    if ((key = ftok("graph.txt", 'M')) == -1) {
        perror("ftok");
        exit(1);
    }
    int mtx = semget(key, 1, 0777|IPC_CREAT);

    //semaphore sync
    if ((key = ftok("graph.txt", 'S')) == -1) {
        perror("ftok");
        exit(1);
    }
    int sync = semget(key, n, 0777|IPC_CREAT);

    //semaphore ntfy
    if ((key = ftok("graph.txt", 'N')) == -1) {
        perror("ftok");
        exit(1);
    }
    int ntfy = semget(key, 1, 0777|IPC_CREAT);


    pop.sem_num = i;
    pop.sem_op = 0;
    P(sync); //wait till the worker's chance

    pop.sem_num = 0;
    pop.sem_op = -1;
    P(mtx); //wait till the worker get the signal to do it's work

    //insert the worker's node into T
    T[*idx] = i;
    //increment idx
    *idx = (*idx)+1;

    //for all the edges from i to another node, decrement the sync (or indegree) values of those nodes
    for(int j=0; j<n; j++){
        if(A[i][j]){
            vop.sem_num = j;
            vop.sem_op = -1;
            V(sync);
        }
    }

    vop.sem_num = 0;
    vop.sem_op = 1;
    V(ntfy);  //send the ntfy signal to the boss

    shmdt(A);
    shmdt(T);
    shmdt(idx);

    return 0;
}