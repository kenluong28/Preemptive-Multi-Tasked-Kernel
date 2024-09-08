/* The KCD Task Template File */
#include "printf.h"
#include "k_inc.h"
#include "k_mem.h"
#include "rtx.h"

char* ERRORSTRING= "Command cannot be processed";
char* INVALIDSTRING = "Invalid Command";
task_t cmd_ids[255];
char input_buf[64];
int tail = 0;

void print_string(char* string)
{
	int i = 0;
	while(string[i]!='\0')
	{
		UART0_PutChar(string[i]);
		i++;
	}
}
int is_alphanumeric(char the_char){
	if ((the_char >= '0' && the_char <= '9') ||
			(the_char >= 'a' && the_char <= 'z') ||
			(the_char >= 'A' && the_char <= 'Z')){
		return 1;
	} else{
		return 0;
	}
}

void kcd_task(void)
{
	#ifdef DEBUG_0
	print_string("\033[2J");
	#endif

	for(int i = 0;i<255;i++)
	{
		cmd_ids[i] = 0;
	}

    if (mbx_create(KCD_MBX_SIZE) == RTX_ERR){
		#ifdef DEBUG_0
        printf(" lil cuck boy mike feewing horny uwu <|:)->-3==D~~~");
	#endif
    }  else{
        size_t buf_len = sizeof(RTX_MSG_HDR) + 64;
        void *buf = k_mem_alloc(buf_len);
        task_t *tid_reg = (task_t*)k_mem_alloc(sizeof(task_t));
        while(1){
            recv_msg(tid_reg, buf, buf_len);

			// do some shit with the recv_msg
			// look at buf for msg type
			if (((RTX_MSG_HDR*)buf)->type == KCD_REG)
			{
				char recieved_char = *((char*)((RTX_MSG_HDR*)buf + 1));

				#ifdef DEBUG_0
				printf("size of length = %d \n\r", ((RTX_MSG_HDR*)buf)->length);
				printf("char: %c \n\r", recieved_char);
				#endif

				if (((RTX_MSG_HDR*)buf)->length != sizeof(RTX_MSG_HDR) + 1)
				{
					continue;
				} else {
					cmd_ids[recieved_char] = *tid_reg;
					#ifdef DEBUG_0
					printf("registered command %c to tid %d\r\n", recieved_char, *tid_reg);
					#endif
				}

			} else if (((RTX_MSG_HDR*)buf)->type == KEY_IN){

				char recieved_char = *((char*)((RTX_MSG_HDR*)buf + 1));

				#ifdef DEBUG_0
				printf("char: %d \n\r", recieved_char);
				#endif

				if((int)recieved_char == 13)
				{
					if(tail>64 || input_buf[0]!='%')
					{
						print_string(INVALIDSTRING);
						tail = 0;
						continue;
					}

					TCB *tcb = &(g_tcbs[cmd_ids[input_buf[1]]]);

					#ifdef DEBUG_0
					printf("command %c is registered to tid %d\r\n", input_buf[1], cmd_ids[input_buf[1]]);
					if (tcb->state == DORMANT) printf("we fucked the dog with half an inch \r\n");
					#endif

					if(is_alphanumeric(input_buf[1]) == 0 || cmd_ids[input_buf[1]] == 0 || tcb->state == DORMANT)
					{
						print_string(ERRORSTRING);
						tail = 0;
						continue;
					}

					int limit = tail>=64? 64 : tail;
					void *send_msg_buf = k_mem_alloc(sizeof(RTX_MSG_HDR) + limit - 1);
					((RTX_MSG_HDR*)send_msg_buf)->length = sizeof(RTX_MSG_HDR) + limit - 1;
					((RTX_MSG_HDR*)send_msg_buf)->type = KEY_IN;
					char* buf = ((char*)send_msg_buf) + sizeof(RTX_MSG_HDR);

					int i = 1;
					for(;i<limit;i++)
					{
						buf[i-1] = input_buf[i];
					}

					#ifdef DEBUG_0
					for (int i = 0; i < ((RTX_MSG_HDR*)send_msg_buf)->length - sizeof(RTX_MSG_HDR); i++ ) {
						printf("%c", *(char*)((char*)send_msg_buf + sizeof(RTX_MSG_HDR) + i));
					}
					printf("\r\n");
					#endif

					if (send_msg(cmd_ids[input_buf[1]], send_msg_buf) == RTX_ERR)
					{
						print_string(ERRORSTRING);
						tail = 0;
						continue;
					}
					tail = -1;

				}
				else if(tail<64)
				{
					input_buf[tail] = recieved_char;
				}
				tail++;

			}
        }
    }
    
}
