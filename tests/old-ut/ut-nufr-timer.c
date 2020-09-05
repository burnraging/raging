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

//! @file    ut-nufr-timer.c
//! @authors Bernie Woodland
//! @date    11Aug17

// Tests file nufr-kernel-timer.c

#include "nufr-global.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-timer.h"

#include "raging-contract.h"

#include <stdio.h>
#include <string.h>


typedef struct
{
    nufr_tid_t  tid;
    unsigned    timer_value;
} task_timer_pair;

void init_for_timer_tests(void)
{
    unsigned    i;
    nufr_tcb_t *tcb;

    // reset w.r.t. timer all tasks specified by nufr_tid_t
    for (i = NUFR_TID_01; i < NUFR_TID_max; i++)
    { 
        tcb = NUFR_TID_TO_TCB((nufr_tid_t)i);

        // Simulate having been launched
        // Not on timer list
        tcb->statuses &= BITWISE_NOT8(NUFR_TASK_NOT_LAUNCHED |
                                      NUFR_TASK_TIMER_RUNNING);
        tcb->notifications &= BITWISE_NOT8(NUFR_TASK_TIMEOUT);

        tcb->flink_timer = NULL;
        tcb->blink_timer = NULL;
    }

    nufr_timer_list = NULL;
    nufr_timer_list_tail = NULL;
}

// Ensure integrity of timer list:
//  walk list, check all pointers
void sanity_check_timer_list(void)
{
    bool        head_tail_same_null_state;
    bool        head_equals_tail;
    bool        not_empty;
    bool        single_tcb_on_list;
    nufr_tcb_t *this_tcb;
    nufr_tcb_t *next_tcb;
    unsigned    i;
    unsigned    list_size = 0;
    unsigned    null_linked_size = 0;
    unsigned    timer_running_count = 0;

    // If head is set, tail must be also
    // If head is not set, tail must not be also
    head_tail_same_null_state = (NULL == nufr_timer_list) ==
                                (NULL == nufr_timer_list_tail);
    head_equals_tail = nufr_timer_list == nufr_timer_list_tail;
    not_empty = NULL != nufr_timer_list;
    single_tcb_on_list = not_empty && head_equals_tail;

    UT_ENSURE(head_tail_same_null_state);

    // Count number of items on list
    this_tcb = nufr_timer_list;
    while (NULL != this_tcb)
    {
        list_size++;
        UT_ENSURE(list_size <= NUFR_NUM_TASKS);

        this_tcb = this_tcb->flink_timer;
    }

    // Count tasks which aren't linked
    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        this_tcb = &nufr_tcb_block[i];

        if ((NULL == this_tcb->flink_timer) && (NULL == this_tcb->blink_timer))
        {
            null_linked_size++;
        }
    }

    if (single_tcb_on_list)
        UT_ENSURE(null_linked_size + list_size - 1 == NUFR_NUM_TASKS);
    else
        UT_ENSURE(null_linked_size + list_size == NUFR_NUM_TASKS);

    if (not_empty)
    {
        // Head and tail's outward links must be null
        UT_ENSURE(NULL == nufr_timer_list->blink_timer);
        UT_ENSURE(NULL == nufr_timer_list_tail->flink_timer);

        // Check that blinks are correct
        this_tcb = nufr_timer_list;
        while (NULL != this_tcb)
        {
            next_tcb = this_tcb->flink_timer;

            if (NULL != next_tcb)
            {
                UT_ENSURE(this_tcb == next_tcb->blink_timer);
            }
            
            this_tcb = next_tcb;
        }
    }

    // Check NUFR_TASK_TIMER_RUNNING statuses
    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        this_tcb = &nufr_tcb_block[i];
        // Not on timer list?
        if (NUFR_IS_STATUS_CLR(this_tcb, NUFR_TASK_TIMER_RUNNING))
        {
            UT_ENSURE(NULL == this_tcb->flink_timer);
            UT_ENSURE(NULL == this_tcb->blink_timer);
        }
        // On timer list. Check null/not-null links
        else
        {
            timer_running_count++;

            if (this_tcb != nufr_timer_list)
            {
                UT_ENSURE(NULL != this_tcb->blink_timer);
            }

            if (this_tcb != nufr_timer_list_tail)
            {
                UT_ENSURE(NULL != this_tcb->flink_timer);
            }
        }
    }

    // Make sure num. tasks with NUFR_TASK_TIMER_RUNNING is same
    // as num. tasks on timer list.
    UT_ENSURE(list_size == timer_running_count);
}

// Verify if timer list is equal to 'match_list'
void match_timer_list(const nufr_tid_t *match_list, unsigned match_list_length)
{
    unsigned index = 0;
    nufr_tcb_t *tcb;
    nufr_tid_t  tid;

    sanity_check_timer_list();

    UT_REQUIRE(match_list_length > 0);

    tcb = nufr_timer_list;

    while (NULL != tcb)
    {
        UT_REQUIRE(index < match_list_length);

        tid = NUFR_TCB_TO_TID(tcb);
        UT_REQUIRE(match_list[index] == tid);

        index++;
        tcb = tcb->flink_timer;
    }

    UT_REQUIRE(index == match_list_length);
}

// Add list of tasks specified by 'timer_list' to
// tasks which are currently on the timer list
void add_tasks_to_timer_list(const nufr_tid_t *timer_list,
                             unsigned timer_list_length)
{
    unsigned     i;
    nufr_tcb_t  *tcb;

    // reset w.r.t. timer all tasks specified by nufr_tid_t
    for (i = 0; i < timer_list_length; i++)
    { 
        tcb = NUFR_TID_TO_TCB(timer_list[i]);
        UT_REQUIRE(NUFR_IS_TCB(tcb));

        nufrkernel_add_to_timer_list(tcb, 1);

        sanity_check_timer_list();
    }
}

void timer_increasing_decreasing_add_purge(void)
{
    nufr_tid_t match_list[] = {
        NUFR_TID_01,
        NUFR_TID_02,
        NUFR_TID_03,
        NUFR_TID_04,
        NUFR_TID_05,
        NUFR_TID_06,
        NUFR_TID_07,
        NUFR_TID_08,
        NUFR_TID_09,
        NUFR_TID_10,
        NUFR_TID_11,
        NUFR_TID_12,
        NUFR_TID_13,
        NUFR_TID_14,
        NUFR_TID_15,
        NUFR_TID_16,
        NUFR_TID_17,
        NUFR_TID_18,
        NUFR_TID_19,
        NUFR_TID_20
    };

    unsigned     i;
    nufr_tcb_t  *tcb;

    init_for_timer_tests();

    // make sure list has all tasks specified by nufr_tid_t
    UT_REQUIRE((match_list[0] - 1) == 0);
    UT_REQUIRE(ARRAY_SIZE(match_list) == NUFR_NUM_TASKS);

    // Add 1 task at at time, from first to last. Verify
    // each step
    for (i = NUFR_TID_01; i < NUFR_TID_max; i += 1)
    {
        tcb = NUFR_TID_TO_TCB((nufr_tid_t)i);
        nufrkernel_add_to_timer_list(tcb, 1);

        match_timer_list(match_list, i);
    }

    // Delete 1 task at at time, from first to last. Verify
    // each step
    for (i = NUFR_TID_01; i < NUFR_TID_max; i++)
    {
        tcb = NUFR_TID_TO_TCB((nufr_tid_t)i);
        nufrkernel_purge_from_timer_list(tcb);

        if (i != NUFR_TID_max - 1)
            match_timer_list(&match_list[i], ARRAY_SIZE(match_list) - i);
    }

    // Add tasks again, from first to last
    for (i = NUFR_TID_01; i < NUFR_TID_max; i++)
    {
        tcb = NUFR_TID_TO_TCB((nufr_tid_t)i);
        nufrkernel_add_to_timer_list(tcb, 1);

        match_timer_list(match_list, i);
    }

    // Delete 1 task at at time, from last to first.
    for (i = NUFR_TID_max - 1; i != NUFR_TID_null; i--)
    {
        tcb = NUFR_TID_TO_TCB((nufr_tid_t)i);
        nufrkernel_purge_from_timer_list(tcb);

        if (i != 1)
            match_timer_list(match_list, i - 1);
    }
}

// Same test as timer_increasing_decreasing_add_purge(),
//  except tasks are removed from timer list by
//  tickout, rather than by purge call.
void timer_increasing_decreasing_add_tickout(void)
{
    nufr_tid_t match_list[] = {
        NUFR_TID_01,
        NUFR_TID_02,
        NUFR_TID_03,
        NUFR_TID_04,
        NUFR_TID_05,
        NUFR_TID_06,
        NUFR_TID_07,
        NUFR_TID_08,
        NUFR_TID_09,
        NUFR_TID_10,
        NUFR_TID_11,
        NUFR_TID_12,
        NUFR_TID_13,
        NUFR_TID_14,
        NUFR_TID_15,
        NUFR_TID_16,
        NUFR_TID_17,
        NUFR_TID_18,
        NUFR_TID_19,
        NUFR_TID_20
    };

    unsigned     i;
    nufr_tcb_t  *tcb;

    init_for_timer_tests();

    // make sure list has all tasks specified by nufr_tid_t
    UT_REQUIRE((match_list[0] - 1) == 0);
    UT_REQUIRE(ARRAY_SIZE(match_list) == NUFR_NUM_TASKS);

    // Add 1 task at at time, from first to last. Verify
    // each step
    for (i = NUFR_TID_01; i < NUFR_TID_max; i += 1)
    {
        tcb = NUFR_TID_TO_TCB((nufr_tid_t)i);

        // Increment timer value for each task, so when tick handler is
        // called, tasks will timeout from first to last
        nufrkernel_add_to_timer_list(tcb, i);

        match_timer_list(match_list, i);
    }

    // Delete 1 task at at time, from first to last. Verify
    // each step
    for (i = NUFR_TID_01; i < NUFR_TID_max; i++)
    {
        tcb = NUFR_TID_TO_TCB((nufr_tid_t)i);

        // Each call decrements timer of all task on timer list
        nufrplat_systick_handler();

        if (i != NUFR_TID_max - 1)
            match_timer_list(&match_list[i], ARRAY_SIZE(match_list) - i);
    }

    // Add tasks again, from first to last
    for (i = NUFR_TID_01; i < NUFR_TID_max; i++)
    {
        tcb = NUFR_TID_TO_TCB((nufr_tid_t)i);
        nufrkernel_add_to_timer_list(tcb, NUFR_TID_max - i);

        match_timer_list(match_list, i);
    }

    // Delete 1 task at at time, from last to first.
    for (i = NUFR_TID_max - 1; i != NUFR_TID_null; i--)
    {
        tcb = NUFR_TID_TO_TCB((nufr_tid_t)i);

        nufrplat_systick_handler();

        if (i != 1)
            match_timer_list(match_list, i - 1);
    }
}

// randomly add and purge tasks from the middle of the timer list
void timer_random_add_purge(void)
{
    nufr_tid_t total_list[] = {
        NUFR_TID_01,
        NUFR_TID_02,
        NUFR_TID_03,
        NUFR_TID_04,
        NUFR_TID_05,
        NUFR_TID_06,
        NUFR_TID_07,
        NUFR_TID_08,
        NUFR_TID_09,
        NUFR_TID_10,
    };

    nufr_tid_t list_without2[] = {
        NUFR_TID_01,
        NUFR_TID_03,
        NUFR_TID_04,
        NUFR_TID_05,
        NUFR_TID_06,
        NUFR_TID_07,
        NUFR_TID_08,
        NUFR_TID_09,
        NUFR_TID_10,
    };
    
    nufr_tid_t list_without2_5_6[] = {
        NUFR_TID_01,
        NUFR_TID_03,
        NUFR_TID_04,
        NUFR_TID_07,
        NUFR_TID_08,
        NUFR_TID_09,
        NUFR_TID_10,
    };
    
    nufr_tid_t list_without2_with_6_5_back_in[] = {
        NUFR_TID_01,
        NUFR_TID_03,
        NUFR_TID_04,
        NUFR_TID_07,
        NUFR_TID_08,
        NUFR_TID_09,
        NUFR_TID_10,
        NUFR_TID_06,
        NUFR_TID_05,
    };
    

    unsigned     i;
    nufr_tcb_t  *tcb;
    bool         rv;

    init_for_timer_tests();

    // First, populate timer list with tasks
    for (i = 0; i < ARRAY_SIZE(total_list); i++)
    {
        tcb = NUFR_TID_TO_TCB(total_list[i]);
        nufrkernel_add_to_timer_list(tcb, 1);
    }

    // Randomly delete a task from middle
    tcb = NUFR_TID_TO_TCB(NUFR_TID_02);
    rv = nufrkernel_purge_from_timer_list(tcb);
    UT_ENSURE(rv);
    match_timer_list(list_without2, ARRAY_SIZE(list_without2));

    // Delete 2 more tasks from middle
    tcb = NUFR_TID_TO_TCB(NUFR_TID_05);
    rv = nufrkernel_purge_from_timer_list(tcb);
    UT_ENSURE(rv);
    tcb = NUFR_TID_TO_TCB(NUFR_TID_06);
    rv = nufrkernel_purge_from_timer_list(tcb);
    UT_ENSURE(rv);
    match_timer_list(list_without2_5_6, ARRAY_SIZE(list_without2_5_6));

    // Add 6 and 5 back in, in that order
    tcb = NUFR_TID_TO_TCB(NUFR_TID_06);
    nufrkernel_add_to_timer_list(tcb, 1);
    tcb = NUFR_TID_TO_TCB(NUFR_TID_05);
    nufrkernel_add_to_timer_list(tcb, 1);
    match_timer_list(list_without2_with_6_5_back_in,
                     ARRAY_SIZE(list_without2_with_6_5_back_in));

}

// same as above, except using a tickout instead of a purge call
void timer_random_add_tickout(void)
{
    task_timer_pair total_list[] = {
        {NUFR_TID_01, 10},
        {NUFR_TID_02, 10},
        {NUFR_TID_03, 1},
        {NUFR_TID_04, 10},
        {NUFR_TID_05, 10},
        {NUFR_TID_06, 10},
        {NUFR_TID_07, 3},   // timeout 8 before 7
        {NUFR_TID_08, 2},
        {NUFR_TID_09, 10},
        {NUFR_TID_10, 10},
    };

    nufr_tid_t list_without_3[] = {
        NUFR_TID_01,
        NUFR_TID_02,
        NUFR_TID_04,
        NUFR_TID_05,
        NUFR_TID_06,
        NUFR_TID_07,
        NUFR_TID_08,
        NUFR_TID_09,
        NUFR_TID_10,
    };
    
    nufr_tid_t list_without_3_7_8[] = {
        NUFR_TID_01,
        NUFR_TID_02,
        NUFR_TID_04,
        NUFR_TID_05,
        NUFR_TID_06,
        NUFR_TID_09,
        NUFR_TID_10,
    };
    
    nufr_tid_t list_without_3_with_7_8_back_in[] = {
        NUFR_TID_01,
        NUFR_TID_02,
        NUFR_TID_04,
        NUFR_TID_05,
        NUFR_TID_06,
        NUFR_TID_09,
        NUFR_TID_10,
        NUFR_TID_07,
        NUFR_TID_08,
    };
  
    nufr_tid_t list_with_7_8[] = {
        NUFR_TID_07,
        NUFR_TID_08,
    };
  

    unsigned     i;
    nufr_tcb_t  *tcb;

    init_for_timer_tests();

    // First, populate timer list with tasks
    for (i = 0; i < ARRAY_SIZE(total_list); i++)
    {
        tcb = NUFR_TID_TO_TCB(total_list[i].tid);
        nufrkernel_add_to_timer_list(tcb, total_list[i].timer_value);
    }

    // First tick will remove 03
    nufrplat_systick_handler();
    match_timer_list(list_without_3, ARRAY_SIZE(list_without_3));

    // Next tick will remove 08, 07
    nufrplat_systick_handler();
    sanity_check_timer_list();
    nufrplat_systick_handler();
    sanity_check_timer_list();
    match_timer_list(list_without_3_7_8, ARRAY_SIZE(list_without_3_7_8));

    // Add 7, 8 back in, in that order. Add original timeout back in.
    tcb = NUFR_TID_TO_TCB(NUFR_TID_07);
    nufrkernel_add_to_timer_list(tcb, total_list[i].timer_value);
    sanity_check_timer_list();
    tcb = NUFR_TID_TO_TCB(NUFR_TID_08);
    nufrkernel_add_to_timer_list(tcb, total_list[i].timer_value);
    match_timer_list(list_without_3_with_7_8_back_in, ARRAY_SIZE(list_without_3_with_7_8_back_in));

    // this should tick out all the remaining original tasks
    for (i = 0; i < total_list[0].timer_value - 3; i++)
    {
        nufrplat_systick_handler();
        sanity_check_timer_list();
    }

    match_timer_list(list_with_7_8, ARRAY_SIZE(list_with_7_8));
}


void ut_timers(void)
{
    timer_increasing_decreasing_add_purge();
    timer_increasing_decreasing_add_tickout();
    timer_random_add_purge();
    timer_random_add_tickout();
}