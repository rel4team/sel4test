#include "async_syscall.h"
#include "scheduler.h"

extern ipc_buffer *SENDER_MAP[MAX_CONNNECTIONS];

void seL4_Async_Send(seL4_CPtr ntfn, seL4_MessageInfo_t msg_info, int cid) {
    ipc_buffer *buf = SENDER_MAP[ntfn];
    int idle = find_first_zero(buf->bitmap);
    set_bit(buf->bitmap, idle);
    buf->items[idle].cid = cid;
    buf->items[idle].msg_info = msg_info.words[0];
    if (buf->recv_status == 0) {
        // need to wake
    }
}