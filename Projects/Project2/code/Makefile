CC = gcc
CFLAGS = -g -c
AR = ar -rc
RANLIB = ranlib

SCHED = PSJF
DEBUG = 0

all: mypthread.a

mypthread.a: mypthread.o
	$(AR) libmypthread.a mypthread.o
	$(RANLIB) libmypthread.a

mypthread.o: mypthread.h

ifeq ($(SCHED), PSJF)
	$(CC) -pthread $(CFLAGS) mypthread.c
else ifeq ($(SCHED), MLFQ)
	$(CC) -pthread $(CFLAGS) -DMLFQ mypthread.c
else
	echo "no such scheduling algorithm"
endif

clean:
	rm -rf testfile *.o *.a

test:
	make clean
	make
	cd benchmarks && \
	make clean && \
	make && \
	./genRecord.sh

parallel:
	make clean
	make
	cd benchmarks && \
	make clean && \
	make && \
	./parallel_cal 3

testSuiteParallel:
	make clean
	make
	cd benchmarks && \
	make clean && \
	./genRecord.sh && \
	make && \
	./parallel_cal 1 && \
	./parallel_cal 2 && \
	./parallel_cal 5 && \
	./parallel_cal 10 && \
	./parallel_cal 25 && \
	./parallel_cal 50 && \
	./parallel_cal 75 && \
	./parallel_cal 100 && \

testSuiteExternal:
	make clean
	make
	cd benchmarks && \
	make clean && \
	./genRecord.sh && \
	make && \
	./external_cal 1 && \
	./external_cal 2 && \
	./external_cal 5 && \
	./external_cal 10 && \
	./external_cal 25 && \
	./external_cal 50 && \
	./external_cal 75 && \
	./external_cal 100 && \

testSuiteVector:
	make clean
	make
	cd benchmarks && \
	make clean && \
	./genRecord.sh && \
	make && \
	./vector_multiply 1 && \
	./vector_multiply 2 && \
	./vector_multiply 5 && \
	./vector_multiply 10 && \
	./vector_multiply 25 && \
	./vector_multiply 50 && \
	./vector_multiply 75 && \
	./vector_multiply 100 && \