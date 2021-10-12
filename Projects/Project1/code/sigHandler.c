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
    
        int* stackPointer = &signum + (0xCC / 0x4);
        *stackPointer += 0x3;

        An additional note, modifying the program counter value by 0x3 causes the local variable values at the end of the function to change to:
            x = 5
            y = 0
            z = 5
        due to the fact that the exact command causing the sigFPE is the idivl command (3 bytes in length), but that idivl command is part of the division as a whole, which involves multiple assembly commands of moving values around through registers (7 bytes in length). By skipping only the idivl command, but not the entire division as a whole, we move the value of %eax (which at this point is the same value as x, located at -0xc(%rbp) into -0x4(%rbp), which is where the variable z is stored, thus changing the value of z to 5.  
        
        The assignment writeup is unclear as to whether this matters.  However, if we were to modify the change to the program counter to 0x6 instead of 0x3, we skip the move of x into z, leaving the variables unchanged when we exit the program.

        So, the exact length of the bad instruction is 0x3, but to leave all variables unchanged, we would have to increment by 0x6.
    
    */
}

int main() {

	int x = 5, y = 0, z = 0;

	signal(SIGFPE, floating_point_exception_handler);

	z = x / y;
	
	printf("I live again!\n");

	return 0;
}