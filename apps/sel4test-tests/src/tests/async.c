
// #include <assert.h>
// #include <stdio.h>
// #include <sel4/sel4.h>
// #include <vka/object.h>

// #include "../helpers.h"

// #include "../runtime/scheduler.h"
// #include "../runtime/async_syscall.h"
// #include "../runtime/uintr.h"

// extern runtime RUNTIMES[MAX_RT_NUM];

// uint64_t client_uintr_handler(struct __uintr_frame *ui_frame, uint64_t irqs) {
//     wake_recv_coroutine(__builtin_ctzll(irqs));
//     return 0;
// }

// uint64_t server_uintr_handler(struct __uintr_frame *ui_frame, uint64_t irqs) {
//     printf("\t-- Server User Interrupt handler --\n");

// //   // read pending bits
// //   printf("\tPending User Interrupts: %lx\n", irqs);
//     wake_recv_coroutine(__builtin_ctzll(irqs));

//   return 0;
// }

// void async_syscall_send(void *args) {
//     async_syscall_args *new_args = args;
//     seL4_MessageInfo_t msg = {.words = {100}};
//     seL4_MessageInfo_t reply = {.words = {0}};
//     seL4_Async_Send(new_args->sender, msg, &reply);
//     printf("async send complete, reply: {}", reply.words[0]);
// }

// void client_recv_coroutine(ipc_buffer *buf) {
//     static long long int flag = 1;
//     while (1) {
//         int idx = find_first_one(buf->bitmap);
//         if (idx == -1) {
//             buf->recv_status = 0;
//             printf("client recv co blocked\n");
//             yield_current();
//         } else {
//             printf("client recv coroutine waked\n");
//             printf("[client_recv_coroutine] idx: %d\n", idx);
//             // printf("client recv co waked\n");
//             ipc_item item = buf->items[idx];
//             printf("ipc_item: %d, %d\n", item.cid, item.msg_info);
//             clear_bit(&buf->bitmap, idx);
//         }
//     }
// }


// static ipc_buffer *alloc_ipc_buffer(env_t env) {
//     cspacepath_t frame_path;
//     seL4_CPtr frame_cap = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
//     // test_assert(frame_cap != seL4_CapNull);
//     vka_cspace_make_path(&env->vka, frame_cap, &frame_path);
//     ipc_buffer *buf = vspace_map_pages(&env->vspace, &frame_cap, seL4_Null, seL4_AllRights, 1, seL4_PageBits, 1);
//     return buf;
// }

// static void
// runtime_helper(int ntfn, ipc_buffer *send_buf, ipc_buffer *recv_buf, int uintr_irq)
// {
//     seL4_SetRTAddr(&RUNTIMES[1]);
//     // enable user interrupt
//     csr_set(CSR_USTATUS, USTATUS_UIE);
// 	csr_set(CSR_UIE, MIE_USIE);
//     // printf("client helper: rt id: %d\n", ntfn);
    
//     int sender_id = register_sender(ntfn, send_buf);

//     register_receiver(uintr_irq, client_recv_coroutine, recv_buf);

//     async_syscall_args args = {.rt_id = -1, .sender = sender_id};
//     spwan_coroutine2(async_syscall_send, &args);
//     runtime_run();
// }


// void server_recv_coroutine(void *args) {
//     async_recv_args *new_args = (async_recv_args *)args;
//     ipc_buffer *recv_buf = new_args->recv_buf;
//     ipc_buffer *reply_buf = new_args->send_buf;
//     // printf("[server_recv_coroutine] buf: %p\n", recv_buf);
//     while (1) {
//         int idx = find_first_one(recv_buf->bitmap);
//         if (idx != -1) {
//             // printf("server recv coroutine waked\n");
//             // printf("[server_recv_coroutine] idx: %d\n", idx);
//             ipc_item item = recv_buf->items[idx];
//             // printf("ipc_item: %d, %d\n", item.cid, item.msg_info);
//             clear_bit(&recv_buf->bitmap, idx);
            

//             // reply
//             int idle = find_first_zero(reply_buf->bitmap);
//             if (idle == -1) {
//                 printf("reply buffer full\n");
//                 return -1;
//             }
//             set_bit(&reply_buf->bitmap, idle);
//             reply_buf->items[idle].cid = item.cid;
//             reply_buf->items[idle].msg_info = item.msg_info + 1;
//             if (reply_buf->recv_status == 0) {
//                 uipi_send(new_args->sender_id);
//             }
//         } else {
//             // printf("server recv coroutine blocked\n");
//             recv_buf->recv_status = 0;
//             yield_current();
//         }
//     }
// }



// static int test_async_ipc(env_t env)
// {
//     int server_rt_id = create_runtime(&env->vka, env->tcb, server_uintr_handler);
//     seL4_SetRTAddr(&RUNTIMES[0]);
//     helper_thread_t client;
//     create_helper_thread(env, &client);
//     set_helper_affinity(env, &client, 1);
//     int client_rt_id = create_runtime(&env->vka, client.thread.tcb.cptr, client_uintr_handler);

//     // alloc ipc buffer
//     ipc_buffer *send_buf = alloc_ipc_buffer(env);
//     ipc_buffer *recv_buf = alloc_ipc_buffer(env);
//     printf("send buf:%p, recv buf: %p\n", send_buf, recv_buf);

//     int sender_id = register_sender(get_runtime_ntfn(client_rt_id), recv_buf);
//     async_recv_args args = {.recv_buf = send_buf, .sender_id = sender_id, .send_buf = recv_buf};
//     register_receiver(0, server_recv_coroutine, &args);

//     start_helper(env, &client, (helper_fn_t) runtime_helper, get_runtime_ntfn(server_rt_id),
//                  (seL4_Word) send_buf, (seL4_Word) recv_buf, sender_id);
//     // async_syscall_args args = {.rt_id = client_rt_id, .sender = get_runtime_ntfn(server_rt_id)};

//     // spwan_coroutine(client_rt_id, async_syscall_send, &args);
//     // uipi_send(sender_id);
//     runtime_run();
//     while (1) {
        
//     }
//     // runtime_run(client_rt_id);
//     wait_for_helper(&client);

//     return sel4test_get_result();
// }



// DEFINE_TEST(ASYNC001,
//             "Test that a bound tcb waiting on a sync endpoint receives normal sync ipc and notification notifications.",
//             test_async_ipc, true)