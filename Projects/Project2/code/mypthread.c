// File:	mypthread.c

// List all group member's name: Craig Li, Prerak Patel
// username of iLab: craigli, pjp179
// iLab Server: rm.cs.rutgers.edu

#include "mypthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE


threadQueue* tQueue = NULL; //thread queue
//threadNode* currRunningThread = NULL; //currently running thread
ucontext_t mainContext; //context of parent/main?
ucontext_t schedulerContext; //context of scheduler
int numThreads = 0;
int ignoreSIGALRM = 0; //flag to avoid interrupt
struct itimerval timerValue;
struct sigaction sigAction;
mypthread_mutex_t mutexLock;
int count = 0;

/*
    Threadwrapper function for use with threads to change state when done
*/
void* threadWrapper(void * arg, void *(*function)(void*), int threadID,
                    threadQueue* tQueue){
    /*
    void thread_wrapper_func(thread_func) {

        tcb->status = RUNNING;
        thread_func();
        tcb->status = DONE;
    */
   //find threadID, set threadStatus to RUNNING
   printf("--------IN WRAPPER----------\n");
   threadNode* runningThread = tQueue->head;
   runningThread->threadControlBlock->threadStatus = RUNNING;

   //store return value
   retVals[(int) runningThread->threadControlBlock->threadID] 
            = (*function)(arg);

    //find thread ID, set threadStatus to FINISHED
   runningThread->threadControlBlock->threadStatus = FINISHED;  
   SIGALRM_Handler();
   //potentially change to scheduler here?
   //***if we are at this point, the function has finished running within the
   //time quantum -> switch to scheduler to schedule new thread

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

    */
    //set flag to override SIGALRM for handler
    ignoreSIGALRM = 1;
    
    *thread = ++numThreads; //change value for every new thread
    printf("IN PTHREAD CREATE\n");
    //create TCB
    tcb* newTCB = malloc(sizeof(tcb));
    newTCB->threadID = *thread;
    newTCB->threadStatus = READY;
    newTCB->elapsedQuantums = 0;
    newTCB->hasDependents = 0;

    //get context for new thread
    ucontext_t newThreadContext;
    getcontext(&newThreadContext);
    if(getcontext(&newThreadContext) == -1){
        perror("Initializing New Thread Context Failed.\n");
        exit(EXIT_FAILURE);
    }

    newThreadContext.uc_stack.ss_sp = malloc(STACKSIZE);
    if(newThreadContext.uc_stack.ss_sp <= 0){
        printf("Memory not allocated for new thread: %d (%d)\n",
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

        //make context for scheduler here
        /*
            make threadNode for main? -> make tcb, set  threadnode->threadControlBlock, put into scheduler so that we can return to main once all threads finish?
        */
        //(void*)
        makecontext(&schedulerContext, schedule, 0); 
        newThreadContext.uc_link = &schedulerContext;
        printf("scheduler context made\n");

        //queue is empty/Null, make new queue
        tQueue = malloc(sizeof(threadQueue));
        tQueue->head = NULL;
        tQueue->tail = NULL;
        
        //if scheduler is FIFO 
        //********IMPLEMENT LOCKS/MUTEX HERE FOR THE QUEUE**********
        printThreadQueue(tQueue);
        if(tQueue->head == NULL){
            //queue is empty/just initialized
            printf("----------EMPTY LL ADDING NODE-------------\n");
            tQueue->head = newThreadNode;
            tQueue->tail = newThreadNode;
            printThreadQueue(tQueue);
        }
        else{
            //queue has nodes in it, append threadNode to rear
            printf("----------NONEMPTY LL ADDING NODE-------------\n"); 
            newThreadNode->prev = tQueue->tail;
            newThreadNode->next = NULL;
            tQueue->tail->next = newThreadNode;
            tQueue->tail = newThreadNode;
            printThreadQueue(tQueue);
        }
        
        //is this initialized correctly? we never go into the thread wrapper?
        makecontext(&newThreadContext, (void*) threadWrapper, 4, arg, function, (int) newTCB->threadID, tQueue->head);
        newTCB->threadContext = newThreadContext;

        //create context for main
        mainContext.uc_stack.ss_sp = malloc(STACKSIZE); //create stack
        
        if(mainContext.uc_stack.ss_sp <= 0){
            //check for memory allocation, end program if unsuccessful
            perror("Memory Not Allocated for Main/Parent Stack, exiting\n");
            exit(EXIT_FAILURE);
        }

        tcb* mainThreadTCB = malloc(sizeof(tcb));
        mainThreadTCB->threadID = *thread;
        mainThreadTCB->threadStatus = MAIN; 
        mainThreadTCB->elapsedQuantums = 0;

        threadNode* mainThread = malloc(sizeof(threadNode));
        mainThread->threadControlBlock = mainThreadTCB;
        mainThread->next = NULL;
        mainThread->prev = NULL;

        mainContext.uc_stack.ss_size = STACKSIZE; //set stack size
        mainContext.uc_link = 0; //no parent?, main process
        //uc_link is where we return to after thread completes
        mainContext.uc_stack.ss_flags = 0;  //no flags on creation

        printf("main context made");
        
        if(getcontext(&mainContext) == -1){
            perror("Initializing Main Context Failed.\n");
            exit(EXIT_FAILURE);
        }
        //start timer here, will return immediately due to ignoreSIGALRM
        SIGALRM_Handler();
        mainThread->threadControlBlock->threadContext = mainContext;

    }
    else{
        //trigger this branch if tQueue is not null / not first time
        //through

        //queue not null, insert threadNode into queue
        //set new node to end of list, and change tail pointer
                //if scheduler is FIFO 
        //********IMPLEMENT LOCKS/MUTEX HERE FOR THE QUEUE**********
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
        makecontext(&newThreadContext, (void*) threadWrapper, 4, arg, function, (int) newTCB->threadID, tQueue->head);
        newTCB->threadContext = newThreadContext;
        //this is for FIFO, work on other criteria for when we 
        //implement STCF
        //if scheduler is STCF - preemptive shortest job first
    }
    printf("\nTHREAD CREATED\n");
    printThreadQueue(tQueue);

    //TODO
    //create algo to find smallest timeElapsed
    //should thread be inserted into queue before making context because
    //of threadWrapper? Probably - tQueue needs to have reference to thread
    //we're inserting

    //assign thread context to run with threadWrapper/function called in actual
    //program (not this library)
    

    
    //remove timer block
    ignoreSIGALRM = 0; 
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
    printf("----------IN PTHREAD_YIELD------------\n");
    if(tQueue == NULL || tQueue->head == NULL){
        //somehow queue is null or empty, just return
        printf("Pthread_Yield: Empty Queue?\n");
        return 0;
    }
    if(tQueue->head->threadControlBlock->threadStatus == RUNNING){
        //find next thread that is READY
        printf("pthread_yield: head is running, find next ready node\n");
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
    printf("\n-----------IN PTHREAD_EXIT---------\n");
    //make sure you release locks possessed by this thread
    //how to get threadID in this function?
    tQueue->head->threadControlBlock->threadStatus = FINISHED;
    //removeThreadNode(tQueue->head);

    if(value_ptr != NULL){
        //have to get return value of thread
        retVals[(int) tQueue->head->threadControlBlock->threadID] = value_ptr;
    }
    ignoreSIGALRM = 0;
    SIGALRM_Handler();
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
    printf("-----------IN MYPTHREAD JOIN--------\n");
    printThreadQueue(tQueue);

    threadNode* waitForThisThread = getThreadNode(thread);
    //fix this, thread to join has to be marked, joinee is put as waiting.
    //waitForThisThread->threadControlBlock->threadStatus = ;
    printf("FOUND THREAD\n");

    //thread not in queue
    if(waitForThisThread == NULL){
        printf("pthread_yield: THIS THREAD NOT IN QUEUE\n");
        if(value_ptr != NULL){
            //have to save return value
            *value_ptr = retVals[(int) thread];
        }
        ignoreSIGALRM = 0;
        return 0;
    }    
    else if(waitForThisThread->threadControlBlock->threadStatus == FINISHED){
        //thread is done or already removed from queue
        printf("pthread_yield: THIS THREAD IS FINISHED\n");
        if(value_ptr != NULL){
            //save return value
            *value_ptr = 
                retVals[(int) waitForThisThread->threadControlBlock->threadID];
        }        

        removeThreadNode(waitForThisThread); 
        ignoreSIGALRM = 0;
        return 0;
    }
    else{
        //thread is still running, yield this thread until waitForThisThread
        //finishes
        printf("pthread_yield: THIS THREAD IS NOT FINISHED\n");
        //signifies that other threads are waiting on this one
        waitForThisThread->threadControlBlock->hasDependents = 1;
        //update LL here, send threadNode behind thread that we are waiting on?
        ignoreSIGALRM = 0;
        SIGALRM_Handler(); //handle joined thread
        
        if(value_ptr != NULL){
            //save return value
            *value_ptr = 
                retVals[(int) thread];
        }
        //come back here to test functionality
        printf("ABORT HERE, CHANGE ME\n");
        removeThreadNode(waitForThisThread);
        return 0;
    }
    
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
    
    //if(SCHEDULE == FIFO){
    //figure out how to define FIFO, STCF in headers
    //IT'S IN THE MAKEFILE ^

    // schedule policy 
    //- can change this to if statements based on comment above?
    printf("\n-----------IN SCHEDULE---------\n");

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
    //block alarm
    ignoreSIGALRM = 1;
    if(tQueue->head == NULL){
        printf("Empty Queue, check logic?\n");
    }
    printf("\n----------IN FIFO SCHEDULER (START)--------\n");
    printThreadQueue(tQueue);
    putchar('\n');
    /*
    count++;
    if(count == 10){
        abort();
    }*/
    threadNode* currRunningThread = tQueue->head;
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
   if(currRunningThread == NULL || 
        currRunningThread->threadControlBlock->threadStatus == MAIN)
    {
        //main thread, or no nodes left in queue
        printf("sched_fifo: no nodes left in queue/main?\n");
        printThreadQueue(tQueue);
        ignoreSIGALRM = 0;
        setcontext(&mainContext);
    }
    else if(currRunningThread->threadControlBlock->threadStatus == RUNNING){
        //swap back to thread context?
        //fifo runs until completion.
        printf("sched_fifo: job not done in fifo, resuming thread\n");
        printThreadQueue(tQueue);
        ignoreSIGALRM = 0;
        setcontext(&currRunningThread->threadControlBlock->threadContext);
        
    }
    else if(currRunningThread->threadControlBlock->threadStatus == FINISHED){
        //thread is done working, remove from queue
        //alert all threads waiting on this thread?
        printf("sched_fifo: job done, remove from queue\n");
        printThreadQueue(tQueue);
        removeThreadNode(currRunningThread);
        if(tQueue->head != NULL){
            //still have nodes in the queue
            ignoreSIGALRM = 0;
            setcontext(&tQueue->head->threadControlBlock->threadContext);
        }
        else{
            //no more nodes in queue, done?
            ignoreSIGALRM = 0;
            setcontext(&mainContext);
        }
    }
    else if(currRunningThread->threadControlBlock->threadStatus == READY){
        //shouldn't be here? this means that the thread hasn't started
        //running...could be from yield
        printf("sched_fifo: thread not started\n");
        currRunningThread->threadControlBlock->threadStatus = RUNNING;
        printThreadQueue(tQueue);
        
        setcontext(&currRunningThread->threadControlBlock->threadContext);
    }
    

    //shouldn't ever get here.
    printf("You shouldn't be at the end of sched_fifo. Why is that?\n");

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
    if(count == 15){
        printf("ABORTING\n");
        abort();
    }
    */
    printf("\n------IN SIGALRM HANDLER------\n");
    //initialize sigALRM timer
    timerValue.it_value.tv_sec = QUANTUM / 1000;
    //convert seconds to ms by dividing by 1000
    timerValue.it_value.tv_usec = (QUANTUM * 1000);
    //convert microseconds to milliseconds by multiplying by 1000
    timerValue.it_interval = timerValue.it_value;
    //set interval of timer, have to repeat this when sigalarm is called

    //condition resets and sets timer to QUANTUM
    if(setitimer(ITIMER_REAL, &timerValue, NULL) == -1){
        //setitimer error
        perror("Error setting timer (setitimer())\n");
        exit(EXIT_FAILURE);
    }
    //reset timer to quantum (10ms)
    if(ignoreSIGALRM == 1){
        //flag to ignore alarms
        printf("SIGALRM HANDLER EXIT EARLY\n");
        return;
    }
    ignoreSIGALRM == 1;

    //swap context to next in queue? ->change to scheduler
    //check status of currThread and next priority thread

    //this is for FIFO, may need to adjust for STCF
    threadNode* currThread = tQueue->head;
    if(currThread == NULL || 
        currThread->threadControlBlock->threadStatus == READY){
        //main thread, switch to scheduler
        //getcontext(&mainContext);
        printf("SIGALRM Handler: SWAP TO scheduler------\n");
        if(swapcontext(&mainContext, &schedulerContext) == -1){
            //error swapping context
            perror("Swap Context Error between main and scheduler\n");
            exit(EXIT_FAILURE);
        }
        return;
        
    }
    else{
        //current Thread was running/done

        //save current thread state?
        if(currThread->threadControlBlock->threadStatus == RUNNING){
            //update elapsed quantums here?
            //check logic here
        
            currThread->threadControlBlock->elapsedQuantums++;
            
        }

        printf("SWAP TO THREAD FROM SCHEDULER------\n");
        if(swapcontext(&currThread->threadControlBlock->threadContext, 
                        &schedulerContext) == -1){
            //error swapping current thread with scheduler
            perror("Swap context between thread and scheduler failed\n");
            exit(EXIT_FAILURE);
        }
        return;
        
    }
    printf("END OF SIGALARM HANDLER\n");
    //schedule();

}

/*
    Helper function that frees all malloc'd memory of a threadNode. Used to cut down redundancy in code.
*/
void freeThreadNode(threadNode* deleteNode){
    
    free(deleteNode->threadControlBlock->threadContext.uc_stack.ss_sp);
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
    if(tempQueue->head == NULL || tempQueue->head == 0){
        printf("Queue is empty\n");
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
    printf("LIST DONE\n");
}

/*
    Locates a specific thread given a threadID.
*/
struct threadNode* getThreadNode(int threadID){
    ignoreSIGALRM = 1;
    threadNode* ptr = tQueue->head;

    printThreadQueue(tQueue);

    while(ptr != NULL){
        if((int) ptr->threadControlBlock->threadID == threadID){
            //match found
            return ptr;
        }
    }
    //not found
    printf("getThreadNode: why are you here?\n");
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
            if(ptr == tQueue->head){
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
    printf("no match found\n");
    return;
}

