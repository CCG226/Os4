#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "worker.h"
#include <errno.h>
#include <time.h>
//Author: Connor Gilmore
//Purpose: takes in a numeric argument
//Program will take in two arguments that represent a random time in seconds and nanoseconds
//are program will then work for that amoount of time by printing status messages
//are worker will constantly check the os's system clock to see if it has reached the end of its work time, if it has it will terminate

int main(int argc, char** argv)
{
	srand(time(NULL));
	

	TaskHandler();//do worker task


	return EXIT_SUCCESS;
}
int AccessMsgQueue()
{
	//access message queue create by workinh using key constant
	int msqid = -1;
	if((msqid = msgget(MSG_SYSTEM_KEY, 0777)) == -1)
	{
		perror("Failed To Join Os's Message Queue.\n");
		exit(1);
	}
	return msqid;
}
struct Sys_Time* AccessSystemTime()
{//access system clock from shared memory (read-only(
	int shm_id = shmget(SYS_TIME_SHARED_MEMORY_KEY, sizeof(struct Sys_Time), 0444);
	if(shm_id == -1)
	{
		perror("Failed To Access System Clock");

		exit(1);	
	}
	return (struct Sys_Time*)shmat(shm_id, NULL, 0);

}
void DisposeAccessToShm(struct Sys_Time* clock)
{//detach system clock from shared memory
	if(shmdt(clock) == -1)
	{
		perror("Failed To Release Shared Memory Resources.\n");
		exit(1);
	}

}
	
//workers console print task
void TaskHandler()
{
	int msqid = AccessMsgQueue();
	//get parent and worker ids to print
	pid_t os_id = getppid();
	pid_t worker_id = getpid();
	
	struct Sys_Time* Clock = AccessSystemTime();

	//determines of worker can keep working, if 0 worker must terminate
	int status = RUNNING;

	msgbuffer msg;

	//while status is NOT 0, keep working
	while(status != TERMINATING)
	{ 


		//wait for os to send a request for status information on wheather or not this worker is done working
		int timeslice = AwaitOsRequestForPermissionsToRunTask(msqid, &msg);
		int task = SelectTask();
		int timeWorkerRan = 0;
		int eventWaitTime = 0;
		if(task == 0)
		{
		 timeWorkerRan = TaskRunAndTerminate(timeslice);
		 status = TERMINATING;
		}
		else if(task == 1)
		{
		timeWorkerRan = TaskRunAndGetExternalResource(timeslice);
		eventWaitTime = GenerateEventWaitTime();
		}
		else
		{
		timeWorkerRan = TaskRun(timeslice);
		}	
		printf("%d %d %d\n", timeWorkerRan, eventWaitTime, task);
		//send status varable indicating status of worker back to os
		SendResponseMsg(msqid, &msg, timeWorkerRan, eventWaitTime);
		
	}
	//worker saying its done
	printf("WORKER PID: %d PPID: %d SysClockS: %d SysClockNano: %d TermTimeS: %d TermTimeNano: %d \n--Terminating  \n",worker_id,os_id,Clock->seconds,Clock->nanoseconds, 0,0);
	//detach workers read only system clock from shared memory
	DisposeAccessToShm(Clock);

}
int SelectTask()
{
	
int option = (rand() % 100) + 1;

if(option > 0 && option <= 5)
{//5% chance to run and terminate
return 0;
}
else if(option > 5 && option <= 15)
{//10% chance to run and need external resource
return 1;
}
else
{//90% to run fully
return 2;
}

}

int TaskRun(int timeslice)
{
  return timeslice;
}
int TaskRunAndTerminate(int timeslice)
{
int timeRanBeforeTerm = (rand() % timeslice) + 1;

return timeRanBeforeTerm * -1;
}
int TaskRunAndGetExternalResource(int timeslice)
{
int percentageOfTimeSliceUsed = (rand() % 100);

double percentageUsedAsDecimal = (double)percentageOfTimeSliceUsed / 100;

int timeSliceUsed = ((int) (timeslice * percentageUsedAsDecimal));

return timeSliceUsed;

}
double GenerateEventWaitTime()
{

int eventWaitSeconds = (rand() % 6);

int eventWaitMilliseconds = (rand() % 1001);

double convertMilliToSecs = (double) eventWaitMilliseconds / 1000;

return (eventWaitSeconds + convertMilliToSecs);

}
int AwaitOsRequestForPermissionsToRunTask(int msqid, msgbuffer *msg)
{
	//blocking wait for message from os requesting status info
	//os communicates to this worker via its pid (4th param)	

if(msgrcv(msqid, msg, sizeof(msgbuffer), getpid(), 0) == -1)
	{
		printf("Failed To Get Message Request From Os. Worker: %d\n", getpid());
		fprintf(stderr, "errno: %d\n", errno);
		exit(1);
	}
	//return msg->timeslice;
	return msg->timeslice;
}
void SendResponseMsg(int msqid, msgbuffer *msg, int timeRan, int eventWaitTime)
{//send status update via integer value Data about if this worker is gonna terminate
	//status == 0 if worker is gonna terminate
	msg->timeslice = timeRan;
	msg->mtype = 1;
	msg->eventWaitTime = eventWaitTime;
	//send message back to os
	if(msgsnd(msqid, msg, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
		perror("Failed To Generate Response Message Back To Os.\n");
		exit(1);
	}

}
