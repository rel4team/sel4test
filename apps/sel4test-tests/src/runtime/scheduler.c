#define MINICORO_IMPL
#include "scheduler.h"
#include <stdio.h>
#include <sel4/sel4.h>
#include <sel4/types.h>
#include <vka/object.h>
#include <assert.h>


runtime RUNTIMES[MAX_RT_NUM];

static bitmap_t RT_BITMAP;

send_pair SENDER_MAP[MAX_CONNNECTIONS];


static recv_pair RECEIVER_MAP[MAX_RT_NUM];

int create_runtime(vka_t *vka, seL4_CPtr tcb) {
    int error;
    int idle = find_first_zero(RT_BITMAP);
    printf("[create_runtime] idle: %d %d\n", idle, RT_BITMAP);
    set_bit(&RT_BITMAP, idle);
    memset(&RUNTIMES[idle], 0, sizeof(runtime));
    printf("helo\n");
    error = vka_alloc_notification(vka, &RUNTIMES[idle].notification);
    if (error != 0) {
        printf("[create_runtime]fail to alloc notification: %d\n", error);
        return -1;
    }
    error = seL4_TCB_BindNotification(tcb, RUNTIMES[idle].notification.cptr);
    if (error != 0) {
        printf("[create_runtime]fail to bould notification: %d\n", error);
        return -1;
    }
    
    return idle;
}

void runtime_run(int runtime) {
    mco_coro *current;
    while (1) {
        while (current = co_queue_pop(&RUNTIMES[runtime].ready_queue)) {
            RUNTIMES[runtime].current = current;
            mco_resume(current);
            if (mco_status(current) == MCO_DEAD) {
                printf("dead cortouine\n");
                RUNTIMES[runtime].cos[current->cid] = NULL;
                mco_destroy(current);
            }
        }
        RUNTIMES[runtime].current = NULL;
        // suspend thread
        break;
    }
}

void coroutine_helper(mco_coro *co) {
    void (*func)(void *args);
    void *args;
    mco_pop(co, &args, sizeof(args));
    mco_pop(co, &func, sizeof(func));
    func(args);
}

int spwan_coroutine(int runtime, void (*func)(void* args), void *args) {
    mco_coro* co;
    mco_desc desc = mco_desc_init(coroutine_helper, 0);
    mco_result res = mco_create(&co, &desc);
    if(res != MCO_SUCCESS) {
        printf("Failed to create coroutine\n");
        return -1;
    }
    mco_push(co, &func, sizeof(func));
    mco_push(co, &args, sizeof(args));
    int cid = -1;
    for (int i = 0; i < MAX_COROUTINE_NUM; ++i) {
        if (RUNTIMES[runtime].cos[i] == NULL) {
            cid = i;
            co->cid = cid;
            RUNTIMES[runtime].cos[i] = co;
            co_queue_push(&RUNTIMES[runtime].ready_queue, co);
            break;
        }
    }
    return cid;
}

int register_sender(seL4_CPtr sender_ntfn, ipc_buffer *buf, int recv_id) {
    SENDER_MAP[sender_ntfn].buf = buf;
    SENDER_MAP[sender_ntfn].recv_id = recv_id;
    return 0;
}

int register_receiver(int rt_id, int sender_id, void (*func)(void* args), void *args) {
    int cid = spwan_coroutine(rt_id, func, args);
    RECEIVER_MAP[sender_id].rt_id = rt_id;
    RECEIVER_MAP[sender_id].cid = cid;
    return 0;
}

seL4_CPtr get_runtime_ntfn(int rt_id) {
    return RUNTIMES[rt_id].notification.cptr;
} 