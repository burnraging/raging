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

//! @file    nsvc-timer.c
//! @authors Bernie Woodland
//! @date    4Aug18
//!
//! @brief   SL App Timers
//!
//! @details When a timer is started it's allocated from the global timer
//! @details pool, and freed back to the pool when it expires.
//! @details App timers send a message upon expiration. 
//! @details App timers are queued up in the order in which they'll expire.
//! @details This queuing allows a single timer (a "quantum timer") to be
//! @details programmed for the next expiration, rather than polling with
//! @details the OS tick. Both quantum timers and OS tick timers are supported.
//! @details 
//!

#include "nufr-global.h"
#include "nufr-platform.h"
#include "nufr-kernel-task.h"     // for 'nufr_running'
#include "nufr-api.h"
#include "nsvc-api.h"
#include "nsvc-app.h"
#include "nsvc.h"
#include "raging-contract.h"
#include "raging-utils-mem.h"


//!
//! @name      nsvc_timer_free_list_head
//! @name      nsvc_timer_free_list_tail
//! @name      nsvc_timer
//!
//! @brief     Timer pool
//!
//! @details   Internally maintained by this module
//!
nsvc_timer_t    *nsvc_timer_free_list_head;
nsvc_timer_t    *nsvc_timer_free_list_tail;
nsvc_timer_t     nsvc_timer[NSVC_NUM_TIMER];

//!
//! @name      IS_NOT_FROM_TIMER_POOL
//!
//! @brief     Sanity check timer block
//!
#define IS_NOT_FROM_TIMER_POOL(x)  ( ((x) < &nsvc_timer[0]) || ((x) > &nsvc_timer[NSVC_NUM_TIMER-1]) )

//!
//! @name      nsvc_timer_quantum_device_reconfigure_fcn_ptr
//!
//! @brief     Function pointer pointing to quantum driver callin fcn.
//!
nsvc_timer_quantum_device_reconfigure_fcn_ptr_t nsvc_timer_quantum_device_reconfigure_fcn_ptr;

nsvc_timer_get_current_time_fcn_ptr_t nsvc_timer_get_current_time_fcn_ptr;

//!
//! @name      nsvc_timer_queue_update_in_progress
//!
//! @details   Let IRQ handler for quantum timer or OS tick handler know
//! @details   not to change timer queue, as task is changing it.
//! @details   Not in .h file, must be externed by other file.
//!
bool nsvc_timer_queue_update_in_progress;

//!
//! @name      nsvc_timer_queue_head
//! @name      nsvc_timer_queue_tail
//! @name      nsvc_timer_queue_length
//!
//! @brief     Active timer queue ordered list
//!
//! @details   This list is maintained in the order of expiration,
//! @details   with the head being the first to expire.
//! @details   'nsvc_timer_queue_length' is the number of timers in list.
//!
nsvc_timer_t    *nsvc_timer_queue_head;
nsvc_timer_t    *nsvc_timer_queue_tail;
unsigned         nsvc_timer_queue_length;

//!
//! @name      nsvc_timer_expired_list_head
//! @name      nsvc_timer_expired_list_tail
//!
//! @brief     Expired timer queue
//!
//! @details   List of expired timers. Temporary list.
//!
nsvc_timer_t    *nsvc_timer_expired_list_head;
nsvc_timer_t    *nsvc_timer_expired_list_tail;

//!
//! @name      nsvc_timer_latest_time
//!
//! @brief     Last known 32-bit app timer H/W time reference
//!
//! @details   System code must provide a 32-bit H/W time reference
//! @details   Time reference starts at zero and increments 1 millisecond
//! @details   per timer count. When it reaches 0xFFFFFFFF (which is
//! @details   49 days 17 hours), timer rolls over to zero and continues.
//! @details   Whenever API call here is made, this value is updated.
//!
uint32_t         nsvc_timer_latest_time;

//!
//! @name      sl_timer_active_insert
//!
//! @brief     Enqueue a timer to the timer active queue
//!
//! @details   Links a timer into the active timer list.
//! @details   Timer is linked in before 'before_this_tm'. If 'before_this_tm'
//! @details   is NULL, timer is appended to list.
//! @details   
//! @details   If called from task level, caller must either lock interrupts
//! @details   or prioritize ('nufr_prioritize()') calling task.
//!
//! @param[in] 'tm'-- timer to enqueue.
//! @param[in] 'before_this_tm'-- Location in list to enqueue timer.
//! @param[in]      Set to NULL to append.
//!
//! @return    'true' if insert is new head
//!
bool sl_timer_active_insert(nsvc_timer_t *tm,
                            nsvc_timer_t *before_this_tm)
{
    bool             head_change = false;

    // List empty?
    if (0 == nsvc_timer_queue_length)
    {
        nsvc_timer_queue_head = tm;
        nsvc_timer_queue_tail = tm;

        head_change = true;
    }
    // Insert before existing timer ('before_this_tm')?
    else if (NULL != before_this_tm)
    {
        if (nsvc_timer_queue_head == before_this_tm)
        {
            nsvc_timer_queue_head = tm;

            head_change = true;
        }
        else
        {
            tm->blink = before_this_tm->blink;
            before_this_tm->blink->flink = tm;
        }

        before_this_tm->blink = tm;
        tm->flink = before_this_tm;
    }
    // Otherwise, append to list
    else
    {
        nsvc_timer_queue_tail->flink = tm;
        tm->blink = nsvc_timer_queue_tail;

        nsvc_timer_queue_tail = tm;
    }

    nsvc_timer_queue_length++;

    SL_REQUIRE_IL((NULL != nsvc_timer_queue_head) &&
                  (NULL != nsvc_timer_queue_tail));
    SL_REQUIRE_IL(nsvc_timer_queue_length > 0);
    SL_REQUIRE_IL(NULL == nsvc_timer_queue_head->blink);
    SL_REQUIRE_IL(NULL == nsvc_timer_queue_tail->flink);
    SL_REQUIRE_IL((1 != nsvc_timer_queue_length) ||
                  (nsvc_timer_queue_head == nsvc_timer_queue_tail));
    SL_REQUIRE_IL((1 != nsvc_timer_queue_length) ||
                  ((NULL == tm->flink) && (NULL == tm->blink)));
    SL_REQUIRE_IL((1 == nsvc_timer_queue_length) ||
                  (NULL != nsvc_timer_queue_head->flink));
    SL_REQUIRE_IL((nsvc_timer_queue_length > 1)?
                  ((NULL != nsvc_timer_queue_head->flink) &&
                  (NULL != nsvc_timer_queue_tail->blink)) : true);

    return head_change;
}

//!
//! @name      sl_timer_dequeue
//!
//! @brief     Dequeue a timer from the timer active queue
//!
//! @details   Assumes timer is on active timer queue.
//! @details   If timer is not on active timer queue, will cause crash. 
//! @details   
//! @details   If called from task level, caller must either lock interrupts
//! @details   or prioritize ('nufr_prioritize()') calling task.
//! @details   
//! @details   
//!
//! @param[in] 'tm'-- timer to dequeue.
//!
//! @return    'true' if active timer deqeued from head
//!
bool sl_timer_active_dequeue(nsvc_timer_t *tm)
{
    bool             head_change = false;

    SL_REQUIRE_IL(nsvc_timer_queue_length > 0);
    SL_REQUIRE_IL((1 != nsvc_timer_queue_length) ||
                  ((NULL == tm->flink) && (NULL == tm->blink)));
    SL_REQUIRE_IL((1 == nsvc_timer_queue_length) ||
                  ((NULL != tm->flink) || (NULL != tm->blink)));

    if (nsvc_timer_queue_head == tm)
    {
        nsvc_timer_queue_head = tm->flink;

        head_change = true;
    }
    else
    {
        tm->blink->flink = tm->flink;
    }

    if (nsvc_timer_queue_tail == tm)
    {
        nsvc_timer_queue_tail = tm->blink;
    }
    else
    {
        tm->flink->blink = tm->blink;
    }

    tm->flink = NULL;
    tm->blink = NULL;

    nsvc_timer_queue_length--;

    SL_REQUIRE_IL((NULL == nsvc_timer_queue_head) ==
                  (NULL == nsvc_timer_queue_tail));
    SL_REQUIRE_IL((NULL != nsvc_timer_queue_head)?
                      (NULL == nsvc_timer_queue_head->blink) : true);
    SL_REQUIRE_IL((NULL != nsvc_timer_queue_tail)?
                      (NULL == nsvc_timer_queue_tail->flink) : true);
    SL_REQUIRE_IL((1 != nsvc_timer_queue_length) ||
                  (nsvc_timer_queue_head == nsvc_timer_queue_tail));
    SL_REQUIRE_IL((nsvc_timer_queue_length > 1)?
                  (NULL != nsvc_timer_queue_head->flink) : true);
    SL_REQUIRE_IL((nsvc_timer_queue_length > 1)?
                  (NULL != nsvc_timer_queue_tail->blink) : true);

    return head_change;
}

//!
//! @name      sl_timer_push_expired
//!
//! @brief     Enqueue a timer to the expired timer queue
//!
//! @details   Appends a timer to the expired timer queue.
//! @details   
//! @details   If called from task level, caller must either lock interrupts
//! @details   or prioritize ('nufr_prioritize()') calling task.
//!
//! @param[in] 'tm'-- timer to enqueue.
//!
void sl_timer_push_expired(nsvc_timer_t *tm)
{
    SL_REQUIRE_IL((NULL == tm->flink) && (NULL == tm->blink));

    if (NULL == nsvc_timer_expired_list_head)
    {
        nsvc_timer_expired_list_head = tm;
        nsvc_timer_expired_list_tail = tm;
    }
    else
    {
        nsvc_timer_expired_list_tail->flink = tm;
        nsvc_timer_expired_list_tail = tm;
    }

    SL_REQUIRE_IL((NULL != nsvc_timer_expired_list_head) &&
                  (NULL != nsvc_timer_expired_list_tail));
    SL_REQUIRE_IL((NULL != nsvc_timer_expired_list_head)?
                  (NULL == nsvc_timer_expired_list_head->blink) &&
                  (NULL == nsvc_timer_expired_list_tail->flink) : true);
}

//!
//! @name      sl_timer_pop_expired
//!
//! @brief     Dequeue the head timer from the expired queue
//!
//! @details   If called from task level, caller must either lock interrupts
//! @details   or prioritize ('nufr_prioritize()') calling task.
//!
//! @return    head timer. NULL if queue was empty.
//!
nsvc_timer_t *sl_timer_pop_expired(void)
{
    nsvc_timer_t *tm;

    if (NULL == nsvc_timer_expired_list_head)
    {
        tm = NULL;
    }
    else
    {
        tm = nsvc_timer_expired_list_head;
        nsvc_timer_expired_list_head = tm->flink;
        tm->flink = NULL;

        if (NULL == nsvc_timer_expired_list_head)
        {
            nsvc_timer_expired_list_tail = NULL;
        }
    }

    SL_REQUIRE_IL((NULL != tm)? NULL == tm->blink : true);
    SL_REQUIRE_IL((NULL == nsvc_timer_expired_list_head) ==
                  (NULL == nsvc_timer_expired_list_tail));
    SL_REQUIRE_IL((NULL != nsvc_timer_expired_list_head)?
                  (NULL == nsvc_timer_expired_list_head->blink) &&
                  (NULL == nsvc_timer_expired_list_tail->flink) : true);

    return tm;
}

//!
//! @name      sl_timer_active_insert
//!
//! @brief     Find where in active timer list to insert a timer.
//!
//! @details   Walks active timer list to find the next timer which
//! @details   which expires after this one. If listed timer expires
//! @details   at same time, then inserted timer goes after this.
//! @details   The next timer is returned, or NULL if insert must
//! @details   be an append.
//! @details   
//! @details   If called from task level, caller must either lock interrupts
//! @details   or prioritize ('nufr_prioritize()') calling task.
//!
//! @param[in] 'current_time'-- current H/W time reference value
//! @param[in] 'duration'-- time duration to this timer's timeout
//! @param[in]      (in milliseconds)
//! @param[in] 'is_new_head_ptr'-- inserted timer will become new head
//!
//! @return    timer which will be after inserted timer. NULL if none.
//!
nsvc_timer_t *sl_timer_find_sorted_insert(uint32_t duration,
                                          bool    *is_new_head_ptr)
{
    nsvc_timer_t  *this_tm; 
    uint32_t       this_duration;
    uint32_t       this_expiration_time;

    this_tm = nsvc_timer_queue_head;

    // No active timers?
    if (NULL == this_tm)
    {
        *is_new_head_ptr = true;

        return NULL;
    }

    // Walk list from head: walk list in order of expiration
    while (NULL != this_tm)
    {
        this_expiration_time = this_tm->expiration_time;

        // Handles wrap case ok too
        this_duration = this_expiration_time - nsvc_timer_latest_time;

        // Does 'tm' expire before this timer?
        if (duration < this_duration)
        {
            // 
            if (this_tm == nsvc_timer_queue_head)
            {
                *is_new_head_ptr = true;
            }

            return this_tm;
        }

        this_tm = this_tm->flink;
    }

    // Appending to end of list
    *is_new_head_ptr = false;

    return NULL;
}

//!
//! @name      sl_timer_check_and_expire
//!
//! @brief     Walk active timer list, looking for expired timers.
//!
//! @details   Walks active timer list to find the all timers which
//! @details   have expired. An expired timer is one whose expiration
//! @details   time is either at 'nsvc_timer_latest_time' or up to
//! @details   'previous_check_time' milliseconds before that time.
//! @details
//! @details   All timers which are found to have expired are
//! @details   taken off the active timer list and pushed onto the
//! @details   expired timer list.
//! @details   
//! @details   If called from task level, caller must either lock interrupts
//! @details   or prioritize ('nufr_prioritize()') calling task.
//!
//! @param[in] 'previous_check_time'-- last reference time that expired
//! @param[in]     timers were checked for.
//!
//! @return    number of expired timers
//!
unsigned sl_timer_check_and_expire(uint32_t previous_check_time)
{
    nsvc_timer_t   *this_tm;
    nsvc_timer_t   *next_tm;
    unsigned        expired_count = 0;

    this_tm = nsvc_timer_queue_head;

    // non-wrap case    
    if (nsvc_timer_latest_time >= previous_check_time)
    {
        while (NULL != this_tm)
        {
            next_tm = this_tm->flink;

            if ((this_tm->expiration_time <= nsvc_timer_latest_time)
                                    &&
                (this_tm->expiration_time >= previous_check_time))
            {
                sl_timer_active_dequeue(this_tm);
                sl_timer_push_expired(this_tm);

                expired_count++;
            }
            else
            {
                // Since active timer list is ordered by expiration
                // time, no timers beyond 'this_tm' can be expired.
                break;
            }

            this_tm = next_tm;
        }
    }
    // wrap case    
    else
    {
        while (NULL != this_tm)
        {
            next_tm = this_tm->flink;

            if ((this_tm->expiration_time <= nsvc_timer_latest_time)
                                    ||
                (this_tm->expiration_time >= previous_check_time))
            {
                sl_timer_active_dequeue(this_tm);
                sl_timer_push_expired(this_tm);

                expired_count++;
            }
            else
            {
                break;
            }

            this_tm = next_tm;
        }
    }

    return expired_count;
}

//!
//! @name      sl_timer_process_expired_timers
//!
//! @brief     Walk expired timer list. Send a message for each
//! @brief     expired timer, then remove that timer from the list.
//!
//! @details   If expired timer is a continuous timer, it
//! @details   is put back on the active timer list.
//!
//! @return    'true' is a continuous timer, when reset, then
//! @return    became the next timer to expire (head)
//!
bool sl_timer_process_expired_timers(void)
{
    nsvc_timer_t    *tm;
    nsvc_timer_t    *before_this_tm;
    bool             is_new_head;
    bool             a_new_head = false;

    // Walk/drain expired timer list.
    while (NULL != (tm = sl_timer_pop_expired()))
    {
        nufr_msg_send(tm->msg_fields, tm->msg_parameter, tm->dest_task_id);

        // If timer is continuous, refire it.
        if (NSVC_TMODE_CONTINUOUS == tm->mode)
        {
            // Calculate new expiration time
            tm->expiration_time = tm->duration + nsvc_timer_latest_time;

            is_new_head = false;                // suppress warning
            before_this_tm = sl_timer_find_sorted_insert(tm->duration,
                                                         &is_new_head);

            a_new_head |= is_new_head || (NULL == before_this_tm);

            sl_timer_active_insert(tm, before_this_tm);
        }
        else
        {
            tm->is_active = false;
        }
    }

    SL_REQUIRE_IL(NULL == nsvc_timer_expired_list_head);
    SL_REQUIRE_IL(NULL == nsvc_timer_expired_list_tail);

    return a_new_head;
}

//!
//! @name      nsvc_timer_init
//!
//! @brief     Initialize SL app timer
//!
//! @details   Call before starting tasks
//!
//! @param[in] 'fptr_reconfigure'-- a callback function registered with
//! @param[in]     this module. This function retrieves current
//! @param[in]     32-bit H/W time.
//!
//! @param[in] 'quantum_device_reconfigure_fcn_ptr'--an optional
//! @param[in]     callback function registered with this module.
//! @param[in]     This function is the means for this module to reconfigure
//! @param[in]     quantum timer to new value based on timer changes.
//! @param[in]     Called from task level when task is prioritized
//! @param[in]     ('nufr_prioritize()'). If set to NULL, then
//! @param[in]     quantum timer is not used: caller will poll
//! @param[in]     app timers (through OS tick, presumably) for expirations.
//!
void nsvc_timer_init(nsvc_timer_get_current_time_fcn_ptr_t fptr_current_time,
           nsvc_timer_quantum_device_reconfigure_fcn_ptr_t fptr_reconfigure)
{
    nsvc_timer_t *tm;
    unsigned      i;

    // Register callback functions
    nsvc_timer_quantum_device_reconfigure_fcn_ptr = fptr_reconfigure;
    nsvc_timer_get_current_time_fcn_ptr = fptr_current_time;

    // Load current H/W time
    nsvc_timer_latest_time = (*fptr_current_time)();

    // Clear this module's static variables
    rutils_memset(nsvc_timer, 0, sizeof(nsvc_timer));
    nsvc_timer_free_list_head = NULL;
    nsvc_timer_free_list_tail = NULL;
    nsvc_timer_queue_length = 0;
    nsvc_timer_expired_list_head = NULL;
    nsvc_timer_expired_list_tail = NULL;

    // Populate timer pool
    for (i = 0; i < NSVC_NUM_TIMER; i++)
    {
        tm = &nsvc_timer[i];

        nsvc_timer_free(tm);
    }

    // Register tick callback
    nufrplat_systick_sl_add_callback(nsvc_timer_expire_timer_callin);
}

//!
//! @name      nsvc_timer_alloc
//!
//! @brief     Allocate a timer from the global timer pool.
//!
//! @details   If called from task level, caller must lock interrupts.
//!
//! @return   'tm'-- timer allocated. NULL if pool is empty.
//!
nsvc_timer_t *nsvc_timer_alloc(void)
{
    nufr_sr_reg_t    saved_psr;
    nsvc_timer_t    *tm;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    if (NULL != nsvc_timer_free_list_head)
    {
        tm = nsvc_timer_free_list_head;
        nsvc_timer_free_list_head = tm->flink;
        if (NULL == nsvc_timer_free_list_head)
        {
            nsvc_timer_free_list_tail = NULL;
        }
    }
    else
    {
        tm = NULL;
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    if (!IS_NOT_FROM_TIMER_POOL(tm))
    {
        // Init fields
        rutils_memset(tm, 0, sizeof(nsvc_timer_t)); 
        tm->mode = NSVC_TMODE_SIMPLE;
        tm->dest_task_id = NUFR_TID_null;
    }
    else
    {
        SL_ENSURE(false);
    }

    return tm;
}

//!
//! @name      nsvc_timer_free
//!
//! @brief     Free a timer back to the global timer pool.
//!
//! @details   If called from task level, caller must lock interrupts.
//!
//! @return   'tm'-- timer to free
//!
void nsvc_timer_free(nsvc_timer_t *tm)
{
    nufr_sr_reg_t  saved_psr;

    if (IS_NOT_FROM_TIMER_POOL(tm))
    {
        SL_REQUIRE_API(false);
        return;
    }

    SL_REQUIRE_API((NULL == tm->flink) && (NULL == tm->blink));

    saved_psr = NUFR_LOCK_INTERRUPTS();

    if (NULL != nsvc_timer_free_list_tail)
    {
        nsvc_timer_free_list_tail->flink = tm;
    }
    else
    {
        nsvc_timer_free_list_head = tm;
    }

    nsvc_timer_free_list_tail = tm;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);
}

//!
//! @name      nsvc_timer_start
//!
//! @brief     Starts a timer.
//!
//! @details   Must allocate timer before calling this API.
//! @details   Cannot be called from an IRQ due to nufr_prioritize() call.
//! @details   
//!
//! @param[in] 'tm'-- timer to start. Must have been allocated prior
//! @param[in]     to starting. Must have the following fields filled in:
//! @param[in]    ->mode:        one-shot (NSVC_MODE_SIMPLE)
//! @param[in]                  or repeating (NSVC_TMODE_CONTINUOUS).
//! @param[in]    ->duration:   expiration interval in milliseconds
//! @param[in]    ->msg_fields: prefix, ID, priority of message to be sent
//! @param[in]          when timer expires. This value maps to a
//! @param[in]          nufr_msg_t->fields word.
//! @param[in]          Set this parm with one of the following macros:
//! @param[in]             NUFR_SET_MSG_FIELDS
//! @param[in]             NSVC_TIMER_SET_ID
//! @param[in]             NSVC_TIMER_SET_PREFIX_ID
//! @param[in]             NSVC_TIMER_SET_PREFIX_ID_PRIORITY
//! @param[in]     ->msg_parameter: parameter value to be set in message
//! @param[in]     ->dest_task_id:  task to send expiration msg to.
//! @param[in]                      Set to 'NUFR_TID_null' for self-task
//!
void nsvc_timer_start(nsvc_timer_t     *tm)
{
    nufr_tid_t       dest_task;
    bool             is_valid_mode;
    bool             is_new_head;
    bool             a_new_head;
    uint8_t          saved_task_priority;
    uint32_t         previous_time;
    nsvc_timer_t    *before_this_tm;

    if (IS_NOT_FROM_TIMER_POOL(tm))
    {
        SL_REQUIRE_API(false);
        return;
    }

    is_valid_mode = (NSVC_TMODE_SIMPLE == tm->mode) ||
                    (NSVC_TMODE_CONTINUOUS == tm->mode);
    SL_REQUIRE_API(is_valid_mode);
    SL_REQUIRE_API(!(tm->is_active));
    SL_REQUIRE_API(NUFR_GET_MSG_ID(tm->msg_fields) <= NUFR_MSG_MAX_ID); 
    SL_REQUIRE_API(NUFR_GET_MSG_PREFIX(tm->msg_fields) <= NUFR_MSG_MAX_PREFIX); 
    SL_REQUIRE_API(NUFR_GET_MSG_PRIORITY(tm->msg_fields) <= NUFR_MSG_MAX_PRIORITY);
    SL_REQUIRE_API(tm->dest_task_id < NUFR_TID_max);
    SL_REQUIRE_API(NULL == tm->flink);
    SL_REQUIRE_API(NULL == tm->blink);

    // Sanity checks
    if (!is_valid_mode || tm->is_active || (0 == tm->duration))
    {
        return;
    }

    if (NUFR_TID_null == tm->dest_task_id)
    {
        dest_task = nufr_self_tid();
        tm->dest_task_id = (uint8_t)dest_task;
    }

    // In case we're nested in 'nufr_prioritize()' calls, saved
    // old priority so it doesn't get lost
    saved_task_priority = nufr_running->priority_restore_prioritized;

    // Need to ensure that no preemption occurs by task or by the quantum
    // timer or the OS timer. Can't have 2 threads updating active timer
    // queue at same time, or crash will occur. Queue is updated atomically
    // in this way:
    //
    //  1) Task making call is prioritized, preventing other tasks from
    //     preempting while timer list is updated.
    //  2) Flag 'nsvc_timer_queue_update_in_progress' is set to inform both
    //     quantum timer IRQ and OS Tick IRQ handler that it's not ok
    //     to pop a timer, as popping a timer meddles with active queue.
    //     Quantum timer/OS handler must take remedial action.
    //
    // nufr_prioritize() is used instead of NUFR_LOCK_INTERRUPTS() because
    // timer inserts to active timer queue could take a while, as it's a
    // an O(n) operation...to long to have interrupts locked.
    nufr_prioritize();

    // Notify IRQ or SysTick handlers that it's not safe
    //  to update timer lists.
    nsvc_timer_queue_update_in_progress = true;

    // Take this opportunity to update 'nsvc_timer_latest_time'
    // Do this to avoid some timing headaches.
    // We might catch 1 or more pending expired timers.
    previous_time = nsvc_timer_latest_time;
    nsvc_timer_latest_time = (*nsvc_timer_get_current_time_fcn_ptr)();

    tm->expiration_time = tm->duration + nsvc_timer_latest_time;

    a_new_head = sl_timer_check_and_expire(previous_time) > 0;

    a_new_head |= sl_timer_process_expired_timers();

    before_this_tm = sl_timer_find_sorted_insert(tm->duration,
                                                 &is_new_head);
    a_new_head |= is_new_head;

    tm->is_active = true;
    (void)sl_timer_active_insert(tm, before_this_tm);

    SL_REQUIRE(nsvc_timer_queue_length > 0);

    // Do we need to reconfigure quantum timer?
    if (a_new_head && (NULL != nsvc_timer_quantum_device_reconfigure_fcn_ptr))
    {
        (*nsvc_timer_quantum_device_reconfigure_fcn_ptr)(
                                          nsvc_timer_queue_head->duration);
    }

    // It's safe for IRQ handlers/SysTick to touch timer lists now.
    nsvc_timer_queue_update_in_progress = false;

    nufr_unprioritize();

    // Restore priority
    nufr_running->priority_restore_prioritized = saved_task_priority;
}

//!
//! @name      nsvc_timer_kill
//!
//! @brief     Cancel an SL app timer if it's active
//!
//! @return    'false' if no timer found to kill
//!
bool nsvc_timer_kill(nsvc_timer_t     *tm)
{
    uint8_t          saved_task_priority;
    bool             is_timer_active;
    bool             a_new_head;
    uint32_t         previous_time;

    if (IS_NOT_FROM_TIMER_POOL(tm))
    {
        SL_REQUIRE_API(false);
        return false;
    }

    // Save, in case of nufr_prioritize() nesting
    saved_task_priority = nufr_running->priority_restore_prioritized;

    nufr_prioritize();

    // Notify IRQ or SysTick handlers that it's not safe
    //  to update timer lists.
    nsvc_timer_queue_update_in_progress = true;

    // Take this opportunity to update 'nsvc_timer_latest_time'
    // Do this to avoid some timing headaches.
    // We might catch 1 or more pending expired timers.
    previous_time = nsvc_timer_latest_time;
    nsvc_timer_latest_time = (*nsvc_timer_get_current_time_fcn_ptr)();

    a_new_head = sl_timer_check_and_expire(previous_time) > 0;

    a_new_head |= sl_timer_process_expired_timers();

    // Is this timer running? If yes, stop it.
    is_timer_active = tm->is_active;
    if (tm->is_active)
    {
        a_new_head |= sl_timer_active_dequeue(tm);

        tm->is_active = false;
    }

    // Did either processing pending expired timers or removing
    // this timer necessitate a reprogramming of the quantum timer?
    if (a_new_head && (NULL != nsvc_timer_quantum_device_reconfigure_fcn_ptr))
    {
        // Reconfigure to new timer value
        if (nsvc_timer_queue_length > 0)
        {
            // Adjust quantum timeout
            (*nsvc_timer_quantum_device_reconfigure_fcn_ptr)(
                                        nsvc_timer_queue_head->duration);
        }
        // Timer that was killed was only active timer? Stop quantum timer
        else
        {
            (*nsvc_timer_quantum_device_reconfigure_fcn_ptr)(0);        
        }
    }

    // It's safe for IRQ handlers/SysTick to touch timer lists now.
    nsvc_timer_queue_update_in_progress = false;

    nufr_unprioritize();

    // Restore priority
    nufr_running->priority_restore_prioritized = saved_task_priority;

    return is_timer_active;
}

//!
//! @name      nsvc_timer_next_expiration_callin
//!
//! @brief     Callin to handle timer expirations
//!
//! @details   Called from one of the following:
//! @details     1) The OS Tick handler
//! @details     2) An IRQ timer handler (quantum timer)
//! @details     3) A high-priority task. Must be higher priority
//! @details        than any other task using app timers.
//! @details   
//! @details   This routine checks for expired timers
//! @details   and processes them. It can be called from a quantum
//! @details   timer, which is a H/W timer source that is
//! @details   configured to expire at the next app timer's timeout.
//! @details   It can also be called from OS Tick handler
//! @details   
//! @details   Regardless of where it is called from, the caller
//! @details   passes in the current time from the 32-bit H/W
//! @details   time reference. It compares current time against
//! @details   last known time, and considers any timers falling
//! @details   within this window to have expired.
//! @details   
//! @details   Expired timers are taken off the active timer
//! @details   list and placed on the expired timer list.
//! @details   The expired timer list is then processed/drained.
//! @details   Each expired timer has its message sent.
//! @details   If an expired timer is continuous, it is
//! @details   reset: placed back on the active timer list.
//! @details   
//! @details   For whatever actions are taken, a new interval
//! @details   to the next timeout is calculated. If the caller
//! @details   was a quantum timer, the quantum timer will be
//! @details   reconfigured to that delay. Otherwise, the return
//! @details   value may or may not be used by caller.
//! @details   
//! @details         WARNING!
//! @details   When using quantum timers, if returning 
//! @details   'NSVC_TCRTN_BACKOFF_QUANTUM_TIMER', then caller
//! @details   should reconfigure timer to a short (1 millisec?)
//! @details   timeout, but ONLY IF no invocation was made to
//! @details   'nsvc_timer_quantum_device_reconfigure_fcn_ptr()'
//! @details   when 'nsvc_timer_queue_update_in_progress' was set.
//! @details   Otherwise, could get erroneous timer reconfiguration.
//!
//! @param[in] 'current_time'-- capture of 32-bit H/W time reference.
//! @param[out] 'reconfigured_time_ptr'-- time in millisecs to
//! @param[out]    set quantum timer to, if return value is
//! @param[out]    NSVC_TCRTN_RECONFIGURE_QUANTUM_TIMER.
//!
//! @return    Action for quantum timer, or perhaps OS tick handler,
//! @return    to take:
//! @return     'NSVC_TCRTN_DISABLE_QUANTUM_TIMER'-- halt quantum timer
//! @return     'NSVC_TCRTN_RECONFIGURE_QUANTUM_TIMER'-- set quantum
//! @return         timeout to new value.
//! @return     'NSVC_TCRTN_BACKOFF_QUANTUM_TIMER'-- SL timer module busy
//! @return         at task level. Call back in again soon to complete action.
//!
// Can't use below--will cause circular include problem!!
//nsvc_timer_callin_return_t nsvc_timer_expire_timer_callin(
uint8_t nsvc_timer_expire_timer_callin(
                                    uint32_t      current_time,
                                    uint32_t     *reconfigured_time_ptr)
{
    nsvc_timer_callin_return_t rv;
    uint32_t                   previous_time;
    uint32_t                   expiration_time;
    bool                       a_new_head;

    // If 'nsvc_timer_queue_update_in_progress' is set, tell
    // caller to do a spin-lock equivalent.
    if (nsvc_timer_queue_update_in_progress)
    {
        *reconfigured_time_ptr = 1;

        return NSVC_TCRTN_BACKOFF_QUANTUM_TIMER;
    }

    // Update 'nsvc_timer_latest_time'
    previous_time = nsvc_timer_latest_time;
    nsvc_timer_latest_time = current_time;

    // Any timers expired?
    a_new_head = sl_timer_check_and_expire(previous_time) > 0;

    // 'a_new_head'==true when the next active timer changed,
    // also if there is no longer an active timer.
    a_new_head |= sl_timer_process_expired_timers();

    // Calculate 'reconfigured_time_ptr':
    //   interval to next active timer's timeout
    if (a_new_head && (nsvc_timer_queue_length > 0))
    {
        expiration_time = nsvc_timer_queue_head->expiration_time;

        // This handles wrap case too
        *reconfigured_time_ptr = expiration_time - nsvc_timer_latest_time;

        rv = NSVC_TCRTN_RECONFIGURE_QUANTUM_TIMER;
    }
    // No change in next timer to expire
    else
    {
        *reconfigured_time_ptr = 0;

        rv = NSVC_TCRTN_DISABLE_QUANTUM_TIMER;
    }

    return rv;
}