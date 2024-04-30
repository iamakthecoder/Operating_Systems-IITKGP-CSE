#include "foothread.h"

// Maximum number of nodes in the tree
#define MAX_NODES 100

// Global variables
int n; // Number of nodes in the tree
int root_node; // ID of the root node in the tree
int parent[MAX_NODES]; // Parent array of the tree
int num_child[MAX_NODES]; // Number of children of a node in the tree
foothread_mutex_t mutex_sum[MAX_NODES]; // Mutexes for each node to synchronize sum updates
foothread_barrier_t barrier[MAX_NODES]; // Barriers for each node to synchronize its run only after its children are done
foothread_barrier_t computation_done; // Barrier to indicate that the computation is done
foothread_mutex_t func_exec; // Mutex to ensure that only one function prints or takes input at a time
int sum[MAX_NODES]; // Partial sums at each node
int total_sum; // Total sum calculated at the root node

// Function prototype
int compute_sum(void* arg);

// Main function
int main() {
    // Read input tree from file
    FILE* fp = fopen("tree.txt", "r");
    if (fp == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Read number of nodes in the tree
    fscanf(fp, "%d", &n);
    
    // Initialize arrays and variables
    for(int i = 0; i < n; i++) {
        num_child[i] = 0; // Initialize number of children to 0
    }

    // Read tree structure from file
    for (int i = 0; i < n; i++) {
        int node, par;
        fscanf(fp, "%d %d", &node, &par);
        
        // Maintain the tree structure
        parent[node] = par;
        if(node != par) {
            num_child[par]++;
        } else {
            root_node = node;
        }
    }

    // Close the file
    fclose(fp);

    // Initialize synchronization resources for each node
    total_sum = 0;
    foothread_mutex_init(&func_exec); // Mutex to ensure only one function prints or takes input at a time
    for (int i = 0; i < n; i++) {
        foothread_mutex_init(&mutex_sum[i]); // Mutexes for each node to synchronize sum updates
        foothread_barrier_init(&(barrier[i]), num_child[i] + 1); // Barriers for each node to synchronize its run only after its children are done
        sum[i] = 0; // Initialize partial sums to 0
    }
    foothread_barrier_init(&computation_done, 2); // Barrier to indicate that the computation is done

    // Create follower threads for each node
    foothread_t threads[MAX_NODES];
    foothread_attr_t attr;
    foothread_attr_setjointype(&attr, FOOTHREAD_JOINABLE); // Create a joinable thread for each node
    foothread_attr_setstacksize(&attr, FOOTHREAD_DEFAULT_STACK_SIZE); // Use the default stack size for each thread
    for (int i = 0; i < n; i++) {
        foothread_create(&threads[i], &attr, compute_sum, (void*)(intptr_t)i); // Create thread for each node
    }

    // Wait till the computation is done
    foothread_barrier_wait(&computation_done);

    // Print total sum calculated at the root node
    printf("Sum at root (node %d) = %d\n", root_node, total_sum);

    // Clean up resources
    for (int i = 0; i < n; i++) {
        foothread_barrier_destroy(&barrier[i]);
        foothread_mutex_destroy(&mutex_sum[i]);
    }
    foothread_barrier_destroy(&computation_done);
    foothread_mutex_destroy(&func_exec);

    // Synchronize threads before exit
    foothread_exit();

    return 0;
}

// Function to compute sum at each node
int compute_sum(void* arg) {
    int node_id = (intptr_t)arg;
    int parent_id = parent[node_id];

    // Wait till all the node's children are done with their computation
    foothread_barrier_wait(&barrier[node_id]);

    // Execute only one thread at a time for printing or taking input
    foothread_mutex_lock(&func_exec);
    if (num_child[node_id] == 0) {
        // If leaf node, get user input
        printf("Leaf node %d :: Enter a positive integer: ", node_id);
        int leaf_val;
        scanf("%d", &leaf_val);
        foothread_mutex_lock(&mutex_sum[node_id]);
        sum[node_id] = leaf_val;
        foothread_mutex_unlock(&mutex_sum[node_id]);
    } else {
        // Print message for internal nodes
        foothread_mutex_lock(&mutex_sum[node_id]);
        printf("Internal node %d gets the partial sum %d from its children\n", node_id, sum[node_id]);
        foothread_mutex_unlock(&mutex_sum[node_id]);
    }
    foothread_mutex_unlock(&func_exec);

    // Update partial sum in the parent's node
    foothread_mutex_lock(&mutex_sum[parent_id]);
    if (node_id != parent_id) {
        sum[parent_id] += sum[node_id];
    }
    foothread_mutex_unlock(&mutex_sum[parent_id]);

    // If root node, indicate that complete execution is done
    if (parent_id == node_id) {
        total_sum = sum[node_id];
        foothread_barrier_wait(&computation_done);
    } else {
        // Notify its parent that it is done with its execution
        foothread_barrier_wait(&barrier[parent_id]);
    }

    // Exit the thread
    foothread_exit();

    return 0;
}
