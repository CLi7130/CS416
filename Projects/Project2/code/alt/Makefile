CC = gcc
CFLAGS = -g -c
AR = ar -rc
RANLIB = ranlib

SCHED = PSJF

all: queue.o mypthread.a

queue.o:
	$(CC) $(CFLAGS) queue.c -o queue.o

mypthread.a: altpthread.o
	$(AR) libmypthread.a altpthread.o queue.o
	$(RANLIB) libmypthread.a

mypthread.o: altpthread.h

ifeq ($(SCHED), PSJF)
	$(CC) -pthread $(CFLAGS) altpthread.c
else ifeq ($(SCHED), MLFQ)
	$(CC) -pthread $(CFLAGS) -DMLFQ altpthread.c
else
	echo "no such scheduling algorithm"
endif

clean:
	rm -rf testfile *.o *.a