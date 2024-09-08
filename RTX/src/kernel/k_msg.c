/**
 * @file:   k_msg.c
 * @brief:  kernel message passing routines
 * @author: Yiqing Huang
 * @date:   2020/10/09
 */

#include "k_msg.h"
#include "k_task.h"
#include "k_inc.h"

int interrupt_flag = 0;

//#define DEBUG_0

#ifdef DEBUG_0
#include "printf.h"
#endif /* ! DEBUG_0 */

void deallocate_msg(MessageNode* msg_node){
    override_ownership = 1;
    k_mem_dealloc(msg_node->buf);
    k_mem_dealloc(msg_node);
    override_ownership = 0;
}
void memcpy(void *dest, const void *src, size_t n){
    char* dest_c = (char*) dest;
    char* src_c = (char*) src;
	#ifdef DEBUG_0
		printf("memcpy dest[1]:0x%x\r\n", &(dest_c[1]));
		printf("memcpy src[1]:0x%x\r\n", &(src_c[1]));
	#endif /* DEBUG_0 */
    for(int i = 0; i < n; i++){
        dest_c[i] = src_c[i];
    }
}

void enqueue_m(MailBox* list, MessageNode* node){
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
void free_mailbox(MailBox *mailbox){
	MessageNode *temp_msg = mailbox->head;
	MessageNode *delete_msg;
	while (temp_msg != NULL){
		delete_msg = temp_msg;
		temp_msg = temp_msg->next;
		deallocate_msg(delete_msg);
	}
	override_ownership = 1;
	k_mem_dealloc(mailbox);
	override_ownership = 0;

}

MessageNode* dequeue_m(MailBox* list){
	if(list->head == NULL){ // if list is empty/list doesn't exist, return
		return NULL;
	}
	// dequeue at the head
	MessageNode* dequeue_node = list->head;
	list->head = list->head->next;
	if (list->head == NULL){
		list->tail = NULL;
	}
	return dequeue_node;
}

int k_mbx_create(size_t size) {
#ifdef DEBUG_0
    printf("k_mbx_create: size = %d\r\n", size);
#endif /* DEBUG_0 */
    // checks if size argument is less than MIN_MBX_SIZE
    if(size < MIN_MBX_SIZE) {
    	return RTX_ERR;
    }

    // checks if calling task already has a mailbox
    if(gp_current_task->mailbox != NULL) {
		return RTX_ERR;
	}

    MailBox* mailbox = (MailBox*) k_mem_alloc(sizeof(MailBox));

    if(mailbox == NULL){
    	return RTX_ERR;
    }

    gp_current_task->mailbox = mailbox;
    mailbox->max_size = size;
    mailbox->curr_size = sizeof(MailBox);
    mailbox->head = NULL;
    mailbox->tail = NULL;


    return RTX_OK;
}

int k_send_msg(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */
    TCB *tcb = &(g_tcbs[receiver_tid]); // receiver tid?

	#ifdef DEBUG_0
		printf("tid recv_msg: %u\r\n", tcb->tid);
	#endif /* DEBUG_0 */

    MailBox* list = tcb->mailbox;

    if (tcb == NULL) return RTX_ERR;
    if (tcb->state == DORMANT) return RTX_ERR;
    if (buf == NULL) return RTX_ERR;
    if (list == NULL) return RTX_ERR;
    if (((RTX_MSG_HDR*)buf)->length < MIN_MSG_SIZE) return RTX_ERR; // how to do this if buf is void pointer // read the first U32 values
#ifdef DEBUG_0
    int size = (tcb->mailbox->curr_size + ((RTX_MSG_HDR*)buf)->length);
    printf("mailbox + message = %d \r\n", size);
    printf("receiver mailbox max size = %d \r\n", tcb->mailbox->max_size);
#endif
    if ((tcb->mailbox->curr_size + ((RTX_MSG_HDR*)buf)->length) > tcb->mailbox->max_size) return RTX_ERR;

    MessageNode *msg = (MessageNode*) k_mem_alloc(sizeof(MessageNode)); // shouldnt this be MessageNode? naming kinda bad
    msg->buf = k_mem_alloc(((RTX_MSG_HDR*)buf)->length);
    if (msg == NULL) return RTX_ERR;
    memcpy(msg->buf, buf, ((RTX_MSG_HDR*)buf)->length);
    if(interrupt_flag == 1)
    {
    	msg->sender_tid = TID_UART_IRQ;
    }
    else
    {
    	msg->sender_tid = gp_current_task->tid;
    }
    interrupt_flag = 0;

    msg->next = NULL;
    tcb->mailbox->curr_size += ((RTX_MSG_HDR*)buf)->length;

    enqueue_m(list, msg);

    if (tcb->state == BLK_MSG) {
    	insert_into_tasks(tcb);
        k_tsk_run_new();
    }


    return RTX_OK;
}

int k_recv_msg(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    
    MailBox* list = gp_current_task->mailbox; // get the mailbox of the list

    if (list == NULL) return RTX_ERR;
	if (list->head == NULL) {
		gp_current_task->state = BLK_MSG;
		#ifdef DEBUG_0
			printf("tid recv_msg: %u\r\n", gp_current_task->tid);
		#endif /* DEBUG_0 */
		k_tsk_run_new();
	}
    if (buf == NULL) return RTX_ERR;

    MessageNode* recv_msg_node = dequeue_m(list);
    int recv_msg_buf_length = ((RTX_MSG_HDR*)(recv_msg_node->buf))->length;

    if(sender_tid != NULL){ // it means that calling task needs sender ID, we don't care ig 
        *sender_tid = recv_msg_node->sender_tid;
    }
    // reduce size of current list by recv_buf

    list->curr_size -= recv_msg_buf_length; // length of deallocate node

    if (recv_msg_buf_length > len){
        deallocate_msg(recv_msg_node);
        return RTX_ERR;
    } 
    // cpy recv_buf -> buf
    memcpy(buf, recv_msg_node->buf, recv_msg_buf_length);
    
    deallocate_msg(recv_msg_node);
    return RTX_OK;
}

int k_recv_msg_nb(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg_nb: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    MailBox* list = gp_current_task->mailbox; // get the mailbox of the list

    if (buf == NULL) return RTX_ERR;
    if (list == NULL) return RTX_ERR;
    if (list->head == NULL) return RTX_ERR;
    
    MessageNode* recv_msg_node = dequeue_m(list);
    int recv_msg_buf_length = ((RTX_MSG_HDR*)(recv_msg_node->buf))->length;

    if(sender_tid != NULL){ // it means that calling task needs sender ID, we don't care ig 
        sender_tid = &(recv_msg_node->sender_tid);
    }
    // reduce size of current list by recv_buf

    list->curr_size -= (recv_msg_buf_length + sizeof(MessageNode)); // length of deallocate node

    if (recv_msg_buf_length < len){
        deallocate_msg(recv_msg_node);
        return RTX_ERR;
    } 
    // cpy recv_buf -> buf
    memcpy(buf, recv_msg_node->buf, recv_msg_buf_length);

    // if mailbox is empty
    deallocate_msg(recv_msg_node);
    return RTX_OK;
}

int k_mbx_ls(task_t *buf, int count) {
#ifdef DEBUG_0
    printf("k_mbx_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}
