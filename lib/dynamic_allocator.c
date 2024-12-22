/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"



//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//


//=====================================================
// 1) GET BLOCK SIZE (including size of its meta data):
//=====================================================
__inline__ uint32 get_block_size(void* va) {
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (*curBlkMetaData) & ~(0x1);
}

//===========================
// 2) GET BLOCK STATUS:
//===========================
__inline__ int8 is_free_block(void* va)
{
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (~(*curBlkMetaData) & 0x1) ;
}

//===========================
// 3) ALLOCATE BLOCK:
//===========================

void *alloc_block(uint32 size, int ALLOC_STRATEGY)
{
	void *va = NULL;
	switch (ALLOC_STRATEGY)
	{
	case DA_FF:
		va = alloc_block_FF(size);
		break;
	case DA_NF:
		va = alloc_block_NF(size);
		break;
	case DA_BF:
		va = alloc_block_BF(size);
		break;
	case DA_WF:
		va = alloc_block_WF(size);
		break;
	default:
		cprintf("Invalid allocation strategy\n");
		break;
	}
	return va;
}

//===========================
// 4) PRINT BLOCKS LIST:
//===========================

void print_blocks_list(struct MemBlock_LIST list)
{
	cprintf("=========================================\n");
	struct BlockElement* blk ;
	cprintf("\nDynAlloc Blocks List:\n");
	LIST_FOREACH(blk, &list)
	{
		cprintf("(size: %d, isFree: %d)\n", get_block_size(blk), is_free_block(blk)) ;
	}
	cprintf("=========================================\n");

}
//
////********************************************************************************//
////********************************************************************************//

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

bool is_initialized = 0;

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================

void initialize_dynamic_allocator(uint32 daStart, uint32 initSizeOfAllocatedSpace)
{
	//cprintf("initializing initialize_dynamic_allocator from %x, size = %u\n", daStart, initSizeOfAllocatedSpace);

	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (initSizeOfAllocatedSpace % 2 != 0) initSizeOfAllocatedSpace++; //ensure it's multiple of 2
		if (initSizeOfAllocatedSpace == 0)
			return ;
		is_initialized = 1;
	}

	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'24.MS1 - #04] [3] DYNAMIC ALLOCATOR - initialize_dynamic_allocator
	//COMMENT THE FOLLOWING LINE BEFORE START CODING

	//Your Code is Here...
	void *Begin_elblock = (void*) daStart;
	void *End_elblock = (Begin_elblock + initSizeOfAllocatedSpace - sizeof(int));

	struct BlockElement* firstFreeBlock_in_the_environment;

	// you should point to the pointer after head and the Begin
	firstFreeBlock_in_the_environment = (struct BlockElement*) (Begin_elblock + 2 * sizeof(int)); // skip the begin block

	//local pointers
	uint32 *h = (uint32*)(Begin_elblock + 4);
	uint32 *f = (uint32*)(End_elblock - 4);

	begin_bound = (void*)daStart;
	end_bound = (void*)(daStart + initSizeOfAllocatedSpace);

	int32 *b = (int32*)(Begin_elblock);
	int32 *e = (int32*)(End_elblock);
	*b = *e = 1;



	// save header and footer initial info
	*h = *f = initSizeOfAllocatedSpace - (2 * sizeof(int));

	LIST_INIT(&freeBlocksList);

	//insert
	LIST_INSERT_HEAD(&freeBlocksList, firstFreeBlock_in_the_environment);

	//set_block_data((Beg + 2 * sizeof(int)), 100, 1);

	//panic("initialize_dynamic_allocator is not implemented yet");



//set_block_data(freeBlocksList.lh_first, 30, 1);
//	cprintf("--------------------------initialize_dynamic_allocator-----------------------------\n");
//	cprintf("Beg = %x \n", Begin_elblock);
//	cprintf("End = %x \n", Begin_elblock);
//	cprintf("value inside End = %d, Beg = %d\n", *e, *b);
//	cprintf("header of first block = %x \n", h);
//	cprintf("footer of first block = %x \n", f);
//	cprintf("value inside header of first block = %d \n", *h);
//	cprintf("value inside footer of first block = %d \n", *f);
//	cprintf("lh_first  = %x \n", freeBlocksList.lh_first);
//	cprintf("lh_last  = %x \n", freeBlocksList.lh_last);
//	cprintf("freeblock  = %x \n", firstFreeBlock_in_the_environment);
//	cprintf("nex of first block = %x \n", firstFreeBlock_in_the_environment->prev_next_info.le_next);
//	cprintf("prev of first block = %x \n", firstFreeBlock_in_the_environment->prev_next_info.le_prev);
//	cprintf("All free memory blocks:\n");
//	print_blocks_list(freeBlocksList);
//	cprintf("--------------------------------------------------------------\n");




	//panic("initialize_dynamic_allocator is not implemented yet");
}

//==================================
// [2] SET BLOCK HEADER & FOOTER:
//==================================


void set_block_data(void* va, uint32 totalSize, bool isAllocated)
{

	//TODO: [PROJECT'24.MS1 - #05] [3] DYNAMIC ALLOCATOR - set_block_data
	//COMMENT THE FOLLOWING LINE BEFORE START CODING

	//Your Code is Here...

	if(totalSize%2) totalSize += 1;

	uint32 blockInfo = totalSize | (uint32)isAllocated;
	uint32 *h = (uint32*)(va - sizeof(int));
	uint32 *f = (uint32*)(va + totalSize - 2 * sizeof(int));
	//cprintf("insize set_block: h = %p, f= %p\n", h, f);
	*h = *f = blockInfo;

//	cprintf("---------------------------set_block_data---------------------\n");
//	cprintf("h  = %d \n", *h);
//	cprintf("f  = %d \n", *f);
//	cprintf("blockInfo  = %d \n", blockInfo);
//	cprintf("--------------------------------------------------------------\n");

}


//=========================================
// [3] ALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *alloc_block_FF(uint32 size)
{
	//cprintf("---------------alloc_block_FF called with size %u---------------\n", size);
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (size % 2 != 0) size++;	//ensure that the size is even (to use LSB as allocation flag)
		if (size < DYN_ALLOC_MIN_BLOCK_SIZE)
			size = DYN_ALLOC_MIN_BLOCK_SIZE;
		if (!is_initialized)
		{

			//cprintf("flag i (not initialized)\n");
			uint32 required_size = size + 2*sizeof(int) /*header & footer*/ + 2*sizeof(int) /*da begin & end*/ ;
			uint32 da_start = (uint32)sbrk(ROUNDUP(required_size, PAGE_SIZE)/PAGE_SIZE);
			//cprintf("flag ii\n");
			uint32 da_break = (uint32)sbrk(0);
			//cprintf("flag iii\n");

			initialize_dynamic_allocator(da_start, da_break - da_start);
			//cprintf("flag iv\n");

		}
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'24.MS1 - #06] [3] DYNAMIC ALLOCATOR - alloc_block_FF
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("alloc_block_FF is not implemented yet");
	//Your Code is Here...

	//cprintf("acctual block FF alloc\n");



	int32 totalAllocationSize = size + 2 * sizeof(int); // totalSize to be allocated.

	struct BlockElement* curMemoryBlock;
	LIST_FOREACH(curMemoryBlock, &freeBlocksList) {

		// should be even because this block is not allocated meaning the status bit = 0
		int32 currBlockSize = get_block_size(curMemoryBlock); // includes header and footer.

		//cprintf("size of the block at %x = %d\n", curMemoryBlock, currBlockSize);
		// header and footer of the block
		void *curr = (void*)curMemoryBlock;
		void *ch = (void*)(curr - sizeof(int));
		void *cf = (void*)(ch + currBlockSize - sizeof(int));
		//cprintf("ch: %x\n", ch);
		//cprintf("cf: %x\n", cf);

		// borders of the block
		void* b_beg = ch;
		void* b_end = cf + sizeof(int);

		// so to insert the new free portion of the current block before my next block.
		struct BlockElement* free_block_after = curMemoryBlock->prev_next_info.le_next;
		//cprintf("falg000\n");
		//cprintf("falg0\n");
		if(currBlockSize >= totalAllocationSize) {


			LIST_REMOVE(&freeBlocksList, curMemoryBlock);

			if((currBlockSize - totalAllocationSize) >= (4 * sizeof(int))) {
				// allocate and make the rest as free block

				// adding the rest to a free block
				void* fb_h = ch + totalAllocationSize; // points to the new free block(header)
				//cprintf("falg01\n");

				struct BlockElement* new_free_toInsert = (struct BlockElement*) (fb_h + sizeof(int));
				//cprintf("free_block_after: %p\n", free_block_after);
				//	cprintf("new_free_block: %p\n", new_free_toInsert);

				// change meta data of the new free block
				int32 new_free_block_size = currBlockSize - totalAllocationSize;


				// insert the free block inside the free block list.
				if(free_block_after == NULL) {
					LIST_INSERT_TAIL(&freeBlocksList, new_free_toInsert);
				} else LIST_INSERT_BEFORE(&freeBlocksList, free_block_after, new_free_toInsert);
				//		cprintf("falg02\n");

				set_block_data(new_free_toInsert, new_free_block_size, 0);

				// change the allocated block meta data.
				// we can do it manually by pointers or by the function set_block_data
				// i trust the function :)
				struct BlockELement* allocated_block = (struct BlockELement*)(ch + sizeof(int));

				set_block_data(allocated_block, totalAllocationSize, 1);


				//cprintf("allocated block at: %p\n", allocated_block);
				//cprintf("-> free block list size: %d\n",LIST_SIZE(&freeBlocksList));
				return allocated_block; // return pointer to the allocated space after header.

			} else {
				//cprintf("flag2\n");
				// internal fragmentation (< 16 Bytes) taken as padding
				// so you just change the meta data.

				// should double check this function correctness.
				struct BlockELement* allocated_block = (struct BlockELement*)(ch + sizeof(int));
				set_block_data(allocated_block, currBlockSize, 1); // note the size won't change
				// i just allocated it and returned its address.
				//cprintf("-> free block list size: %d\n",LIST_SIZE(&freeBlocksList));
				return allocated_block;
			}


		}

	}

	// sbrk and pushing a new block of free memory into the free blocks list

	//cprintf("the called size = %d BUT RETURNED sbrk()\n", size);
	//cprintf("flag - DA-0\n");
	uint32 required_size = size + 2*sizeof(int);
	//cprintf("flag - DA-1\n");
//
	uint32 reserved = ((required_size + PAGE_SIZE - 1)/PAGE_SIZE)*PAGE_SIZE;

	void* ret = sbrk((reserved/PAGE_SIZE));
	//cprintf("ret = %p\n", ret);
	if(ret == (void*)-1)
	{
		//cprintf("flag - DA- NULLL\n");
		return NULL;
	}
	//cprintf("flag - DA-2\n");

	// end block init
	int32 *new_end_block = (int32 *)(ret + reserved - sizeof(int));
	*new_end_block = 1;


	int32 *prev_f = (int32*)(ret - 2 * sizeof(int)); // get the prev block footer
	int32 is_alloc_prev = (*prev_f)%2;

	if(!is_alloc_prev) {
		struct BlockElement *curr_new_block = (struct BlockElement*)(ret - *prev_f);
		struct BlockElement *prev = curr_new_block;
		struct BlockElement *prev_of_prev = prev->prev_next_info.le_prev;


		LIST_REMOVE(&freeBlocksList, prev);
		if(prev_of_prev != NULL) {
			LIST_INSERT_AFTER(&freeBlocksList, prev_of_prev, curr_new_block);
		} else {
			LIST_INSERT_HEAD(&freeBlocksList, curr_new_block);
		}

		set_block_data(curr_new_block, (*prev_f + reserved), 0);

	} else {
		struct BlockElement *curr_new_block = (struct BlockElement*) ret;
		//void *it_h = (void*)next_h; // points to head of the next block
		LIST_INSERT_TAIL(&freeBlocksList, curr_new_block);

		set_block_data(curr_new_block, reserved, 0);
	}

//	cprintf("flag - DA-3\n");
//	//end_bound = ret + (ROUNDUP(required_size, PAGE_SIZE));
//	cprintf("flag - DA-4\n");

	return alloc_block_FF(size);

}


//=========================================
// [4] ALLOCATE BLOCK BY BEST FIT:
//=========================================
void *alloc_block_BF(uint32 size)
{

	//TODO: [PROJECT'24.MS1 - BONUS] [3] DYNAMIC ALLOCATOR - alloc_block_BF
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("alloc_block_BF is not implemented yet");
	//Your Code is Here...


	int32 totalAllocationSize = size + 2 * sizeof(int); // totalSize to be allocated.
	struct BlockElement* ptrMiniSize = NULL;
	bool flag = 0;
	bool found = 0;
	int32 currBlockSize;
	struct BlockElement* curMemoryBlock;
	LIST_FOREACH(curMemoryBlock, &freeBlocksList) {
		currBlockSize = get_block_size(curMemoryBlock); // includes header and footer.

		if(currBlockSize == totalAllocationSize) { // we should compare with the total not reg. size
			ptrMiniSize = curMemoryBlock;
			found = 1;
			break;
		}
		else if(currBlockSize > totalAllocationSize){

			if(!flag){
				flag = 1;
				//*ptrMiniSize = currBlockSize;
				ptrMiniSize = curMemoryBlock;
				found = 1;
			}

			if(currBlockSize < get_block_size(ptrMiniSize)){
				//*ptrMiniSize = currBlockSize;
				ptrMiniSize = curMemoryBlock;
			}
		}

	}
	// should be even because this block is not allocated meaning the status bit = 0
	//int32 currBlockSize = get_block_size(curMemoryBlock); // includes header and footer.

	//cprintf("size of the block at %x = %d\n", curMemoryBlock, currBlockSize);

	if(ptrMiniSize != NULL){
		curMemoryBlock = ptrMiniSize;
		currBlockSize = get_block_size(curMemoryBlock);
		// header and footer of the block
		void* curr = (void*)curMemoryBlock;
		void *ch = (curr - sizeof(int));
		void *cf = ch + currBlockSize - sizeof(int);
		//cprintf("ch: %x\n", ch);
		//cprintf("cf: %x\n", cf);

		// borders of the block
		void* b_beg = ch;
		void* b_end = cf + sizeof(int);

		// so to insert the new free portion of the current block before my next block.
		struct BlockElement* free_block_after = curMemoryBlock->prev_next_info.le_next;
		//cprintf("falg000\n");
		//cprintf("falg0\n");
		if(currBlockSize >= totalAllocationSize) {


			LIST_REMOVE(&freeBlocksList, curMemoryBlock);

			if((currBlockSize - totalAllocationSize) >= (4 * sizeof(int))) {
				// allocate and make the rest as free block

				// adding the rest to a free block
				void* fb_h = b_beg + totalAllocationSize; // points to the new free block(header)
				//cprintf("falg01\n");

				struct BlockElement* new_free_toInsert = (struct BlockElement*) (fb_h + sizeof(int));
				//cprintf("free_block_after: %p\n", free_block_after);
				//	cprintf("new_free_block: %p\n", new_free_toInsert);

				// change meta data of the new free block
				int32 new_free_block_size = currBlockSize - totalAllocationSize;
				set_block_data(new_free_toInsert, new_free_block_size, 0);

				// insert the free block inside the free block list.
				if(free_block_after == NULL) {
					LIST_INSERT_TAIL(&freeBlocksList, new_free_toInsert);
				} else LIST_INSERT_BEFORE(&freeBlocksList, free_block_after, new_free_toInsert);
				//		cprintf("falg02\n");


				// change the allocated block meta data.
				// we can do it manually by pointers or by the function set_block_data
				// i trust the function :)
				void* allocated_block = (ch + sizeof(int));

				set_block_data(allocated_block, totalAllocationSize, 1);


				//cprintf("allocated block at: %p\n", allocated_block);
				//cprintf("-> free block list size: %d\n",LIST_SIZE(&freeBlocksList));
				return allocated_block; // return pointer to the allocated space after header.

			} else {
				//cprintf("flag2\n");
				// internal fragmentation (< 16 Bytes) taken as padding
				// so you just change the meta data.

				// should double check this function correctness.
				void* allocated_block = (ch + sizeof(int));
				set_block_data(allocated_block, currBlockSize, 1); // note the size won't change
				// i just allocated it and returned its address.
				//cprintf("-> free block list size: %d\n",LIST_SIZE(&freeBlocksList));
				return allocated_block;
			}
		}
	}

	//cprintf("the called size = %d BUT RETURNED sbrk()\n", size);


	uint32 required_size = size + 2*sizeof(int);
	void* ret = sbrk(ROUNDUP(required_size, PAGE_SIZE)/PAGE_SIZE);

	if(*((uint32 *) ret) == -1)
	{
		return NULL;
	}

	return alloc_block_BF(size);


}
//===================================================
// [5] FREE BLOCK WITH COALESCING:
//===================================================

void free_block(void *va)
{
	//TODO: [PROJECT'24.MS1 - #07] [3] DYNAMIC ALLOCATOR - free_block
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
//	panic("free_block is not implemented yet");
	//Your Code is Here...

//	STEP1: get the block with virtual address 'va'
	//cprintf("[free block before ex.]-> free block list size: %d\n",LIST_SIZE(&freeBlocksList));
	//cprintf("flag 1  %d \n", (int32)USER_HEAP_START);

//	cprintf("---------------------------\n");
//	cprintf("[free before ex.]-> free block list size: %d\n",LIST_SIZE(&freeBlocksList));
//	cprintf("-> calling free with va = %x \n", va);


//	if((char*)va < (char*)(begin_bound + sizeof(int)) || (char*)va >= (char*)(end_bound - sizeof(int)))
//	{   cprintf ("bra 7dod\n");
//		return;
//	}

	//cprintf("the free is live\n");


	if(va == NULL) {
		return;
	}
//	cprintf("flag -1 \n");


	// check if the current block
	struct BlockElement *it;
	bool found = 0;
	LIST_FOREACH(it, &freeBlocksList) {
		if ((void*)it == va) {
			//cprintf("the va matches!");
			found = 1;
			break;
		}
	}

//	cprintf("flag 0 \n");

	if(found) { // if it's already free, return nothing
		//cprintf("-> returning because the address already free\n", va);
		return;
	}



	it = (struct BlockElement*)va;
	int32* cur_block_h = (int32*)(va - sizeof(int));
	int32 cur_size =  (int32)get_block_size(it); // decrease one since it is already allocated.

	int32 *prev_f = (int32*)(va - 2 * sizeof(int)); // get the prev block footer
	int32 is_alloc_prev = (*prev_f)%2;

	int32 *next_h = (int32*)(va + cur_size - sizeof(int));
	int32 is_alloc_next = (*next_h)%2;


//	cprintf("is_alloc_prev: %d, is_alloc_next: %d\n", is_alloc_prev, is_alloc_next);
//
//	cprintf("flag 0 \n");

	if(!is_alloc_next && !is_alloc_prev) {
		//cprintf("-> merging both\n");

//		cprintf("flag 1 \n");


		struct BlockElement *curr_new_block = (struct BlockElement*) (va - *prev_f);
		struct BlockElement *prev = curr_new_block;
		struct BlockElement *next = (struct BlockElement*)(((void*)next_h ) + sizeof(int));
		struct BlockElement *next_of_next = next->prev_next_info.le_next;


		//note: the address of prev = address of the curr_block
		// so you can't add cur to the list then remove prev;
		// you should delete first;
		LIST_REMOVE(&freeBlocksList, prev);
		LIST_REMOVE(&freeBlocksList, next);
		if(next_of_next != NULL) {
			LIST_INSERT_BEFORE(&freeBlocksList, next_of_next, curr_new_block);
		} else {
			LIST_INSERT_TAIL(&freeBlocksList, curr_new_block);
		}


		set_block_data(curr_new_block, (*prev_f + *next_h + cur_size), 0);


	} else if(!is_alloc_prev) {
//		cprintf("flag 2 \n");
//		cprintf("-> merging prev\n");
		struct BlockElement *curr_new_block = (struct BlockElement*)(va - *prev_f);
		struct BlockElement *prev = curr_new_block;
		struct BlockElement *prev_of_prev = prev->prev_next_info.le_prev;


		LIST_REMOVE(&freeBlocksList, prev);
		if(prev_of_prev != NULL) {
			LIST_INSERT_AFTER(&freeBlocksList, prev_of_prev, curr_new_block);
		} else {
			LIST_INSERT_HEAD(&freeBlocksList, curr_new_block);
		}

		set_block_data(curr_new_block, (*prev_f + cur_size), 0);



	} else if(!is_alloc_next) {
//		cprintf("flag 3 \n");
//		cprintf("-> merging next\n");
		struct BlockElement *curr_new_block = (struct BlockElement*) va;
		struct BlockElement *next = (struct BlockElement*) (((void*)next_h) + sizeof(int));
		struct BlockElement *next_of_next = next->prev_next_info.le_next;

		LIST_REMOVE(&freeBlocksList, next);
		if(next_of_next != NULL) {
			LIST_INSERT_BEFORE(&freeBlocksList, next_of_next, curr_new_block);
		} else {
			LIST_INSERT_TAIL(&freeBlocksList, curr_new_block);
		}

		set_block_data(curr_new_block, ((*next_h) + cur_size), 0);



	} else {
//		cprintf("flag 4 \n");
//		cprintf("-> no merging\n");
		// im the only to be free
		struct BlockElement *curr_new_block = (struct BlockElement*) va;
		void *it_h = (void*)next_h; // points to head of the next block
		void *next = NULL;

		while(*((int32*)it_h) != 1) { // while inside the borders of the heap. (!= end block)
			int32 *cur_h = (int32*)it_h;
			if((*cur_h)%2 == 0) { // if not allocated and after me
				next = (it_h + sizeof(int)); // save the loc of the next free block after me
				break;
			}

			int32 cur_sz = ((*cur_h) - 1);
			it_h += (cur_sz); // move to the next block head
		}



		// insert the free block in the block list.
		if (next == NULL) {
			LIST_INSERT_TAIL(&freeBlocksList, curr_new_block);
		}else {
			LIST_INSERT_BEFORE(&freeBlocksList, ((struct BlockElement*)next), curr_new_block);
		}
		set_block_data(curr_new_block, cur_size, 0);

	}

//	cprintf("[free block after ex.]-> free block list size: %d\n",LIST_SIZE(&freeBlocksList));
//	cprintf("---------------------------\n");
}

//=========================================
// [6] REALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *realloc_block_FF(void* va, uint32 new_size)
{
	 //TODO: [PROJECT'24.MS1 - #08] [3] DYNAMIC ALLOCATOR - realloc_block_FF
	 //COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("realloc_block_FF is not implemented yet");
	//Your Code is Here...
//	REQUIREMENTS:
//	1) Resize the block at va to new size
//	2) if next block has enough space, resize in same place 'va'
//	3) Else relocate, using alloc_FF.
//	4) if no space is found, use sbrk, to create more space
//	5) if still no sufficient space, return NULL
//	6) make sure to handle the shrinking of the block as well.
//
//	INITIAL VALIDATIONS:
//	1) va && 0 = free_block	& return NULL
//	2) NULL && size = alloc_ff & return the new address
//	3) NULL && 0 = do nothing :0



//		VALIDATIONS:

		int32 total_new_size = new_size + 2*sizeof(int);
		struct BlockElement *currentBlock = (struct BlockElement*) va;

		//new size is zero --> free block
		//new size is zero --> free block
		if(va != NULL && new_size == 0){
			free_block((void*)currentBlock);
			return NULL;
		}
		//va is NULL and new size is zero
		if(va == NULL && new_size == 0) {
			return NULL;
		}
		//va is NULL --> alloc_block_ff
		if(va == NULL && new_size > 0) {
			return alloc_block_FF(new_size);
		}


		////////////////////////////////////////

		int32 cur_size = get_block_size((void*) currentBlock);
		cur_size = cur_size - (cur_size & 1);

		uint32 *next_h = (uint32*)(va + cur_size - sizeof(int));
		int32 is_alloc_next = (int32)(*next_h) % 2;
		int32 size_difference = total_new_size - cur_size;

		struct BlockElement *next = (struct BlockElement*)((void*)next_h + sizeof(int));

///////////////CASE 1:///////////////
		if(size_difference < 0) {
			// return address after all conditions
			set_block_data((void *)currentBlock,(uint32)total_new_size, 1); //allocated done
			if(is_alloc_next == 1) {

//				if next is allocated & the size difference is (-ve)
//				allocate at same location & check if size 'diff > 16'

				if( (-1 * size_difference) >= 4*sizeof(int)) {
					// create the free block
					struct BlockElement *new_free_block =(struct BlockElement*) (va + total_new_size);
					set_block_data(new_free_block, (-1 * size_difference), 0);
					// find it's place in the freeBlocksList. & insert it.
					struct BlockElement *it = NULL;
					struct BlockElement *prev = NULL;

					LIST_FOREACH(it, &freeBlocksList){
						if ((void*)it > (void*)new_free_block) break;
						prev = it;
					}
					if (prev == NULL) {
						LIST_INSERT_HEAD(&freeBlocksList, new_free_block);
					}else{
						LIST_INSERT_AFTER(&freeBlocksList, prev, new_free_block);
					}
				}
				else
				{   // no sufficient size for new free block
					// the same block will not change
					// internal fragmentation
					set_block_data((void *)currentBlock, (uint32)cur_size, 1); // might be incorrect, ASK TA.
				}
			}
			else if(is_alloc_next == 0) {
				//merge
				int32 total_free_size = get_block_size((void*)next) + (-1 * size_difference);
				struct BlockElement *new_free_block = (struct BlockElement *)(va + total_new_size);
				struct BlockElement *prev = next->prev_next_info.le_prev;

				LIST_REMOVE(&freeBlocksList, next);
				set_block_data((void *)new_free_block, (uint32)total_free_size, 0);//free

				if (prev == NULL) {
					LIST_INSERT_HEAD(&freeBlocksList, new_free_block);
				}else{
					LIST_INSERT_AFTER(&freeBlocksList, prev, new_free_block);
				}
			}


			return currentBlock;
		}


///////////////CASE 2:///////////////
		else if(size_difference > 0){
			if(is_alloc_next == 1){

				//search a suitable free block for new_Size
				void* newAddress = alloc_block_FF(new_size);
				free_block((void *)currentBlock);
				return newAddress;


			}
			else if(is_alloc_next == 0) {
				//merge

				if(get_block_size((void *)next) >= size_difference)
				{
					struct BlockElement *next = (struct BlockElement*)((void *)next_h + sizeof(int));
					struct BlockElement *prev = next->prev_next_info.le_prev;
					LIST_REMOVE(&freeBlocksList, next);
					set_block_data((void *)currentBlock, (uint32)total_new_size, 1);
					if(get_block_size((void *)next) == size_difference){
						return currentBlock;
					}

					int32 free_size_difference = get_block_size((void*)next) - size_difference;

					if(free_size_difference  >= 4*sizeof(int)){
						struct BlockElement *new_free_block = (struct BlockElement *)(va + total_new_size);
						set_block_data((void*)new_free_block, (uint32)free_size_difference, 0);
						if (prev == NULL){
							LIST_INSERT_HEAD(&freeBlocksList, new_free_block);
						}
						else if (prev != NULL){
							LIST_INSERT_AFTER(&freeBlocksList , prev , new_free_block);
						}

					}
					else if(free_size_difference  <= 4*sizeof(int)){set_block_data((void *)currentBlock, (uint32)(total_new_size + free_size_difference), 1);}
					return currentBlock;

				}
				else if (get_block_size((void *)next) < size_difference){
					void* newAddress = alloc_block_FF(new_size);
					free_block((void *)currentBlock);
					return newAddress;
				}
			}
		}
///////////////CASE 3:///////////////
		else if(total_new_size - cur_size == 0){ //no change
			return currentBlock;
		}
		return NULL;
}


/*********************************************************************************************/
/*********************************************************************************************/
/*********************************************************************************************/
//=========================================
// [7] ALLOCATE BLOCK BY WORST FIT:
//=========================================
void *alloc_block_WF(uint32 size)
{
	panic("alloc_block_WF is not implemented yet");
	return NULL;
}

//=========================================
// [8] ALLOCATE BLOCK BY NEXT FIT:
//=========================================
void *alloc_block_NF(uint32 size)
{
	panic("alloc_block_NF is not implemented yet");
	return NULL;
}
