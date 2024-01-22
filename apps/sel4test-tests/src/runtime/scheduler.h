#ifndef RUNTIME_SCHEDULER
#define RUNTIME_SCHEDULER

#include <vka/object.h>
#include <sel4/types.h>
#include "queue.h"

#define MAX_COROUTINE_NUM 128
#define MAX_RT_NUM 64
#define MAX_CONNNECTIONS 16
#define FORCE_INLINE __attribute__((always_inline))

typedef uint64_t bitmap_t;
static inline void FORCE_INLINE set_bit(bitmap_t *bitmap, int pos) {
    *bitmap |= (1ULL << pos);
}

static inline void FORCE_INLINE clear_bit(bitmap_t *bitmap, int pos) {
    *bitmap &= ~(1ULL << pos);
}

static inline int FORCE_INLINE get_bit(bitmap_t bitmap, int pos) {
    return (bitmap >> pos) & 1U;
}

inline int FORCE_INLINE find_first_zero(bitmap_t bitmap) {
    return __builtin_ctzll(~bitmap);
}

static inline int FORCE_INLINE find_first_one(bitmap_t bitmap) {
    return __builtin_ctzll(bitmap);
}

typedef struct ipc_item_t {
    uint64_t cid;
    uint64_t msg_info;
} ipc_item;

typedef struct ipc_buffer_t {
    seL4_Word recv_status;
    bitmap_t bitmap;
    ipc_item  items[64];
} ipc_buffer;

typedef struct recv_pair_t {
    int rt_id;
    int cid;
} recv_pair;

typedef struct send_pair_t {
    int recv_id;
    ipc_buffer *buf;
} send_pair;

typedef struct runtime_t {
    int rt_id;
    mco_coro *cos[MAX_COROUTINE_NUM];
    mco_coro *current;
    co_queue ready_queue;
    vka_object_t notification;
    seL4_CPtr tcb;
    ipc_buffer *sender_map[MAX_CONNNECTIONS];
    mco_coro *receiver_map[MAX_CONNNECTIONS];
} runtime;

typedef struct async_syscall_args_t {
    int rt_id;
    seL4_CPtr sender;
} async_syscall_args;

typedef struct async_recv_args_t {
    int sender_id;
    ipc_buffer *recv_buf;
    ipc_buffer *send_buf;
} async_recv_args;


static inline FORCE_INLINE void yield_current() {
    mco_yield(((runtime *)seL4_GetRTAddr())->current);
}

int create_runtime(vka_t *vka, seL4_CPtr tcb, void *uintr_handler);

void runtime_run();

int spwan_coroutine(int runtime, void (*func)(void* args), void *args);

void wake_recv_coroutine(int uintr_irq);

int spwan_coroutine2(void (*func)(void* args), void *args);

int register_sender(int sender_ntfn, ipc_buffer *buf);

int register_receiver(int uintr_irq, void (*func)(void* args), void *args);

seL4_CPtr get_runtime_ntfn(int rt_id);

#endif
