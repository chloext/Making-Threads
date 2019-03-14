 /*
 ============================================================================
 Name        : OSA1.1.c
 Author      : Xiaoyan Tao
 UPI         : xtao151
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "littleThread.h"
#include "threads2.c" // rename this for different threads

Thread newThread; // the thread currently being set up
Thread mainThread; // the main thread
Thread currThread;
Thread firstThread;
struct sigaction setUpAction;
int num_finished=0;
void scheduler();
void printThreadStates();
const char *enum_names[] = {"setup", "running", "ready", "finished"}; 

/*
 * Switches execution from prevThread to nextThread.
 */
void switcher(Thread prevThread, Thread nextThread) {
	Thread temp;
	if (prevThread->state == FINISHED) { // it has finished
		printf("disposing %d\n\n", prevThread->tid);
		free(prevThread->stackAddr); // Wow!
		nextThread->state=RUNNING;
		printThreadStates();
		longjmp(nextThread->environment, 1);
	} else if (setjmp(prevThread->environment) == 0) { // so we can come back here
		prevThread->state = READY;
		nextThread->state = RUNNING;
		printThreadStates();
		longjmp(nextThread->environment, 1);
	}
}

/*
 * Associates the signal stack with the newThread.
 * Also sets up the newThread to start running after it is long jumped to.
 * This is called when SIGUSR1 is received.
 */
void associateStack(int signum) {
	Thread localThread = newThread; // what if we don't use this local variable?
	localThread->state = READY; // now it has its stack
	if (setjmp(localThread->environment) != 0) { // will be zero if called directly
		(localThread->start)();
		printf("\n");
		localThread->state = FINISHED;
		num_finished += 1;
		scheduler(); // at the moment back to the main thread
	}
}

/*
 * Sets up the user signal handler so that when SIGUSR1 is received
 * it will use a separate stack. This stack is then associated with
 * the newThread when the signal handler associateStack is executed.
 */
void setUpStackTransfer() {
	setUpAction.sa_handler = (void *) associateStack;
	setUpAction.sa_flags = SA_ONSTACK;
	sigaction(SIGUSR1, &setUpAction, NULL);
}


void printThreadStates(void){
	printf("\nThread State\n=============\n");
	Thread temp = firstThread;
	for (int i=0;i<NUMTHREADS;i++){
		printf("threadID: %d state:%s\n",temp->tid,enum_names[temp->state]);
		temp = temp->next;
	}
	printf("\n");
}

/*
 *  Sets up the new thread.
 *  The startFunc is the function called when the thread starts running.
 *  It also allocates space for the thread's stack.
 *  This stack will be the stack used by the SIGUSR1 signal handler.
 */
Thread createThread(void (startFunc)()) {
	static int nextTID = 0;
	Thread thread;
	stack_t threadStack;

	if ((thread = malloc(sizeof(struct thread))) == NULL) {
		perror("allocating thread");
		exit(EXIT_FAILURE);
	}
	thread->tid = nextTID++;
	thread->state = SETUP;
	thread->start = startFunc;
	if ((threadStack.ss_sp = malloc(SIGSTKSZ)) == NULL) { // space for the stack
		perror("allocating stack");
		exit(EXIT_FAILURE);
	}
	thread->stackAddr = threadStack.ss_sp;
	threadStack.ss_size = SIGSTKSZ; // the size of the stack
	threadStack.ss_flags = 0;
	if (sigaltstack(&threadStack, NULL) < 0) { // signal handled on threadStack
		perror("sigaltstack");
		exit(EXIT_FAILURE);
	}
	newThread = thread; // So that the signal handler can find this thread
	kill(getpid(), SIGUSR1); // Send the signal. After this everything is set.
	return thread;
}

void threadYield(void){
	currThread->state = READY;
	scheduler();
}

Thread new_thread(void){
	if (currThread->state==FINISHED){
                Thread temp;
                currThread->prev->next = currThread->next;
		currThread->next->prev = currThread->prev;
                temp =  currThread;
		currThread = currThread->next;
        	return temp;
	}else{
                currThread=currThread->next;
		return currThread->prev;
        }

}

void scheduler(void){
	Thread prev;
	if (num_finished>=NUMTHREADS){
		prev = new_thread();
		switcher(prev,mainThread);
	}
	else{
		prev = new_thread();
		switcher(prev,currThread);
	}
}

void createList(Thread* threads){
	for (int t = 0; t < NUMTHREADS; t++) {
		threads[t]->next = threads[(NUMTHREADS+t+1)%NUMTHREADS];
		threads[t]->prev = threads[(NUMTHREADS+t-1)%NUMTHREADS];
	}
	firstThread = threads[0];
	currThread = threads[0];
}

int main(void) {
	struct thread controller;
	Thread threads[NUMTHREADS];
	mainThread = &controller;
	mainThread->state = RUNNING;
	setUpStackTransfer();
	// create the threads
	for (int t = 0; t < NUMTHREADS; t++) {
		threads[t] = createThread(threadFuncs[t]);
	}
	createList(threads);
	printThreadStates();
	puts("switching to first thread\n");
	switcher(mainThread, currThread);
	puts("back to the main thread\n");
	return EXIT_SUCCESS;
}
