struct Sys_Time {
int seconds;
int nanoseconds;
int rate;

};
typedef struct msgbuffer {
long mtype;
int timeslice;
double eventWaitTime;
} msgbuffer;
//work printing status task done here
void TaskHandler();

int SelectTask();

int TaskRunAndGetExternalResource(int timeslice);

int TaskRunAndTerminate(int timeslice);

int TaskRun(int timeslice);

double GenerateEventWaitTime();
//accesses shared memory resource for system clock
struct Sys_Time* AccessSystemTime();

//detaches worker from shared memory system clock resource after terminating 
void DisposeAccessToShm(struct Sys_Time* clock);

//accesses message queue create by os to communicate to os
int AccessMsgQueue();

//worker waits for os to send request message via msgrcv so that worker can check to if termination time has passed and thus can terminate 
int AwaitOsRequestForPermissionsToRunTask(int msqid, msgbuffer *msg);

//if worker must terminate it sends a 0 back to os, else its still working and sends a 1 back to os
void SendResponseMsg(int msqid, msgbuffer *msg, int timeRan, int eventWaitTime);

const int RUNNING = 1;
const int TERMINATING = 0;

const int MSG_SYSTEM_KEY = 5303;
const int SYS_TIME_SHARED_MEMORY_KEY = 63131;
