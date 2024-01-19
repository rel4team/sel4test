#ifndef RUNTIME_SCHEDULER
#define RUNTIME_SCHEDULER

#include <vka/object.h>
#include <sel4/types.h>
#include "queue.h"

#define MAX_COROUTINE_NUM 128
#define MAX_RT_NUM 16
#define MAX_CONNNECTIONS 4096
#define FORCE_INLINE __attribute__((always_inline))

typedef uint64_t bitmap_t;
inline void FORCE_INLINE set_bit(bitmap_t *bitmap, int pos) {
    *bitmap |= (1ULL << pos);
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

typedef struct runtime_t {
    mco_coro *cos[MAX_COROUTINE_NUM];
    mco_coro *current;
    co_queue ready_queue;
    vka_object_t notification;
} runtime;


typedef struct ipc_item_t {
    uint64_t cid;
    uint64_t msg_info;
} ipc_item;

typedef struct ipc_buffer_t {
    seL4_Word recv_status;
    bitmap_t bitmap;
    ipc_item  items[64];
} ipc_buffer;



#endif
