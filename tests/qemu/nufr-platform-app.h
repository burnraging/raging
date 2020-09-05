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

//! @file     nufr-platform-app.h
//! @authors  Bernie Woodland
//! @date     30Sep17
//!
//! @brief   Application specification of OS objects
//!

#ifndef NUFR_PLATFORM_APP_H
#define NUFR_PLATFORM_APP_H

#include "nufr-global.h"

#include "nufr-kernel-base-task.h"


//#define QEMU_PROJECT   1
// Knight Rider on by default
#define QEMU_PROJECT   2


//!
//! @enum  task IDs
//!
//! @details Mandatory enum
//! @details Mandatory members:
//! @details     NUFR_TID_null
//! @details     NUFR_TID_max
//!
#if QEMU_PROJECT == 1
typedef enum
{
    NUFR_TID_null = 0,    // not a task, do not change
    NUFR_TID_01,
    NUFR_TID_02,
    NUFR_TID_03,
    NUFR_TID_04,
    NUFR_TID_05,
    NUFR_TID_max          // not a task, do not change
} nufr_tid_t;

#elif QEMU_PROJECT == 2
typedef enum
{
    NUFR_TID_null = 0,    // not a task, do not change
    NUFR_TID_01,
    NUFR_TID_EVENT_TASK,
    NUFR_TID_STATE_TASK,
    NUFR_TID_max          // not a task, do not change
} nufr_tid_t;

#else
    #error "Invalid QEMU_PROJECT value!!!"
#endif

#define NUFR_NUM_TASKS       (NUFR_TID_max - 1)

//!
//! @enum  task priority values
//!
//! @details Mandatory enum
//! @details Mandatory members:
//! @details   NUFR_TPR_null
//! @details   NUFR_TPR_guaranteed_highest
//!
typedef enum
{
    NUFR_TPR_null = 0,                // Do not change. Do not assign to tasks
    NUFR_TPR_guaranteed_highest = 1,  // Do not change. Do not assign to tasks

    // Add/delete/change per needs
    NUFR_TPR_HIGHEST = 7,
    NUFR_TPR_HIGHER = 8,
    NUFR_TPR_HIGH = 9,

    // Must have this enum (can change value, however).
    // Default priority, most tasks will use this
    NUFR_TPR_NOMINAL = 10,

    // Add/delete/change per needs
    NUFR_TPR_LOW = 11,
    NUFR_TPR_LOWER = 12,
    NUFR_TPR_LOWEST = 13
} nufr_tpr_t;

//!
//! @name     NUFR_MAX_MSGS
//!
//! @brief    Size of message block pool (bpool)
//! @brief    Mandatory definition
//!
#define NUFR_MAX_MSGS                        10

//!
//! @brief     Semaphore
//!


typedef enum
{
    NUFR_SEMA_null = 0,  // not a sema, do not change
    NUFR_SEMA_POOL_START,      // fixed enum name, used by SL
    NUFR_SEMA_POOL_END = NUFR_SEMA_POOL_START + 2,  // fixed too
    NUFR_SEMA_max        // not a sema, do not change
} nufr_sema_t;

#define NUFR_NUM_SEMAS      (NUFR_SEMA_max - 1)

#define NUFR_SEMA_POOL_SIZE (NUFR_SEMA_POOL_END - NUFR_SEMA_POOL_START + 1)


#ifndef NUFR_PLAT_APP_GLOBAL_DEFS
    extern const nufr_task_desc_t nufr_task_desc[NUFR_NUM_TASKS];
#endif  //NUFR_PLAT_APP_GLOBAL_DEFS

#if QEMU_PROJECT == 1
void entry_01(unsigned parm);
void entry_02(unsigned parm);
void entry_03(unsigned parm);
void entry_04(unsigned parm);
void entry_05(unsigned parm);
#elif QEMU_PROJECT == 2
void entry_01(unsigned parm);
void entry_event_task(unsigned parm);
void entry_state_task(unsigned parm);
#endif

#endif  //NUFR_PLATFORM_APP_H