// Student name: Craig Li
// Ilab machine used: rm.cs.rutgers.edu

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void floating_point_exception_handler(int signum) {
    
	printf("I am slain!\n");

	/* Do your tricks here */

    char* stackPointer = ((char*) &signum) + 0xCC;
    *stackPointer += 0x3;

    /*
        can also be done using an int* via:
        int* stackPointer = &signum + (0xCC / 4);
        *stackPointer += 0x3;
    */

}

int main() {

	int x = 5, y = 0, z = 0;

	signal(SIGFPE, floating_point_exception_handler);

	z = x / y;
	
	printf("I live again!\n");

	return 0;
}