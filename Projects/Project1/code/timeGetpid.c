#include <stdlib.h>
#include <stdio.h>
#define RUNS 1000000
#include <sys/time.h>
#include<unistd.h>

// Place any necessary global variables here

int main(int argc, char *argv[]){

	/*
	 *
	 * Implement Here
	 *
	 *
	 */
	struct timeval tv;
    	struct timezone tz;
	gettimeofday(&tv, &tz);
	int timeStartM = tv.tv_usec;
	int timeStartS = tv.tv_sec;
	for(int i = 0; i < RUNS; i++){
		getpid();
	}
	gettimeofday(&tv, &tz);
	int timeEndM = tv.tv_usec;
	int timeEndS = tv.tv_sec;
	int timeDiffS = timeEndS - timeStartS;
	int timeDiffM = timeEndM - timeStartM;
	//printf("timeStartM = %d\ntimeStartS = %d\ntimeEndM = %d\ntimeEndS = %d\ntimeDiffS = %d\ntimeDiffM = %d\n", timeStartM, timeStartS, timeEndM, timeEndS, timeDiffS, timeDiffM);
	timeDiffM = timeDiffM + (timeDiffS * 1000000);
	printf("Syscalls Performed: %d\nTotal Elapsed time: %d microseconds\nAverage Time Per Syscall: %f microseconds\n", RUNS, timeDiffM, (double) timeDiffM/RUNS);
	return 0;

}
