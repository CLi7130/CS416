#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#define COUNTER_VALUE (1UL << 24)


int counter = 0;
pthread_mutex_t global_lock;
void *countFunct(){
        pthread_mutex_lock(&global_lock);
	for(int i=0; i < COUNTER_VALUE; i++){
		counter++;
	}
	pthread_mutex_unlock(&global_lock);
	pthread_exit(0);
}


int main(int argc, char** argv) {
	int Threads = 0;
	if(argc != 2){
		perror("Need Arguement for Number of Threads");
		exit(0);
	}
	else{
		if ((Threads = atoi(argv[1])) == 0){
			perror("Need an integer");
			exit(0);
		}
	}	
	
	struct timeval tv;
    	struct timezone tz;
	gettimeofday(&tv, &tz);
	int timeStartM = tv.tv_usec;
	int timeStartS = tv.tv_sec;

	pthread_t *tids;
	tids = malloc(Threads * sizeof(pthread_t));

	int err = 0;
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

	printf("Counter finish in %f ms\nThe value of counter should be %d\nThe value of counter is %d\n", timeDiffMS, correctCounter, counter);
	
	return 0;
}
