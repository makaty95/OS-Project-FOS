// User-level Semaphore

#include "inc/lib.h"

struct semaphore create_semaphore(char *semaphoreName, uint32 value)
{
//TODO: [PROJECT'24.MS3 - #02] [2] USER-LEVEL SEMAPHORE - create_semaphore
//COMMENT THE FOLLOWING LINE BEFORE START CODING
//panic("create_semaphore is not implemented yet");
//Your Code is Here...

struct semaphore theSemaphore;
theSemaphore.semdata = (struct __semdata*)smalloc(semaphoreName, sizeof(struct __semdata), 1);

theSemaphore.semdata->count = value;
theSemaphore.semdata->lock = 0;
strcpy(theSemaphore.semdata->name, semaphoreName);
sys_initializeTheQueue(&(theSemaphore.semdata->queue));

return theSemaphore;


}
struct semaphore get_semaphore(int32 ownerEnvID, char* semaphoreName)
{
//TODO: [PROJECT'24.MS3 - #03] [2] USER-LEVEL SEMAPHORE - get_semaphore
//COMMENT THE FOLLOWING LINE BEFORE START CODING
//panic("get_semaphore is not implemented yet");
//Your Code is Here...

struct semaphore theSemaphore;
theSemaphore.semdata = (struct __semdata*) sget(ownerEnvID, semaphoreName);

return theSemaphore;

}

void wait_semaphore(struct semaphore sem)
{
//TODO: [PROJECT'24.MS3 - #04] [2] USER-LEVEL SEMAPHORE - wait_semaphore
//COMMENT THE FOLLOWING LINE BEFORE START CODING
//panic("wait_semaphore is not implemented yet");
//Your Code is Here...

uint32 key = 1;

while(xchg(&(sem.semdata->lock),key) != 0);

sem.semdata->count--;

if (sem.semdata->count < 0)
{
sys_sleepOnSemaphore(&sem);
}
sem.semdata->lock = 0;

return;
}

void signal_semaphore(struct semaphore sem)
{
//TODO: [PROJECT'24.MS3 - #05] [2] USER-LEVEL SEMAPHORE - signal_semaphore
//COMMENT THE FOLLOWING LINE BEFORE START CODING
//panic("signal_semaphore is not implemented yet");
//Your Code is Here...

uint32 key = 1;
while(xchg(&(sem.semdata->lock),key) !=0 );

sem.semdata->count++;

if (sem.semdata->count <= 0)
{
sys_signalToSemaphore(&sem);

}
sem.semdata->lock = 0;

return;


}

int semaphore_count(struct semaphore sem)
{
return sem.semdata->count;
}
