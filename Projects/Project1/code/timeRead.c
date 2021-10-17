#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#define FILE "timeRead.txt"


// Place any necessary global variables here

int main(int argc, char *argv[]){
	
	//create the 512MB file
	/**	
	int fd = open("timeRead.txt", O_RDWR);
	for(int j = 0; j < 512000; j++){
		for(int i = 0; i < 1024; i++){
			write(fd, "I", 1);
		}	
	}
	close(fd);
	 */
	
	int fd = open(FILE, O_RDWR);
	int count = 0;
	char string[4096];
	struct timeval tv;
        struct timezone tz;
	gettimeofday(&tv, &tz);
        int timeStartM = tv.tv_usec;
        int timeStartS = tv.tv_sec;
	while(read(fd, string, 4096) != NULL){
		count++;
	}
	gettimeofday(&tv, &tz);
        int timeEndM = tv.tv_usec;
        int timeEndS = tv.tv_sec;
        int timeDiffS = timeEndS - timeStartS;
        int timeDiffM = timeEndM - timeStartM;
	timeDiffM = timeDiffM + (timeDiffS * 1000000);
	printf("Syscalls Performed: %d\nTotal Elapsed Time: %d microseconds\nAverage Time Per Syscall: %f microseconds\n", count, timeDiffM, (double) timeDiffM/count);
	close(fd);
	
	return 0;

}







