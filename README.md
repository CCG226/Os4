# Os4
compile: oss will takes in 4 arguments: 
-n for the amount n of workers you want to launch 
-s for the maximum amount of workers the os is allowed to launch at once 
-t is the time interval to launch next worker if allowed to.
-f for the name of the log file to use for logging.
-h OPTIONAL lists help info

run: ./oss -n 1 -s 1 -t 7 -f test.txt
compile: make all
clean: make clean

unique: -Os Clock is a struct type called Sys_Time made up of three members to make it easier to store and access clock info in shared memory Sys_Time members: 
rate: the speed the system clock will 'tick'/'iterate' at 'x'  nanoseconds (the rate 'x' changes based on how long it takes os to do a task). 
seconds: contains the current time of the system clock in seconds nanoseconds: 
contains the current time of the system clock in nanoseconds

-for the 60 second timer, I used alarm function from signal.h that allows you to genertae a signal (SIGALRM) after 60 seconds in which the program will call a method that will kill the program. this behavior is handled functions: Begin_OS_LifeCycle() and End_OS_LifeCycle()

-process Table:
-replaced occupied and blocked fields with singular field called state that is used to determine whether a worker is blocked, ready, running, or terminated/DNE
possible values for state:

-3: STATE_BLOCKED = meaning the worker picked the task to wait for event and has been put in blocked queue and must wait in the blocked queue until the os wakes it up and puts it in the ready queue when its eventWait time is passed

-2: STATE_READY = worker has been launched and is waiting for the os to schedule it by sending a message that it can run

-1: STATE_RUNNNING = worker has been scheduled to run by os for a specific timeslcie

-0: STATE_TERMINATED = the worker has finished its task and notifies os that is will terminate OR this process table slot is empty (DNE)

