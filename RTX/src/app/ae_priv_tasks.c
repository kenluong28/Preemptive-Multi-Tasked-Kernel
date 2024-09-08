/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                 Copyright 2020-2021 ECE 350 Teaching Team
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

#include "ae.h"
#include "ae_priv_tasks.h"
#include "ae_usr_tasks.h"
#include "Serial.h"
#include "printf.h"
#include "k_mem.h"

task_t ktid1;
task_t ktid2;
task_t ktid3;


#if TEST == 0
/*****************************************************************************
 * @brief       a task that prints AAAAA, BBBBB, CCCCC,...., ZZZZZ on each line.
 *              It yields the cpu every 6 lines are printed.
 *****************************************************************************/
void ktask1(void)
{
    int i = 0;
    int j = 0;
    long int x = 0;

    while (1) {
        char out_char = 'A' + i % 26;
        for (j = 0; j < 5; j++ ) {
            SER_PutChar(0,out_char);
        }

        SER_PutStr(0,"\n\r");

        for ( x = 0; x < DELAY; x++);

        if ( (++i) % 6 == 0 ) {
            SER_PutStr(0,"priv_task1 before yielding cpu.\n\r");
            k_tsk_yield();
            SER_PutStr(0,"priv_task1 after yielding cpu.\n\r");
        }
    }

}
void k_tsk_create_tester(void){
	SER_PutStr(0,"entered enter k_tsk_create test.\n\r");
	task_t tid;
	if (k_tsk_create(&tid, &utask1, 5, 65535) != RTX_ERR)
		SER_PutStr(0,"max is fine.\n\r");
	else
		SER_PutStr(0,"size good.\n\r");
	if (k_tsk_create(&tid, &utask1, 256, 0x200) != RTX_ERR)
		SER_PutStr(0,"failed.\n\r");
	else
		SER_PutStr(0,"prio pass.\n\r");
	if (k_tsk_create(&tid, NULL, 5, 0x200) != RTX_ERR)
		SER_PutStr(0,"failed.\n\r");
	else
		SER_PutStr(0,"no task provided pass.\n\r");

	return;
}
void k_tsk_exit_tester(void){
	SER_PutStr(0,"entered enter first test.\n\r");

	k_tsk_exit(); // since this tsk was exited, the next in queue is priority three

	SER_PutStr(0,"pass.\n\r");

	return;
}
void k_tsk_set_prio_tester(void){
	SER_PutStr(0,"entered k_tsk_set_prio_tester test.\n\r");
	k_tsk_set_prio(1, 202);
	SER_PutStr(0,"Failed, test_invalid_prio still running \n\r");
	return;
}
void k_tsk_get_info_tester(void){
	SER_PutStr(0,"entered k_tsk_get_info_tester test.\n\r");
	RTX_TASK_INFO *buf = (RTX_TASK_INFO *)k_mem_alloc(sizeof(RTX_TASK_INFO));
	TCB* tcb = &g_tcbs[1]; // task id at 1
	k_tsk_get_info(1, buf); // fills buf with info
	printf("buf prio: %d\n\r", buf->prio);
	printf("tcb prio: %d.\n\r", tcb->prio);

	if (buf->ptask == tcb->task_entry){
		SER_PutStr(0,"pass ptask.\n\r");
	} else{
		SER_PutStr(0,"fail ptask.\n\r");
	}
	if (buf->k_stack_hi == (U32)tcb->ksp){
		SER_PutStr(0,"pass k_stack_hi.\n\r");
	} else{
		SER_PutStr(0,"fail k_stack_hi.\n\r");
	}
	if (buf->u_stack_hi == (U32)(tcb->u_stack_start + tcb->u_stack_size)){
		SER_PutStr(0,"pass u_stack_hi.\n\r");
	} else{
		SER_PutStr(0,"fail u_stack_hi.\n\r");
	}
	if (buf->k_stack_size == K_STACK_SIZE){
		SER_PutStr(0,"pass K_STACK_SIZE.\n\r");
	} else{
		SER_PutStr(0,"fail K_STACK_SIZE.\n\r");
	}
	printf("buf ustack: %d\n\r", buf->u_stack_size);
	printf("tcb ustack: %d.\n\r", tcb->u_stack_size);
	if (buf->u_stack_size == tcb->u_stack_size){
		SER_PutStr(0,"pass u_stack_size.\n\r");
	} else{
		SER_PutStr(0,"fail u_stack_size.\n\r");
	}
	if (buf->tid == tcb->tid){
		SER_PutStr(0,"pass tid.\n\r");
	} else{
		SER_PutStr(0,"fail tid.\n\r");
	}
	if (buf->prio == tcb->prio){
		SER_PutStr(0,"pass prio.\n\r");
	} else{
		SER_PutStr(0,"fail prio.\n\r");
	}
	if (buf->state == tcb->state){
		SER_PutStr(0,"pass state.\n\r");
	} else{
		SER_PutStr(0,"fail state.\n\r");
	}
	if (buf->priv == tcb->priv){
		SER_PutStr(0,"pass priv.\n\r");
	} else{
		SER_PutStr(0,"fail priv.\n\r");
	}
	tcb = &g_tcbs[2]; // task id at 1
	k_tsk_get_info(2, buf); // fills buf with info
	printf("buf prio: %d\n\r", buf->prio);
	printf("tcb prio: %d.\n\r", tcb->prio);

	if (buf->ptask == tcb->task_entry){
		SER_PutStr(0,"pass ptask.\n\r");
	} else{
		SER_PutStr(0,"fail ptask.\n\r");
	}
	if (buf->k_stack_hi == (U32)tcb->ksp){
		SER_PutStr(0,"pass k_stack_hi.\n\r");
	} else{
		SER_PutStr(0,"fail k_stack_hi.\n\r");
	}
	if (buf->u_stack_hi == (U32)(tcb->u_stack_start + tcb->u_stack_size)){
		SER_PutStr(0,"pass u_stack_hi.\n\r");
	} else{
		SER_PutStr(0,"fail u_stack_hi.\n\r");
	}
	if (buf->k_stack_size == K_STACK_SIZE){
		SER_PutStr(0,"pass K_STACK_SIZE.\n\r");
	} else{
		SER_PutStr(0,"fail K_STACK_SIZE.\n\r");
	}
	printf("buf ustack: %d\n\r", buf->u_stack_size);
	printf("tcb ustack: %d.\n\r", tcb->u_stack_size);
	if (buf->u_stack_size == tcb->u_stack_size){
		SER_PutStr(0,"pass u_stack_size.\n\r");
	} else{
		SER_PutStr(0,"fail u_stack_size.\n\r");
	}
	if (buf->tid == tcb->tid){
		SER_PutStr(0,"pass tid.\n\r");
	} else{
		SER_PutStr(0,"fail tid.\n\r");
	}
	if (buf->prio == tcb->prio){
		SER_PutStr(0,"pass prio.\n\r");
	} else{
		SER_PutStr(0,"fail prio.\n\r");
	}
	if (buf->state == tcb->state){
		SER_PutStr(0,"pass state.\n\r");
	} else{
		SER_PutStr(0,"fail state.\n\r");
	}
	if (buf->priv == tcb->priv){
		SER_PutStr(0,"pass priv.\n\r");
	} else{
		SER_PutStr(0,"fail priv.\n\r");
	}
	k_mem_dealloc(&buf);
//	k_tsk_exit();
	// change some params and see if its reflected
//	buf->priv = 1;
//	buf->state = DORMANT;
//	buf->prio = 255;
//	if (tcb->prio == 255){
//		SER_PutStr(0,"pass change.\n\r");
//	}

	return;
}

void k_tsk_get_tid_tester(void){
	task_t tid = k_tsk_get_tid();
	if(gp_current_task->tid == tid){
		printf("pass\r\n");
	} else{
		printf("fail\r\n");
	}

}
// ownership
int* dealloc_pointer = 0;
void task_dealloc_tester1(void){
	// going to try to deallocate stuff on task 2 even tho on task 1
	printf("entered task_dealloc_tester1\r\n");
	dealloc_pointer = (int*) k_mem_alloc(sizeof(int)); // alloc a pointer of int
	k_tsk_yield();
}
void task_dealloc_tester2(void){
	printf("entered task_dealloc_tester2\r\n");
	if (k_mem_dealloc(dealloc_pointer) == RTX_ERR){
		printf("pass\r\n");
	} else{
		printf("fail\r\n");
	}

}
// Fill the entire priority queue with tasks till max tasks
// Increment each task's priority by 5
void max_tasks_prio_change_test(void)
{
    task_t tid[MAX_TASKS];

//    RTX_TASK_INFO task_info[MAX_TASKS];
    SER_PutStr (0,"utask1: entering \n\r");

    /* do something */
    for(int i = 2; i < MAX_TASKS; i++){
        k_tsk_create(&tid[i], &utask1, i, 0x200);
    }
    k_tsk_exit();
}

void test_tid(void)
{
	task_t tid[MAX_TASKS];
	for(int i = 2; i < MAX_TASKS-1; i++){
		k_tsk_create(&tid[i], &utask1, 150, 0x200);
	}
	k_tsk_create(&tid[MAX_TASKS-1], &utask2, 1, 0x200);
	k_tsk_create(&tid[MAX_TASKS-1], &utask1, 150, 0x200);

}


/*****************************************************************************
 * @brief:      a task that prints 00000, 11111, 22222,....,99999 on each line.
 *              It yields the cpu every 6 lines are printed
 *              before printing these lines indefinitely, it creates a
 *              new task and and obtains the task information. It then
 *              changes the newly created task's priority.
 *****************************************************************************/

void ktask2(void)
{
    long int x = 0;
    int i = 0;
    int j = 0;
    task_t tid;

    k_tsk_create(&tid, &utask1, LOW, 0x200);  /*create a user task */
    k_tsk_set_prio(tid, LOWEST);


    for (i = 1;;i++) {
        char out_char = '0' + i%10;
        for (j = 0; j < 5; j++ ) {
            SER_PutChar(0,out_char);
        }
        SER_PutStr(0,"\n\r");

        for ( x = 0; x < DELAY; x++); // some artifical delay
        if ( i%6 == 0 ) {
            SER_PutStr(0,"priv_task2 before yielding CPU.\n\r");
            k_tsk_yield();
            SER_PutStr(0,"priv_task2 after yielding CPU.\n\r");
        }
    }

}
#endif
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
