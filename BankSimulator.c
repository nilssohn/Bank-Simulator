#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>

#define NO_OF_TELLERS (3)
#define TOTAL_TIME (420)
#define HUNDRED_MS  (100000000L)
#define QUEUE (3)
#define TELLER_1 (0)
#define TELLER_2 (1)
#define TELLER_3 (2)

#define TRUE (1)
#define FALSE (0)

// stats
int total_customers, avg_queue_time, avg_service_time, avg_teller_wait_time,
max_queue_wait_time, max_teller_wait_time, max_service_time, max_queue_length;

// grad stats
int breaks[NO_OF_TELLERS], avg_break_time[NO_OF_TELLERS], longest_break_time[NO_OF_TELLERS], shortest_break_time[NO_OF_TELLERS];

int wait_start_time[200];
int wait_end_time[200];
int customers_served;
int break_time[3];
int teller_wait_time_start[3];
int teller_busy[3];
int teller_thread_count;
int queue_thread_count;
int clock_thread_count;
int queue_size;
int global_time;

pthread_mutex_t queue_mutex;

// thread functions
void *teller_process(void *arg);
void *clock_process(void *arg);
void *queue_process(void *arg);
void add_to_queue(union sigval val);
void decrement_clock(union sigval val);
void break_time_set_0(union sigval val);
void break_time_set_1(union sigval val);
void break_time_set_2(union sigval val);
int generate_rand(int min, int max);
void set_break_time(int id);

// queue thread
void *queue_process(void *arg) {
    // Setup for the timer
    struct itimerspec queue_tim;
    struct sigevent queue_event;
    timer_t queue_timer_id;
	queue_tim.it_value.tv_sec = 0;
	queue_tim.it_value.tv_nsec;
	queue_tim.it_interval.tv_sec = 0;
	queue_tim.it_interval.tv_nsec = 0;
	queue_event.sigev_notify = SIGEV_THREAD;                                       // timer expire starts a new thread
	queue_event.sigev_value.sival_ptr = &queue_timer_id;
	queue_event.sigev_notify_function = &add_to_queue;                             // add_to_queue thread runs
	queue_event.sigev_notify_attributes = NULL;
	timer_create(CLOCK_MONOTONIC, &queue_event, &queue_timer_id);
	
    // while the day is still there
    while (global_time > 0) {
        max_queue_length = queue_size > max_queue_length 
                ? queue_size 
                : max_queue_length;

        // queue is locked until the timer runs out
        if (queue_thread_count == 0) {
            queue_thread_count = 1;
            queue_tim.it_value.tv_nsec = generate_rand(1, 4) * HUNDRED_MS;          // next customer arrival
            timer_settime(queue_timer_id, 0, &queue_tim, 0);                        // timer set
        }
    }
}

// handles the results of customer arrival timing having elapsed
void add_to_queue(union sigval val) {
	wait_start_time[total_customers] = global_time;                                 // metrics
	// counters and accumulators are updated
    queue_size++;
	total_customers++;
	queue_thread_count--;
}


// teller processes
void *teller_process(void *arg) {
    int *id = (int *) arg;
    int wait_time = 0;
    int time = 0;
    struct timespec teller_time;
    teller_time.tv_sec = 0;
    set_break_time(*id);

    // teller thread handles the transactions and servicing.
    while (global_time > 0 || queue_size > 0) {
        pthread_mutex_lock(&queue_mutex);                                           // shared resource is locked
        if (break_time[*id] == 1) {                                                 // break flag is set for teller with id
            pthread_mutex_unlock(&queue_mutex);
            if (teller_wait_time_start[*id] > 0) {                                  // metrics are gathered if the teller started waiting
                wait_time = teller_wait_time_start[*id] - global_time;
                avg_teller_wait_time += wait_time;
                teller_wait_time_start[*id] = 0;
                max_teller_wait_time = max_teller_wait_time > wait_time
                ? max_teller_wait_time
                : wait_time;
            }
            breaks[*id]++;
            set_break_time(*id);
            time = generate_rand(1, 4);
            avg_break_time[*id] += time;
            longest_break_time[*id] = longest_break_time[*id] > time
                    ? longest_break_time[*id]
                    : time;
            shortest_break_time[*id] = shortest_break_time[*id] < time
                    ? shortest_break_time[*id]
                    : time;
            teller_time.tv_nsec = (time * HUNDRED_MS)/2;
            nanosleep(&teller_time, NULL);
            break_time[*id] = 0;
        } else if (queue_size > 0) {                                                // handles metrics gathering and process timer
            queue_size--;
            wait_end_time[customers_served] = global_time;
            customers_served++;
            pthread_mutex_unlock(&queue_mutex);
            if (teller_wait_time_start[*id] > 0) {
                wait_time = teller_wait_time_start[*id] - global_time;
                avg_teller_wait_time += wait_time;
                teller_wait_time_start[*id] = 0;
                max_teller_wait_time = max_teller_wait_time > wait_time
                ? max_teller_wait_time
                : wait_time;
            }
            time = (generate_rand(1, 16) * HUNDRED_MS)/2;
            max_service_time = max_service_time > (time / HUNDRED_MS)
                ? max_service_time
                : (time/HUNDRED_MS);
            avg_service_time += (time / HUNDRED_MS);
            teller_time.tv_nsec = time;
            nanosleep(&teller_time, NULL);
        } else {                                                                    // waiting metric starts if not already waiting
            pthread_mutex_unlock(&queue_mutex);
            if (teller_wait_time_start[*id] == 0) {
                teller_wait_time_start[*id] = global_time;
            }
        }
    }
}

// starts the new break time
void set_break_time(int id) {
    struct itimerspec break_tim;
    struct sigevent break_event;
    timer_t break_timer_id;


    switch (id) {
        case 0:
            break_event.sigev_notify_function = &break_time_set_0;
            break;
        case 1:
            break_event.sigev_notify_function = &break_time_set_1;
            break;
        case 2:
            break_event.sigev_notify_function = &break_time_set_2;
            break;
        default:
            return;
    }

    break_tim.it_interval.tv_sec = 0;
    break_tim.it_interval.tv_nsec = 0;
    break_event.sigev_notify = SIGEV_THREAD;
    break_event.sigev_value.sival_ptr = &break_timer_id;
    break_event.sigev_notify_attributes = NULL;
    timer_create(CLOCK_MONOTONIC, &break_event, &break_timer_id);

    // generate the time of next break
    int time = generate_rand(30, 60);
    int sec = time / 10;
    long nsec = (time % 10) * HUNDRED_MS;
    break_tim.it_value.tv_sec = sec;
    break_tim.it_value.tv_nsec = nsec;
    timer_settime(break_timer_id, 0, &break_tim, 0);
}

// different break time functions for each teller
// sigevent doesn't allow values to be passed
void break_time_set_0(union sigval val) {
    if (global_time > 8) {
        break_time[0] = 1;
    }
}

void break_time_set_1(union sigval val) {
    if (global_time > 8) {
        break_time[1] = 1;
    }
}

void break_time_set_2(union sigval val) {
    if (global_time > 8) {
        break_time[2] = 1;
    }
}

// decrements clock process
void *clock_process(void *arg) {
    struct itimerspec clock_tim;
    struct sigevent clock_event;
    timer_t clock_timer_id;
    clock_tim.it_value.tv_sec = 0;
    clock_tim.it_value.tv_nsec = HUNDRED_MS;
    clock_tim.it_interval.tv_sec = 0;
    clock_tim.it_interval.tv_nsec = HUNDRED_MS;                                 // The clock runs in intervals, expired timer resets
    clock_event.sigev_notify = SIGEV_THREAD;
    clock_event.sigev_value.sival_ptr = &clock_timer_id;
    clock_event.sigev_notify_function = decrement_clock;
    clock_event.sigev_notify_attributes = NULL;
    timer_create(CLOCK_MONOTONIC, &clock_event, &clock_timer_id);
    timer_settime(clock_timer_id, 0, &clock_tim, 0);
}


void decrement_clock(union sigval val) {
	clock_thread_count = 0;
	global_time--;
	avg_queue_time += queue_size;
}

int generate_rand(int min, int max) {
    return (rand() % (max - min + 1)) + min;
}

int initialize_global_variables() {
    total_customers = 0;
    avg_queue_time = 0;
    avg_service_time = 0;
    avg_teller_wait_time = 0;
    max_queue_wait_time = 0;
    max_teller_wait_time = 0;
    max_service_time = 0;
    max_queue_length = 0;
    queue_thread_count = 0;

    int i = 0;
    while (i < NO_OF_TELLERS) {
        break_time[i] = 0;
        breaks[i] = 0;
        avg_break_time[i] = 0;
        longest_break_time[i] = 0;
        shortest_break_time[i] = 99;
        teller_busy[i] = 0;
        teller_wait_time_start[i] = 0;
        i++;
    }

    queue_size = 0;
    global_time = TOTAL_TIME;
    pthread_mutex_init(&queue_mutex, NULL);
}

int main() {
    initialize_global_variables();
    srand(time(NULL));

    int id[3];
    int queue_id;
    pthread_t queue_thread;
    pthread_t clock_thread;
    pthread_t teller_threads[3];

    struct itimerspec teller_tim[3];
    struct sigevent teller_event[3];
    timer_t teller_timer_id[3];

    // start all threads
    printf("Bank Open!\n");
    pthread_create(&clock_thread, NULL, clock_process, NULL);
    pthread_create(&queue_thread, NULL, queue_process, NULL);
    id[0] = 0;
    pthread_create(&teller_threads[0], NULL, teller_process, (void *) &id[0]);
    id[1] = 1;
    pthread_create(&teller_threads[1], NULL, teller_process, (void *) &id[1]);
    id[2] = 2;
    pthread_create(&teller_threads[2], NULL, teller_process, (void *) &id[2]);

    // main waits on these functions to end
    pthread_join(teller_threads[0], NULL);
    pthread_join(teller_threads[1], NULL);
    pthread_join(teller_threads[2], NULL);

    printf("Bank Closed!\n");

    // metrics calculations
    double avg_queue_time_frac = ((double)avg_queue_time) / ((double)total_customers);
    avg_service_time /= total_customers;
    avg_teller_wait_time /= NO_OF_TELLERS;
    int i = 0;

    while (i < NO_OF_TELLERS) {
        avg_break_time[i] /= breaks[i];
        i++;
    }

    i = 0;
    int wait_time;
    while (i < total_customers) {
        wait_time = wait_start_time[i] - wait_end_time[i];
        max_queue_wait_time = max_queue_wait_time > wait_time
        ? max_queue_wait_time
        : wait_time;
        i++;
    }

    printf("\n\nMetrics:\n");
    printf("Total_customers: %d\n", total_customers);
    printf("Average time in queue: %.2f mins\n", avg_queue_time_frac);
    printf("Average service time (minutes): %d mins\n", avg_service_time);
    printf("Average teller wait time (minutes): %d mins\n", avg_teller_wait_time);
    printf("Max queue wait time (minutes): %d mins\n", max_queue_wait_time);
    printf("Max teller wait time (minutes): %d mins\n", max_teller_wait_time);
    printf("Max queue length: %d\n", max_queue_length);
    printf("Max service time: %d\n", max_service_time);
    printf("\n\nGrad metrics: \n");
    printf("Number of breaks (minutes): %d %d %d\n", breaks[0], breaks[1], breaks[2]);
    printf("Average break time (minutes): %d %d %d\n", avg_break_time[0], avg_break_time[1], avg_break_time[2]);
    printf("Longest break time (minutes): %d %d %d\n", longest_break_time[0], longest_break_time[1], longest_break_time[2]);
    printf("Shortest break time (minutes): %d %d %d\n", shortest_break_time[0], shortest_break_time[1], shortest_break_time[2]);
    //int total_customers, avg_queue_time, avg_service_time, avg_teller_wait_time,
    //max_queue_wait_time, max_teller_wait_time, max_service_time, max_queue_length;
    // grad stats
    //int breaks[NO_OF_TELLERS], avg_break_time[NO_OF_TELLERS], longest_break_time[NO_OF_TELLERS], shortest_break_time[NO_OF_TELLERS];

    printf("Goodbye...\n");
}
