#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#define COUNTER_VALUE (1UL << 24)


unsigned long global_counter = 0;
pthread_mutex_t global_lock;

/**
 *  Uses a counter with mutexes to increment a counter COUNTER_VALUE times.
*/
void *countFunct(){

	for(int i=0; i < COUNTER_VALUE; i++){
        pthread_mutex_lock(&global_lock);
		global_counter++;
        pthread_mutex_unlock(&global_lock);
	}
	
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
	unsigned long correctCounter = Threads * COUNTER_VALUE;
	free(tids);

	printf("Counter finish in %f ms\n"
            "The value of counter should be %ld\n"
            "The value of counter is %ld\n", 
            timeDiffMS, correctCounter, global_counter);
	
	return 0;
}
