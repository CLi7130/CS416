// File:	mypthread.c

// List all group member's name: Craig Li, Prerak Patel
// username of iLab: craigli, pjp179
// iLab Server: rm.cs.rutgers.edu

#include "mypthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE


threadQueue* tQueue = NULL; //thread queue
threadNode* currRunningThread = NULL; //currently running thread
ucontext_t mainContext; //context of parent/main?
ucontext_t schedulerContext; //context of scheduler
int numThreads = 0;
int ignoreSIGALRM = 0; //flag to avoid interrupt
struct itimerval timerValue;
struct sigaction sigAction;
mypthread_mutex_t mutexLock;

/*
    Threadwrapper function for use with threads to change state when done
*/
void threadWrapper(void* arg, void*(*function)(void*), int threadID,
                    threadQueue* tQueue){
    /*
    void thread_wrapper_func(thread_func) {

        tcb->status = RUNNING;
        thread_func();
        tcb->status = DONE;
    */
   //find threadID, set threadStatus to RUNNING
   threadNode* runningThread = getThreadNode(threadID, tQueue->head);
   runningThread->threadControlBlock->threadStatus = RUNNING;
   //void* theadFunction = (*function)(arg);
   function(arg);
   //find thread ID, set threadStatus to FINISHED
   //potentially change to scheduler here?
   //if we are at this point, the function has finished running within the
   //time quantum -> switch to scheduler to schedule new thread
   runningThread->threadControlBlock->threadStatus = FINISHED;

}
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

    /*
        Steps to create a thread:
        1. create Thread Control Block
        2. create/get thread context
            2a. during pthread_create, makecontext() will be used, we need to 
                update the context structure before that
        3. Create sighandler
        4. initialize timer
        4. Create Runqueue
            3a. once thread context is set, add thread to scheduler runqueue (linked list/queue)
        5. make threadwrapper function to change thread state after execution? check piazza


        create main_context
            - continually update main context through swapcontext(&maincontext, &scheduler)?
            - 
        uc_link should point to pthread_exit for non main threads?

    */
    //set flag to override SIGALRM for handler
    ignoreSIGALRM = 1;
    
    *thread = ++numThreads; //change value for every new thread

    //check if queue is empty/null

    if(tQueue == NULL){
        //should go through this on first run of pthread_create
        //initialize timer, sighandler, contexts
        //initialize mutexes here?
        //initialize sigALRM timer
        timerValue.it_value.tv_sec = QUANTUM / 1000;
        //convert seconds to ms by dividing by 1000
        timerValue.it_value.tv_usec = (QUANTUM * 1000);
        //convert microseconds to milliseconds by multiplying by 1000
        timerValue.it_interval = timerValue.it_value;
        //set interval of timer, have to repeat this when sigalarm is called

        //initialize sigAction to 0
        memset(&sigAction, 0, sizeof(struct sigaction));
        //set sa_mask to empty set
        sigemptyset(&(sigAction.sa_mask));

        //set sigAction handler to user defined SIGARLM_handler
        sigAction.sa_handler = SIGALRM_Handler;
        if(sigaction(SIGALRM, &sigAction, NULL) == -1){
            //signal handler/sigaction failed?
            perror("SIGACTION failed\n");
            exit(EXIT_FAILURE);
        }

        //call handler to start timer?
        SIGALRM_Handler();

        //queue is empty/Null, make new queue
        tQueue = malloc(sizeof(threadQueue));
    
        //create context for main
        if(getcontext(&mainContext) == -1){
            perror("Initializing Main Context Failed.\n");
            exit(EXIT_FAILURE);
        }
        mainContext.uc_stack.ss_sp = malloc(STACKSIZE); //create stack
        
        if(mainContext.uc_stack.ss_sp <= 0){
            //check for memory allocation, end program if unsuccessful
            perror("Memory Not Allocated for Main/Parent Stack, exiting\n");
            exit(EXIT_FAILURE);
        }

        tcb* mainThreadTCB = malloc(sizeof(tcb));
        mainThreadTCB->threadID = *thread;
        mainThreadTCB->threadStatus = READY; 
        mainThreadTCB->elapsedQuantums = 0;

        threadNode* mainThread = malloc(sizeof(threadNode));
        mainThread->threadControlBlock = mainThreadTCB;
        mainThread->next = NULL;
        mainThread->prev = NULL;

        mainContext.uc_stack.ss_size = STACKSIZE; //set stack size
        mainContext.uc_link = 0; //no parent?, main process
        //uc_link is where we return to after thread completes
        mainContext.uc_stack.ss_flags = 0;  //no flags on creation

        if(getcontext(&schedulerContext) == -1){
            error("Initializing Scheduler Context Failed.\n");
            exit(EXIT_FAILURE);
        }

        schedulerContext.uc_stack.ss_sp = malloc(STACKSIZE);

        if(schedulerContext.uc_stack.ss_sp <= 0){
            //memory allocation error for context
            perror("Memory Not allocated for Scheduler Context\n");
            exit(EXIT_FAILURE);
        }

        //return after scheduler to main? could set link to 0
        schedulerContext.uc_link = 0; //&mainContext;
        schedulerContext.uc_stack.ss_size = STACKSIZE;
        schedulerContext.uc_stack.ss_flags = 0;

        //make context for scheduler here
        /*
            make threadNode for main? -> make tcb, set  threadnode->threadControlBlock, put into scheduler so that we can return to main once all threads finish?
        */

        makecontext(&schedulerContext, (void*) &schedule, 0); 
    }

    //create TCB
    tcb* newTCB = malloc(sizeof(tcb));
    newTCB->threadID = *thread;
    newTCB->threadStatus = READY;
    newTCB->elapsedQuantums = 0;

    //get context for new thread
    ucontext_t newThreadContext;
    getcontext(&newThreadContext);
    if(getcontext(&newThreadContext) == -1){
        error("Initializing New Thread Context Failed.\n");
        exit(EXIT_FAILURE);
    }

    newThreadContext.uc_stack.ss_sp = malloc(STACKSIZE);
    if(newThreadContext.uc_stack.ss_sp <= 0){
        fprintf("Memory not allocated for new thread: %d (%d)\n",
                newTCB->threadID, numThreads);
        exit(EXIT_FAILURE);
    }

    //link to exit, when thread is done, cleanup and change status to done
    //link to scheduler context? have to run scheduler anyway
    newThreadContext.uc_link = &schedulerContext;
    newThreadContext.uc_stack.ss_size = STACKSIZE;
    newThreadContext.uc_stack.ss_flags = 0;

    //set new thread context to TCB
    newTCB->threadContext = newThreadContext;

    //insert tcb into new threadnode
    threadNode* newThreadNode = malloc(sizeof(threadNode));
    newThreadNode->threadControlBlock = newTCB;
    newThreadNode->next = NULL;
    newThreadNode->prev = NULL;

    //queue not null, insert threadNode into queue
    //set new node to end of list, and change tail pointer

    //this is for FIFO, work on other criteria for when we 
    //implement STCF
    
    //if scheduler is FIFO 
    //do we need locks here for the LL? probably?
    if(tQueue->head == NULL){
        //queue is empty/just initialized
        tQueue->head = newThreadNode;
        tQueue->tail = newThreadNode;
    }
    else{
        //queue has nodes in it, append threadNode to rear
        newThreadNode->prev = tQueue->tail;
        newThreadNode->next = NULL;
        tQueue->tail->next = newThreadNode;
        tQueue->tail = newThreadNode;
        
    }

    //if scheduler is STCF - preemptive shortest job first

    //TODO
    //create algo to find smallest timeElapsed
    //should thread be inserted into queue before making context because
    //of threadWrapper? Probably - tQueue needs to have reference to thread
    //we're inserting

    //assign thread context to run with threadWrapper/function called in actual
    //program (not this library)
    makecontext(&newThreadContext, &threadWrapper, 4, arg, function, 
                    (int) newTCB->threadID, tQueue->head);


    //remove timer block
    ignoreSIGALRM = 0;
    return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {

	// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// switch from thread context to scheduler context

	// YOUR CODE HERE

	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread

	// YOUR CODE HERE
    ignoreSIGALRM = 1;
    printf("\n-----------IN PTHREAD_EXIT---------\n");
    //make sure you release locks possessed by this thread
    //how to get threadID in this function?

    ignoreSIGALRM = 0;
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {

	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread

	// YOUR CODE HERE
	return 0;
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

        //use test and set?
        return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex) {
	// Release mutex and make it available again.
	// Put threads in block list to run queue
	// so that they could compete for mutex later.

	// YOUR CODE HERE
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
    

    // schedule policy
    #ifndef MLFQ
        // Choose STCF
    #else
        // Choose MLFQ
        //not req for 416
    #endif

    //not req to implement MLFQ for 416
    //we only need to route to stcf here?

}

/* Preemptive SJF (STCF) scheduling algorithm */
static void sched_stcf() {
	// Your own implementation of STCF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
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
    SIGALRM Handler
*/
static void SIGALRM_Handler(){

    /*
        TODO:
        swap threads/contexts when alarm is reset
    */
    //initialize sigALRM timer
    timerValue.it_value.tv_sec = QUANTUM / 1000;
    //convert seconds to ms by dividing by 1000
    timerValue.it_value.tv_usec = (QUANTUM * 1000);
    //convert microseconds to milliseconds by multiplying by 1000
    timerValue.it_interval = timerValue.it_value;
    //set interval of timer, have to repeat this when sigalarm is called

    //condition resets and fires timer to QUANTUM
    if(setitimer(ITIMER_REAL, &timerValue, NULL) == -1){
        //setitimer error
        perror("Error setting timer (setitimer())\n");
        exit(EXIT_FAILURE);
    }
    //reset timer to quantum (10ms)
    if(ignoreSIGALRM == 1){
        //flag to ignore alarms
        return;
    }
    ignoreSIGALRM == 1;

    //swap context to next in queue?
    //check status of currThread and next priority thread

    swapcontext(&mainContext, &schedulerContext);

    return;

}
/*
    Frees all thread Nodes, incomplete. modify for individual nodes?
*/
void freeThreadNodes(struct threadNode* head){
    if(head == NULL){
        return;
    }
    struct threadNode* next = head->next;
    
    //free head's TCB, stack space, etc

    //free thread stack space
    free(head->threadControlBlock->threadContext.uc_stack.ss_sp);
    //free node
    free(head);
    freeThreadNodes(next);
}

/*
    Prints thread nodes, incomplete
*/
void printThreadNodes(struct threadNode* head){
    if(head == 0){
        return;
    }
    int count = 1;
    struct threadNode* ptr = head;
    while(ptr != NULL){
    
        printf("Node %d.\n", count);
        printf("Thread ID: %d\n", ptr->threadControlBlock->threadID);
        printf("Thread Status: %s\n", ptr->threadControlBlock->threadStatus);
        printf("Thread Elapsed Quantums (Runtime): %d\n", ptr->threadControlBlock->elapsedQuantums);

        ptr = ptr->next;
    }
}

/*
    Locates a specific thread given a threadID and the head of the queue/linked list.
*/
struct threadNode* getThreadNode(int threadID, struct threadNode* head){
    
    threadNode* ptr = head;

    while(ptr != NULL){
        if(ptr->threadControlBlock->threadID == threadID){
            //match found
            return ptr;
        }
    }
    //not found
    return -1;
}