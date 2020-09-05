/*
Copyright (c) 2018, Bernie Woodland
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//! @file    nufr-kernel-semaphore.c
//! @authors Bernie Woodland
//! @date    8Aug17

#define NUFR_SEMA_GLOBAL_DEFS

#include "nufr-global.h"

#if NUFR_CS_SEMAPHORE == 1

#include "nufr-kernel-base-semaphore.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-kernel-semaphore.h"
#include "nufr-kernel-task.h"
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    #include "nufr-kernel-task-inlines.h"
    #include "nufr-kernel-semaphore-inlines.h"
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
#include "nufr-kernel-timer.h"

#include "raging-contract.h"
#include "raging-utils-mem.h"

#ifdef USING_SECONDARY_CONTEXT_SWITCH_FILE
    #include "secondary-context-switch.h"
#endif

//!
//!  @brief       Semaphore blocks
//!
nufr_sema_block_t nufr_sema_block[NUFR_NUM_SEMAS];


// API calls


//!
//! @name      nufrkernel_sema_reset
//!
//! @brief     Initializes a sema, to be used at bootup.
//!
//! @details   Does not do a warm reset of sema, so if tasks are
//! @details   already on sema's task wait list, they're not handled.
//! @details    Does not do a warm reset of sema, so if tasks are
//! 
//! @param[in] 'sema_block'-- task added to this sema's wait list
//! @param[in] 'initial_count'--
//!
void nufrkernel_sema_reset(nufr_sema_block_t *sema_block,
                           unsigned           initial_count,
                           bool    priority_inversion_protection)
{
    KERNEL_REQUIRE_API(NUFR_IS_SEMA_BLOCK(sema_block));

    rutils_memset(sema_block, 0, sizeof(nufr_sema_block_t));

    sema_block->count = initial_count;

    if (priority_inversion_protection)
    {
        sema_block->flags |= NUFR_SEMA_PREVENT_PRI_INV;
    }
}

// If using inlines, forbid using these
#if NUFR_CS_OPTIMIZATION_INLINES == 0

//!
//! @name      nufrkernel_sema_link_task
//!
//! brief      Internal call to add a tcb to a sema's wait list.
//! brief      Since sema's wait list is priority sorted, insert
//! brief      must find slot in list to insert, according
//! brief      to priority.
//!
//! @details   Calling environment:
//! @details     (1) Caller must lock interrupts
//! @details     (2) This API intended for nufr kernel use
//! @details   
//!
//! @param[in] 'sema_block'-- task added to this sema's wait list
//! @param[in] 'add_tcb'--task to be added
//!
void nufrkernel_sema_link_task(nufr_sema_block_t *sema_block,
                               nufr_tcb_t        *add_tcb)
{
    nufr_tcb_t             *head_tcb;
    nufr_tcb_t             *tail_tcb;
    nufr_tcb_t             *prev_tcb;
    nufr_tcb_t             *next_tcb;
    unsigned                add_priority;

    KERNEL_REQUIRE_IL(sema_block != NULL);
    KERNEL_REQUIRE_IL(NUFR_IS_TCB(add_tcb));
    KERNEL_REQUIRE_IL(add_tcb->flink == NULL);
    KERNEL_REQUIRE_IL(add_tcb->blink == NULL);

    add_priority = add_tcb->priority;

    head_tcb = sema_block->task_list_head;
    tail_tcb = sema_block->task_list_tail;

    // Empty list?
    if (NULL == head_tcb)
    {
        sema_block->task_list_head = add_tcb;
        sema_block->task_list_tail = add_tcb;
    }
    // Append to tail?
    else if (add_priority >= tail_tcb->priority)
    {
        add_tcb->blink = tail_tcb;
        tail_tcb->flink = add_tcb;
        sema_block->task_list_tail = add_tcb;
    }
    // Insert in front of list?
    else if (add_priority < head_tcb->priority)
    {
        add_tcb->flink = head_tcb;
        head_tcb->blink = add_tcb;
        sema_block->task_list_head = add_tcb;
    }
    // Insert between 2 tcb's on list
    else
    {
        prev_tcb = head_tcb;
        next_tcb = prev_tcb->flink;

        KERNEL_REQUIRE_IL(prev_tcb != NULL);
        KERNEL_REQUIRE_IL(next_tcb != NULL);

        // Could've done a 'while(1)' instead
        while (NULL != next_tcb)
        {
            if (add_priority < next_tcb->priority)
            {
                break;
            }

            prev_tcb = next_tcb;
            next_tcb = next_tcb->flink;
        }

        KERNEL_REQUIRE_IL(next_tcb != NULL);

        // Stitch links together
        add_tcb->flink = prev_tcb->flink;
        prev_tcb->flink = add_tcb;
        add_tcb->blink = next_tcb->blink;
        next_tcb->blink = add_tcb;
    }

    // Sanity check sema task list
    KERNEL_ENSURE((sema_block->task_list_head == NULL) ==
           (sema_block->task_list_tail == NULL));
    KERNEL_ENSURE(sema_block->task_list_head != NULL ?
           sema_block->task_list_head->blink == NULL
           : true);
    KERNEL_ENSURE(sema_block->task_list_tail != NULL ?
           sema_block->task_list_tail->flink == NULL
           : true);
    KERNEL_ENSURE((sema_block->task_list_head != NULL) &&
           (sema_block->task_list_head != sema_block->task_list_tail) ?
           (sema_block->task_list_head->flink != NULL) &&
           (sema_block->task_list_tail->blink != NULL)
           : true);
}

//!
//! @name      nufrkernel_sema_unlink_task
//!
//! brief      Internal call to remove a tcb to a sema's wait list.
//! brief      Assumes that tcb is on a sema wait list.
//! brief      Since sema wait list is doubly-linked (both flink and blink),
//! brief      List walk not necessary.
//!
//! @details   Calling environment:
//! @details     (1) Caller must lock interrupts
//! @details     (2) This API intended for nufr kernel use
//! @details   
//!
//! @param[in] 'sema_block'-- task removed from this sema's wait list
//! @param[in] 'add_tcb'--task to be removed
//!
void nufrkernel_sema_unlink_task(nufr_sema_block_t *sema_block,
                                 nufr_tcb_t        *delete_tcb)
{
    KERNEL_REQUIRE_IL(sema_block != NULL);
    KERNEL_REQUIRE_IL(NUFR_IS_TCB(delete_tcb));

    // Deleting target at list head?
    // Then, adjust list head to next tcb after deleting target.
    // Note that 'delete_tcb->blink' must be NULL.
    if (sema_block->task_list_head == delete_tcb)
    {
        sema_block->task_list_head = delete_tcb->flink;
    }
    // Otherwise, there must be a tcb before deleting target.
    // Have the tcb before target point to tcb after target.
    // Clear deleting tcb's blink.
    else
    {
        (delete_tcb->blink)->flink = delete_tcb->flink;
    }

    // Deleting target at list tail?
    // Then, adjust list tail to previous tcb before deleting target.
    // Note that 'delete_tcb->flink' must be NULL.
    if (sema_block->task_list_tail == delete_tcb)
    {
        sema_block->task_list_tail = delete_tcb->blink;
    }
    // Otherwise, there must be a tcb after deleting target.
    // Have the tcb after target point to tcb before target.
    // Clear deleting tcb's flink.
    else
    {
        (delete_tcb->flink)->blink = delete_tcb->blink;
    }

    delete_tcb->flink = NULL;
    delete_tcb->blink = NULL;

    // Sanity check sema task list
    KERNEL_ENSURE((sema_block->task_list_head == NULL) ==
           (sema_block->task_list_tail == NULL));
    KERNEL_ENSURE(sema_block->task_list_head != NULL ?
           sema_block->task_list_head->blink == NULL
           : true);
    KERNEL_ENSURE(sema_block->task_list_tail != NULL ?
           sema_block->task_list_tail->flink == NULL
           : true);
    KERNEL_ENSURE((sema_block->task_list_head != NULL) &&
           (sema_block->task_list_head != sema_block->task_list_tail) ?
           (sema_block->task_list_head->flink != NULL) &&
           (sema_block->task_list_tail->blink != NULL)
           : true);
}

#endif  // NUFR_CS_OPTIMIZATION_INLINES == 0

//!
//! @name      nufr_sema_count_get
//
//! @details   
//!
//! @param[in] 'sema'
//!
//! @return    count
//!
unsigned nufr_sema_count_get(nufr_sema_t sema)
{
    nufr_sema_block_t *sema_block;

    sema_block = NUFR_SEMA_ID_TO_BLOCK(sema);

    KERNEL_REQUIRE_API(NUFR_IS_SEMA_BLOCK(sema_block));

    // No interrupt locking needed
    return sema_block->count;
}

//!
//! @name      nufr_sema_getW
//
//! @details   Cannot be called from an ISR or from BG task
//! @details
//!
//! @param[in] 'sema'
//! @param[in] 'abort_priority_of_rx_msg'--  If a message that's of a priority greater than
//! @param[in]    'abort_priority_of_rx_msg' is sent to the waiting task's message queue, the
//! @param[in]    semaphore wait is aborted
//!
//! @return     Get result
//!
nufr_sema_get_rtn_t nufr_sema_getW(nufr_sema_t    sema,
                                   nufr_msg_pri_t abort_priority_of_rx_msg)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_sr_reg_t           saved_psr;
    nufr_sema_block_t      *sema_block;
    nufr_sema_get_rtn_t     return_value = NUFR_SEMA_GET_OK_NO_BLOCK;
    nufr_tcb_t             *owner_tcb;
#if NUFR_CS_TASK_KILL == 1
    unsigned                notifications;
#endif  //NUFR_CS_TASK_KILL
    bool                    inv_protect;
    bool                    block_on_sema;

    sema_block = NUFR_SEMA_ID_TO_BLOCK(sema);

    KERNEL_REQUIRE_API(NUFR_IS_SEMA_BLOCK(sema_block));
    KERNEL_REQUIRE_API(nufr_running != (nufr_tcb_t *)nufr_bg_sp);
#if NUFR_CS_TASK_KILL == 1
    KERNEL_REQUIRE_API(abort_priority_of_rx_msg < NUFR_CS_MSG_PRIORITIES);
#else
    UNUSED(abort_priority_of_rx_msg);
#endif  //NUFR_CS_TASK_KILL

    // Assume this setting doesn't change after init, so don't need
    //  to block interrupts to read it.
    inv_protect = ANY_BITS_SET(sema_block->flags, NUFR_SEMA_PREVENT_PRI_INV);

    //######   Step One: Get semaphore
    //###
    saved_psr = NUFR_LOCK_INTERRUPTS();

    // We'll either be blocked on this sema or we'll own it
    nufr_running->sema_block = sema_block;

    // If count==0, then we'll have to block until the other/another
    //   task returns the/a sema
    block_on_sema = sema_block->count == 0;
    if (block_on_sema)
    {
    #if NUFR_CS_OPTIMIZATION_INLINES == 1
        NUFRKERNEL_BLOCK_RUNNING_TASK(NUFR_TASK_BLOCKED_SEMA);

        NUFRKERNEL_SEMA_LINK_TASK(sema_block, nufr_running);
    #else
        nufrkernel_block_running_task(NUFR_TASK_BLOCKED_SEMA);

        nufrkernel_sema_link_task(sema_block, nufr_running);
    #endif

        nufr_running->notifications = 0;
    #if NUFR_CS_TASK_KILL == 1
        nufr_running->abort_message_priority = abort_priority_of_rx_msg;
    #endif  // NUFR_CS_TASK_KILL

        owner_tcb = sema_block->owner_tcb;
        KERNEL_REQUIRE_IL(NUFR_IS_TCB(owner_tcb));

        if (inv_protect)
        {
            // Does a priority inversion condition exist?
            if (owner_tcb->priority > nufr_running->priority)
            {
                owner_tcb->statuses |= NUFR_TASK_INVERSION_PRIORITIZED;

                // If owner is blocked,
                //   then can simply poke new priority
                //   value into tcb.
                if (NUFR_IS_TASK_BLOCKED(owner_tcb))
                {
                    owner_tcb->priority_restore_inversion = owner_tcb->priority;
                    owner_tcb->priority = nufr_running->priority;
                }
                // Else, already in ready list.
                // Since priority is changing, must find it a new place in
                //   the ready list.
                else
                {
                #if NUFR_CS_OPTIMIZATION_INLINES == 1
                    NUFRKERNEL_DELETE_TASK_FROM_READY_LIST(owner_tcb);
                #else
                    nufrkernel_delete_task_from_ready_list(owner_tcb);
                #endif

                    owner_tcb->priority_restore_inversion = owner_tcb->priority;
                    owner_tcb->priority = nufr_running->priority;

                #if NUFR_CS_OPTIMIZATION_INLINES == 1
                    NUFRKERNEL_ADD_TASK_TO_READY_LIST(owner_tcb);
                   UNUSED(macro_do_switch);                  // suppress warning
                #else
                    nufrkernel_add_task_to_ready_list(owner_tcb);
                #endif
                }
            }
        }

        NUFR_INVOKE_CONTEXT_SWITCH();
    }
    else
    {
        // We now own the sema (Note relevancy if sema
        //  initialized with count > 1).
        sema_block->owner_tcb = nufr_running;

        (sema_block->count)--;
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    // Task will block/resume if 'block_on_sema' is 'true'

    //#####     Step Two: Calculate return value
    //###
#if NUFR_CS_TASK_KILL == 1
    if (block_on_sema)
    {
        // Interrupt locking not needed
        //saved_psr = NUFR_LOCK_INTERRUPTS();
    
        notifications = nufr_running->notifications;

        //NUFR_UNLOCK_INTERRUPTS(saved_psr);

        if (ANY_BITS_SET(notifications, NUFR_TASK_UNBLOCKED_BY_MSG_SEND))
        {
            return_value = NUFR_SEMA_GET_MSG_ABORT;
        }
        else
        {
            return_value = NUFR_SEMA_GET_OK_BLOCK;
        }
    }
#else
    if (block_on_sema)
    {
        return_value = NUFR_SEMA_GET_OK_BLOCK;
    }
#endif  // NUFR_CS_TASK_KILL

    return return_value;
}

//!
//! @name      nufr_sema_getT
//
//! @details   Cannot be called from an ISR or from BG task
//! @details
//!
//! @param[in] 'sema'
//! @param[in] 'abort_priority_of_rx_msg'--  If a message that's of a
//! @param[in]     priority greater than 'abort_priority_of_rx_msg' is sent
//! @param[in]     to the waiting task's message queue, the semaphore wait
//! @param[in]     is aborted.
//! @param[in] 'timeout_ticks'-- Timeout in OS ticks. If ==0, no waiting
//! @param[in]                   if sema not available.
//!
//! @return     Get result
//!
nufr_sema_get_rtn_t nufr_sema_getT(nufr_sema_t    sema,
                                   nufr_msg_pri_t abort_priority_of_rx_msg,
                                   unsigned       timeout_ticks)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_sr_reg_t           saved_psr;
    nufr_sema_block_t      *sema_block;
    nufr_sema_get_rtn_t     return_value;
    nufr_tcb_t             *owner_tcb;
    unsigned                notifications;
    bool                    inv_protect;
    bool                    block_on_sema;
    bool                    immediate_timeout;

    sema_block = NUFR_SEMA_ID_TO_BLOCK(sema);

    KERNEL_REQUIRE_API(NUFR_IS_SEMA_BLOCK(sema_block));
    KERNEL_REQUIRE_API(nufr_running != (nufr_tcb_t *)nufr_bg_sp);
#if NUFR_CS_TASK_KILL == 1
    KERNEL_REQUIRE_API(abort_priority_of_rx_msg < NUFR_CS_MSG_PRIORITIES);
#else
    UNUSED(abort_priority_of_rx_msg);
#endif

    inv_protect = ANY_BITS_SET(sema_block->flags, NUFR_SEMA_PREVENT_PRI_INV);
    immediate_timeout = (0 == timeout_ticks);

    //######   Step One: Get semaphore
    //###
    saved_psr = NUFR_LOCK_INTERRUPTS();

    // We'll either blocked on this sema or we'll own it
    nufr_running->sema_block = sema_block;

    // If count==0, then we'll have to block until the other/another
    //   task returns the/a sema
    block_on_sema = sema_block->count == 0;
    if (block_on_sema && !immediate_timeout)
    {
    #if NUFR_CS_OPTIMIZATION_INLINES == 1
        NUFRKERNEL_BLOCK_RUNNING_TASK(NUFR_TASK_BLOCKED_SEMA);

        NUFRKERNEL_SEMA_LINK_TASK(sema_block, nufr_running);
    #else
        nufrkernel_block_running_task(NUFR_TASK_BLOCKED_SEMA);

        nufrkernel_sema_link_task(sema_block, nufr_running);
    #endif

        nufr_running->notifications = 0;
    #if NUFR_CS_TASK_KILL == 1
        nufr_running->abort_message_priority = abort_priority_of_rx_msg;
    #endif  // NUFR_CS_TASK_KILL

        nufrkernel_add_to_timer_list(nufr_running, timeout_ticks);

        owner_tcb = sema_block->owner_tcb;
        KERNEL_REQUIRE_IL(NUFR_IS_TCB(owner_tcb));

        if (inv_protect)
        {
            // Does a priority inversion condition exist?
            if (owner_tcb->priority > nufr_running->priority)
            {
                owner_tcb->statuses |= NUFR_TASK_INVERSION_PRIORITIZED;

                // If owner is blocked,
                //   then can simply poke new priority
                //   value into tcb.
                if (NUFR_IS_TASK_BLOCKED(owner_tcb))
                {
                    owner_tcb->priority_restore_inversion = owner_tcb->priority;
                    owner_tcb->priority = nufr_running->priority;
                }
                // Else, already in ready list.
                // Since priority is changing, must find it a new place in
                //   the ready list.
                else
                {
                #if NUFR_CS_OPTIMIZATION_INLINES == 1
                    NUFRKERNEL_DELETE_TASK_FROM_READY_LIST(owner_tcb);
                #else
                    nufrkernel_delete_task_from_ready_list(owner_tcb);
                #endif

                    owner_tcb->priority_restore_inversion = owner_tcb->priority;
                    owner_tcb->priority = nufr_running->priority;

                #if NUFR_CS_OPTIMIZATION_INLINES == 1
                    NUFRKERNEL_ADD_TASK_TO_READY_LIST(owner_tcb);
                    UNUSED(macro_do_switch);              // suppress warning
                #else
                    nufrkernel_add_task_to_ready_list(owner_tcb);
                #endif
                }
            }
        }

        NUFR_INVOKE_CONTEXT_SWITCH();
    }
    else if (!block_on_sema)
    {
        // We now own the sema (Note relevancy if sema
        //  initialized with count > 1).
        sema_block->owner_tcb = nufr_running;

        (sema_block->count)--;
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    // Task will block/resume here if 'block_on_sema' is 'true'


    //##### Step Two: Kill zombie timer
    if (!immediate_timeout)
    {
        saved_psr = NUFR_LOCK_INTERRUPTS();

        if (NUFR_IS_STATUS_SET(nufr_running, NUFR_TASK_TIMER_RUNNING))
        {
            nufrkernel_purge_from_timer_list(nufr_running);
        }

        NUFR_UNLOCK_INTERRUPTS(saved_psr);
    }

    //#####     Step Three: Calculate return value
    //###
    // A timeout of zero means that if the sema count is zero,
    //  then a timeout occurs immediately, no blocking.
    if (block_on_sema && immediate_timeout)
    {
        return_value = NUFR_SEMA_GET_TIMEOUT;
    }
    else if (block_on_sema)
    {
        // Interrupt locking not needed
        notifications = nufr_running->notifications;

    #if NUFR_CS_TASK_KILL == 1
        if (ANY_BITS_SET(notifications, NUFR_TASK_UNBLOCKED_BY_MSG_SEND))
        {
            return_value = NUFR_SEMA_GET_MSG_ABORT;
        }
        else if (ANY_BITS_SET(notifications, NUFR_TASK_TIMEOUT))
        {
            return_value = NUFR_SEMA_GET_TIMEOUT;
        }
        else
        {
            return_value = NUFR_SEMA_GET_OK_BLOCK;
        }
    #else
        if (ANY_BITS_SET(notifications, NUFR_TASK_TIMEOUT))
        {
            return_value = NUFR_SEMA_GET_TIMEOUT;
        }
        else
        {
            return_value = NUFR_SEMA_GET_OK_BLOCK;
        }
    #endif  //NUFR_CS_TASK_KILL
    }
    else
    {
        return_value = NUFR_SEMA_GET_OK_NO_BLOCK;
    }

    return return_value;
}

//!
//! @name      nufr_sema_release
//
//! @brief     Increment semaphore
//!
//! @details   Cannot be called from ISR or from Systick handler
//!
//! @param[in] 'sema'
//!
//! @return     'true' if another task was waiting on this sema
bool nufr_sema_release(nufr_sema_t sema)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_sr_reg_t           saved_psr;
    nufr_sema_block_t      *sema_block;
    nufr_tcb_t             *head_tcb;
    nufr_tcb_t             *old_head_tcb;
    bool                    called_from_bg;
    bool                    none_to_unblock;
    bool                    invoke = false;

    sema_block = NUFR_SEMA_ID_TO_BLOCK(sema);
    KERNEL_REQUIRE_API(NUFR_IS_SEMA_BLOCK(sema_block));

    called_from_bg = (nufr_running == (nufr_tcb_t *)nufr_bg_sp);

    saved_psr = NUFR_LOCK_INTERRUPTS();

    // The reason this API can't be called from tick handler or ISR:
    //   Running task gets written to here.
    if (!called_from_bg)
    {
        // If running task has its priority inverted, restore it.
        if (NUFR_IS_STATUS_SET(nufr_running, NUFR_TASK_INVERSION_PRIORITIZED))
        {
            nufr_running->statuses &=
                            BITWISE_NOT8(NUFR_TASK_INVERSION_PRIORITIZED);

            old_head_tcb = nufr_ready_list;

            // Remove from ready list, change priority, re-insert in ready list.
            // (Note: 'nufr_running' won't be changed until context switch.)
        #if NUFR_CS_OPTIMIZATION_INLINES == 1
            NUFRKERNEL_REMOVE_HEAD_TASK_FROM_READY_LIST();
        #else
            nufrkernel_remove_head_task_from_ready_list();
        #endif

            nufr_running->priority = nufr_running->priority_restore_inversion;

        #if NUFR_CS_OPTIMIZATION_INLINES == 1
            NUFRKERNEL_ADD_TASK_TO_READY_LIST(nufr_running);
            UNUSED(macro_do_switch);                      // suppress warning
        #else
            (void)nufrkernel_add_task_to_ready_list(nufr_running);
        #endif

            invoke = (nufr_ready_list != old_head_tcb);
        }

        nufr_running->sema_block = NULL;
    }

    head_tcb = sema_block->task_list_head;

    // If sema count is > 0, there can be no tasks waiting on a sema
    KERNEL_ENSURE_IL((sema_block->count > 0)? (NULL == head_tcb) : true);

    none_to_unblock = (NULL == head_tcb);
    if (none_to_unblock)
    {
        // No one owns this sema anymore
        sema_block->owner_tcb = NULL;

        (sema_block->count)++;
    }
    // Otherwise, unblock first task on wait list,
    //   keeping sema count the same.
    else
    {
    #if NUFR_CS_OPTIMIZATION_INLINES == 1
        NUFRKERNEL_SEMA_UNLINK_TASK(sema_block, head_tcb);
    #else
        nufrkernel_sema_unlink_task(sema_block, head_tcb);
    #endif

        // NOTE: This is done by API sema getT caller, not here
        //nufrkernel_purge_from_timer_list(head_tcb);

        // Change sema owner on sema and add refer to sema
        //   from tcb.
        sema_block->owner_tcb = head_tcb;
        head_tcb->sema_block = sema_block;

    #if NUFR_CS_OPTIMIZATION_INLINES == 1
        NUFRKERNEL_ADD_TASK_TO_READY_LIST(head_tcb);
        invoke |= macro_do_switch;
    #else
        invoke |= nufrkernel_add_task_to_ready_list(head_tcb);
    #endif
    }

    if (invoke)
    {
        NUFR_INVOKE_CONTEXT_SWITCH();
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    return !none_to_unblock;
}

#endif  //NUFR_CS_SEMAPHORE