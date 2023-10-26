#include <sys/types.h>
#include <stdio.h>
//represents shared memory Operating system clock
struct Sys_Time {
int seconds;//current second
int nanoseconds; //current nanosecond
int rate;//increment rate
};
//process table
struct PCB {
	int state;//is or is not working (1/0)
	pid_t pid; //worker process id
	int startSeconds;//second launched
	int startNano; //nanosecond launched
	int serviceTimeSeconds;//time process ran seconds
	int serviceTimeNano;//time process ran  nanoseconds
	int eventWaitSec;//time to wait for event in sec
	int eventWaitNano;//time to wait for event in nano
};
//message data sent through message queue
typedef struct msgbuffer {
long mtype;
int timeslice; //time process gets to run
double eventWaitTime; // time a process must wait in blocked queue if it needs to access external resource
} msgbuffer;

//help information
void Help();

//parses arguments to get -n value (workerAmount), -s value (workerSimLimit), -t value for (timeInterval), -f value for logfile name 
//workerAmount = amount of workers os will launch in total
//workerSimLimit = amount of workers allowed to run on os at once
//timeInterval = every timeInterval maount of time passes in the system clock a new porcess is to be launched if possible
//fileName = file information will be logged too
void ArgumentParser(int argc, char** argv, int* workerAmount,int* workerSimLimit, int* timeInterval,char** fileName);

//validates command line arguments input and shuts down software if argument is invalid
int ValidateInput(int workerAmount, int workerSimLimit, int timeInterval, char* fileName);

//sets up shared memory location and ties the address to are os's system clock
int StartSystemClock(struct Sys_Time **Clock);

//detaches shared memory from systemc clock and removes shared memory location
void StopSystemClock(struct Sys_Time *Clock, int sharedMemoryId);

//increments clock based on incRate
void RunSystemClock(struct Sys_Time *Clock, int incRate);

//handles how many workers are laucnhed,if its time to launch a new worker as well as if its ok too
//runs the system clock
//prints process table and handles logger
//calls scheduler to schedule a process from ready queue to run  and WakeUpProcess to see if a blocked worker doesnt have to be blocked anymore
//updates process table info
//handles response messages from workers after they run
//workAmount = n value, workerSimLimit = -s value, timeInterval = -t value, logFile = -f value, OsClock = shared memory clock, table is process pcb table 
void WorkerHandler(int workerAmount, int workerSimLimit, int timeInterval,char* logFile, struct Sys_Time* OsClock, struct PCB table[]);

//logs a particular message to logfile 
int LogMessage(FILE* logger, const char* format,...);

// await to see if a worker is done
//return 0 if no workers done
//returns id of worker done if a worker is done
int AwaitWorker(pid_t worker_Id);

//launches worker processes
//amount launched at once is based on simLimit, adds workers id and state to ready as well as start clock time to process table post launch, 
void WorkerLauncher(int amount, struct PCB table[], struct Sys_Time* clock, FILE* logger);
//starts alarm clock for 3 seconds
void Begin_OS_LifeCycle();
//kills os after 3 second event signal is triggered
void End_OS_LifeCycle();
//adds worker data to process table  after worker is launched including its pid and time it was launchhed and the fact that is state is ready so its ready queue
void AddWorkerToProcessTable(struct PCB table[], pid_t workerId, int secondsCreated, int nanosecondsCreated);
//updates state of a worker
void UpdateWorkerStateInProcessTable(struct PCB table[], pid_t workerId, int state);
//gets the index value of a particular woker from the process table using workers pid
int GetWorkerIndexFromProcessTable(struct PCB table[], pid_t workerId);
//prints process table
void PrintProcessTable(struct PCB processTable[],int curTimeSeconds, int curTimeNanoseconds);
//constructor for process table
void BuildProcessTable(struct PCB table[]);
//returns 1 if 1/2 second has passed, else it return 0
int HasHalfSecPassed(int currentSec, int currentNano, int halfSecMark, int halfNanoMark);

//creates message queue and returns message queues id
int ConstructMsgQueue();
//destroys message queue via id 
void DestructMsgQueue(int msqid);
//sends request to specific worker using workers pid via message queue
//the request contains information about the workers timeslice, how much time it gets to run on cpu
//recieves a response containing the amount of time it got to run on the cpu and if it was blocked how much time it must wait for an event
msgbuffer SendAndRecieveScheduleMsg(int msqid, pid_t worker_id);

//calculates the ratios of all workers in the process table. ratio formula: amount_Of_time_worker_serviced_on_cpu/amount_of_time_process_has_been_in_the_system
void RatioCompiler(struct PCB table[], double ratios[], struct Sys_Time* Clock);
//decides which work to schedule next by picking a worker that has a ready state and has the lowest ratio
int ReadyWorkerScheduler(struct PCB table[], struct Sys_Time* Clock, FILE* logger);
//get data after decimal point. ex: (4.2) => (0.2)
double getDataAfterDecimal(double data);
//stores the amount of time a worker must remain blocked based on event time amount it sent to os as response msg
void StoreWorkerEventWaitTime(struct PCB table[], pid_t workerId, double eventTime, struct Sys_Time* clock);
//stores amount of time worker has got to run on the system after it finishes working. timeWorkingNano is timeslice worker sent back to os in response msg after working
void StoreWorkerServiceTime(struct PCB table[], pid_t workerId, int timeWorkingNano);
//determunes if time interval has passed (time interval -t) , if it has we might be able to launch another worker
int CanLaunchWorker(int currentSecond,int currentNano,int LaunchTimeSec,int LaunchTimeNano);
//calculates the time to a particular event by taking in current clock time as well as how long until a event occurs, and two values passed by ref pointers (eventSec/eventNano) that store the time when the next event occurs.
//this method is used to calculate when a workers event time will finish, when a 1/2 second passes to print table, and if time interval t time has passed and wer can launch a new worker
void GenerateTimeToEvent(int currentSecond,int currentNano,int timeIntervalNano,int timeIntervalSec, int* eventSec, int* eventNano);
//wakes up workers in blocked queue when thier event time has passed
//returns amount of workers woken up from blocked queue
int WakeUpProcess(struct PCB table[], struct Sys_Time* Clock, FILE* logger);
//after a process terminates, this method calculates how much time the worker was NOT running
//math: Rime Worker In System - Time Worker On CPU
//returns amount of time spent waiting in total fo a singular process
double TimeProcessWaited(struct Sys_Time* clock, int id, struct PCB table[]);
//calculates total time spent running a process
double GetTimeSpentWorking(struct PCB table[]);
//report on avg wait time, cpu utlization, avg time in blocked queue, cpu idle time
void Report(double totalWaitTimeOfAllWorkers,double totalTimeSpentBlocked, int amountBlocked, int workerAmount, struct PCB table[], struct Sys_Time* clock);
//calcualtes avg cpu utilixation 
double AvgCpuUtilization(double timeRunningProccesses, double totalSysTime);
//calculates time spent idle (nothing running on cpu)
double IdleTime(double totalSysTime, double timeRunningProccesses);
//calcualtes average time of all proccesses waiting in blocked queue
double AvgWaitTimeWhileBlocked(double totalTimeBlocked,int amountBlocked);
//calcualtes avg time spent waiting to run
double AvgWaitTimeToRun(double totalWaitTimeInReady,int amountOfWorkers);
//worker state blocked
const int STATE_BLOCKED = 3;
//worker state ready
const int STATE_READY = 2;
//worker state running
const int STATE_RUNNING = 1;
//worker state terminated
const int STATE_TERMINATED = 0;
//table size of process table
const int TABLE_SIZE = 20;
//time statics 
const int HALF_SEC = 500000000;

const int MAX_NANOSECOND = 1000000000;

const int SCHEDULE_TIME = 50000000;
//shrd memory and msg queue keys

const int MSG_SYSTEM_KEY = 5303;

const int SYS_TIME_SHARED_MEMORY_KEY = 63131;
