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
//==================================
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	if (ptrPageInfo < &pageBlockInfoArr[0] || ptrPageInfo >= &pageBlockInfoArr[DYN_ALLOC_MAX_SIZE/PAGE_SIZE])
			panic("to_page_va called with invalid pageInfoPtr");
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================
// [2] GET PAGE INFO OF PAGE VA:
//==================================
__inline__ struct PageInfoElement * to_page_info(uint32 va)
{
	int idxInPageInfoArr = (va - dynAllocStart) >> PGSHIFT;
	if (idxInPageInfoArr < 0 || idxInPageInfoArr >= DYN_ALLOC_MAX_SIZE/PAGE_SIZE)
		panic("to_page_info called with invalid pa");
	return &pageBlockInfoArr[idxInPageInfoArr];
}

//=============================================================================
// [3] GET THE SMALLEST POWER OF 2 GREATER THAN OR EQUAL THE GIVEN NUMBER :
//=============================================================================
__inline__ uint32 nearest_pow2_ceil_temp(uint32 x)
{
	if (x<=8)
	return 8;
    int power = 2;
    x--;
    while (x >>= 1) {
    power <<= 1;
    }
    return power;
}

// 4 => 8
//=============================================================================
// [4] GET THE SMALLEST POWER OF 2 GREATER THAN OR EQUAL THE GIVEN NUMBER :
//=============================================================================
__inline__ uint32 log2_ceil_temp(uint32 x)
{
    if (x <= 1) return 1;
    int bits_cnt = 2;
    x--;
    while (x >>= 1) {
        bits_cnt++;
    }
    return bits_cnt;
}
//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=============================================================================
// GET THE SMALLEST POWER OF 2 GREATER THAN OR EQUAL THE GIVEN NUMBER :
//=============================================================================
int get_next_pow2(int n)  {
	// compute the next highest power of 2 of 32-bit n


	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;
	return n;
}

//=============================================================================
// GET THE POWER P SUCH THAT 2^P = X
//=============================================================================
__inline__ uint32 get_exponent(uint32 x)
{
    if (x <= 1) return 1;
    int bits_cnt = 0;
    while (x) {
    	x >>= 1;
        bits_cnt++;
    }
    return bits_cnt - 1;
}

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
bool is_initialized = 0;

void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
	//Your code is here
	//Comment the following line
	//panic("initialize_dynamic_allocator() Not implemented yet");

	// DA Limits Initialization
    		dynAllocStart = daStart;
    		dynAllocEnd = daEnd;
    // Array of Page Info Initialization
    for (int i = 0; i < (LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1); ++i)
        {
            LIST_INIT(&freeBlockLists[i]);
        }
    // Free Page List Initialization
    		LIST_INIT(&freePagesList);
    // Free Block Lists Initialization
	// Sorry for That Mistake :(
    for (uint32 i = 0; i < ((daEnd - daStart + 1) / PAGE_SIZE); ++i)

        {
            // Reset necessary fields in PageInfoElement
            pageBlockInfoArr[i].block_size = 0;
            pageBlockInfoArr[i].num_of_free_blocks = 0;

            // Add all PageInfo elements to the freePagesList
            LIST_INSERT_TAIL(&freePagesList, &(pageBlockInfoArr[i]));
        }

}

//===========================
// [2] GET BLOCK SIZE:
//===========================



__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here
	//Comment the following line
	//panic("get_block_size() Not implemented yet");
	// Convert VA to the corresponding PageInfoElement pointer
	 	 struct PageInfoElement *ptr_page_info = to_page_info((uint32)va);
    // Return the stored block size
	     return ptr_page_info->block_size;
}



//===========================
// 3) ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block
	//Your code is here
	//Comment the following line
	//panic("alloc_block() Not implemented yet");
	// Stop Working if the size of the block is smaller than 2 bytes (LOG2_MIN_SIZE) or equal Zero
		if (size == 0) return NULL;
	// Stop Working if the size of the block is larger than 32 Mega bytes (LOG2_MAX_SIZE)
		if (size > DYN_ALLOC_MAX_BLOCK_SIZE) return NULL;

		int SizeInBytes;
    // Calculate the nearest power of two and determine the list index
	//uint32 required_block_size = calc_next_power_of_two(size);
	    SizeInBytes = (size <= 8) ? 8 : get_next_pow2(size);
	    int idx = get_exponent(SizeInBytes) - (uint32)3;

	    //cprintf("size: %d, ourSize: %d, power: %d, idx: %d\n", size, SizeInBytes, olog2_ceil(SizeInBytes), idx);

	    //size = get_next_pow2(size);
	// Create our New Block for our new Size
	    struct BlockElement *NewblockPointer = NULL;
	// First Case => Block is Available and we just need to allocate it
	    if (!LIST_EMPTY(&(freeBlockLists[idx])))
	        {

	    	//cprintf("case 1\n");
	    		NewblockPointer = LIST_FIRST(&(freeBlockLists[idx]));
	            // Update Page Info: Decrement the count of free blocks

	    		struct PageInfoElement *ptr_page_info = to_page_info((uint32)NewblockPointer);
	            ptr_page_info->num_of_free_blocks--;

	            //cprintf("freeBlockLists[idx] size before: %d\n", (freeBlockLists[idx]).size);
	            LIST_REMOVE(&(freeBlockLists[idx]), NewblockPointer);
	            //cprintf("freeBlockLists[idx] size after: %d\n", (freeBlockLists[idx]).size);



	            return (void*)NewblockPointer;

	        }

	// Second Case => No Block Available , we will check if there is a free page
	    if (!LIST_EMPTY(&freePagesList))
	        {

	    	//cprintf("case 2\n");

	    	    // take a free page from the freePagesList
	            struct PageInfoElement *ptr_page_info = LIST_FIRST(&freePagesList);

	            //cprintf("freePagesList size before: %d\n", (freePagesList).size);
	            LIST_REMOVE(&freePagesList, ptr_page_info);
	            //cprintf("freePagesList size after: %d\n", (freePagesList).size);

	            //cprintf("f 1\n");
	            // Acquire a physical page and map it
	            uint32 page_va = to_page_va(ptr_page_info);
	            get_page((void*)page_va);

	            //cprintf("f 2\n");
	            // Initialize Page Info for this new page
	            ptr_page_info->block_size = SizeInBytes;
	            uint32 num_blocks = PAGE_SIZE / SizeInBytes;
	            ptr_page_info->num_of_free_blocks = num_blocks;

	            //cprintf("f 3\n");
	            // Split page into blocks and add ALL to the free list
	            for (uint32 i = 0; i < num_blocks; ++i)
	            {
	            	//cprintf("f 41\n");
	                uint32 block_va = page_va + (i * SizeInBytes);
	                //cprintf("f 42\n");
	                struct BlockElement *TEmpblock = (struct BlockElement *)block_va;
	                //cprintf("f 43\n");
	                LIST_INSERT_TAIL(&(freeBlockLists[idx]), TEmpblock);
	                //cprintf("f 44\n");
	            }

	            //cprintf("f 5\n");
	            // Return the first block (now that the list is populated)
	            NewblockPointer = LIST_FIRST(&(freeBlockLists[idx]));
	            ptr_page_info->num_of_free_blocks--;
	            //cprintf("f 6\n");
	            LIST_REMOVE(&(freeBlockLists[idx]), NewblockPointer);

	            return (void*)NewblockPointer;
	        }

	// Third Case => No Free Pages , Choose a Bigger Size Block , Waste of memory
	        for (int large_idx = idx + 1; large_idx < (LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1); ++large_idx)
	        {
	        	//cprintf("case 3 - for body\n");
	            if (!LIST_EMPTY(&freeBlockLists[large_idx]))
	            {
	            	//cprintf("case 3 - if body\n");
	                // Found a larger block. Use it, causing internal fragmentation
	            	NewblockPointer = LIST_FIRST(&(freeBlockLists[large_idx]));

	            	// Update Page Info
	                struct PageInfoElement *ptr_page_info = to_page_info((uint32)NewblockPointer);
	                ptr_page_info->num_of_free_blocks--;

	                // remove the block from the list
	                LIST_REMOVE(&(freeBlockLists[large_idx]), NewblockPointer);
	                return (void*)NewblockPointer;

	            }
	        }
	// Fourth Case => No Free Pages , No Free Blocks
	     panic("alloc_block() failed: No free resources available.");
	     return NULL;
	//TODO: [PROJECT'25.BONUS#1] DYNAMIC ALLOCATOR - block if no free block
}



//===========================
// [4] FREE BLOCK:
//===========================
void free_block(void *va)
{
    //==================================================================================
    //DON'T CHANGE THESE LINES==========================================================
    //==================================================================================
    {
        assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
    }
    //==================================================================================
    //==================================================================================

    //TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
    //Your code is here
    //Comment the following line
    //panic("free_block() Not implemented yet");
    // Find Page Info of the Block using VA
       struct PageInfoElement *The_Block_Page = to_page_info((uint32)va);
    // Get block size
       uint32 Size_Not_Index = The_Block_Page->block_size;
       uint32 Index_From_Size = log2_ceil_temp(Size_Not_Index) - 4;
    // Insert at head
        LIST_INSERT_HEAD(&freeBlockLists[Index_From_Size], (struct BlockElement*)va);
    // 2. Update Page Info
        The_Block_Page->num_of_free_blocks++; // Increment free count
    // 3. Check for Full Page Free
        uint32 Max_Blocks = PAGE_SIZE / Size_Not_Index;
        if (The_Block_Page->num_of_free_blocks == Max_Blocks)
        {
            // A. Remove all blocks associated with this page from the free list
        	// Get page VA
            uint32 page_va = to_page_va(The_Block_Page);
            struct BlockElement *element = LIST_FIRST(&freeBlockLists[Index_From_Size]);
            while (element != NULL) {
            	// Get next before removal
                struct BlockElement *next_element = LIST_NEXT(element);
                // Check if block belongs to the current page (by checking the PageInfoElement associated with the block VA)
                if (to_page_va(to_page_info((uint32)element)) == page_va) {
                // Remove the block
                    LIST_REMOVE(&freeBlockLists[Index_From_Size], element);
                }
                element = next_element;
            }


            // B. Return the page to the Kernel Page Allocator
            return_page((void*)page_va); //
            // C. Reset P and add to freePagesList
            The_Block_Page->block_size = 0;
            The_Block_Page->num_of_free_blocks = 0;
            LIST_INSERT_TAIL(&freePagesList, The_Block_Page);
        }
}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


void transfer_data(uint32 old_address, uint32 new_address, uint32 total_bytes) {
	// Transfers data from old address to the new one
	memcpy((void*) new_address, (void*) old_address, total_bytes);
}

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	//panic("realloc_block() Not implemented yet");


	// it is guaranteed that va != NULL

    new_size = new_size <= 8 ? 8 : get_next_pow2(new_size);
    uint32 old_size = get_block_size(va);

    if(new_size == old_size) return va;

	// try to allocate the new size
	// if allocated then
		// transfer data
		// free old memory
		// return new address
	// if not then return null

	void* new_address = alloc_block(new_size);

	if(new_address != NULL) {

		uint32 bytes = new_size > old_size ? old_size : new_size;

		transfer_data((uint32)new_address, (uint32)va, bytes);

		free_block(va);

		return new_address;

	}


    return NULL;

}
