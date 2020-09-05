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

//! @file    nufr-kernel-task.c
//! @authors Bernie Woodland
//! @date    19May18


#define NUFR_TASK_GLOBAL_DEFS

#include "nufr-global.h"

#include "nufr-platform.h"
#include "nufr-platform-import.h"
#include "nufr-platform-app.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-timer.h"
#include "nufr-api.h"
#include "nufr-kernel-semaphore.h"

#include "raging-contract.h"
#include "raging-utils.h"

nufr_tcb_t nufr_tcb_block[NUFR_NUM_TASKS];

// Current running task.
// Only updated by PendSV handler
// If BG task is running, set to ptr to 'nufr_bg_sp'
nufr_tcb_t *nufr_running;

// Ready list head. NULL if list is empty/the BG task is running.
// This is also the current running task
nufr_tcb_t *nufr_ready_list;

// 'nufr_ready_list' value before last context switch
//nufr_tcb_t *nufr_previous_running_task;           not needed ??

//ready list tail of NUFR_TPR_NOMINAL tasks. NULL if no NUFR_TPR_NOMINAL
//   tasks on list.
nufr_tcb_t *nufr_ready_list_tail_nominal;

//ready list tail. NULL if list is empty
nufr_tcb_t *nufr_ready_list_tail;

// Background task's stack pointer (necessary since BG task has no TCB).
// Must be big enough to hold an SP offset in it.
uint32_t nufr_bg_sp[NUFR_SP_INDEX_IN_TCB + 1];

uint16_t nufr_bop_key;




//! @name      nufr_launch_task
// !
//! @brief     Goes into Task Descriptor block, finds stack and entry point
//! @brief     and puts task on ready list.
//!
//! @details   Task may either self-terminate by calling 'nufr_exit_running_task()',
//! @details   or simply return from the entry point.
//! @details
//! @details   Before launching, must place function pointer to
//! @details   'nufrplat_task_exit_point()' onto bottom of stack of task
//! @details   being launched. When task returns from entry point, it'll
//! @details   pop 'nufrplat_task_exit_point' into LR, and thereby
//! @details   jump to it.
//!
//! @param[in] 'task_id'-- task to be launched
//! @param[in] 'parameter'-- parameter to be passed to entry point of task
//!
void nufr_launch_task(nufr_tid_t task_id, unsigned parameter)
{
       
}

//! @name      nufrkernel_exit_running_task
//
//! @brief     Under-the-covers exit routine. Gets called automatically.
//! @brief     Tasks need only return from their entry point call.
//!
void nufrkernel_exit_running_task(void)
{
}

#if NUFR_CS_TASK_KILL == 1
void nufr_kill_task(nufr_tid_t task_id)
{
}
#endif  //NUFR_CS_TASK_KILL

//! @name      nufr_self_tid
//
//! @brief     Returns task ID of currently running task
nufr_tid_t nufr_self_tid(void)
{
    return NUFR_TID_null;
}

//! @name      nufr_task_running_state
//
//! @brief     Ascertain running or blocked state of a given task
//
//! @param[in] 'task_id'
//
//! @return    State enumerator
nufr_bkd_t nufr_task_running_state(nufr_tid_t task_id)
{
    return NUFR_BKD_NOT_LAUNCHED;
}

//! @name      nufr_sleep
//!
//! @brief     Cause currently running task to sleep for so many OS clock ticks
//!
//! @details   Cannot be called from an ISR or from BG task
//! @details
//!
//! @param[in] sleep_delay_in_ticks-- sleep interval, in OS clock ticks
//! @param[in]              recommend wrapping 'sleepDelayInTicks' parm with
//! @param[in]              NUFR_MILLISECS_TO_TICKS or NUFR_SECS_TO_TICKS
//! @param[in] abort_priority_of_rx_msg-- priority where, lower than that,
//! @param[in]        message rx will abort sleep.
//!
//! @return    'true' if aborted by message send
bool nufr_sleep(unsigned       sleep_delay_in_ticks,
                nufr_msg_pri_t abort_priority_of_rx_msg)
{
    return false;
}

//! @name      nufr_yield
//
//! @brief     If other tasks of the same task priority are ready, current
//! @brief     running task lets them run; otherwise, no change
//
//! @details   Cannot be called from an ISR or from BG task
//! @details
//!
//! @return    'true' if another task was waiting/context switch happened
bool nufr_yield(void)
{
    return false;
}

//! @name      nufr_prioritize
//
//! @brief     Sets current running task to a priority (NUFR_TPR_guaranteed_highest)
//! @brief     higher than any other task's. Saves off old priority, for restoration
//! @brief     during nufr_unprioritize()
//
//! @details   Cannot be called from an ISR or from BG task
//! @details
//!
void nufr_prioritize(void)
{
}

void nufr_unprioritize(void)
{
}

void nufr_change_task_priority(nufr_tid_t tid, unsigned new_priority)
{
}

nufr_bop_wait_rtn_t nufr_bop_waitW(nufr_msg_pri_t abort_priority_of_rx_msg)
{
    return NUFR_BOP_WAIT_INVALID;
}

nufr_bop_wait_rtn_t nufr_bop_waitT(nufr_msg_pri_t abort_priority_of_rx_msg,
                                   unsigned       timeout_ticks)
{
    return NUFR_BOP_WAIT_INVALID;
}