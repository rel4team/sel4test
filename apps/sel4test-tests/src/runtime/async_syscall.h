#ifndef RUNTIME_ASYNC_SYSCALL
#define RUNTIME_ASYNC_SYSCALL
#include <sel4/types.h>

void seL4_Async_Send(seL4_CPtr ntfn, seL4_MessageInfo_t msg_info, int cid);

#endif