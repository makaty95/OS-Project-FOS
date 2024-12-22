/*
 * chunk_operations.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include <kern/trap/fault_handler.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/proc/user_environment.h>
#include "kheap.h"
#include "memory_manager.h"
#include <inc/queue.h>

//extern void inctst();

/******************************/
/*[1] RAM CHUNKS MANIPULATION */
/******************************/

//===============================
// 1) CUT-PASTE PAGES IN RAM:
//===============================
//This function should cut-paste the given number of pages from source_va to dest_va on the given page_directory
//	If the page table at any destination page in the range is not exist, it should create it
//	If ANY of the destination pages exists, deny the entire process and return -1. Otherwise, cut-paste the number of pages and return 0
//	ALL 12 permission bits of the destination should be TYPICAL to those of the source
//	The given addresses may be not aligned on 4 KB
int cut_paste_pages(uint32* page_directory, uint32 source_va, uint32 dest_va, uint32 num_of_pages)
{
	//[PROJECT] [CHUNK OPERATIONS] cut_paste_pages
	// Write your code here, remove the panic and write your code
	panic("cut_paste_pages() is not implemented yet...!!");
}

//===============================
// 2) COPY-PASTE RANGE IN RAM:
//===============================
//This function should copy-paste the given size from source_va to dest_va on the given page_directory
//	Ranges DO NOT overlapped.
//	If ANY of the destination pages exists with READ ONLY permission, deny the entire process and return -1.
//	If the page table at any destination page in the range is not exist, it should create it
//	If ANY of the destination pages doesn't exist, create it with the following permissions then copy.
//	Otherwise, just copy!
//		1. WRITABLE permission
//		2. USER/SUPERVISOR permission must be SAME as the one of the source
//	The given range(s) may be not aligned on 4 KB
int copy_paste_chunk(uint32* page_directory, uint32 source_va, uint32 dest_va, uint32 size)
{
	//[PROJECT] [CHUNK OPERATIONS] copy_paste_chunk
	// Write your code here, remove the //panic and write your code
	panic("copy_paste_chunk() is not implemented yet...!!");
}

//===============================
// 3) SHARE RANGE IN RAM:
//===============================
//This function should copy-paste the given size from source_va to dest_va on the given page_directory
//	Ranges DO NOT overlapped.
//	It should set the permissions of the second range by the given perms
//	If ANY of the destination pages exists, deny the entire process and return -1. Otherwise, share the required range and return 0
//	If the page table at any destination page in the range is not exist, it should create it
//	The given range(s) may be not aligned on 4 KB
int share_chunk(uint32* page_directory, uint32 source_va,uint32 dest_va, uint32 size, uint32 perms)
{
	//[PROJECT] [CHUNK OPERATIONS] share_chunk
	// Write your code here, remove the //panic and write your code
	panic("share_chunk() is not implemented yet...!!");
}

//===============================
// 4) ALLOCATE CHUNK IN RAM:
//===============================
//This function should allocate the given virtual range [<va>, <va> + <size>) in the given address space  <page_directory> with the given permissions <perms>.
//	If ANY of the destination pages exists, deny the entire process and return -1. Otherwise, allocate the required range and return 0
//	If the page table at any destination page in the range is not exist, it should create it
//	Allocation should be aligned on page boundary. However, the given range may be not aligned.
int allocate_chunk(uint32* page_directory, uint32 va, uint32 size, uint32 perms)
{
	//[PROJECT] [CHUNK OPERATIONS] allocate_chunk
	// Write your code here, remove the //panic and write your code
	panic("allocate_chunk() is not implemented yet...!!");
}

//=====================================
// 5) CALCULATE ALLOCATED SPACE IN RAM:
//=====================================
void calculate_allocated_space(uint32* page_directory, uint32 sva, uint32 eva, uint32 *num_tables, uint32 *num_pages)
{
	//[PROJECT] [CHUNK OPERATIONS] calculate_allocated_space
	// Write your code here, remove the panic and write your code
	panic("calculate_allocated_space() is not implemented yet...!!");
}

//=====================================
// 6) CALCULATE REQUIRED FRAMES IN RAM:
//=====================================
//This function should calculate the required number of pages for allocating and mapping the given range [start va, start va + size) (either for the pages themselves or for the page tables required for mapping)
//	Pages and/or page tables that are already exist in the range SHOULD NOT be counted.
//	The given range(s) may be not aligned on 4 KB
uint32 calculate_required_frames(uint32* page_directory, uint32 sva, uint32 size)
{
	//[PROJECT] [CHUNK OPERATIONS] calculate_required_frames
	// Write your code here, remove the panic and write your code
	panic("calculate_required_frames() is not implemented yet...!!");
}

//=================================================================================//
//===========================END RAM CHUNKS MANIPULATION ==========================//
//=================================================================================//

/*******************************/
/*[2] USER CHUNKS MANIPULATION */
/*******************************/

//======================================================
/// functions used for USER HEAP (malloc, free, ...)
//======================================================

//=====================================
/* DYNAMIC ALLOCATOR SYSTEM CALLS */
//=====================================


void mark_page(uint32 page_va, struct Env* env, uint32 state) {
	// page_va is the virtual address within some page , could be with offset doesn't matter.

	uint32* ptr_page_table;
	int status = get_page_table(env->env_page_directory, page_va, &ptr_page_table);
	if(status == TABLE_NOT_EXIST) {
		//cprintf("creating page table\n");
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

int32 uhis_free_page(uint32 page_va, struct Env* env) {
	// page_va is the virtual address within some page , could be with offset doesn't matter.

	uint32* ptr_page_table;
	int status = get_page_table(env->env_page_directory, page_va, &ptr_page_table);
	if(status == TABLE_NOT_EXIST) {
		ptr_page_table = (uint32*)create_page_table(env->env_page_directory, page_va);
	}

	uint32 mask = (1 << 10);
	return !((ptr_page_table[PTX(page_va)]&mask)); // if 0 its not marked , if other it is set
}

uint32 is_start_of_range(uint32 page_va, struct Env* env) {


	uint32* ptr_page_table;
	int status = get_page_table(env->env_page_directory, page_va, &ptr_page_table);
	if(status == TABLE_NOT_EXIST) {
		ptr_page_table = (uint32*)create_page_table(env->env_page_directory, page_va);
	}

	uint32 mask = (1 << 11);
	if((ptr_page_table[PTX(page_va)]&mask)) {
		return 1;
	}
	else return 0;


}


void* sys_sbrk(int numOfPages)
{
	/* numOfPages > 0: move the segment break of the current user program to increase the size of its heap
	 * 				by the given number of pages. You should allocate NOTHING,
	 * 				and returns the address of the previous break (i.e. the beginning of newly mapped memory).
	 * numOfPages = 0: just return the current position of the segment break
	 *
	 * NOTES:
	 * 	1) As in real OS, allocate pages lazily. While sbrk moves the segment break, pages are not allocated
	 * 		until the user program actually tries to access data in its heap (i.e. will be allocated via the fault handler).
	 * 	2) Allocating additional pages for a process’ heap will fail if, for example, the free frames are exhausted
	 * 		or the break exceed the limit of the dynamic allocator. If sys_sbrk fails, the net effect should
	 * 		be that sys_sbrk returns (void*) -1 and that the segment break and the process heap are unaffected.
	 * 		You might have to undo any operations you have done so far in this case.
	 */

	//TODO: [PROJECT'24.MS2 - #11] [3] USER HEAP - sys_sbrk
	/*====================================*/
	/*Remove this line before start coding*/
	//return (void*)-1 ;
	/*====================================*/
	//cprintf("----------(chunk sbrk called with size = %d)-----------\n", PAGE_SIZE * numOfPages);
	struct Env* env = get_cpu_proc(); //the current running Environment to adjust its break limit

	/////////
	if(numOfPages == 0) return (void*)env->sbreak; // edge case

	uint32 increasing = numOfPages * PAGE_SIZE; // size to be allocated
	uint32 last_address = env->sbreak + increasing;
	uint32 old_sbrk = env->sbreak;
//	cprintf("old sbreak = %x\n", old_sbrk);
//	cprintf("hlimit = %x\n", env->hlimit);

	if(last_address > env->hlimit)
	{
		return (void*)E_UNSPECIFIED;
	}


	//page address
	uint32 it = env->sbreak;
	//cprintf("old sbreak: %x\n", it);

	//c//printf("flag a\n");
	// mark the range
	for(int32 i = 0 ; i < numOfPages ; i++, it += PAGE_SIZE) {
		//MARK
		mark_page(it, env, PAGE_MARKED);
	}

	//cprintf("flag b\n");

	// update sbreak pointer
	env->sbreak = last_address;
//	cprintf("flag c\n");

	// adjusting end block
//	cprintf("flag d\n");
//
//	cprintf("flag e\n");
	// updating end block in env
	//env->end_bound = (uint32)new_end_block;

//	cprintf("chunk sbrk done its job\n");
	return (void*)old_sbrk;


}

//=====================================
// 1) ALLOCATE USER MEMORY:
//=====================================

uint32 uhget_pgallocation_address(uint32 size, struct Env* e) {


	uint32 it_start = e->hlimit+PAGE_SIZE;
	uint32* page_directory = e->env_page_directory;
	uint32 pg_alloc_last = e->pgalloc_last;

	uint32 it = it_start;
	uint32 curSize = 0;
	uint32 pgalloc_ptr = 0;

	//cprintf("searchig for space = %umb, sratring from %p to %p\n",(size/(1<<20)), (void*)it_start, (void*)pg_alloc_last);
	for (; curSize < size && it < pg_alloc_last; it += PAGE_SIZE) {

		if (uhis_free_page(it, e)) { // if free page
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

void allocate_user_mem(struct Env* e, uint32 virtual_address, uint32 size)
{

	//cprintf("-------------allocate_user_mem called-------------\n");
	/*====================================*/
	/*Remove this line before start coding*/
	//	inctst();
	//	return;
	/*====================================*/

	//TODO: [PROJECT'24.MS2 - #13] [3] USER HEAP [KERNEL SIDE] - allocate_user_mem()
	// Write your code here, remove the panic and write your code
	//panic("allocate_user_mem() is not implemented yet...!!");

	// getting the start address of the allocation
	uint32 total_size = ((size+PAGE_SIZE-1)/PAGE_SIZE)*PAGE_SIZE;
	virtual_address = uhget_pgallocation_address(total_size, e);

	if (virtual_address == e->pgalloc_last) {
		if((e->pgalloc_last + total_size) > (uint32)USER_HEAP_MAX) {
			e->returned_address = NULL;
			return;
		}
		e->pgalloc_last += total_size;
	}

	e->returned_address = (void*)virtual_address;

	// marking the range
	uint32 it = virtual_address;
	uint32 num_of_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE; // rounding up the pages

	for (int i = 0; i < num_of_pages; i++, it += PAGE_SIZE) {
		uint32* ptr_page_table = NULL;
		int page_status = get_page_table(e->env_page_directory, it, &ptr_page_table);
		if(page_status == TABLE_NOT_EXIST) {
			create_page_table(e->env_page_directory, it);
		}
		pt_set_page_permissions(e->env_page_directory, it, 0, PERM_PRESENT | PERM_USER); // edited

		mark_page(it, e, PAGE_MARKED);

	}

	mark_page(virtual_address, e, PAGE_MARK_START);
}

//=====================================
// 2) FREE USER MEMORY:
//=====================================


void free_user_mem(struct Env* e, uint32 virtual_address, uint32 size)
{
	/*====================================*/
	/*Remove this line before start coding*/
//	inctst();
//	return;
	/*====================================*/

	//TODO: [PROJECT'24.MS2 - #15] [3] USER HEAP [KERNEL SIDE] - free_user_mem
	// Write your code here, remove the panic and write your code
	//panic("free_user_mem() is not implemented yet...!!");

	uint32 va = virtual_address;

	//uint32 start_page = virtual_address/PAGE_SIZE;

	// the page num of the last page in the range
	// uint32 end_page = (virtual_address + size - PAGE_SIZE)/PAGE_SIZE;
	size = 0;
//	cprintf("done0\n");
	// 1. unmark and free the pages
	while((va < USER_HEAP_MAX && va >= e->hlimit+PAGE_SIZE) &&
			((!is_start_of_range(va, e) && !uhis_free_page(va, e)) || va == virtual_address)) {
//		cprintf("va = %x\n", va);
//		if(!is_start_of_range(va, e)) {
//			cprintf("start_range is false\n");
//		} else {
//			cprintf("start_range is true\n");
//		}
//		if(!uhis_free_page(va, e)) {
//			cprintf("uhis_free_page is false\n");
//		} else {
//			cprintf("uhis_free_page is true\n");
//		}

//		cprintf("hi 1\n");
		// unmark the page


		// free the page
		uint32 *page_table_ptr;
		struct FrameInfo* frame_info_ptr = NULL;
		frame_info_ptr = get_frame_info(e->env_page_directory, va, &page_table_ptr);
//		cprintf("frame_ptr = %p\n", frame_info_ptr);
//		cprintf("hi 2\n");
		if(frame_info_ptr != NULL) {
			free_frame(frame_info_ptr);
			unmap_frame(e->env_page_directory, va);
		}
		//else cprintf("null frame info\n");

//		cprintf("hi 3\n");
//		cprintf("hi 4\n");

		// remove the page from page file
		pf_remove_env_page(e, va);
//		cprintf("hi 5\n");
		mark_page(va, e, FREE_PAGE);
		va+=PAGE_SIZE;
		size+=PAGE_SIZE;
	}

	//cprintf("done1\n");

	// 2. remove page from the page file (done in the above loop for each page)

	// 3. remove pages from the process working set elements
	struct WorkingSetElement* ws_element;
	struct WorkingSetElement* held_ws_element = NULL;
	LIST_FOREACH(ws_element, &(e->page_WS_list)) {

		// remove the hold element
		if(held_ws_element != NULL) {
			LIST_REMOVE(&(e->page_WS_list), held_ws_element);
			held_ws_element = NULL;
		}


		if((char*)(ws_element->virtual_address) >= (char*)virtual_address &&
				(char*)(ws_element->virtual_address) < (char*)(virtual_address + size)) {
			// if the current working set element within the given range
			// i will hold it and remove it in the next iteration
			// can't remove it in the current iteration as it will make the iterator pointer lost
			held_ws_element = ws_element;
		}
	}

	// if the last element is to be removed
	if(held_ws_element != NULL) {
		LIST_REMOVE(&(e->page_WS_list), held_ws_element);
	}

	//cprintf("done2\n");

	// move the  pgalloc_last pointer down if exist some free pages before it.
	uint32 ptr = e->pgalloc_last - PAGE_SIZE;
	while(ptr >= (e->hlimit + PAGE_SIZE) && uhis_free_page(ptr, e)) {
		e->pgalloc_last -= PAGE_SIZE;
		ptr -= PAGE_SIZE;
	}

	//cprintf("done freeing\n");


	//TODO: [PROJECT'24.MS2 - BONUS#3] [3] USER HEAP [KERNEL SIDE] - O(1) free_user_mem // done
}

//=====================================
// 2) FREE USER MEMORY (BUFFERING):
//=====================================
void __free_user_mem_with_buffering(struct Env* e, uint32 virtual_address, uint32 size)
{
	// your code is here, remove the panic and write your code
	panic("__free_user_mem_with_buffering() is not implemented yet...!!");
}

//=====================================
// 3) MOVE USER MEMORY:
//=====================================
void move_user_mem(struct Env* e, uint32 src_virtual_address, uint32 dst_virtual_address, uint32 size)
{
	//[PROJECT] [USER HEAP - KERNEL SIDE] move_user_mem
	//your code is here, remove the panic and write your code
	panic("move_user_mem() is not implemented yet...!!");
}

//=================================================================================//
//========================== END USER CHUNKS MANIPULATION =========================//
//=================================================================================//

