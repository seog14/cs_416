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
long mutex_id_incrementer = 1;  

// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE

ucontext_t scheduler_context; 
deque queue={NULL, NULL};
deque *MLFQList[TOTAL_QUEUES];
node *currentlyRunningNode = NULL;
int timers;

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

	if (currentlyRunningNode == NULL){
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

		int i;

		for (i = 0; i < TOTAL_QUEUES; i++){
			MLFQList[i] = malloc(sizeof(deque));
			MLFQList[i]->head = NULL;
			MLFQList[i]->tail = NULL;
		}

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
	// timer first goes here then to scheduler
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
	mutex->id = mutex_id_incrementer; 
	deque q = {NULL, NULL};
	mutex->blocked_threads = &q; 
	mutex->available = 0; 
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {
	if (__sync_lock_test_and_set(&(mutex->available), 1) == 0){
		return 0; 
	}
	else{
		node * current_node = queue.head; 
		current_node->thread_control_block->status == blocked; 
		enqueue_mutex(current_node, mutex->blocked_threads); 
		swapcontext(&(queue.head->thread_control_block->context), &scheduler_context);
	}

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
    return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	__sync_lock_release(&(mutex->available));
	node * temp; 
	while(mutex->blocked_threads->head != NULL){
		temp = dequeue_mutex(mutex->blocked_threads); 
		enqueue(temp); 
	}
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
	if (queue.head->thread_control_block->status == running){
		node * current_node = dequeue(); 
		current_node->thread_control_block->status = ready; 
		enqueue(current_node); 
		node * temp; 
		while(queue.head->thread_control_block->status == exitted || queue.head->thread_control_block->status == blocked){
			if (queue.head->thread_control_block->status == exitted){
				temp = dequeue(); 
				enqueue(temp); 
			}
			else{
				temp = dequeue(); 
			}
		}	
		queue.head->thread_control_block->status = running; 
		makecontext(&scheduler_context, &schedule, 0);
		setcontext(&(queue.head->thread_control_block->context)); 	
	}
	if(queue.head->thread_control_block->status == ready){
		queue.head->thread_control_block->status = running; 
		makecontext(&scheduler_context, &schedule, 0);
		setcontext(&(queue.head->thread_control_block->context)); 
	}
	while(queue.head->thread_control_block->status == blocked || queue.head->thread_control_block->status == exitted){
		if (queue.head->thread_control_block->status == blocked){
			dequeue();  
		}
		else{
			node * current_node = dequeue(); 
			enqueue(current_node); 
		}
	}
	queue.head->thread_control_block->status = running; 
	makecontext(&scheduler_context, &schedule, 0);
	setcontext(&(queue.head->thread_control_block->context)); 
#else 
	// Choose MLFQ
	sched_mlfq();
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
	removeExittedBlockedThreadsMLFQ();
	int level = levelOfMLFQ();
	if (level == NULL){
		return;
	}
	deque * q = MLFQList[level];
	if (q->head->thread_control_block->status == running){
		node * current_node = dequeueMLFQ(); 
		current_node->thread_control_block->status = ready;
		/*
		int priority = current_node->thread_control_block->priority;
		if (priority < TOTAL_QUEUES - 1){
			priority ++;
		}
		current_node->thread_control_block->priority = priority;
		enqueueMLFQ(current_node, priority);*/
		// highest active level of queue may change?
		int newLevel = levelOfMLFQ();
		deque * q2 = MLFQList[newLevel];
		node * temp;
		setMLFQRunningThread();
		q2.head->thread_control_block->status = running; 
		makecontext(&scheduler_context, &schedule, 0);
		setcontext(&(q2.head->thread_control_block->context)); 	
	}
	if(q.head->thread_control_block->status == ready){
		q.head->thread_control_block->status = running; 
		makecontext(&scheduler_context, &schedule, 0);
		setcontext(&(q.head->thread_control_block->context)); 
	}
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

void enqueue_mutex(node* new_node, deque* q){
	if (q->head == NULL){
		q->head = new_node; 
		q->tail = new_node; 
		return;
	}
	node * temp_node = q->head; 
	while(temp_node->next != NULL){
		temp_node = temp_node->next; 
	}
	temp_node->next = new_node; 
	q->tail = new_node; 
	return; 	
}

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

node * dequeue_mutex(deque* q){
	if(q->head == NULL){
		return NULL; 
	}
	if(q->head == q->tail){
		node * temp_node = q->tail; 
		q->tail = NULL; 
		q->head = NULL; 
		return temp_node; 
	}
	node * temp_node = q->head; 
	q->head = q->head->next; 
	temp_node->next = NULL; 
	return temp_node; 	
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

int levelOfMLFQ(){
	int i;
	for (i = 0; i < TOTAL_QUEUES; i++) {
		deque *q = MLFQList[i];
		if(q.head == NULL){
			continue; 
		}
		return i;
	}
	return NULL;
}

node * dequeueMLFQ(){
	int i;
	node *thread;
	for (i = 0; i < TOTAL_QUEUES; i++) {
		deque *q = MLFQList[i];
		if(q.head == NULL){
			continue; 
		}
		if(q.tail == q.head){
			node * temp_node = q.tail; 
			q.tail = NULL; 
			q.head = NULL;
			return temp_node; 
		}
		node * temp_node = q.head; 
		q.head = q.head->next; 
		temp_node->next = NULL; 
		return temp_node;
	}
	return NULL;
}

void enqueueMLFQ(node* new_node, int priority){
	new_node->thread_control_block->priority = priority;
	deque *q = MLFQList[priority];
	if (q.head == NULL){
		queue.head = new_node; 
		queue.tail = new_node; 
		return;
	}
	node * temp_node = q.head; 
	while(temp_node->next != NULL){
		temp_node = temp_node->next; 
	}
	temp_node->next = new_node; 
	q.tail = new_node; 
	return;
}

void removeExittedBlockedThreadsMLFQ(){

}

void handle(int signum){
	worker_yield();
	//swapcontext(&(queue.head->thread_control_block->context), &scheduler_context);
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