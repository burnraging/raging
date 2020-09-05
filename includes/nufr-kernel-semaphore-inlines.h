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

//! @file     nufr-kernel-semaphore-inlines.h
//! @authors  Bernie Woodland
//! @date     26May19
//!
//! @brief   Inline version of called kernel functions
//!
//! @details WARNING! YOU MUST KEEP THESE FCNS INDENTICAL
//! @details          TO NON-INLINED VERSIONS!
//!

#ifndef NUFR_KERNEL_SEMAPHORE_INLINES_H_
#define NUFR_KERNEL_SEMAPHORE_INLINES_H_

#include "nufr-global.h"

#include "nufr-platform.h"
#include "nufr-kernel-base-task.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-base-semaphore.h"

#include "raging-contract.h"


//!
//! @name      NUFRKERNEL_SEMA_LINK_TASK
//!
//! brief      Macro form of nufrkernel_sema_link_task()
//!
#define NUFRKERNEL_SEMA_LINK_TASK(m_sema_block, m_add_tcb)                    \
{                                                                             \
    nufr_tcb_t             *head_tcb;                                         \
    nufr_tcb_t             *tail_tcb;                                         \
    nufr_tcb_t             *prev_tcb;                                         \
    nufr_tcb_t             *next_tcb;                                         \
    unsigned                add_priority;                                     \
                                                                              \
    KERNEL_REQUIRE_IL((m_sema_block) != NULL);                                \
    KERNEL_REQUIRE_IL(NUFR_IS_TCB((m_add_tcb)));                              \
    KERNEL_REQUIRE_IL((m_add_tcb)->flink == NULL);                            \
    KERNEL_REQUIRE_IL((m_add_tcb)->blink == NULL);                            \
                                                                              \
    add_priority = (m_add_tcb)->priority;                                     \
                                                                              \
    head_tcb = (m_sema_block)->task_list_head;                                \
    tail_tcb = (m_sema_block)->task_list_tail;                                \
                                                                              \
    if (NULL == head_tcb)                                                     \
    {                                                                         \
        (m_sema_block)->task_list_head = (m_add_tcb);                         \
        (m_sema_block)->task_list_tail = (m_add_tcb);                         \
    }                                                                         \
    else if (add_priority >= tail_tcb->priority)                              \
    {                                                                         \
        (m_add_tcb)->blink = tail_tcb;                                        \
        tail_tcb->flink = (m_add_tcb);                                        \
        (m_sema_block)->task_list_tail = (m_add_tcb);                         \
    }                                                                         \
    else if (add_priority < head_tcb->priority)                               \
    {                                                                         \
        (m_add_tcb)->flink = head_tcb;                                        \
        head_tcb->blink = (m_add_tcb);                                        \
        (m_sema_block)->task_list_head = (m_add_tcb);                         \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        prev_tcb = head_tcb;                                                  \
        next_tcb = prev_tcb->flink;                                           \
                                                                              \
        KERNEL_REQUIRE_IL(prev_tcb != NULL);                                  \
        KERNEL_REQUIRE_IL(next_tcb != NULL);                                  \
                                                                              \
        while (NULL != next_tcb)                                              \
        {                                                                     \
            if (add_priority < next_tcb->priority)                            \
            {                                                                 \
                break;                                                        \
            }                                                                 \
                                                                              \
            prev_tcb = next_tcb;                                              \
            next_tcb = next_tcb->flink;                                       \
        }                                                                     \
                                                                              \
        KERNEL_REQUIRE_IL(next_tcb != NULL);                                  \
                                                                              \
        (m_add_tcb)->flink = prev_tcb->flink;                                 \
        prev_tcb->flink = (m_add_tcb);                                        \
        (m_add_tcb)->blink = next_tcb->blink;                                 \
        next_tcb->blink = (m_add_tcb);                                        \
    }                                                                         \
                                                                              \
    KERNEL_ENSURE(((m_sema_block)->task_list_head == NULL) ==                 \
           ((m_sema_block)->task_list_tail == NULL));                         \
    KERNEL_ENSURE((m_sema_block)->task_list_head != NULL ?                    \
           (m_sema_block)->task_list_head->blink == NULL                      \
           : true);                                                           \
    KERNEL_ENSURE((m_sema_block)->task_list_tail != NULL ?                    \
           (m_sema_block)->task_list_tail->flink == NULL                      \
           : true);                                                           \
    KERNEL_ENSURE(((m_sema_block)->task_list_head != NULL) &&                 \
           ((m_sema_block)->task_list_head != (m_sema_block)->task_list_tail) ?\
           ((m_sema_block)->task_list_head->flink != NULL) &&                 \
           ((m_sema_block)->task_list_tail->blink != NULL)                    \
           : true);                                                           \
}

//!
//! @name      nufrkernel_sema_unlink_task
//!
//! brief      Macro equivalent for nufrkernel_sema_unlink_task()
#define NUFRKERNEL_SEMA_UNLINK_TASK(m_sema_block, m_delete_tcb)               \
{                                                                             \
    KERNEL_REQUIRE_IL((m_sema_block) != NULL);                                \
    KERNEL_REQUIRE_IL(NUFR_IS_TCB((m_delete_tcb)));                           \
                                                                              \
    if ((m_sema_block)->task_list_head == (m_delete_tcb))                     \
    {                                                                         \
        (m_sema_block)->task_list_head = (m_delete_tcb)->flink;               \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        ((m_delete_tcb)->blink)->flink = (m_delete_tcb)->flink;               \
    }                                                                         \
                                                                              \
    if ((m_sema_block)->task_list_tail == (m_delete_tcb))                     \
    {                                                                         \
        (m_sema_block)->task_list_tail = (m_delete_tcb)->blink;               \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        ((m_delete_tcb)->flink)->blink = (m_delete_tcb)->blink;               \
    }                                                                         \
                                                                              \
    (m_delete_tcb)->flink = NULL;                                             \
    (m_delete_tcb)->blink = NULL;                                             \
                                                                              \
    KERNEL_ENSURE(((m_sema_block)->task_list_head == NULL) ==                 \
           ((m_sema_block)->task_list_tail == NULL));                         \
    KERNEL_ENSURE((m_sema_block)->task_list_head != NULL ?                    \
           (m_sema_block)->task_list_head->blink == NULL                      \
           : true);                                                           \
    KERNEL_ENSURE((m_sema_block)->task_list_tail != NULL ?                    \
           (m_sema_block)->task_list_tail->flink == NULL                      \
           : true);                                                           \
    KERNEL_ENSURE(((m_sema_block)->task_list_head != NULL) &&                 \
           ((m_sema_block)->task_list_head != (m_sema_block)->task_list_tail) ?\
           ((m_sema_block)->task_list_head->flink != NULL) &&                 \
           ((m_sema_block)->task_list_tail->blink != NULL)                    \
           : true);                                                           \
}


#endif  // NUFR_KERNEL_SEMAPHORE_INLINES_H_