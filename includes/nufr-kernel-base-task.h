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

//! @file     nufr-kernel-base-task.h
//! @authors  Bernie Woodland
//! @date     7Aug17
//!
//! @brief   Task-related definitions that fit either of:
//! @brief     (1) Non-customizable platform layer constructs
//! @brief     (2) Defined in kernel but needed in platform layer
//!
//! @details 

#ifndef NUFR_KERNEL_BASE_TASK_H
#define NUFR_KERNEL_BASE_TASK_H

#include "nufr-global.h"

#include "nufr-platform-import.h"

#if NUFR_CS_MESSAGING == 1
    #include "nufr-kernel-base-messaging.h"
#endif  //NUFR_CS_MESSAGING

#if NUFR_CS_SEMAPHORE == 1
    #include "nufr-kernel-base-semaphore.h"
#endif  //NUFR_CS_SEMAPHORE

//!
//! @struct  Task descriptor
//!
typedef struct
{
    char        *name;
    void       (*entry_point_fcn_ptr)(unsigned);
    uint32_t    *stack_base_ptr;
    unsigned     stack_size;
    uint8_t      start_priority;    //of 'nufr_tpr_t'
    uint8_t      instance;
} nufr_task_desc_t;

//!
//! @struct  Task Control Block (TCB)
//!
// 12 bytes for ARM; 6 bytes for MSP430
// NUFR_SP_OFFSET_IN_TCB must be == offsetof(nufr_tcb_t, stack_ptr)
//
#define NUFR_SP_INDEX_IN_TCB              3
//#define NUFR_SP_OFFSET_IN_TCB            (NUFR_SP_INDEX_IN_TCB * sizeof(unsigned *))

typedef struct nufr_tcb_t_
{
    // main link pointer, points to next tcb on list
    struct nufr_tcb_t_ *flink;         // recommend keeping at top

    // Links used with OS tick timer
    struct nufr_tcb_t_ *flink_timer;
    struct nufr_tcb_t_ *blink_timer;

        // 'stack_ptr' must be offset NUFR_SP_OFFSET_IN_TCB bytes 'nufr_tcb_t'
    unsigned           *stack_ptr;       // 'unsigned' will be 32-bits by default, 16-bits/20-bits on MSP430/X

#if NUFR_CS_LOCAL_STRUCT == 1
    void               *local_struct_ptr;
#endif

#if NUFR_CS_SEMAPHORE == 1
    // The sema task wait list is implemented as a dual linked list, instead
    // of a single linked list.
    struct nufr_tcb_t_ *blink;

    // The sema which this task is engaged with. If NULL, no sema in use.
    // If not NULL, means:
    //   1. If task is blocked, the sema it's blocked on
    //   2. It task is not blocked, the sema which this task has taken.
    //      This has limited usefulness, if a sema is initialized with
    //      a count > 1, then this task can take multiple counts.
    //      Also, it's possible for this task to have taken multiple sema's,
    //      so this whould be the last one taken.
    nufr_sema_block_t  *sema_block;
#endif  //NUFR_CS_SEMAPHORE

    uint32_t            timer;

    // Flags indicating why task isn't ready.
    // Each block condition has its own bit.
    // If 'block_flags'==0, then task is ready.
    uint8_t             block_flags;             //see below

    // General status flags
    uint8_t             statuses;                //see below

    // Flags to be passed from kernel to API upon task
    // resuming after having been blocked on API.
    uint8_t             notifications;           //see below

    uint8_t             priority;                //of 'nufr_tpr_t'

    // Saved priority, used only by nufr_prioritize()
    uint8_t             priority_restore_prioritized ; //of 'nufr_tpr_t'

    // Saved priority, used when task priority is raised to
    //   prevent a priority inversion on a sem
    uint8_t             priority_restore_inversion;    //of 'nufr_tpr_t'

#if NUFR_CS_TASK_KILL == 1
    // Msg priority below which a message send will abort an API wait
    uint8_t             abort_message_priority;  //of 'nufr_msg_pri_t'
#endif  //NUFR_CS_TASK_KILL

    uint16_t            bop_key;
#if NUFR_CS_MESSAGING == 1
    // Each message priority level has its own queue, so
    // the 0,1,2,3 variables are for each msg priority level.
    // Ordering dependency in these variables! They're indexed
    // like in an array.
        nufr_msg_t     *msg_head0;
    #if NUFR_CS_MSG_PRIORITIES >= 2
        nufr_msg_t     *msg_head1;
    #endif
    #if NUFR_CS_MSG_PRIORITIES >= 3
        nufr_msg_t     *msg_head2;
    #endif
    #if NUFR_CS_MSG_PRIORITIES == 4
        nufr_msg_t     *msg_head3;
    #endif
        nufr_msg_t     *msg_tail0;
    #if NUFR_CS_MSG_PRIORITIES >= 2
        nufr_msg_t     *msg_tail1;
    #endif
    #if NUFR_CS_MSG_PRIORITIES >= 3
        nufr_msg_t     *msg_tail2;
    #endif
    #if NUFR_CS_MSG_PRIORITIES == 4
        nufr_msg_t     *msg_tail3;
    #endif
#endif  //NUFR_CS_MESSAGING
} nufr_tcb_t;

// values for tcb->block_flags field
#define NUFR_TASK_NOT_LAUNCHED           0x01
#define NUFR_TASK_BLOCKED_ASLEEP         0x02
#define NUFR_TASK_BLOCKED_BOP            0x04
#define NUFR_TASK_BLOCKED_MSG            0x08
#define NUFR_TASK_BLOCKED_SEMA           0x10
#define NUFR_TASK_BLOCKED_ALL                       \
        (NUFR_TASK_NOT_LAUNCHED   |                 \
         NUFR_TASK_BLOCKED_ASLEEP |                 \
         NUFR_TASK_BLOCKED_BOP    |                 \
         NUFR_TASK_BLOCKED_MSG    |                 \
         NUFR_TASK_BLOCKED_SEMA)

// values for tcb->statuses field
          // task on OS timer list
#define NUFR_TASK_TIMER_RUNNING          0x01
          // bop sent before task checks for it
#define NUFR_TASK_BOP_PRE_ARRIVED        0x02
          // bop wait locked against bop timeout
#define NUFR_TASK_BOP_LOCKED             0x04
          // priority raised to prevent inversion/'priority_restore_inversion'
#define NUFR_TASK_INVERSION_PRIORITIZED  0x08


// values for tcb->notifications
         // task was waiting on API timeout; OS tick handler timed out task
#define NUFR_TASK_TIMEOUT                0x01
         // Task unblocked due to message at abort priority
         // Only relevant if NUFR_CS_TASK_KILL set
#define NUFR_TASK_UNBLOCKED_BY_MSG_SEND  0x02


// TCB bit helper macros
#define NUFR_IS_TASK_LAUNCHED(tcb)    (!ANY_BITS_SET((tcb)->block_flags, NUFR_TASK_NOT_LAUNCHED) )
#define NUFR_IS_TASK_BLOCKED(tcb)     ( ANY_BITS_SET((tcb)->block_flags, NUFR_TASK_BLOCKED_ALL) )
#define NUFR_IS_TASK_NOT_BLOCKED(tcb) ( ARE_BITS_CLR((tcb)->block_flags, NUFR_TASK_BLOCKED_ALL) )
#define NUFR_IS_STATUS_CLR(tcb, bits) ( ARE_BITS_CLR((tcb)->statuses, (bits)) )
#define NUFR_IS_STATUS_SET(tcb, bits) ( ANY_BITS_SET((tcb)->statuses, (bits)) )
#define NUFR_IS_BLOCK_CLR(tcb, bits)  ( ARE_BITS_CLR((tcb)->block_flags, (bits)) )
#define NUFR_IS_BLOCK_SET(tcb, bits)  ( ANY_BITS_SET((tcb)->block_flags, (bits)) )
#define NUFR_IS_NOTIF_CLR(tcb, bits)  ( ARE_BITS_CLR((tcb)->notifications, (bits)) )
#define NUFR_IS_NOTIF_SET(tcb, bits)  ( ANY_BITS_SET((tcb)->notifications, (bits)) )

#endif  //NUFR_KERNEL_BASE_TASK_H