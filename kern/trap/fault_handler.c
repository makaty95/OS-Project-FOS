/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */


#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>
#include <kern/mem/kheap.h>

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;
int cnt = 0;
//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE ;
}
void setPageReplacmentAlgorithmCLOCK(){_PageRepAlgoType = PG_REP_CLOCK;}
void setPageReplacmentAlgorithmFIFO(){_PageRepAlgoType = PG_REP_FIFO;}
void setPageReplacmentAlgorithmModifiedCLOCK(){_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;}
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal(){_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;}
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps){_PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps;}
/*2024*/ void setFASTNchanceCLOCK(bool fast){ FASTNchanceCLOCK = fast; };
/*2025*/ void setPageReplacmentAlgorithmOPTIMAL(){ _PageRepAlgoType = PG_REP_OPTIMAL; };

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmOPTIMAL(){if(_PageRepAlgoType == PG_REP_OPTIMAL) return 1; return 0;}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt){_EnableModifiedBuffer = enableIt;}
uint8 isModifiedBufferEnabled(){  return _EnableModifiedBuffer ; }

void enableBuffering(uint32 enableIt){_EnableBuffering = enableIt;}
uint8 isBufferingEnabled(){  return _EnableBuffering ; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length;}
uint32 getModifiedBufferLength() { return _ModifiedBufferLength;}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [0] INIT HANDLER:
//==================
void fault_handler_init()
{
	//setPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX);
	//setPageReplacmentAlgorithmOPTIMAL();
	setPageReplacmentAlgorithmCLOCK();
	//setPageReplacmentAlgorithmModifiedCLOCK();
	enableBuffering(0);
	enableModifiedBuffer(0) ;
	setModifiedBufferLength(1000);
}
//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault  = 0;
extern uint32 sys_calculate_free_frames() ;



uint32 isMarkedPage(struct Env* faulted_env, uint32 va) {

	// This will check if the faulted page is marked in the user heap or not

    uint32* ptr_page_table = NULL;
    int ret = get_page_table(faulted_env->env_page_directory, va, &ptr_page_table);
    if(ptr_page_table == NULL) {
    	return 0; // table not in memory -> not marked
	}

    uint32 idx = PTX(va);
    return ((ptr_page_table[idx] & PERM_UHPAGE));

}


//======================================================================================================//
//======================================== <MY MODULE 6 HELPER FUNCTIONS> =============================//
//======================================================================================================//


void update_pointer(struct Env* faulted_env, struct WorkingSetElement* wse) {
#if USE_KHEAP
	faulted_env->page_last_WS_element = wse->prev_next_info.le_next;
	if(faulted_env->page_last_WS_element == NULL) {
		faulted_env->page_last_WS_element = faulted_env->page_WS_list.lh_first;
	}
#endif

}


void modified_clock_replace(
		struct Env* faulted_env,
		uint32 faulted_va,
		struct WorkingSetElement* victim_wse) {

	// this method will handle the overall process of replacement
	// it removes the victim, allocate memory for the faulted page, add the new working set element to the list,
	// write the victim on memory if modified and load the new faulted page to memory

	//cprintf("modified_clock_replace() \n");
	//--------------------------------------------------------------------------------------------------//
	// Creating the frame for the faulted page.
	struct FrameInfo *new_frame = NULL;
	int ret = allocate_frame(&new_frame); // the return is useless here.
	//--------------------------------------------------------------------------------------------------//

	//--------------------------------------------------------------------------------------------------//
	// mapping the allocated memory
	uint32 perms = (PERM_USER | PERM_PRESENT | PERM_WRITEABLE | PERM_USED);
	int rett = map_frame(faulted_env->env_page_directory, new_frame, faulted_va, perms);
	//--------------------------------------------------------------------------------------------------//

#if USE_KHEAP
	//----------------------------------------------------------//
	// add the new working set element
	struct WorkingSetElement* new_alloc_ws_element =
	env_page_ws_list_create_element(faulted_env, faulted_va);
	LIST_INSERT_AFTER(&(faulted_env->page_WS_list), victim_wse, new_alloc_ws_element);
	//----------------------------------------------------------//
#endif

	//----------------------------------------------------------//
	// writing the victim page on disk if modified
	int p = pt_get_page_permissions(faulted_env->env_page_directory, victim_wse->virtual_address);
	if((p&PERM_MODIFIED)) {
		uint32 *page_table = NULL;
		struct FrameInfo *frame_info = get_frame_info(faulted_env->env_page_directory, victim_wse->virtual_address, &page_table);
		int ret = pf_update_env_page(faulted_env, victim_wse->virtual_address, frame_info);
		if(ret == E_NO_PAGE_FILE_SPACE) {
			cprintf("[Our logs] can't place modified page on disk (This should be handled out of the project scope)\n");
		}
	}
	//----------------------------------------------------------//


	//----------------------------------------------------------//
	// remove the victim
	env_page_ws_invalidate(faulted_env, victim_wse->virtual_address); // removing it's entry & unmapping it's memory
	//----------------------------------------------------------//

	//----------------------------------------------------------//
	// load the new page from disk
	int r = pf_read_env_page(faulted_env, (void*)faulted_va);
	//----------------------------------------------------------//

#if USE_KHEAP
	// update pointer so it point to the next WSE
	update_pointer(faulted_env, new_alloc_ws_element);

#endif
	//cprintf("modified_clock_replace() return\n");

}


int perms_are(uint32 expected_used, uint32 expected_modified, struct WorkingSetElement* wse, struct Env* faulted_env) { //OK
	// check if the perms of the given working set element are as expected

	//cprintf("perms_are() \n");
	// get it's permissions
	int perms = pt_get_page_permissions(faulted_env->env_page_directory, wse->virtual_address);
	//cprintf("flag 0 \n");
	uint32 actual_used = perms&PERM_USED;
	uint32 actual_modified = perms&PERM_MODIFIED;

	//cprintf("flag 1 \n");
	// check
	int usedBit_as_expected = (expected_used && actual_used) || (!expected_used && !actual_used);
	int modifiedBit_as_expected = (expected_modified && actual_modified) || (!expected_modified && !actual_modified);

	//cprintf("perms_are() return\n");
	return (usedBit_as_expected && modifiedBit_as_expected);

}




int Trial1(struct Env* faulted_env, uint32 faulted_va) { //OK
	// Search for a working set element which have (0,0) -> used and modified = 0
		// if found then:
			// replace it and modify last_ws_ptr to the next element
			// return 1;
		// if not then:
			// return 0;

	//cprintf("Trial1 \n");
	//env_page_ws_print(faulted_env);
	struct WorkingSetElement* wse = NULL;
#if USE_KHEAP
	wse = faulted_env->page_WS_list.lh_first;

	//cprintf("flag 0 \n");

	// first, check the start element
	if(perms_are(0, 0, wse, faulted_env)) {
		// replace the first and return
		modified_clock_replace(faulted_env, faulted_va, wse);

		//cprintf("Trial1 return1 \n");
		return 1; // 1 means found and replaced
	}

	{ // update the iterating pointer
		wse = wse->prev_next_info.le_next;
		if(wse == NULL) wse = faulted_env->page_WS_list.lh_first; // Circular iteration
		//cprintf("flag 1 \n");
	}

	while(wse != NULL) {
		if(perms_are(0, 0, wse, faulted_env)) {
			//cprintf("flag 2 \n");
			// replace here and return
			modified_clock_replace(faulted_env, faulted_va, wse);

			//cprintf("Trial1 return2 \n");
			return 1; // 1 means found and replaced
		}

		// update the iterating pointer
		wse = wse->prev_next_info.le_next;
		//cprintf("flag 6 \n");
	}

	//cprintf("Trial1 return3 \n");
	// can't find (0, 0) WSE
	return 0;

#else
	return -1;
#endif
	//----------------------------------------------------------//


}

int Trial2(struct Env* faulted_env, uint32 faulted_va) {
	// Search for a working set element which have (0,1) -> used = 0
	// at your way move the pointer and modify each one used bit to 0 in the way
		// if found then:
			// replace it and modify last_ws_ptr to the next element after it
			// return 1;
		// if not then:
			// return 0;

	//cprintf("Trial2 \n");
	//env_page_ws_print(faulted_env);
#if USE_KHEAP

	struct WorkingSetElement* ptr_wse = faulted_env->page_last_WS_element;
	struct WorkingSetElement* wse = faulted_env->page_last_WS_element;

	//cprintf("flag 1 \n");
	// first, check the page_last_WS_element element
	if(perms_are(0, 1, wse, faulted_env)) {
		//cprintf("flag 2 \n");

		// replace the first and return
		modified_clock_replace(faulted_env, faulted_va, wse);

		//cprintf("Trial2 return1 \n");
		return 1; // 1 means found and replaced
	}

	{ // update
		//cprintf("flag 2 \n");

		// make it's used bit 0
		pt_set_page_permissions(faulted_env->env_page_directory, wse->virtual_address, 0, PERM_USED);
		//cprintf("flag 3 \n");

		// update pointer so it point to the next WSE
		update_pointer(faulted_env, wse);

		// update the iterating pointer
		wse = wse->prev_next_info.le_next;
		if(wse == NULL) wse = faulted_env->page_WS_list.lh_first; // Circular iteration
	}

	while(wse != ptr_wse) {
		if(perms_are(0, 1, wse, faulted_env)) {
			//cprintf("flag 4 \n");
			// replace here and return
			modified_clock_replace(faulted_env, faulted_va, wse);

			//cprintf("Trial2 return2 \n");
			return 1; // 1 means found and replaced
		}
		//cprintf("flag 7 \n");

		// make it's used bit 0
		pt_set_page_permissions(faulted_env->env_page_directory, wse->virtual_address, 0, PERM_USED);

		// update pointer so it point to the next WSE
		update_pointer(faulted_env, wse);

		// update iterating pointer
		wse = wse->prev_next_info.le_next;
		if(wse == NULL) wse = faulted_env->page_WS_list.lh_first; // Circular iteration
	}
	//cprintf("flag 9 \n");
	// can't find (0, 1) element
	//cprintf("Trial2 return3 \n");
	return 0;

#else
	return -1;
#endif

}
//======================================================================================================//
//======================================== </MY MODULE 6 HELPER FUNCTIONS> =============================//
//======================================================================================================//

struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//cprintf("************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env)
	{
		num_repeated_fault++ ;
		if (num_repeated_fault == 3)
		{
			print_trapframe(tf);
			panic("Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32)tf->tf_eip;
	last_fault_va = fault_va ;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap)
	{
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env && fault_va >= (uint32)cur_env->kstack && fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va >= (uint32)c->stack && fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!", c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else
	{
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL)
	{
		cprintf("\nFaulted VA = %x\n", fault_va);
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ( (faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		faulted_env->tableFaultsCounter ++ ;
		table_fault_handler(faulted_env, fault_va);
	}
	else
	{
		if (userTrap)
		{
			/*============================================================================================*/
			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #2 Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			//your code is here

			int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
			//cprintf("\n[Our logs] faulted va = %p \n", fault_va);

			if(perms == -1) {
				cprintf("\n[Our logs] [This is a rare may be not applicable case] faulted va perms = -1 (no page table in memory.) \n");
			}

//			if(fault_va >= KERNEL_HEAP_START && fault_va < KERNEL_HEAP_MAX) {
//				cprintf("Area 1 \n");
//			} else if(fault_va >= KERNEL_BASE) {
//				cprintf("Area 2 \n");
//			} else if(fault_va >= USER_LIMIT) {
//				cprintf("Area 3 \n");
//			} else if(fault_va >= USER_TOP) {
//				cprintf("Area 4 \n");
//			} else if(fault_va >= USTACKTOP) {
//				cprintf("Area 5 \n");
//			} else if(fault_va >= USTACKBOTTOM) {
//				cprintf("Area 6 \n");
//			} else if(fault_va >= USER_HEAP_MAX) {
//				cprintf("Area 7 \n");
//			} else if(fault_va >= USER_HEAP_START) {
//				cprintf("Area 8 \n");
//			} else {
//				cprintf("Area 9 \n");
//			}

			// if pointing to kernel address
			if(fault_va >= USER_LIMIT) {
				//cprintf("\n case 1\n");
				env_exit();
			}


			// if present and not writable but user try to write
			if ((perms&PERM_PRESENT) && !(perms & PERM_WRITEABLE)) {
				//cprintf("\n case 2\n");
				env_exit();

			}


			int inUserHeap = fault_va < USER_HEAP_MAX && fault_va >= USER_HEAP_START;
			// if not marked page in user heap
			if(inUserHeap && !(perms&PERM_UHPAGE)) {
				//cprintf("\n case 3\n");
				env_exit();
			}




			/*============================================================================================*/
		}

		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
		if (perms & PERM_PRESENT)
			panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va) ;
		/*============================================================================================*/


		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter ++ ;

//				cprintf("[%08s] user PAGE fault va %08x\n", faulted_env->prog_name, fault_va);
//				cprintf("\nPage working set BEFORE fault handler...\n");
//				env_page_ws_print(faulted_env);
		//int ffb = sys_calculate_free_frames();

		if(isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
			page_fault_handler(faulted_env, fault_va);
		}

		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(faulted_env);
		//		int ffa = sys_calculate_free_frames();
		//		cprintf("fault handling @%x: difference in free frames (after - before = %d)\n", fault_va, ffa - ffb);
	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}



//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env * curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

//=========================
// [3] PAGE FAULT HANDLER:
//=========================
/* Calculate the number of page faults according th the OPTIMAL replacement strategy
 * Given:
 * 	1. Initial Working Set List (that the process started with)
 * 	2. Max Working Set Size
 * 	3. Page References List (contains the stream of referenced VAs till the process finished)
 *
 * 	IMPORTANT: This function SHOULD NOT change any of the given lists
 */

int get_optimal_victim(struct PageRef_List* pageReferences, int ref_idx, uint32* tempWS, int maxWSSize) {
	// return the index in the WS for the victim

	// move the reference to the ref_idx + 1 element which the search will start from
	struct PageRefElement *ref = LIST_FIRST(pageReferences);
	for (int idx = 0; idx < ref_idx + 1; idx++) ref = LIST_NEXT(ref);

	int dist[maxWSSize];
	for(int i = 0; i<maxWSSize; i++) dist[i] = -1;

	int distance = 0;
	while(ref != NULL) {

		uint32 curr_va = ROUNDDOWN(ref->virtual_address, PAGE_SIZE);

		// check if it exists in the WS
		for(int i = 0; i<maxWSSize; i++) {
			if(tempWS[i] == curr_va && dist[i] == -1) {
				dist[i] = distance;
				break;
			}
		}

		distance++;
		ref = LIST_NEXT(ref);
	}

    for (int i = 0; i < maxWSSize; i++) {
        if (dist[i] == -1) return i;
    }

	int longest_idx = 0;
	int longest_rank = 0;

	int victim = 0;
	int max_dist = dist[0];

	for (int i = 1; i < maxWSSize; i++) {
		if (dist[i] > max_dist) {
			max_dist = dist[i];
			victim = i;
		}
	}

	return victim;
}


int get_optimal_num_faults(struct WS_List *initWorkingSet, int maxWSSize,
		struct PageRef_List *pageReferences) {
	//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #2 get_optimal_num_faults
	//Your code is here
	//Comment the following line
	//panic("get_optimal_num_faults() is not implemented yet...!!");

	//cprintf("get_optimal_num_faults called. \n ");






	int faults = 0;
	uint32* tempWS = kmalloc(maxWSSize * sizeof(uint32));

	// init by -1 -> free WS
	for(int i = 0; i<maxWSSize; i++) tempWS[i] = (uint32)-1;

	int wsCount = 0;


	// copying WS into some temp WS
	struct WorkingSetElement *wse;
	LIST_FOREACH(wse, initWorkingSet) {
		if (wsCount < maxWSSize) {
			tempWS[wsCount++] = ROUNDDOWN(wse->virtual_address, PAGE_SIZE);
		}
	}

/*
	// For each reference:
	// If it exists: then do nothing
	// Else {
	 * 	if(ws have place): place the new faulted page in it
	 * 	else {
	 * 		we need to replace by taking the optimal victim
	 * 		which is the least to be used in future
	 *
	 * 	}
	 * }
*/
	struct PageRefElement *ref;
	int ref_idx = 0;
	LIST_FOREACH(ref, pageReferences) {

		uint32 refVA = ROUNDDOWN(ref->virtual_address, PAGE_SIZE);

		// search for the page in WS
		int found = 0;
		for (int i = 0; i < maxWSSize; i++) {
			if (tempWS[i] == refVA) {
				found = 1;
				break;
			}
		}

	    if (!found) {
	        faults++;

	        if (wsCount < maxWSSize) {
	            for(int i = 0; i < maxWSSize; i++) {
	                if (tempWS[i] == (uint32)-1) {
	                    tempWS[i] = refVA;
	                    break;
	                }
	            }
	            wsCount++;
	        } else {
	            int victim_idx = get_optimal_victim(pageReferences, ref_idx, tempWS, maxWSSize);
	            assert(victim_idx >= 0 && victim_idx < maxWSSize);
	            tempWS[victim_idx] = refVA;
	        }
	    }

	    ref_idx++;

	}

	kfree(tempWS);
	return faults;
	//======================>


}



void print_activeList() {

	struct Env* e = get_cpu_proc();

	uint32 i = 0;
	cprintf("PAGE WS:\n");
	struct WorkingSetElement *wse = NULL;
	LIST_FOREACH(wse, &(e->ActiveList))
	{
		uint32 virtual_address = wse->virtual_address;
		uint32 time_stamp = wse->time_stamp;

		uint32 perm = pt_get_page_permissions(e->env_page_directory, virtual_address) ;
		char isModified = ((perm&PERM_MODIFIED) ? 1 : 0);
		char isUsed= ((perm&PERM_USED) ? 1 : 0);
		char isBuffered= ((perm&PERM_BUFFERED) ? 1 : 0);

		cprintf("%d: %x",i, virtual_address);

		//2021
		cprintf(", used= %d, modified= %d, buffered= %d, time stamp= %x, sweeps_cnt= %d",
				isUsed, isModified, isBuffered, time_stamp, wse->sweeps_counter) ;

		cprintf("\n");
		i++;
	}
	for (; i < e->page_WS_max_size; ++i)
	{
		cprintf("EMPTY LOCATION\n");
	}

}

void initActiveList(struct Env* faulted_env) {
	//env_page_ws_print(faulted_env);
	//cprintf("\n\n");
	//			cprintf("faulted_env->ActiveListSize = %d \n", faulted_env->ActiveListSize);
	//			cprintf("faulted_env->page_WS_max_size = %d \n", faulted_env->page_WS_max_size);

#if USE_KHEAP


	// [1] Copying all WSEs : page_WS_list --> ActiveList
	struct WorkingSetElement *WSE;
	LIST_FOREACH_SAFE(WSE, &(faulted_env->page_WS_list), WorkingSetElement) {
		struct WorkingSetElement *WSE_new =
				env_page_ws_list_create_element(faulted_env, WSE->virtual_address);

		LIST_INSERT_TAIL(&(faulted_env->ActiveList), WSE_new);
	}

#endif

	//cprintf("active list after copying size = %d \n", LIST_SIZE(&(faulted_env->ActiveList)));
}

void page_fault_handler(struct Env * faulted_env, uint32 fault_va)
{
#if USE_KHEAP
	if (isPageReplacmentAlgorithmOPTIMAL())
	{
		//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #1 Optimal Reference Stream
		//Your code is here
		//Comment the following line
		//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");

		fault_va = ROUNDDOWN(fault_va, PAGE_SIZE);

	    uint32* ptr_page_table = NULL;
	    int table_state = get_page_table(faulted_env->env_page_directory, fault_va, &ptr_page_table);
	    if (table_state == TABLE_NOT_EXIST) {
	        ptr_page_table = create_page_table(faulted_env->env_page_directory, fault_va);
	    }

		if(!faulted_env->list_copied) {
			initActiveList(faulted_env);
			faulted_env->list_copied = 1;
		}


		//cprintf("Optimal Reference Stream, va = %p \n", fault_va);
	//	print_activeList();


		int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
		if ((perms & PERM_PRESENT) == 0) { // [2] If faulted page not in memory, read it from disk

			struct FrameInfo *frame = get_frame_info(faulted_env->env_page_directory, fault_va, &ptr_page_table);
			if(frame == NULL) {
				//--------------------------------------------------------------------------------------------------//
				// Creating the frame for the faulted page.
				struct FrameInfo *new_frame = NULL;
				int ret = allocate_frame(&new_frame);
				//--------------------------------------------------------------------------------------------------//

				//--------------------------------------------------------------------------------------------------//
				// mapping the allocated memory
				uint32 perms = (PERM_USER | PERM_PRESENT | PERM_WRITEABLE | PERM_USED);
				int rett = map_frame(faulted_env->env_page_directory, new_frame, fault_va, perms);
				//--------------------------------------------------------------------------------------------------//

				ret = pf_read_env_page(faulted_env, (void*)fault_va);

			} else {
				pt_set_page_permissions(faulted_env->env_page_directory, fault_va, PERM_PRESENT, 0);
			}
		}


		// Check its existence in WS
		int in_active_ws = 0;
		struct WorkingSetElement *wse;
		LIST_FOREACH(wse, &(faulted_env->ActiveList)) {
			if (ROUNDDOWN(wse->virtual_address, PAGE_SIZE) == fault_va) {
				in_active_ws = 1;
				break;
			}
		}

		if (!in_active_ws) { // [3] If the faulted page in the Active WS, do nothing
			// Else, if Active WS is FULL, reset present & delete all its pages
			//cprintf("page not found in active list\n");
			uint32 wsSize = LIST_SIZE(&(faulted_env->ActiveList));
			if (wsSize >= faulted_env->page_WS_max_size) // Else, if Active WS is FULL, reset present & delete all its pages
			{

				//cprintf("current active list is full, size = %d, emptying it.. \n", LIST_SIZE(&(faulted_env->ActiveList)));

				struct WorkingSetElement *iter = LIST_FIRST(&(faulted_env->ActiveList));
				while (iter != NULL)
				{

					struct WorkingSetElement *next = LIST_NEXT(iter);
					uint32 wva = ROUNDDOWN(iter->virtual_address, PAGE_SIZE);

					pt_set_page_permissions(faulted_env->env_page_directory, wva, 0, PERM_PRESENT);

					// delete the page form WS
					LIST_REMOVE(&(faulted_env->ActiveList), iter);
					kfree(iter);

					iter = next;
				}

				//cprintf("after emptying, size = %d \n", LIST_SIZE(&(faulted_env->ActiveList)));

			}

		}


		// insert it into the WS list (Maintaining FIFO order) [Part of Module 6]
		struct WorkingSetElement *new_wse = env_page_ws_list_create_element(faulted_env, fault_va);
		LIST_INSERT_TAIL(&(faulted_env->ActiveList), new_wse);

		//cprintf("after inserting element, size = %d \n", LIST_SIZE(&(faulted_env->ActiveList)));

		// [5] Add faulted page to the end of the reference stream list
		struct PageRefElement *pref = (struct PageRefElement*) kmalloc(sizeof(struct PageRefElement));
		if (pref == NULL) {
			panic("OPTIMAL: kmalloc failed for PageRefElement");
		}
		pref->virtual_address = fault_va;
		LIST_INSERT_TAIL(&(faulted_env->referenceStreamList), pref);


	}
	else
	{

		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
		if(wsSize < (faulted_env->page_WS_max_size))
		{
			//TODO: [PROJECT'25.GM#3] FAULT HANDLER I - #3 placement
			//Your code is here
			//Comment the following line
			//panic("page_fault_handler().PLACEMENT is not implemented yet...!!");

			//cprintf("Placement with va = %p\n", (uint32)fault_va);
			//env_page_ws_print(faulted_env);

			//--------------------------------------------------------------------------------------------------//
			// Checking for page table
			// if it does not exist, then we will create it in the memory.
			uint32 *ptr_page_table = NULL;
			int table_state =  get_page_table(faulted_env->env_page_directory, (uint32)fault_va, &ptr_page_table);
			if (table_state == TABLE_NOT_EXIST) {
				ptr_page_table = create_page_table(faulted_env->env_page_directory, (uint32)fault_va);
			}
			//--------------------------------------------------------------------------------------------------//


			//--------------------------------------------------------------------------------------------------//
			// Creating the frame for the faulted page.
			struct FrameInfo *new_frame = NULL;
			int ret = allocate_frame(&new_frame); // the return is useless here.
			//--------------------------------------------------------------------------------------------------//


			//--------------------------------------------------------------------------------------------------//
			// mapping the allocated memory
			uint32 perms = (PERM_USER | PERM_PRESENT | PERM_WRITEABLE | PERM_USED);
			int rett = map_frame(faulted_env->env_page_directory,
					new_frame, fault_va,
					perms);
			//--------------------------------------------------------------------------------------------------//


			//--------------------------------------------------------------------------------------------------//
			// read the page from disk
			int page_status = pf_read_env_page(faulted_env, (void*)fault_va);
			if(page_status == E_PAGE_NOT_EXIST_IN_PF) {
				//cprintf("[Our logs] Placement: page is not in page file (should be stack or heap page) :)");

				int page_in_user_stack = (fault_va >= USTACKBOTTOM && fault_va < USTACKTOP);
				int page_in_user_heap = (fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX);

				// if the page is not either in the stack or the heap, then
				// it is an illegal address access -> exit the process immediately.
				if(!page_in_user_stack && !page_in_user_heap) {
					unmap_frame(faulted_env->env_page_directory, fault_va); // return the frame
					env_exit();
				}

			}
			//--------------------------------------------------------------------------------------------------//

			//--------------------------------------------------------------------------------------------------//
			// Checking for the size of the working set.
			int current_ws_size = LIST_SIZE(&(faulted_env->page_WS_list));
			int ws_max_size = faulted_env->page_WS_max_size;
			if(current_ws_size < ws_max_size) { // we can add the page.

				// create the WS element.
				struct WorkingSetElement *new_alloc_ws_element =
						env_page_ws_list_create_element(faulted_env, fault_va);

				// insert it into the WS list (Maintaining FIFO order) [Part of Module 6]
				if(faulted_env->page_last_WS_element != NULL) {
					LIST_INSERT_BEFORE(&(faulted_env->page_WS_list), faulted_env->page_last_WS_element, new_alloc_ws_element);
				} else {
					LIST_INSERT_TAIL(&(faulted_env->page_WS_list), new_alloc_ws_element);
				}

				// mark the new loaded page as present
				pt_set_page_permissions(faulted_env->env_page_directory, fault_va, PERM_PRESENT, 0);

				// make the reflect on the list.
				// changed to new FIFO order strategy
				if(LIST_SIZE(&(faulted_env->page_WS_list)) >= faulted_env->page_WS_max_size) {

					// Set the page_last_WS_element to it's appropriate reference
					faulted_env->page_last_WS_element = new_alloc_ws_element->prev_next_info.le_next;
					if(faulted_env->page_last_WS_element == NULL) {
						faulted_env->page_last_WS_element = (LIST_FIRST(&(faulted_env->page_WS_list)));
					}


				}

				//env_page_ws_print(faulted_env);


			} else {
				//cprintf("[Our logs] Placement: working set is at maximum capacity, no place for new page now.");
			}


			//--------------------------------------------------------------------------------------------------//


		}
		else
		{

			if (isPageReplacmentAlgorithmCLOCK())
			{
				//TODO: [PROJECT'25.IM#1] FAULT HANDLER II - #3 Clock Replacement
				//Your code is here
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");

				/*
				 *
				 * */

				//cprintf("Clock Replacement called. \n ");


				struct WorkingSetElement *hand = faulted_env->page_last_WS_element;
				struct WorkingSetElement *cur = hand;
				while (1) {
					uint32 cur_va = cur->virtual_address;
					int pperms = pt_get_page_permissions(faulted_env->env_page_directory, cur_va);

					if (pperms & PERM_USED) {
						pt_set_page_permissions(faulted_env->env_page_directory, cur_va, 0, PERM_USED);
					} else {

						// Replacement here
						//--------------------------------------------------------------------------------------------------//
						// Creating the frame for the faulted page.
						struct FrameInfo *new_frame = NULL;
						int ret = allocate_frame(&new_frame);
						//--------------------------------------------------------------------------------------------------//

						//--------------------------------------------------------------------------------------------------//
						// mapping the allocated memory
						uint32 perms = (PERM_USER | PERM_PRESENT | PERM_WRITEABLE | PERM_USED);
						int rett = map_frame(faulted_env->env_page_directory, new_frame, fault_va, perms);
						//--------------------------------------------------------------------------------------------------//

						//----------------------------------------------------------//
						// add the new working set element
						struct WorkingSetElement* new_alloc_ws_element =
						env_page_ws_list_create_element(faulted_env, fault_va);
						LIST_INSERT_AFTER(&(faulted_env->page_WS_list), cur, new_alloc_ws_element);
						//----------------------------------------------------------//


						//----------------------------------------------------------//
						// writing the page on disk if modified
						int p = pt_get_page_permissions(faulted_env->env_page_directory, cur->virtual_address);
						if((p&PERM_MODIFIED)) {
							uint32 *page_table = NULL;
							struct FrameInfo *frame_info = get_frame_info(faulted_env->env_page_directory,
									cur_va, &page_table);

							ret = pf_update_env_page(faulted_env, cur_va, frame_info);

							if(ret == E_NO_PAGE_FILE_SPACE) {
								//cprintf("[Our logs - Clock replacement] can't place modified page on disk (This should be handled out of the project scope)\n");
							}
						}
						//----------------------------------------------------------//

						//----------------------------------------------------------//
						// remove the victim
						env_page_ws_invalidate(faulted_env, cur_va); // removing it's entry & unmapping it's memory
						//----------------------------------------------------------//



						//----------------------------------------------------------//
						// load the new page from disk
						int r = pf_read_env_page(faulted_env, (void*)fault_va);
						//----------------------------------------------------------//

//							unmap_frame(faulted_env->env_page_directory, cur_va);
//							cur->virtual_address = fault_va;

						// set new_alloc_ws_element
						struct WorkingSetElement *next = LIST_NEXT(new_alloc_ws_element);
						if (next == NULL)
							next = LIST_FIRST(&(faulted_env->page_WS_list));
						faulted_env->page_last_WS_element = next;


						break;
					}


					struct WorkingSetElement *next = LIST_NEXT(cur);
					if (next == NULL) next = LIST_FIRST(&(faulted_env->page_WS_list));
					cur = next;

				}


			}
			else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX))
			{
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #2 LRU Aging Replacement
				//Your code is here
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");


//				cprintf("isPageReplacmentAlgorithmLRU, max_ws_size: %d, curr_size: %d\n",
//						faulted_env->page_WS_max_size, LIST_SIZE(&(faulted_env->page_WS_list)));
//
//				cprintf("cnt: %d \n", cnt);
//
//				cprintf("CURRENT WORKING SET (BEFORE REPLACEMENT): \n");
//				env_page_ws_print(faulted_env);

				// description:
				// search for the victim with the lowest counter
				// once found remove that victim and load the faulted page in the working set

				//----------------------------------------------------------//
				// preparing some data
				struct WorkingSetElement* workingSetElement = NULL;
				struct WS_List* currentWS = &(faulted_env->page_WS_list);
				uint32 *page_dir_ptr = faulted_env->env_page_directory;
				//----------------------------------------------------------//

				//----------------------------------------------------------//
				// Searching for the victim with the lowest counter -> have more AGING
				struct WorkingSetElement* victim_wse = NULL;
				LIST_FOREACH_SAFE(workingSetElement, currentWS, WorkingSetElement) {
					if(victim_wse == NULL || (workingSetElement->time_stamp < victim_wse->time_stamp)) {
						victim_wse = workingSetElement;
					}
				}
				//----------------------------------------------------------//


				//cprintf("flag 1\n");
				//--------------------------------------------------------------------------------------------------//
				// Creating the frame for the faulted page.
				struct FrameInfo *new_frame = NULL;
				int ret = allocate_frame(&new_frame); // the return is useless here.
				//--------------------------------------------------------------------------------------------------//
				//cprintf("flag 2\n");

				//--------------------------------------------------------------------------------------------------//
				// mapping the allocated memory
				uint32 perms = (PERM_USER | PERM_PRESENT | PERM_WRITEABLE | PERM_USED);
				int rett = map_frame(faulted_env->env_page_directory, new_frame, fault_va, perms);
				//--------------------------------------------------------------------------------------------------//

				//cprintf("flag 3\n");
				//----------------------------------------------------------//
				// add the new working set element
				struct WorkingSetElement* new_alloc_ws_element =
				env_page_ws_list_create_element(faulted_env, fault_va);

				//cprintf("flag 4\n");
				//cprintf("flag 3\n");
				LIST_INSERT_AFTER(&(faulted_env->page_WS_list), victim_wse, new_alloc_ws_element);
				//----------------------------------------------------------//


				//----------------------------------------------------------//
				// writing the page on disk if modified
				int p = pt_get_page_permissions(faulted_env->env_page_directory, victim_wse->virtual_address);
				if((p&PERM_MODIFIED)) {
					uint32 *page_table = NULL;
					struct FrameInfo *frame_info = get_frame_info(page_dir_ptr, victim_wse->virtual_address, &page_table);
					ret = pf_update_env_page(faulted_env, victim_wse->virtual_address, frame_info);
					if(ret == E_NO_PAGE_FILE_SPACE) {
						//cprintf("[Our logs] can't place modified page on disk (This should be handled out of the project scope)\n");
					}
				}
				//----------------------------------------------------------//

				//----------------------------------------------------------//
				// remove the victim
				env_page_ws_invalidate(faulted_env, victim_wse->virtual_address); // removing it's entry & unmapping it's memory
				//----------------------------------------------------------//

				//----------------------------------------------------------//
				// load the new page from disk
				int r = pf_read_env_page(faulted_env, (void*)fault_va);

				//----------------------------------------------------------//
				//cprintf("flag 7\n");

			}
			else if (isPageReplacmentAlgorithmModifiedCLOCK())
			{
				//TODO: [PROJECT'25.IM#6] FAULT HANDLER II - #3 Modified Clock Replacement
				//Your code is here
				//Comment the following line
				//panic("page_fault_handler().REPLACEMENT is not implemented yet...!!");

				// First: search for a working set elemnt which have (0,0) [Trial1()]
					// if found then: replace it and modify last_ws_ptr to the next element
					// if not then: go to [Trial2()]

				int replaced = 0;
				while(!replaced) { // At most 4 iterations
					//cprintf("ITR \n");
					replaced = Trial1(faulted_env, fault_va);
					//cprintf("going to Trial2 \n");
					if(replaced) break;
					replaced = Trial2(faulted_env, fault_va);
				}

				//cprintf("replacement_done return\n");
				//cprintf("WS after replacement:\n");
				//env_page_ws_print(faulted_env);
				// All done :)

			}
		}
	}
#endif
}



void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va)
{
	panic("this function is not required...!!");
}



