#ifndef RUNTIME_ASYNC_SYSCALL
#define RUNTIME_ASYNC_SYSCALL
#include <sel4/types.h>

int seL4_Async_Send(seL4_CPtr send_id, seL4_MessageInfo_t msg_info, seL4_MessageInfo_t *reply);

#endif