#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_kspinlock(&AllShares.shareslock, "shares lock");
	//init_sleeplock(&AllShares.sharessleeplock, "shares sleep lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//=========================
// [2] Find Share Object:
//=========================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* find_share(int32 ownerID, char* name)
{
#if USE_KHEAP
	struct Share * ret = NULL;
	bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
	if (!wasHeld)
	{
		acquire_kspinlock(&(AllShares.shareslock));
	}
	{
		struct Share * shr ;
		LIST_FOREACH(shr, &(AllShares.shares_list))
		{
			//cprintf("shared var name = %s compared with %s\n", name, shr->name);
			if(shr->ownerID == ownerID && strcmp(name, shr->name)==0)
			{
				//cprintf("%s found\n", name);
				ret = shr;
				break;
			}
		}
	}
	if (!wasHeld)
	{
		release_kspinlock(&(AllShares.shareslock));
	}
	return ret;
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [3] Get Size of Share Object:
//==============================
int size_of_shared_object(int32 ownerID, char* shareName)
{
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	struct Share* ptr_share = find_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=====================================
// [1] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* alloc_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #1 alloc_share
	//Your code is here
	//Comment the following line
	// panic("alloc_share() is not implemented yet...!!");


	// Allocate needed memory in kernel heap
	struct Share *shared_object_ptr = (struct Share*) kmalloc(sizeof(struct Share));

	if(shared_object_ptr == NULL ) return NULL;


	uint32 overall_size = ROUNDUP(size, PAGE_SIZE);

	// fill the object //============================================>
	uint32 MSK = (1 << 31);
	shared_object_ptr->ID = ((uint32)shared_object_ptr) & MSK;
	shared_object_ptr->size = (uint32)size;
	strcpy(shared_object_ptr->name, shareName);
	shared_object_ptr->ownerID = ownerID;
	shared_object_ptr->references = 1;
	shared_object_ptr->isWritable = isWritable;
	//============================================>

	uint32 frames_num = overall_size / PAGE_SIZE;
	uint32 size_to_allocate = sizeof(struct FrameInfo*) * frames_num;
	struct FrameInfo** frameArray =  (struct FrameInfo**) kmalloc(size_to_allocate);

	// if allocation failed, free the memory we created.
	if(frameArray == NULL) {
		kfree((void*) shared_object_ptr);
		return NULL;
	}

	shared_object_ptr->framesStorage = frameArray;
	return shared_object_ptr;

}


//=========================
// [4] Create Share Object:
//=========================
int create_shared_object(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #3 create_shared_object
	//Your code is here
	//Comment the following line
	//panic("create_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should create the shared object at the given virtual address with the given size
	// and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_EXISTS if the shared object already exists
	//	c) E_NO_SHARE if failed to create a shared object


	// check if it already exists
	if(find_share(ownerID, shareName) != NULL) {
		return E_SHARED_MEM_EXISTS;
	}

	uint32 roundedSize = (ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE) * PAGE_SIZE;
	uint32 pages = ROUNDUP(roundedSize, PAGE_SIZE) / PAGE_SIZE;


	struct Share* shareObj = alloc_share(ownerID , shareName , roundedSize , isWritable);
	if(shareObj == NULL) return E_NO_SHARE;


	uint32 ptr_va = (uint32)virtual_address;
	int free_idx = 0;

	// allocating and mapping all the pages
	for (int i = 0 ; i < pages; i++, ptr_va += PAGE_SIZE, free_idx++) {


		struct FrameInfo *myFrame = NULL;
		int state = allocate_frame(&myFrame);
		if(myFrame == NULL) {

			// delete all i allocated and return no_share
			uint32 base_address = (uint32)virtual_address;
			while(ptr_va >= base_address) {
				ptr_va -= PAGE_SIZE;

				free_idx--;
				shareObj->framesStorage[free_idx] = NULL;

				// unmap memory
				unmap_frame((myenv->env_page_directory), ptr_va);

			}

			return E_NO_SHARE;
		}

		uint32 permissions = PERM_WRITEABLE | PERM_USER | PERM_PRESENT;
		state = map_frame(myenv->env_page_directory, myFrame, ptr_va, permissions);
		if (state != 0) return E_NO_SHARE;
		else shareObj->framesStorage[i] = myFrame;

	}


#if USE_KHEAP

	acquire_kspinlock(&AllShares.shareslock);
	// critical section
	LIST_INSERT_TAIL(&AllShares.shares_list , shareObj);
	release_kspinlock(&AllShares.shareslock);

#endif

	return shareObj->ID;

}


//======================
// [5] Get Share Object:
//======================
int get_shared_object(int32 ownerID, char* shareName, void* virtual_address) {
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #5 get_shared_object
	//Your code is here
	//Comment the following line
	//panic("get_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// 	This function should share the required object in the heap of the current environment
	//	starting from the given virtual_address with the specified permissions of the object: read_only/writable
	// 	and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	struct Share* share_object = find_share(ownerID, shareName);
	if (share_object == NULL)
		return E_SHARED_MEM_NOT_EXISTS;

	uint32 size__ = (uint32) (share_object->size);
	uint32 totalSize = ROUNDUP(size__, PAGE_SIZE);

	uint32 VA = (uint32) virtual_address;
	uint32 CNT = totalSize / PAGE_SIZE;
	uint32 perms_to_set = (
			share_object->isWritable ?
					PERM_WRITEABLE | PERM_PRESENT : PERM_PRESENT);

	for (int i = 0; i < CNT; i++, VA += PAGE_SIZE) {
		int32 state = map_frame(myenv->env_page_directory,
				share_object->framesStorage[i], VA, perms_to_set | PERM_USER);
	}

	share_object->references++;
	return share_object->ID;

}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//
//=========================
// [1] Delete Share Object:
//=========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - free_share
	//Your code is here
	//Comment the following line
	panic("free_share() is not implemented yet...!!");
}


//=========================
// [2] Free Share Object:
//=========================
int delete_shared_object(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - delete_shared_object
	//Your code is here
	//Comment the following line
	panic("delete_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should free (delete) the shared object from the User Heapof the current environment
	// If this is the last shared env, then the "frames_store" should be cleared and the shared object should be deleted
	// RETURN:
	//	a) 0 if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	// Steps:
	//	1) Get the shared object from the "shares" array (use get_share_object_ID())
	//	2) Unmap it from the current environment "myenv"
	//	3) If one or more table becomes empty, remove it
	//	4) Update references
	//	5) If this is the last share, delete the share object (use free_share())
	//	6) Flush the cache "tlbflush()"

}
