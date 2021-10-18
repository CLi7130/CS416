// File:	mypthread_t.h

// List all group member's name: Craig Li  
// username of iLab: craigli
// iLab Server: rm.cs.rutgers.edu

#ifndef MYTHREAD_T_H
#define MYTHREAD_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_MYTHREAD macro */
#define USE_MYTHREAD 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <ucontext.h> //makecontext()
#include <signal.h> //signal stack/sigaction()

//thread states, add more if necessary
typedef enum status{
    READY, WAITING, RUNNING, FINISHED, MAIN
    //ready = 0
    //waiting = 1
    //running = 2
    //finished = 3
    //main -> only set to main thread, no other threads have this
    //status
    //do we need blocked according to piazza? TA says no..
}status;

#define QUANTUM 10 //10 ms for quantum window?
#define STACKSIZE 32000 //32KB for user thread stack = 32000 bytes
// <100 threads used for grading according to benchmark readme?


#ifdef STCF
    #define SCHEDULE STCF
#else
    #define SCHEDULE FIFO 
#endif

//identifier "uint" is undefined - changed to unsigned int?
typedef unsigned int mypthread_t;

//threadControlBlock inserted into threadNode for use in linked list/queue
typedef struct threadControlBlock {
	/* add important states in a thread control block */
	// thread Id
	// thread status
	// thread context
	// thread stack
	// thread priority
	// And more ...

	// YOUR CODE HERE
    pthread_t* threadID; //pointer to thread
    status threadStatus;//ready,scheduled,blocked
    ucontext_t threadContext; //context for thread
    int elapsedQuantums; //number of quantums thread has run
    //add priority? could base priority off of elapsedQuantums
    //int priority; //0 for top priority, 1 is next highest, etc

} tcb;

/* mutex struct definition */
typedef struct mypthread_mutex_t {
	/* add something here */

	// YOUR CODE HERE
    int mutexID; //mutex identifier
    int isLocked; //0 for false/not locked, 1 for true/locked
    //use with __test_and_set()?

} mypthread_mutex_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE

//TCB goes into a threadNode, which is put into threadQueue
typedef struct threadNode{
    tcb* threadControlBlock;

    struct threadNode* next;
    struct threadNode* prev;

} threadNode;
 
 //container for all threadNodes
typedef struct threadQueue{
    struct threadNode* head;
    struct threadNode* tail;

} threadQueue;


/* Function Declarations: */

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int mypthread_yield();

/* terminate a thread */
void mypthread_exit(void *value_ptr);

/* wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr);

/* initial the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex);

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex);

/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex);

/* Handles SIGALRM*/
static void SIGALRM_Handler();

#ifdef USE_MYTHREAD
#define pthread_t mypthread_t
#define pthread_mutex_t mypthread_mutex_t
#define pthread_create mypthread_create
#define pthread_exit mypthread_exit
#define pthread_join mypthread_join
#define pthread_mutex_init mypthread_mutex_init
#define pthread_mutex_lock mypthread_mutex_lock
#define pthread_mutex_unlock mypthread_mutex_unlock
#define pthread_mutex_destroy mypthread_mutex_destroy
#endif

#endif
