struct Sys_Time {
int seconds;
int nanoseconds;
int rate;

};
typedef struct msgbuffer {
long mtype;
int timeslice;//amount of time worker gets to run
double eventWaitTime;//amount of time wokrer must wait for external resoruce
} msgbuffer;
//sets up worker behaviour
//which is to wait for os to send request to run with timeslice, then call SelectTask()
//after a task has been picked (run, terminate, access external resource) send a response to the os with details on its task
void TaskHandler();
//randomaly pick a task to do (run a bit and terminate, run entire timeslice, run a bit and access external resource )
int SelectTask();
//if task invloves being interrupted by I/O
////pick a random percent to represent percent of time slice ran
int TaskRunAndGetExternalResource(int timeslice);
//if task is to run and terminate
//return random value 0 - timeslice_value as amount of time ran before terminating and return that value
int TaskRunAndTerminate(int timeslice);
//if we are tun run entrie timeslice, simply return timeslice
int TaskRun(int timeslice);
//if worker blocked by external resource it must wait for
//generate random time worker must 'wait' for as double between 0 - 5 seconds +  0 - 1000 miliseconds 
double GenerateEventWaitTime();
//accesses shared memory resource for system clock
struct Sys_Time* AccessSystemTime();

//detaches worker from shared memory system clock resource after terminating 
void DisposeAccessToShm(struct Sys_Time* clock);

//accesses message queue create by os to communicate to os
int AccessMsgQueue();

//worker waits for os to send request message with timeslice value to indicate worker can for timeslcie amount of time
int AwaitOsRequestForPermissionsToRunTask(int msqid, msgbuffer *msg);

//send resposne to os indicating how much time it ran and the type of task it did
void SendResponseMsg(int msqid, msgbuffer *msg, int timeRan, double eventWaitTime);
//if worker is in operation
const int RUNNING = 1;
//if worker is terminating and ending operations
const int TERMINATING = 0;

const int MSG_SYSTEM_KEY = 5303;
const int SYS_TIME_SHARED_MEMORY_KEY = 63131;
