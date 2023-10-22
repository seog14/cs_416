// File:	thread-worker.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "thread-worker.h"

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;

long id_incrementer = 1; 

// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE

ucontext_t scheduler_context; 
deque queue={NULL, NULL}; 

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

	if (queue.head == NULL){
		// Initialize Scheduler 
		if (getcontext(&scheduler_context) < 0){
			perror("getcontext"); 
			exit(1);
		}		
		void * scheduler_stack=malloc(STACK_SIZE); 
		scheduler_context.uc_link=NULL; 
		scheduler_context.uc_stack.ss_flags=0; 
		scheduler_context.uc_stack.ss_sp=scheduler_stack; 
		scheduler_context.uc_stack.ss_size=STACK_SIZE; 
		makecontext(&scheduler_context, &schedule, 0); 

		// Create main thread control block 
		tcb * main_control_block = malloc(sizeof(tcb)); 
		main_control_block->id = id_incrementer; 
		id_incrementer++; 
		void * main_stack=malloc(STACK_SIZE);
		main_control_block->stack=main_stack; 
		main_control_block->priority=0; 
		main_control_block->status = ready;

		// Create and initialize main context 
		if (getcontext(&(main_control_block->context)) < 0){
			perror("getcontext"); 
			exit(1);
		}
		main_control_block->context.uc_link=NULL; 
		main_control_block->context.uc_stack.ss_flags=0; 
		main_control_block->context.uc_stack.ss_sp=main_control_block->stack; 
		main_control_block->context.uc_stack.ss_size=STACK_SIZE; 
		makecontext(&(main_control_block->context), &main_context, 0); 

		// Enqueue main into queue 
		node* main_node = malloc(sizeof(node)); 
		main_node->thread_control_block = main_control_block; 
		main_node->next = NULL; 
		enqueue(main_node); 

		// Create tcb for thread 
		tcb * thread_control_block = malloc(sizeof(tcb)); 
		*thread = id_incrementer; 
		thread_control_block->id = id_incrementer; 
		id_incrementer++; 
		void *stack=malloc(STACK_SIZE); 
		thread_control_block->stack=stack; 
		thread_control_block->priority=0; 
		thread_control_block->status = ready;

		// Create and initialize context 
		if (getcontext(&(thread_control_block->context)) < 0){
			perror("getcontext"); 
			return 1; 
		}
		thread_control_block->context.uc_link=NULL; 
		thread_control_block->context.uc_stack.ss_flags=0; 
		thread_control_block->context.uc_stack.ss_sp=thread_control_block->stack; 
		thread_control_block->context.uc_stack.ss_size=STACK_SIZE; 
		makecontext(&(thread_control_block->context), (void (*) (void))function, 1, arg); 

		// Enqueue thread context
		node * curr_thread = malloc(sizeof(node)); 
		curr_thread->thread_control_block = thread_control_block; 
		curr_thread->next = NULL; 
		enqueue(curr_thread); 

		// Initialize Timer 
		// Use sigaction to register signal handler
		struct sigaction sa;
		memset (&sa, 0, sizeof (sa));
		sa.sa_handler = &handle;
		sigaction (SIGPROF, &sa, NULL);

		// Create timer struct
		struct itimerval timer;

		// Set up what the timer should reset to after the timer goes off
		timer.it_interval.tv_usec = 0; 
		timer.it_interval.tv_sec = 1;

		// Set up the current timer to go off in 1 second
		// Note: if both of the following values are zero
		//       the timer will not be active, and the timer
		//       will never go off even if you set the interval value
		timer.it_value.tv_usec = 0;
		timer.it_value.tv_sec = 1;
		// Set the timer up (start the timer)
		setitimer(ITIMER_PROF, &timer, NULL); 
		return 0; 
	}
	// Create thread
	tcb * thread_control_block = malloc(sizeof(tcb)); 
	*thread = id_incrementer; 
	thread_control_block->id = id_incrementer; 
	id_incrementer++; 
	void *stack=malloc(STACK_SIZE); 
	thread_control_block->stack=stack; 
	thread_control_block->priority=0; 
	thread_control_block->status = ready;

	// Create and initialize context 
	if (getcontext(&(thread_control_block->context)) < 0){
		perror("getcontext"); 
		return 1; 
	}
	thread_control_block->context.uc_link=NULL; 
	thread_control_block->context.uc_stack.ss_flags=0; 
	thread_control_block->context.uc_stack.ss_sp=thread_control_block->stack; 
	thread_control_block->context.uc_stack.ss_size=STACK_SIZE; 
	makecontext(&(thread_control_block->context), (void (*) (void))function, 1, arg); 

	// Push thread into queue
	node * curr_thread = malloc(sizeof(node)); 
	curr_thread->thread_control_block = thread_control_block; 
	curr_thread->next = NULL; 
	enqueue(curr_thread); 

    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
void  worker_yield() {
	swapcontext(&(queue.head->thread_control_block->context), &scheduler_context);
	
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread
	//free context stack
	free(queue.head->thread_control_block->stack); 
	queue.head->thread_control_block->status=exitted; 
	worker_yield(); 
	return;
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	node * temp = queue.head; 
	node * joining_thread_node = NULL; 
	while(temp != NULL){
		if(temp->thread_control_block->id ==thread){
			joining_thread_node = temp; 
			break; 
		}
		temp = temp->next; 
	}
	if (joining_thread_node == NULL){
		perror("Invalid joining thread");
		return 1; 
	}
	tcb * joining_thread = joining_thread_node->thread_control_block;
	while(1){
		if(joining_thread->status==exitted){
			break;
		}
	}
	pop(thread); 
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

	// YOUR CODE HERE
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init

	return 0;
};

static void main_context(){
	return; 
}

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function


	// - invoke scheduling algorithms according to the policy (PSJF or MLFQ)

	// if (sched == PSJF)
	//		sched_psjf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

// - schedule policy
#ifndef MLFQ
	node* current_node = dequeue(); 
	while(current_node->thread_control_block->status == exitted){
		enqueue(current_node); 
		current_node = dequeue(); 
	}
	current_node->thread_control_block->status = ready; 
	enqueue(current_node);
	node * temp; 
	while(queue.head->thread_control_block->status == exitted){
		temp = dequeue(); 
		enqueue(temp); 
	}
	queue.head->thread_control_block->status = running; 
	makecontext(&scheduler_context, &schedule, 0);
	setcontext(&(queue.head->thread_control_block->context)); 
#else 
	// Choose MLFQ
#endif

}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

//DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {

       fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
       fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
       fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}


// Feel free to add any other functions you need

// YOUR CODE HERE


void enqueue(node* new_node){
	if (queue.head == NULL){
		queue.head = new_node; 
		queue.tail = new_node; 
		return;
	}
	node * temp_node = queue.head; 
	while(temp_node->next != NULL){
		temp_node = temp_node->next; 
	}
	temp_node->next = new_node; 
	queue.tail = new_node; 
	return; 
}

node * dequeue(){
	if(queue.head == NULL){
		return NULL; 
	}
	if(queue.tail == queue.head){
		node * temp_node = queue.tail; 
		queue.tail = NULL; 
		queue.head = NULL; 
		return temp_node; 
	}
	node * temp_node = queue.head; 
	queue.head = queue.head->next; 
	temp_node->next = NULL; 
	return temp_node; 
}

void handle(int signum){
	swapcontext(&(queue.head->thread_control_block->context), &scheduler_context);
}

void pop(worker_t thread){

	node * temp = queue.head; 
	node * selected_thread = NULL; 
	while(temp!= NULL){
		if(temp->thread_control_block->id==thread){
			selected_thread = temp; 
			break; 
		}
		temp = temp->next; 
	}
	if (selected_thread == NULL){
		perror("Invalid joining thread");
		exit(1); 
	}
	if (selected_thread == queue.head){
		selected_thread = dequeue(); 
		free(selected_thread->thread_control_block); 
		free(selected_thread); 
		return; 
	}
	node * after = selected_thread->next; 
	node * before = queue.head; 
	while(before->next!=selected_thread){
		before = before->next; 
	}
	before->next = after; 
	free(selected_thread->thread_control_block); 
	free(selected_thread); 
	return; 
}