#include "queue.h"
#include <string.h>
void co_queue_create(co_queue *q) {
    memset(q->cos, 0x00, sizeof(q->cos));
    q->front = 0;
    q->rear = 0;
}

inline int co_queue_full(co_queue *q) {
    return ((q->rear + 1) % QUEUE_SIZE) == q->front;
}

inline int co_queue_empty(co_queue *q) {
    return q->front == q->rear;
}

int co_queue_push(co_queue *q, mco_coro *co) {
    if (co_queue_full(q)) {
        return -1;
    }
    q->cos[q->rear] = co;
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    return 0;
}

mco_coro *co_queue_pop(co_queue *q) {
    if (co_queue_empty(q)) {
        return NULL;
    }
    mco_coro *res = q->cos[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    return res;
}