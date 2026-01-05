/*
 * channel.c
 *
 *  Created on: Sep 22, 2024
 *      Author: HP
 */
#include "channel.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <inc/string.h>
#include <inc/disk.h>

//===============================
// 1) INITIALIZE THE CHANNEL:
//===============================
// initialize its lock & queue
void init_channel(struct Channel *chan, char *name)
{
	strcpy(chan->name, name);
	init_queue(&(chan->queue));
}

//===============================
// 2) SLEEP ON A GIVEN CHANNEL:
//===============================
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// Ref: xv6-x86 OS code
void sleep(struct Channel *chan, struct kspinlock* lk)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #1 CHANNEL - sleep
	//Your code is here
	//Comment the following line
	//panic("sleep() is not implemented yet...!!");

	//cprintf("sleep was called. \n");

	struct kspinlock* kspinlock_ptr = &(ProcessQueues.qlock);
	acquire_kspinlock(kspinlock_ptr);

	//cprintf("inside critical section. \n");

	release_kspinlock(lk);


	// get current environment
	struct Env *current_environment_ptr = get_cpu_proc();

	// block current process
	current_environment_ptr->env_status = ENV_BLOCKED;

	// enqueue the process in channel queue
	struct Env_Queue* chan_queue_ptr = &(chan->queue);
	enqueue(chan_queue_ptr, current_environment_ptr);

	sched(); // schedule next process

	acquire_kspinlock(lk);

	release_kspinlock(kspinlock_ptr);

	//cprintf("[sleep done (belbaraka :)]. \n");

}

//==================================================
// 3) WAKEUP ONE BLOCKED PROCESS ON A GIVEN CHANNEL:
//==================================================
// Wake up ONE process sleeping on chan.
// The qlock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes
void wakeup_one(struct Channel *chan)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #2 CHANNEL - wakeup_one
	//Your code is here
	//Comment the following line
	//panic("wakeup_one() is not implemented yet...!!");

	//cprintf("wakeup_one called. \n");

	struct kspinlock *kspin_lock_ptr = &(ProcessQueues.qlock);
	struct Env_Queue *env_queue = &(chan->queue);

	acquire_kspinlock(kspin_lock_ptr);

	//cprintf("inside the critical section. \n");

	if(queue_size(env_queue) > 0) sched_insert_ready(dequeue(env_queue));


	release_kspinlock(kspin_lock_ptr);

}

//====================================================
// 4) WAKEUP ALL BLOCKED PROCESSES ON A GIVEN CHANNEL:
//====================================================
// Wake up all processes sleeping on chan.
// The queues lock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes

void wakeup_all(struct Channel *chan)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #3 CHANNEL - wakeup_all
	//Your code is here
	//Comment the following line
	//panic("wakeup_all() is not implemented yet...!!");

	//cprintf("wakeup_all called. \n");

	struct Env_Queue *env_q_ptr = &(chan->queue);
	struct kspinlock *kspinlock_ptr = &(ProcessQueues.qlock);

	acquire_kspinlock(kspinlock_ptr); // critical section for queue protection

	while(env_q_ptr->lh_last != NULL) {
		sched_insert_ready(dequeue(env_q_ptr));
	}

	release_kspinlock(kspinlock_ptr); // critical section for queue protection

	//cprintf("wakeup_all finished it's work. \n");

}

