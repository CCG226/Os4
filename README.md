# Os3
compile: oss will takes in 4 arguments: -n for the amount n of workers you want to launch -s for the maximum amount of workers the os is allowed to launch at once -t for the max random time value (seconds) workers can work for. oss will randomly select a value between (1 - t) for worker to run for. -f for the name of the log file to use for logging. -h OPTIONAL lists help info

run: ./oss -n 1 -s 1 -t 7 -f test.txt
compile: make all
clean: make clean

unique: -Os Clock is a struct type called Sys_Time made up of three members to make it easier to store and access clock info in shared memory Sys_Time members: rate: the speed the system clock will 'tick'/'iterate' at in nanoseconds (default is 5000) set in StartSystemClock method seconds: contains the current time of the system clock in seconds nanoseconds: contains the current time of the system clock in nanoseconds

-for the 60 second timer, I used alarm function from signal.h that allows you to genertae a signal (SIGALRM) after 60 seconds in which the program will call a method that will kill the program. this behavior is handled functions: Begin_OS_LifeCycle() and End_OS_LifeCycle()

-when a worker is done, occupied value in the table is set to 0
