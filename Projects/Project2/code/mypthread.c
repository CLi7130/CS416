// File:	mypthread.c

// List all group member's name: Craig Li, Prerak Patel
// username of iLab: craigli, pjp179
// iLab Server: rm.cs.rutgers.edu

#include "mypthread.h"
//#include "queue.h" //modify this?

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE


static tcbNode* tQueue = NULL; //thread queue
static mypthread_t currThread;
static tcb* currTCB;

static int newThreadID = 0;

#ifndef DEBUG
#define DEBUG 1
#endif

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
        initTimer();
        initMain();
        newThreadID++;
    }

    //create TCB
    tcb* newTCB = malloc(sizeof(tcb));

    newTCB->threadID = newThreadID;
    *thread = newThreadID;

    newTCB->threadStatus = READY;
    newTCB->elapsedQuantums = 0;
    newTCB->waitingThread = -1;
    newTCB->value_ptr = NULL;
    newTCB->returnValue = NULL;

    newThreadID++;

    enqueue(&tQueue, newTCB, 0);

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

    currTCB->threadStatus == FINISHED;
    if(currTCB->value_ptr == NULL){
        currTCB->returnValue = value_ptr;
    }
    else{
        //value_ptr is not null, have to return value_ptr
        *currTCB->value_ptr = value_ptr;
        currTCB->threadStatus = REMOVE;
    }
    updateQueueRunnable(&ThreadQueue, currTCB->threadID);
    schedule();
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {

	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread

	// YOUR CODE HERE
    //tcb* tempTCB = getTCB(&ThreadQueue, thread);
    if(checkIfFinished(&ThreadQueue, thread)){

        tcb* tempTCB = getTCB(&ThreadQueue, thread);
        if(value_ptr != NULL){

            tempTCB->threadStatus = REMOVE;
            *value_ptr = tempTCB->returnValue;

        }
        return 0;

    }
    currTCB->threadStatus = WAITING;
    currTCB->waitingThread = thread;
    tcb* tempTCB = getTCB(&ThreadQueue, thread);
    currTCB->value_ptr = value_ptr;
    schedule();
   
};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex,
                          const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex

	// YOUR CODE HERE

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
            lockThisNode->qTCB = currTCB;
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

        ptr->qTCB->threadStatus = READY;
        tcbNode* ptr2 = ptr;
        ptr = ptr->next;
        free(ptr2);
    }
    
    mutex->waitList = NULL;
    mutex->isLocked = 0; //unlocked
	return 0;
};


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in mypthread_mutex_init

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

    cleanup(&ThreadQueue);
    signal(SIGPROF, SIG_IGN);
    
    tcb* prevThread = currTCB;
    currTCB = dequeue(&ThreadQueue);
    if(currTCB == NULL){
        currTCB = prevThread;
        return;
    }
    prevThread->elapsedQuantums++;
    enqueue(&ThreadQueue, prevThread, prevThread->elapsedQuantums);

    initTimer();

    swapcontext(&(prevThread->threadContext), &(currTCB->threadContext));
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
    Helper function that frees all malloc'd memory of a threadNode. Used to cut down redundancy in code.
*/
void freeThreadNode(tcb* deleteNode){
    
    free(deleteNode->threadContext.uc_stack.ss_sp);
    free(deleteNode);
}

/*
    Prints thread nodes in the thread queue
*/
void printThreadQueue(struct tcbNode* tempQueue){
    //have to modify this
    printf("------THREAD QUEUE-----------\n");
    if(tempQueue == NULL || tempQueue->head == NULL){
        printf("Queue is empty or NULL\n");
        return;
    }
    int count = 1;
    struct threadNode* ptr = tempQueue->head;
    while(ptr != NULL){
    
        printf("Node %d.\n", count);
        printf("Thread ID: %d\n",  ptr->threadControlBlock->threadID);
        printf("Thread Status: %d\n", (int) ptr->threadControlBlock->threadStatus);
        printf("Thread Elapsed Quantums (Runtime): %d\n\n",
                 ptr->threadControlBlock->elapsedQuantums);

        ptr = ptr->next;
        count++;
    }
    if(DEBUG){
        printf("LIST DONE\n");
    }
}

/*
    Locates a specific threadControlBlock given a threadID.
*/
struct tcb* getTCB(threadQueue* tQueue, int threadID){

    threadNode* ptr = tQueue->head;

    //printThreadQueue(tQueue);

    while(ptr != NULL){
        if((int) ptr->threadControlBlock->threadID == (int) threadID){
            //match found
            if(DEBUG){
                printf("getThreadNode: node found\n");
            }
            return ptr;
        }
    }
    //not found
    if(DEBUG){
        printf("getThreadNode: why are you here?\n");
    }
    //return;
}
/*
    Searches thread Queue to find a node whose ID matches the ID of the given
    parameter node, then frees the found node and the found node's TCB, then 
    removes it from the tcbNode.
*/
void removeThreadNode(threadNode* findThreadNode){
    //probably need locks/mutex in this function
    ignoreSIGALRM = 1;
    if(DEBUG){
        printf("remove thread node\n");
    }
    if(tQueue == NULL || tQueue->head == NULL){
        //shouldn't be null if we're here, but have to check
        printf("removeThreadNode: tQueue or tQueue head is null\n");
        return;
    }

    threadNode* ptr = tQueue->head;
    while(ptr != NULL){

        if(ptr->threadControlBlock->threadID == findThreadNode->threadControlBlock->threadID)
        {
            //match found, remove/free threadControlBlock and remove from queue
            if(getQueueSize(tQueue) == 1){
                //only head left, remove
                tQueue->head = NULL;
                tQueue->tail = NULL;
                ignoreSIGALRM = 1;
                free(ptr->threadControlBlock->threadContext.uc_stack.ss_sp);
                free(ptr->threadControlBlock);
                free(ptr);
                return;

            }
            if(ptr == tQueue->head && getQueueSize(tQueue) > 1){
                tQueue->head = ptr->next;
                tQueue->head->prev = NULL;
                ptr->next = NULL;

                freeThreadNode(ptr);
                return;
            }
            else if(ptr == tQueue->tail){
                tQueue->tail->prev->next = NULL;
                tQueue->tail = tQueue->tail->prev;
                ptr->prev = NULL;

                freeThreadNode(ptr);
                return;
            }
            else{
                //somewhere in the middle of the queue
                ptr->prev->next = ptr->next;
                ptr->next->prev = ptr->prev;

                ptr->next = NULL;
                ptr->prev = NULL;

                freeThreadNode(ptr);
                return;
            }
        }

        ptr = ptr->next;
    }
    if(DEBUG){
        printf("no match found\n");
    }
    return;
}
/*
    Utility function for counting number of nodes in queue
*/
int getQueueSize(struct tcbNode* inputQueue){
    if(DEBUG){
        printf("getQueueSize\n");
    }
    
    if(inputQueue == NULL || inputQueue->head == NULL){
        printf("NOTHING IN QUEUE\n");
        return 0;
    }
    struct threadNode* ptr = inputQueue->head;
    int count = 0;
    while(ptr != NULL){
        count++;
        ptr = ptr->next;
    }
    return count;
}

/*
    Create timer
*/
void initTimer(){
    struct sigaction timer;
    struct itimerval interval;

    memset(&timer, 0, sizeof(timer));

    timer.sa_handler = &schedule;
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
    //atexit(exitCleanup);

    tcb* mainTCB = malloc(sizeof(tcb));

    currThread = newThreadID;
    mainTCB->threadID = currThread;
    mainTCB->waitingThread = -1;

    mainTCB->threadStatus = READY;
    mainTCB->elapsedQuantums = 0;

    mainTCB->value_ptr = NULL;
    mainTCB->returnValue = NULL;

    currTCB = mainTCB;
    getcontext(&(currTCB->threadContext));

}

void exitCleanup(void){
    PQueue* Li = ThreadQueue;
    while(Li != NULL){
        PQueue* temp = Li;
        Li = Li->next;
        free(temp->control->context.uc_stack.ss_sp);
        free(temp->control);
        free(temp);
    }
    free(runningBlock);
}
