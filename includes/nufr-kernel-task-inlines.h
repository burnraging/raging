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

//! @file     nufr-kernel-task-inlines.h
//! @authors  Bernie Woodland
//! @date     25May19
//!
//! @brief   Inline version of called kernel functions
//!
//! @details WARNING! YOU MUST KEEP THESE FCNS INDENTICAL
//! @details          TO NON-INLINED VERSIONS!
//!

#ifndef NUFR_KERNEL_TASK_INLINES_H_
#define NUFR_KERNEL_TASK_INLINES_H_

#include "nufr-global.h"

#include "nufr-platform.h"
#include "nufr-kernel-base-task.h"
#include "nufr-kernel-task.h"

#include "raging-contract.h"

//! @name      NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS
//
//! @brief     Declarations for NUFRKERNEL_ADD_TASK_TO_READY_LIST
//!
#define NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS                         \
    bool      macro_do_switch;                                                 \
    unsigned  macro_priority

//! @name      NUFRKERNEL_ADD_TASK_TO_READY_LIST
//
//! @brief     inline version of nufrkernel_add_task_to_ready_list()
//
#define NUFRKERNEL_ADD_TASK_TO_READY_LIST(m_tcb)                               \
                                                                               \
    macro_do_switch = false;                                                   \
                                                                               \
    KERNEL_ENSURE_IL(NULL != (m_tcb));                                         \
    KERNEL_ENSURE_IL(NULL == (m_tcb)->flink);                                  \
                                                                               \
    macro_priority = (m_tcb)->priority;                                        \
                                                                               \
    if (NULL == nufr_ready_list)                                               \
    {                                                                          \
        if (NUFR_TPR_NOMINAL == macro_priority)                                \
        {                                                                      \
            nufr_ready_list_tail_nominal = m_tcb;                              \
        }                                                                      \
                                                                               \
        nufr_ready_list = m_tcb;                                               \
        nufr_ready_list_tail = m_tcb;                                          \
                                                                               \
        macro_do_switch = true;                                                \
                                                                               \
        KERNEL_ENSURE_IL(NULL == (m_tcb)->flink);                              \
        KERNEL_ENSURE_IL(nufr_ready_list == nufr_ready_list_tail);             \
    }                                                                          \
    else if ((NUFR_TPR_NOMINAL == macro_priority) &&                           \
             (NULL != nufr_ready_list_tail_nominal))                           \
    {                                                                          \
        nufr_tcb_t *flink = nufr_ready_list_tail_nominal->flink;               \
        nufr_ready_list_tail_nominal->flink = m_tcb;                           \
        (m_tcb)->flink = flink;                                                \
                                                                               \
        nufr_ready_list_tail_nominal = m_tcb;                                  \
                                                                               \
        if (NULL == flink)                                                     \
        {                                                                      \
            nufr_ready_list_tail = m_tcb;                                      \
        }                                                                      \
                                                                               \
        KERNEL_ENSURE_IL(NULL != nufr_ready_list);                             \
        KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);                        \
    }                                                                          \
    else if (macro_priority < nufr_ready_list->priority)                       \
    {                                                                          \
        if (NUFR_TPR_NOMINAL == macro_priority)                                \
        {                                                                      \
            nufr_ready_list_tail_nominal = m_tcb;                              \
        }                                                                      \
                                                                               \
        (m_tcb)->flink = nufr_ready_list;                                      \
        nufr_ready_list = m_tcb;                                               \
                                                                               \
        macro_do_switch = true;                                                \
                                                                               \
        KERNEL_ENSURE_IL(NULL != nufr_ready_list);                             \
        KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);                        \
    }                                                                          \
    else if (macro_priority >= nufr_ready_list_tail->priority)                 \
    {                                                                          \
        if (NUFR_TPR_NOMINAL == macro_priority)                                \
        {                                                                      \
            nufr_ready_list_tail_nominal = m_tcb;                              \
        }                                                                      \
                                                                               \
        nufr_ready_list_tail->flink = m_tcb;                                   \
        nufr_ready_list_tail = m_tcb;                                          \
                                                                               \
        KERNEL_ENSURE_IL(NULL != nufr_ready_list);                             \
        KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);                        \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        nufr_tcb_t *prev_tcb;                                                  \
        nufr_tcb_t *next_tcb;                                                  \
        bool        null_nominal_tail = (NULL == nufr_ready_list_tail_nominal);\
                                                                               \
        KERNEL_ENSURE_IL(NULL != nufr_ready_list);                             \
        KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);                        \
        KERNEL_ENSURE_IL(nufr_ready_list != nufr_ready_list_tail);             \
                                                                               \
        if ((macro_priority < NUFR_TPR_NOMINAL) || null_nominal_tail)          \
        {                                                                      \
            if (null_nominal_tail && (NUFR_TPR_NOMINAL == macro_priority))     \
            {                                                                  \
                nufr_ready_list_tail_nominal = m_tcb;                          \
            }                                                                  \
                                                                               \
            prev_tcb = nufr_ready_list;                                        \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            prev_tcb = nufr_ready_list_tail_nominal;                           \
        }                                                                      \
                                                                               \
        next_tcb = prev_tcb->flink;                                            \
                                                                               \
        KERNEL_ENSURE_IL(NULL != prev_tcb);                                    \
        KERNEL_ENSURE_IL(NULL != next_tcb);                                    \
                                                                               \
        while (NULL != next_tcb)                                               \
        {                                                                      \
            if (macro_priority < next_tcb->priority)                           \
            {                                                                  \
                KERNEL_ENSURE_IL(NULL != prev_tcb);                            \
                KERNEL_ENSURE_IL(NULL != next_tcb);                            \
                                                                               \
                (m_tcb)->flink = next_tcb;                                     \
                prev_tcb->flink = m_tcb;                                       \
                                                                               \
                break;                                                         \
            }                                                                  \
                                                                               \
            prev_tcb = next_tcb;                                               \
            next_tcb = next_tcb->flink;                                        \
        }                                                                      \
                                                                               \
        KERNEL_ENSURE_IL(NULL != next_tcb);                                    \
    }                                                                          \
                                                                               \
    KERNEL_ENSURE_IL((nufr_ready_list == NULL) == (nufr_ready_list_tail == NULL));\
    KERNEL_ENSURE_IL(nufr_ready_list == NULL?                                  \
               nufr_ready_list_tail_nominal == NULL : true);                   \
    KERNEL_ENSURE_IL(nufr_ready_list_tail != NULL?                             \
               nufr_ready_list_tail->flink == NULL : true);                    \
    KERNEL_ENSURE_IL((nufr_ready_list != NULL) && (nufr_ready_list_tail != NULL) \
           && (nufr_ready_list != nufr_ready_list_tail)?                       \
               nufr_ready_list->flink != NULL : true)


//! @name      NUFRKERNEL_BLOCK_RUNNING_TASK
//
//! @brief     nufrkernel_block_running_task() macro
#define NUFRKERNEL_BLOCK_RUNNING_TASK(m_block_flag)                            \
{                                                                              \
    nufr_tcb_t *next_tcb;                                                      \
                                                                               \
    KERNEL_REQUIRE_IL(ANY_BITS_SET((m_block_flag), NUFR_TASK_NOT_LAUNCHED   |  \
                                     NUFR_TASK_BLOCKED_ASLEEP |                \
                                     NUFR_TASK_BLOCKED_BOP    |                \
                                     NUFR_TASK_BLOCKED_MSG    |                \
                                     NUFR_TASK_BLOCKED_SEMA));                 \
    KERNEL_REQUIRE_IL(ANY_BITS_SET((m_block_flag), NUFR_TASK_NOT_LAUNCHED)?    \
            ARE_BITS_CLR((m_block_flag), NUFR_TASK_BLOCKED_ASLEEP |            \
                                     NUFR_TASK_BLOCKED_BOP    |                \
                                     NUFR_TASK_BLOCKED_MSG    |                \
                                     NUFR_TASK_BLOCKED_SEMA)                   \
            : true);                                                           \
    KERNEL_REQUIRE_IL(ANY_BITS_SET((m_block_flag), NUFR_TASK_BLOCKED_ASLEEP)?  \
            ARE_BITS_CLR((m_block_flag), NUFR_TASK_BLOCKED_BOP    |            \
                                     NUFR_TASK_BLOCKED_MSG    |                \
                                     NUFR_TASK_BLOCKED_SEMA)                   \
            : true);                                                           \
    KERNEL_REQUIRE_IL(ANY_BITS_SET((m_block_flag), NUFR_TASK_BLOCKED_BOP)?     \
            ARE_BITS_CLR((m_block_flag), NUFR_TASK_BLOCKED_MSG    |            \
                                     NUFR_TASK_BLOCKED_SEMA)                   \
            : true);                                                           \
    KERNEL_REQUIRE_IL(ANY_BITS_SET((m_block_flag), NUFR_TASK_BLOCKED_BOP)?     \
            ARE_BITS_CLR((m_block_flag), NUFR_TASK_BLOCKED_SEMA)               \
            : true);                                                           \
                                                                               \
    KERNEL_REQUIRE_IL(NULL != nufr_ready_list);                                \
    KERNEL_REQUIRE_IL(NULL != nufr_ready_list_tail);                           \
                                                                               \
    nufr_ready_list->block_flags = (m_block_flag);                             \
                                                                               \
    next_tcb = nufr_ready_list->flink;                                         \
    nufr_ready_list->flink = NULL;                                             \
                                                                               \
    if (NULL == next_tcb)                                                      \
    {                                                                          \
        KERNEL_ENSURE_IL(nufr_ready_list == nufr_ready_list_tail);             \
                                                                               \
        nufr_ready_list = NULL;                                                \
        nufr_ready_list_tail = NULL;                                           \
        nufr_ready_list_tail_nominal = NULL;                                   \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        if (nufr_ready_list_tail_nominal == nufr_ready_list)                   \
        {                                                                      \
            nufr_ready_list_tail_nominal = NULL;                               \
        }                                                                      \
                                                                               \
        nufr_ready_list = next_tcb;                                            \
    }                                                                          \
                                                                               \
    KERNEL_ENSURE_IL((nufr_ready_list == NULL) == (nufr_ready_list_tail == NULL));\
    KERNEL_ENSURE_IL(nufr_ready_list == NULL?                                  \
               nufr_ready_list_tail_nominal == NULL : true);                   \
    KERNEL_ENSURE_IL(nufr_ready_list_tail != NULL?                             \
               nufr_ready_list_tail->flink == NULL : true);                    \
    KERNEL_ENSURE_IL((nufr_ready_list != NULL) && (nufr_ready_list_tail != NULL)\
           && (nufr_ready_list != nufr_ready_list_tail)?                       \
               nufr_ready_list->flink != NULL : true);                         \
}

//! @name      NUFRKERNEL_REMOVE_HEAD_TASK_FROM_READY_LIST
//
//! @brief     nufrkernel_remove_head_task_from_ready_list() macro form
#define NUFRKERNEL_REMOVE_HEAD_TASK_FROM_READY_LIST()                          \
{                                                                              \
    nufr_tcb_t *next_tcb;                                                      \
                                                                               \
    KERNEL_ENSURE_IL(NULL != nufr_ready_list);                                 \
    KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);                            \
                                                                               \
    next_tcb = nufr_ready_list->flink;                                         \
    nufr_ready_list->flink = NULL;                                             \
                                                                               \
    if (NULL == next_tcb)                                                      \
    {                                                                          \
        KERNEL_ENSURE_IL(nufr_ready_list == nufr_ready_list_tail);             \
                                                                               \
        nufr_ready_list = NULL;                                                \
        nufr_ready_list_tail = NULL;                                           \
        nufr_ready_list_tail_nominal = NULL;                                   \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        if (nufr_ready_list_tail_nominal == nufr_ready_list)                   \
        {                                                                      \
            nufr_ready_list_tail_nominal = NULL;                               \
        }                                                                      \
                                                                               \
        nufr_ready_list = next_tcb;                                            \
    }                                                                          \
                                                                               \
    KERNEL_ENSURE_IL((nufr_ready_list == NULL) == (nufr_ready_list_tail == NULL));\
    KERNEL_ENSURE_IL(nufr_ready_list == NULL?                                  \
               nufr_ready_list_tail_nominal == NULL : true);                   \
    KERNEL_ENSURE_IL(nufr_ready_list_tail != NULL?                             \
               nufr_ready_list_tail->flink == NULL : true);                    \
    KERNEL_ENSURE_IL((nufr_ready_list != NULL) && (nufr_ready_list_tail != NULL)\
           && (nufr_ready_list != nufr_ready_list_tail)?                       \
               nufr_ready_list->flink != NULL : true);                         \
}

//! @name      NUFRKERNEL_DELETE_TASK_FROM_READY_LIST
//!
//! @brief     Macro form of nufrkernel_delete_task_from_ready_list()
//!
#define NUFRKERNEL_DELETE_TASK_FROM_READY_LIST(m_tcb)                          \
{                                                                              \
    nufr_tcb_t *prev_tcb;                                                      \
    nufr_tcb_t *this_tcb;                                                      \
    nufr_tcb_t *next_tcb;                                                      \
    bool        found_it;                                                      \
                                                                               \
    KERNEL_REQUIRE_IL(NUFR_IS_TCB(m_tcb));                                     \
                                                                               \
    /* Changes here... */                                                      \
    if ((NULL != nufr_ready_list) && ((m_tcb) != nufr_running))                \
    {                                                                          \
                                                                               \
    KERNEL_ENSURE_IL(NULL != nufr_ready_list);                                 \
    KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);                            \
    KERNEL_ENSURE_IL(nufr_ready_list == nufr_ready_list_tail ?                 \
              nufr_running == (nufr_tcb_t *)nufr_bg_sp                         \
              : true);                                                         \
                                                                               \
    if ((m_tcb) == nufr_ready_list)                                            \
    {                                                                          \
        prev_tcb = NULL;                                                       \
        this_tcb = nufr_ready_list;                                            \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        prev_tcb = nufr_ready_list;                                            \
        this_tcb = prev_tcb->flink;                                            \
    }                                                                          \
                                                                               \
    found_it = false;                                                          \
                                                                               \
    while (NULL != this_tcb)                                                   \
    {                                                                          \
        if ((m_tcb) == this_tcb)                                               \
        {                                                                      \
            found_it = true;                                                   \
                                                                               \
            break;                                                             \
        }                                                                      \
                                                                               \
        prev_tcb = this_tcb;                                                   \
        this_tcb = this_tcb->flink;                                            \
    }                                                                          \
                                                                               \
    /* Changes here... */                                                      \
    if (found_it && (NULL != this_tcb))                                        \
    {                                                                          \
                                                                               \
    next_tcb = this_tcb->flink;                                                \
                                                                               \
    if ((m_tcb) == nufr_ready_list)                                            \
    {                                                                          \
        nufr_ready_list = next_tcb;                                            \
    }                                                                          \
                                                                               \
    if ((m_tcb) == nufr_ready_list_tail_nominal)                               \
    {                                                                          \
        if ((NULL != prev_tcb) && (NUFR_TPR_NOMINAL == prev_tcb->priority))    \
        {                                                                      \
            nufr_ready_list_tail_nominal = prev_tcb;                           \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            nufr_ready_list_tail_nominal = NULL;                               \
        }                                                                      \
    }                                                                          \
                                                                               \
    if ((m_tcb) == nufr_ready_list_tail)                                       \
    {                                                                          \
        nufr_ready_list_tail = prev_tcb;                                       \
    }                                                                          \
                                                                               \
    if (NULL != prev_tcb)                                                      \
    {                                                                          \
        prev_tcb->flink = (m_tcb)->flink;                                      \
    }                                                                          \
                                                                               \
    (m_tcb)->flink = NULL;                                                     \
                                                                               \
    /* Changes here... */                                                      \
    }                                                                          \
    }                                                                          \
                                                                               \
    KERNEL_ENSURE_IL((nufr_ready_list == NULL) == (nufr_ready_list_tail == NULL));\
    KERNEL_ENSURE_IL(nufr_ready_list == NULL?                                  \
               nufr_ready_list_tail_nominal == NULL : true);                   \
    KERNEL_ENSURE_IL(nufr_ready_list_tail != NULL?                             \
               nufr_ready_list_tail->flink == NULL : true);                    \
    KERNEL_ENSURE_IL((nufr_ready_list != NULL) && (nufr_ready_list_tail != NULL)\
           && (nufr_ready_list != nufr_ready_list_tail)?                       \
               nufr_ready_list->flink != NULL : true);                         \
}

#endif  // NUFR_KERNEL_TASK_INLINES_H_