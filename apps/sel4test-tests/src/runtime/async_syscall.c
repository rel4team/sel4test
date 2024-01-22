#include "async_syscall.h"
#include "scheduler.h"
#include "uintr.h"

extern send_pair SENDER_MAP[MAX_CONNNECTIONS];
extern runtime RUNTIMES[MAX_RT_NUM];

int seL4_Async_Send(seL4_CPtr send_id, seL4_MessageInfo_t msg_info, seL4_MessageInfo_t *reply) {
    runtime *rt = (runtime *)seL4_GetRTAddr();
    mco_coro *current = rt->current;
    ipc_buffer *buf = rt->sender_map[send_id];
    int idle = find_first_zero(buf->bitmap);
    if (idle == -1) {
        printf("ipc buffer full\n");
        return -1;
    }
    set_bit(&buf->bitmap, idle);
    printf("[seL4_Async_Send] buf: %p\n", buf);
    buf->items[idle].cid = current->cid;
    buf->items[idle].msg_info = msg_info.words[0];
    
    if (buf->recv_status == 0) {
        uipi_send(send_id);
    }
    mco_yield(current);
    mco_pop(current, reply, sizeof(seL4_MessageInfo_t));
    return 0;
}