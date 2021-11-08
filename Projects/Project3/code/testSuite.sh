#! /bin/bash

# test suite for counter files, tests each program with
# certain numbers of threads, it assumes you are in the
# Projects/Project3/code directory
# can be run by using the following command:
#
# ./testSuite.sh

# this can probably be implemented more efficiently, but I have no idea 
# how bash scripting works
# tip: can use ": '" and "'" to open and close multiline bash comments

# test counter programs with the following numbers of threads
testSuite=(1 2 4 8 16 32 64 128 500 1000 5000)

clear
echo "-------------------------------------------------------------"
echo "----------------TEST SUITE FOR COUNTER PROGRAMS--------------"
echo "-------------------------------------------------------------"
echo
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "RUNNING PROGRAMS WITH LARGER AMOUNTS OF THREADS MAY RESULT IN LONGER RUNTIMES"
echo
echo "RUNTIMES ARE ILAB SERVER DEPENDENT, AND WILL DIFFER ON EACH RUN"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

make clean
make

echo
echo "-----------------------NAIVE COUNTER---------------------------"
echo

for i in ${testSuite[@]}; do
    echo "./naive_counter" ${i}
    ./naive_counter $i
    echo
done

echo "-----------------------NAIVE COUNTER PLUS---------------------"
echo

for i in ${testSuite[@]}; do
    echo "./naive_counter_plus" ${i}
    ./naive_counter_plus $i
    echo
done

echo "-----------------------ATOMIC COUNTER---------------------"
echo

for i in ${testSuite[@]}; do
    echo "./atomic_counter" ${i}
    ./atomic_counter $i
    echo
done


echo "-----------------------SCALABLE COUNTER---------------------"
echo

for i in ${testSuite[@]}; do
    echo "./scalable_counter" ${i}
    ./scalable_counter $i
    echo
done

