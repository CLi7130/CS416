// Student name: Prerak Patel
// Ilab machine used: kill.cs.rutgers.edu

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


void floating_point_exception_handler(int signum) {

	printf("I am slain!\n");
	void *address = &signum;

	address += 0xCC;
	*(int *)address += 0x3;

	/* Do your tricks here*/
	
	
}

int main() {

	int x = 5, y = 0, z = 0;

	signal(SIGFPE, floating_point_exception_handler);

	z = x / y;
	
	printf("I live again!\n");

	return 0;
}
