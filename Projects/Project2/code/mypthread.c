// File:	mypthread.c

// List all group member's name: Craig Li, Prerak Patel
// username of iLab: craigli, pjp179
// iLab Server: rm.cs.rutgers.edu

#include "mypthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE

tQueue* threadQ = NULL;
static mypthread_t currThread;
static tcb* currTCB;
static struct sigaction timer;
static struct itimerval interval;

static int newThreadID = 0;

#ifndef DEBUG
    #define DEBUG 0
#endif
void freeTCBQueue(void);
static void schedule();
static void sched_stcf();

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr,
                      void *(*function)(void*), void * arg) {
        //void *(*function)(void*) - 3rd arg
        //change args to include threadwrapper? see above

       // create Thread Control Block
       // create and initialize the context of this thread
       // allocate space of stack for this thread to run
       // after everything is all set, push this thread int
       // YOUR CODE HERE

    if(DEBUG){
        printf("IN PTHREAD CREATE\n");
    }

    if(newThreadID == 0){
        //initialization for first time through
        //set timer and main TCB
        initMain();
        newThreadID++;
    }
    

    currThread = newThreadID;
    //create TCB
    tcb* newTCB = malloc(sizeof(tcb));

    newTCB->threadID = newThreadID;
    *thread = newThreadID;

    newTCB->threadStatus = READY;
    newTCB->elapsedQuantums = 0;
    newTCB->waitingThread = -1;
    newTCB->valPtr = NULL;
    newTCB->returnValue = NULL;

    newThreadID++;

    enqueue(&threadQ, newTCB, 0);

    if(getcontext(&(newTCB->threadContext)) == -1){
        perror("Initializing New Thread Context Failed.\n");
        exit(EXIT_FAILURE);
    }
    newTCB->threadContext.uc_link = NULL;
    newTCB->threadContext.uc_stack.ss_sp = malloc(STACKSIZE);
    if(newTCB->threadContext.uc_stack.ss_sp <= 0){
        printf("Memory not allocated for new thread: %d\n",
                 newTCB->threadID);
        exit(EXIT_FAILURE);
    }
    newTCB->threadContext.uc_stack.ss_size = STACKSIZE;
    makecontext(&(newTCB->threadContext), (void*) function, 1, arg);

    if(DEBUG){
        printf("\n-----------THREAD CREATED----------------\n");
    }

};

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {

	// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// switch from thread context to scheduler context

	// YOUR CODE HERE

    //all of this is done in schedule()
    schedule();
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread

	// YOUR CODE HERE

    currTCB->threadStatus = FINISHED;

    if(currTCB->valPtr == NULL){
        //value_ptr is not null, have to return value_ptr
        currTCB->returnValue = value_ptr;
    }
    else{

        *currTCB->valPtr = value_ptr;
        currTCB->threadStatus = REMOVE;
        if(removeNode(currTCB->threadID, &threadQ) == -1){
            perror("Error removing node, aborting\n");
            abort();
        }
        //free(currTCB);
    }

    notifyThreads(&threadQ, currTCB->threadID);
    schedule();
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {

	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread

	// YOUR CODE HERE
    
    if(isFinished(thread, &threadQ) == 1){
        //thread done
        tcb* tempTCB = getTCB(thread, &threadQ);
        if(value_ptr != NULL){

            tempTCB->threadStatus = REMOVE;
            *value_ptr = tempTCB->returnValue;
            
            tcb* ptr = tempTCB;
            if(removeNode(ptr->threadID, &threadQ) == -1){
                perror("Error removing node, aborting\n");
                abort();
            }

        }
        return 0;

    }
    currTCB->threadStatus = WAITING;
    currTCB->waitingThread = thread;

    tcb* tempTCB = getTCB(thread, &threadQ);
    tempTCB->valPtr = value_ptr;
    schedule();

};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex,
                          const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex

	// YOUR CODE HERE

    
    if(newThreadID == 0){
        initMain();
    }
    mutex->isLocked = 0;
    mutex->waitList = NULL;

	return 0;
};

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex) {
        // use the built-in test-and-set atomic function to test the mutex
        // if the mutex is acquired successfully, enter the critical section
        // if acquiring mutex fails, push current thread into block list and //
        // context switch to the scheduler thread

        // YOUR CODE HERE
        
        while(atomic_flag_test_and_set(&(mutex->isLocked))){
            currTCB->threadStatus = WAITING;
            tcbNode* lockThisNode = malloc(sizeof(tcbNode));

            lockThisNode->next = mutex->waitList;
            lockThisNode->nTCB = currTCB;
            mutex->waitList = lockThisNode;

            schedule();
        }
        return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex) {
	// Release mutex and make it available again.
	// Put threads in block list to run queue
	// so that they could compete for mutex later.

	// YOUR CODE HERE
    
    tcbNode* ptr = mutex->waitList;
    while(ptr != NULL){
        ptr->nTCB->threadStatus = READY;
        tcbNode* deleteNode = ptr;
        ptr = ptr->next;
        free(deleteNode);
    }
    
    mutex->waitList = NULL;
    mutex->isLocked = 0; //unlocked
	return 0;
};


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in mypthread_mutex_init

    //dynamic memory is allocated/freed in lock/unlock
	return 0;
};

/* scheduler */
static void schedule() {
	// Every time when timer interrup happens, your thread library
	// should be contexted switched from thread context to this
	// schedule function

	// Invoke different actual scheduling algorithms
	// according to policy (STCF or MLFQ)

	// if (sched == STCF)
	//		sched_stcf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE
    

    //not req to implement MLFQ for 416
    //we only need to route to stcf here?
    if(DEBUG){
        printf("\n-----------IN SCHEDULE---------\n");
    }

    #ifndef MLFQ

        // Choose STCF
        sched_stcf();

    #elif
        
        // Choose MLFQ
        //not req for 416
        
    #endif

}

/* Preemptive SJF (STCF) scheduling algorithm */
static void sched_stcf() {
	// Your own implementation of STCF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE

   
    //ignore alarm during scheduling
    signal(SIGALRM, SIG_IGN);
    
    tcb* prevTCB = currTCB;

    if((currTCB = dequeue(&threadQ)) == NULL){
        currTCB = prevTCB;
        return;
    }
    prevTCB->elapsedQuantums++;

    enqueue(&threadQ, prevTCB, prevTCB->elapsedQuantums);

    //restart timer/alarm
    initTimer(timer, interval);

    swapcontext(&(prevTCB->threadContext), &(currTCB->threadContext));
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
    
    //not required to implement for 416
}

// Feel free to add any other functions you need

// YOUR CODE HERE

/*
    Prints thread nodes in the thread queue, used for debugging
*/
void printThreadQueue(tQueue** queue){
    //have to modify this
    printf("-------------PRINT THREAD QUEUE-----------\n");
    printf("~~~~~~Current TCB~~~~~\n");
    if(currTCB == NULL){
        printf("No Current TCB\n");
        return;
    }
    else{
        printf("Thread ID: %d\n",  currTCB->threadID);
        printf("Thread Status: %d\n", (int) currTCB->threadStatus);
        printf("Thread Elapsed Quantums (Runtime): %d\n\n",
                 currTCB->elapsedQuantums);
    }
    printf("~~~~~~~THREAD QUEUE~~~~~~~~~~\n");
    tQueue* ptr = *queue;

    if(ptr == NULL){
        printf("Queue is empty or NULL\n");
        return;
    }

    int count = 1;
    for(ptr; ptr != NULL; ptr = ptr->next){
    
        printf("Node %d.\n", count);
        printf("Thread ID: %d\n",  ptr->TCB->threadID);
        printf("Thread Status: %d\n", (int) ptr->TCB->threadStatus);
        printf("Waiting Thread: %d\n", ptr->TCB->waitingThread);
        printf("Thread Elapsed Quantums (Runtime): %d\n\n",
                 ptr->TCB->elapsedQuantums);

        printf("Return Value: %p\n", ptr->TCB->returnValue);
        printf("Value Pointer: %p\n", ptr->TCB->valPtr);
        /*
            mypthread_t threadID; //pointer to thread
            mypthread_t waitingThread;
            status threadStatus;//ready,waiting,finished etc
            ucontext_t threadContext; //context for thread

            int elapsedQuantums; //number of quantums thread has run
            void** valPtr; //original arg
            void* returnValue; //return values for thread completion/REMOVE
        */
        count++;
    }
    printf("-------------LIST DONE------------\n");
}
/*
    Searches thread Queue to find a node whose ID matches the ID of the given
    parameter node, and returns a pointer to that node's control block. Returns NULL if not found.
*/
tcb* getTCB(mypthread_t threadID, tQueue** threadQ){
   
    tcb* tempTCB = NULL;
    for(tQueue* ptr = *threadQ; ptr != NULL; ptr = ptr->next){

        if(ptr->TCB->threadID == threadID){
            tempTCB = ptr->TCB;
            break;
        }

    }
    return tempTCB;
}
/*
    Enqueue a tcb into the queue based on its elapsedQuantums. Lower amount
    of elapsedQuantums is higher priority.
*/
void enqueue(tQueue** argQueue, tcb* argTCB, int argQuantums){
    
    if(argQueue == NULL){
        if(DEBUG){
            printf("EMPTY OR NULL QUEUE\n");
        }
        return;
    }
    if(DEBUG){
        printf("ENQUEUING NODE: pre add\n");
        printThreadQueue(argQueue);
    }

    //node to insert
    tQueue *newNode = malloc(sizeof(tQueue));

    newNode->elapsedQuantums = argQuantums;
    newNode->TCB = argTCB;
    newNode->next = NULL;
    newNode->prev = NULL;

    //reference to head
    tQueue *head = *argQueue;
    
    if(head == NULL){
        *argQueue = newNode;
        return;
    }
    else if(head->elapsedQuantums > argQuantums){
        newNode->next = head;
        head->prev = newNode;
        *argQueue = newNode;
        return;
    }
    tQueue* ptr = head->next;
    tQueue* ptr2 = head;
    
    while(ptr != NULL && ptr->elapsedQuantums < argQuantums){
        ptr2 = ptr;
        ptr = ptr->next;
    }
    newNode->next = ptr;
    if(ptr != NULL){
        ptr->prev = newNode;
    }
    ptr2->next = newNode;
    newNode->prev = ptr2;

    if(DEBUG){
        printf("ENQUEUING NODE: post add\n");
        printThreadQueue(argQueue);
    }
}
/*
    Dequeues the first READY Node from the queue, used in sched_stcf to set currTCB
*/
tcb* dequeue(tQueue** argQueue){
   
    //reference to head
    tQueue* temp = *argQueue;

    if(temp == NULL){
        if(DEBUG){
            printf("QUEUE IS EMPTY\n");
        }
        return NULL;
    }
    if(DEBUG){
        printf("DEQUEUEING NODE: pre DQ\n");
        printThreadQueue(argQueue);
    }
    //returned TCB
    tcb* priorityTCB = NULL;

    for(tQueue *ptr = *argQueue; ptr != NULL; ptr = ptr->next){
        if(ptr->TCB->threadStatus == READY){
            if(ptr == temp){
                //head of queue
                *argQueue = ptr->next;
                //ptr->next->prev = NULL;
                ptr->next = NULL;
                priorityTCB = ptr->TCB;
                free(ptr);
                break;
            }
            else if(ptr->next != NULL){
                //middle of queue
                priorityTCB = ptr->TCB;
                ptr->prev->next = ptr->next;
                ptr->next->prev = ptr->prev;
                ptr->next = NULL;
                ptr->prev = NULL;
                free(ptr);
                break;
            }
            else{
                //tail/end of queue
                priorityTCB = ptr->TCB;
                ptr->prev->next = NULL;
                ptr->prev = NULL;
                free(ptr);
                break;

            }
        }
    }

    if(DEBUG){
        printf("DEQUEUEING NODE: post DQ\n");
        printThreadQueue(argQueue);
    }
    return priorityTCB;
}

/*
    Utility function for counting number of nodes in queue
*/
int getQueueSize(tQueue** queue){
    
    int count = 0;
    tQueue* ptr = *queue;

    if(ptr == NULL){
        return count;
    }

    while(ptr != NULL){
        count++;
        ptr = ptr->next;
    }

    return count;
}
/*
    Create timer
*/
void initTimer(struct sigaction timer, struct itimerval interval){

    memset(&timer, 0, sizeof(timer));

    timer.sa_handler = &schedule;
    //timer calls schedule when it goes off, no need for sighandler or scheduler context

    if(sigaction(SIGALRM, &timer, NULL) < 0){//use SIGPROF/ITIMER_PROF?
        //signal handler/sigaction failed?
        perror("SIGACTION failed\n");
        exit(EXIT_FAILURE);
    }

    //initial timer expiration at QUANTUM ms
    interval.it_value.tv_sec = 0;
    interval.it_value.tv_usec = QUANTUM;

    //set interval to QUANTUM ms 
    interval.it_interval.tv_sec = 0;
    interval.it_interval.tv_usec = QUANTUM;
    //start timer
    setitimer(ITIMER_REAL, &interval, NULL);
}

/*
    Initialize main TCB
*/
void initMain(){
    
    
    //signal(SIGALRM, SIG_IGN);
    currThread = newThreadID;
    tcb* mainTCB = malloc(sizeof(tcb));

    mainTCB->threadID = currThread;
    mainTCB->waitingThread = -1;

    mainTCB->threadStatus = READY;
    mainTCB->elapsedQuantums = 0;

    mainTCB->valPtr = NULL;
    mainTCB->returnValue = NULL;

    currTCB = mainTCB;
    initTimer(timer, interval);
    newThreadID++;
    atexit(freeTCBQueue);
    //currTCB->threadContext.uc_link = &freeTCBQueue;
    getcontext(&(currTCB->threadContext));
    
    
    //tcb* mainReturn = malloc(sizeof(tcb));
    //getcontext(&(mainReturn->threadContext));
    //makecontext(&(mainReturn->threadContext), freeTCBQueue, 0);

    //currTCB->threadContext.uc_link = &(mainReturn->threadContext);

}
/*
    Updates all threads that were waiting on a thread to exit, changes status from WAITING to READY.
*/
void notifyThreads(tQueue** queue, mypthread_t waiting){
    
    for(tQueue* ptr = *queue; ptr != NULL; ptr = ptr->next){

        if(ptr->TCB->threadStatus == WAITING 
            && ptr->TCB->waitingThread == waiting){

            ptr->TCB->waitingThread = -1;
            ptr->TCB->threadStatus = READY;
        }
    }
    return;
}
/*
    Iterates through queue and frees all malloc'd memory from nodes
*/
void freeTCBQueue(void){
    
    tQueue* ptr = threadQ;

    while(ptr != NULL){
        tQueue* temp = ptr;
        ptr = ptr->next;
        freeThreadNode(temp);
    }
    free(currTCB);
}

/*
    Helper function that frees all malloc'd memory of a threadNode. Used to cut down redundancy in code.
*/
void freeThreadNode(tQueue* deleteNode){

    free(deleteNode->TCB->threadContext.uc_stack.ss_sp);
    free(deleteNode->TCB);
    free(deleteNode);
}
/*
    Checks whether a given thread has finished
*/
int isFinished(mypthread_t thread, tQueue** argQueue){

    int isFinished = 0;
    for(tQueue* ptr = *argQueue; ptr != NULL; ptr = ptr->next){
        if(ptr->TCB->threadStatus == FINISHED 
            && ptr->TCB->threadID == thread)
        {

            isFinished = 1;
            break;
        }
    }

    return isFinished;
}

/*
    Deallocates memory from a node in the queue
*/
int removeNode(mypthread_t thread, tQueue** argQueue){

    int removed = -1;
    for(tQueue* ptr = *argQueue; ptr != NULL; ptr = ptr->next){
        if(ptr->TCB->threadStatus == REMOVE 
            && ptr->TCB->threadID == thread)
        {
            tQueue* node = ptr;
            
            if(node->next != NULL){
                node->next->prev = node->prev;
            }
            if(node->prev != NULL){
                node->prev->next = node->next;
            }
            node->next = NULL;
            node->prev = NULL;
            freeThreadNode(node);

            removed = 1;
            break;
        }
    }

    return removed;
}