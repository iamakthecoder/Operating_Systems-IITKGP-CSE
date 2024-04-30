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

int main(){
    struct sembuf pop, vop ;
    pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1 ;


    int n;
    key_t key;

    FILE *file;

    // Open graph.txt in read mode
    file = fopen("graph.txt", "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    // Read the integer from the first line in graph.txt
    if (fscanf(file, "%d", &n) != 1) {
        perror("Error reading integer from file");
        exit(1);
    }
    

    size_t sz;
    // Create a shared memory segment for A
    if ((key = ftok("graph.txt", 'R')) == -1) {
        perror("ftok");
        exit(1);
    }
    int shmid_A;
    sz = sizeof(int)*n*n;
    if ((shmid_A = shmget(key, sz, IPC_CREAT | 0777)) < 0) {
        perror("shmget1");
        exit(1);
    }
    int (*A)[n];
    if ((A = shmat(shmid_A, NULL, 0)) == (void *) -1) {
        perror("shmat");
        exit(1);
    }

    // Create a shared memory segment for T
    if ((key = ftok("graph.txt", 'T')) == -1) {
        perror("ftok");
        exit(1);
    }
    int shmid_T;
    sz = sizeof(int)*n;
    if ((shmid_T = shmget(key, sz, IPC_CREAT | 0777)) < 0) {
        perror("shmget2");
        exit(1);
    }
    int *T;
    if ((T = shmat(shmid_T, NULL, 0)) == (void *) -1) {
        perror("shmat");
        exit(1);
    }

    // Create a shared memory segment for idx
    if ((key = ftok("graph.txt", 'I')) == -1) {
        perror("ftok");
        exit(1);
    }
    int shmid_idx;
    sz = sizeof(int);
    if ((shmid_idx = shmget(key, sz, IPC_CREAT | 0777)) < 0) {
        perror("shmget3");
        exit(1);
    }
    int *idx;
    if ((idx = shmat(shmid_idx, NULL, 0)) == (void *) -1) {
        perror("shmat");
        exit(1);
    }

    //fill A with the adjacency matrix
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (fscanf(file, "%d", &A[i][j]) != 1) {
                perror("Error reading adjacency matrix from file");
                exit(1);
            }
        }
    }
    //initialize T array with -1
    for(int i=0; i<n; i++){
        T[i]=-1;
    }
    //initialize the idx value with 0
    *idx = 0;


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

    //semaphore key
    if ((key = ftok("graph.txt", 'N')) == -1) {
        perror("ftok");
        exit(1);
    }
    int ntfy = semget(key, 1, 0777|IPC_CREAT);

    //calculate indegree of all the nodes in the graph
    int indeg[n];
    for(int i=0; i<n; i++){
        indeg[i]=0;
    }
    for(int i=0; i<n; i++){
        for(int j=0; j<n; j++){
            if(A[i][j]){
                indeg[j]++;
            }
        }
    }
    //initialize semaphore 'sync' with indegree of the nodes
    for(int i=0; i<n; i++){
        semctl(sync, i, SETVAL, indeg[i]);
    }
    //initialize semaphore 'mtx' with 0
    semctl(mtx, 0, SETVAL, 0);
    //initialize semaphore 'ntfy' with 1
    semctl(ntfy, 0, SETVAL, 1);

    printf("+++ Boss: Setup done...\n");
    int done = 0; //count of nodes inserted in T
    while(1){
        P(ntfy);
        done++;
        V(mtx);
        if(*idx==n)break; //all the n nodes are inserted in T
    }

    printf("+++ Topological sorting of the vertices\n");
    for(int i=0; i<n; i++){
        printf("%d ", T[i]);
    }
    printf("\n");


    int correct = 1;
    for(int i=0; i<n; i++){
        if(indeg[T[i]]==0){
            for(int j=0; j<n; j++){
                if(A[T[i]][j]){
                    indeg[j]--;
                }
            }
        }
        else{
            correct = 0;
            break;
        }
    }

    if(correct){
        printf("+++ Boss: Well done, my team...\n");
    }
    else{
        printf("+++ Wrong output...\n");
    }


    fclose(file); //close the graph.txt file
    //close all the semaphores
    semctl(ntfy, 0, IPC_RMID, 0);
    semctl(sync, 0, IPC_RMID, 0);
    semctl(mtx, 0, IPC_RMID, 0);
    //detach all the shared memory segments
    shmdt(A);
    shmdt(T);
    shmdt(idx);
    //close all the shared memory segments
    shmctl(shmid_A, IPC_RMID, 0);
    shmctl(shmid_T, IPC_RMID, 0);
    shmctl(shmid_idx, IPC_RMID, 0);


    return 0;
}