#include "async_syscall.h"
#include "scheduler.h"

extern send_pair SENDER_MAP[MAX_CONNNECTIONS];
extern runtime RUNTIMES[MAX_RT_NUM];

void seL4_Async_Send(seL4_CPtr ntfn, seL4_MessageInfo_t msg_info, int rt_id) {
    mco_coro *current = RUNTIMES[rt_id].current;
    send_pair pair = SENDER_MAP[ntfn];
    ipc_buffer *buf = pair.buf;
    int idle = find_first_zero(buf->bitmap);
    printf("[seL4_Async_Send] buf: %p\n", buf);
    buf->items[idle].cid = cid;
    buf->items[idle].msg_info = msg_info.words[0];
    set_bit(&buf->bitmap, idle);
    
    if (buf->recv_status == 0) {
        // need to wake
    }
    mco_yield(current);
    seL4_MessageInfo_t reply;
    mco_pop(current, &reply, sizeof(reply));
    
}