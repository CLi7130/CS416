// Student name: Craig Li
// Ilab machine used: rm.cs.rutgers.edu

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h> //gettimeofday()
#include <sys/types.h>
#include <unistd.h>

unsigned long totalSyscalls = 100000; 
//arbitary high number of syscalls as per piazza
unsigned long totalTimeElapsed = 0;
double avgTimePerSyscall = 0;

int main(int argc, char *argv[]){

    struct timeval start, end;

    for(int i = 0; i < totalSyscalls; i++){
        gettimeofday(&start, NULL); //start timer

        getpid();

        gettimeofday(&end, NULL); //end timer

        //convert to microseconds
        long syscallTime_seconds = (end.tv_sec - start.tv_sec);
        long syscallTime_microsec = ((syscallTime_seconds * 1000000) 
                                    + end.tv_usec) - (start.tv_usec);

        totalTimeElapsed += syscallTime_microsec;
        
    }
    
    avgTimePerSyscall = (double) totalTimeElapsed / totalSyscalls;

    printf("Syscalls Performed: %lu\n", totalSyscalls);
    printf("Total Elapsed Time: %lu microseconds \n", totalTimeElapsed);
    printf("Average Time Per Syscall: %lf micoseconds\n", avgTimePerSyscall);

	return 0;

}
