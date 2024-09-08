/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     curr_blockright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above curr_blockright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE curr_blockRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL curr_blockRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_mem.c
 * @brief       Kernel Memory Management API C Code
 *
 * @version     V1.2021.01.lab2
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @note        skeleton code
 *
 *****************************************************************************/

/** 
 * @brief:  k_mem.c kernel API implementations, this is only a skeleton.
 * @author: Yiqing Huang
 */

#include "k_mem.h"
#include "Serial.h"
#include "k_inc.h"

//#define DEBUG_0

#ifdef DEBUG_0
#include "printf.h"
#endif

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */
// kernel stack size, referred by startup_a9.s
const U32 g_k_stack_size = K_STACK_SIZE;
// task proc space stack size in bytes, referred by system_a9.cs
const U32 g_p_stack_size = U_STACK_SIZE;

// task kernel stacks
U32 g_k_stacks[MAX_TASKS][K_STACK_SIZE >> 2] __attribute__((aligned(8)));

//process stack for tasks in SYS mode
U32 g_p_stacks[MAX_TASKS][U_STACK_SIZE >> 2] __attribute__((aligned(8)));

typedef struct con_block{
	 int size;
	 void* next;
	 task_t tid; 
	 int buf;
} ControlBlock;

//Free list head ptr
ControlBlock* HEAD = NULL;

int byte_aligning = 8;

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */	

U32* k_alloc_k_stack(task_t tid)
{
    return g_k_stacks[tid+1];
}

U32* k_alloc_p_stack(task_t tid)
{
	// so we want the tid to be ksp ?
	TCB *tcb = &(g_tcbs[tid]);
	U32 amount_alloc = tcb->u_stack_size;

	tcb->u_stack_start = k_mem_alloc((size_t)amount_alloc);
	return tcb->u_stack_start;
}

int k_mem_init(void) {
    unsigned int end_addr = (unsigned int) &Image$$ZI_DATA$$ZI$$Limit; // is this 4 byte aligned?
#ifdef DEBUG_0
    printf("k_mem_init: image ends at 0x%x\r\n", end_addr);
    printf("k_mem_init: RAM ends at 0x%x\r\n", RAM_END);
#endif /* DEBUG_0 */
    int remainder = end_addr % byte_aligning;
    if (remainder != 0){
    	end_addr += (byte_aligning - remainder);

    }
    if (RAM_END - end_addr - sizeof(ControlBlock) <= 0){
    	return RTX_ERR;
    }
    // Set the head to the first address of our memory block
    HEAD = (ControlBlock*)end_addr;
    // Set default values for our initialized block
    HEAD->size = RAM_END - end_addr - sizeof(ControlBlock);
    HEAD->next = 0;

	#ifdef DEBUG_0
    printf("k_mem_init: HEAD at 0x%x\r\n", HEAD);
	#endif

    return RTX_OK;
}

void* k_mem_alloc(size_t size) {
    // Memory must be initialized first
	if (HEAD == NULL || size == 0) {
    	return 0;
    }
	
	//k_mem_count_extfrag(10);

    // Do 4-byte aligning on the requested size
    int remainder = size % byte_aligning;
    if(remainder!=0)
    {
    	size += (byte_aligning-remainder);
    }

	#ifdef DEBUG_0
	printf("k_mem_alloc: requested memory size = %d\r\n", size);
	#endif

	ControlBlock *curr_block = HEAD;
	ControlBlock *prev_block = 0;
	while(curr_block){
		if (curr_block->size >= size){ // means we have space
			if (size + sizeof(ControlBlock) + byte_aligning <= curr_block->size){
				ControlBlock* new_block = (ControlBlock*)((char*)curr_block + sizeof(ControlBlock) + size); // Create a new free block from the remaining space
				new_block->size = curr_block->size - size - sizeof(ControlBlock); // Gives the size of the new ControlBlock

				if (curr_block == HEAD){ // setting newest control block
					HEAD = new_block;
				} else{ // set everything before curr block
					prev_block->next = new_block;
				}
				new_block->next = curr_block->next;
				curr_block->size = size;
				//printf("New Block address: 0x%x\r\n",new_block);
				//printf("HEAD address: 0x%x\r\n",HEAD);

			} else{ // not enough space to split on this one, need to skip through
				if (curr_block != HEAD){
					prev_block->next = curr_block->next;
				} else{
					HEAD = curr_block->next;
				}
			}
			#ifdef DEBUG_0
			printf("Return address: 0x%x\r\n",(char*) curr_block + sizeof(ControlBlock));
			#endif
			// set the TID of our control block to the current block for ownership
			curr_block->tid = gp_current_task->tid;
			return (char*) curr_block + sizeof(ControlBlock);
		}
		prev_block = curr_block;
		curr_block = curr_block->next;
	}
	// means we dont have space
    return NULL;
}

int k_mem_dealloc(void *ptr) {
	#ifdef DEBUG_0
    printf("k_mem_dealloc: freeing 0x%x\r\n", (U32) ptr);
	#endif

    if(ptr == NULL || (unsigned int)ptr>RAM_END)
    {
    	return RTX_ERR;
    }

    ControlBlock *header = (ControlBlock*)((char*)ptr - sizeof(ControlBlock)); // find start of control block for given memory block
	// only dealloc if the TID belongs to that task
	if (!override_ownership && gp_current_task->tid != header->tid){
		return RTX_ERR;
	}
    if (header < HEAD){ // this means that we need to deallocate and change head
    	// need to check for coelsasidtstst
    	if (((ControlBlock*)((char*)header + sizeof(ControlBlock) + header->size) == HEAD)) {
    		// Update header size
			header->size += HEAD->size + sizeof(ControlBlock);
			// Update header next
			header->next = HEAD->next;
			// change the previous
			HEAD = header;
    	}
    	else
    	{
    		header->next = HEAD;
    		HEAD = header;
    	}
    	return RTX_OK;
    }

    char did_merge = 0;

	ControlBlock *curr_block = HEAD; // first iterator
	while(curr_block) {
		if (curr_block == header)
		{
			return RTX_ERR;
		}
		// check if we can coalesce
		if ((((char*)header + sizeof(ControlBlock) + header->size) == (char*)curr_block->next)) {
			// Update header size
			header->size += ((ControlBlock*)curr_block->next)->size + sizeof(ControlBlock);
			// Update header next
			header->next = ((ControlBlock*)curr_block->next)->next;
			// change the previous
			curr_block->next = header;
			did_merge = 1;
		}
		if ((ControlBlock*)((char*)curr_block + sizeof(ControlBlock) + curr_block->size) == header) {
			// Update current block to take over the deallocated memory block
			curr_block->size += header->size + sizeof(ControlBlock);
			if (did_merge) {
				curr_block->next = header->next;
			}
			did_merge = 1;
		}

		if (did_merge) return RTX_OK;

		if (header < curr_block->next) {
			header->next = curr_block->next;
			curr_block->next = header;
			return RTX_OK;
		}
		curr_block = curr_block->next;
	}


    return RTX_ERR;
}

int k_mem_count_extfrag(size_t size) {
#ifdef DEBUG_0
    printf("k_mem_extfrag: size = %d\r\n", size);
#endif /* DEBUG_0 */

    ControlBlock* curr_block = HEAD;
    int i = 0;
    int count = 0;
    while(curr_block){
    	if(curr_block->size + sizeof(ControlBlock) < size)
    		count++;
		#ifdef DEBUG_0
    	printf("Block %d: addy = 0x%x\r\n",i, (void*)curr_block);
    	printf("Block %d: end = 0x%x\r\n",i, (char*)curr_block + sizeof(ControlBlock) + curr_block->size);
		printf("Block %d: size = %d\r\n",i, curr_block->size);
		printf("Block %d: next = 0x%x\r\n",i, curr_block->next);
    	printf("\r\n");
		#endif /* DEBUG_0 */
    	curr_block = curr_block->next;

    	i++;

    }
    return count;
}

/*
 *
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
