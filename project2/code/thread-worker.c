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
long time_ran = 0; 
ucontext_t scheduler_context; 
deque queue={NULL, NULL};
deque *MLFQList[TOTAL_QUEUES];
deque exitQueue = {NULL, NULL};
node *currentlyRunningNode = NULL;
deque buffer = {NULL, NULL}; 
struct itimerval timer; 
//node *currentlyRunningMLFQ;
int timers;

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

	if (id_incrementer == 1){
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
		main_control_block->elapsed = 0;
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
		enqueueMLFQ(main_node);

		// Create tcb for thread 
		tcb * thread_control_block = malloc(sizeof(tcb)); 
		*thread = id_incrementer; 
		thread_control_block->id = id_incrementer; 
		id_incrementer++; 
		void *stack=malloc(STACK_SIZE); 
		thread_control_block->stack=stack; 
		thread_control_block->priority=0; 
		thread_control_block->elapsed=0; 
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
		enqueueMLFQ(curr_thread); 

		// Initialize Timer 
		// Use sigaction to register signal handler
		struct sigaction sa;
		memset (&sa, 0, sizeof (sa));
		sa.sa_handler = &handle;
		sigaction (SIGPROF, &sa, NULL);

		// Set up what the timer should reset to after the timer goes off
		timer.it_interval.tv_usec = QUANTUM; 
		timer.it_interval.tv_sec = 0;
		timer.it_value.tv_usec = QUANTUM;
		timer.it_value.tv_sec = 0;
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
	thread_control_block->elapsed=0; 
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
	enqueueMLFQ(curr_thread); 

    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
void  worker_yield() {
	swapcontext(&(currentlyRunningNode->thread_control_block->context), &scheduler_context);
	// timer first goes here then to scheduler
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread
	//free context stack
	free(currentlyRunningNode->thread_control_block->stack); 
	if (currentlyRunningNode->thread_control_block->status == blocked){
		printf("oh no\n");
	}
	currentlyRunningNode->thread_control_block->status=exitted;
	
	swapcontext(&(currentlyRunningNode->thread_control_block->context), &scheduler_context);
	return;
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	node * joining_thread_node = NULL;
	int i;
	int found = 0;
	//First search if thread has already been executed
	if(found_in_exit_queue(thread)){
		pop(thread); 
		return 0; 
	}
	//Wait until thread exists in mlfq
	while(1){
		for (i = 0; i < TOTAL_QUEUES; i++){
			deque *q = MLFQList[i];
			node *cur = q->head;
			if (cur != NULL){
				while (cur != NULL) {
					if (cur->thread_control_block->id == thread){
						joining_thread_node = cur;
						found = 1;
						break;
					}
					cur = cur->next;
				}
			}
			if (found){
				break;
			}
		}
		if(found){
			break;
		}
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
	deque *q = malloc(sizeof(deque));
	deque qe = {NULL, NULL}; 
	*q = qe;
	mutex->blocked_threads = q; 
	mutex->available = 0; 
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {
	if (__sync_lock_test_and_set(&(mutex->available), currentlyRunningNode->thread_control_block->id) == 0){
		return 0; 
	}
	else{
		node * current_node = currentlyRunningNode; 
		current_node->thread_control_block->status = blocked; 
		enqueue_mutex(current_node, mutex->blocked_threads); 
		swapcontext(&(currentlyRunningNode->thread_control_block->context), &scheduler_context);
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
		enqueueMLFQ(temp);
	}
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	free(mutex->blocked_threads); 
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
	sched_mlfq(); 
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
	printf("Current_running_context at sched: %d At priority: %d with status: %d\n", currentlyRunningNode->thread_control_block->id, currentlyRunningNode->thread_control_block->priority, currentlyRunningNode->thread_control_block->status);
	timer.it_interval.tv_sec = 0; 
	timer.it_interval.tv_usec = 0; 
	timer.it_value.tv_usec = 0;
	timer.it_value.tv_sec = 0; 
	setitimer(ITIMER_PROF, &timer, NULL); 
	if (currentlyRunningNode->thread_control_block->status == exitted){
		node *tempNode = dequeueMLFQ();
		enqueueExit(tempNode);
	}
	if (currentlyRunningNode -> thread_control_block->status == blocked){
		if (time_ran >= 2000){
			time_ran = 0; 
			node* tempNode = dequeueMLFQ(); 
			tempNode->thread_control_block->priority = 0; 
			tempNode = dequeueMLFQ(); 
			while(tempNode != NULL){
				node * node_to_enqueue = tempNode; 
				node_to_enqueue->thread_control_block->priority = 0; 
				enqueue_buffer(node_to_enqueue); 
				tempNode = dequeueMLFQ(); 
			}
			tempNode = dequeue_buffer(); 
			while(tempNode != NULL){
				enqueueMLFQ(tempNode); 
				tempNode = dequeue_buffer(); 
			}
		}
		else
			dequeueMLFQ();
	}
	else if(time_ran >= 2000){
		time_ran = 0; 
		node * tempNode = dequeueMLFQ(); 
		while(tempNode != NULL){
			node * node_to_enqueue = tempNode; 
			node_to_enqueue->thread_control_block->priority = 0; 
			enqueue_buffer(node_to_enqueue); 
			tempNode = dequeueMLFQ(); 
		}
		tempNode = dequeue_buffer(); 
		while(tempNode != NULL){
			enqueueMLFQ(tempNode); 
			tempNode = dequeue_buffer(); 
		}

	}
	if (currentlyRunningNode->thread_control_block->status == running){
		node *tempNode = dequeueMLFQ();
		tempNode->thread_control_block->status = ready;
		tempNode->thread_control_block->elapsed += QUANTUM;
		changePriority(tempNode);
		enqueueMLFQ(tempNode);
	}
	time_ran += QUANTUM; 
	currentlyRunningNode->thread_control_block->status = running; 
	timer.it_interval.tv_sec = 0; 
	timer.it_interval.tv_usec = QUANTUM; 
	timer.it_value.tv_usec = QUANTUM;
	timer.it_value.tv_sec = 0; 
	setitimer(ITIMER_PROF, &timer, NULL); 
	setcontext(&(currentlyRunningNode->thread_control_block->context)); 
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

node * dequeueExit(){
	if(exitQueue.head == NULL){
		return NULL; 
	}
	if(exitQueue.tail == exitQueue.head){
		node * temp_node = exitQueue.tail; 
		exitQueue.tail = NULL; 
		exitQueue.head = NULL; 
		return temp_node; 
	}
	node * temp_node = exitQueue.head; 
	exitQueue.head = exitQueue.head->next; 
	temp_node->next = NULL; 
	return temp_node; 
}

void enqueueExit(node* new_node){
	if (exitQueue.head == NULL){
		exitQueue.head = new_node; 
		exitQueue.tail = new_node; 
		return;
	}
	node * temp_node = exitQueue.head; 
	while(temp_node->next != NULL){
		temp_node = temp_node->next; 
	}
	temp_node->next = new_node; 
	exitQueue.tail = new_node; 
	return; 
}


node * dequeueMLFQ(){
	timer.it_interval.tv_sec = 0; 
	timer.it_interval.tv_usec = 0; 
	timer.it_value.tv_usec = 0;
	timer.it_value.tv_sec = 0; 
	setitimer(ITIMER_PROF, &timer, NULL); 
	int level = currentlyRunningNode->thread_control_block->priority;
	deque *q = MLFQList[level];
	if (q->head == NULL){
		timer.it_interval.tv_sec = 0; 
		timer.it_interval.tv_usec = QUANTUM; 
		timer.it_value.tv_usec = QUANTUM;
		timer.it_value.tv_sec = 0; 
		setitimer(ITIMER_PROF, &timer, NULL); 
		currentlyRunningNode = NULL; 
		return NULL; 
	}
	node* tempNode = currentlyRunningNode;
	if (q->tail == q->head){
		q->tail = NULL;
		q->head = NULL;
	}
	else {
		q->head = q->head->next;
	}
	tempNode->next = NULL;
	int i; 
	for (i = 0; i < TOTAL_QUEUES; i++) {
		deque *q = MLFQList[i];
		if(q->head == NULL){
			continue; 
		} else {
			currentlyRunningNode = q->head;
			break; 
		}
	}
	return tempNode;
}

void enqueueMLFQ(node* new_node){
	timer.it_interval.tv_sec = 0; 
	timer.it_interval.tv_usec = 0; 
	timer.it_value.tv_usec = 0;
	timer.it_value.tv_sec = 0; 
	setitimer(ITIMER_PROF, &timer, NULL); 
	if (currentlyRunningNode == NULL){
		currentlyRunningNode = new_node; 
		currentlyRunningNode->thread_control_block->status=running; 
	}
	deque *q = MLFQList[new_node->thread_control_block->priority];
	if (q->head == NULL){
		q->head = new_node; 
		q->tail = new_node; 
		timer.it_interval.tv_sec = 0; 
		timer.it_interval.tv_usec = QUANTUM; 
		timer.it_value.tv_usec = QUANTUM;
		timer.it_value.tv_sec = 0; 
		setitimer(ITIMER_PROF, &timer, NULL); 
		return;
	}
	node * temp_node = q->head; 
	while(temp_node->next != NULL){
		temp_node = temp_node->next; 
	}
	temp_node->next = new_node; 
	q->tail = new_node; 
	timer.it_interval.tv_sec = 0; 
	timer.it_interval.tv_usec = QUANTUM; 
	timer.it_value.tv_usec = QUANTUM;
	timer.it_value.tv_sec = 0; 
	setitimer(ITIMER_PROF, &timer, NULL); 
	return;
}

void handle(int signum){
	worker_yield();
}

void pop(worker_t thread){
	node * selected_thread = NULL;
	node *temp = exitQueue.head;
	if (temp == NULL){
		perror("No exitted threads");
		exit(1);
	}
	while (temp != NULL) {
		if (temp->thread_control_block->id == thread){
			selected_thread = temp;
			break;
		}
		temp = temp->next;
	}
	if (selected_thread == NULL){
		perror("Invalid joining thread");
		exit(1); 
	}
	if (selected_thread == exitQueue.head){
		selected_thread = dequeueExit(); 
		free(selected_thread->thread_control_block); 
		free(selected_thread); 
		return; 
	}
	node * after = selected_thread->next; 
	node * before = exitQueue.head; 
	while(before->next!=selected_thread){
		before = before->next; 
	}
	before->next = after; 
	free(selected_thread->thread_control_block); 
	free(selected_thread); 
	return; 
}

void changePriority(node * newNode){
	int elapsed = newNode->thread_control_block->elapsed;
	int priority = newNode->thread_control_block->priority;
	if (priority == TOTAL_QUEUES - 1){
		return;
	}
	if (elapsed >= 2*(priority +1)*QUANTUM){
		newNode->thread_control_block->priority += 1;
		return;
	}
	return;
}

void enqueue_buffer(node* new_node){
	if (buffer.head == NULL){
		buffer.head = new_node; 
		buffer.tail = new_node; 
		return;
	}
	node * temp_node = buffer.head; 
	while(temp_node->next != NULL){
		temp_node = temp_node->next; 
	}
	temp_node->next = new_node; 
	buffer.tail = new_node; 
	return; 
}
node* dequeue_buffer(){
	if(buffer.head == NULL){
		return NULL; 
	}
	if(buffer.tail == buffer.head){
		node * temp_node = buffer.tail; 
		buffer.tail = NULL; 
		buffer.head = NULL; 
		return temp_node; 
	}
	node * temp_node = buffer.head; 
	buffer.head = buffer.head->next; 
	temp_node->next = NULL; 
	return temp_node; 
} 

int found_in_exit_queue(worker_t thread){
	node* temp_node = exitQueue.head; 
	while(temp_node != NULL){
		if(temp_node->thread_control_block->id == thread){
			return 1; 
		}
		temp_node = temp_node->next; 
	}
	return 0; 
}