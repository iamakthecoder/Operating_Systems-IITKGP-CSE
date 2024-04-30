#include <foothread.h>

// Structure to hold function pointer and arguments
typedef struct {
    int (*start_routine)(void *);
    void *args;
} func_and_args;

// Structure to manage threads
typedef struct {
    foothread_t threads[FOOTHREAD_THREADS_MAX];
    foothread_attr_t attrs[FOOTHREAD_THREADS_MAX];
    sem_t mutex[FOOTHREAD_THREADS_MAX];
    void *stack[FOOTHREAD_THREADS_MAX];
    int live[FOOTHREAD_THREADS_MAX];
    int count;
} foothread_manager_t;

// Static instances of thread manager and mutex
static foothread_manager_t foothread_manager;
static sem_t manager_mutex;
static int init_done = 0;

// Set join type attribute for thread
void foothread_attr_setjointype(foothread_attr_t *attr, int join_type) {
    if(attr==NULL)return;
    if (join_type == FOOTHREAD_JOINABLE || join_type == FOOTHREAD_DETACHED) {
        attr->join_type = join_type;
    }
}

// Set stack size attribute for thread
void foothread_attr_setstacksize(foothread_attr_t *attr, int stack_size) {
    if(attr==NULL)return;
    attr->stack_size = stack_size;
}

// Function to start a thread
int foothread_start(void *param) {
    func_and_args *new_func = (func_and_args *)param;
    int (*start_routine)(void *) = new_func->start_routine;
    void *args = new_func->args;

    start_routine(args);
    return 0;
}

// Function to create a thread
void foothread_create(foothread_t *thread, foothread_attr_t *attr, int (*start_routine)(void*), void *arg) {
    // Initialize manager if not done already
    if(init_done==0){
        sem_init(&manager_mutex, 0, 1); // Initialize manager_mutex with value 1
        memset(&foothread_manager, 0, sizeof(foothread_manager_t)); // Initialize foothread_manager with null
        foothread_manager.count = 0; // Set count to 0
        init_done = 1; // Update init_done
    }

    // Handle invalid arguments
    if (thread == NULL) {
        printf("ERROR: First argument (foothread_t *) is NULL\n");
        exit(EXIT_FAILURE); 
    }
    if(attr!=NULL){
        if(attr->join_type!=FOOTHREAD_JOINABLE && attr->join_type!=FOOTHREAD_DETACHED){
            printf("ERROR: A non-null uninitialized attribute is being used.\n");
            exit(EXIT_FAILURE);
        }
    }

    // Allocate stack for the thread
    size_t stack_size = (attr != NULL) ? attr->stack_size : FOOTHREAD_DEFAULT_STACK_SIZE;
    void *stack = malloc(stack_size);
    if (stack == NULL) {
        perror("Error allocating stack");
        exit(EXIT_FAILURE);
    }

    // Flags for cloning
    int clone_flags = SIGCHLD|CLONE_SIGHAND|CLONE_FS|CLONE_VM|CLONE_FILES|CLONE_THREAD;

    // Lock manager mutex
    sem_wait(&manager_mutex);

    // Check if maximum threads limit reached
    if(foothread_manager.count == FOOTHREAD_THREADS_MAX){
        printf("ERROR: Maximum allowed number of threads already created\n");
        free(stack);
        sem_post(&manager_mutex);
        exit(EXIT_FAILURE);
    }

    // Prepare function and arguments for the new thread
    func_and_args *new_func = (func_and_args *)malloc(sizeof(func_and_args));
    new_func->start_routine = start_routine;
    new_func->args = arg;

    // Create the thread
    int tid = clone(foothread_start, (char*)stack + stack_size, clone_flags, (void *)new_func);
    if (tid == -1) {
        perror("Error creating thread");
        free(stack);
        free(new_func);
        sem_post(&manager_mutex);
        exit(EXIT_FAILURE);
    }

    // Fill in thread information in manager
    thread->pid = getpid();
    thread->tid = tid;
    foothread_manager.threads[foothread_manager.count] = *thread;
    if (attr != NULL) {
        foothread_manager.attrs[foothread_manager.count] = *attr;
    } else {
        foothread_attr_t default_attr = FOOTHREAD_ATTR_INITIALIZER;
        foothread_manager.attrs[foothread_manager.count] = default_attr;
    }
    sem_init(&foothread_manager.mutex[foothread_manager.count], 0, 0);
    foothread_manager.stack[foothread_manager.count] = stack;
    foothread_manager.live[foothread_manager.count] = 1;
    foothread_manager.count++;

    // Release manager mutex
    sem_post(&manager_mutex);

    return;
}

// Function to exit the thread
void foothread_exit() {
    if(init_done==0){
        printf("ERROR: Invalid function call (no thread created)\n");
        exit(EXIT_FAILURE);
    }

    int pid = getpid();
    int tid = gettid();

    // Lock manager mutex
    sem_wait(&manager_mutex);
    int maxt = foothread_manager.count;
    sem_post(&manager_mutex);

    // If called from main thread
    if(pid==tid){
        for(int i=0; i<maxt; i++){
            if(foothread_manager.live[i]==0)continue;
            if(foothread_manager.threads[i].pid == pid && foothread_manager.attrs[i].join_type == FOOTHREAD_JOINABLE){
                sem_wait(&foothread_manager.mutex[i]);
                sem_wait(&manager_mutex);
                free(foothread_manager.stack[i]);
                foothread_manager.live[i] = 0;
                sem_post(&manager_mutex);
            }
            sem_wait(&manager_mutex);
            maxt = foothread_manager.count;
            sem_post(&manager_mutex);
        }
        exit(EXIT_SUCCESS);
    }
    // If called from child thread
    else{
        for(int i=0; i<maxt; i++){
            if(foothread_manager.live[i]==0)continue;
            if(foothread_manager.threads[i].pid == pid && foothread_manager.threads[i].tid == tid){
                sem_post(&foothread_manager.mutex[i]);
                break;
            }
        }
    }
}


// Initialize a mutex
void foothread_mutex_init(foothread_mutex_t *mutex) {
    // Check for NULL argument
    if(mutex==NULL){
        printf("ERROR: NULL argument is provided as first argument (foothread_mutex_t *)\n");
        exit(EXIT_FAILURE);
    }

    // Initialize mutex properties
    mutex->is_live = 1; // Mutex is live (active)
    mutex->is_locked = 0; // Mutex is initially unlocked
    sem_init(&(mutex->sem), 0, 1); // Initialize the semaphore used for locking

    return;
}

// Lock a mutex
void foothread_mutex_lock(foothread_mutex_t *mutex) {  
    // Check for NULL argument
    if(mutex==NULL){
        printf("ERROR: NULL argument is provided as first argument (foothread_mutex_t *)\n");
        exit(EXIT_FAILURE);
    }  

    // Check if mutex is valid
    if(mutex->is_live == 0){
        printf("ERROR: Invalid mutex provided\n");
        exit(EXIT_FAILURE);
    }

    // Wait for semaphore to be available
    sem_wait(&(mutex->sem));
    
    // Update mutex state
    mutex->is_locked = 1; // Mutex is now locked
    mutex->owner_pid = getpid(); // Record the owner process ID
    mutex->owner_tid = gettid(); // Record the owner thread ID

    return;
}

// Unlock a mutex
void foothread_mutex_unlock(foothread_mutex_t *mutex) {
    // Check for NULL argument
    if(mutex==NULL){
        printf("ERROR: NULL argument is provided as first argument (foothread_mutex_t *)\n");
        exit(EXIT_FAILURE);
    }

    // Check if mutex is valid
    if(mutex->is_live==0){
        printf("ERROR: Invalid mutex provided\n");
        exit(EXIT_FAILURE);
    }

    // Check if mutex is already unlocked
    if (!(mutex->is_locked)) {
        // Error: Attempt to unlock an unlocked mutex
        printf("ERROR: Attempt to unlock an unlocked mutex\n");
        // Handle error accordingly, e.g., print error message or terminate
        exit(EXIT_FAILURE);
    }

    // Check if the current thread is the owner of the mutex
    if (mutex->owner_pid != getpid() || mutex->owner_tid != gettid()) {
        // Error: Attempt to unlock a mutex by another thread
        printf("ERROR: Attempt to unlock a mutex by another thread");
        // Handle error accordingly, e.g., print error message or terminate
        exit(EXIT_FAILURE);
    }

    // Update mutex state
    mutex->is_locked = 0; // Mutex is now unlocked
    sem_post(&(mutex->sem)); // Release the semaphore

    return;
}

// Destroy a mutex
void foothread_mutex_destroy(foothread_mutex_t *mutex) {
    // Check for NULL argument
    if(mutex==NULL){
        printf("ERROR: NULL argument is provided as first argument (foothread_mutex_t *)\n");
        exit(EXIT_FAILURE);
    }

    // Check if mutex is valid
    if(!(mutex->is_live)){
        printf("ERROR: Invalid mutex provided\n");
        return;
    }

    // Update mutex state
    mutex->is_live = 0; // Mark mutex as inactive
    mutex->is_locked = 0; // Ensure mutex is unlocked before destruction
    sem_destroy(&(mutex->sem)); // Destroy the semaphore

    return;
}

// Initialize a barrier
void foothread_barrier_init(foothread_barrier_t *barrier, int n){
    // Check for NULL argument
    if(barrier==NULL){
        printf("ERROR: NULL argument is provided as first argument (foothread_barrier_t *)\n");
        exit(EXIT_FAILURE);
    }

    // Initialize barrier properties
    barrier->is_live = 1; // Barrier is live (active)
    barrier->n = n; // Number of threads to synchronize
    barrier->count = 0; // Counter to track arrivals at the barrier
    sem_init(&(barrier->mutex), 0, 1); // Initialize the semaphore used for mutual exclusion
    sem_init(&(barrier->sem1), 0, 0); // Initialize the semaphore for signaling completion of arrivals
    sem_init(&(barrier->sem2), 0, 1); // Initialize the semaphore for signaling completion of departures

    return;
}

// Wait at a barrier
void foothread_barrier_wait(foothread_barrier_t *barrier){
    // Check for NULL argument
    if(barrier==NULL){
        printf("ERROR: NULL argument is provided as first argument (foothread_barrier_t *)\n");
        exit(EXIT_FAILURE);
    }

    // Check if barrier is valid
    if(!(barrier->is_live)){
        printf("ERROR: Invalid barrier provided\n");
        exit(EXIT_FAILURE);
    }

    // Increment arrival counter in a thread-safe manner
    sem_wait(&barrier->mutex);
    barrier->count++;
    if(barrier->count == barrier->n){
        // Last thread reached the barrier, signal others to proceed
        sem_wait(&barrier->sem2);
        sem_post(&barrier->sem1);
    }
    sem_post(&barrier->mutex);

    // Wait for signal to proceed
    sem_wait(&barrier->sem1);
    sem_post(&barrier->sem1);

    // Decrement arrival counter in a thread-safe manner
    sem_wait(&barrier->mutex);
    barrier->count--;
    if (barrier->count == 0){
        // Last thread departed the barrier, signal others to reset
        sem_wait(&barrier->sem1);
        sem_post(&barrier->sem2);
    }
    sem_post(&barrier->mutex);

    // Wait for signal to reset
    sem_wait(&barrier->sem2);
    sem_post(&barrier->sem2);

    return;
}

// Destroy a barrier
void foothread_barrier_destroy(foothread_barrier_t *barrier){
    // Check for NULL argument
    if(barrier==NULL){
        printf("ERROR: NULL argument is provided as first argument (foothread_barrier_t *)\n");
        exit(EXIT_FAILURE);
    }

    // Check if barrier is valid
    if(!(barrier->is_live)){
        printf("ERROR: Invalid barrier provided\n");
        exit(EXIT_FAILURE);
    }

    // Mark barrier as inactive and destroy associated semaphores
    barrier->is_live = 0;
    sem_destroy(&(barrier->mutex));
    sem_destroy(&(barrier->sem1));
    sem_destroy(&(barrier->sem2));
}
