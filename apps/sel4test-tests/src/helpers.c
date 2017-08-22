/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#include <sel4/sel4.h>
#include <sel4utils/arch/util.h>
#include <sel4utils/helpers.h>

#include <sel4test/test.h>
#include <stdarg.h>
#include <stdlib.h>

#include <utils/util.h>
#include <vka/capops.h>

#include <sel4platsupport/timer.h>

#include "helpers.h"
#include "test.h"

int
check_zeroes(seL4_Word addr, seL4_Word size_bytes)
{
    test_assert_fatal(IS_ALIGNED(addr, sizeof(seL4_Word)));
    test_assert_fatal(IS_ALIGNED(size_bytes, sizeof(seL4_Word)));
    seL4_Word *p = (void*)addr;
    seL4_Word size_words = size_bytes / sizeof(seL4_Word);
    while (size_words--) {
        if (*p++ != 0) {
            ZF_LOGE("Found non-zero at position %ld: %lu\n", ((long)p) - (addr), (unsigned long)p[-1]);
            return 0;
        }
    }
    return 1;
}

/* Determine whether a given slot in the init thread's CSpace is empty by
 * examining the error when moving a slot onto itself.
 *
 * Serves as == 0 comparator for caps.
 */
int
is_slot_empty(env_t env, seL4_Word slot)
{
    int error;

    error = cnode_move(env, slot, slot);

    assert(error == seL4_DeleteFirst || error == seL4_FailedLookup);
    return (error == seL4_FailedLookup);
}

seL4_Word
get_free_slot(env_t env)
{
    seL4_CPtr slot;
    UNUSED int error = vka_cspace_alloc(&env->vka, &slot);
    assert(!error);
    return slot;
}

int
cnode_copy(env_t env, seL4_CPtr src, seL4_CPtr dest, seL4_CapRights_t rights)
{
    cspacepath_t src_path, dest_path;
    vka_cspace_make_path(&env->vka, src, &src_path);
    vka_cspace_make_path(&env->vka, dest, &dest_path);
    return vka_cnode_copy(&dest_path, &src_path, rights);
}

int
cnode_delete(env_t env, seL4_CPtr slot)
{
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, slot, &path);
    return vka_cnode_delete(&path);
}

int
cnode_mint(env_t env, seL4_CPtr src, seL4_CPtr dest, seL4_CapRights_t rights, seL4_CapData_t badge)
{
    cspacepath_t src_path, dest_path;

    vka_cspace_make_path(&env->vka, src, &src_path);
    vka_cspace_make_path(&env->vka, dest, &dest_path);
    return vka_cnode_mint(&dest_path, &src_path, rights, badge);
}

int
cnode_move(env_t env, seL4_CPtr src, seL4_CPtr dest)
{
    cspacepath_t src_path, dest_path;

    vka_cspace_make_path(&env->vka, src, &src_path);
    vka_cspace_make_path(&env->vka, dest, &dest_path);
    return vka_cnode_move(&dest_path, &src_path);
}

int
cnode_mutate(env_t env, seL4_CPtr src, seL4_CPtr dest)
{
    cspacepath_t src_path, dest_path;

    vka_cspace_make_path(&env->vka, src, &src_path);
    vka_cspace_make_path(&env->vka, dest, &dest_path);
    return vka_cnode_mutate(&dest_path, &src_path, seL4_NilData);
}

int
cnode_cancelBadgedSends(env_t env, seL4_CPtr cap)
{
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, cap, &path);
    return vka_cnode_cancelBadgedSends(&path);
}

int
cnode_revoke(env_t env, seL4_CPtr cap)
{
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, cap, &path);
    return vka_cnode_revoke(&path);
}

int
cnode_rotate(env_t env, seL4_CPtr src, seL4_CPtr pivot, seL4_CPtr dest)
{
    cspacepath_t src_path, dest_path, pivot_path;

    vka_cspace_make_path(&env->vka, src, &src_path);
    vka_cspace_make_path(&env->vka, dest, &dest_path);
    vka_cspace_make_path(&env->vka, pivot, &pivot_path);
    return vka_cnode_rotate(&dest_path, seL4_NilData, &pivot_path, seL4_NilData, &src_path);
}

int
cnode_savecaller(env_t env, seL4_CPtr cap)
{
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, cap, &path);
    return vka_cnode_saveCaller(&path);
}

int
are_tcbs_distinct(seL4_CPtr tcb1, seL4_CPtr tcb2)
{
    seL4_UserContext regs;

    /* Initialise regs to prevent compiler warning. */
    int error = seL4_TCB_ReadRegisters(tcb1, 0, 0, 1, &regs);
    if (error) {
        return -1;
    }

    for (int i = 0; i < 2; ++i) {
        sel4utils_set_instruction_pointer(&regs, i);
        error = seL4_TCB_WriteRegisters(tcb1, 0, 0, 1, &regs);

        /* Check that we had permission to do that and the cap was a TCB. */
        if (error) {
            return -1;
        }

        error = seL4_TCB_ReadRegisters(tcb2, 0, 0, 1, &regs);

        /* Check that we had permission to do that and the cap was a TCB. */
        if (error) {
            return -1;
        } else if (sel4utils_get_instruction_pointer(regs) != i) {
            return 1;
        }

    }

    return 0;
}

void
create_helper_process(env_t env, helper_thread_t *thread)
{
    UNUSED int error;

    error = vka_alloc_endpoint(&env->vka, &thread->local_endpoint);
    assert(error == 0);

    thread->is_process = true;

    sel4utils_process_config_t config = {0};
    config = process_config_asid_pool(config, env->asid_pool);
    config = process_config_noelf(config, NULL, 0);
    config = process_config_create_cnode(config, env->cspace_size_bits);
    config = process_config_create_vspace(config, env->regions, env->num_regions);
    vka_object_t fault_endpoint = { .cptr = env->endpoint };
    config = process_config_fault_endpoint(config, fault_endpoint);
    config = process_config_priority(config, OUR_PRIO - 1);

    error = sel4utils_configure_process_custom(&thread->process, &env->vka, &env->vspace,
                                               config);
    assert(error == 0);

    /* copy the elf reservations we need into the current process */
    memcpy(thread->regions, env->regions, sizeof(sel4utils_elf_region_t) * env->num_regions);
    thread->num_regions = env->num_regions;

    /* clone data/code into vspace */
    for (int i = 0; i < env->num_regions; i++) {
        error = sel4utils_bootstrap_clone_into_vspace(&env->vspace, &thread->process.vspace, thread->regions[i].reservation);
        assert(error == 0);
    }

    thread->thread = thread->process.thread;
    assert(error == 0);
}

NORETURN static void
signal_helper_finished(seL4_CPtr local_endpoint, int val)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, val);
    seL4_Call(local_endpoint, info);
    assert(0);
    while (1);
}

NORETURN static void
helper_thread(int argc, char **argv)
{

    helper_fn_t entry_point = (void *) atol(argv[0]);
    seL4_CPtr local_endpoint = (seL4_CPtr) atol(argv[1]);

    seL4_Word args[HELPER_THREAD_MAX_ARGS] = {0};
    for (int i = 2; i < argc && i - 2 < HELPER_THREAD_MAX_ARGS; i++) {
        assert(argv[i] != NULL);
        args[i - 2] = atol(argv[i]);
    }

    /* run the thread */
    int result = entry_point(args[0], args[1], args[2], args[3]);
    signal_helper_finished(local_endpoint, result);
    /* does not return */
}

extern uintptr_t sel4_vsyscall[];

void
start_helper(env_t env, helper_thread_t *thread, helper_fn_t entry_point,
             seL4_Word arg0, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3)
{

    UNUSED int error;

    seL4_CPtr local_endpoint;

    if (thread->is_process) {
        /* copy the local endpoint */
        cspacepath_t path;
        vka_cspace_make_path(&env->vka, thread->local_endpoint.cptr, &path);
        local_endpoint = sel4utils_copy_path_to_process(&thread->process, path);
    } else {
        local_endpoint = thread->local_endpoint.cptr;
    }

    sel4utils_create_word_args(thread->args_strings, thread->args, HELPER_THREAD_TOTAL_ARGS,
        (seL4_Word) entry_point, local_endpoint,
        arg0, arg1, arg2, arg3);

    if (thread->is_process) {
        thread->process.entry_point = (void*)helper_thread;
        error = sel4utils_spawn_process_v(&thread->process, &env->vka, &env->vspace,
                                        HELPER_THREAD_TOTAL_ARGS, thread->args, 0);
        assert(error == 0);
        /* sel4utils_spawn_process_v has created a stack frame that contains, amongst other
           things, our arguments. Since we are going to be running a clone of this vspace
           we would like to not call _start as this will result in initializing the C library
           a second time. As we know the argument count and where argv will start we can
           construct a register (or stack) layout that will allow us to pretend to be doing
           a function call to helper_thread. */
        seL4_UserContext context;
        uintptr_t argv_base = (uintptr_t)thread->process.thread.initial_stack_pointer + sizeof(long);
        uintptr_t aligned_stack_pointer = ALIGN_DOWN((uintptr_t)thread->process.thread.initial_stack_pointer, STACK_CALL_ALIGNMENT);
        error = sel4utils_arch_init_context_with_args((sel4utils_thread_entry_fn)helper_thread, (void*)HELPER_THREAD_TOTAL_ARGS,
                                              (void*)argv_base, NULL, false, (void*)aligned_stack_pointer,
                                              &context, &env->vka, &env->vspace, &thread->process.vspace);
        assert(error == 0);
        error = seL4_TCB_WriteRegisters(thread->process.thread.tcb.cptr, 1, 0, sizeof(seL4_UserContext) / sizeof(seL4_Word), &context);
        assert(error == 0);
    } else {
        error = sel4utils_start_thread(&thread->thread, (sel4utils_thread_entry_fn)helper_thread,
                                       (void *) HELPER_THREAD_TOTAL_ARGS, (void *) thread->args, 1);
        assert(error == 0);
    }
}

void
cleanup_helper(env_t env, helper_thread_t *thread)
{

    vka_free_object(&env->vka, &thread->local_endpoint);

    if (thread->is_process) {
        /* free the regions (no need to unmap, as the
        * entry address space / cspace is being destroyed */
        for (int i = 0; i < thread->num_regions; i++) {
            vspace_free_reservation(&thread->process.vspace, thread->regions[i].reservation);
        }

        thread->process.fault_endpoint.cptr = 0;
        sel4utils_destroy_process(&thread->process, &env->vka);
    } else {
        sel4utils_clean_up_thread(&env->vka, &env->vspace, &thread->thread);
    }
}

void
create_helper_thread(env_t env, helper_thread_t *thread)
{
    UNUSED int error;

    error = vka_alloc_endpoint(&env->vka, &thread->local_endpoint);
    assert(error == 0);

    thread->is_process = false;
    thread->fault_endpoint = env->endpoint;
    seL4_CapData_t data = seL4_CapData_Guard_new(0, seL4_WordBits - env->cspace_size_bits);
    error = sel4utils_configure_thread(&env->vka, &env->vspace, &env->vspace, env->endpoint,
                                       env->cspace_root, data, &thread->thread);
    assert(error == 0);

    /* set its priority */
    set_helper_priority(env, thread, OUR_PRIO - 1);
}

int
wait_for_helper(helper_thread_t *thread)
{
    seL4_Word badge;

    seL4_Recv(thread->local_endpoint.cptr, &badge);
    return seL4_GetMR(0);
}

void
set_helper_priority(env_t env, helper_thread_t *thread, seL4_Word prio)
{
    UNUSED int error;
    error = seL4_TCB_SetPriority(thread->thread.tcb.cptr, prio);
    assert(error == seL4_NoError);
}

void
set_helper_mcp(env_t env, helper_thread_t *thread, seL4_Word mcp)
{
    UNUSED int error;
    error = seL4_TCB_SetMCPriority(thread->thread.tcb.cptr, mcp);
    assert(error == seL4_NoError);
}

void
set_helper_affinity(UNUSED env_t env, helper_thread_t *thread, seL4_Word affinity)
{
#ifdef CONFIG_KERNEL_RT
    seL4_Time timeslice = CONFIG_BOOT_THREAD_TIME_SLICE * US_IN_S;
    int error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple, affinity),
                                       thread->thread.sched_context.cptr,
                                       timeslice, timeslice, 0, 0);
    ZF_LOGF_IF(error, "Failed to configure scheduling context");
#elif CONFIG_MAX_NUM_NODES > 1
    int error = seL4_TCB_SetAffinity(thread->thread.tcb.cptr, affinity);
    ZF_LOGF_IF(error, "Failed to set tcb affinity");
#endif
}

seL4_CPtr
get_helper_tcb(helper_thread_t *thread)
{
    return thread->thread.tcb.cptr;
}

uintptr_t
get_helper_ipc_buffer_addr(helper_thread_t *thread)
{
    return thread->thread.ipc_buffer_addr;
}

uintptr_t
get_helper_initial_stack_pointer(helper_thread_t *thread)
{
    return (uintptr_t)thread->thread.initial_stack_pointer;
}

void
wait_for_timer_interrupt(env_t env)
{
    seL4_Word sender_badge;
    seL4_Wait(env->timer_notification.cptr, &sender_badge);
    sel4platsupport_handle_timer_irq(&env->timer, sender_badge);
}

void
wait(env_t env, uint64_t ns)
{
    ltimer_set_timeout(&env->timer.ltimer, ns, TIMEOUT_RELATIVE);
    wait_for_timer_interrupt(env);
}

void
sleep(env_t env, uint64_t ns)
{
    uint64_t start = 0;
    uint64_t end = 0;
    uint64_t new_end = 0;

    UNUSED int error = ltimer_get_time(&env->timer.ltimer, &start);
    end = start;
    do {
        int error = ltimer_set_timeout(&env->timer.ltimer, ns - (end - start), TIMEOUT_RELATIVE);
        ZF_LOGF_IF(error, "failed to get time");

        ZF_LOGV("Waiting for timer irq");
        wait_for_timer_interrupt(env);
        error = ltimer_get_time(&env->timer.ltimer, &new_end);
        ZF_LOGF_IF(error, "failed to get time");

        if (end == new_end) {
            ZF_LOGE("Time doesn't appear to be changing");
        }
        end = new_end;


        ZF_LOGV("Got it");
        if (end - start < ns) {
            ZF_LOGE("Wanted to wait: %"PRIu64", actually %"PRIu64"\n", ns, end - start);
        }

    } while (end - start < ns);
}

uint64_t
timestamp(env_t env)
{
    uint64_t time = 0;
    UNUSED int error = ltimer_get_time(&env->timer.ltimer, &time);
    ZF_LOGF_IF(error, "failed to get time");
    return time;
}
