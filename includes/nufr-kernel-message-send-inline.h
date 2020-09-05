/*
Copyright (c) 2019, Bernie Woodland
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

//! @file     nufr-kernel-message-send-inline.h
//! @authors  Bernie Woodland
//! @date     4Dec19
//!
//! @brief   Macro to be used for faster message sends
//!
//! @details This will speed up message sends by up to 2:1, maybe
//! @details Intended to be used in IRQ's, but can be used elsewhere
//!

#ifndef NUFR_KERNEL_MESSAGE_SEND_INLINE_H_
#define NUFR_KERNEL_MESSAGE_SEND_INLINE_H_


#include "raging-global.h"
#include "nufr-global.h"
#include "nufr-kernel-base-messaging.h"
#include "nufr-kernel-message-blocks.h"
#include "nufr-platform-app.h"
#include "nufr-platform.h"
#include "nufr-api.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-task-inlines.h"
#include "raging-contract.h"

//!
//! @name      NUFR_MSG_SEND_INLINE
//!
//! @brief     Speed-opitimized inline message sending
//!
//! @details   Equivalent to nufr_msg_send()
//! @details   Intended to be used in IRQ's, but can be used anywhere
//! @details   
//! @details   No support for the following:
//! @details    1) Secondary context switches (NUFR_SECONDARY_CONTEXT_SWITCH())
//! @details        This macro won't work on MSP430.
//! @details    2) Abort messages (Task kill/'NUFR_CS_TASK_KILL', bop lock)
//! @details   
//! @details   To run faster, all input parameters should be constants or enums.
//! @details   
//!
//! @param[in] task_id-- type 'nufr_tid_t'
//! @param[in] msg_prefix-- whatever you like/'nsvc_msg_prefix_t' in some cases
//! @param[in] msg_id-- whatever you like/a user-defined enum
//! @param[in] msg_priority-- type 'nufr_msg_pri_t'
//! @param[in] fixed_parameter-- type 'uint32_t'. Optional message parameter.
//!
#define NUFR_MSG_SEND_INLINE(task_id, msg_prefix, msg_id, msg_priority, fixed_parameter) \
{                                                                              \
    nufr_sr_reg_t           saved_psr;                                         \
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;                            \
    nufr_msg_t             *msg_ptr;                                           \
    nufr_msg_t             *next_msg_ptr;                                      \
    nufr_tcb_t             *dest_tcb;                                          \
    nufr_msg_t            **head_ptr;                                          \
    nufr_msg_t            **tail_ptr;                                          \
    unsigned                block_flags;                                       \
    bool                    send_occured;                                      \
    bool                    is_queue_empty;                                    \
    bool                    is_awakeable;                                      \
                                                                               \
    dest_tcb = NUFR_TID_TO_TCB(task_id);                                       \
                                                                               \
    saved_psr = NUFR_LOCK_INTERRUPTS();                                        \
                                                                               \
    block_flags = dest_tcb->block_flags;                                       \
                                                                               \
    /* Sanity check: dest task must be active */                               \
    send_occured = ARE_BITS_CLR(block_flags, NUFR_TASK_NOT_LAUNCHED);          \
    KERNEL_REQUIRE_IL(send_occured);                                           \
    if (send_occured)                                                          \
    {                                                                          \
        KERNEL_REQUIRE_IL(NULL != nufr_msg_free_head);                         \
                                                                               \
        /* Check that a message block is available */                          \
        if (NULL != nufr_msg_free_head)                                        \
        {                                                                      \
            /* Allocate block from pool. */                                    \
            msg_ptr = nufr_msg_free_head;                                      \
            next_msg_ptr = msg_ptr->flink;                                     \
            nufr_msg_free_head = next_msg_ptr;                                 \
            if (NULL == next_msg_ptr)                                          \
            {                                                                  \
                nufr_msg_free_tail = NULL;                                     \
                nufr_msg_pool_empty_count++;                                   \
                KERNEL_REQUIRE_IL(false);                                      \
            }                                                                  \
                                                                               \
            /*** Populate msg_ptr block members */                             \
            msg_ptr->flink = NULL;                                             \
            msg_ptr->fields = NUFR_SET_MSG_FIELDS((msg_prefix), (msg_id), 0, (msg_priority)); \
            msg_ptr->parameter = (fixed_parameter);                            \
                                                                               \
            /*** Enqueue msg_ptr block onto TCB */                             \
                                                                               \
            /* Should be a compile-time check */                               \
            KERNEL_REQUIRE_IL(msg_priority < NUFR_CS_MSG_PRIORITIES);          \
                                                                               \
            head_ptr = &(&dest_tcb->msg_head0)[msg_priority];                  \
            tail_ptr = &(&dest_tcb->msg_tail0)[msg_priority];                  \
                                                                               \
            /* Empty queue? */                                                 \
            is_queue_empty = (NULL == *head_ptr);                              \
            if (is_queue_empty)                                                \
            {                                                                  \
                *head_ptr = msg_ptr;                                           \
            }                                                                  \
            else                                                               \
            {                                                                  \
                (*tail_ptr)->flink = msg_ptr;                                  \
            }                                                                  \
                                                                               \
            /* Finish stitching links */                                       \
            *tail_ptr = msg_ptr;                                               \
                                                                               \
            /*** Unblock task if this warrants it */                           \
                                                                               \
            is_awakeable = ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_MSG);   \
            if (is_awakeable)                                                  \
            {                                                                  \
                /* Set 'block_flags' to ready state */                         \
                dest_tcb->block_flags = 0;                                     \
                                                                               \
                NUFRKERNEL_ADD_TASK_TO_READY_LIST(dest_tcb);                   \
                                                                               \
                if (macro_do_switch)                                           \
                {                                                              \
                    NUFR_INVOKE_CONTEXT_SWITCH();                              \
                }                                                              \
            }                                                                  \
        }                                                                      \
    }                                                                          \
                                                                               \
    NUFR_UNLOCK_INTERRUPTS(saved_psr);                                         \
}

//!
//! @name      NUFR_MSG_SEND_INLINE_NO_LOCKING
//!
//! @brief     Speed-opitimized inline message sending w.o. interrupt locks
//!
//! @details   Same as NUFR_MSG_SEND_INLINE, except interrupt locks
//! @details   have been omitted. Comments from above apply
//! @details   
//! @details   You can disable locking if we're in an ISR and no other ISR
//! @details   is both set at a higher interrupt priority and makes nufr
//! @details   calls.
//! @details   
#define NUFR_MSG_SEND_INLINE_NO_LOCKING(task_id, msg_prefix, msg_id, msg_priority, fixed_parameter) \
{                                                                              \
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;                            \
    nufr_msg_t             *msg_ptr;                                           \
    nufr_msg_t             *next_msg_ptr;                                      \
    nufr_tcb_t             *dest_tcb;                                          \
    nufr_msg_t            **head_ptr;                                          \
    nufr_msg_t            **tail_ptr;                                          \
    unsigned                block_flags;                                       \
    bool                    send_occured;                                      \
    bool                    is_queue_empty;                                    \
    bool                    is_awakeable;                                      \
                                                                               \
    dest_tcb = NUFR_TID_TO_TCB(task_id);                                       \
                                                                               \
    block_flags = dest_tcb->block_flags;                                       \
                                                                               \
    /* Sanity check: dest task must be active */                               \
    send_occured = ARE_BITS_CLR(block_flags, NUFR_TASK_NOT_LAUNCHED);          \
    KERNEL_REQUIRE_IL(send_occured);                                           \
    if (send_occured)                                                          \
    {                                                                          \
        KERNEL_REQUIRE_IL(NULL != nufr_msg_free_head);                         \
                                                                               \
        /* Check that a message block is available */                          \
        if (NULL != nufr_msg_free_head)                                        \
        {                                                                      \
            /* Allocate block from pool. */                                    \
            msg_ptr = nufr_msg_free_head;                                      \
            next_msg_ptr = msg_ptr->flink;                                     \
            nufr_msg_free_head = next_msg_ptr;                                 \
            if (NULL == next_msg_ptr)                                          \
            {                                                                  \
                nufr_msg_free_tail = NULL;                                     \
                nufr_msg_pool_empty_count++;                                   \
                KERNEL_REQUIRE_IL(false);                                      \
            }                                                                  \
                                                                               \
            /*** Populate msg_ptr block members */                             \
            msg_ptr->flink = NULL;                                             \
            msg_ptr->fields = NUFR_SET_MSG_FIELDS((msg_prefix), (msg_id), 0, (msg_priority)); \
            msg_ptr->parameter = (fixed_parameter);                            \
                                                                               \
            /*** Enqueue msg_ptr block onto TCB */                             \
                                                                               \
            /* Should be a compile-time check */                               \
            KERNEL_REQUIRE_IL(msg_priority < NUFR_CS_MSG_PRIORITIES);          \
                                                                               \
            head_ptr = &(&dest_tcb->msg_head0)[msg_priority];                  \
            tail_ptr = &(&dest_tcb->msg_tail0)[msg_priority];                  \
                                                                               \
            /* Empty queue? */                                                 \
            is_queue_empty = (NULL == *head_ptr);                              \
            if (is_queue_empty)                                                \
            {                                                                  \
                *head_ptr = msg_ptr;                                           \
            }                                                                  \
            else                                                               \
            {                                                                  \
                (*tail_ptr)->flink = msg_ptr;                                  \
            }                                                                  \
                                                                               \
            /* Finish stitching links */                                       \
            *tail_ptr = msg_ptr;                                               \
                                                                               \
            /*** Unblock task if this warrants it */                           \
                                                                               \
            is_awakeable = ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_MSG);   \
            if (is_awakeable)                                                  \
            {                                                                  \
                /* Set 'block_flags' to ready state */                         \
                dest_tcb->block_flags = 0;                                     \
                                                                               \
                NUFRKERNEL_ADD_TASK_TO_READY_LIST(dest_tcb);                   \
                                                                               \
                if (macro_do_switch)                                           \
                {                                                              \
                    NUFR_INVOKE_CONTEXT_SWITCH();                              \
                }                                                              \
            }                                                                  \
        }                                                                      \
    }                                                                          \
                                                                               \
}

#endif  // NUFR_KERNEL_MESSAGE_SEND_INLINE_H_