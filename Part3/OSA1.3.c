/*
 ============================================================================
 Name        : OSA1.3.c
 UPI         : bche722
 Author      : Baiwei Chen
 Version     : 1.3
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h> 
#include "littleThread.h"
#include "threads3.c" // rename this for different threads

Thread newThread; // the thread currently being set up
Thread mainThread; // the main thread
Thread firstThread;
Thread runningThread;
struct sigaction setUpAction;



void printState(Thread thread){
	printf("threadID: %d state:", thread->tid);
	switch(thread->state){
		case 0: printf("setup\n");break;
		case 1: printf("running\n");break;
		case 2: printf("ready\n");break;
		case 3: printf("finished\n");break;
		default: break;
	}
	if(thread->next != firstThread){	
		printState(thread->next);
	}
}

void printThreadStates(){
	printf("\nThread States\n");
	printf("=============\n");		
	printState(firstThread);
	printf("\n");
}

Thread scheduler(Thread thread){	
	Thread localThread=thread;
	while(localThread->next!=thread){		
		if(localThread->next->state!=READY){
			localThread=localThread->next;
		}else{
			localThread->next->state = RUNNING;	
			return localThread->next;
		}
	}
	if(thread->state==FINISHED){	
		return mainThread;
	}else{
		return thread;
	}
}

/*
 * Switches execution from prevThread to nextThread.
 */
void switcher(Thread prevThread, Thread nextThread) {
	if (prevThread->state == FINISHED) { // it has finished
		printf("disposing %d\n", prevThread->tid);
		if(nextThread != mainThread){
			printThreadStates();
		}
		free(prevThread->stackAddr); // Wow!
		runningThread=nextThread;
		longjmp(nextThread->environment, 1);
	} else if (setjmp(prevThread->environment) == 0) { // so we can come back here
		prevThread->state = READY;
		nextThread->state = RUNNING;
		printThreadStates();
		runningThread=nextThread;
		longjmp(nextThread->environment, 1);
	}
}

void threadYield(){
	Thread nextThread=scheduler(runningThread);
	switcher(runningThread, nextThread);
}

void setUpTimer(){
	struct itimerval value;
        signal(SIGVTALRM, threadYield);
        value.it_value.tv_sec = 0;
        value.it_value.tv_usec = 20000;
        value.it_interval.tv_sec = 0;
        value.it_interval.tv_usec = 20000;
        setitimer(ITIMER_VIRTUAL, &value,NULL);
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
		localThread->state = FINISHED;
		Thread nextThread = scheduler(localThread);
		switcher(localThread, nextThread); // at the moment back to the main thread
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
	thread->prev = newThread;
	thread->next = 0;
	newThread->next= thread;
	newThread = thread; // So that the signal handler can find this thread
	kill(getpid(), SIGUSR1); // Send the signal. After this everything is set.
	return thread;
}

int main(void) {
	struct thread controller;
	Thread threads[NUMTHREADS];
	mainThread = &controller;
	mainThread->state = RUNNING;
	newThread = mainThread;
	setUpStackTransfer();
	// create the threads
	for (int t = 0; t < NUMTHREADS; t++) {
		threads[t] = createThread(threadFuncs[t]);
	}
	threads[NUMTHREADS-1]->next=threads[0];
	threads[0]->prev=threads[NUMTHREADS-1];
	firstThread=threads[0];
	setUpTimer();
	printThreadStates();
	puts("switching to first thread");
	runningThread =threads[0];
	switcher(mainThread, threads[0]);
	puts("\nback to the main thread");
	printThreadStates();
	return EXIT_SUCCESS;
}
