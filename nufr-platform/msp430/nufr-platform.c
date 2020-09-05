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

//! @file     nufr-platform.c
//! @authors  Bernie Woodland
//! @date     12Aug17
//!
//! @brief   nufr platform layer
//!
//! @details The "mandatory" extensions are functions, variables, definitions,
//! @details etc that the kernel compiles against.
//! @details The platform part of the kernel also allows customization, to
//! @details scale nufr up or down, according to the needs of the project.

#include "nufr-global.h"
#include "nufr-kernel-base-task.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-base-semaphore.h"
#include "nufr-kernel-base-messaging.h"
#include "nufr-kernel-message-blocks.h"
#include "nufr-kernel-timer.h"
#include "nufr-kernel-semaphore.h"
#include "nufr-api.h"

#include "raging-contract.h"
#include "raging-utils-mem.h"

#include <stdio.h>


//  Implementing the onContractFailure method with a print statement
//  to allow local debugging under commandline workflow
static uint32_t g_contractFailureCount;
void onContractFailure(uint8_t* file, uint32_t line)
{
    g_contractFailureCount++;
    //printf(">>>>> [%08u] Contract Failure occurred in %s on line %u\r\n", g_contractFailureCount, file, line);
    //fflush(stdout);
}

//! @name      nufr_sl_timer_callback_fcn_ptr
//!
//! @brief     Statically defined message pool
static void (*nufr_sl_timer_callback_fcn_ptr)(void);

//! @name      nufr_init
//!
//! @brief     Initializes the OS.
//! @brief     Cannot be called from a task.
//!
void nufr_init(void)
{
    unsigned                i;
    nufr_tcb_t             *tcb;
    const nufr_task_desc_t *desc_ptr;
    uint32_t               *start_stack_ptr;

    // Zero out TCB
    rutils_memset(&nufr_tcb_block, 0, sizeof(nufr_tcb_block));

    // All other task inits
    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        tcb = &nufr_tcb_block[i];

        // Set all non-zero values in TCB
        tcb->block_flags |= NUFR_TASK_NOT_LAUNCHED;

        desc_ptr = nufrplat_task_get_desc(tcb, NUFR_TID_null);

        // Zero out stack
        start_stack_ptr = desc_ptr->stack_base_ptr;
        rutils_memset(start_stack_ptr, 0, sizeof(desc_ptr->stack_size));

        // Set inital SP to bottom address of stack
        tcb->stack_ptr = (unsigned *)(desc_ptr->stack_base_ptr +
                         desc_ptr->stack_size - BYTES_PER_WORD32);

        // Set priority
        tcb->priority = desc_ptr->start_priority;
    }

    // Set BG task as running task
    //  (Since BG task has no TCB, must set
    nufr_running = (nufr_tcb_t *)nufr_bg_sp;

    nufr_ready_list = NULL;
    nufr_ready_list_tail = NULL;
    nufr_ready_list_tail_nominal = NULL;
    nufr_bop_key = 0;
    nufr_timer_list = NULL;
    nufr_timer_list_tail = NULL;

#if NUFR_CS_SEMAPHORE == 1
    // Semaphore inits
    for (i = 1; i < NUFR_NUM_SEMAS; i++)
    {
    #if NUFR_CS_HAS_SL == 1
        // don't init semas in pool, leave that for SL
        if ((i < NUFR_SEMA_POOL_START) || (i > NUFR_SEMA_POOL_END))
        {
            nufrkernel_sema_reset(NUFR_SEMA_ID_TO_BLOCK(i), 1, true);
        }
    #else
        nufrkernel_sema_reset(NUFR_SEMA_ID_TO_BLOCK(i), 1, true);
    #endif  // NUFR_CS_HAS_SL
    }
#endif  // NUFR_CS_SEMAPHORE

#if NUFR_CS_MESSAGING == 1
    // Init message bpool
    nufr_msg_bpool_init();
#endif  // NUFR_CS_MESSAGING
}

#if NUFR_CS_EXCLUDING_OS_INTERNAL_TICKS == 0
    #error "Does not agree with nufrplat_systick_handler()"
#endif

//! @name      nufr_plat_systick_handler
//
//! @brief     Entry point for timer which is dedicated to OS clock
//
//! @details   On the ARM Cortex, this is the SysTick exception handler.
//! @details   It's defined in the platform so a user can add functionality
//! @details   to it. It MUST call 'nufrkernel_update_task_timers()'.
//!
//! @details   NOT USED: MSP430 IS CURRENTLY CONFIGURED AS TICKLESS!
//!
//! @param[in]    sp-- Stack Pointer register value set by wrapper. See asm code.
//!
#if 0   // not used--this is a tickless OS implementation
void nufrplat_systick_handler(void)
{
    NUFR_SYSTICK_PREPROCESSING();

    // user-defined code (if any)

#if NUFR_CS_EXCLUDING_OS_INTERNAL_TICKS == 0
    nufrkernel_update_task_timers();
#endif

    if (NULL != nufr_sl_timer_callback_fcn_ptr)
    {
        (*nufr_sl_timer_callback_fcn_ptr)();
    }

    // user-defined code (if any)

    NUFR_SYSTICK_POSTPROCESSING();
}
#endif

//! @name      nufrplat_systick_sl_add_callback
//
//! @brief     Means for Service Layer timer to get SysTick callback
//
//! @details   ...instead of having compile switch
//void nufrplat_systick_sl_add_callback(void (*fcn_ptr)(void))
void nufrplat_systick_sl_add_callback(void *fcn_ptr)
{
    nufr_sl_timer_callback_fcn_ptr = fcn_ptr;
}

// Needed?
void nufrplat_task_exit_point(void)
{
}

//! @name      nufr_task_get_desc
//!
//! @brief     Retrieve a task's descriptor block
//!
//! @param[in] tcb-- task to get block for, or if null, then use...
//! @param[in] tid-- task to get block for
//!
//! @return    ptr to descriptor block (one of 'nufr_task_desc')
//! @return    returns NULL if 'tcb' or 'tid' is out of range
const nufr_task_desc_t *nufrplat_task_get_desc(nufr_tcb_t *tcb,
                                               nufr_tid_t tid)
{
    unsigned index;

    if (NULL == tcb)
        index = tid - 1;
    else
        index = NUFR_TCB_TO_TID(tcb) - 1;

    KERNEL_REQUIRE(index < ARRAY_SIZE(nufr_tcb_block));

    return &nufr_task_desc[index];
}