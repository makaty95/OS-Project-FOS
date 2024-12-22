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
struct Share* get_share(int32 ownerID, char* name);
//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_spinlock(&AllShares.shareslock, "shares lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [2] Get Size of Share Object:
//==============================
int getSizeOfSharedObject(int32 ownerID, char* shareName)
{
	//[PROJECT'24.MS2] DONE
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	struct Share* ptr_share = get_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}



//===========================================================


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===========================
// [1] Create frames_storage:
//===========================
// Create the frames_storage and initialize it by 0
inline struct FrameInfo** create_frames_storage(int numOfFrames)
{
	//TODO: [PROJECT'24.MS2 - #16] [4] SHARED MEMORY - create_frames_storage()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("create_frames_storage is not implemented yet");
	//Your Code is Here...

	//cprintf("-------------create_frames_storage called------------\n");
	struct FrameInfo** frames_arr = (struct FrameInfo**)kmalloc(sizeof(struct FrameInfo*)*numOfFrames);


   if(frames_arr == NULL) {
	   return NULL;
   }

   for(int i = 0 ;i<numOfFrames ; i++) { // i = 2

	   frames_arr[i] = NULL;
   }

   return frames_arr;
}


//=====================================
// [2] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* create_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
	//TODO: [PROJECT'24.MS2 - #16] [4] SHARED MEMORY - create_share()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("create_share is not implemented yet");
	//Your Code is Here...
//	cprintf("---------------create_share called-------------\n");
//	cprintf("flag create_share 1 size = %d\n" , size);
//	cprintf("flag create_share 2\n");

	ownerID &= (~(1<<31));

	struct Share *share_obj = (struct Share *)kmalloc(sizeof(struct Share));

	if(share_obj==NULL )
	{   //cprintf("flag create_share 3\n");
		return NULL;
	}
	//cprintf("flag create_share 3.5\n");
	strcpy(share_obj->name, shareName);
	///cprintf("flag create_share 4\n");

	uint32 total_size = ROUNDUP(size, PAGE_SIZE);
	share_obj->size = (uint32)size;
	//cprintf("size %u if eq share_obj->size-> %d\n", size, share_obj->size);
	//cprintf("flag create_share 5\n");
	share_obj->isWritable = isWritable;
	//cprintf("flag create_share 6\n");
	share_obj->references = 1;
	//cprintf("flag create_share 7\n");

	//mask MSB to 1 0000000000000000010010101
	// 			  0 1111111111111111111111111


	share_obj->ID = ownerID;
	//cprintf("flag create_share 8\n");
	uint32 number_of_pages = total_size / PAGE_SIZE;
	//cprintf("flag create_share 9\n");
	//cprintf("numframes = %d\n", number_of_pages);
	struct FrameInfo** frames_arr = create_frames_storage((int)number_of_pages);
	if(frames_arr == NULL) {
		kfree((void *)share_obj);
		return NULL;
	}
	share_obj->framesStorage = frames_arr;

	//cprintf("flag create_share 12\n");

    return share_obj;
}

//=============================
// [3] Search for Share Object:
//=============================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* get_share(int32 ownerID, char* name)
{
	//TODO: [PROJECT'24.MS2 - #17] [4] SHARED MEMORY - get_share()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("get_share is not implemented yet");
	//Your Code is Here..
	//cprintf("---------get_share called----------------\n");

	ownerID &= (~(1<<31));
	acquire_spinlock(&AllShares.shareslock);
	//cprintf("a\n");

	struct Share* it = NULL;
	LIST_FOREACH(it , &AllShares.shares_list)
	{
		//cprintf("b\n");
		///cprintf("shared: id = %d, name = %s, size = %d\n", it->ID, it->name, it->size);
		if(it->ID==ownerID  && strcmp(it->name, name) == 0 && strlen(it->name) == strlen(name)) {
			//cprintf("c\n");
			release_spinlock(&AllShares.shareslock);
			//cprintf("d\n");
			return it;
		}
		//cprintf("e\n");
	}
	//cprintf("f\n");
	release_spinlock(&AllShares.shareslock);
	//cprintf("g\n");
	return NULL;


}

//=========================
// [4] Create Share Object:
//=========================
uint32 smis_free_page(uint32 page_va, struct Env* env) {
	// page_va is the virtual address within some page , could be with offset doesn't matter.

	uint32* ptr_page_table;
	int status = get_page_table(env->env_page_directory, page_va, &ptr_page_table);
	if(status == TABLE_NOT_EXIST) {
		ptr_page_table = (uint32*)create_page_table(env->env_page_directory, page_va);
	}

	uint32 mask = (1 << 10);
	return !((ptr_page_table[PTX(page_va)]&mask)); // if 0 its not marked , if other it is set
}


void sm_mark_page(uint32 page_va, struct Env* env, uint32 state) {
	// page_va is the virtual address within some page , could be with offset doesn't matter.

	uint32* ptr_page_table;
	int status = get_page_table(env->env_page_directory, page_va, &ptr_page_table);
	if(status == TABLE_NOT_EXIST) {
		cprintf("creating page table\n");
		ptr_page_table = (uint32*)create_page_table(env->env_page_directory, page_va);
	}


	if(state == FREE_PAGE) {
		//cprintf("%p free done\n", (void*)page_va);
		ptr_page_table[PTX(page_va)] &= (~(1<<10));
		ptr_page_table[PTX(page_va)] &= (~(1<<11));
		return;
	}

	ptr_page_table[PTX(page_va)] |= (1 << 10);
	//cprintf("%p mark done\n", (void*)page_va);
	if(state == PAGE_MARK_START) {
		//cprintf("%p mark start done\n", (void*)page_va);
		ptr_page_table[PTX(page_va)] |= (1<<11);
		//cprintf("address %x marked start\n", page_va);
	}

	//cprintf("page at address %x marked\n", page_va);
}

uint32 smget_pgallocation_address(uint32 size, struct Env* e) {


		uint32 it_start = e->hlimit+PAGE_SIZE;
		uint32* page_directory = e->env_page_directory;
		uint32 pg_alloc_last = e->pgalloc_last;

		uint32 it = it_start;
		uint32 curSize = 0;
		uint32 pgalloc_ptr = 0;

		//cprintf("searchig for space = %umb, sratring from %p to %p\n",(size/(1<<20)), (void*)it_start, (void*)pg_alloc_last);
		for (; curSize < size && it < pg_alloc_last; it += PAGE_SIZE) {

			if (smis_free_page(it, e)) { // if free page
				if(curSize == 0) {
					pgalloc_ptr = it;
				}
				curSize += PAGE_SIZE;

			}else {	// if marked for another space
				curSize = 0;
				pgalloc_ptr = 0;
			}
		}

		// if exist some free pages before pgalloc_last which could be used.
		if(pgalloc_ptr != 0 && curSize >= size) {
			return pgalloc_ptr;
		}

		return pg_alloc_last;

}

int createSharedObject(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #19] [4] SHARED MEMORY [KERNEL SIDE] - createSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("createSharedObject is not implemented yet");
	//Your Code is Here...
    //acquire_spinlock(&(AllShares.shareslock));
	//cprintf("---------createSharedObject called size = %u, nae = %s----------------\n", size, shareName);
	struct Env* myenv = get_cpu_proc(); //The calling environment


	ownerID &= (~(1<<31));

	// if the object already exists.
	if(get_share(ownerID, shareName) != NULL) {
		return E_SHARED_MEM_EXISTS;
	}

	// masking the id to +ve

    int32 ownderid = ownerID;
    uint32 total_size = (ROUNDUP(size, PAGE_SIZE)/PAGE_SIZE)*PAGE_SIZE;

    // getting start address.
	uint32  ret_add = smget_pgallocation_address(total_size , myenv);


	if (ret_add == myenv->pgalloc_last) {
		if((myenv->pgalloc_last + total_size) > (uint32)USER_HEAP_MAX) {
			//cprintf("damn, there is not allocation space to be allocated in uheap\n");
			return E_NO_SHARE;
		}

		myenv->pgalloc_last += total_size;
	}

	// the address to be returned
	void* ret = (void*)ret_add;

//	uint32 end_ptr = total_size + ret_add;
//    if(end_ptr > USER_HEAP_MAX) {
//		return E_NO_SHARE;
//	}


	struct Share* cur_share = create_share(ownerID , shareName , total_size , isWritable);
	if(cur_share == NULL)
	{
		return E_NO_SHARE;
	}

	uint32 num_of_pages = ROUNDUP(total_size,PAGE_SIZE)/PAGE_SIZE;
	uint32 it = ret_add;
	int i = 0;
//	cprintf("isWritable == %x\n" , isWritable);

	// allocating pages
	for (i = 0 ; i < num_of_pages; i++, it += PAGE_SIZE) {


		struct FrameInfo *newFrame = NULL;
		int state = allocate_frame(&newFrame);
		if(newFrame == NULL) {
			return E_NO_SHARE;
		}

		state = map_frame(myenv->env_page_directory, newFrame, it, PERM_WRITEABLE|PERM_USER|PERM_PRESENT);
		if (state != 0) {
			return E_NO_SHARE;
		}

		sm_mark_page(it, myenv, PAGE_MARKED);
		cur_share->framesStorage[i] = newFrame;
//		cprintf("flag sys_smalloc 14.5 success\n");

	}
	sm_mark_page(ret_add, myenv, PAGE_MARK_START);
	//cprintf("flag sys_smalloc 15\n");

	// insert the shared object inside the list

	acquire_spinlock(&AllShares.shareslock);
	LIST_INSERT_TAIL(&AllShares.shares_list , cur_share);
	release_spinlock(&AllShares.shareslock);



	myenv->sharedObjectsCounter++;

	myenv->shr_returned_address = ret;

	return (int)cur_share->ID;
}


//======================
// [5] Get Share Object:
//======================
int getSharedObject(int32 ownerID, char* shareName, void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #21] [4] SHARED MEMORY [KERNEL SIDE] - getSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("getSharedObject is not implemented yet");
	//Your Code is Here...

	//cprintf("---------getSharedObject called name = %s----------------\n", shareName);

	ownerID &= (~(1<<31));

	struct Env* myenv = get_cpu_proc(); //The calling environment

    struct Share* cur_share = get_share(ownerID, shareName);
    //cprintf("after get_share\n");
    if(cur_share == NULL) {
    	return E_SHARED_MEM_NOT_EXISTS ;
    }

    //cprintf("orig size: %d\n", cur_share->size);
    uint32 sz = (uint32)(cur_share->size);


    uint32 total_size = ROUNDUP(sz, PAGE_SIZE); //
    //cprintf("size: %u, total_size: %u\n", sz, total_size);

	uint32  ret_add = smget_pgallocation_address(total_size , myenv);
	//cprintf("after smget_pgallocation_address call\n");

	if (ret_add == myenv->pgalloc_last) {
		//cprintf("it = last\n");

		if((myenv->pgalloc_last + total_size) > (uint32)USER_HEAP_MAX) {
			//cprintf("returned not share\n");
			return E_NO_SHARE;
		}
	    myenv->pgalloc_last += total_size;
	}

//	cprintf("resuming\n");


    uint32 permm = (cur_share->isWritable ? PERM_WRITEABLE|PERM_PRESENT : PERM_PRESENT);

    uint32 it = (uint32)ret_add;
    int32 arr_sz = sizeof(cur_share->framesStorage)/sizeof(cur_share->framesStorage[0]);
    uint32 pages = total_size / PAGE_SIZE;
    //cprintf("pages: %u, arr_sz: %d\n", pages, arr_sz);

    for(int i = 0 ; i < pages ; i++, it += PAGE_SIZE) {
    	int32 state = map_frame(myenv->env_page_directory, cur_share->framesStorage[i], it, permm|PERM_USER);
		sm_mark_page(it, myenv, PAGE_MARKED);
    }

    sm_mark_page(ret_add, myenv, PAGE_MARK_START);

	 cur_share->references++;
	 myenv->get_shr_returned_address = (void*)ret_add;
//	 cprintf("(syscall)get_shr_returned_address: %p\n", (void*)ret_add);
//	 cprintf("done done\n");
	 return cur_share->ID;
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//==========================
// [B1] Delete Share Object:
//==========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself

//helper function
struct Share* find_share(void *startVA , struct Env* e){

	acquire_spinlock(&AllShares.shareslock);
	uint32* pg_table = NULL;
	struct FrameInfo* ptr_frame =get_frame_info(e->env_page_directory ,(uint32) startVA , &pg_table);
	struct Share* it= NULL;

    LIST_FOREACH(it , &AllShares.shares_list){
    	int32 sz =sizeof(it->framesStorage)/sizeof(it->framesStorage[0]);
    	for(int i =0 ; i<sz ; i++){
    		if(ptr_frame==it->framesStorage[i]){
    			//cprintf("\nfound frame successfully\n");
    			release_spinlock(&AllShares.shareslock);
    			return it;
    		}
    	}
    }


    release_spinlock(&AllShares.shareslock);
    return  NULL;
}
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - free_share()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("free_share is not implemented yet");
	//Your Code is Here...

	    acquire_spinlock(&AllShares.shareslock);
		LIST_REMOVE(&AllShares.shares_list , ptrShare);
		release_spinlock(&AllShares.shareslock);
		//delete frames
//		int32 sz = sizeof(ptrShare->framesStorage)/sizeof(ptrShare->framesStorage[0]);
//		for(int i = 0 ;i<sz ; i++)
//		{
//			ptrShare->framesStorage[i]=NULL;
//		}
		//delete shared object from kernel heap

		kfree((void *)ptrShare);
		kfree((void *)ptrShare->framesStorage);

}
//========================
// [B2] Free Share Object:
//========================
int freeSharedObject(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - freeSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("freeSharedObject is not implemented yet");
	//Your Code is Here...

	struct Env* myenv =get_cpu_proc();

	//step1 find your share object
	struct Share* cur_share =find_share(startVA , myenv);
	if(cur_share==NULL)
		return 0;

	//step2 unmap frames

	uint32 num_of_frames = (ROUNDUP(cur_share->size, PAGE_SIZE)/PAGE_SIZE);
	uint32 it =(uint32)startVA;

	uint32 *ptr_page_table = NULL;
	struct FrameInfo* frame_info_ptr = get_frame_info(myenv->env_page_directory, it, &ptr_page_table);

	for(int i = 0 ;i<num_of_frames ; i++ , it+=PAGE_SIZE){

		struct FrameInfo* frame_info_ptr = get_frame_info(myenv->env_page_directory, it, &ptr_page_table);
		if(frame_info_ptr != NULL) {

			unmap_frame(myenv->env_page_directory, it);
			sm_mark_page(it ,myenv ,FREE_PAGE);
		}

			int empty = 1;
			for(int i = 0 ;i<1024 ; i++)
			{
				if(ptr_page_table[i]!=0){

					empty=0;
					break;
				}
			}
			if(empty){

				struct FrameInfo* frame_info_ptr = get_frame_info(myenv->env_page_directory, (uint32)ptr_page_table, &ptr_page_table);
				free_frame(frame_info_ptr);
				pd_clear_page_dir_entry(myenv->env_page_directory,(uint32)it);
			}

	}

	//step3 remove page table if empty

//	frame_info_ptr = get_frame_info(myenv->env_page_directory, (uint32)ptr_page_table, &ptr_page_table);
//	if(frame_info_ptr!=NULL){
//		int empty = 1;
//		for(int i = 0 ;i<1024 ; i++)
//		{
//			if(ptr_page_table[i]!=0){
//				empty=0;
//				break;
//			}
//		}
//		if(empty){
//			//cprintf("\npd_clear_page_dir_entry\n");
//			struct FrameInfo* frame_info_ptr = get_frame_info(myenv->env_page_directory, (uint32)ptr_page_table, &ptr_page_table);
//			free_frame(frame_info_ptr);
//			pd_clear_page_dir_entry(myenv->env_page_directory,(uint32)it);
//		}
//	}


    //cprintf("\nref ======= %d\n" , cur_share->references);
	//step4 decrement refrences
	cur_share->references--;

	//step5 last share remove it
	if(cur_share->references==0){
		//cprintf("\nfree_share\n");
		free_share(cur_share);
	}

	//step6 is already completed by unmap and free frames;




return 0;

}
