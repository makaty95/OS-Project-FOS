#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"


#if USE_KHEAP
	struct
	{

		struct spinlock kheaplock;
	}kheap_protection;
	void kheap_lock_init();

#else

#endif

// helper functions //

void kheap_lock_init()
{
#if USE_KHEAP
	init_spinlock(&kheap_protection.kheaplock, "kheap lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}
int synced_map_frame(uint32 *ptr_page_directory, struct FrameInfo *ptr_frame_info, uint32 virtual_address, int perm) {

	// physical address for the frame
	uint32 physical_address = to_physical_address(ptr_frame_info);

	// regular maping for the frame
	//cprintf(">> mapping va: %x to phya: %x\n", virtual_address, physical_address);
	int status = map_frame(ptr_page_directory, ptr_frame_info, virtual_address, perm);

	// frame number in physical memory
	uint32 frame_num = physical_address/PAGE_SIZE;

	// saving virtual address.
	va_page_num[frame_num] = (virtual_address/PAGE_SIZE);

	return status;

}

void synced_unmap_frame(uint32 *ptr_page_directory, uint32 virtual_address) {

	// physical address for the frame mapped the the given va
	uint32 physical_address = kheap_physical_address(virtual_address);

	unmap_frame(ptr_page_directory, virtual_address);

	// frame number in physical memory
	uint32 frame_num = physical_address/PAGE_SIZE;

	// removing the mapping ... -1 means the frame is not mapped to any page.
	va_page_num[frame_num] = -1;

}


//----------------------------/

//Initialize the dynamic allocator of kernel heap with the given start address, size & limit
//All pages in the given range should be allocated
//Remember: call the initialize_dynamic_allocator(..) to complete the initialization
//Return:
//	On success: 0
//	Otherwise (if no memory OR initial size exceed the given limit): PANIC
int initialize_kheap_dynamic_allocator(uint32 daStart, uint32 initSizeToAllocate, uint32 daLimit)
{
	//cprintf("---------------initialize_kheap_dynamic_allocator called---------------\n");
	//TODO: [PROJECT'24.MS2 - #01] [1] KERNEL HEAP - initialize_kheap_dynamic_allocator
	// Write your code here, remove the panic and write your code
	//panic("initialize_kheap_dynamic_allocator() is not implemented yet...!!");




	// initialize allocated_pages_num to 0s means no pages allocated starting from any page.
	// initialize va_page_num to -1 means no mapping yet.
	memset(allocated_pages_num, 0, sizeof(allocated_pages_num));
	memset(va_page_num, -1, sizeof(va_page_num));
	kheap_lock_init();




    //step1 limits of kheap
	start_kernal_heap =  daStart;
	segment_break = start_kernal_heap + initSizeToAllocate;
	hard_limit = daLimit;
	pgalloc_last = (hard_limit + PAGE_SIZE);

	if(daStart + initSizeToAllocate > hard_limit)
	{
		panic("initialize_kheap_dynamic_allocator() exceeds hard limit..!!");
	}


	//2.2 allocate all pages and map
	uint32 number_of_pages = ROUNDUP(initSizeToAllocate, PAGE_SIZE) / PAGE_SIZE; // 1007617 = (daStart - KERNEL_HEAP_MAX) ,,, (daStart + initSizeToAllocate )
	//print
//	cprintf("dastart : %x \n", daStart);
//	cprintf("daLimit : %x \n", hard_limit);
//	cprintf("number_of_pages : %d \n", number_of_pages);
	//page address
    uint32 va = daStart;
	for(uint32 i = 0 ; i < number_of_pages ; i++, va = va + PAGE_SIZE)
	{
		uint32 *check_page_table = NULL;
		int32 r = get_page_table(ptr_page_directory , va , &check_page_table);
		if(r != TABLE_IN_MEMORY) {
			check_page_table = (uint32*)create_page_table(ptr_page_directory , va);
		}

		struct FrameInfo *ptr_frame_info;
        int ret = allocate_frame(&ptr_frame_info);

        if(ret == E_NO_MEM){
        	panic("initialize_kheap_dynamic_allocator() no memory ..!!");
        }

        int rrr = synced_map_frame(ptr_page_directory , ptr_frame_info , va , PERM_PRESENT | PERM_WRITEABLE);

        //map_frame(ptr_page_directory , ptr_frame_info , va , PERM_WRITEABLE);


        if(rrr != 0){
			cprintf("count : %d , rrr: %d", i, ret);
			panic("initialize_kheap_dynamic_allocator() not mapped ..!!");
		}
        va = va + PAGE_SIZE;
	}

	initialize_dynamic_allocator(start_kernal_heap , initSizeToAllocate);
	return 0;
}



void* sbrk(int numOfPages)
{
	/* numOfPages > 0: move the segment break of the kernel to increase the size of its heap by the given numOfPages,
	 * 				you should allocate pages and map them into the kernel virtual address space,
	 * 				and returns the address of the previous break (i.e. the beginning of newly mapped memory).
	 * numOfPages = 0: just return the current position of the segment break
	 *
	 * NOTES:
	 * 	1) Allocating additional pages for a kernel dynamic allocator will fail if the free frames are exhausted
	 * 		or the break exceed the limit of the dynamic allocator. If sbrk fails, return -1
	 */

	//MS2: COMMENT THIS LINE BEFORE START CODING==========
	//return (void*)-1 ;
	//====================================================

	//TODO: [PROJECT'24.MS2 - #02] [1] KERNEL HEAP - sbrk
	// Write your code here, remove the panic and write your code
	//panic("sbrk() is not implemented yet...!!");

	if(numOfPages == 0) return (void*)segment_break; // edge case

	uint32 increasing = numOfPages * PAGE_SIZE; // size to be allocated
	uint32 last_address = segment_break + increasing;
	uint32 old_sbrk = segment_break;

	if(last_address > hard_limit)
	{
		return (void *)E_UNSPECIFIED;
	}



	//page address
	uint32 va = segment_break;

	for(int32 i = 0 ; i < numOfPages ; i++,  va += PAGE_SIZE)
	{

		// checking for the page table
		uint32 *ptr_table = NULL;
		int pg_status = get_page_table(ptr_page_directory , va , &ptr_table);
		if(pg_status == TABLE_NOT_EXIST) {
			ptr_table = (uint32*)create_page_table(ptr_page_directory , va);
		}

		struct FrameInfo *ptr_frame_info = get_frame_info(ptr_page_directory , va , &ptr_table);
		if(ptr_frame_info != NULL){
			// this shouldn't execute at all
			//cprintf("LOL\n");
			continue;
		}

        uint32 ret = allocate_frame(&ptr_frame_info);
        if(ret == E_NO_MEM) {
        	return (void *)E_UNSPECIFIED;
        }

        synced_map_frame(ptr_page_directory , ptr_frame_info , va ,  PERM_WRITEABLE);
        va += PAGE_SIZE;
	}

	//cprintf("->>>%d,  ->>%d\n", (void*)(va - sizeof(int))-(void*)(segment_break - sizeof(int)), increasing);


	struct blockElement * newBlock = (struct blockElement *)(segment_break); // assign old brk to a block
	segment_break = va;
	int32 *end_last_page = (int32 *)(va - sizeof(int));

	*end_last_page = 1;


	set_block_data((void*)newBlock , increasing , 1);

	// you have to set bounds first before calling free as it checks for them
	end_bound = (void*) end_last_page;


	segment_break = last_address;

	return (void*)old_sbrk;
}

//TODO: [PROJECT'24.MS2 - BONUS#2] [1] KERNEL HEAP - Fast Page Allocator

int32 khis_free_page(uint32 page_va) {

	// page_va is the virtual address within some page , could be with offset doesn't matter.
	uint32 page_num = page_va / PAGE_SIZE;
	return (allocated_pages_num[page_num] == 0);
}


uint32 get_pgallocation_address(uint32 size, uint32 start, uint32* page_directory, uint32 pg_alloc_last) {
	//cprintf("%d pages needed\n", pages_needed);

	uint32 it = start;
	uint32 curSize = 0;
	uint32 pgalloc_ptr = 0;

	for (; curSize < size && it < pg_alloc_last; it += PAGE_SIZE) {

		if (khis_free_page(it)) { // if free page
			//cprintf("[-]free_Page\n");
			if(curSize == 0) {
				pgalloc_ptr = it;
			}
			curSize += PAGE_SIZE;

		}else {
			//cprintf("[-]occupied_Page\n");
			curSize = 0;
			pgalloc_ptr = 0;
		}
	}



	// if exist some free pages before pgalloc_last which could be used.
	if(pgalloc_ptr != 0 && curSize >= size) {
		//cprintf("[-]returning pgalloc_ptr -> pages found before pgalloc_last\n");
		return pgalloc_ptr;
	}
//
//	cprintf("[-]returning pgalloc_last\n");
	return pg_alloc_last;


}


void* kmalloc(unsigned int size)
{
    //TODO: [PROJECT'24.MS2 - #03] [1] KERNEL HEAP - kmalloc
    // Write your code here, remove the panic and write your code
	//panic("kmalloc() is not implemented yet...!!");

//	  1. validate size
//    2. create vp
//    3. allocate physical frame
//    4. map virtual to physical

       acquire_spinlock(&kheap_protection.kheaplock);
		//	SIZE VALIDATION:
		if(size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
			void* ret =  alloc_block_FF(size);
			//cprintf("[block allocator]the returned kmalloc address: %p\n", ret);
			release_spinlock(&kheap_protection.kheaplock);
			return ret;
		}

		// total size is now multiple of page_size
		uint32 total_size = ((size + PAGE_SIZE - 1)/PAGE_SIZE) * PAGE_SIZE;

		//cprintf("----------kmalloc page_alloc called with size: %d, %d pages----------\n", total_size, total_size/PAGE_SIZE);

		uint32 it = get_pgallocation_address((uint32)total_size, hard_limit+PAGE_SIZE, ptr_page_directory, pgalloc_last);
		// the problem here................
		// in returning null

//		cprintf("called size: %u, total_size: %u, it: %p\n", size, total_size, (void*)it);
//		cprintf("pgalloc_last: %u\n", pgalloc_last);
//		cprintf("KHM: %u\n", (uint32)KERNEL_HEAP_MAX);
//		cprintf("wanted pages: %u\n", total_size/PAGE_SIZE);
//		cprintf("MX page: %u\n", (KERNEL_HEAP_MAX/PAGE_SIZE));
//		cprintf("last_ptr page: %u\n", (pgalloc_last/PAGE_SIZE));
//		cprintf("av pages: %u\n", (KERNEL_HEAP_MAX/PAGE_SIZE)-(pgalloc_last/PAGE_SIZE));


		//		ALLOCATE  & MAP
		if (it == pgalloc_last) {
//			cprintf("inside\n");
			if (total_size > (KERNEL_HEAP_MAX - pgalloc_last)) {
				//cprintf("[kmalloc ret = null]no enough memory available\n");
				release_spinlock(&kheap_protection.kheaplock);
				return NULL;
			}
			pgalloc_last += total_size;
		}

		uint32 result = it;
		uint32 num_pages = total_size/PAGE_SIZE;
		for (int i = 0, cnt = num_pages; i < num_pages; i++, it += PAGE_SIZE, cnt--) {
			//cprintf("flag2\n");

			struct FrameInfo *newFrame = NULL;
			int state = allocate_frame(&newFrame);
			if (state == E_NO_MEM) {
				release_spinlock(&kheap_protection.kheaplock);
				return NULL;
			}


			state = synced_map_frame(ptr_page_directory, newFrame, it, PERM_WRITEABLE);

			if (state == E_NO_MEM) {		// just to make sure.
				return NULL;
			}

			// saving the number of allocated pages.
			uint32 page_num = ((uint32)it) / (uint32)PAGE_SIZE;
			allocated_pages_num[page_num] = cnt;

		}


//		cprintf("[page allocator]the returned kmalloc address: %p\n", result);
		release_spinlock(&kheap_protection.kheaplock);
		return (void*)result;
}

void kfree(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #04] [1] KERNEL HEAP - kfree
	// Write your code here, remove the panic and write your code

	//panic("kfree() is not implemented yet...!!");

	//	1. validate arguments
	//	2. if Blk allocator | Call free
	//	3. if Pg allocator  | unmap & remove all the related frames.

	////	validate:
	if (virtual_address == NULL) {
		panic("kfree() : the provided address is NULL..!!");
		return;
	}

	//	free: whether Blk or Pg allocator
	if ((char*)virtual_address < (char*)segment_break && (char*)virtual_address >= (char*)start_kernal_heap) {
		// block allocator: call free;
//		cprintf("free using the dynalloc.\n");
		free_block(virtual_address);
		return;
	} else if((char*)virtual_address >= (char*)(hard_limit + PAGE_SIZE) && (char*)virtual_address < (char*)KERNEL_HEAP_MAX) {
		// page allocator free
		int page_num = ((uint32)virtual_address) / (uint32)PAGE_SIZE;
		uint32 num_of_pages = allocated_pages_num[page_num];

		// if no pages allocated
		if (num_of_pages == 0) return;

		void *it = virtual_address;
		for(uint32 i = 0; i < num_of_pages; i++, it += PAGE_SIZE){
			uint32 *ptr_page_table = NULL;
			uint32 table_status =  get_page_table(ptr_page_directory, (uint32)it, &ptr_page_table);
			if(table_status == TABLE_NOT_EXIST) {
				ptr_page_table = (uint32*)create_page_table(ptr_page_directory, (int32)it);
			}
			struct FrameInfo *frame = get_frame_info(ptr_page_directory, (uint32)it, &ptr_page_table);
			if (frame == NULL){continue;}

			free_frame(frame);
			synced_unmap_frame(ptr_page_directory, (uint32)it);

			// updating the pages array ... necessary to detect if the page is free or not
			uint32 page_num = ((uint32)it) / (uint32)PAGE_SIZE; // get page index
			allocated_pages_num[page_num] = 0;

		}

		// move the  pgalloc_last pointer down if exist some free pages before it.
		uint32 ptr = pgalloc_last - PAGE_SIZE;
		while(ptr >= (hard_limit + PAGE_SIZE) && khis_free_page(ptr)) {
			pgalloc_last -= PAGE_SIZE;
			ptr -= PAGE_SIZE;
		}

	} else {
		panic("kfree() : the provided address is invalid..!!");
	}


	//you need to get the size of the given allocation using its address
	//refer to the project presentation and documentation for details

}

unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #05] [1] KERNEL HEAP - kheap_physical_address
	// Write your code here, remove the panic and write your code
	//panic("kheap_physical_address() is not implemented yet...!!");
	uint32 *ptr_table = NULL;
	struct FrameInfo *ptr_frame_info = get_frame_info(ptr_page_directory, (uint32)virtual_address, &ptr_table);
	if(ptr_frame_info == NULL) {

		return 0;
	}

	uint32 mask = (1 << 12)-1;
	uint32 offset = (mask&virtual_address);
	return (to_physical_address(ptr_frame_info) + offset);
	//return the physical address corresponding to given virtual_address
	//refer to the project presentation and documentation for details

	//EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED ==================
}


unsigned int kheap_virtual_address(unsigned int physical_address) {
	//TODO: [PROJECT'24.MS2 - #06] [1] KERNEL HEAP - kheap_virtual_address
	// Write your code here, remove the panic and write your code

	//panic("kheap_virtual_address() is not implemented yet...!!");


	//return the virtual address corresponding to given physical_address
	//refer to the project presentation and documentation for details

	// the frame number in physical memory
	uint32 ind = physical_address/PAGE_SIZE;

	uint32 offset = physical_address%PAGE_SIZE;

	// getting page number from array
	uint32 page_num = va_page_num[ind];

	// return 0 if no mapped page for the given physical address
	if(page_num == -1) return 0;

	return (page_num  * PAGE_SIZE + offset);

	//EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED ==================
}
//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, if moved to another loc: the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

void * update_number_of_allocated_pages(uint32 it, uint32 num_pages, uint32 num_page_start, uint32 num_page_end){
	uint32 result = it;
	//uint32 num_pages = total_size/PAGE_SIZE;
	for (int i = 0, cnt = num_pages; i < num_pages; i++, it += PAGE_SIZE, cnt--) {
		//cprintf("flag2\n");
        if(i + 1 >= num_page_start && i + 1 <= num_page_end){
        	struct FrameInfo *newFrame = NULL;
			int state = allocate_frame(&newFrame);
			if (state == E_NO_MEM) {
				return NULL;
			}
			state = synced_map_frame(ptr_page_directory, newFrame, it, PERM_PRESENT | PERM_WRITEABLE);
        }

		// saving the number of allocated pages.
		uint32 page_num = ((uint32)it) / (uint32)PAGE_SIZE;
		allocated_pages_num[page_num] = cnt;

	}
	return (void*)result;
}

void *realloc_page(void *virtual_address, uint32 new_size){

	/* Validations --> done
	 * new_size = ROUND_UP(new_size, PAGE_SIZE) //so After this line all sizes are (n* PAGE_SIZE)
	 * size_difference = new_size - old_size
	 * a) size_difference == 0 --> return old address with the old size
	 *               case 1: new size before round up < (n*PAGE_SIZE) "size el deleted --> internal fragmentation"  //8 --> 7
	 *               case 2: new size before round up == (n*PAGE_SIZE)) "new_size same old_size"
	 * b) size_difference > 0  'increasing'
	 * 			find size_freePages After old size till end(pageAllocated || KERNEL_HEAP_MAX)
	 * 			1) size_freePages == size_difference  --> return old address; //old address with new size, update number of allocated pages, allocated
	 * 			2) size_freePages != size_difference  --> return if( kmalloc(new_size) == NULL) return old address;  else return kk;//new location
	 * c) size_difference < 0  'decreasing'
	 * 			free the pages after old address + new_size and set in array "addressNewFreePages = old address + new_size"
	 * 			kfree(addressNewFreePages);
	 * 			update_number_allocated_pages, allocated = 0;
	 * 			return old address; //old address with new size  'same location'
	 * */
	// check this va is allocated or not
	 uint32 total_size = ((new_size + PAGE_SIZE - 1)/PAGE_SIZE) * PAGE_SIZE; //ROUND_UP

	 // get old_size(size of allocated_pages for this virtual_address)
	 uint32 page_num = ((uint32)virtual_address) / (uint32)PAGE_SIZE; // get page index
	 uint32 allocated_pages_number = allocated_pages_num[page_num];
	 uint32 old_size = allocated_pages_number * (uint32)PAGE_SIZE;

	 // find size_difference
	 int32 size_difference = (int32)total_size - (int32)old_size;
	 // 3 cases
	 if(size_difference == 0) return virtual_address;
	 else if(size_difference > 0) {

		 uint32 num_of_new_pages = (uint32)size_difference / PAGE_SIZE;
		 uint32 it = (uint32)virtual_address + old_size;
		 //loop
		 uint32 page_num = ((uint32)it) / (uint32)PAGE_SIZE;

		 while(va_page_num[page_num] == -1) {

			 num_of_new_pages--;
			 if(num_of_new_pages == 0){
				 //allocate
				 void * returnfun = update_number_of_allocated_pages((uint32)virtual_address, total_size / (uint32)PAGE_SIZE, allocated_pages_number + 1, total_size / (uint32)PAGE_SIZE);
				 if(returnfun == NULL) return virtual_address;
				 return virtual_address; //return same address with new size
			 }
			 it += PAGE_SIZE;
			 page_num = ((uint32)it) / (uint32)PAGE_SIZE;
		 }
		 return kmalloc((unsigned int)new_size);

	 }
	 else if(size_difference < 0){
		 uint32 addressNewFreePages = (uint32)virtual_address + total_size;
		 //free the allocatedPages i don't need
		 kfree((void*)addressNewFreePages);
		 // update the number of allocated pages.
		 void * returnfun = update_number_of_allocated_pages(addressNewFreePages, (uint32)(-1 * size_difference) / (uint32)PAGE_SIZE, 0, 0);
		 if(returnfun == NULL) return virtual_address;
		 return virtual_address; //return same address with new size
	 }

	return virtual_address;
}
void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'24.MS2 - BONUS#1] [1] KERNEL HEAP - krealloc
	// Write your code here, remove the panic and write your code
	//Validations
	//	1) va && 0 --> kfree & return NULL
	//	2) NULL && size --> kmalloc & return the new address
	//	3) NULL && 0 --> do nothing (return NULL)
    if(virtual_address != NULL && new_size == 0){
    	kfree(virtual_address);
    	return NULL;
    }
    if(virtual_address == NULL && new_size > 0){
    	 return kmalloc((unsigned int)new_size);
    }
    if(virtual_address == NULL && new_size == 0){
    	return NULL;
    }

	//  4) va && size :
	// Blocks
	if ((char*)virtual_address < (char*)segment_break && (char*)virtual_address >= (char*)(start_kernal_heap + sizeof(int))) {

		if(new_size <= DYN_ALLOC_MAX_BLOCK_SIZE){
			return realloc_block_FF(virtual_address, new_size);
		}
		else {
			//'new location in page allocator'
			kfree((void*)virtual_address);
			return kmalloc((unsigned int)new_size);
		}
	}
	// Pages
	else if((char*)virtual_address >= (char*)(hard_limit + sizeof(int)) && (char*)virtual_address < (char*)KERNEL_HEAP_MAX) {
		if(new_size >= PAGE_SIZE){
			return realloc_page(virtual_address, new_size);
		}
		else {

			kfree((void*)virtual_address);
			return kmalloc((unsigned int)new_size); //'new location in block allocator'
		}

	}

	return virtual_address;

	//panic("krealloc() is not implemented yet...!!");
}
