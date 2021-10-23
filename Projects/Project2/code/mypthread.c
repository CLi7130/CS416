// File:	mypthread.c

// List all group member's name: Craig Li, Prerak Patel
// username of iLab: craigli, pjp179
// iLab Server: rm.cs.rutgers.edu

#include "mypthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE


static threadQueue* tQueue = NULL; //thread queue
//threadNode* currRunningThread = NULL; //currently running thread
static ucontext_t mainContext; //context of parent/main?
static ucontext_t schedulerContext; //context of scheduler
static int numThreads = 0;
static int ignoreSIGALRM = 0; //flag to avoid interrupt
struct itimerval timerValue;
struct sigaction timer;
mypthread_mutex_t mutexLock;
int count = 0;
static int init = 0;

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

    /*
        todo:
        setitimer/sigmyAlarm not working?
        have to check swaps/ignore flags?

    */
    //set flag to override SIGALRM for handler
    ignoreSIGALRM = 1;
    //SIGALRM_Handler();
    *thread = ++numThreads; //change value for every new thread
    if(DEBUG){
        printf("IN PTHREAD CREATE\n");
    }

    /* Configure the timer to expire after 10 msec... */

    if(init == 0){
        //initialize everything
        init = 1;
        if(DEBUG){
            printf("IN INIT\n");
        }

        //should go through this on first run of pthread_create
        //initialize timer, sighandler, contexts
        //initialize mutexes here?
        //initialize sigALRM timer
        //initialize sigAction to 0
        
        memset(&timer, 0, sizeof(timer));
        timer.sa_handler = &SIGALRM_Handler;
        if(sigaction(SIGALRM, &timer, NULL) < 0){//use SIGPROF/ITIMER_PROF?
            //signal handler/sigaction failed?
            perror("SIGACTION failed\n");
            exit(EXIT_FAILURE);
        }
        // Configure the timer to expire after QUANTUM msec... 
        timerValue.it_value.tv_sec = 0;
        timerValue.it_value.tv_usec = (QUANTUM * 1000);
        // ... and every QUANTUM msec after that. 
        timerValue.it_interval.tv_sec = 0;
        timerValue.it_interval.tv_usec = (QUANTUM * 1000);
        // Start a .timer.
        setitimer(ITIMER_REAL, &timerValue, NULL);
        
        //queue is empty/Null, make new queue
        tQueue = malloc(sizeof(threadQueue));
        tQueue->head = NULL;
        tQueue->tail = NULL;

        tcb* mainThreadTCB = malloc(sizeof(tcb));
        mainThreadTCB->threadID = *thread;
        mainThreadTCB->threadStatus = MAIN; 
        mainThreadTCB->elapsedQuantums = 0; //use INT_MAX for priority queue?

        threadNode* mainThread = malloc(sizeof(threadNode));
        mainThread->threadControlBlock = mainThreadTCB;
        mainThread->next = NULL;
        mainThread->prev = NULL;

        mainContext.uc_stack.ss_size = STACKSIZE; //set stack size
        //mainContext.uc_link = 0; //no parent?, main process
        //uc_link is where we return to after thread completes
        mainContext.uc_stack.ss_flags = 0;  //no flags on creation
        //create context for main
        mainContext.uc_stack.ss_sp = malloc(STACKSIZE); //create stack
        
        if(mainContext.uc_stack.ss_sp <= 0){
            //check for memory allocation, end program if unsuccessful
            perror("Memory Not Allocated for Main/Parent Stack, exiting\n");
            exit(EXIT_FAILURE);
        }

        //insert into tQueue
        tQueue->head = mainThread;
        tQueue->tail = mainThread;
        if(DEBUG){
            printf("main inserted into queue\n");
        }

        if(getcontext(&mainContext) == -1){
            perror("Initializing Main Context Failed.\n");
            exit(EXIT_FAILURE);
        }
        mainThread->threadControlBlock->threadContext = mainContext;

        if(getcontext(&schedulerContext) == -1){
            perror("Initializing Scheduler Context Failed.\n");
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

        makecontext(&schedulerContext, (void*)schedule, 0); 
        *thread = ++numThreads;
    }

    //create TCB
    tcb* newTCB = malloc(sizeof(tcb));
    newTCB->threadID = *thread;
    newTCB->threadStatus = READY;
    newTCB->elapsedQuantums = 0;
    newTCB->hasDependents = 0;


    if(getcontext(&(newTCB->retContext)) == -1){
        perror("Initializing New Thread Context Failed.\n");
        exit(EXIT_FAILURE);
    }
    newTCB->retContext.uc_stack.ss_sp = malloc(STACKSIZE);

    if(newTCB->retContext.uc_stack.ss_sp <= 0){
        printf("Memory not allocated for new thread: %d (%d)\n",
                 newTCB->threadID, numThreads);
        exit(EXIT_FAILURE);
    }

    //link to exit, when thread is done, cleanup and change status to done
    //link to scheduler context? have to run scheduler anyway
    newTCB->retContext.uc_link = &schedulerContext;
    newTCB->retContext.uc_stack.ss_size = STACKSIZE;
    newTCB->retContext.uc_stack.ss_flags = 0;

    makecontext(&(newTCB->retContext), (void*)finishThread, 1, newTCB->threadID);

    //get context for new thread
    //ucontext_t newThreadContext;
    if(getcontext(&(newTCB->threadContext)) == -1){
        perror("Initializing New Thread Context Failed.\n");
        exit(EXIT_FAILURE);
    }

    newTCB->threadContext.uc_stack.ss_sp = malloc(STACKSIZE);
    if(newTCB->threadContext.uc_stack.ss_sp <= 0){
        printf("Memory not allocated for new thread: %d (%d)\n",
                 newTCB->threadID, numThreads);
        exit(EXIT_FAILURE);
    }

    //link to return context, which finishes job
    newTCB->threadContext.uc_link = &(newTCB->retContext);
    newTCB->threadContext.uc_stack.ss_size = STACKSIZE;
    newTCB->threadContext.uc_stack.ss_flags = 0;

    //set new thread context to TCB
    makecontext(&(newTCB->threadContext), (void*)function, 1, arg);

    //makecontext(&newThreadContext, (void*) threadWrapper, 4, arg, function, (int) newTCB->threadID, tQueue);
    
    

    //insert tcb into new threadnode
    threadNode* newThreadNode = malloc(sizeof(threadNode));
    newThreadNode->threadControlBlock = newTCB;
    newThreadNode->prev = NULL;
    newThreadNode->next = NULL;


    
    //add new node to end of queue
    tQueue->tail->next = newThreadNode;
    newThreadNode->prev = tQueue->tail;
    tQueue->tail = newThreadNode;

    //if(init == 0){
        /*
        threadNode* ptr = tQueue->head;

        tQueue->head = newThreadNode;
        newThreadNode->next = ptr;
        ptr->prev = newThreadNode;
        */
        //
    if(getcontext(&mainContext) == -1){
        perror("Initializing Main Context Failed.\n");
        exit(EXIT_FAILURE);
    }
  //  }
    


    if(DEBUG){
        printf("\n-----------THREAD CREATED----------------\n");
    }
    //printThreadQueue(tQueue);

    //TODO
    //create algo to find smallest timeElapsed
    //should thread be inserted into queue before making context because
    //of threadWrapper? Probably - tQueue needs to have reference to thread
    //we're inserting

    //assign thread context to run with threadWrapper/function called in actual
    //program (not this library)
    

    //remove timer block
    ignoreSIGALRM = 0; 
    SIGALRM_Handler();
    return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {
    //REVISE

	// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// switch from thread context to scheduler context

	// YOUR CODE HERE

    //***logic is for FIFO, may need to find different node for STCF
    //would have to use TCB-> for other schedulers?
    //
    ignoreSIGALRM = 1;
    if(DEBUG){
        printf("----------IN PTHREAD_YIELD------------\n");
        //abort();
    }
    if(tQueue == NULL || tQueue->head == NULL){
        //somehow queue is null or empty, just return
        printf("Pthread_Yield: Empty Queue?\n");
        return 0;
    }
    if(tQueue->head->threadControlBlock->threadStatus == RUNNING){
        //find next thread that is READY
        if(DEBUG){
            printf("pthread_yield: head is running, find next ready node\n");
        }
        threadNode* ptr = tQueue->head->next;
        threadNode* yieldedPTR = tQueue->head;
        
        //we have to put this at the back of the queue to allow 
        //all other threads in the queue to run
        yieldedPTR->threadControlBlock->threadStatus == READY;

        if(ptr != NULL){
            //more than one node in the list, move yieldedPTR node to tail
            //if there is only one node, we can just resume scheduler
            yieldedPTR->next = NULL;
            ptr->prev = NULL;
            tQueue->tail->next = yieldedPTR;
            yieldedPTR->prev = NULL;
            tQueue->tail = yieldedPTR;
            tQueue->head = ptr;
            
        }
        swapcontext(&yieldedPTR->threadControlBlock->threadContext, 
                    &schedulerContext);
    }
    ignoreSIGALRM = 0;
    //SIGALRM_Handler();
	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread

	// YOUR CODE HERE
    ignoreSIGALRM = 1;
    if(DEBUG){
        printf("\n-----------IN PTHREAD_EXIT---------\n");
    }
    //make sure you release locks possessed by this thread
    //how to get threadID in this function?
    tQueue->head->threadControlBlock->threadStatus = FINISHED;
    //removeThreadNode(tQueue->head);

    if(value_ptr != NULL){
        //have to get return value of thread
        retVals[(int) tQueue->head->threadControlBlock->threadID] = value_ptr;
    }
    ignoreSIGALRM = 0;
    //SIGALRM_Handler();
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {

	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread

	// YOUR CODE HERE


    /*
        Potential scheduling idea, when join is called, move current thread to
        just behind thread to join, so when thread to join completes, we already
        have correct ordering of next thread. Can multiple threads join?
    */
    ignoreSIGALRM = 1;
    if(DEBUG){
        printf("-----------IN MYPTHREAD JOIN--------\n");
        printThreadQueue(tQueue);
        printf("pthread_join: FIND THREAD %d\n", (int) thread);
    }
    threadNode* waitForThisThread = getThreadNode(thread);
    //fix this, thread to join has to be marked, joinee is put as waiting.
    //waitForThisThread->threadControlBlock->threadStatus = ;
    if(DEBUG){
        printf("FOUND THREAD\n");
    }
    threadNode* ptr = tQueue->head;
    threadNode* joinNode = tQueue->head;

    while(ptr != NULL){
        if(ptr->threadControlBlock->threadID ==       
            waitForThisThread->threadControlBlock->threadID){
                //place current thread behind this thread, have to wait for it to finish.
                //may need to use WAITING instead of READY here.
            //waitForThisThread->threadControlBlock->threadStatus = READY;

            //move from front of queue to behind waitForThisThread
            tQueue->head = tQueue->head->next;
            tQueue->head->prev = NULL;
            joinNode->next = NULL;

            if(ptr->next == NULL){
                tQueue->tail = joinNode;
                ptr->next = joinNode;
                joinNode->next = NULL;
            }
            else{
                ptr->next->prev = joinNode;
                joinNode->next = ptr->next;
                ptr->next = joinNode;
            }

            if(value_ptr != NULL){
                //have to save return value
                *value_ptr = retVals[(int) thread];
            }
            printThreadQueue(tQueue);
            ignoreSIGALRM = 0;
            SIGALRM_Handler();

        }
        ptr = ptr->next;
    }
    ignoreSIGALRM = 0;
    SIGALRM_Handler();
    //node not found
    //abort();



    /*
    //thread not in queue
    if(waitForThisThread == NULL){
        if(DEBUG){
            printf("pthread_yield: THIS THREAD NOT IN QUEUE\n");
        }
        if(value_ptr != NULL){
            //have to save return value
            *value_ptr = retVals[(int) thread];
        }
        ignoreSIGALRM = 0;
        return 0;
    }    
    else if(waitForThisThread->threadControlBlock->threadStatus == FINISHED){
        //thread is done or already removed from queue
        if(DEBUG){
            printf("pthread_yield: THIS THREAD IS FINISHED\n");
        }
        if(value_ptr != NULL){
            //save return value
            *value_ptr = 
                retVals[(int) waitForThisThread->threadControlBlock->threadID];
        }        

        removeThreadNode(waitForThisThread); 
        ignoreSIGALRM = 0;
        return 0;
    }
    else if(waitForThisThread->threadControlBlock->threadStatus == READY){
        //move current node behind this node?

    }
    else{
        //thread is still running, yield this thread until waitForThisThread
        //finishes
        if(DEBUG){
            printf("pthread_yield: THIS THREAD IS NOT FINISHED\n");
        }
        //signifies that other threads are waiting on this one
        waitForThisThread->threadControlBlock->hasDependents = 1;
        //update LL here, send threadNode behind thread that we are waiting on?
        ignoreSIGALRM = 0;
        SIGALRM_Handler(); //handle joined thread
        //infinite loop here?
        
        if(value_ptr != NULL){
            //save return value
            *value_ptr = 
                retVals[(int) thread];
        }
        //come back here to test functionality
        if(DEBUG){
            printf("ABORT HERE, CHANGE ME\n");
        }

        removeThreadNode(waitForThisThread);
        return 0;
    }

    if(DEBUG){
        printf("HOW ARE YOU HERE? PTHREAD_JOIN\n"); 
    }*/

    
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

/*
    Once a job is finished running, it's uc_link will take it here, and it 
    is marked as finished, then we call sigalrm handler to take us back to schedule.
*/
void finishThread(int threadID){
    ignoreSIGALRM = 1;
    threadNode* finishedThread = getThreadNode(threadID);
    if(DEBUG){
        printf("~~~~~~~~~~~~IN FINISH THREAD FUNCTION~~~~~~~~~~~~~~~~~\n");
    }

    if(finishedThread == NULL || finishedThread->threadControlBlock == NULL){
        perror("TRYING TO FIND DELETED JOB\n");
        exit(EXIT_FAILURE);
    }
    finishedThread->threadControlBlock->threadStatus = FINISHED;

    ignoreSIGALRM = 0;
    SIGALRM_Handler();
}

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
    
    //if(SCHEDULE == FIFO){
    //figure out how to define FIFO, STCF in headers
    //IT'S IN THE MAKEFILE ^

    // schedule policy 
    //- can change this to if statements based on comment above?
    if(DEBUG){
        printf("\n-----------IN SCHEDULE---------\n");
    }
    
    ignoreSIGALRM = 1;
    #ifndef MLFQ
        // Choose STCF

        //current implementation is fifo, change when STCF is implemented
        sched_fifo();
    #elif
        
        // Choose MLFQ
        //not req for 416
        
        //current implementation schedules stcf if not FIFO
        sched_stcf();
    #endif
    
    //not req to implement MLFQ for 416
    //we only need to route to stcf here?

}

/* First in First Out (FIFO) scheduling algorithm */
static void sched_fifo() {
	// Your own implementation of FIFO
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE

    /*
        Fifo should run thread to completion based on queue order
        - remove head from queue once job is done
            - adjust queue afterwards
        - job should remain runnning?
    */
    //block myAlarm
    
    if(tQueue->head == NULL){
        printf("Empty Queue, check logic?\n");
    }
    if(DEBUG){
        printf("\n----------IN FIFO SCHEDULER (START)--------\n");
        printThreadQueue(tQueue);
    }

    threadNode* currRunningThread = tQueue->head;
    status currStatus = currRunningThread->threadControlBlock->threadStatus;
    //thread should already be running thanks to threadWrapper()
    //currThread->threadControlBlock->threadStatus = RUNNING;

    /*
        - if threadStatus == finished, remove from queue
        - if threadStatus == running, set context to currThread
            - fifo runs until completion?
        - if threadstatus == MAIN or null, we don't have threads
            left to run? swap to main.

        - need to implement case for threadStatus WAITING
            - this is for when threads call join, or potentially are 
                waiting for a mutex?
    */
   if(currRunningThread == NULL || currStatus == MAIN){
        //main thread, or no nodes left in queue
        
        if(DEBUG){
            printThreadQueue(tQueue);
        }
        if(getQueueSize(tQueue) == 1 && 
                tQueue->head->threadControlBlock->threadStatus == MAIN){
            //main is only node in queue,don't have to ignore sigalrm anymore? 
            printf("sched_fifo: no nodes left in queue/main?\n");
            ignoreSIGALRM = 0;

            removeThreadNode(tQueue->head);
            tQueue->head == NULL;
            tQueue->tail == NULL;
            setcontext(&mainContext);
        }
        else{
            //main not only node in queue, move to back
            if(DEBUG){
                printf("sched_fifo: main first in queue let other nodes go first?\n");
            }
            threadNode* ptr = tQueue->head;
            threadNode* iterator = tQueue->head->next;
            
            tQueue->head = tQueue->head->next;
            ptr->next = NULL;
            ptr->prev = NULL;
            tQueue->head->prev = NULL;

            //move main thread node to back of list.
            ptr->prev = tQueue->tail;
            tQueue->tail->next = ptr;
            tQueue->tail = ptr;
            if(DEBUG){
                printf("sched_fifo: moving main\n");
                printThreadQueue(tQueue);
            }
            //switch to next thread in queue
            ignoreSIGALRM = 0;
            setcontext(&schedulerContext);
            
        }

    }
    else if(currStatus == RUNNING){
        //swap back to thread context?
        //fifo runs until completion.
        if(DEBUG){
            printf("sched_fifo: job not done in fifo, resuming thread\n");
            printf("INCREASING ELAPSED QUANTUMS\n");
            printThreadQueue(tQueue);
        }
        currRunningThread->threadControlBlock->elapsedQuantums++;
        ignoreSIGALRM = 0;
        swapcontext(&currRunningThread->threadControlBlock->threadContext, 
                    &schedulerContext);
        
    }
    else if(currStatus == FINISHED){
        //thread is done working, remove from queue
        //alert all threads waiting on this thread?
        if(DEBUG){
            printf("sched_fifo: job done, remove from queue\n");
            printThreadQueue(tQueue);
        }
        removeThreadNode(currRunningThread);
        printThreadQueue(tQueue);
        if(tQueue->head != NULL){
            //still have nodes in the queue
            ignoreSIGALRM = 0;
            setcontext(&schedulerContext);
        }

    }
    else if(currStatus == READY){
        //shouldn't be here? this means that the thread hasn't started
        //running...could be from yield
        if(DEBUG){
            printf("sched_fifo: thread not started\n");
        }
        currRunningThread->threadControlBlock->threadStatus = RUNNING;
        printThreadQueue(tQueue);
        ignoreSIGALRM = 0;
        setcontext(&currRunningThread->threadControlBlock->threadContext);
    }
    

    //shouldn't ever get here.
    if(DEBUG){
        printf("You shouldn't be at the end of sched_fifo. Why is that?\n");
    }

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
    SIGALRM Handler, swaps context to scheduler every time QUANTUM is elapsed, and SIGALRM is called.
*/
static void SIGALRM_Handler(){
    /*
    count++;
    if(count == 40){
        abort();
    }*/
    if(DEBUG){
        printf("\n------IN SIGALRM HANDLER------\n");
    }
    if(ignoreSIGALRM == 1){
        //flag to ignore myAlarms
        if(DEBUG){
            printf("SIGALRM HANDLER EXIT EARLY\n");
        }
        return;
    }
    ignoreSIGALRM = 1;
    printThreadQueue(tQueue);

    //swap context to next in queue? ->change to scheduler
    //check status of currThread and next priority thread

    //this is for FIFO, may need to adjust for STCF
    threadNode* currThread = tQueue->head;
    if(currThread == NULL){
        //shouldn't be here? should have something in queue, main should be last one
        if(DEBUG){
            printf("SIGALRM: NO QUEUE OR NULL\n");
        }
        //abort();

    } 
    if(currThread->threadControlBlock->threadStatus == MAIN){
        //main thread, switch to scheduler
        //getcontext(&mainContext);
        if(DEBUG){
            printf("SIGALRM Handler: SWAP MAIN TO scheduler------\n");
        }
        ignoreSIGALRM = 0;
        if(swapcontext(&mainContext, &schedulerContext) == -1){
            //error swapping context
            perror("Swap Context Error between main and scheduler\n");
            exit(EXIT_FAILURE);
        }
        
    }
    else if (currThread->threadControlBlock->threadStatus == RUNNING){
        //current Thread was running
        //update elapsed quantums here?
        //check logic here - do we have to save thread state?
        printf("SAH: RUNNING - SWAP TO THREAD FROM SCHEDULER------\n");

        if(swapcontext(&currThread->threadControlBlock->threadContext, 
                        &schedulerContext) == -1){
            //error swapping current thread with scheduler
            perror("Swap context between thread and scheduler failed\n");
            exit(EXIT_FAILURE);
        }
        
    }
    else if(currThread->threadControlBlock->threadStatus == READY){
        //start this thread?
        if(DEBUG){
            printf("SAH: READY - SWAP TO THREAD FROM SCHEDULER------\n");
        }
        printThreadQueue(tQueue);
        currThread->threadControlBlock->threadStatus == RUNNING;
        if(swapcontext(&currThread->threadControlBlock->threadContext, 
                        &schedulerContext) == -1){
            //error swapping current thread with scheduler
            perror("Swap context between thread and scheduler failed\n");
            exit(EXIT_FAILURE);
        }
    }


}

/*
    Helper function that frees all malloc'd memory of a threadNode. Used to cut down redundancy in code.
*/
void freeThreadNode(threadNode* deleteNode){
    
    free(deleteNode->threadControlBlock->threadContext.uc_stack.ss_sp);
    free(deleteNode->threadControlBlock->retContext.uc_stack.ss_sp);
    free(deleteNode->threadControlBlock);
    free(deleteNode);
}

/*
    Frees all thread Nodes, incomplete. modify for individual nodes?
*/
void freeThreadNodes(threadNode* head){
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
    Prints thread nodes in the thread queue
*/
void printThreadQueue(struct threadQueue* tempQueue){
    ignoreSIGALRM = 1;
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
    Locates a specific thread given a threadID.
*/
struct threadNode* getThreadNode(int threadID){
    ignoreSIGALRM = 1;
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
    removes it from the threadQueue.
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
int getQueueSize(struct threadQueue* inputQueue){
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

