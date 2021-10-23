#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../mypthread.h"

/* A scratch program template on which to call and
 * test mypthread library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */

void *thread(void *arg) {
    /*
    int i = 0;
    while(1){
        printf("%d. hello from %d\n", i, (int) arg);
        //sleep(100000);
        i++;
        if(i >= 200){
            break;
        }
    }*/
    printf("\n~~~~~~~~~~~~~~~~IN THE THREAD FUNCTION~~~~~~~~~~~~~~~~~~~~~\n");
    printf(" hello from %d\n", (int) arg);
    //spin until interrupt
   //for(int i = 0; i < 5000; i++){ //i, arg);
   // }

  
}

void timer_handler (int signum){

 static int count = 0;
 printf ("timer expired %d times\n", ++count);

}


int main(int argc, char **argv) {

    mypthread_t* tids;
    void *ret;
    int numThreads = 1;
    int err = 0;

    tids = malloc(numThreads * sizeof(mypthread_t));

    for(int i = 0; i < numThreads; i++){
        err = mypthread_create(&tids[i], NULL, thread, i);
        //while(1);
        printf("thread %d created\n", i);
        if(err != 0){
            perror("pthread_create\n");
        }
    }
    
    for(int i = 0; i < 50000; i++){
        printf("%d. ------------Sleeping In the main function;--------\n", i);
    }

    for(int i = 0; i < numThreads; i++){
        printf("~~~~~~~~~~~~~~~~~MAIN: IN PTHREAD_JOIN~~~~~~~~~~~~~~~~~~\n");
        printf("thread %d joined\n", i);
        mypthread_join(tids[i], NULL);
    }
    
    printf("thread exited with\n");
    free(tids);
	return 0;
    
}
