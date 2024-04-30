#define _GNU_SOURCE
#include <err.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>

#define FOOTHREAD_JOINABLE 26
#define FOOTHREAD_DETACHED 27

#define FOOTHREAD_THREADS_MAX 100
#define FOOTHREAD_DEFAULT_STACK_SIZE 2097152 // 2MB

typedef struct {
    int join_type; // FOOTHREAD_JOINABLE or FOOTHREAD_DETACHED
    size_t stack_size;
} foothread_attr_t;

#define FOOTHREAD_ATTR_INITIALIZER {FOOTHREAD_DETACHED, FOOTHREAD_DEFAULT_STACK_SIZE}

typedef struct {
    pid_t pid;
    pid_t tid;
} foothread_t;

typedef struct {
    int is_live;
    int is_locked;
    pid_t owner_pid;
    pid_t owner_tid;
    sem_t sem;
} foothread_mutex_t;

typedef struct {
    int is_live;
    int n;
    int count;
    sem_t mutex;
    sem_t sem1;
    sem_t sem2;
} foothread_barrier_t;


void foothread_attr_setjointype ( foothread_attr_t * , int ) ;
void foothread_attr_setstacksize ( foothread_attr_t * , int ) ;
void foothread_create ( foothread_t * , foothread_attr_t * , int (*)(void *) , void * ) ;
void foothread_exit ( ) ;

void foothread_mutex_init(foothread_mutex_t *mutex);
void foothread_mutex_lock(foothread_mutex_t *mutex);
void foothread_mutex_unlock(foothread_mutex_t *mutex);
void foothread_mutex_destroy(foothread_mutex_t *mutex);

void foothread_barrier_init ( foothread_barrier_t * , int ) ;
void foothread_barrier_wait ( foothread_barrier_t * ) ;
void foothread_barrier_destroy ( foothread_barrier_t * ) ;