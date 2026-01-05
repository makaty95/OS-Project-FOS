#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"

//=================================== OUR LOCKS FOR KERNEL PROTECTION ======================//

#if USE_KHEAP

struct  {
	struct kspinlock kernel_lock;
} kernel_protection;

#endif

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
// TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
// Remember to initialize locks (if any)
void kheap_init()
{
	//==================================================================================
	// DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		kheapPageAllocBreak = kheapPageAllocStart;

		// make all pages as free initially
		memset(kalloc_size, 0, sizeof(kalloc_size));

		// all pages are not mapped to any frames
		memset(frameToPageMapping, -1, sizeof(frameToPageMapping));

#if USE_KHEAP
		// initialize the kernel lock
		init_kspinlock(&(kernel_protection.kernel_lock), "our kernel lock");
#endif

	}
	//==================================================================================
	//==================================================================================
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//================== ============================
int get_page(void *va)
{
	// cprintf("I am a va = %p \n", va);
	int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);

	// mapping the page for later use in kheap_virtual_address
	uint32 phy_a = kheap_physical_address((uint32)va);
	uint32 frame_num = phy_a/PAGE_SIZE;
	uint32 page_num = ((uint32)va)/PAGE_SIZE;
	frameToPageMapping[frame_num] = page_num;

	if (ret < 0)
		panic("get_page() in kern: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void *va)
{
	// unmapping the page
	uint32 phy_a = kheap_physical_address((uint32)va);
	uint32 frame_num = phy_a/PAGE_SIZE;
	uint32 page_num = ((uint32)va) / (uint32)PAGE_SIZE;
	frameToPageMapping[frame_num] = -1;

	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================



/////////////////////////////////////////////////////////////////////////////////

int32 is_kheap_free_page(uint32 page_virtual_address) {

	// page_va is the virtual address within some page , could be with offset doesn't matter.
	uint32 relative_va = page_virtual_address - kheapPageAllocStart;
	uint32 pageNumber = relative_va / PAGE_SIZE;
	return kalloc_size[pageNumber] == 0 ? 1 : 0;

}

void get_alloc_info(uint32 i_size, uint32 *o_num_pages_to_alloc, uint32 *o_new_size)
{
	*o_num_pages_to_alloc = (i_size + PAGE_SIZE - 1) / PAGE_SIZE; // taking ceil
	*o_new_size = (*o_num_pages_to_alloc) * PAGE_SIZE;
}

void* get_cf_alloc_address(uint32 size) {


	uint32 mx_va = 0;
	uint32 mx_size = 0;

	int currentSpace = 0;
	uint32 startAddress = 0;
	int begin = 1;

	// check for space before the break
	for (uint32 va = kheapPageAllocStart; va < kheapPageAllocBreak; va += PAGE_SIZE) {

		if(is_kheap_free_page(va)) { // if the page is free
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

	// if exist wf block
	if(mx_size != 0 && mx_size >= size) return (void*)mx_va;

	// if break extend is required , return null;
	return NULL;




	return 0;

}



// this function will do all the cases
void* alloc_all_cases(uint32 new_size, uint32 num_pages_to_alloc)
{

	void* add_ret = get_cf_alloc_address(new_size);
	//cprintf("\n[Our logs] computed cf address: %p", add_ret);

	if(add_ret != NULL) {
		// allocate 3ady
		for (uint32 page = 0, cnt = num_pages_to_alloc; page < num_pages_to_alloc; ++page, --cnt) {

			uint32 va = (((uint32)add_ret) + (page * PAGE_SIZE));

			// allocating the page
			get_page((void*)va);


			// save the size of the pages for the later free
			uint32 page_num = (va - kheapPageAllocStart) / PAGE_SIZE;
			kalloc_size[page_num] = cnt;

		}

		return (void*) add_ret;

	} else if (kheapPageAllocBreak <= (KERNEL_HEAP_MAX - new_size)) { // if we can extend.
		uint32 old_va = kheapPageAllocBreak;
		kheapPageAllocBreak += new_size;
		for (uint32 page = 0, cnt = num_pages_to_alloc; page < num_pages_to_alloc; ++page, --cnt)
		{
			uint32 va = (old_va + (page * (uint32)PAGE_SIZE));

			// allocating the page.
			get_page((void*)va);


			// save the size of the pages for the later free
			uint32 page_num = (va - kheapPageAllocStart) / PAGE_SIZE;
			kalloc_size[page_num] = cnt;
		}

		return (void*)old_va;
	}

	return NULL;


}


void *k_alloc_page(uint32 size)
{
	uint32 num_pages_to_alloc, new_size;
	get_alloc_info(size, &num_pages_to_alloc, &new_size); // calculate new_size to alloc, and number of pages to alloc
	//cprintf("\n[Our logs] pages to allocate: %d", num_pages_to_alloc);

	return alloc_all_cases(new_size, num_pages_to_alloc);

}


void *kmalloc(unsigned int size)
{
	// TODO: [PROJECT'25.GM#2] KERNEL HEAP - #1 kmalloc
	// Your code is here
	// Comment the following line
	// kpanic_into_prompt("kmalloc() is not implemented yet...!!");

#if USE_KHEAP
	// acquire lock
	acquire_kspinlock(&(kernel_protection.kernel_lock));


	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE)
	{
		void * va = alloc_block(size);
		//cprintf("\n[Our logs] allocate block with size = %d at %p\n", size, va);

		// release lock
		release_kspinlock(&(kernel_protection.kernel_lock));
		return va;
	}
	else
	{
		void * va = k_alloc_page(size);

		//cprintf("\n[Our logs] allocate page with size = %d at %p\n", size, va);
		release_kspinlock(&(kernel_protection.kernel_lock));
		return va;

	}

#else
	return NULL;
#endif
	// TODO: [PROJECT'25.BONUS#3] FAST PAGE ALLOCATOR
}




//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void kfree(void *virtual_address)
{
	// TODO: [PROJECT'25.GM#2] KERNEL HEAP - #2 kfree
	// Your code is here
	// Comment the following line
	//  panic("kfree() is not implemented yet...!!");
	// panic("kfree() have invalid va !!");

	if (virtual_address == NULL) {
		panic("[Our logs] The provided address for kfree is null");
		return;
	}


	int isInsideBlockAllocator =
			(uint32)virtual_address < (uint32)(kheapPageAllocStart - PAGE_SIZE)
			&& (uint32)virtual_address >= (uint32)KERNEL_HEAP_START;

	int isInsidePageAllocator =
			(uint32)virtual_address >= (uint32)(kheapPageAllocStart)
			&& (uint32)virtual_address < (uint32)KERNEL_HEAP_MAX;


	if(isInsidePageAllocator) { // check if the va is inside page allocator
		// page allocated address -> free in the page allocator

		//cprintf("[Our logs] kfree called with page allocator address = %p \n", virtual_address);

		// get the number of pages which was allocated with the kmalloc
		int page_num = (((uint32)virtual_address) - kheapPageAllocStart) / PAGE_SIZE;
		uint32 num_of_pages_to_free = kalloc_size[page_num];


		//cprintf("[Our logs] kfree, num of pages to free =  = %d \n", num_of_pages_to_free);

		uint32 it = (uint32)virtual_address;
		for(uint32 i = 0; i < num_of_pages_to_free; i++, it += PAGE_SIZE) {
			//-----------------------------return the page--------------------------------
			return_page((void*)it);
			//-------------------------------------------------------------------------------------

			//---------------------------update the allocated_pages_num array--------------------------------
			uint32 pageNumber = (it - kheapPageAllocStart) / (uint32)PAGE_SIZE;
			kalloc_size[pageNumber] = 0; // 0 means this page is now free
			//-----------------------------------------------------------------------------------------------

		}


		//----------move the break down if exist some free space before it-------------------------------
		uint32 ptr = kheapPageAllocBreak - PAGE_SIZE;
		while(ptr >= (kheapPageAllocStart) && is_kheap_free_page(ptr)) {
			kheapPageAllocBreak -= PAGE_SIZE;
			ptr -= PAGE_SIZE;
		}
		//-----------------------------------------------------------------------------------------------

		return;

	} else if (isInsideBlockAllocator) {
		// block allocated address -> we should call free_block;
		//cprintf("[Our logs] kfree called with block allocator address = %p \n", virtual_address);
		free_block(virtual_address);
		return;
	}


	panic("[Our logs] The provided address in kfree is not in block allocator neither page allocator.");

}

//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address)
{
	// TODO: [PROJECT'25.GM#2] KERNEL HEAP - #3 kheap_virtual_address
	// Your code is here
	// Comment the following line
	//  panic("kheap_virtual_address() is not implemented yet...!!");

	uint32 offset = physical_address%PAGE_SIZE;
	uint32 frame_num = physical_address/PAGE_SIZE;
	if(frameToPageMapping[frame_num] == -1) return 0;
	return frameToPageMapping[frame_num] * PAGE_SIZE + offset;

}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{
	// TODO: [PROJECT'25.GM#2] KERNEL HEAP - #4 kheap_physical_address
	// Your code is here
	// Comment the following line
	//  panic("kheap_physical_address() is not implemented yet...!!");

	uint32 *ptr_page_table = NULL;
	struct FrameInfo *ptr_frame_info = get_frame_info(ptr_page_directory, (uint32)virtual_address, &ptr_page_table);
	if(ptr_frame_info == NULL) { // there is no frame mapped for this address.
		//cprintf("[Our logs] There is no frame for this address. :(\n");
		return 0;
	}


	uint32 offset_mask = ((1 << 12) - 1);
	uint32 offset = (offset_mask&virtual_address);
	uint32 frame_num = ptr_frame_info - frames_info;
	uint32 physical_address = (frame_num * PAGE_SIZE) + offset;
	return physical_address;

}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

extern __inline__ uint32 get_block_size(void *va);



void extend_more_pages(uint32 virtual_address, uint32 ex_pages) {

	uint32 page_num = virtual_address / PAGE_SIZE;
	uint32 pages = kalloc_size[page_num];
	uint32 newPages = ex_pages + pages;

	// update old allocated pages info
	int K = ex_pages + pages;
	for (int idx = 0; idx < pages; idx++) {
		uint32 va =  (PAGE_SIZE * idx) + virtual_address;
		uint32 cur_page_number = (va - kheapPageAllocStart) / PAGE_SIZE;
		kalloc_size[cur_page_number] = K; --K;
	}

	// allocate the new pages and update their info
	uint32 address_ptr = virtual_address + (pages * PAGE_SIZE);
	for (int i = 0; i < ex_pages; ++i, address_ptr += PAGE_SIZE) {

		// allocating the page
		get_page((void*) address_ptr);

		// save the size of the pages for the later free
		uint32 page_num = (address_ptr - kheapPageAllocStart) / PAGE_SIZE;
		kalloc_size[page_num] = K;
		--K;

	}

}



void *realloc_page(void *virtual_address, uint32 new_size) {



	// get old_size(size of allocated_pages for this virtual_address)
	uint32 page_num = ((uint32) virtual_address) / (uint32) PAGE_SIZE; // get page index
	uint32 allocated_pages_number = kalloc_size[page_num];
	uint32 old_size = allocated_pages_number * (uint32) PAGE_SIZE;

	// check this va is allocated or not
	uint32 new_total_size = ((new_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE; //ROUND_UP

	// find size_difference
	int32 size_difference = (int32) new_total_size - (int32) old_size;

	// 3 cases
	if (size_difference == 0)
		return virtual_address;
	else if (size_difference > 0) {

		// We want a bigger size than the old one

		uint32 num_of_new_pages =
				((size_difference + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
		uint32 it = (uint32) virtual_address + old_size;


		uint32 page_num = ((uint32) it) / (uint32) PAGE_SIZE;
		while (frameToPageMapping[page_num] == -1) {

			num_of_new_pages--;
			if (num_of_new_pages == 0) {

				// Extend pages
				extend_more_pages((uint32) virtual_address, ((size_difference + PAGE_SIZE - 1) / PAGE_SIZE)* PAGE_SIZE);
				return virtual_address; //return same address with new size
			}

			// update current iterator
			it += PAGE_SIZE;
			page_num = ((uint32) it) / (uint32) PAGE_SIZE;
		}

		// We should search for new allocation
		// If there is a new fit size:
		// 		move the data from the old space to the new one
		// 		free the old space
		// If not then return the same address

		void* new_virtual_address = kmalloc(new_size);
		if (virtual_address != NULL) {

			// transfer data from old to new space
			transfer_data((uint32)virtual_address, (uint32)new_virtual_address, allocated_pages_number * PAGE_SIZE);

			// free old space
			kfree(virtual_address);

			return new_virtual_address;
		} else {
			return NULL;
		}

	} else {

		size_difference *= -1; // absolute value

		uint32 upper_pages = ((size_difference + PAGE_SIZE - 1) / PAGE_SIZE);
		uint32 lower_pages = page_num - upper_pages;
		uint32 addressNewFreePages = (uint32) virtual_address + new_total_size;

		// update old allocated pages info with smaller size
		for (int i = 0, cnt = lower_pages; i < lower_pages; i++, cnt--) {
			uint32 va = (uint32)virtual_address + (i * PAGE_SIZE);
			uint32 page_num = (va - kheapPageAllocStart) / PAGE_SIZE;
			kalloc_size[page_num] = cnt;

		}

		kfree((void*) addressNewFreePages);

		return virtual_address; //return same address with new size
	}

}

void *krealloc(void *virtual_address, uint32 new_size) {


	// TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	// Your code is here
	// Comment the following line
	//panic("krealloc() is not implemented yet...!!");

	int case1 = virtual_address != NULL && new_size == 0;
	int case2 = virtual_address == NULL && new_size > 0;
	int case3 = virtual_address == NULL && new_size == 0;

	// Initial cases
	if (case1) {
		kfree(virtual_address);
		return NULL;
	}

	if (case2) return kmalloc(new_size);

	if (case3) return NULL;

	int insideBlockAlloc = (uint32) virtual_address < (uint32) dynAllocEnd &&
			(uint32) virtual_address >= (uint32) (dynAllocStart);

	int insidePageAlloc = (uint32) virtual_address >= (uint32) (dynAllocEnd + PAGE_SIZE) &&
			(uint32)virtual_address < (uint32)KERNEL_HEAP_MAX;

	// check if the VA inside block allocation area
	if (insideBlockAlloc) return realloc_block(virtual_address, new_size);

	// check if the VA inside page allocation area
	if (insidePageAlloc) return realloc_page(virtual_address, new_size);


	return NULL;

}
