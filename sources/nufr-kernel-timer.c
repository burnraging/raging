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

//!
//! @file    nufr-kernel-timer.c
//! @authors Bernie Woodland
//! @date    12Aug17
//!
//! @brief   Kernel management of OS Tick w.r.t. task timers.
//!

#define NUFR_TIMER_GLOBAL_DEFS

#include "nufr-global.h"
#include "nufr-kernel-base-task.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nufr-platform-export.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-semaphore.h"
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    #include "nufr-kernel-task-inlines.h"
    #include "nufr-kernel-semaphore-inlines.h"
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 0
#include "nufr-api.h"

#include "raging-contract.h"

#ifdef USING_SECONDARY_CONTEXT_SWITCH_FILE
    #include "secondary-context-switch.h"
#endif

//! @name      nufr_timer_list, nufr_timer_list_tail
//!
//! @brief     Kernel-maintained linked list of all tasks waiting
//! @brief     on an API timeout, or a sleep timeout.
nufr_tcb_t *nufr_timer_list;
nufr_tcb_t *nufr_timer_list_tail;

//! @name      nufr_os_tick_count
//!
//! @brief     Continuous counter, increments each OS tick, wraps
uint32_t nufr_os_tick_count;

//! @name      nufrkernel_update_task_timers
//
//! @brief     Kernel's function which updates task timers for nufr_sleep(),
//! @brief     nufr_bop_waitT(), etc
//
//! @details   When an API call starts a task timer:
//! @details     (1) The tcb->timer value is set to the timeout specifed by
//! @details         the API call
//! @details     (2) Calls 'nufrkernel_add_to_timer_list' for task
//! @details     (3) That task's tcb is marked as "timer running"
//! @details         (tcb->statuses NUFR_TASK_TIMER_RUNNING)
//! @details     (4) The tcb is added to 'nufr_timer_list', no ordering.
//! @details         Of note is that the timer list uses the tcb->flink_timer
//! @details         and tcb->blink_timer instead of tcb->flink.
//! @details
//! @details   Each OS tick exception:
//! @details     (1) The timer list is walked. Each tcb on list is checked.
//! @details         Note that interrutps are NOT locked while list is
//! @details         traversed.
//! @details     (2) Each task's timer is decremented.
//! @details     (3) If task's timer reached zero, then it has timed out.
//! @details     (4) A timeout causes the task to be taken off the timer list.
//! @details         (Algorithm described in nufrkernel_purge_from_timer_list).
//! @details     (5) For the timed out task, interrupts are locked while
//! @details         shared tcb variables are modified.
//! @details     (6) [Task timeout algorithm proceeds, described below.]
//! @details     (7) A flag keeps track of whether a context switch is needed.
//! @details
//! @details   When a given task's timer has timed out:
//! @details     (1) Remove the tcb from the timer list by calling
//! @details         'nufr_purge_from_timer_list'.
//! @details     (2) Lock interrupts
//! @details     (3) Check if bop locked ('nufr_bop_lock_waiter')
//! @details     (3) Clear tcb->statuses : NUFR_TASK_TIMER_RUNNING
//! @details     (4) Set tcb->notifications : NUFR_TASK_TIMEOUT
//! @details     (5) Most likely, the task is still blocked (there's
//! @details         a small window wherein an ISR might've unblocked
//! @details         the task). Check if the task is still blocked
//! @details         and clear the blocking bit. Possible bits:
//! @details         (To save CPU cycles, can just clear
//! @details          all these bits in one write.)
//! @details     (6) If task was blocked on a sema, remove the tcb
//! @details         from the sema's tcb wait list. Fix the flink and blink
//! @details         pointers of both task and sema.
//! @details     (7) If task was blocked on a bop, no special action
//! @details     (8) If the task was found to have been blocked
//! @details         (most likely, it was), clear block bit and
//! @details         insert it in the ready list.
//! @details     (9) Unlock interrupts
//! @details    (10) If the ready list insert called for a context switch,
//! @details          set 'context_switch_needed'.
void nufrkernel_update_task_timers(void)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_tcb_t        *tcb;
    nufr_tcb_t        *next_tcb;
    nufr_sr_reg_t      saved_psr;
    bool               bop_locked;
    bool               invoke = false;

    nufr_os_tick_count++;

    // Empty timer list?
    if (NULL == nufr_timer_list)
    {
        return;
    }

    // If there's a head, there must be a tail
    SL_ENSURE((nufr_timer_list == NULL) == (nufr_timer_list_tail == NULL));
    // Head/tail blink/flink ptrs must be NULL
    SL_ENSURE((nufr_timer_list != NULL) ?
           (nufr_timer_list->blink_timer == NULL) &&
               (nufr_timer_list_tail->flink_timer == NULL)
           : true);
    // If > 1 task on list, must have non-NULL flink/blinks on head/tail
    SL_ENSURE((nufr_timer_list != NULL) &&
                (nufr_timer_list != nufr_timer_list_tail) ?
           (nufr_timer_list->flink_timer != NULL) &&
               (nufr_timer_list_tail->blink_timer != NULL)
           : true);

    // Note that interrupts are not locked while traversing the timer
    //  list. The timer list is only modified by API calls at task level,
    //  never by ISRs, so no interrupt locking needed.

    // Walk timer list
    tcb = nufr_timer_list;

    while (NULL != tcb)
    {
        // When a timer expires, its flink_timer gets cleared, so
        // must save away flink_timer now.
        next_tcb = tcb->flink_timer;

        SL_ENSURE(tcb->timer > 0);

        tcb->timer--;

        // Timeout occured?
        if (0 == tcb->timer)
        {
            bop_locked = false;

        #if NUFR_CS_LOCAL_STRUCT == 1
            saved_psr = NUFR_LOCK_INTERRUPTS();

            // If this task is being bop-locked, and timer
            // has expired, just extend timeout until unlock
            // occurs, which shouldn't take long.
            if (NUFR_IS_STATUS_SET(tcb, NUFR_TASK_BOP_LOCKED))
            {
                if (NUFR_IS_BLOCK_SET(tcb, NUFR_TASK_BLOCKED_BOP))
                {
                    tcb->timer = 1;
                    bop_locked = true;
                }
            }

            NUFR_UNLOCK_INTERRUPTS(saved_psr);
        #endif  // NUFR_CS_LOCAL_STRUCT

            if (!bop_locked)
            {
                // Is 'tcb' at list head?
                if (nufr_timer_list == tcb)
                {
                    SL_ENSURE(NULL == tcb->blink_timer);
                    nufr_timer_list = tcb->flink_timer;
                }
                else
                {
                    SL_ENSURE(NULL != tcb->blink_timer);
                    (tcb->blink_timer)->flink_timer = tcb->flink_timer;
                }

                // Is 'tcb' at list tail?
                if (nufr_timer_list_tail == tcb)
                {
                    SL_ENSURE(NULL == tcb->flink_timer);
                    nufr_timer_list_tail = tcb->blink_timer;
                }
                else
                {
                    SL_ENSURE(NULL != tcb->flink_timer);
                    (tcb->flink_timer)->blink_timer = tcb->blink_timer;
                }

                tcb->flink_timer = NULL;
                tcb->blink_timer = NULL;

                // If there's a head, there must be a tail
                SL_ENSURE((nufr_timer_list == NULL) == (nufr_timer_list_tail == NULL));
                // Head/tail blink/flink ptrs must be NULL
                SL_ENSURE((nufr_timer_list != NULL) ?
                          (nufr_timer_list->blink_timer == NULL) &&
                              (nufr_timer_list_tail->flink_timer == NULL)
                          : true);
                // If > 1 task on list, must have non-NULL flink/blinks on head/tail
                SL_ENSURE((nufr_timer_list != NULL) &&
                               (nufr_timer_list != nufr_timer_list_tail) ?
                          (nufr_timer_list->flink_timer != NULL) &&
                              (nufr_timer_list_tail->blink_timer != NULL)
                          : true);

                saved_psr = NUFR_LOCK_INTERRUPTS();

                tcb->statuses &= BITWISE_NOT8(NUFR_TASK_TIMER_RUNNING);

                // Cannot assume task is still blocked on blocking condition
                //   which caused it to be put on timer list.
                //   Cases:
                //    -- Task may have been unblocked at ISR level, with
                //       ISR sending task a message, sending a bop, or
                //       incrementing a sema.
                //    -- Another task may have unblocked task whose timer
                //       just expired. The task being unblocked, and not the
                //       unblocker, is responsible for removing task from
                //       timer list. It's possible to have task on ready
                //       list and timer still being decremented here.
                //
                // Only this fcn. and exit points to API calls /w timeouts
                //   can unlink a tcb from the timer list.
                if (NUFR_IS_TASK_BLOCKED(tcb))
                {
                    SL_REQUIRE_IL(ANY_BITS_SET(tcb->block_flags,
                                         NUFR_TASK_BLOCKED_ALL));

                #if NUFR_CS_SEMAPHORE == 1
                    if (NUFR_IS_BLOCK_SET(tcb, NUFR_TASK_BLOCKED_SEMA))
                    {
                    #if NUFR_CS_OPTIMIZATION_INLINES == 1
                        NUFRKERNEL_SEMA_UNLINK_TASK(tcb->sema_block, tcb);
                    #else
                        nufrkernel_sema_unlink_task(tcb->sema_block, tcb);
                    #endif
                    }
                #endif  // NUFR_CS_SEMAPHORE

                    tcb->block_flags = 0;

                    // Nofify task being released by timeout at exit of API
                    tcb->notifications |= NUFR_TASK_TIMEOUT;

                #if NUFR_CS_OPTIMIZATION_INLINES == 1
                    NUFRKERNEL_ADD_TASK_TO_READY_LIST(tcb);
                    invoke |= macro_do_switch;
                #else
                    invoke |= nufrkernel_add_task_to_ready_list(tcb);
                #endif
                }

                NUFR_UNLOCK_INTERRUPTS(saved_psr);
            }  // end 'if (!bop_locked)'
        }  // end 'if (0 == tcb->timer)'

        tcb = next_tcb;
    }  // end 'while (NULL != tcb)'

    // Did we trigger a context switch?
    // NOTE: small timing corner case: ISR might've requested context
    //       switch already, but since OS Systick handler is a higher
    //       priority than PendSV, only one PendSV will be invoked.

    if (invoke)
    {
        NUFR_INVOKE_CONTEXT_SWITCH();
        NUFR_SECONDARY_CONTEXT_SWITCH();
    }
}

//! @name      nufrkernel_add_to_timer_list
//
//! @brief     Append this task to timer list.
//
//! @details   Notes:
//! @details     -- Intended to be called from task level; cannot be
//! @details        called from ISR level.
//! @details     -- Caller must lock interrupts.
//! @details     -- No checks here for tcb->blocked bits. Caller must
//! @details        do that independently.
//! @details   
//!
//! params[in] 'task'
//! params[in] 'initial_timer_value'
void nufrkernel_add_to_timer_list(nufr_tcb_t *tcb,
                                  uint32_t    initial_timer_value)
{
    bool            already_on_timer_list;

    SL_REQUIRE_IL(tcb->flink_timer == NULL);
    SL_REQUIRE_IL(tcb->blink_timer == NULL);

    // Sanity check, not necessary    
    already_on_timer_list = NUFR_IS_STATUS_SET(tcb, NUFR_TASK_TIMER_RUNNING);
    if (already_on_timer_list)
    {
        SL_REQUIRE_IL(false);    // fatal error
        return;
    }
    else
    {
        tcb->statuses |= NUFR_TASK_TIMER_RUNNING;

        // Notifications should have already been cleared by API caller
    }

    // Empty list?
    if (NULL == nufr_timer_list)
    {
        SL_ENSURE_IL(NULL == nufr_timer_list_tail);
        nufr_timer_list = tcb;
    }
    // Else, append to tail
    else
    {
        SL_ENSURE_IL(NULL != nufr_timer_list_tail);
        nufr_timer_list_tail->flink_timer = tcb;
        tcb->blink_timer = nufr_timer_list_tail;
    }

    nufr_timer_list_tail = tcb;

    tcb->timer = initial_timer_value;

    // If there's a head, there must be a tail
    SL_ENSURE_IL((nufr_timer_list == NULL) == (nufr_timer_list_tail == NULL));
    // Head/tail blink/flink ptrs must be NULL
    SL_ENSURE_IL((nufr_timer_list != NULL) ?
                 (nufr_timer_list->blink_timer == NULL) &&
                     (nufr_timer_list_tail->flink_timer == NULL)
                 : true);
    // If > 1 task on list, must have non-NULL flink/blinks on head/tail
    SL_ENSURE_IL((nufr_timer_list != NULL) &&
                      (nufr_timer_list != nufr_timer_list_tail) ?
                 (nufr_timer_list->flink_timer != NULL) &&
                     (nufr_timer_list_tail->blink_timer != NULL)
                 : true);
}

//! @name      nufrkernel_purge_from_timer_list
//
//! @brief     Removes task from timer list.
//! @brief     Intention is for tasks exiting an API call where a
//! @brief     timed delay was used will call this to take
//! @brief     themselves off the timer list.
//!
//! @details   Notes:
//! @details     -- Called from task level; not allowed to be called at
//! @details        interrupt level.
//! @details     -- Caller must lock interrupts
//!            
//! @params[in] 'tcb'
//!            
//! @return     'true' if task was found on timer list
bool nufrkernel_purge_from_timer_list(nufr_tcb_t *tcb)
{
    // Sanity check: must already be on timer list
    if (NUFR_IS_STATUS_CLR(tcb, NUFR_TASK_TIMER_RUNNING))
    {
        return false;
    }

    // Clear bit which indicates task is on timer list
    tcb->statuses &= BITWISE_NOT8(NUFR_TASK_TIMER_RUNNING);

    // Is 'tcb' at list head?
    if (nufr_timer_list == tcb)
    {
        SL_ENSURE_IL(NULL == tcb->blink_timer);
        nufr_timer_list = tcb->flink_timer;
    }
    else
    {
        SL_ENSURE_IL(NULL != tcb->blink_timer);
        (tcb->blink_timer)->flink_timer = tcb->flink_timer;
    }

    // Is 'tcb' at list tail?
    if (nufr_timer_list_tail == tcb)
    {
        SL_ENSURE_IL(NULL == tcb->flink_timer);
        nufr_timer_list_tail = tcb->blink_timer;
    }
    else
    {
        SL_ENSURE_IL(NULL != tcb->flink_timer);
        (tcb->flink_timer)->blink_timer = tcb->blink_timer;
    }

    tcb->flink_timer = NULL;
    tcb->blink_timer = NULL;

    // If there's a head, there must be a tail
    SL_ENSURE_IL((nufr_timer_list == NULL) == (nufr_timer_list_tail == NULL));
    // Head/tail blink/flink ptrs must be NULL
    SL_ENSURE_IL((nufr_timer_list != NULL) ?
                 (nufr_timer_list->blink_timer == NULL) &&
                     (nufr_timer_list_tail->flink_timer == NULL)
                 : true);
    // If > 1 task on list, must have non-NULL flink/blinks on head/tail
    SL_ENSURE_IL((nufr_timer_list != NULL) &&
                      (nufr_timer_list != nufr_timer_list_tail) ?
                 (nufr_timer_list->flink_timer != NULL) &&
                     (nufr_timer_list_tail->blink_timer != NULL)
                 : true);

    return true;
}

//! @name      nufr_tick_count_get
//
//! @brief     Retrieve current OS tick count.
//!
//! @details   Nufr maintains a variable called 'nufr_os_tick_count'.
//! @details   Count increments each OS tick, wraps and continues.
//!            
//! @return    count
uint32_t nufr_tick_count_get(void)
{
    return nufr_os_tick_count;
}

//! @name      nufr_tick_count_delta
//
//! @brief     Retrieve a delta OS tick count between a previously
//! @brief     captured OS tick value and the current OS tick value.
//!
//! @param[in] 'reference_count'-- the previously captured OS tick value.
//!            
//! @return    Difference in OS tick counts
uint32_t nufr_tick_count_delta(uint32_t reference_count)
{
    // Snapshot value in case we get preempted
    uint32_t current_count = nufr_os_tick_count;

    // Did 'nufr_os_tick_count' wrap inbetween reference time and
    //   current time?
    if (reference_count > current_count)
    {
        return current_count - (BITWISE_NOT32(0) - reference_count) + 1;
    }

    return current_count - reference_count;
}