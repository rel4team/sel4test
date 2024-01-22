#define MINICORO_IMPL
#include "scheduler.h"
#include "uintr.h"
#include <stdio.h>
#include <sel4/sel4.h>
#include <sel4/types.h>
#include <vka/object.h>
#include <assert.h>


runtime RUNTIMES[MAX_RT_NUM];

static bitmap_t RT_BITMAP;

send_pair SENDER_MAP[MAX_CONNNECTIONS];


static recv_pair RECEIVER_MAP[MAX_RT_NUM];


int create_runtime(vka_t *vka, seL4_CPtr tcb, void* uintr_handler) {
    int error;
    int idle = find_first_zero(RT_BITMAP);
    printf("[create_runtime] idle: %d %d\n", idle, RT_BITMAP);
    set_bit(&RT_BITMAP, idle);
    memset(&RUNTIMES[idle], 0, sizeof(runtime));
    error = vka_alloc_notification(vka, &RUNTIMES[idle].notification);
    if (error != 0) {
        printf("[create_runtime]fail to alloc notification: %d\n", error);
        return -1;
    }
    RUNTIMES[idle].tcb = tcb;
    error = seL4_TCB_BindNotification(tcb, RUNTIMES[idle].notification.cptr);
    if (error != 0) {
        printf("[create_runtime]fail to bind notification: %d\n", error);
        return -1;
    }

    error = uintr_register_receiver(tcb, RUNTIMES[idle].notification.cptr, uintr_handler);
    if (error != 0) {
        printf("[create_runtime]fail to register receiver. %d\n", error);
        return -1;
    }
    RUNTIMES[idle].rt_id = idle;
    // seL4_SetRTAddr(&RUNTIMES[idle]);

    return idle;
}

void runtime_run() {
    runtime *rt = (runtime *)seL4_GetRTAddr();
    mco_coro *current;
    while (1) {
        while (current = co_queue_pop(&rt->ready_queue)) {
            rt->current = current;
            mco_resume(current);
            if (mco_status(current) == MCO_DEAD) {
                printf("dead cortouine\n");
                rt->cos[current->cid] = NULL;
                mco_destroy(current);
            }
        }
        rt->current = NULL;
        // suspend thread
        // break;
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

int spwan_coroutine2(void (*func)(void* args), void *args) {
    runtime *rt = (runtime *)seL4_GetRTAddr();
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
        if (rt->cos[i] == NULL) {
            cid = i;
            co->cid = cid;
            rt->cos[i] = co;
            co_queue_push(&rt->ready_queue, co);
            break;
        }
    }
    return cid;
}

int register_sender(int sender_ntfn, ipc_buffer *buf) {
    uintr_register_sender(sender_ntfn);
    int sender_id = seL4_GetUIntrFlag();
    ((runtime *)seL4_GetRTAddr())->sender_map[sender_id] = buf;
    return sender_id;
}

int register_receiver(int uintr_irq, void (*func)(void* args), void *args) {
    runtime *rt = (runtime *)seL4_GetRTAddr();
    int cid = spwan_coroutine2(func, args);
    rt->receiver_map[uintr_irq] = rt->cos[cid];
    return 0;
}

seL4_CPtr get_runtime_ntfn(int rt_id) {
    return RUNTIMES[rt_id].notification.cptr;
} 


void wake_recv_coroutine(int uintr_irq) {
    runtime *rt = (runtime *)seL4_GetRTAddr();
    printf("[client_uintr_handler] vec: %d", uintr_irq);
    mco_coro *co = rt->receiver_map[uintr_irq];
    co_queue_push(&rt->ready_queue, co);
}