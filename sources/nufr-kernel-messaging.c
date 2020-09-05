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

//! @file    nufr-kernel-messaging.c
//! @authors Bernie Woodland
//! @date    7Aug17

#include "nufr-global.h"

#if NUFR_CS_MESSAGING == 1

#include "nufr-kernel-base-messaging.h"
#include "nufr-kernel-message-blocks.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-kernel-task.h"
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    #include "nufr-kernel-task-inlines.h"
    #include "nufr-kernel-semaphore-inlines.h"
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
#include "nufr-kernel-timer.h"
#include "nufr-kernel-semaphore.h"

#include "raging-contract.h"

#ifdef USING_SECONDARY_CONTEXT_SWITCH_FILE
    #include "secondary-context-switch.h"
#endif


// API calls

//!
//! @name      nufr_msg_drain
//!
//! @brief     Removes select messages in a task's inbox, returning
//! @brief     messages to pool. Messages to be drained are
//! @brief     from a certain level on down.
//!
//! @param[in] 'task_id'-- use 'NUFR_TID_null' for running task
//! @param[in] 'from_this_priority'-- drain this priority and lower only
//!
void nufr_msg_drain(nufr_tid_t task_id, nufr_msg_pri_t from_this_priority)
{
    nufr_tcb_t             *target_tcb;
    nufr_sr_reg_t           saved_psr;
    nufr_msg_t             *local_head_msg;
    nufr_msg_t             *local_tail_msg;
    nufr_msg_t             *this_msg;

    target_tcb = NUFR_TID_TO_TCB(task_id);

    // Can't do for BG task
    KERNEL_REQUIRE_API(NUFR_IS_TCB(target_tcb));
    // Range check 'from_this_priority'
    KERNEL_REQUIRE_API((unsigned)from_this_priority < NUFR_CS_MSG_PRIORITIES);
    KERNEL_INVARIANT((NUFR_CS_MSG_PRIORITIES > 0) && (NUFR_CS_MSG_PRIORITIES <= 4));

    local_head_msg = NULL;
    local_tail_msg = NULL;

    //######     Transfer messages to local list
    //##   Take the messages off the tcb's messages list and put them
    //##   on a local list, to be processed later. This local list
    //##   concatenates all the messages into one list.
    saved_psr = NUFR_LOCK_INTERRUPTS();

#if NUFR_CS_MSG_PRIORITIES == 4
    if ((from_this_priority <= 3) && (NULL != target_tcb->msg_head3))
    {
        local_head_msg = target_tcb->msg_head3;
        local_tail_msg = target_tcb->msg_tail3;
        target_tcb->msg_head3 = NULL;
        target_tcb->msg_tail3 = NULL;
    }
#endif

#if NUFR_CS_MSG_PRIORITIES >= 3
    if ((from_this_priority <= 2) && (NULL != target_tcb->msg_head2))
    {
        if (NULL == local_head_msg)
        {
            local_head_msg = target_tcb->msg_head2;
        }
        else
        {
            local_tail_msg->flink = target_tcb->msg_head2;
        }
        local_tail_msg = target_tcb->msg_tail2;
        target_tcb->msg_head2 = NULL;
        target_tcb->msg_tail2 = NULL;
    }
#endif

#if NUFR_CS_MSG_PRIORITIES >= 2
    if ((from_this_priority <= 1) && (NULL != target_tcb->msg_head1))
    {
        if (NULL == local_head_msg)
        {
            local_head_msg = target_tcb->msg_head1;
        }
        else
        {
            local_tail_msg->flink = target_tcb->msg_head1;
        }
        local_tail_msg = target_tcb->msg_tail1;
        target_tcb->msg_head1 = NULL;
        target_tcb->msg_tail1 = NULL;
    }
#endif

    if ((0 == from_this_priority) && (NULL != target_tcb->msg_head0))
    {
        if (NULL == local_head_msg)
        {
            local_head_msg = target_tcb->msg_head0;
        }
        else
        {
            local_tail_msg->flink = target_tcb->msg_head0;
        }
        local_tail_msg = target_tcb->msg_tail0;
        target_tcb->msg_head0 = NULL;
        target_tcb->msg_tail0 = NULL;
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    KERNEL_ENSURE(local_tail_msg != NULL?
                  local_tail_msg->flink == NULL :
                  true);

    //######     Return messages on local list to pool
    //##   

    this_msg = local_head_msg;

    while (NULL != this_msg)
    {
        nufr_msg_t *next_msg = this_msg->flink;
        // Detached messages always have flink nulled. We must tool.
        this_msg->flink = NULL;

        nufr_msg_free_block(this_msg);

        this_msg = next_msg;
    }
}


//!
//! @name      nufr_msg_purge
//!
//! @brief     Removes select messages in calling task's inbox, returning
//! @brief     messages to pool.
//!
//! @details   Messages to be removed are the ones which having
//! @details   matching message prefixes and ID's, and at the specified
//! @details   message priority--only.
//!
//! @details   NOTE: Potential collision of callinig calling 'nufr_msg_drain()'
//! @details         from another higher-priority task and calling this api.
//! @details         OK to do with a task kill, that's about all.
//!
//! @param[in] 'msg_fields'-- Message fields packed by macro. Needs
//! @param[in]       to have msg prefix, msg id, and msg priority fields set.
//! @param[in] 'do_all'-- if 'false', purge first match and abort;
//! @param[in]       otherwise, purge every occurent
//!
//! @return    Number of messages purged
//!
unsigned nufr_msg_purge(uint32_t msg_fields, bool do_all)
{
    nufr_sr_reg_t           saved_psr;
    unsigned                num_purges;
    unsigned                msg_priority;
    uint32_t                this_fields;
    uint32_t                prefix_id_pair;
    bool                    matching_msg;
    nufr_msg_t            **head_ptr;
    nufr_msg_t            **tail_ptr;
    nufr_msg_t             *previous_msg;
    nufr_msg_t             *this_msg;
    nufr_msg_t             *next_msg;

    // Can't be called from BG task
    if (nufr_running == (nufr_tcb_t *)nufr_bg_sp)
    {
        KERNEL_REQUIRE_API(false);
        return 0;
    }

    // Extract msg prefix and ID that caller packed
    prefix_id_pair = NUFR_GET_MSG_PREFIX_ID_PAIR(msg_fields);

    // Sanity check message priority
    msg_priority = NUFR_GET_MSG_PRIORITY(msg_fields);
    if (msg_priority >= NUFR_CS_MSG_PRIORITIES)
    {
        KERNEL_REQUIRE_API(false);
        return 0;
    }

    // Get message queue for this priority
    head_ptr = &(&nufr_running->msg_head0)[msg_priority];
    tail_ptr = &(&nufr_running->msg_tail0)[msg_priority];

    // Initialize to head
    num_purges = 0;
    previous_msg = NULL;
    // Lock against a higher priority task or ISR
    // appending a message
    saved_psr = NUFR_LOCK_INTERRUPTS();
    this_msg = *head_ptr;
    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    // Walk messages to end.
    // Assume if 1 or more messages are on queue, then head isn't changed
    // by other tasks or IRQs, except by a task kill.
    // Otherwise, we'd have to lock interrupts across the entire walk,
    // and that doesn't scale.

    while (NULL != this_msg)
    {
        // Isolate msg prefix and ID
        this_fields = this_msg->fields;
        matching_msg = NUFR_GET_MSG_PREFIX_ID_PAIR(this_fields)
                                  ==
                           prefix_id_pair;

        // Does 'this_msg' need to be purged?
        if (matching_msg)
        {
            // Lock against a higher priority task or ISR
            // sending a message
            saved_psr = NUFR_LOCK_INTERRUPTS();

            //
            // Break off 'this_msg' from linked list and stitch
            // other previous and next messages together.
            //

            // Deferred updating 'next_msg' until the locks were
            // applied, so we don't catch an ISR, etc. while appending
            // to queue.
            next_msg = this_msg->flink;

            // Is 'this_msg' at head in queue?
            if (NULL == previous_msg)
            {
                *head_ptr = next_msg;
            }
            else
            {
                previous_msg->flink = next_msg;
            }

            // Is 'this_msg' last message in queue?
            if (NULL == next_msg)
            {
                *tail_ptr = previous_msg;
            }

            NUFR_UNLOCK_INTERRUPTS(saved_psr);

            // Cap purged msg/'this_msg' off properly and return to pool
            this_msg->flink = NULL;
            nufr_msg_free_block(this_msg);

            num_purges++;

            if (!do_all)
            {
                KERNEL_ENSURE(1 == num_purges);

                return num_purges;
            }

            // Update 'this_msg' for next pass in loop
            // 'previous_msg' doesn't change/won't advance
            // We're using 'next_msg', which was frozen when
            // locks were on.
            this_msg = next_msg;
        }
        // else, 'this_msg' was not a matching message
        else
        {
            // Update 'previous_msg', 'this_msg' for next pass in loop
            previous_msg = this_msg;
            // Lock against a higher priority task or ISR
            // appending a message
            saved_psr = NUFR_LOCK_INTERRUPTS();
            this_msg = this_msg->flink;
            NUFR_UNLOCK_INTERRUPTS(saved_psr);
        }
    }

    return num_purges;
}

//!
//! @name      nufr_msg_send
//!
//! @brief     Main kernel message sending API
//!
//! @details   Steps in calling environment:
//! @details     (1) nufr_msg_get_block() called to fetch a message
//! @details         block to be passed into nufr_msg_send_by_block().
//! @details     (2) SENDING-TASK value for 'dest_tcb' determined
//! @details         nufrplat_msg_prefix_id_to_tid() to determine this.
//! @details     (3) Priority, message prefixes and IDs chosen.
//! @details         msg->fields packed using NUFR_SET_MSG_FIELDS().
//! @details     (4) Sanity checks on parameters applied.
//!
//! @param[in] 'msg_fields'-- Message fields packed by macro
//! @param[in]                NUFR_SET_MSG_FIELDS() or equivalent
//! @param[in]  Message fields that must be filled in:       
//! @param[in]        PREFIX (svc_msg_prefix_t)
//! @param[in]        ID (nufr_msg_id_t )
//! @param[in]        SENDING_TASK (nufr_tid_t)
//! @param[in]        PRIORITY (nufr_msg_pri_t) 
//! @param[in]         
//! @param[in]  'optional_parameter' message parameter attached to message
//! @param[in]         
//! @param[in]  'dest_task_id'--task that will receive message
//
//! @return      Action applied to receiving task
//!
nufr_msg_send_rtn_t nufr_msg_send(uint32_t   msg_fields,
                                  uint32_t   optional_parameter,
                                  nufr_tid_t dest_task_id)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    unsigned                send_priority;
    nufr_sr_reg_t           saved_psr;
    unsigned                block_flags;
    bool                    send_occured;
    bool                    is_queue_empty;
    bool                    is_awakeable;
#if NUFR_CS_TASK_KILL == 1
    bool                    is_abort_level_met;
    bool                    is_abortable_api;
    bool                    will_abort = false;
#endif  //NUFR_CS_TASK_KILL
    bool                    invoke = false;
    nufr_msg_t             *msg;
    nufr_tcb_t             *dest_tcb;
    nufr_msg_t            **head_ptr;
    nufr_msg_t            **tail_ptr;
    nufr_msg_send_rtn_t     return_value;

    KERNEL_REQUIRE_API(NUFR_GET_MSG_SENDING_TASK(msg_fields) < NUFR_TID_max);

    dest_tcb = NUFR_TID_TO_TCB(dest_task_id);
    send_priority = NUFR_GET_MSG_PRIORITY(msg_fields);
    if ((send_priority >= NUFR_CS_MSG_PRIORITIES) || !NUFR_IS_TCB(dest_tcb))
    {
        KERNEL_REQUIRE_API(false);
        return NUFR_MSG_SEND_ERROR;
    }

    head_ptr = &(&dest_tcb->msg_head0)[send_priority];
    tail_ptr = &(&dest_tcb->msg_tail0)[send_priority];

    saved_psr = NUFR_LOCK_INTERRUPTS();

    block_flags = dest_tcb->block_flags;

    // Sanity check: dest task must be active
    send_occured = ARE_BITS_CLR(block_flags, NUFR_TASK_NOT_LAUNCHED);
    if (send_occured)
    {
        // Grab next block from bpool head, update links
        msg = nufr_msg_free_head;
        if (NULL != msg)
        {
            nufr_msg_free_head = msg->flink;
            // Did this alloc deplete bpool?
            if (NULL == msg->flink)
            {
                nufr_msg_free_tail = NULL;
                nufr_msg_pool_empty_count++;
                KERNEL_ENSURE_IL(false);
            }

            // Assign all this block's struct fields
            msg->flink = NULL;
            msg->fields = msg_fields;
            msg->parameter = optional_parameter;

            is_queue_empty = (NULL == *head_ptr);

            // Empty queue?
            if (is_queue_empty)
            {
                *head_ptr = msg;
            }
            else
            {
                (*tail_ptr)->flink = msg;
            }

            // Finish stitching links
            *tail_ptr = msg;

            // Is task blocked in such a way that a msg send could
            //   possibly make it ready? :
            //      nufr_msg_getW(), nufr_msg_getT(),
            //      nufr_bop_waitW(), nufr_sema_waitW(),
            //      nufr_bop_waitT(), nufr_sema_waitT(),
            //      nufr_sleep()
        #if NUFR_CS_TASK_KILL == 1
            is_awakeable = ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_MSG |
                                                     NUFR_TASK_BLOCKED_ASLEEP |
                                                     NUFR_TASK_BLOCKED_BOP |
                                                     NUFR_TASK_BLOCKED_SEMA);
        #else
            is_awakeable = ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_MSG);
        #endif  //NUFR_CS_TASK_KILL
            if (is_awakeable)
            {
            #if NUFR_CS_TASK_KILL == 1
                // Does this API support abort feature?
                is_abortable_api = ANY_BITS_SET(block_flags,
                                          NUFR_TASK_BLOCKED_ASLEEP |
                                          NUFR_TASK_BLOCKED_BOP |
                                          NUFR_TASK_BLOCKED_SEMA);

                // Send message's priority passes abort level check?
                is_abort_level_met = send_priority < dest_tcb->abort_message_priority;

                // All conditions met for an abort?
                will_abort = is_abort_level_met && is_abortable_api;

                // Wake up task if:
                //  - Task is blocked on msg get
                //  - Task is blocked on a bop or a sema and 
                //    an abort to a high priority msg condition met
                if (will_abort || !is_abortable_api)
            #else
                //if (true)
            #endif  //NUFR_CS_TASK_KILL
                {
                #if NUFR_CS_TASK_KILL == 1
                    if (will_abort)
                    {
                        KERNEL_INVARIANT_IL(dest_tcb->notifications == 0);
                        dest_tcb->notifications = NUFR_TASK_UNBLOCKED_BY_MSG_SEND;

                        if (ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_SEMA))
                        {
                        #if NUFR_CS_OPTIMIZATION_INLINES == 1
                            NUFRKERNEL_SEMA_UNLINK_TASK(dest_tcb->sema_block, dest_tcb);
                        #else
                            nufrkernel_sema_unlink_task(dest_tcb->sema_block, dest_tcb);
                        #endif
                            dest_tcb->sema_block = NULL;
                        }
                    }
                #endif  //NUFR_CS_TASK_KILL

                    // NOTE: this code commented out because this API can be
                    //       called from an ISR, and timer lists can't be
                    //       manipulated from ISRs. It would be ok from task
                    //       level, however.
                    //       Let timer get cleared when API exits
                    //
                    // If API call was with timeout
                    //   (nufr_bop_waitT(), nufr_sema_waitT(), nufr_asleep()),
                    //   then must kill OS timer before making task ready.
                    //if (NUFR_IS_STATUS_SET(dest_tcb, NUFR_TASK_TIMER_RUNNING))
                    //{
                    //    nufrkernel_purge_from_timer_list(dest_tcb);
                    //}

                #if NUFR_CS_LOCAL_STRUCT == 1
                    // If task is locked due to a bop lock, don't let message send
                    //   due to an abort message put it on ready list. However,
                    //   bop block status needs to be cleared so the bop unlock
                    //   will know to unblock it, so abort message will awaken
                    //   task from bop wait.
                    if (NUFR_IS_STATUS_CLR(dest_tcb, NUFR_TASK_BOP_LOCKED))
                    {
                #endif
                        // Set 'block_flags' to ready state
                        dest_tcb->block_flags = 0;

                    #if NUFR_CS_OPTIMIZATION_INLINES == 1
                        NUFRKERNEL_ADD_TASK_TO_READY_LIST(dest_tcb);
                        invoke = macro_do_switch;
                    #else
                        invoke = nufrkernel_add_task_to_ready_list(dest_tcb);
                    #endif
                        if (invoke)
                        {
                            NUFR_INVOKE_CONTEXT_SWITCH();
                        }

                #if NUFR_CS_LOCAL_STRUCT == 1
                    }
                    else
                    {
                        dest_tcb->block_flags &= BITWISE_NOT8(NUFR_TASK_BLOCKED_BOP);
                    }
                #endif
                }
            }
        }
        // else, no message block was available
        else
        {
            send_occured = false;
            KERNEL_ENSURE(false);
        }
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

#if NUFR_CS_TASK_KILL == 1
    if (will_abort)
        return_value = NUFR_MSG_SEND_ABORTED_RECEIVER;
    else if (invoke)
#else
    if (invoke)
#endif  //NUFR_CS_TASK_KILL
        return_value = NUFR_MSG_SEND_AWOKE_RECEIVER;
    else if (!send_occured)
        return_value = NUFR_MSG_SEND_ERROR;
    else
        return_value = NUFR_MSG_SEND_OK;

    return return_value;
}

//!
//! @name      nufr_msg_send_by_block
//!
//! @brief     Secondary kernel message sending API
//!
//! @details   A use case is a send to multiple destination SL API
//! @details   
//! @details   Steps in calling environment:
//! @details     (1) nufr_msg_get_block() called to fetch a message
//! @details         block to be passed into nufr_msg_send_by_block().
//! @details     (2) SENDING-TASK value for 'dest_tcb' determined
//! @details         nufrplat_msg_prefix_id_to_tid() to determine this.
//! @details     (3) Priority, message prefixes and IDs chosen.
//! @details         msg->fields packed using NUFR_SET_MSG_FIELDS().
//! @details     (4) Sanity checks on parameters applied.
//!
//! @param[in] 'msg'-- Pointer to message block to be sent.
//! @param[in]         Must have been allocated from message pool.
//! @param[in]         If there was no error on send, message receiver
//! @param[in]         owns block, and therefore must free it.
//! @param[in]  Message fields that must be filled in:       
//! @param[in]        PREFIX (svc_msg_prefix_t)
//! @param[in]        ID (nufr_msg_id_t )
//! @param[in]        PRIORITY (nufr_msg_pri_t) 
//! @param[in]         
//! @param[in]        PREFIX, ID, PRIORITY are bit fields in 'msg->fields'
//! @param[in]        Recommend using macro NUFR_SET_MSG_FIELDS() or equivalent
//! @param[in]         
//! @param[in]  'msg->parameter' is optional
//! @param[in]         
//! @param[in]  'dest_task_id'--task that will receive message
//
//! @return      Action applied to receiving task
//!
nufr_msg_send_rtn_t nufr_msg_send_by_block(nufr_msg_t *msg, nufr_tid_t dest_task_id)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    uint32_t                fields;
    unsigned                send_priority;
    nufr_sr_reg_t           saved_psr;
    unsigned                block_flags;
    bool                    send_occured;
    bool                    is_queue_empty;
    bool                    is_awakeable;
#if NUFR_CS_TASK_KILL == 1
    bool                    is_abort_level_met;
    bool                    is_abortable_api;
    bool                    will_abort = false;
#endif  //NUFR_CS_TASK_KILL
    bool                    invoke = false;
    nufr_tcb_t             *dest_tcb;
    nufr_msg_t            **head_ptr;
    nufr_msg_t            **tail_ptr;
    nufr_msg_send_rtn_t     return_value;

    KERNEL_REQUIRE_API(msg != NULL);
    KERNEL_REQUIRE_API(msg->flink == NULL);

    fields = msg->fields;
    KERNEL_REQUIRE_API(NUFR_GET_MSG_SENDING_TASK(fields) < NUFR_TID_max);

    dest_tcb = NUFR_TID_TO_TCB(dest_task_id);
    send_priority = NUFR_GET_MSG_PRIORITY(fields);
    if ((send_priority >= NUFR_CS_MSG_PRIORITIES) || !NUFR_IS_TCB(dest_tcb))
    {
        KERNEL_REQUIRE_API(false);
        return NUFR_MSG_SEND_ERROR;
    }

    head_ptr = &(&dest_tcb->msg_head0)[send_priority];
    tail_ptr = &(&dest_tcb->msg_tail0)[send_priority];

    saved_psr = NUFR_LOCK_INTERRUPTS();

    block_flags = dest_tcb->block_flags;

    // Sanity check: dest task must be active
    send_occured = ARE_BITS_CLR(block_flags, NUFR_TASK_NOT_LAUNCHED);
    if (send_occured)
    {
        is_queue_empty = (NULL == *head_ptr);

        // Empty queue?
        if (is_queue_empty)
        {
            *head_ptr = msg;
        }
        else
        {
            (*tail_ptr)->flink = msg;
        }

        // Finish stitching links
        *tail_ptr = msg;

        // Is task blocked in such a way that a msg send could
        //   possibly make it ready? :
        //      nufr_msg_getW(), nufr_msg_getT(),
        //      nufr_bop_waitW(), nufr_sema_waitW(),
        //      nufr_bop_waitT(), nufr_sema_waitT(),
        //      nufr_sleep()
    #if NUFR_CS_TASK_KILL == 1
        is_awakeable = ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_MSG |
                                                 NUFR_TASK_BLOCKED_ASLEEP |
                                                 NUFR_TASK_BLOCKED_BOP |
                                                 NUFR_TASK_BLOCKED_SEMA);
    #else
        is_awakeable = ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_MSG);
    #endif  //NUFR_CS_TASK_KILL
        if (is_awakeable)
        {
        #if NUFR_CS_TASK_KILL == 1
            // Does this API support abort feature?
            is_abortable_api = ANY_BITS_SET(block_flags,
                                      NUFR_TASK_BLOCKED_ASLEEP |
                                      NUFR_TASK_BLOCKED_BOP |
                                      NUFR_TASK_BLOCKED_SEMA);

            // Send message's priority passes abort level check?
            is_abort_level_met = send_priority < dest_tcb->abort_message_priority;

            // All conditions met for an abort?
            will_abort = is_abort_level_met && is_abortable_api;

            // Wake up task if:
            //  - Task is blocked on msg get
            //  - Task is blocked on a bop or a sema and 
            //    an abort to a high priority msg condition met
            if (will_abort || !is_abortable_api)
        #else
            //if (true)
        #endif  //NUFR_CS_TASK_KILL
            {
            #if NUFR_CS_TASK_KILL == 1
                if (will_abort)
                {
                    KERNEL_INVARIANT_IL(dest_tcb->notifications == 0);
                    dest_tcb->notifications = NUFR_TASK_UNBLOCKED_BY_MSG_SEND;

                    if (ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_SEMA))
                    {
                    #if NUFR_CS_OPTIMIZATION_INLINES == 1
                        NUFRKERNEL_SEMA_UNLINK_TASK(dest_tcb->sema_block, dest_tcb);
                    #else
                        nufrkernel_sema_unlink_task(dest_tcb->sema_block, dest_tcb);
                    #endif
                        dest_tcb->sema_block = NULL;
                    }
                }
            #endif  //NUFR_CS_TASK_KILL

                // NOTE: this code commented out because this API can be
                //       called from an ISR, and timer lists can't be
                //       manipulated from ISRs. It would be ok from task
                //       level, however.
                //       Let timer get cleared when API exits
                //
                // If API call was with timeout
                //   (nufr_bop_waitT(), nufr_sema_waitT(), nufr_asleep()),
                //   then must kill OS timer before making task ready.
                //if (NUFR_IS_STATUS_SET(dest_tcb, NUFR_TASK_TIMER_RUNNING))
                //{
                //    nufrkernel_purge_from_timer_list(dest_tcb);
                //}

            #if NUFR_CS_LOCAL_STRUCT == 1
                // If task is locked due to a bop lock, don't let message send
                //   due to an abort message put it on ready list. However,
                //   bop block status needs to be cleared so the bop unlock
                //   will know to unblock it, so abort message will awaken
                //   task from bop wait.
                if (NUFR_IS_STATUS_CLR(dest_tcb, NUFR_TASK_BOP_LOCKED))
                {
            #endif
                    // Set 'block_flags' to ready state
                    dest_tcb->block_flags = 0;

                #if NUFR_CS_OPTIMIZATION_INLINES == 1
                    NUFRKERNEL_ADD_TASK_TO_READY_LIST(dest_tcb);
                    invoke = macro_do_switch;
                #else
                    invoke = nufrkernel_add_task_to_ready_list(dest_tcb);
                #endif
                    if (invoke)
                    {
                        NUFR_INVOKE_CONTEXT_SWITCH();
                    }

            #if NUFR_CS_LOCAL_STRUCT == 1
                }
                else
                {
                    dest_tcb->block_flags &= BITWISE_NOT8(NUFR_TASK_BLOCKED_BOP);
                }
            #endif
            }
        }
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

#if NUFR_CS_TASK_KILL == 1
    if (will_abort)
        return_value = NUFR_MSG_SEND_ABORTED_RECEIVER;
    else if (invoke)
#else
    if (invoke)
#endif  //NUFR_CS_TASK_KILL
        return_value = NUFR_MSG_SEND_AWOKE_RECEIVER;
    else if (!send_occured)
        return_value = NUFR_MSG_SEND_ERROR;
    else
        return_value = NUFR_MSG_SEND_OK;

    return return_value;
}

//!
//! @name      nufr_msg_getW
//!
//! @brief     Get a message, block indefinitely until you get one
//!
//! @details   Cannot be called from an ISR or from BG task
//! @details   Always returns having obtained a message: no other case.
//! @details
//!
//! @param[out] 'msg_fields_ptr'-- caller variable to put msg->fields
//! @param[out]                for this msg.
//! @param[out] 'parameter_ptr'-- caller variable to put msg->parameter
//! @param[out]           If NULL, ignore
//!
void nufr_msg_getW(uint32_t *msg_fields_ptr, uint32_t *parameter_ptr)
{
    nufr_sr_reg_t           saved_psr;
    unsigned                pri_index;
    const unsigned          MSG_NOT_FOUND = 128;  // value is arbitrary
    nufr_msg_t            **head_ptr;
    nufr_msg_t            **tail_ptr;
    nufr_msg_t             *msg;

    KERNEL_REQUIRE_API(NUFR_IS_TCB(nufr_running));
    KERNEL_REQUIRE_API(NULL != msg_fields_ptr);

    //###### First: Check if a message is already available,
    //####          If there is one, get that message and be done with it.
    //###
    pri_index = MSG_NOT_FOUND;
    msg = NULL;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    if (NULL != nufr_running->msg_head0)
    {
        pri_index = 0;
    }
#if NUFR_CS_MSG_PRIORITIES > 1
    else if (NULL != nufr_running->msg_head1)
    {
        pri_index = 1;
    }
#endif
#if NUFR_CS_MSG_PRIORITIES > 2
    else if (NULL != nufr_running->msg_head2)
    {
        pri_index = 2;
    }
#endif
#if NUFR_CS_MSG_PRIORITIES > 3
    else if (NULL != nufr_running->msg_head3)
    {
        pri_index = 3;
    }
#endif

    if (MSG_NOT_FOUND == pri_index)
    {
        //###### Second: Block on receiving a message
        //###
        nufr_running->notifications = 0;

    #if NUFR_CS_OPTIMIZATION_INLINES == 1
        NUFRKERNEL_BLOCK_RUNNING_TASK(NUFR_TASK_BLOCKED_MSG);
    #else
        nufrkernel_block_running_task(NUFR_TASK_BLOCKED_MSG);
    #endif

        NUFR_INVOKE_CONTEXT_SWITCH();
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    // If no msg taken, task has gone to sleep and was awakened
    //   by msg rx.

    //##### Third: If message was dequeued without task having
    //##            waited for it, grab it and leave.
    if (MSG_NOT_FOUND != pri_index)
    {
        saved_psr = NUFR_LOCK_INTERRUPTS();

        head_ptr = &(&nufr_running->msg_head0)[pri_index];
        tail_ptr = &(&nufr_running->msg_tail0)[pri_index];

        // Pop head, stitch links back together.
        msg = *head_ptr;
        KERNEL_ENSURE(msg != NULL);
        *head_ptr = (*head_ptr)->flink;
        if (msg == *tail_ptr)
        {
            *tail_ptr = NULL;
        }
        // Must always return to block pool a block with a NULL 'msg->flink'
        msg->flink = NULL;

        // Copy message block member values over to fcn. parameters
        *msg_fields_ptr = msg->fields;
        if (NULL != parameter_ptr)
        {
            *parameter_ptr = msg->parameter;
        }

        // Free message block
        // Is message block pool depleted?
        if (NULL == nufr_msg_free_tail)
        {
            nufr_msg_free_head = msg;
            nufr_msg_free_tail = msg;
        }
        // Probable path: Message block pool not depleted
        else
        {
            nufr_msg_free_tail->flink = msg;
            nufr_msg_free_tail = msg;
        }

        NUFR_UNLOCK_INTERRUPTS(saved_psr);

        return;
    }

    //###### Fourth: Get message.
    //####           There must be one this time.
    //###
    KERNEL_ENSURE(pri_index == MSG_NOT_FOUND);
    KERNEL_ENSURE(msg == NULL);

    saved_psr = NUFR_LOCK_INTERRUPTS();

    if (NULL != nufr_running->msg_head0)
    {
        pri_index = 0;
    }
#if NUFR_CS_MSG_PRIORITIES > 1
    else if (NULL != nufr_running->msg_head1)
    {
        pri_index = 1;
    }
#endif
#if NUFR_CS_MSG_PRIORITIES > 2
    else if (NULL != nufr_running->msg_head2)
    {
        pri_index = 2;
    }
#endif
#if NUFR_CS_MSG_PRIORITIES > 3
    else if (NULL != nufr_running->msg_head3)
    {
        pri_index = 3;
    }
#endif
    else
    {
        pri_index = MSG_NOT_FOUND;
    }

#if NUFR_CS_TASK_KILL == 0
    KERNEL_ENSURE_IL(pri_index != MSG_NOT_FOUND);
#else
    // Corner case: a task kill could've jumped in and drained message queue
#endif

    if (MSG_NOT_FOUND != pri_index)
    {
        head_ptr = &(&nufr_running->msg_head0)[pri_index];
        tail_ptr = &(&nufr_running->msg_tail0)[pri_index];

        // Pop head, stitch links back together.
        msg = *head_ptr;
        KERNEL_ENSURE(msg != NULL);
        *head_ptr = (*head_ptr)->flink;
        if (msg == *tail_ptr)
        {
            *tail_ptr = NULL;
        }
        // Must always return to block pool a block with a NULL 'msg->flink'
        msg->flink = NULL;

        // Copy message block member values over to fcn. parameters
        *msg_fields_ptr = msg->fields;
        if (NULL != parameter_ptr)
        {
            *parameter_ptr = msg->parameter;
        }

        // Free message block
        // Is message block pool depleted?
        if (NULL == nufr_msg_free_tail)
        {
            nufr_msg_free_head = msg;
            nufr_msg_free_tail = msg;
        }
        // Probable path: Message block pool not depleted
        else
        {
            nufr_msg_free_tail->flink = msg;
            nufr_msg_free_tail = msg;
        }
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

}

//!
//! @name      nufr_msg_getT
//!
//! @brief     Get a message, block until 'timeout_ticks'
//!
//! @details   Cannot be called from an ISR or from BG task
//! @details
//! @details   Same as 'nufr_msg_getW()', except with timeout
//! @details   
//!
//! @params[in] timeout_ticks-- timeout. If == 0, return from API call
//! @params[in]       immediately if no msg waiting.
//! @param[out] 'msg_fields_ptr'-- caller variable to put msg->fields
//! @param[out]                for this msg.
//! @param[out] 'parameter_ptr'-- caller variable to put msg->parameter
//! @param[out]           If NULL, ignore
//!
//! @return     'true' if timeout occured
//!
bool nufr_msg_getT(unsigned  timeout_ticks,
                   uint32_t *msg_fields_ptr,
                   uint32_t *parameter_ptr)
{
    nufr_sr_reg_t           saved_psr;
    unsigned                pri_index;
    const unsigned          MSG_NOT_FOUND = 128;  // value is arbitrary
    nufr_msg_t            **head_ptr;
    nufr_msg_t            **tail_ptr;
    nufr_msg_t             *msg;
    bool                    immediate_timeout;

    KERNEL_REQUIRE_API(NUFR_IS_TCB(nufr_running));
    KERNEL_REQUIRE_API(NULL != msg_fields_ptr);

    immediate_timeout = timeout_ticks == 0;

    //###### First: Check if a message is already available,
    //####          If there is one, get that message and be done with it.
    //###
    pri_index = MSG_NOT_FOUND;
    msg = NULL;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    if (NULL != nufr_running->msg_head0)
    {
        pri_index = 0;
    }
#if NUFR_CS_MSG_PRIORITIES > 1
    else if (NULL != nufr_running->msg_head1)
    {
        pri_index = 1;
    }
#endif
#if NUFR_CS_MSG_PRIORITIES > 2
    else if (NULL != nufr_running->msg_head2)
    {
        pri_index = 2;
    }
#endif
#if NUFR_CS_MSG_PRIORITIES > 3
    else if (NULL != nufr_running->msg_head3)
    {
        pri_index = 3;
    }
#endif

    if (!immediate_timeout && (MSG_NOT_FOUND == pri_index))
    {
        //###### Second: Block on receiving a message
        //###
        nufr_running->notifications = 0;

        nufrkernel_add_to_timer_list(nufr_running, timeout_ticks);

    #if NUFR_CS_OPTIMIZATION_INLINES == 1
        NUFRKERNEL_BLOCK_RUNNING_TASK(NUFR_TASK_BLOCKED_MSG);
    #else
        nufrkernel_block_running_task(NUFR_TASK_BLOCKED_MSG);
    #endif

        NUFR_INVOKE_CONTEXT_SWITCH();
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    // If no msg taken, task has gone to sleep and was awakened
    //   by msg rx.


    //##### Third: Kill zombie timer
    saved_psr = NUFR_LOCK_INTERRUPTS();

    if (NUFR_IS_STATUS_SET(nufr_running, NUFR_TASK_TIMER_RUNNING))
    {
        nufrkernel_purge_from_timer_list(nufr_running);
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);


    //##### Fourth: If message was dequeued without task having
    //##            waited for it, grab it and leave.
    if (MSG_NOT_FOUND != pri_index)
    {
        saved_psr = NUFR_LOCK_INTERRUPTS();

        head_ptr = &(&nufr_running->msg_head0)[pri_index];
        tail_ptr = &(&nufr_running->msg_tail0)[pri_index];

        // Pop head, stitch links back together.
        msg = *head_ptr;
        KERNEL_ENSURE(msg != NULL);
        *head_ptr = (*head_ptr)->flink;
        if (msg == *tail_ptr)
        {
            *tail_ptr = NULL;
        }
        // Must always return to block pool a block with a NULL 'msg->flink'
        msg->flink = NULL;

        // Copy message block member values over to fcn. parameters
        *msg_fields_ptr = msg->fields;
        if (NULL != parameter_ptr)
        {
            *parameter_ptr = msg->parameter;
        }

        // Free message block
        // Is message block pool depleted?
        if (NULL == nufr_msg_free_tail)
        {
            nufr_msg_free_head = msg;
            nufr_msg_free_tail = msg;
        }
        // Probable path: Message block pool not depleted
        else
        {
            nufr_msg_free_tail->flink = msg;
            nufr_msg_free_tail = msg;
        }

        NUFR_UNLOCK_INTERRUPTS(saved_psr);

        return false;
    }
    // Immediate timeout and no message in inbox?
    else if (immediate_timeout)
    {
        return true;
    }

    //###### Fifth: Get message.
    //####          There will either be a message or a timeout
    //###
    KERNEL_ENSURE(pri_index == MSG_NOT_FOUND);
    KERNEL_ENSURE(msg == NULL);

    saved_psr = NUFR_LOCK_INTERRUPTS();

    if (NULL != nufr_running->msg_head0)
    {
        pri_index = 0;
    }
#if NUFR_CS_MSG_PRIORITIES > 1
    else if (NULL != nufr_running->msg_head1)
    {
        pri_index = 1;
    }
#endif
#if NUFR_CS_MSG_PRIORITIES > 2
    else if (NULL != nufr_running->msg_head2)
    {
        pri_index = 2;
    }
#endif
#if NUFR_CS_MSG_PRIORITIES > 3
    else if (NULL != nufr_running->msg_head3)
    {
        pri_index = 3;
    }
#endif
    else
    {
        pri_index = MSG_NOT_FOUND;
    }

    if (MSG_NOT_FOUND != pri_index)
    {
        head_ptr = &(&nufr_running->msg_head0)[pri_index];
        tail_ptr = &(&nufr_running->msg_tail0)[pri_index];

        // Pop head, stitch links back together.
        msg = *head_ptr;
        KERNEL_ENSURE(msg != NULL);
        *head_ptr = (*head_ptr)->flink;
        if (msg == *tail_ptr)
        {
            *tail_ptr = NULL;
        }
        // Must always return to block pool a block with a NULL 'msg->flink'
        msg->flink = NULL;

        // Copy message block member values over to fcn. parameters
        *msg_fields_ptr = msg->fields;
        if (NULL != parameter_ptr)
        {
            *parameter_ptr = msg->parameter;
        }

        // Free message block
        // Is message block pool depleted?
        if (NULL == nufr_msg_free_tail)
        {
            nufr_msg_free_head = msg;
            nufr_msg_free_tail = msg;
        }
        // Probable path: Message block pool not depleted
        else
        {
            nufr_msg_free_tail->flink = msg;
            nufr_msg_free_tail = msg;
        }
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    return MSG_NOT_FOUND != pri_index;
}

//!
//! @name      nufr_msg_peek
//!
//! @brief     Returns the first message block without dequeing it
//!
//! @details   Caller might want to lock/unlock interrupts around this
//! @details   call, as another task or an ISR may change the msg queue
//! @details   at an inconvenient time.
//! @details   
//!
//! @return      Message at head of list. NULL if no message.
//!
nufr_msg_t *nufr_msg_peek(void)
{
    nufr_sr_reg_t           saved_psr;
    unsigned                pri_index;
    const unsigned          MSG_NOT_FOUND = 128;
    nufr_msg_t            **head_ptr;
    nufr_msg_t             *msg;

    pri_index = MSG_NOT_FOUND;
    msg = NULL;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    if (NULL != nufr_running->msg_head0)
    {
        pri_index = 0;
    }
#if NUFR_CS_MSG_PRIORITIES > 1
    else if (NULL != nufr_running->msg_head1)
    {
        pri_index = 1;
    }
#endif
#if NUFR_CS_MSG_PRIORITIES > 2
    else if (NULL != nufr_running->msg_head2)
    {
        pri_index = 2;
    }
#endif
#if NUFR_CS_MSG_PRIORITIES > 3
    else if (NULL != nufr_running->msg_head3)
    {
        pri_index = 3;
    }
#endif

    if (MSG_NOT_FOUND != pri_index)
    {
        head_ptr = &(&nufr_running->msg_head0)[pri_index];

        msg = *head_ptr;
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    return msg;
}

#endif  //NUFR_CS_MESSAGING