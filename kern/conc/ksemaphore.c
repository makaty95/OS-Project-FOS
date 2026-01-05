// Kernel-level Semaphore

#include "inc/types.h"
#include "inc/x86.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/environment_definitions.h"
#include "inc/assert.h"
#include "inc/string.h"
#include "ksemaphore.h"
#include "channel.h"
#include "../cpu/cpu.h"
#include "../proc/user_environment.h"
#include <kern/cpu/sched.h>

void init_ksemaphore(struct ksemaphore *ksem, int value, char *name)
{
	init_channel(&(ksem->chan), "ksemaphore channel");
	init_kspinlock(&(ksem->lk), "lock of ksemaphore");
	strcpy(ksem->name, name);
	ksem->count = value;
}


void ksem_sleep(struct ksemaphore *ksem) {



    struct Env *current_env_ptr = get_cpu_proc();

	//cprintf("ksem_sleep with env_id: %d \n", current_env_ptr->env_id);

    struct kspinlock* kspin_lock_ptr = &(ProcessQueues.qlock);

    acquire_kspinlock(kspin_lock_ptr);

    ksem->lk.locked = 0;

    struct Env_Queue* queue_ptr = &(ksem->chan.queue);

    current_env_ptr->env_status = ENV_BLOCKED;
    enqueue(queue_ptr, current_env_ptr);

    sched(); // schedule next environment

    ksem->lk.locked = 1;

    release_kspinlock(kspin_lock_ptr);


    //cprintf("ksem_sleep with env_id: %d finished \n", current_env_ptr->env_id);
}

void wait_ksemaphore(struct ksemaphore *ksem)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #6 SEMAPHORE - wait_ksemaphore
	//Your code is here
	//Comment the following line
	//panic("wait_ksemaphore() is not implemented yet...!!");

	//cprintf("wait_ksemaphore is begin. \n");

	uint32 K = 1;

	while(xchg(&(ksem->lk.locked), (uint32)K) != 0);

	ksem->count--; // changing cnt

	if (ksem->count < 0) {
		// sleep on that semaphore //
		ksem_sleep(ksem);
	}

	ksem->lk.locked = 0;

	//cprintf("wait_ksemaphore finished. \n");


}

void signal_to_ksem(struct ksemaphore *ksem) {

    acquire_kspinlock(&(ProcessQueues.qlock));
    sched_insert_ready(dequeue(&(ksem->chan.queue)));
    release_kspinlock(&(ProcessQueues.qlock));

}

void signal_ksemaphore(struct ksemaphore *ksem)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #7 SEMAPHORE - signal_ksemaphore
	//Your code is here
	//Comment the following line
	//panic("signal_ksemaphore() is not implemented yet...!!");

	//cprintf("signal_ksemaphore called \n");

	uint32 KK = 1;
	while(xchg(&(ksem->lk.locked), KK) != 0);

	ksem->count++;

	if (ksem->count <= 0) {
		// signal the semaphore
		signal_to_ksem(ksem);

	}
	ksem->lk.locked = 0;

	//cprintf("signal_ksemaphore finished \n");

}


