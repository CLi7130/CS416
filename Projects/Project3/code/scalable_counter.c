#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#define COUNTER_VALUE (1UL << 24)
#define INCREMENTBY 50
#define local_threshold 1000

#ifndef NUM_THREADS
    #define NUM_THREADS 2
#endif

int counter = 0;
pthread_mutex_t global_lock;
pthread_mutex_t local_lock;

typedef struct __counter_t {
    int global; //global count
    pthread_mutex_t global_lock; //global lock
    int local[NUM_THREADS];
    pthread_mutex_t local_lock[NUM_THREADS];
    int threshold;

} counter_t;

/**
 * record threshold, init locks, init values of all local counts and global
 * count.
*/
void init(counter_t *c, int threshold){
    c->threshold = threshold;
    c->global = 0;
    pthread_mutex_init(&c->global_lock, NULL);
    
    for(int i = 0; i < NUM_THREADS; i++){
        c->local[i] = 0;
        pthread_mutex_init(&c->local_lock[i], NULL);
    }
}

/**
 * Grab local lock and update local amount, up until threshold is reached,
 * then grab global lock and transfer local values to it, resetting local 
 * counter.
*/
void update(counter_t *c, int threadID, int amount){
    int cpu = threadID % NUM_THREADS;
    pthread_mutex_lock(&c->local_lock[cpu]);
    c->local[cpu] += amount;

    if(c->local[cpu] >= c->threshold){
        //transfer amount in local cpu to threshold
        pthread_mutex_lock(&c->global_lock);
        c->global += c->local[cpu];
        pthread_mutex_unlock(&c->global_lock);
        c->local[cpu] = 0;
    }
}


void *countFunct(){

    pthread_mutex_lock(&global_lock);
	for(int i=0; i < COUNTER_VALUE; i+=INCREMENTBY){
		counter += INCREMENTBY;
	}
	pthread_mutex_unlock(&global_lock);
	pthread_exit(0);
}


int main(int argc, char** argv) {
	int Threads = 0;
	if(argc != 2){
		perror("Need Argument for Number of Threads");
		exit(EXIT_FAILURE);
	}
	else{
		if ((Threads = atoi(argv[1])) == 0){
			perror("Need an integer");
			exit(EXIT_FAILURE);
		}
	}	
	
	pthread_t *tids;
	tids = malloc(Threads * sizeof(pthread_t));

	int err = 0;	

	struct timeval tv;
    struct timezone tz;
	gettimeofday(&tv, &tz);
	int timeStartM = tv.tv_usec;
	int timeStartS = tv.tv_sec;

	pthread_mutex_init(&global_lock, NULL);
	for (int i = 0; i < Threads; i++){
		err = pthread_create(&tids[i], NULL, countFunct, NULL);
		if(err != 0){
			perror("pthread_create");
		}
	}
	
    for(int i = 0; i < Threads; i++){
        pthread_join(tids[i], NULL);
    }

	gettimeofday(&tv, &tz);
	int timeEndM = tv.tv_usec;
	int timeEndS = tv.tv_sec;

	int timeDiffS = timeEndS - timeStartS;
	int timeDiffM = timeEndM - timeStartM;
	timeDiffM = timeDiffM + (timeDiffS * 1000000);
	double timeDiffMS = (double) timeDiffM/1000;
	int correctCounter = Threads * COUNTER_VALUE;
	free(tids);

	printf("Counter finish in %f ms\n"
            "The value of counter should be %d\n"
            "The value of counter is %d\n",
            timeDiffMS, correctCounter, counter);
	
	return 0;
}
