#include <inc/lib.h>



//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//


//==========================OUR HELPER DIFINITIONS==========================>
// this will be used to mark pages in user side
uint32 alloc_state[((uint32)USER_HEAP_MAX - (uint32)USER_HEAP_START) / (uint32)PAGE_SIZE];

int sget_ret_id;

// states of pages in uheap
#define FREE_PAGE 	0
#define MARKED_PAGE 1
#define RSTART_PAGE 2

//==========================================================================>


//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
int __firstTimeFlag = 1;
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;

		//cprintf("init uheapPageAllocStart with %p \n", uheapPageAllocStart);

		__firstTimeFlag = 0;
	}
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//



//====================================USER HEAP UTIL================================//
void uh_get_alloc_info(uint32 i_size, uint32 *o_num_pages_to_alloc, uint32 *o_new_size)
{
	*o_num_pages_to_alloc = (i_size + PAGE_SIZE - 1) / PAGE_SIZE; // round up
	*o_new_size = (*o_num_pages_to_alloc) * PAGE_SIZE;
}


int check_page_status(uint32 page_va, int expected) {
	int idx = (page_va - USER_HEAP_START)/PAGE_SIZE;
	return alloc_state[idx] == expected;
}

void set_page_status(uint32 page_va, int wanted) {
	int idx = (page_va - USER_HEAP_START)/PAGE_SIZE;
	alloc_state[idx] = wanted;
}


void* uh_get_cf_alloc_address(uint32 size) {

//	cprintf("uh_get_cf_alloc_address called \n", uheapPageAllocStart, uheapPageAllocBreak);
//	cprintf("uheapPageAllocStart = %d, uheapPageAllocBreak = %d\n", uheapPageAllocStart, uheapPageAllocBreak);
//	cprintf("uheapPageAllocStart = %d, uheapPageAllocBreak = %d\n", uheapPageAllocStart, uheapPageAllocBreak);

	uint32 mx_va = 0;
	uint32 mx_size = 0;

	int currentSpace = 0;
	uint32 startAddress = 0;
	int begin = 1;

	// check for space before the break
	for (uint32 va = uheapPageAllocStart; va < uheapPageAllocBreak; va += PAGE_SIZE) {

		if(check_page_status(va, FREE_PAGE)) { // if the page is free
			if(begin) {
				startAddress = va;
				begin = 0;
			}
			currentSpace += PAGE_SIZE;
		} else {
			if(currentSpace == size) { // if there is exact return in immediately
				return (void*)startAddress;
			} else if(mx_size < currentSpace) {
				mx_size = currentSpace;
				mx_va = startAddress;
			}
			currentSpace = 0;
			startAddress = 0;
			begin = 1;

		}
	}


	if(currentSpace != 0) {
		if(currentSpace == size) { // if there is exact return in immediately
			return (void*)startAddress;
		} else if(mx_size < currentSpace) {
			mx_size = currentSpace;
			mx_va = startAddress;
		}
		begin = 1;
		currentSpace = 0;
	}

	// if exist WF block
	if(mx_size != 0 && mx_size >= size) return (void*)mx_va;

	// if break extend is required , return null;
	return NULL;

}

// for malloc
void* uh_alloc_all_cases(uint32 new_size, uint32 num_pages_to_alloc) {
	void* add_ret = uh_get_cf_alloc_address(new_size);
	//cprintf("\n[Our logs (uheap)] computed cf address: %p, size = %d\n", add_ret, new_size);

	if(add_ret != NULL) {
		//cprintf("section1 \n");

		// mark all pages at user side
		for (uint32 page = 0; page < num_pages_to_alloc; ++page) {
			uint32 va = ((uint32)add_ret + (page * (uint32)PAGE_SIZE));
			set_page_status(va, MARKED_PAGE);
		}

		// mark 3ady
		sys_allocate_user_mem((uint32)add_ret, new_size);

		// mark the start of the address
		set_page_status((uint32)add_ret, RSTART_PAGE);

		return add_ret;

	} else if (uheapPageAllocBreak <= (USER_HEAP_MAX - new_size)) { // if we can extend.
		//cprintf("section2 \n");
		uint32 old_va = uheapPageAllocBreak;
		uheapPageAllocBreak += new_size;
		//cprintf("num_pages_to_alloc = %d\n", num_pages_to_alloc);

		// mark all pages at user side
		for (uint32 page = 0; page < num_pages_to_alloc; ++page) {
			uint32 va = (old_va + (page * (uint32)PAGE_SIZE));
			set_page_status(va, MARKED_PAGE);
		}

		// mark 3ady in the kernel side
		sys_allocate_user_mem((uint32)old_va, new_size);

		// mark the start of the address
		set_page_status(old_va, RSTART_PAGE);

		return (void*)old_va;
	}

	return NULL;

}


// for smalloc
void* suh_alloc_all_cases(uint32 new_size, uint32 num_pages_to_alloc, char *sharedVarName, uint8 isWritable) {

	//cprintf("suh_alloc_all_cases called \n");
	void* add_ret = uh_get_cf_alloc_address(new_size);

	//cprintf("FLAG 1\n");

	if(add_ret != NULL) {
		//cprintf("section1 \n");

		// mark 3ady
		int ret = sys_create_shared_object(sharedVarName, new_size, isWritable, (void*)add_ret);

		//cprintf("FLAG 2\n");
		if(ret == E_SHARED_MEM_EXISTS) {
			//cprintf("ret = E_SHARED_MEM_EXISTS, returning null \n");
			return NULL;
		}

		if(ret == E_NO_SHARE) {
			return NULL;
		}

		// mark all pages at user side
		for (uint32 page = 0; page < num_pages_to_alloc; ++page) {
			uint32 va = ((uint32)add_ret + (page * (uint32)PAGE_SIZE));
			set_page_status(va, MARKED_PAGE);
		}


		set_page_status((uint32)add_ret, RSTART_PAGE);

		return add_ret;

	} else if (uheapPageAllocBreak <= (USER_HEAP_MAX - new_size)) { // if we can extend.
		//cprintf("section2 \n");
		uint32 old_va = uheapPageAllocBreak;
		uheapPageAllocBreak += new_size;
		//cprintf("num_pages_to_alloc = %d\n", num_pages_to_alloc);

		int ret = sys_create_shared_object(sharedVarName, new_size, isWritable, (void*)old_va);
		if(ret == E_SHARED_MEM_EXISTS) {
			//cprintf("ret = E_SHARED_MEM_EXISTS, returning null \n");
			return NULL;
		}

		// mark all pages at user side
		for (uint32 page = 0; page < num_pages_to_alloc; ++page) {
			uint32 va = (old_va + (page * (uint32)PAGE_SIZE));
			set_page_status(va, MARKED_PAGE);
		}


		set_page_status(old_va, RSTART_PAGE);

		return (void*)old_va;
	}

	//cprintf("NO mem, returning null\n");
	return NULL;

}


// for sget
void* sguh_alloc_all_cases(uint32 new_size, uint32 num_pages_to_alloc, int32 ownerEnvID, char *sharedVarName) {
	void* add_ret = uh_get_cf_alloc_address(new_size);

	if(add_ret != NULL) {
		//cprintf("section1 \n");

		// mark all pages at user side
		for (uint32 page = 0; page < num_pages_to_alloc; ++page) {
			uint32 va = ((uint32)add_ret + (page * (uint32)PAGE_SIZE));
			set_page_status(va, MARKED_PAGE);
		}

		int id = sys_get_shared_object(ownerEnvID, sharedVarName, (void*)add_ret);
		sget_ret_id = id;

		set_page_status((uint32)add_ret, RSTART_PAGE);

		return (void*)add_ret;

	} else if (uheapPageAllocBreak <= (USER_HEAP_MAX - new_size)) { // if we can extend.
		//cprintf("section2 \n");
		uint32 old_va = uheapPageAllocBreak;
		uheapPageAllocBreak += new_size;
		//cprintf("num_pages_to_alloc = %d\n", num_pages_to_alloc);

		// mark all pages at user side
		for (uint32 page = 0; page < num_pages_to_alloc; ++page) {
			uint32 va = (old_va + (page * (uint32)PAGE_SIZE));
			set_page_status(va, MARKED_PAGE);
		}

		int id = sys_get_shared_object(ownerEnvID, sharedVarName, (void*)old_va);
		sget_ret_id = id;

		set_page_status(old_va, RSTART_PAGE);

		return (void*)old_va;
	}

	return NULL;

}


//=================================
// [1] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'25.IM#2] USER HEAP - #1 malloc
	//Your code is here
	//Comment the following line
	//panic("malloc() is not implemented yet...!!");

	//cprintf("malloc called with size = %d\n", size);
	//	cprintf(" malloc uheapPageAllocStart = %p, uheapPageAllocBreak = %p\n", uheapPageAllocStart, uheapPageAllocBreak);
	//	cprintf("malloc uheapPageAllocStart = %p, uheapPageAllocBreak = %p\n", uheapPageAllocStart, uheapPageAllocBreak);

	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
		return alloc_block(size);
	} else {

		uint32 num_pages_to_alloc, new_size;
		uh_get_alloc_info(size, &num_pages_to_alloc, &new_size);

		uint32 vaa = (uint32) uh_alloc_all_cases(new_size, num_pages_to_alloc);
		return (void*) vaa;

	}

}




//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{
	//TODO: [PROJECT'25.IM#2] USER HEAP - #3 free
	//Your code is here
	//Comment the following line
	//panic("free() is not implemented yet...!!");

	if (virtual_address == NULL) {
			panic("ufree() : the provided address is NULL..!!");
			return;
	}

	int isInsideBlockAllocator =
			(uint32)virtual_address < (uint32)(uheapPageAllocStart - PAGE_SIZE)
			&& (uint32)virtual_address >= (uint32)USER_HEAP_START;

	int isInsidePageAllocator =
			(uint32)virtual_address >= (uint32)(uheapPageAllocStart)
			&& (uint32)virtual_address < (uint32)USER_HEAP_MAX;


	if (isInsideBlockAllocator) {
		// block allocated address -> we should call free_block;
		free_block(virtual_address);
		return;
	}

	if (isInsidePageAllocator) { // check if the va is inside page allocator

		int allocated_size = 0;
		uint32 va = (uint32)virtual_address;
		while((va < USER_HEAP_MAX && va >= uheapPageAllocStart) &&
					((!check_page_status(va, RSTART_PAGE) && !check_page_status(va, FREE_PAGE)) || va == (uint32)virtual_address)) {

			va += PAGE_SIZE;
			allocated_size += PAGE_SIZE;
		}

		int num_of_pages = (allocated_size + PAGE_SIZE - 1) / PAGE_SIZE;

		// unmark all pages in user side
		for (uint32 page = 0; page < num_of_pages; ++page) {
			uint32 va = ((uint32)virtual_address + (page * (uint32)PAGE_SIZE));

			// unmark the page
			set_page_status(va, FREE_PAGE);
		}

		// unmark all pages in the kernel side & remove the WS elements
		sys_free_user_mem((uint32)virtual_address, allocated_size);



		//----------move the break down if exist some free space before it-------------------------------
		uint32 ptr = uheapPageAllocBreak - PAGE_SIZE;
		while(ptr >= (uheapPageAllocStart) && check_page_status(ptr, FREE_PAGE)) {
			uheapPageAllocBreak -= PAGE_SIZE;
			ptr -= PAGE_SIZE;
		}
		//-----------------------------------------------------------------------------------------------


		return;
	}


	panic("[Our logs] The provided address in kfree is not in block allocator neither page allocator.");





}

//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
	//Your code is here
	//Comment the following line
	//panic("smalloc() is not implemented yet...!!");

	//cprintf("smalloc called size = %d, sharedVarName = %s \n", size, sharedVarName);

	uint32 num_pages_to_alloc, new_size;
	uh_get_alloc_info(size, &num_pages_to_alloc, &new_size);

	void* ret = suh_alloc_all_cases(new_size, num_pages_to_alloc, sharedVarName, isWritable);;
	//cprintf("returned address in smalloc = %p \n", (uint32)ret);

	return ret;

}

//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #4 sget
	//Your code is here
	//Comment the following line
	//panic("sget() is not implemented yet...!!");

	uint32 size = sys_size_of_shared_object(ownerEnvID, sharedVarName);
	if(size == (uint32)NULL) return NULL;

	uint32 num_pages_to_alloc, new_size;
	uh_get_alloc_info(size, &num_pages_to_alloc, &new_size);


	void* vaa = sguh_alloc_all_cases(new_size, num_pages_to_alloc, ownerEnvID, sharedVarName);

	if(sget_ret_id == E_SHARED_MEM_NOT_EXISTS) return NULL;

	return vaa;


}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


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
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
