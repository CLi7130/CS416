#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdatomic.h>

#define COUNTER_VALUE (1UL << 24)
#define LOCAL_THRESHOLD 1024

atomic_int_fast64_t global_counter = 0;
pthread_mutex_t global_lock;

/**
 * Implementation of an approximate/scalable counter, uses local counters to 
 * cut down on concurrency overhead (global counter is accessed less overall).
 * Local Counter is incremented until a threshold is hit, at which point it
 * pushes its value into the global counter.  This is Thread safe, but we are 
 * trading accuracy for time performance (final result may not be correct)
 */
void *countFunct(){
    int local_counter = 0;

    for(int i = 0; i < COUNTER_VALUE; i++){
        //increment local thread's counter
        local_counter++;

        //dump value of local counter into global once we hit
        //local threshold
        if(local_counter >= LOCAL_THRESHOLD){
            atomic_fetch_add(&global_counter, local_counter);

            //reset local_counter to 0 once we've hit threshold
            local_counter = 0;
        }
        
    }
    //add any remaining leftovers in local_counter, this removes error 
    //from approximate counter
    if(local_counter > 0){
        atomic_fetch_add(&global_counter, local_counter);
    }

}


int main(int argc, char** argv) {
	int Threads = 0;
	if(argc != 2){
		perror("Need Argument for Number of Threads");
		exit(EXIT_FAILURE);
	}
	else{
		if((Threads = atoi(argv[1])) == 0){
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
            exit(EXIT_FAILURE);
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
	unsigned long correctCounter = Threads * COUNTER_VALUE;
	free(tids);

	printf("Counter finish in %f ms\n"
            "The value of counter should be %ld\n"
            "The value of counter is %ld\n", 
            timeDiffMS, correctCounter, global_counter);
	
	return 0;
}
