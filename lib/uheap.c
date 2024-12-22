#include <inc/lib.h>



//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=============================================
// [1] CHANGE THE BREAK LIMIT OF THE USER HEAP:
//=============================================
/*2023*/
void* sbrk(int increment) {
	return (void*) sys_sbrk(increment);
}


//=================================
// [2] ALLOCATE SPACE IN USER HEAP:
//=================================




void* malloc(uint32 size)
{

	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'24.MS2 - #12] [3] USER HEAP [USER SIDE] - malloc()
	// Write your code here, remove the panic and write your code
	//panic("malloc() is not implemented yet...!!");
	//return NULL;
	//Use sys_isUHeapPlacementStrategyFIRSTFIT() and	sys_isUHeapPlacementStrategyBESTFIT()
	//to check the current strategy

	//uint32 va = myEnv->start;

	if (sys_isUHeapPlacementStrategyFIRSTFIT()) {
		if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
			//cprintf("[uheap block alloc]\n");
			return alloc_block_FF(size);
		}else {
//			cprintf("[uheap page alloc]\n");

			//cprintf("result = %x\n", result);
			uint32 total_size = ((size+PAGE_SIZE-1)/PAGE_SIZE)*PAGE_SIZE;
			sys_allocate_user_mem(USER_HEAP_START+(5*PAGE_SIZE), total_size);
			//cprintf("page alloc return address: %p\n", myEnv->returned_address);
			return myEnv->returned_address;
		}
	} else if(sys_isUHeapPlacementStrategyBESTFIT()) { // best fit strategy

	}


	return NULL;


}

//=================================
// [3] FREE SPACE FROM USER HEAP:
//=================================

void free(void* virtual_address) {
	//TODO: [PROJECT'24.MS2 - #14] [3] USER HEAP [USER SIDE] - free()
	// Write your code here, remove the panic and write your code
	//panic("free() is not implemented yet...!!");

	//cprintf("-----------free called with va = %p---------\n", virtual_address);
	if (virtual_address == NULL) {
			panic("ufree() : the provided address is NULL..!!");
			return;
	}



	//	free: whether Blk or Pg allocator
	if ((char*)virtual_address < (char*)(myEnv->sbreak - sizeof(int)) && (char*)virtual_address >= (char*)(myEnv->start + sizeof(int))) {
		// block allocator: call free;
		//cprintf("(user free)free using the dynalloc.\n");
		free_block(virtual_address);
		return;
	} else if((char*)virtual_address >= (char*)(myEnv->hlimit + PAGE_SIZE) && (char*)virtual_address < (char*)USER_HEAP_MAX) {
		// unmark pages and free them
		//cprintf("(uheap free, va = %p)free using the page allocator.\n", virtual_address);
		// get the size of the current allocated pages
		// uint32 page_num = (((uint32)virtual_address) - USER_HEAP_START)/PAGE_SIZE;
		// uint32 pages = 0;

		// delete the allocated range from the list

		// unmark the allocated pages
		// uint32 va = (uint32)virtual_address;
		//		for(uint32 i = 0 ; i < pages ; i++, va += PAGE_SIZE) {
		//			unmark_page(va);
		//		}

		sys_free_user_mem((uint32)virtual_address, (2*PAGE_SIZE));
	} else {
		panic("(user free) the provided address is invalid!\n");
	}
}


//=================================
// [4] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'24.MS2 - #18] [4] SHARED MEMORY [USER SIDE] - smalloc()
	// Write your code here, remove the panic and write your code
	//panic("smalloc() is not implemented yet...!!");

	//pass any adress and handle all cases in kernel side
	//cprintf("flag smalloc 1\n");
	int ret = sys_createSharedObject(sharedVarName , size , isWritable ,(void *)(USER_HEAP_START +(3*PAGE_SIZE)));
	//cprintf("flag smalloc 2\n");
	if(ret==E_NO_SHARE || ret==E_SHARED_MEM_EXISTS )
	{   //cprintf("flag smalloc 3\n");
		return NULL;
	}
	//cprintf("flag smalloc 4\n");
	//cprintf("myEnv return address = %x \n" ,myEnv->returned_address);
	return myEnv->shr_returned_address;
}

//========================================
// [5] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//TODO: [PROJECT'24.MS2 - #20] [4] SHARED MEMORY [USER SIDE] - sget()
	// Write your code here, remove the panic and write your code
	//panic("sget() is not implemented yet...!!");

	int ret = sys_getSharedObject(ownerEnvID, sharedVarName, (void *)(USER_HEAP_START + 3*PAGE_SIZE));
	if(ret == E_SHARED_MEM_NOT_EXISTS || ret == E_NO_SHARE)
	{
		//cprintf("gets returned null\n");
		return NULL;
	}

	//cprintf("the returned sget address: %p\n", myEnv->get_shr_returned_address);
	return  myEnv->get_shr_returned_address;
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_freeSharedObject(...); which switches to the kernel mode,
//	calls freeSharedObject(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the freeSharedObject() function is empty, make sure to implement it.

void sfree(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [USER SIDE] - sfree()
	// Write your code here, remove the panic and write your code
	//panic("sfree() is not implemented yet...!!");
	int32 anyID = 4940;
	sys_freeSharedObject(anyID, virtual_address);

}

//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.

void *realloc(void *virtual_address, uint32 new_size)
{
	//[PROJECT]
	// Write your code here, remove the panic and write your code
	panic("realloc() is not implemented yet...!!");
	return NULL;

}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//

void expand(uint32 newSize)
{
	panic("Not Implemented");

}
void shrink(uint32 newSize)
{
	panic("Not Implemented");

}
void freeHeap(void* virtual_address)
{
	panic("Not Implemented");

}
