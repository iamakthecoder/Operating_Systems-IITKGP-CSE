#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 
#include <unistd.h>  
#include <pthread.h>  

#include <event.h>

// Define a structure to hold event and ID information
typedef struct {
   event e; // Event details
   int id;  // ID of the event (reporter, patient or sales representative)
} param;

// Declare counters for patients, reporters, and sales representatives
int patient_cnt = 0, reporter_cnt = 0, salesrep_cnt = 0;
// Declare counters for waiting patients, reporters, and sales representatives
int patient_wait = 0, reporter_wait = 0, salesrep_wait = 0;
// Declare counters for current patient, reporter, and sales representative
int patient_curr = 0, reporter_curr = 0, salesrep_curr = 0;
// Declare variable to hold the current time
int curr_time = -1e9; // Initialized to a very large negative value
// Declare a flag to indicate if the doctor is free
int doc_free = 0;
// Declare a flag to indicate if the doctor's session has ended
int doc_session_end = 0;
// Declare mutexes for synchronization
pthread_mutex_t mutex; // Mutex for general synchronization
pthread_mutex_t doc_mutex; // Mutex for doctor's operations
// Declare variables and condition variables related to the doctor's state
int doc_state = 0; // Doctor's state: 0 - Not busy, 1 - Busy, -1 - Leaving
pthread_cond_t doc_cond, doc_done; // Condition variables related to doctor's operations
// Declare condition variables for patients, reporters, and sales representatives
pthread_cond_t patient_cond, reporter_cond, salesrep_cond;
// Declare flags and condition variables for thread initialization
int patient_done = 0, reporter_done = 0, salesrep_done = 0; // Flags indicating thread initialization
pthread_cond_t patient_init, reporter_init, salesrep_init; // Condition variables for thread initialization


char *format_time(int minutes, char *time_str){
    int hr, min;
    // If minutes are positive, calculate hours and minutes after adding 9 hours to a 24-hour clock.
    if(minutes>=0){
        min = minutes%60;
        hr = (9+minutes/60)%24;
    }
    // If minutes are negative, calculate hours and minutes before 9 am
    else{
        min = 60 - (-minutes)%60;
        hr = (9 - (-minutes)/60);
        hr--;
        // Adjust hours and minutes for the special case
        if(min==60){
            min=0;
            hr++;
        }
        //Adjust rolling back to previous day
        if(hr<0){
            int n = (-hr+24-1)/24;
            hr = (hr+24*n)%24;
        }
    }
    // Determine the period (am/pm) based on the hour.
    char period[3];
    if(hr>=12){
        strcpy(period, "pm");
        hr-=12;
    }
    else{
        strcpy(period, "am");
    }

    // Format the time string and store it in the provided character array.
    sprintf(time_str, "%02d:%02d%s", hr == 0 ? 12 : hr, min, period);

    return time_str;
}

void *doctor(){
    char time_buff[10];
    while(1){
        pthread_mutex_lock(&doc_mutex);
        // Wait until signaled by the condition variable when there's a change in doctor's state
        while(doc_state==0){
            pthread_cond_wait(&doc_cond, &doc_mutex);
        }
        // Lock the main mutex to ensure safe access to shared variables
        pthread_mutex_lock(&mutex);
        // Check if the doctor is leaving
        if(doc_state==-1){
            printf("[%s] Doctor leaves\n", format_time(curr_time, time_buff));
            // Set a flag indicating the end of doctor's session
            doc_session_end = 1;
            // Unlock the main mutex and the doctor's mutex before exiting
            pthread_mutex_unlock(&mutex);
            pthread_mutex_unlock(&doc_mutex);
            // Exit the loop and return from the function
            break;
        }
        else{
            // Print a message indicating the doctor's next visitor
            printf("[%s] Doctor has next visitor\n", format_time(curr_time, time_buff));
            // Unlock the main mutex before updating the doctor's state
            pthread_mutex_unlock(&mutex);
            // Reset the doctor's state to idle and signal the visitor that the doctor is available
            doc_state=0;
            pthread_cond_signal(&doc_done);
            // Unlock the doctor's mutex
            pthread_mutex_unlock(&doc_mutex);
        }
    }

    return NULL;
}

void *patient(void *id_param){
    char time_buff1[10], time_buff2[10];

    // Extract patient ID and duration information from the passed parameter
    param *patient_id = (param *)id_param;
    param id = *patient_id;

    // Signal that this patient thread has been initialized
    pthread_mutex_lock(&mutex);
    patient_done = 1;
    pthread_cond_signal(&patient_init);
    pthread_mutex_unlock(&mutex);

    // Wait until it's this patient's turn
    pthread_mutex_lock(&mutex);
    while(patient_curr != id.id){
        pthread_cond_wait(&patient_cond, &mutex);
    }
    // Print a message indicating the patient's entry into the doctor's chamber
    printf("[%s - %s] Patient %d is in doctor's chamber\n", format_time(curr_time, time_buff1), format_time(curr_time+id.e.duration, time_buff2), id.id);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void *reporter(void *id_param){
    char time_buff1[10], time_buff2[10];

    // Extract reporter ID and duration information from the passed parameter
    param *reporter_id = (param *)id_param;
    param id = *reporter_id;

    // Signal that this reporter thread has been initialized
    pthread_mutex_lock(&mutex);
    reporter_done = 1;
    pthread_cond_signal(&reporter_init);
    pthread_mutex_unlock(&mutex);

    // Wait until it's this reporter's turn
    pthread_mutex_lock(&mutex);
    while(reporter_curr != id.id){
        pthread_cond_wait(&reporter_cond, &mutex);
    }
    // Print a message indicating the reporter's entry into the doctor's chamber
    printf("[%s - %s] Reporter %d is in doctor's chamber\n", format_time(curr_time, time_buff1), format_time(curr_time+id.e.duration, time_buff2), id.id);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void *salesrep(void *id_param){
    char time_buff1[10], time_buff2[10];

    // Extract sales representative ID and duration information from the passed parameter
    param *salesrep_id = (param *)id_param;
    param id = *salesrep_id;

    // Signal that this sales representative thread has been initialized
    pthread_mutex_lock(&mutex);
    salesrep_done = 1;
    pthread_cond_signal(&salesrep_init);
    pthread_mutex_unlock(&mutex);

    // Wait until it's this sales representative's turn
    pthread_mutex_lock(&mutex);
    while(salesrep_curr != id.id){
        pthread_cond_wait(&salesrep_cond, &mutex);
    }
    // Print a message indicating the sales representative's entry into the doctor's chamber
    printf("[%s - %s] Sales representative %d is in doctor's chamber\n", format_time(curr_time, time_buff1), format_time(curr_time+id.e.duration, time_buff2), id.id);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main(){
    // Initialize mutexes and condition variables
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&doc_mutex, NULL);
    pthread_cond_init(&doc_cond, NULL);
    pthread_cond_init(&doc_done, NULL);
    pthread_cond_init(&patient_cond, NULL);
    pthread_cond_init(&salesrep_cond, NULL);
    pthread_cond_init(&reporter_cond, NULL);
    pthread_cond_init(&patient_init, NULL);
    pthread_cond_init(&reporter_init, NULL);
    pthread_cond_init(&salesrep_init, NULL);

    char time_buff[10];

    // Initialize event queues and other variables
    pthread_t patient_tids[25];
    pthread_t salesrep_tids[3];
    pthread_t reporter_tids[100];

    eventQ E = initEQ("arrival.txt");
    event temp;
    temp.type = 'D';
    temp.time = 0;
    E = addevent(E, temp);

    pthread_t doctor_tid;
    pthread_create(&doctor_tid, NULL, doctor, NULL);
    // this will store the visitors of each type separately, in the sequential order
    event reporterQ[100];
    event patientQ[25];
    event salesrepQ[3];

    while(!emptyQ(E)){
        event newevent = nextevent(E);
        E = delevent(E);

        pthread_mutex_lock(&mutex);
        curr_time = newevent.time;

        // Handle different types of events
        if(newevent.type=='D'){ 
            // Doctor is available
            doc_free = 1;

            // Check if the session is over and notify the doctor
            if(patient_curr==25 && salesrep_curr==3 && reporter_wait==0){
                pthread_mutex_lock(&doc_mutex);
                doc_state = -1;
                pthread_cond_signal(&doc_cond);
                pthread_mutex_unlock(&doc_mutex);
                doc_session_end = 1;
                doc_free = 0;
                pthread_mutex_unlock(&mutex);
                // Wait for the doctor thread to finish and continue to the next event
                pthread_join(doctor_tid, NULL);
                continue;
            }
            
            pthread_mutex_unlock(&mutex);
        }
        else if(newevent.type=='P'){
            // Increment patient count and print arrival message with the current time
            patient_cnt++;
            printf("\t[%s] Patient %d arrives\n", format_time(curr_time, time_buff), patient_cnt);

            // Check if the doctor's session is over, if so, patient leaves and loop continues
            if(doc_session_end){
                printf("\t[%s] Patient %d leaves (session over)\n", format_time(curr_time, time_buff), patient_cnt);
                pthread_mutex_unlock(&mutex);
                continue;
            }

            // Check if patient quota is full, if so, patient leaves and loop continues
            if(patient_cnt > 25){
                printf("\t[%s] Patient %d leaves (quota full)\n", format_time(curr_time, time_buff), patient_cnt);
                pthread_mutex_unlock(&mutex);
                continue;
            }
            // Increment patient wait count
            patient_wait++;
            int curr_id = patient_cnt;
            patient_done=0;
            pthread_mutex_unlock(&mutex);

            // Create a new parameter structure for the patient and start a new patient thread
            param new_patient;
            new_patient.e = newevent;
            new_patient.id = curr_id;
            pthread_t new_patient_tid;
            pthread_create(&new_patient_tid, NULL, patient, (void *)&new_patient);

            // Wait until the patient thread is initialized
            pthread_mutex_lock(&mutex);
            while(patient_done==0){
                pthread_cond_wait(&patient_init, &mutex);
            }
            patient_done=0;
            pthread_mutex_unlock(&mutex);

            // Store patient event and thread ID in corresponding arrays
            patientQ[curr_id-1] = newevent;
            patient_tids[curr_id-1] = new_patient_tid;
        }
        else if(newevent.type=='R'){
            // Increment reporter count and print arrival message with the current time
            reporter_cnt++;
            printf("\t[%s] Reporter %d arrives\n", format_time(curr_time, time_buff), reporter_cnt);

            // Check if the doctor's session is over, if so, reporter leaves and loop continues
            if(doc_session_end){
                printf("\t[%s] Reporter %d leaves (session over)\n", format_time(curr_time, time_buff), reporter_cnt);
                pthread_mutex_unlock(&mutex);
                continue;
            }

            // Increment reporter wait count
            reporter_wait++;
            int curr_id = reporter_cnt;
            reporter_done = 0;
            pthread_mutex_unlock(&mutex);

            // Create a new parameter structure for the reporter and start a new reporter thread
            param new_reporter;
            new_reporter.e = newevent;
            new_reporter.id = curr_id;
            pthread_t new_reporter_tid;
            pthread_create(&new_reporter_tid, NULL, reporter, (void *)&new_reporter);

            // Wait until the reporter thread is initialized
            pthread_mutex_lock(&mutex);
            while(reporter_done == 0){
                pthread_cond_wait(&reporter_init, &mutex);
            }
            reporter_done = 0;
            pthread_mutex_unlock(&mutex);

            // Store reporter event and thread ID in corresponding arrays
            reporterQ[curr_id-1] = newevent;
            reporter_tids[curr_id-1] = new_reporter_tid;
        }
        else if(newevent.type=='S'){
            // Increment sales representative count and print arrival message with the current time
            salesrep_cnt++;
            printf("\t[%s] Sales representative %d arrives\n", format_time(curr_time, time_buff), salesrep_cnt);

            // Check if the doctor's session is over, if so, sales representative leaves and loop continues
            if(doc_session_end){
                printf("\t[%s] Sales representative %d leaves (session over)\n", format_time(curr_time, time_buff), salesrep_cnt);
                pthread_mutex_unlock(&mutex);
                continue;
            }

            // Check if the maximum number of sales representatives has been reached, if so, sales representative leaves and loop continues
            if(salesrep_cnt > 3){
                printf("\t[%s] Sales representative %d leaves (quota full)\n", format_time(curr_time, time_buff), salesrep_cnt);
                pthread_mutex_unlock(&mutex);
                continue;
            }

            // Increment sales representative wait count
            salesrep_wait++;
            int curr_id = salesrep_cnt;
            salesrep_done = 0;
            pthread_mutex_unlock(&mutex);

            // Create a new parameter structure for the sales representative and start a new sales representative thread
            param new_salesrep;
            new_salesrep.e = newevent;
            new_salesrep.id = curr_id;
            pthread_t new_salesrep_tid;
            pthread_create(&new_salesrep_tid, NULL, salesrep, (void *)&new_salesrep);

            // Wait until the sales representative thread is initialized
            pthread_mutex_lock(&mutex);
            while(salesrep_done == 0){
                pthread_cond_wait(&salesrep_init, &mutex);
            }
            salesrep_done = 0;
            pthread_mutex_unlock(&mutex);

            // Store sales representative event and thread ID in corresponding arrays
            salesrepQ[curr_id-1] = newevent;
            salesrep_tids[curr_id-1] = new_salesrep_tid;
        }


        pthread_mutex_lock(&mutex);
        // Check if the doctor is free and the session is ongoing
        if(doc_free == 1 && doc_session_end == 0){
            // If there are reporters waiting, prioritize them
            if(reporter_wait > 0){
                // Get the next reporter from the queue
                event visitor = reporterQ[reporter_curr];
                // Decrement reporter wait count
                reporter_wait--;
                // Indicate that the doctor is busy
                doc_free = 0;
                pthread_mutex_unlock(&mutex);

                // Lock the doctor's mutex and signal the doctor is ready
                pthread_mutex_lock(&doc_mutex);
                doc_state = 1;
                pthread_cond_signal(&doc_cond);
                pthread_mutex_unlock(&doc_mutex);

                // Wait for the doctor to finish with the reporter
                pthread_mutex_lock(&doc_mutex);
                while(doc_state == 1){
                    pthread_cond_wait(&doc_done, &doc_mutex);
                }
                pthread_mutex_unlock(&doc_mutex);

                // Lock the mutex again to update shared variables
                pthread_mutex_lock(&mutex);
                // Move to the next reporter in the queue
                reporter_curr++;
                int curr_id = reporter_curr;
                // Signal waiting reporters
                pthread_cond_broadcast(&reporter_cond);
                // Record the current time
                int current_time = curr_time;
                pthread_mutex_unlock(&mutex);

                // Wait for the reporter thread to finish
                pthread_join(reporter_tids[curr_id - 1], NULL);

                // Create an event marking the end of the reporter's visit
                event visitor_done;
                visitor_done.type = 'D';
                visitor_done.time = current_time + visitor.duration;
                // Add the event to the event queue
                E = addevent(E, visitor_done);
            }
            // If there are patients waiting, give them the next priority
            else if(patient_wait > 0){
                // Get the next patient from the queue
                event visitor = patientQ[patient_curr];
                // Decrement patient wait count
                patient_wait--;
                // Indicate that the doctor is busy
                doc_free = 0;
                pthread_mutex_unlock(&mutex);

                // Lock the doctor's mutex and signal the doctor is ready
                pthread_mutex_lock(&doc_mutex);
                doc_state = 1;
                pthread_cond_signal(&doc_cond);
                pthread_mutex_unlock(&doc_mutex);

                // Wait for the doctor to finish with the patient
                pthread_mutex_lock(&doc_mutex);
                while(doc_state == 1){
                    pthread_cond_wait(&doc_done, &doc_mutex);
                }
                pthread_mutex_unlock(&doc_mutex);

                // Lock the mutex again to update shared variables
                pthread_mutex_lock(&mutex);
                // Move to the next patient in the queue
                patient_curr++;
                int curr_id = patient_curr;
                // Signal waiting patients
                pthread_cond_broadcast(&patient_cond);
                // Record the current time
                int current_time = curr_time;
                pthread_mutex_unlock(&mutex);

                // Wait for the patient thread to finish
                pthread_join(patient_tids[curr_id - 1], NULL);

                // Create an event marking the end of the patient's visit
                event visitor_done;
                visitor_done.type = 'D';
                visitor_done.time = current_time + visitor.duration;
                // Add the event to the event queue
                E = addevent(E, visitor_done);
            }
            // If there are sales representatives waiting, with the last priority
            else if(salesrep_wait > 0){
                // Get the next sales representative from the queue
                event visitor = salesrepQ[salesrep_curr];
                // Decrement sales representative wait count
                salesrep_wait--;
                // Indicate that the doctor is busy
                doc_free = 0;
                pthread_mutex_unlock(&mutex);

                // Lock the doctor's mutex and signal the doctor is ready
                pthread_mutex_lock(&doc_mutex);
                doc_state = 1;
                pthread_cond_signal(&doc_cond);
                pthread_mutex_unlock(&doc_mutex);

                // Wait for the doctor to finish with the sales representative
                pthread_mutex_lock(&doc_mutex);
                while(doc_state == 1){
                    pthread_cond_wait(&doc_done, &doc_mutex);
                }
                pthread_mutex_unlock(&doc_mutex);

                // Lock the mutex again to update shared variables
                pthread_mutex_lock(&mutex);
                // Move to the next sales representative in the queue
                salesrep_curr++;
                int curr_id = salesrep_curr;
                // Signal waiting sales representatives
                pthread_cond_broadcast(&salesrep_cond);
                // Record the current time
                int current_time = curr_time;
                pthread_mutex_unlock(&mutex);

                // Wait for the sales representative thread to finish
                pthread_join(salesrep_tids[curr_id - 1], NULL);

                // Create an event marking the end of the sales representative's visit
                event visitor_done;
                visitor_done.type = 'D';
                visitor_done.time = current_time + visitor.duration;
                // Add the event to the event queue
                E = addevent(E, visitor_done);
            }
            // If there are no visitors waiting
            else{
                pthread_mutex_unlock(&mutex);
            }
        }
        // If the doctor is not free or the session is over
        else{
            pthread_mutex_unlock(&mutex);
        }

    }

    // Destroy mutexes and condition variables
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&doc_mutex);
    pthread_cond_destroy(&doc_cond);
    pthread_cond_destroy(&doc_done);
    pthread_cond_destroy(&patient_cond);
    pthread_cond_destroy(&reporter_cond);
    pthread_cond_destroy(&salesrep_cond);
    pthread_cond_destroy(&patient_init);
    pthread_cond_destroy(&reporter_init);
    pthread_cond_destroy(&salesrep_init);


    return 0;
}