// Student name: Craig Li
// Ilab machine used: rm.cs.rutgers.edu

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h> //gettimeofday()
#include <unistd.h> //read()
#include <fcntl.h>

unsigned long totalSyscalls = 0;
unsigned long totalTimeElapsed = 0;
double avgTimePerSyscall = 0;
int bufferSize = 4000; //buffer of 4KB

int main(int argc, char *argv[]){

    /*
        testFile is a 512MB file created via the following terminal command:
        
        truncate -s 512MB testFile
    */
    int fd = open("testFile", O_RDONLY);
    if(fd < 0){
        perror("open error\n");
        abort();
    }

    struct timeval start, end;
    char *c = malloc(sizeof(char) * bufferSize);
    int sizeRead;

    do{
        gettimeofday(&start, NULL); //start timer

        sizeRead = read(fd, c, bufferSize); //read 4KB from file

        gettimeofday(&end, NULL); //end timer
        if(sizeRead == 0){
            break;
        }
        long syscallTime_seconds = (end.tv_sec - start.tv_sec);
        long syscallTime_microsec = ((syscallTime_seconds * 1000000) 
                                    + end.tv_usec) - (start.tv_usec);

        totalSyscalls++;
        totalTimeElapsed += syscallTime_microsec;
        
    }while(sizeRead > 0);
    
    avgTimePerSyscall = (double) totalTimeElapsed / totalSyscalls;

    printf("Syscalls Performed: %lu\n", totalSyscalls);
    printf("Total Elapsed Time: %lu microseconds \n", totalTimeElapsed);
    printf("Average Time Per Syscall: %lf micoseconds\n", avgTimePerSyscall);

    close(fd);
	return 0;

}
