// File:	worker_t.h

// List all group member's name:
// username of iLab:
// iLab Server:

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1

#define STACK_SIZE SIGSTKSZ

#define TOTAL_QUEUES 4

#define QUANTUM 250000

#define S QUANTUM * 75

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include<time.h>

enum thread_status{running, exitted, ready, blocked};

typedef uint worker_t;

typedef struct TCB {
	/* add important states in a thread control block */
	// thread Id
	// thread status
	// thread context
	// thread stack
	// thread priority
	// And more ...

	// YOUR CODE HERE
	worker_t id;
	ucontext_t context; 
	int priority; 
	int priorityPSJF;
	void* stack; 
	enum thread_status status;
	int elapsed;
	int response_flag;
	clock_t start_time; 

} tcb; 
typedef struct node{
	tcb * thread_control_block; 
	struct node * next; 

} node; 

typedef struct deque{
	node* head; 
	node* tail; 
} deque; 


/* mutex struct definition */
typedef struct worker_mutex_t {
	int id; 
	deque * blocked_threads;
	int available; 
} worker_mutex_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE


/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
void worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);


/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

void enqueue(node * new_node);
void enqueueExit(node *new_node); 

void enqueue_mutex(node* new_node, deque* q); 

void enqueue_buffer(node* new_node); 
node* dequeue_buffer(); 

node * dequeueMLFQ(); 
void enqueueMLFQ(node* new_node); 

int found_in_exit_queue(worker_t thread);

node * dequeue_mutex(deque* q); 
node * dequeue(); 

void handle(int signum);

void pop(worker_t thread);
void dequeuePSJF(node *exitNode);
void setCurrentlyRunningPSJF();
void enqueuePSJF(node *new_node);

static void main_context(); 
static void schedule(); 
static void sched_psjf(); 
static void sched_mlfq(); 

void changePriority(node* newNode);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#endif

#endif
