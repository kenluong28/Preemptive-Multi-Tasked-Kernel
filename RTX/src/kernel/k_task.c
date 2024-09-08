/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
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
 * @file        k_task.c
 * @brief       task management C file
 *              l2
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only two simple privileged task and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/

//#include "VE_A9_MP.h"
#include "Serial.h"
#include "k_task.h"
#include "k_rtx.h"
#include "k_inc.h"

//#define DEBUG_0

#ifdef DEBUG_0
#include "printf.h"
#endif /* DEBUG_0 */

/*
 *===========================================================================
 *                             GLOBAL VARS
 *===========================================================================
 */
TCB             *gp_current_task = NULL;	// the current RUNNING task
int				override_ownership = 0;
TCB             g_tcbs[MAX_TASKS];			// an array of TCBs
RTX_TASK_INFO   g_null_task_info;			// The null task info
U32             g_num_active_tasks = 0;		// number of non-dormant tasks

U8				available_tid_queue[MAX_TASKS-1]; // Ready queue of available TIDS
int tid_queue_head = 0;
int tid_queue_tail = 0;
int available_tid_queue_size = 0;

void add_tid(U8 new_tid)
{
	if(available_tid_queue_size == MAX_TASKS-1) return;

	available_tid_queue[tid_queue_tail] = new_tid;
	tid_queue_tail++;
	available_tid_queue_size++;
	if(tid_queue_tail>=MAX_TASKS-1)
		tid_queue_tail = 0;
}

U8 get_next_tid()
{
	if (available_tid_queue_size == 0) return 0;
	U8 next_tid = available_tid_queue[tid_queue_head];
	tid_queue_head++;
	available_tid_queue_size--;
	if(tid_queue_head >= MAX_TASKS-1)
		tid_queue_head = 0;
	return next_tid;
}

/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:

                       RAM_END+---------------------------+ High Address
                              |                           |
                              |                           |
                              |    Free memory space      |
                              |   (user space stacks      |
                              |         + heap            |
                              |                           |
                              |                           |
                              |                           |
 &Image$$ZI_DATA$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      U_STACK_SIZE         |     |
             g_p_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |  other kernel proc stacks |     |
                              |---------------------------|     |
                              |      U_STACK_SIZE         |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      K_STACK_SIZE         |     |                
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      K_STACK_SIZE         |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |                           |     |
                              |                           |     V
                     RAM_START+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            ARRAY
 *===========================================================================
 */

typedef struct LinkedList{
	TCB* head;
	TCB* tail;
} LinkedList;

LinkedList tasks[PRIO_NULL+1]; // store all the tasks in null

int highest_priority = PRIO_NULL; //

int yield_flag = 0;

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
*/

void update_highest_priority(int new_priority){
	//printf("update_highest_priority: new_priority = %d\r\n", new_priority);
	if(highest_priority > new_priority){
		highest_priority = new_priority;
	}
	//printf("highest_priority = %d \r\n", highest_priority);
}
void enqueue(LinkedList *list, TCB* node){
	// We have to enqueue the list at the tail
	if(list->head == NULL){ // if we don't have a head yet
		list->head = node;
		list->tail = node;
	} else { // head exists
		list->tail->next = node; // make the next of tail equal to the node
		list->tail = node; // make tail equal to that node
	}

	return;
}

TCB* dequeue(LinkedList *list){
	if(list->head == NULL){ // if list is empty/list doesn't exist, return
		return NULL;
	}
	// dequeue at the head
	TCB* dequeue_node = list->head;
	list->head = list->head->next;
	if (list->head == NULL){
		list->tail = NULL;
	}
	return dequeue_node;
}

void insert_into_tasks(TCB *tcb){
	int priority = tcb->prio;
	update_highest_priority(priority);
	tcb->state = READY;
	tcb->next = NULL;
	enqueue(&(tasks[priority]), tcb);
}

TCB* remove_node(LinkedList *list, task_t tid){
	if(list->head == NULL) { // if list is empty/list doesn't exist, return can't pop
		return NULL;
	}
	TCB* itr = list->head;
	TCB* next_itr = list->head->next;
	// unique case of at the head
	if(itr->tid == tid){
		return dequeue(list);
	}
	while(next_itr){ // loop until reach end of the list
		if(next_itr->tid == tid){ // if the next itr
			// this node is the one to remove
			if(next_itr == list->tail){
				list->tail = itr;
			}
			itr->next = next_itr->next; // re-attach
			return next_itr;
		}
		itr = next_itr;
		next_itr = next_itr->next;
	}
	return NULL;
}

void initialize_priority_array(){
	for(int i = 0; i <= PRIO_NULL; i++){
		tasks[i].head = NULL;
		tasks[i].tail = NULL;
	}
}


TCB *retrieve_from_tasks(){
	// might need to keep null task always in array
	TCB* tcb = dequeue(&tasks[highest_priority]);
	if (tasks[highest_priority].head == NULL){
		int iter = highest_priority + 1;
		while(iter <= 255){
			if (tasks[iter].head != NULL){
				highest_priority = iter;
				break;
			}
			iter++;
		}
	}
	return tcb;
}

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 *
 *****************************************************************************/

TCB *scheduler(void)
{
#ifdef DEBUG_0
	printf("Scheduler running \n\r");
	printf("curr_prio: %u \n\r",gp_current_task->prio);
	printf("highest prio: %u \n\r",highest_priority);
#endif
	if (gp_current_task->state == DORMANT || gp_current_task->state == BLK_MSG){
		// task is dormant, so we can schedule the next task
		TCB *new_tcb = retrieve_from_tasks();
		#ifdef DEBUG_0
		printf("gp_current_task id: %d \n\r", gp_current_task->tid);
		#endif
		return new_tcb;
	} else if ((yield_flag &&  gp_current_task->prio == highest_priority) || gp_current_task->prio > highest_priority){
		yield_flag = 0;
		TCB *new_tcb = retrieve_from_tasks();
		#ifdef DEBUG_0
		printf("new_tcb: %d \n\r", new_tcb->tid);
		#endif
		insert_into_tasks(gp_current_task);
		return new_tcb;
	}
	return gp_current_task;
}



/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 *
 * @see         k_tsk_create_new
 *****************************************************************************/

int k_tsk_init(RTX_TASK_INFO *task_info, int num_tasks)
{
	#ifdef DEBUG_0
	printf("inside k_tsk_init\r\n");
	#endif
	for(int i = 1; i<MAX_TASKS;i++)
	{
		if (i == TID_KCD) continue;
		add_tid(i);
	}

	initialize_priority_array();

    extern U32 SVC_RESTORE;

    RTX_TASK_INFO *p_taskinfo = &g_null_task_info;
    g_num_active_tasks = 0;

    if (num_tasks > MAX_TASKS) {
    	return RTX_ERR;
    }

    // create the first task
    TCB *p_tcb = &g_tcbs[0];
    p_tcb->prio     = PRIO_NULL;
    p_tcb->priv     = 1;
    p_tcb->tid      = TID_NULL;
    p_tcb->state    = RUNNING;
    g_num_active_tasks++;
    gp_current_task = p_tcb;
//    insert_into_tasks(p_tcb);

    // create the rest of the tasks
    p_taskinfo = task_info;
    for ( int i = 0; i < num_tasks; i++ ) {
    	U8 new_tid;
		if (p_taskinfo->ptask == &kcd_task) {
			new_tid = TID_KCD;
		} else {
			new_tid = get_next_tid();
		}
        TCB *p_tcb = &(g_tcbs[new_tid]);
        if (k_tsk_create_new(p_taskinfo, p_tcb, new_tid) == RTX_OK) {
        	g_num_active_tasks++;
        	insert_into_tasks(p_tcb);
			#ifdef DEBUG_0
        	printf("tid, %d, prio %d\r\n", p_tcb->tid, p_tcb->prio);
			#endif
        }
        p_taskinfo++;
    }

    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task information structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR12)
 *              then we stack up the kernel initial context (kLR, kR0-kR12)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              30 registers in total
 *
 *****************************************************************************/
int k_tsk_create_new(RTX_TASK_INFO *p_taskinfo, TCB *p_tcb, task_t tid)
{
    extern U32 SVC_RESTORE;

    U32 *sp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }

    p_tcb ->tid = tid;
    p_tcb->state = READY;

    /*---------------------------------------------------------------
     *  Step1: allocate kernel stack for the task
     *         stacks grows down, stack base is at the high address
     * -------------------------------------------------------------*/

    ///////sp = g_k_stacks[tid] + (K_STACK_SIZE >> 2) ;
    sp = k_alloc_k_stack(tid);

    // 8B stack alignment adjustment
    if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
        sp--;               // adjust it to 8B aligned
    }

    /*-------------------------------------------------------------------
     *  Step2: create task's user/sys mode initial context on the kernel stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the user/sys mode context saved on the kernel stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         16 registers listed in push order
     *         <xPSR, PC, uSP, uR12, uR11, ...., uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    // uSP: initial user stack
    if ( p_taskinfo->priv == 0 ) { // unprivileged task
        // xPSR: Initial Processor State
        *(--sp) = INIT_CPSR_USER;
        // PC contains the entry point of the user/privileged task
        *(--sp) = (U32) (p_taskinfo->ptask);

        //********************************************************************//
        //*** allocate user stack from the user space, not implemented yet ***//
        //********************************************************************//
        p_tcb->u_stack_size = p_taskinfo->u_stack_size;
        *(--sp) = (U32) k_alloc_p_stack(tid);
        // uR12, uR11, ..., uR0
        for ( int j = 0; j < 13; j++ ) {
            *(--sp) = 0x0;
        }
    }


    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         14 registers listed in push order
     *         <kLR, kR0-kR12>
     * -------------------------------------------------------------*/
    if ( p_taskinfo->priv == 0 ) {
        // user thread LR: return to the SVC handler
        *(--sp) = (U32) (&SVC_RESTORE);
    } else {
        // kernel thread LR: return to the entry point of the task
        *(--sp) = (U32) (p_taskinfo->ptask);
    }

    // kernel stack R0 - R12, 13 registers
    for ( int j = 0; j < 13; j++) {
        *(--sp) = 0x0;
    }

    // kernel stack CPSR
    *(--sp) = (U32) INIT_CPSR_SVC;
    p_tcb->ksp = sp;
    p_tcb->prio = p_taskinfo->prio;
	p_tcb->priv = p_taskinfo->priv;
	p_tcb->mailbox = NULL;

    return RTX_OK;
}


/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param:      p_tcb_old, the old tcb that was in RUNNING
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note:       caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PUSH    {R0-R12, LR}
        MRS 	R1, CPSR
        PUSH 	{R1}
        STR     SP, [R0, #TCB_KSP_OFFSET]   ; save SP to p_old_tcb->ksp
        LDR     R1, =__cpp(&gp_current_task);
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_KSP_OFFSET]   ; restore ksp of the gp_current_task
        POP		{R0}
        MSR		CPSR_cxsf, R0
        POP     {R0-R12, PC}
}


/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;

    if (gp_current_task == NULL) {
    	yield_flag = 0;
    	return RTX_ERR;
    }

    p_tcb_old = gp_current_task;

    gp_current_task = scheduler();
    
    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        return RTX_ERR;
    }

    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
        gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
        if (p_tcb_old->state != DORMANT && p_tcb_old->state != BLK_MSG)p_tcb_old->state = READY;           // change state of the to-be-switched-out tcb
        k_tsk_switch(p_tcb_old);
        	// switch stacks

    }

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
	yield_flag = 1;
    return k_tsk_run_new();
}


/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */

int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U16 stack_size)
{
#ifdef DEBUG_0
    printf("k_tsk_create: entering...\n\r");
    printf("task = 0x%x, task_entry = 0x%x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */

    if (prio == PRIO_NULL || prio == PRIO_RT)
    	return RTX_ERR;
    if (task == NULL)
    	return RTX_ERR;
    if (task_entry == NULL)
    	return RTX_ERR;
    if (g_num_active_tasks >= MAX_TASKS)
    	return RTX_ERR;
    if (stack_size < U_STACK_SIZE)
    	return RTX_ERR;

    U8 unique_tid;
    if (task_entry == &kcd_task) {
    	unique_tid = TID_KCD;
    } else {
    	unique_tid = get_next_tid();
    }

	(*task) = unique_tid;
    TCB *new_tcb = &(g_tcbs[unique_tid]);


	RTX_TASK_INFO p_taskinfo;

	p_taskinfo.u_stack_size = stack_size;
	p_taskinfo.ptask = task_entry;
	p_taskinfo.tid = unique_tid;
	p_taskinfo.prio = prio;
	p_taskinfo.priv = 0; // unprivileged for now

	k_tsk_create_new(&p_taskinfo, new_tcb, unique_tid);

	new_tcb->u_stack_size = stack_size;
	new_tcb->task_entry = task_entry;

    insert_into_tasks(new_tcb);

    k_tsk_run_new();

    return RTX_OK;

}

void k_tsk_exit(void) 
{
#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */

    if (gp_current_task->mailbox != NULL) {
    	free_mailbox(gp_current_task->mailbox);
    }

    if (gp_current_task->priv == 0){ // user task, so we have to deallocate the memory
    	override_ownership = 1;
    	k_mem_dealloc(gp_current_task->u_stack_start);
    	override_ownership = 0;
    }
    // dealloc mailbox messages


    gp_current_task->state = DORMANT;

    if (gp_current_task->tid != TID_KCD) {
        add_tid(gp_current_task->tid);
    }

    g_num_active_tasks--;

    k_tsk_run_new();

    return;
}


int k_tsk_set_prio(task_t task_id, U8 prio) 
{
#ifdef DEBUG_0
    printf("k_tsk_set_prio: entering...\n\r");
    printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */

    int is_call_task_kernel = gp_current_task->priv;
    int is_changed_task_kernel = g_tcbs[task_id].priv;

    // check privilege of current task and task
    if (!is_call_task_kernel &&  is_changed_task_kernel) {
    	// calling task is kernel
    	return RTX_ERR;
    }

    //check if working on null task
    if (task_id == 0) {
    	return RTX_ERR;
    }

    // check prio param is valid
    if (!(prio > 0 && prio < 255)) {
    	return RTX_ERR;
    }

    //Special Case for blocked, just change prio, dont insert into prio queue:
    TCB* changed_tcb = &(g_tcbs[task_id]);
    if(changed_tcb->state == BLK_MSG)
    {
    	changed_tcb->prio = prio;
    	return RTX_OK;
    }

    if (gp_current_task->tid == task_id) {
		gp_current_task->prio = prio;
    	if (prio >= highest_priority) {
//    	    insert_into_tasks(gp_current_task);
    		k_tsk_yield();
    	    return RTX_OK;
    	} else {
    		update_highest_priority(prio);
    		return RTX_OK;
    	}
    }

    LinkedList* list_to_search = &(tasks[g_tcbs[task_id].prio]);
	#ifdef DEBUG_0
    printf("Current task prio: %u",g_tcbs[task_id].prio);
	#endif
    TCB* task = remove_node(list_to_search, task_id);


    task->prio = prio;
    insert_into_tasks(task);

    k_tsk_run_new();

    return RTX_OK;    
}

int k_tsk_get_info(task_t task_id, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    printf("k_tsk_get_info: entering...\n\r");
    printf("task_id = %d, buffer = 0x%x.\n\r", task_id, buffer);
#endif /* DEBUG_0 */    
    if (buffer == NULL) {
        return RTX_ERR;
    }
    /* The code fills the buffer with some fake task information. 
       You should fill the buffer with correct information    */
    TCB* tcb = &g_tcbs[task_id];

	buffer->ptask = tcb->task_entry;
    buffer->k_stack_hi = (U32)tcb->ksp;
    buffer->u_stack_hi = (U32)(tcb->u_stack_start + tcb->u_stack_size);
    buffer->k_stack_size = K_STACK_SIZE;
    buffer->u_stack_size = tcb->u_stack_size; // change tcb size from u32 -> u16? whys so big
    buffer->tid = tcb->tid;
    buffer->prio = tcb->prio;
    buffer->state = tcb->state;
    buffer->priv = tcb->priv;

    return RTX_OK;     
}

task_t k_tsk_get_tid(void)
{
#ifdef DEBUG_0
    printf("k_tsk_get_tid: entering...\n\r");
#endif /* DEBUG_0 */ 

    return gp_current_task->tid;
}

int k_tsk_ls(task_t *buf, int count){
#ifdef DEBUG_0
    printf("k_tsk_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB4
 *===========================================================================
 */

int k_tsk_create_rt(task_t *tid, TASK_RT *task)
{
    return 0;
}

void k_tsk_done_rt(void) {
#ifdef DEBUG_0
    printf("k_tsk_done: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

void k_tsk_suspend(TIMEVAL *tv)
{
#ifdef DEBUG_0
    printf("k_tsk_suspend: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
