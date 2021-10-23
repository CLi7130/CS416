// File:	mypthread_t.h

// List all group member's name: Craig Li  
// username of iLab: craigli
// iLab Server: rm.cs.rutgers.edu

#ifndef MYTHREAD_T_H
#define MYTHREAD_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_MYTHREAD macro */
//#define USE_MYTHREAD 1

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
#include <string.h>
#include <limits.h> //for intmax, probably should not use this
#include <stdatomic.h> //for atomic test and set

//thread states, add more if necessary
typedef enum status{
    READY, WAITING, FINISHED, REMOVE
    //ready = 0
    //waiting = 1
    //finished = 2
    //remove = 3

    //do we need blocked? piazza says no...
}status;

#define QUANTUM 10000 
//10 ms for quantum window? -> 10 * 1000 to convert to microseconds
#define STACKSIZE 32000 
//32KB for user thread stack = 32000 bytes
// <100 threads used for grading according to benchmark readme?


#ifdef PJSF
    #define SCHEDULE FIFO
#else
    #define SCHEDULE STCF
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
    mypthread_t threadID; //pointer to thread
    mypthread_t waitingThread;
    status threadStatus;//ready,waiting,finished etc
    ucontext_t threadContext; //context for thread

    int elapsedQuantums; //number of quantums thread has run
    void** value_ptr; //original arg
    void* returnValue; //return values for thread completion/REMOVE
    
    //add priority? could base priority off of elapsedQuantums
    //int priority; //0 for top priority, 1 is next highest, etc

    //don't need priority if we're doing a priority queue, base it 
    //off elapsedQuantums as per writeup

} tcb;

/* mutex struct definition */
typedef struct mypthread_mutex_t {
	/* add something here */

	// YOUR CODE HERE

    tcbNode* waitList; 
    int isLocked; //0 for false/not locked, 1 for true/locked
    //use with __test_and_set()?

} mypthread_mutex_t;

typedef struct tcbNode{

    struct tcbNode* next;
    struct tcbNode* prev;
    tcb* qTCB; //corresponding threadControlBlock

} tcbNode;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE


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

static void schedule();
void freeThreadNode(threadNode* deleteNode);
void printThreadQueue(struct tcbNode* tempQueue);
struct threadNode* getThreadNode(int threadID);
void removeThreadNode(threadNode* findThreadNode);
int getQueueSize(struct tcbNode* inputQueue);
void finishThread(int threadID);
void initTimer();
void initMain();
void exitCleanup(void);

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
