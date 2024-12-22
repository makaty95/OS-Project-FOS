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

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

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

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}

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
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault  = 0;

int is_page_read_write(struct Env *faulted_env, uint32 fault_va) {
	int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
	if ((perms&PERM_WRITEABLE)) {
	   return 1;
	}
	else return 0;
}

uint32 is_marked_page(struct Env* faulted_env, uint32 va) {
    // check if the 10th bit is set to 1, if not return 0

//	cprintf(">checking validation for page at %p\n", (void*)va);
//	cprintf("->>>address in MB>>> %d mb\n", ((va)>>20));

    uint32* ptr_page_table;
    int status = get_page_table(faulted_env->env_page_directory, va, &ptr_page_table);
    if(status == TABLE_NOT_EXIST) {
		//ptr_page_table = (uint32*)create_page_table(faulted_env->env_page_directory, va);
    	return 0;
	}
    /*
	masking to get the 9th bit
	va       -> 00000000 00000000 00101100 11011001
	mask     -> 00000000 00000000 00000010 00000000
	mask&va  -> 00000000 00000000 00000000 00000000
	*/

    uint32 mask = (1 << 10);
    return ((ptr_page_table[PTX(va)]&mask)); // if 0 its not marked , if other it is set

}

struct Env* last_faulted_env = NULL;

void fault_handler(struct Trapframe *tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//	cprintf("\n************Faulted VA = %x************\n", fault_va);
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
			panic("Failed to handle fault! fault @ va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
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
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ( (faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		// we have a table fault =============================================================
		//		cprintf("[%s] user TABLE fault va %08x\n", curenv->prog_name, fault_va);
		//		print_trapframe(tf);

		faulted_env->tableFaultsCounter ++ ;

		table_fault_handler(faulted_env, fault_va);
	}
	else
	{
		if (userTrap)
		{
			/*============================================================================================*/
			//TODO: [PROJECT'24.MS2 - #08] [2] FAULT HANDLER I - Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			//your code is here
			//cprintf("FAULT HANDLER I - Check for invalid pointers called \n");
			if(fault_va >= KERNEL_BASE && fault_va <= KERNEL_HEAP_MAX) {
				//cprintf("[FAULT HANDLER I - Check for invalid pointers]pointing to kernel address address\n");
				env_exit();

			}
			else if((is_page_read_write(faulted_env, fault_va))) {
				//cprintf("[FAULT HANDLER I - Check for invalid pointers]it must be read only. \n");
				env_exit();
				//return;
			}else if(fault_va < USER_HEAP_MAX && fault_va >= USER_HEAP_START && !is_marked_page(faulted_env, fault_va)) {
				//cprintf("[FAULT HANDLER I - Check for invalid pointers]unmarked page\n");
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

		//		cprintf("[%08s] user PAGE fault va %08x\n", curenv->prog_name, fault_va);
		//		cprintf("\nPage working set BEFORE fault handler...\n");
		//		env_page_ws_print(curenv);

		if(isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
			//page_fault_handler(faulted_env, fault_va);
			page_fault_handler(faulted_env, fault_va);
		}
		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(curenv);
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
void page_fault_handler(struct Env * faulted_env, uint32 fault_va)
{
	#if USE_KHEAP
			struct WorkingSetElement *victimWSElement = NULL;
			uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
	#else
			int iWS =faulted_env->page_last_WS_index;
			uint32 wsSize = env_page_ws_get_size(faulted_env);
	#endif
			//cprintf("PLACEMENT=========================WS Size = %d\n", wsSize );
//		cprintf("PLACEMENT=========================WS MX Size = %d\n", faulted_env->page_WS_max_size);
		if(wsSize < (faulted_env->page_WS_max_size)) {

			//cprintf("working set before PLACEMENT:\n");
			//env_page_ws_print(faulted_env);
			//TODO: [PROJECT'24.MS2 - #09] [2] FAULT HANDLER I - Placement
			// Write your code here, remove the panic and write your code
			//panic("page_fault_handler().PLACEMENT is not implemented yet...!!");

			uint32 *ptr_page_table = NULL;
			int status =  get_page_table(faulted_env->env_page_directory, (uint32)fault_va, &ptr_page_table);
			if (status == TABLE_NOT_EXIST) {
				//cprintf("PLACEMENT -> page table doesn't exist\n");
				ptr_page_table = create_page_table(faulted_env->env_page_directory, (uint32)fault_va);
			}

			// acctual allocation and mapping of the page
			uint32 perms = (PERM_PRESENT | PERM_USER | PERM_WRITEABLE);
			struct FrameInfo *framee = NULL;

			int ss = allocate_frame(&framee);
			ss = map_frame(faulted_env->env_page_directory, framee, fault_va, perms);
			//faulted_env->is_marked[(fault_va - USER_HEAP_START)/PAGE_SIZE] = 1;

			int ret = pf_read_env_page(faulted_env, (void *)fault_va);
			if (ret == E_PAGE_NOT_EXIST_IN_PF) {

				if (!(fault_va >= (USTACKBOTTOM) && fault_va < USTACKTOP)) {
					if (!(fault_va >= (USER_HEAP_START) && fault_va < USER_HEAP_MAX)){
						env_exit();
					}

				}
			} else {
				//cprintf("Placement> the page found on disk successcully! :)\n");
			}


			if(LIST_SIZE(&(faulted_env->page_WS_list)) < faulted_env->page_WS_max_size) {

				struct WorkingSetElement *ws_element = env_page_ws_list_create_element(faulted_env, fault_va);
				 LIST_INSERT_TAIL(&(faulted_env->page_WS_list), ws_element);
				 if(LIST_SIZE(&(faulted_env->page_WS_list)) < faulted_env->page_WS_max_size) { // if still not full
					 // list is not full
					 faulted_env->page_last_WS_element = NULL;
				 } else {
					 // list is full
					 faulted_env->page_last_WS_element = (LIST_FIRST(&(faulted_env->page_WS_list)));
				 }

				 //if(ws_element->virtual_address >= USER_HEAP_START)

				 //cprintf("working set -> ws element added\n");
			} else {
				//cprintf("full working set -> can't add the ws element\n");
			}



//			cprintf("working set after PLACEMENT:\n");
//			env_page_ws_print(faulted_env);

	} else {
		//cprintf("REPLACEMENT=========================WS Size = %d\n", wsSize );
		//refer to the project presentation and documentation for details
		//TODO: [PROJECT'24.MS3] [2] FAULT HANDLER II - Replacement
		// Write your code here, remove the panic and write your code
		int found = 0;
		uint32 *page_dir = faulted_env->env_page_directory;
		struct WorkingSetElement *ws_iter = faulted_env->page_last_WS_element;
//		# NORMAL Nth CHANCE;
		int replStrat = page_WS_max_sweeps >= 0? 1:-1;
		int max_sweeps = replStrat == -1? page_WS_max_sweeps * -1: page_WS_max_sweeps;
		if (isPageReplacmentAlgorithmNchanceCLOCK()){
			while (found != 1){
				uint32 *page_table = NULL;
				uint32 iter_va = ws_iter->virtual_address;

				struct FrameInfo *frame_iter = get_frame_info(page_dir, iter_va, &page_table);
				uint32 perms = pt_get_page_permissions(page_dir, iter_va);
				uint32 isUsed = perms & PERM_USED;
				uint32 isModif = perms & PERM_MODIFIED;

				if (isUsed == PERM_USED){
					pt_set_page_permissions(page_dir, iter_va, 0, PERM_USED);
					ws_iter->sweeps_counter = 0;
				}else{
					ws_iter->sweeps_counter++;
					if (ws_iter->sweeps_counter >= max_sweeps){
						if (isModif == PERM_MODIFIED){
							if (replStrat == -1 && ws_iter->sweeps_counter < (max_sweeps +1)){
								ws_iter = LIST_NEXT(ws_iter) != NULL? LIST_NEXT(ws_iter) : LIST_FIRST(&faulted_env->page_WS_list);
								continue;
							}else{
								pf_update_env_page(faulted_env, iter_va, frame_iter);
							}
						}
						perms = perms | PERM_USED;
						map_frame(page_dir, frame_iter, fault_va, perms);
						unmap_frame(page_dir, iter_va);
						int ret = pf_read_env_page(faulted_env, (void *)fault_va);
						ws_iter->virtual_address = fault_va;
						if (ret == E_PAGE_NOT_EXIST_IN_PF) {

							if (!(fault_va >= (USTACKBOTTOM) && fault_va < USTACKTOP)) {
								if (!(fault_va >= (USER_HEAP_START) && fault_va < USER_HEAP_MAX)){
									env_exit();
								}

							}
						}
						faulted_env->page_last_WS_element = LIST_NEXT(ws_iter) != NULL? LIST_NEXT(ws_iter):LIST_FIRST(&faulted_env->page_WS_list);
						found = 1;
					}
				}
				ws_iter = LIST_NEXT(ws_iter) != NULL? LIST_NEXT(ws_iter) : LIST_FIRST(&faulted_env->page_WS_list);

			}
		}
	}

}

void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va) {
	//[PROJECT] PAGE FAULT HANDLER WITH BUFFERING
	// your code is here, remove the panic and write your code
	panic("__page_fault_handler_with_buffering() is not implemented yet...!!");

}

