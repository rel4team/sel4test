
#include <assert.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <vka/object.h>

#include "../helpers.h"

#include "../runtime/scheduler.h"
#include "../runtime/async_syscall.h"

static ipc_buffer *alloc_ipc_buffer(env_t env) {
    cspacepath_t frame_path;
    seL4_CPtr frame_cap = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
    // test_assert(frame_cap != seL4_CapNull);
    vka_cspace_make_path(&env->vka, frame_cap, &frame_path);
    ipc_buffer *buf = vspace_map_pages(&env->vspace, &frame_cap, seL4_Null, seL4_AllRights, 1, seL4_PageBits, 1);
    return buf;
}

static void
runtime_helper(int rt_id)
{
    printf("server helper: rt id: %d\n", rt_id);
    runtime_run(rt_id);
}


void client_recv_coroutine(void *recv_buf) {
    ipc_buffer *buf = recv_buf;
    // static long long int flga = 1;
    // while (1) {
    //     int idx = find_first_one(buf->bitmap);
    //     // printf("[client_recv_coroutine] idx: %d\n", idx);
    //     // ipc_item item = buf->items[idx];
    //     // printf("ipc_item: %d, %d\n", item.cid, item.msg_info);
    // }
}


void server_recv_coroutine(void *recv_buf) {
    ipc_buffer *buf = recv_buf;
    printf("[server_recv_coroutine] buf: %p\n", buf);
    while (1) {
        int idx = find_first_one(buf->bitmap);
        if (idx != -1) {
            printf("[server_recv_coroutine] idx: %d\n", idx);
            ipc_item item = buf->items[idx];
            printf("ipc_item: %d, %d\n", item.cid, item.msg_info);
        }
        // if (idx < 0 || idx >=64) {
        //     break;
        // }
        
    }
}

typedef struct async_syscall_args_t {
    int rt_id;
    seL4_CPtr sender;
} async_syscall_args;

void async_syscall_send(void *args) {
    async_syscall_args *new_args = args;
    seL4_MessageInfo_t msg = {.words = {0}};
    seL4_Async_Send(new_args->sender, msg, 666);
}

static int test_async_ipc(env_t env)
{
    int client_rt_id = create_runtime(&env->vka, env->tcb);
    helper_thread_t server;
    create_helper_thread(env, &server);
    set_helper_affinity(env, &server, 1);
    int server_rt_id = create_runtime(&env->vka, server.thread.tcb.cptr);

    // alloc ipc buffer
    ipc_buffer *send_buf = alloc_ipc_buffer(env);
    ipc_buffer *recv_buf = alloc_ipc_buffer(env);
    printf("send buf:%p, recv buf: %p\n", send_buf, recv_buf);
    

    // client register connection
    register_sender(get_runtime_ntfn(server_rt_id), send_buf, 0);
    register_receiver(client_rt_id, 0, client_recv_coroutine, recv_buf);

    // server register connection
    register_sender(get_runtime_ntfn(client_rt_id), recv_buf, 0);
    register_receiver(server_rt_id, 0, server_recv_coroutine, send_buf);

    start_helper(env, &server, (helper_fn_t) runtime_helper, server_rt_id,
                 0, 0, 0);
    async_syscall_args args = {.rt_id = client_rt_id, .sender = get_runtime_ntfn(server_rt_id)};

    spwan_coroutine(client_rt_id, async_syscall_send, &args);
    
    runtime_run(client_rt_id);

    wait_for_helper(&server);

    return sel4test_get_result();
}



DEFINE_TEST(ASYNC001,
            "Test that a bound tcb waiting on a sync endpoint receives normal sync ipc and notification notifications.",
            test_async_ipc, true)