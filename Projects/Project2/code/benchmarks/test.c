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
  
  for(int i = 0; i < 5; i++){
      printf("%d. hello from %d\n", i, (int) arg);
  }
  
}


int main(int argc, char **argv) {
    mypthread_t* tids;
    void *ret;
    int numThreads = 4;
    int err = 0;

    tids = malloc(numThreads * sizeof(mypthread_t));
    /*
    if (mypthread_create(&thid, NULL, thread, "thread 1") != 0) {
        perror("pthread_create() error");
        exit(1);
    }

    if (mypthread_join(thid, &ret) != 0) {
        perror("pthread_create() error");
        exit(3);
    }*/

    for(int i = 0; i < numThreads; i++){
        err = mypthread_create(&tids[i], NULL, thread, i);
        printf("thread %d created\n", i);
        if(err != 0){
            perror("pthread_create\n");
        }
    }
    
    for(int i = 0; i < numThreads; i++){
        mypthread_join(tids[i], NULL);
        printf("thread %d joined\n", i);
    }
    
    printf("thread exited with\n");
    free(tids);
	return 0;
}
