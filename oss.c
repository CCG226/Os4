#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <errno.h>
#include "oss.h"
#include <string.h>
//Author: Connor Gilmore
//Purpose: Program task is to handle and launch x amount of worker processes 
//The Os will terminate no matter what after 60 seconds
//the OS operates its own system clock it stores in shared memory to control worker runtimes
//the OS stores worker status information in a process table
//takes arguments: 
//-h for help,
//-n for number of child processes to run
//-s for number of child processes to run simultanously
//-t max range value for random amount of time workers will work for 
//-f for the name of the  log file you want os to log too. (reccommend .txt)
void Begin_OS_LifeCycle()
{
	printf("\n\nBooting Up Operating System...\n\n");	
	//set alarm to trigger a method to kill are program after 60 seconds via signal
	signal(SIGALRM, End_OS_LifeCycle);

	alarm(10);

}

void End_OS_LifeCycle()
{
	//clear shared memmory and message queue as the program failed to reack EOF
	int shm_id = shmget(SYS_TIME_SHARED_MEMORY_KEY, sizeof(struct Sys_Time), 0777);
	shmctl(shm_id, IPC_RMID, NULL);
	int msqid = msgget(MSG_SYSTEM_KEY, 0777);
	msgctl(msqid, IPC_RMID, NULL);
	//kill system
	printf("\n\nOS Has Reached The End Of Its Life Cycle And Has Terminated.\n\n");	
	exit(1);

}	

int main(int argc, char** argv)
{
	srand(time(NULL));

	//set 60 second timer	
	Begin_OS_LifeCycle();

	//process table
	struct PCB processTable[20];

	//assigns default values to processtable elements (constructor)
	BuildProcessTable(processTable);

	//stores shared memory system clock info (seconds passed, nanoseconds passed, clock speed(rate))
	struct Sys_Time *OS_Clock;

	//default values for argument variables are 0
	int workerAmount = 0;//-n proc value (number of workers to launch)
	int simultaneousLimit = 0;//-s simul value (number of workers to run in sync)
	int timeInterval = 0;//-t iter value (number to pass to worker as parameter)
	char* logFileName = NULL;//-f value to store file name of log file


	//stored id to access shared memory for system clock
	int shm_ClockId = -1;

	//pass workerAmount (-n), simultaneousLimit (-s),timeLimit(-t) by reference and assigns coresponding argument values from the command line to them
	ArgumentParser(argc, argv,&workerAmount, &simultaneousLimit, &timeInterval, &logFileName);
	
	//'starts' aka sets up shared memory system clock and assigns default values for clock 
	shm_ClockId = StartSystemClock(&OS_Clock);

	//handles how os laumches, tracks, and times workers
	WorkerHandler(workerAmount, simultaneousLimit, timeInterval,logFileName, OS_Clock, processTable);

	//removes system clock from shared mem
	StopSystemClock(OS_Clock ,shm_ClockId);
	printf("\n\n\n");
	return EXIT_SUCCESS;

}
int ConstructMsgQueue()
{
	int id = -1;
	//ccreate a message queue and return id of queue
	if((id = msgget(MSG_SYSTEM_KEY, 0777 | IPC_CREAT)) == -1)
	{
		printf("Failed To Construct OS's Msg Queue\n");
		exit(1);

	}
	return id;
}
void DestructMsgQueue(int msqid)
{
	//delete message queue from system
	if(msgctl(msqid, IPC_RMID, NULL) == -1)
	{
		printf("Failed To Destroy Message Queue\n");
		exit(1);
	}
}
msgbuffer SendAndRecieveScheduleMsg(int msqid, pid_t workerId) 
{
	msgbuffer msg;
	//send message to specific worker via id, Data sent doesnt matter we should want to notify worker via message to get a status update from them
	msg.mtype = (int) workerId;
	msg.timeslice = SCHEDULE_TIME;
	msg.eventWaitTime = 0;	
	if(msgsnd(msqid, &msg, sizeof(msgbuffer)-sizeof(long),0) == -1)
	{
		printf("Failed To Send Message To Worker %d. \n", workerId);
		exit(1);
	}

	//wait ti recieve message from worker to see if they are done working and need to terminate or not
	if(msgrcv(msqid, &msg, sizeof(msgbuffer), 1, 0) == -1)
	{
		printf("Failed To Receive Message From Worker In Oss.\n");
		fprintf(stderr, "errno: %d\n", errno);

		exit(1);

	}
	return msg;

}
int StartSystemClock(struct Sys_Time **Clock)
{
	//sets up shared memory location for system clock
	int shm_id = shmget(SYS_TIME_SHARED_MEMORY_KEY, sizeof(struct Sys_Time), 0777 | IPC_CREAT);
	if(shm_id == -1)
	{
		printf("Failed To Create Shared Memory Location For OS Clock.\n");

		exit(1);
	}
	//attaches system clock to shared memory
	*Clock = (struct Sys_Time *)shmat(shm_id, NULL, 0);
	if(*Clock == (struct Sys_Time *)(-1)) {
		printf("Failed to connect OS Clock To A Slot In Shared Memory\n");
		exit(1);
	}
	//set default clock values
	//clock speed is 50000 nanoseconds per 'tick'
	(*Clock)->rate = 0;
	(*Clock)->seconds = 0;
	(*Clock)->nanoseconds = 0;

	return shm_id;
}
void StopSystemClock(struct Sys_Time *Clock, int sharedMemoryId)
{ //detach shared memory segement from our system clock
	if(shmdt(Clock) == -1)
	{
		printf("Failed To Release Shared Memory Resources..");
		exit(1);
	}
	//remove shared memory location
	if(shmctl(sharedMemoryId, IPC_RMID, NULL) == -1) {
		printf("Error Occurred When Deleting The OS Shared Memory Segmant");
		exit(1);
	}
}
void RunSystemClock(struct Sys_Time *Clock, int incRate) {

	if(incRate < 0)
	{
	incRate = incRate * -1;
	}

	Clock->rate = incRate;
	//handles clock iteration / ticking
	if((Clock->nanoseconds + Clock->rate) >= MAX_NANOSECOND)
	{
		Clock->nanoseconds = (Clock->nanoseconds + Clock->rate) - MAX_NANOSECOND;	
		Clock->seconds = Clock->seconds + 1;
	}
	else {
	Clock->nanoseconds = Clock->nanoseconds + Clock->rate;
	}
	

}


void ArgumentParser(int argc, char** argv, int* workerAmount, int* workerSimLimit, int* timeInterval, char** fileName) {
	//assigns argument values to workerAmount, simultaneousLimit, workerArg variables in main using ptrs 
	int option;
	//getopt to iterate over CL options
	while((option = getopt(argc, argv, "hn:s:t:f:")) != -1)
	{
		switch(option)
		{//junk values (non-numerics) will default to 0 with atoi which is ok
			case 'h'://if -h
				Help();
				break;
			case 'n'://if -n int_value
				*(workerAmount) = atoi(optarg);
				break;
			case 's'://if -s int_value
				*(workerSimLimit) = atoi(optarg);
				break;
			case 't'://if t int_value
				*(timeInterval) = atoi(optarg);
				break;
			case 'f':
				*(fileName) = optarg;
				break;
			default://give help info
				Help();
				break; 
		}

	}
	//check if arguments are valid 
	int isValid = ValidateInput(*workerAmount, *workerSimLimit, *timeInterval, *fileName);

	if(isValid == 0)
	{//valid arguments 
		return;	
	}
	else
	{
		exit(1);
	}
}
int ValidateInput(int workerAmount, int workerSimLimit, int timeInterval, char* fileName)
{
	//acts a bool	
	int isValid = 0;

	FILE* tstPtr = fopen(fileName, "w");
	//if file fails to open then throw error
	//likely invald extension
	if(tstPtr == NULL)
	{
		printf("\nFailed To Access Input File\n");
		exit(1);
	}
	fclose(tstPtr);
	//arguments cant be negative 
	if(workerAmount < 0 || workerSimLimit < 0 || timeInterval < 0)
	{
		printf("\nInput Arguments Cannot Be Negative Number!\n");	 
		isValid = 1;	
	}
	//args cant be zero
	if(workerAmount < 1)
	{
		printf("\nNo Workers To Launch!\n");
		isValid = 1;
	}
	//no more than 20 workers
	if(workerAmount > 20)
	{
		printf("\nTo many Workers!\n");
		isValid = 1;
	}
	if(workerSimLimit < 1)
	{
		printf("\nWe Need To Be Able To Launch At Least 1 Worker At A Time!\n");
		isValid = 1;
	}
	if(timeInterval > MAX_NANOSECOND)
	{
		printf("\nTime Interval must be less than 1 second!\n");
		isValid = 1;
	}
	return isValid;

}
void RatioCompiler(struct PCB table[], double ratios[], struct Sys_Time* Clock)
{
	for(int i = 0; i < TABLE_SIZE;i++)
	{
	 int timeWorkerInSysSec = Clock->seconds - table[i].startSeconds;
	 int timeWorkerInSysNano = Clock->nanoseconds - table[i].startNano;
	 if(timeWorkerInSysSec == 0 && timeWorkerInSysNano == 0)
	 {
	 ratios[i] = 0;
	 }
	 else
	 {
	 double NanoSecToSecTimeInSys = (double) timeWorkerInSysNano / MAX_NANOSECOND;
	 double NanoSecToSecTimeServiced = (double) table[i].serviceTimeNano / MAX_NANOSECOND;

	 double RealTimeWorkerInSys = timeWorkerInSysSec + NanoSecToSecTimeInSys;
	 double RealTimeWorkerServiced = table[i].serviceTimeSeconds + NanoSecToSecTimeServiced;

	 ratios[i] = RealTimeWorkerServiced / RealTimeWorkerInSys;
	
	 }
	
	}
}
void WakeUpProcess(struct PCB table[], struct Sys_Time* Clock)
{

for(int i = 0; i < TABLE_SIZE;i++)
{
if(table[i].state == STATE_BLOCKED)
{
if(table[i].eventWaitSec <= Clock->seconds && table[i].eventWaitNano <= Clock->nanoseconds)
{
table[i].state = STATE_READY;
table[i].eventWaitNano = 0;
table[i].eventWaitSec = 0;
printf("unblocked: %d\n", table[i].pid);
}

}

}

}
int ReadyWorkerScheduler(struct PCB table[], struct Sys_Time* Clock)
{
	double ratioList[20];
 RatioCompiler(table, ratioList, Clock);
 double LowestRatio = -1;
 int HighestPriorityWorker = 0;

for(int i = 0; i < TABLE_SIZE;i++)
{
if(table[i].state == STATE_READY)
{
if(LowestRatio == -1 || ratioList[i] < LowestRatio)
{
LowestRatio = ratioList[i];
HighestPriorityWorker = table[i].pid;
}

}
}

 return HighestPriorityWorker;


}

void LogMessage(FILE** logger, int msgType, int workerId,...)
{
	//logs status message to logfile

	char buffer[100];	
	if(msgType == 0)
	{//if msgType equals Terminating then output that this work is about to terminate
		sprintf(buffer, "OSS: Worker %d PID %d is planning to terminate.\n", workerIndex, workerId);
	}
	else
	{ //output the fact that oss is sending/recieving a message from a specific worker
		sprintf(buffer, "OSS: %s message to worker %d PID %d at time %d:%d.\n",msgType, workerIndex, workerId, curSec, curNano);
	}

	fputs(buffer, *logger);


}
void WorkerHandler(int workerAmount, int workerSimLimit,int timeInterval, char* logFile, struct Sys_Time* OsClock, struct PCB processTable[])
{	//access logfile
	FILE *logger = fopen(logFile, "w");
	//get id of message queue after creating ti
	int msqid = ConstructMsgQueue();

	//holds pid of next worker to message from os, os will send and recieve a status response from this worker 
	int nxtWorkerToSchedule = 0;
	//tracks amount of workers finished with task
	int workersComplete = 0;

	//tracks amount of workers left to be launched
	int workersLeftToLaunch = workerAmount;


	int workersInSystem = 0;

	RunSystemClock(OsClock, timeInterval);

	int timeToLaunchNxtWorkerSec = OsClock->seconds;
	int timeToLaunchNxtWorkerNano = OsClock->nanoseconds;

	int timeToOutputSec = 0;
	int timeToOutputNano = 0;
	GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds,HALF_SEC,0, &timeToOutputSec, &timeToOutputNano);

	//keep looping until all workers (-n) have finished working
	while(workersComplete != workerAmount)
	{
		
     if(workersLeftToLaunch != 0)
     {
	if(workersInSystem <= workerSimLimit)
	{	
     	     if(CanLaunchWorker(OsClock->seconds, OsClock->nanoseconds, timeToLaunchNxtWorkerSec, timeToLaunchNxtWorkerNano) == 1)
	{

       		WorkerLauncher(1, processTable, OsClock);

	 workersLeftToLaunch--;

 	 workersInSystem++;

	 RunSystemClock(OsClock, 500000);
		 
	GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds,timeInterval,0, &timeToLaunchNxtWorkerSec, &timeToLaunchNxtWorkerNano);
	
	}
	}
     }		
		
	
		//if 1/2 second passed, print process table
		if(HasHalfSecPassed(OsClock->seconds,OsClock->nanoseconds, timeToOutputSec, timeToOutputNano) == 1)
		{
			RunSystemClock(OsClock, 500000);
			PrintProcessTable(processTable, OsClock->seconds, OsClock->nanoseconds);
	GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds,HALF_SEC,0, &timeToOutputSec, &timeToOutputNano);
		}
			
		WakeUpProcess(processTable, OsClock);
		 nxtWorkerToSchedule = ReadyWorkerScheduler(processTable,OsClock);

		//if pid of next worker to message (nxtWorkerToMsg) is not zero send a message
		if(nxtWorkerToSchedule != 0)
		{	

			UpdateWorkerStateInProcessTable(processTable, nxtWorkerToSchedule, STATE_RUNNING);

			
			//send and recieve message to specific worker. returns a integer sent from the work about its status 
			msgbuffer msg = SendAndRecieveScheduleMsg(msqid, nxtWorkerToSchedule);
		
			int timeResults = msg.timeslice;
			
			//if statusState is 0, then worker is gonna terminate
			if(timeResults < 0)
			{
				//log fact that worker is terminating
				LogMessage(&logger,"Terminating", 0, nxtWorkerToSchedule,0,0);	
				//await that worker toterminate and get its pid
				int WorkerFinishedId = AwaitWorker(nxtWorkerToSchedule);
				//another worker is done
				workersComplete++;
				//worker no longer occupied
				UpdateWorkerStateInProcessTable(processTable, WorkerFinishedId, STATE_TERMINATED);
			
				workersInSystem--;

			}
			else if(timeResults >= 0 && timeResults < SCHEDULE_TIME)
			{
			 UpdateWorkerStateInProcessTable(processTable, nxtWorkerToSchedule, STATE_BLOCKED);
			
			 StoreWorkerEventWaitTime(processTable, nxtWorkerToSchedule, msg.eventWaitTime, OsClock);
			}
			else
			{
			 UpdateWorkerStateInProcessTable(processTable, nxtWorkerToSchedule, STATE_READY);

			}
			StoreWorkerServiceTime(processTable, nxtWorkerToSchedule,timeResults);
			RunSystemClock(OsClock,timeResults);			

		}	
		RunSystemClock(OsClock,SCHEDULE_TIME);//REMOVCE LATER
	}
	//relesase external resources (message queue and log file)
	DestructMsgQueue(msqid);
	fclose(logger);

}
int CanLaunchWorker(int currentSecond,int currentNano,int LaunchTimeSec,int LaunchTimeNano)
{
 if(LaunchTimeSec <= currentSecond && LaunchTimeNano <= currentNano)
 {
 return 1;
 }	 
 return 0;
}

void GenerateTimeToEvent(int currentSecond,int currentNano,int timeIntervalNano,int timeIntervalSec, int* eventSec, int* eventNano)
{
	*(eventSec) = currentSecond + timeIntervalSec;
	if(currentNano + timeIntervalNano >= MAX_NANOSECOND)
	{
	*(eventNano) = (currentNano + timeIntervalNano) - MAX_NANOSECOND;
	*(eventSec) = *(eventSec) + 1;
	}
	else
	{
	*(eventNano) = (currentNano + timeIntervalNano);
	}
}
void WorkerLauncher(int simLimit, struct PCB table[], struct Sys_Time* clock)
{
	//stores random time argument values workers will work	
	//	int secondsArg = 0;
	//iint nanosecondsArg = 0;

	//keep launching workers until limit (is reached
	//to create workers, first create a new copy process (child)
	//then replace that child with a worker program using execlp

	for(int i = 0 ; i < simLimit; i++)
	{

		
		pid_t newProcess = fork();

		if(newProcess < 0)
		{
			printf("\nFailed To Create New Process\n");
			exit(1);
		}
		if(newProcess == 0)
		{//child process (workers launching and running their software)

			execlp("./worker", "./worker", NULL);

			printf("\nFailed To Launch Worker\n");
			exit(1);
		}
		//add worker launched to process table 
		AddWorkerToProcessTable(table,newProcess, clock->seconds, clock->nanoseconds);
	}	
	return;
}

int AwaitWorker(pid_t worker_Id)
{	
	int pid = 0;

	while(pid == 0)
	{	
		int stat = 0;
		//nonblocking await to check if any workers are done
		pid = waitpid(worker_Id, &stat, WNOHANG);


		if(WIFEXITED(stat)) {

			if(WEXITSTATUS(stat) != 0)
			{
				exit(1);
			}	
		}
	}
	//return pid of finished worker
	//pid will equal 0 if no workers done
	return pid;
}

int HasHalfSecPassed(int currentSec, int currentNano, int halfSecMark, int halfNanoMark)
{//return 0, aka true if 1/2 second has passed
	if(halfSecMark <= currentSec && halfNanoMark <= currentNano)
	{
		return 1;
	}	
	return 0;
}
double getDataAfterDecimal(double data)
{
return (data - (int)data);
}
void StoreWorkerEventWaitTime(struct PCB table[], pid_t workerId, double eventTime, struct Sys_Time* clock)
{
int TimeToWaitNano = getDataAfterDecimal(eventTime) * MAX_NANOSECOND;
int TimeToWaitSec = (int) eventTime;
int EventTimeNano = 0;
int EventTimeSec = 0;
GenerateTimeToEvent(clock->seconds, clock->nanoseconds, TimeToWaitNano, TimeToWaitSec, &EventTimeSec, &EventTimeNano);
for(int i = 0; i < TABLE_SIZE;i++)
{
if(table[i].pid == workerId)
{
table[i].eventWaitSec = EventTimeSec;
table[i].eventWaitNano = EventTimeNano;
printf("event: id: %d clock: %d %d timeWait %d %d eventTimeAt %d %d param: %f\n",workerId,clock->seconds,clock->nanoseconds,TimeToWaitNano, TimeToWaitSec, EventTimeSec, EventTimeNano, eventTime);
}
}
}
void StoreWorkerServiceTime(struct PCB table[], pid_t workerId, int timeWorkingNano)
{
if(timeWorkingNano < 0)
{
timeWorkingNano = timeWorkingNano * -1;
}
for(int i = 0; i < TABLE_SIZE;i++)
{
if(table[i].pid == workerId)
{
 if(table[i].serviceTimeNano + timeWorkingNano >= MAX_NANOSECOND)
 {
 table[i].serviceTimeNano = (table[i].serviceTimeNano + timeWorkingNano) - MAX_NANOSECOND;
 table[i].serviceTimeSeconds = table[i].serviceTimeSeconds + 1; 
 }
 else
 {
 table[i].serviceTimeNano = (table[i].serviceTimeNano + timeWorkingNano); 
 }
}
}

}
int GetWorkerIndexFromProcessTable(struct PCB table[], pid_t workerId)
{//find the index (0 - 19) of worker with pid passed in params using table
	for(int i = 0; i < TABLE_SIZE;i++)
	{
		if(table[i].pid == workerId)
		{
			return i;
		}

	}
	printf("Invalid Worker Id\n");
	exit(1);
}
void AddWorkerToProcessTable(struct PCB table[], pid_t workerId, int secondsCreated, int nanosecondsCreated)
{
	for(int i = 0; i < TABLE_SIZE; i++)
	{
		//look for empty slot of process table bt checking each rows pid	
		if(table[i].pid == 0)
		{

			table[i].pid = workerId;
			table[i].startSeconds = secondsCreated;
			table[i].startNano = nanosecondsCreated;
			table[i].state = STATE_READY;
			//break out of loop, prevents assiginged this worker to every empty row
			break;
		}

	}	
}

void UpdateWorkerStateInProcessTable(struct PCB table[], pid_t workerId, int state)
{ //using pid of worker, update status state of theat work. currently used to set a running workers state to terminated (aka 0 value)

	for(int i = 0; i < TABLE_SIZE;i++)
	{
		if(table[i].pid == workerId)
		{
			table[i].state = state;
		}
	}
}	
void BuildProcessTable(struct PCB table[])
{ 
	for(int i = 0; i < TABLE_SIZE;i++)
	{ //give default values to process table so it doesnt output junk
		table[i].pid = 0;
		table[i].state = 0;
		table[i].startSeconds = 0;
		table[i].startNano = 0;
		table[i].serviceTimeSeconds = 0;
		table[i].serviceTimeNano = 0;
		table[i].eventWaitSec = 0;
		table[i].eventWaitNano = 0;
	}	

}	


void PrintProcessTable(struct PCB processTable[],int curTimeSeconds, int curTimeNanoseconds)
{ //print to console 
	int os_id = getpid();	
	printf("\nOSS PID:%d SysClockS: %d SysclockNano: %d\n",os_id, curTimeSeconds, curTimeNanoseconds);
	printf("Process Table:\n");
	printf("Entry State  PID          StartS      StartN     ServiceS    ServiceN     EventS    EventN\n");
	for(int i = 0; i < TABLE_SIZE; i++)
	{
		printf("%d      %d        %d          %d        %d       %d        %d      %d        %d\n", i, processTable[i].state, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano, processTable[i].serviceTimeSeconds, processTable[i].serviceTimeNano, processTable[i].eventWaitSec, processTable[i].eventWaitNano);
	}
}

//help info
void Help() {
	printf("When executing this program, please provide three numeric arguments");
	printf("Set-Up: oss [-h] [-n proc] [-s simul] [-t time]");
	printf("The [-h] argument is to get help information");
	printf("The [-n int_value] argument to specify the amount of workers to launch");
	printf("The [-s int_value] argument to specify how many workers are allowed to run simultaneously");
	printf("The [-t int_value] argument will be used to generate a random time in seconds (1 - t) that the workers will work for");
	printf("The [-f string_value] argument is used to specify the file name of the file that will be used as log a file");
	printf("Example: ./oss -n 5 -s 3 -t 7 -f test.txt");
	printf("\n\n\n");
	exit(1);
}
