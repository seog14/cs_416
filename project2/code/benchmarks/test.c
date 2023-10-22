#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../thread-worker.h"

/* A scratch program template on which to call and
 * test thread-worker library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */

int * counter_1; 
int * counter_2; 
int sum1 = 0;
int sum2 = 0; 
pthread_t thread1;
pthread_t thread2; 
pthread_t thread3; 

void increment(void * arg){
	int counter = * ((int *) arg);
	for(int i = 0; i < counter; i++)
		sum1 +=1; 
	
	pthread_exit(NULL);
	return;
}
void decrement(void * arg){
	int counter = *((int *) arg); 
	for(int i = 0; i < counter; i++)
		sum1 -=1; 
	
	pthread_exit(NULL);
	return;
}
int main(int argc, char **argv) {
	int x = 100000000; 
	int y = 100000000; 
	counter_1 = &x; 
	counter_2 = &y; 
	pthread_create(&thread1, NULL, &increment, counter_1); 
	pthread_create(&thread2, NULL, &increment, counter_2); 
	pthread_create(&thread3, NULL, &increment, counter_2); 
	pthread_join(thread1, NULL); 
	pthread_join(thread2, NULL);
	printf("sum1: %d", sum1);
	printf("sum2: %d", sum2);
	return 0;
}
