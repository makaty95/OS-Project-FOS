// Sleeping locks

#include "inc/types.h"
#include "inc/x86.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/environment_definitions.h"
#include "inc/assert.h"
#include "inc/string.h"
#include "sleeplock.h"
#include "channel.h"
#include "../cpu/cpu.h"
#include "../proc/user_environment.h"

void init_sleeplock(struct sleeplock *lk, char *name)
{
	init_channel(&(lk->chan), "sleep lock channel");
	char prefix[30] = "lock of sleeplock - ";
	char guardName[30+NAMELEN];
	strcconcat(prefix, name, guardName);
	init_kspinlock(&(lk->lk), guardName);
	strcpy(lk->name, name);
	lk->locked = 0;
	lk->pid = 0;
}

void acquire_sleeplock(struct sleeplock *lk)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #4 SLEEP LOCK - acquire_sleeplock
	//Your code is here
	//Comment the following line
	//panic("acquire_sleeplock() is not implemented yet...!!");

	//cprintf("acquire_sleeplock called. \n");


	struct kspinlock *guard_lk_ptr = &(lk->lk);
	struct Channel* chan_ptr = &(lk->chan);

	acquire_kspinlock(guard_lk_ptr);

	while((lk->locked)) {
		//cprintf("ENV. ID : %d \n", env->env_id);
		sleep(chan_ptr, guard_lk_ptr); // sleep on that channel
	}


	lk->locked = 1; // acquire lock again when wakeup

	release_kspinlock(guard_lk_ptr); // release guard

	//cprintf("acquire_sleeplock finish. \n");


}

void release_sleeplock(struct sleeplock *lk)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #5 SLEEP LOCK - release_sleeplock
	//Your code is here
	//Comment the following line
	//panic("release_sleeplock() is not implemented yet...!!");

	//cprintf("release_sleeplock begin. \n");

	struct Env_Queue *env_queue_ptr = &(lk->chan.queue);

	acquire_kspinlock(&(lk->lk));

	if(queue_size(env_queue_ptr) > 0) { // wake up sleeping processes
		wakeup_all(&(lk->chan));
	}

	lk->locked = 0;

	release_kspinlock(&(lk->lk));

	//cprintf("release_sleeplock finished. \n");
}

int holding_sleeplock(struct sleeplock *lk)
{
	int r;
	acquire_kspinlock(&(lk->lk));
	r = lk->locked && (lk->pid == get_cpu_proc()->env_id);
	release_kspinlock(&(lk->lk));
	return r;
}



