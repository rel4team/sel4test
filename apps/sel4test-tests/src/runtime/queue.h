#ifndef RUNTIME_QUEUE

#define RUNTIME_QUEUE

#include "minicoro.h"

#define QUEUE_SIZE 1024

typedef struct co_queue_t {
	mco_coro *cos[QUEUE_SIZE];
	int rear;
	int front;
} co_queue;

void co_queue_init(co_queue *q);

int co_queue_full(co_queue *q);

int co_queue_empty(co_queue *q);

int co_queue_push(co_queue *q, mco_coro *co);

mco_coro *co_queue_pop(co_queue *q);

#endif // RUNTIME_QUEUE