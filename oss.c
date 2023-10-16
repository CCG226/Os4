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
	int timeLimit = 0;//-t iter value (number to pass to worker as parameter)
	char* logFileName = NULL;//-f value to store file name of log file


	//stored id to access shared memory for system clock
	int shm_ClockId = -1;

	//pass workerAmount (-n), simultaneousLimit (-s),timeLimit(-t) by reference and assigns coresponding argument values from the command line to them
	ArgumentParser(argc, argv,&workerAmount, &simultaneousLimit, &timeLimit, &logFileName);
	
	//'starts' aka sets up shared memory system clock and assigns default values for clock 
	shm_ClockId = StartSystemClock(&OS_Clock);

	//handles how os laumches, tracks, and times workers
	WorkerHandler(workerAmount, simultaneousLimit, timeLimit,logFileName, OS_Clock, processTable);

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
int SendAndRecieveStatusMsg(int msqid, pid_t workerId) 
{
	msgbuffer msg;
	//send message to specific worker via id, Data sent doesnt matter we should want to notify worker via message to get a status update from them
	msg.mtype = (int) workerId;
	msg.timeslice = 50000000;
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
	return msg.timeslice;

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
	(*Clock)->rate = 50000;
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
void RunSystemClock(struct Sys_Time *Clock) {
	//handles clock iteration / ticking
	if(Clock->nanoseconds < 1000000000)
	{
		Clock->nanoseconds += Clock->rate;	
	}
	if(Clock->nanoseconds == 1000000000)
	{ //nanoseconds reack 1,000,000,000 then reset nanoseconds and iterate clock
		Clock->seconds++;
		Clock->nanoseconds = 0;	 
	}

}


void ArgumentParser(int argc, char** argv, int* workerAmount, int* workerSimLimit, int* workerTimeLimit, char** fileName) {
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
				*(workerTimeLimit) = atoi(optarg);
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
	int isValid = ValidateInput(*workerAmount, *workerSimLimit, *workerTimeLimit, *fileName);

	if(isValid == 0)
	{//valid arguments 
		return;	
	}
	else
	{
		exit(1);
	}
}
int ValidateInput(int workerAmount, int workerSimLimit, int workerTimeLimit, char* fileName)
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
	if(workerAmount < 0 || workerSimLimit < 0 || workerTimeLimit < 0)
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
	if(workerTimeLimit < 1)
	{
		printf("\nWorkers Need At Least A Second To Complete Their Task!\n");
		isValid = 1;
	}
	return isValid;

}
pid_t GetNxtWorkerToMsg(struct PCB table[], int* curIndex)
{	//store pid of next worker to message in the os system
	pid_t nxtWorker = 0;	
	//iterate through the table starting at curIndex position 
	//to find next running worker to message 
	for(int i = 0; i < TABLE_SIZE; i++)
	{       //iterate to next worker on pcb table
		*(curIndex) = *(curIndex) + 1;
		//if we reach the end of the table reset curIndex to first position on table
		if(*(curIndex) == TABLE_SIZE)
		{
			*(curIndex) = 0;

		}

		if(table[*(curIndex)].state == STATE_RUNNING)
		{//if we find a running worker on the table assign ints pid to local var and escape loop
			nxtWorker = table[*(curIndex)].pid;

			break;

		}

	}
	return nxtWorker;

}
void LogMessage(FILE** logger,const char* msgType,int workerIndex,pid_t workerId,int curSec,int curNano)
{
	//logs status message to logfile

	char buffer[100];	
	if(strcmp(msgType, "Terminating") == 0)
	{//if msgType equals Terminating then output that this work is about to terminate
		sprintf(buffer, "OSS: Worker %d PID %d is planning to terminate.\n", workerIndex, workerId);
	}
	else
	{ //output the fact that oss is sending/recieving a message from a specific worker
		sprintf(buffer, "OSS: %s message to worker %d PID %d at time %d:%d.\n",msgType, workerIndex, workerId, curSec, curNano);
	}

	fputs(buffer, *logger);


}
void WorkerHandler(int workerAmount, int workerSimLimit,int workerTimeLimit, char* logFile, struct Sys_Time* OsClock, struct PCB processTable[])
{	//access logfile
	FILE *logger = fopen(logFile, "w");
	//get id of message queue after creating ti
	int msqid = ConstructMsgQueue();
	//tracks pcb table index of last selected worker to message, after messaging that worker we can use this index to remeber where are in the pcb table and find the mext worker to message on the pcb table, thus avoiding the os repeatedly sending a message to the smae worker
	int nxtWorkerIndex = 0;
	//holds pid of next worker to message from os, os will send and recieve a status response from this worker 
	pid_t nxtWorkerToMsg = 0;

	//tracks amount of workers finished with task
	int workersComplete = 0;

	//tracks amount of workers left to be launched
	int workersLeft = 0;


	if(workerSimLimit > workerAmount)
	{
		//if (-s) values is greater than (-n) value, then launch all workers at once	
		WorkerLauncher(workerAmount, processTable, OsClock);
		//no more workers left to be launched
		workersLeft = 0;

	}
	else
	{
		//workerAmount (-n) is greater than or equal to  WorkerSimLimit (-s), so launch (-s)  amount of workers	
		WorkerLauncher(workerSimLimit, processTable, OsClock);	

		//subtract simultanoues limit from amount of workers (n) to get amount of workers left to launch
		workersLeft = workerAmount - workerSimLimit;

	}

	//keep looping until all workers (-n) have finished working
	while(workersComplete !=  workerAmount)
	{	
		//increment clock
		RunSystemClock(OsClock);
		//if 1/2 second passed, print process table
		if(HasHalfSecPassed(OsClock->nanoseconds) == 0)
		{
			PrintProcessTable(processTable, OsClock->seconds, OsClock->nanoseconds);
		}
		//get pid of next worker in pcb table
		nxtWorkerToMsg = GetNxtWorkerToMsg(processTable, &nxtWorkerIndex);

		//if pid of next worker to message (nxtWorkerToMsg) is not zero send a message
		if(nxtWorkerToMsg != 0)
		{	

			//holds index value of next worker os will message for the sake of logging. (so we can saay for example OSS: Senfing message to worker 'x')
			int workerIndexNum = GetWorkerIndexFromProcessTable(processTable, nxtWorkerToMsg);			
			//log the fact that os is sending a message to specific worker
			LogMessage(&logger,"Sending", workerIndexNum, nxtWorkerToMsg, OsClock->seconds, OsClock->nanoseconds);
			//send and recieve message to specific worker. returns a integer sent from the work about its status 
			int statusState = SendAndRecieveStatusMsg(msqid, nxtWorkerToMsg);
			//log the fact that os recieved a response message from specific worker
			LogMessage(&logger,"Recieving", workerIndexNum, nxtWorkerToMsg, OsClock->seconds, OsClock->nanoseconds);
			//if statusState is 0, then worker is gonna terminate
			if(statusState < 0)
			{
				//log fact that worker is terminating
				LogMessage(&logger,"Terminating", workerIndexNum, nxtWorkerToMsg,0,0);	
				//await that worker toterminate and get its pid
				int WorkerFinishedId = AwaitWorker(nxtWorkerToMsg);
				//another worker is done
				workersComplete++;
				//worker no longer occupied
				UpdateWorkerStateInProcessTable(processTable, WorkerFinishedId, STATE_TERMINATED);

				if(workersLeft != 0)
				{ //if we are allowed to, launch another worker
					WorkerLauncher(1,processTable,OsClock);
					workersLeft--;
				}

			}
		}	

	}
	//relesase external resources (message queue and log file)
	DestructMsgQueue(msqid);
	fclose(logger);

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

int HasHalfSecPassed(int nanoseconds)
{//return 0, aka true if 1/2 second has passed
	if(nanoseconds == 500000000 || nanoseconds == 0)
	{
		return 0;
	}	
	return 1;
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
			table[i].state = STATE_RUNNING;
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
	}	

}	


void PrintProcessTable(struct PCB processTable[],int curTimeSeconds, int curTimeNanoseconds)
{ //print to console 
	int os_id = getpid();	
	printf("\nOSS PID:%d SysClockS: %d SysclockNano: %d\n",os_id, curTimeSeconds, curTimeNanoseconds);
	printf("Process Table:\n");
	printf("Entry State  PID          StartS  StartN\n");
	for(int i = 0; i < TABLE_SIZE; i++)
	{
		printf("%d      %d        %d          %d        %d\n", i, processTable[i].state, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano);
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
