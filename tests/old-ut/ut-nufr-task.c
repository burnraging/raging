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

//! @file    ut-nufr-task.c
//! @authors Bernie Woodland
//! @date    11Aug17

#include "nufr-global.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-timer.h"

#include "raging-contract.h"

#include <stdio.h>
#include <string.h>


void sanity_check_timer_list(void);

// Clears all NUFR_TASK_NOT_LAUNCHED, but tasks
//   are left in limbo, as they're not blocked but not
//   in ready list.
static void clear_task_launched_bits(void)
{
    unsigned i;

    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        nufr_tcb_block[i].block_flags &= BITWISE_NOT8(NUFR_TASK_NOT_LAUNCHED);
    }
}

// Puts all task in blocked state, blocked on a bop
static void block_on_bop_all(void)
{
    unsigned i;

    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        nufr_tcb_block[i].block_flags |= NUFR_TASK_BLOCKED_BOP;
    }
}

// Returns 'true' if tcb address is a valid ptr to one
//  of the 'nufr_tcb_block' members
static bool is_tcb_in_block(nufr_tcb_t *tcb)
{
    bool     is_within;
    unsigned i;

    UT_ENSURE(NULL != tcb);

    if (NULL != tcb)
    {
        for (i = 0; i < NUFR_NUM_TASKS; i++)
        {
            is_within = tcb == &nufr_tcb_block[i];
            if (is_within)
                return true;
        }
    }

    return false;
}

// Return 'true' if up to 'match_length' entries in 'tid_list_ptr',
//  which is a list of task ids, overlay what's on ready list.
static bool match_tid_list_to_ready_list(nufr_tid_t *tid_list_ptr,
                                         unsigned    match_length)
{
	bool result = false;
    unsigned count = 0;
    nufr_tcb_t *tcb = nufr_ready_list;

    nufr_tid_t expected = NUFR_TID_null;
    nufr_tid_t actual = NUFR_TID_null;
    while (NULL != tcb)
    {
    	expected = tid_list_ptr[count];
    	actual = NUFR_TCB_TO_TID(tcb);

    	printf("Expected: %02u == %02u Actual\r\n",expected,actual);

        //UT_ENSURE(tid_list_ptr[count] == NUFR_TCB_TO_TID(tcb));

        count++;

        tcb = tcb->flink;
    }

    result = (match_length == count);

    return result;
}

static bool is_task_on_timer_list(nufr_tcb_t *tcb)
{
    nufr_tcb_t *this_tcb;
    bool        found = false;

    this_tcb = nufr_timer_list;

    while (NULL != this_tcb)
    {
        if (this_tcb == tcb)
        {
            found = true;
            break;
        }

        this_tcb = this_tcb->flink;
    }

    return found;
}

static bool is_task_on_ready_list(nufr_tcb_t *tcb)
{
    nufr_tcb_t *this_tcb;
    bool        found = false;

    this_tcb = nufr_ready_list;

    while (NULL != this_tcb)
    {
        if (this_tcb == tcb)
        {
            found = true;
            break;
        }

        this_tcb = this_tcb->flink;
    }

    return found;
}

// Make sure UT environment is sane
void ut_tcb_setup(void)
{
    unsigned                i;

    // Sanity on enums:

    // Task IDs (tids)
    UT_REQUIRE(NUFR_NUM_TASKS > 0);
    UT_REQUIRE(NUFR_NUM_TASKS == (NUFR_TID_max - NUFR_TID_null - 1));
    UT_REQUIRE(NUFR_TID_max > NUFR_TID_null + 1);
    UT_REQUIRE(NUFR_TID_null == 0);

    // Task priorities
    UT_REQUIRE(NUFR_TPR_null == 0);
    UT_REQUIRE(NUFR_TPR_guaranteed_highest > NUFR_TPR_null);
    UT_REQUIRE(NUFR_TPR_NOMINAL > NUFR_TPR_guaranteed_highest);


    for (i = NUFR_TID_null + 1; i < NUFR_TID_max; i++)
    {
        nufr_tid_t tid = (nufr_tid_t) i;
        nufr_tcb_t *tcb = NUFR_TID_TO_TCB(tid);
        const nufr_task_desc_t *desc_ptr1;
        const nufr_task_desc_t *desc_ptr2;
        const nufr_task_desc_t *desc_ptr;

        // Verify NUFR_TID_TO_TCB macro
        UT_ENSURE(tcb != NULL);
        // Verify 'tcb' fails within address range of 'nufr_tcb_block'
        UT_ENSURE(is_tcb_in_block(tcb));

        // Verify that 'nufrplat_task_get_desc' gives same results
        //   in both modes of parameter format.
        desc_ptr1 = nufrplat_task_get_desc(NULL, tid);
        desc_ptr2 = nufrplat_task_get_desc(tcb, NUFR_TID_null);
        UT_ENSURE(desc_ptr1 == desc_ptr2);

        // Verify that 'nufrplat_task_get_desc' entry is found for this tid
        desc_ptr = nufrplat_task_get_desc(NULL, tid);
        UT_ENSURE(desc_ptr != NULL);

        // Task's assigned priority must be sane
        UT_ENSURE(desc_ptr->start_priority > NUFR_TPR_guaranteed_highest);
    }

}

// Verify TCBs are in init state
void ut_tcb_init(nufr_tcb_t *tcb)
{
    bool not_launched = (tcb->block_flags & NUFR_TASK_NOT_LAUNCHED) != 0;
    bool blocked_asleep = (tcb->block_flags & NUFR_TASK_BLOCKED_ASLEEP) != 0;
    bool blocked_bop = (tcb->block_flags & NUFR_TASK_BLOCKED_BOP) != 0;
    bool blocked_msg = (tcb->block_flags & NUFR_TASK_BLOCKED_MSG) != 0;
    bool blocked_sema = (tcb->block_flags & NUFR_TASK_BLOCKED_SEMA) != 0;
    bool timer_running = (tcb->statuses & NUFR_TASK_TIMER_RUNNING) != 0;
    bool bop_locked = (tcb->statuses & NUFR_TASK_BOP_LOCKED) != 0;
    bool timeout = (tcb->notifications & NUFR_TASK_TIMEOUT) != 0;
    bool unblocked_by_msg_send = (tcb->notifications &
                                  NUFR_TASK_UNBLOCKED_BY_MSG_SEND) != 0;

    bool ored = blocked_asleep || blocked_bop ||
                blocked_msg ||
                blocked_sema || timer_running ||
                bop_locked ||
                timeout || unblocked_by_msg_send;

    UT_ENSURE(not_launched && !ored);
}

// TCB sanity: no matter what config..assumes launched
void tcb_sanity(nufr_tcb_t *tcb)
{
    unsigned flag_sum = 0;
    bool     unblocked;

    bool not_launched = (tcb->block_flags & NUFR_TASK_NOT_LAUNCHED) != 0;
    bool blocked_asleep = (tcb->block_flags & NUFR_TASK_BLOCKED_ASLEEP) != 0;
    bool blocked_bop = (tcb->block_flags & NUFR_TASK_BLOCKED_BOP) != 0;
    bool blocked_msg = (tcb->block_flags & NUFR_TASK_BLOCKED_MSG) != 0;
    bool blocked_sema = (tcb->block_flags & NUFR_TASK_BLOCKED_SEMA) != 0;
    bool timer_running = (tcb->statuses & NUFR_TASK_TIMER_RUNNING) != 0;
    bool bop_locked = (tcb->statuses & NUFR_TASK_BOP_LOCKED) != 0;
    bool timeout = (tcb->notifications & NUFR_TASK_TIMEOUT) != 0;
    bool unblocked_by_msg_send = (tcb->notifications &
                                  NUFR_TASK_UNBLOCKED_BY_MSG_SEND) != 0;

    UNUSED(timeout);
    UNUSED(unblocked_by_msg_send);

    UT_REQUIRE(!not_launched);

    flag_sum += blocked_asleep? 1 : 0;
    flag_sum += blocked_bop? 1 : 0;
    flag_sum += blocked_msg? 1 : 0;
    flag_sum += blocked_sema? 1 : 0;

    unblocked = flag_sum == 0;

    // Sanity check 'NUFR_IS_TASK_BLOCKED'
    UT_ENSURE(unblocked == NUFR_IS_TASK_NOT_BLOCKED(tcb));

    // If blocked, can only block on one thing at a time
    UT_ENSURE(unblocked || (flag_sum == 1));

    if (unblocked)
    {
        // Can't have any status flags set while ready
        UT_ENSURE(!(timer_running || bop_locked));
    }
    else
    {
    }
}

void tcb_sanity_all(void)
{
    unsigned i;

    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        tcb_sanity(&nufr_tcb_block[i]);
    }
}

// Basic ready list checks that can be applied at any time,
//   except if OS hasn't been initialized.
void ready_list_sanity(void)
{
    unsigned i;
    unsigned num_nominal_tasks = 0;
    unsigned num_tasks_blocked = 0;
    unsigned num_nominals_blocked = 0;
    unsigned num_tasks_ready;
    unsigned num_nominal_tasks_ready;
    nufr_tcb_t *this_tcb;
    nufr_tcb_t *next_tcb;


    // Consistency in head/tail/nominal ptrs being NULL/not NULL
    UT_ENSURE((nufr_ready_list != NULL) == (nufr_ready_list_tail != NULL));
    if (nufr_ready_list_tail_nominal != NULL)
    {
        UT_ENSURE(nufr_ready_list != NULL);
    }

    // Calculate number of blocked tasks
    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        nufr_tcb_t *tcb = &nufr_tcb_block[i];

        num_tasks_blocked += NUFR_IS_TASK_BLOCKED(tcb)? 1 : 0;

        num_nominals_blocked += (NUFR_IS_TASK_BLOCKED(tcb) &&
                         tcb->priority == NUFR_TPR_NOMINAL) ?
                                1 : 0;

        num_nominal_tasks += NUFR_TPR_NOMINAL == tcb->priority;
    }

    // Nominal blocks are a subset of total number of blocks
    UT_ENSURE(num_nominals_blocked <= num_tasks_blocked);
    if (num_nominals_blocked > 0)
    {
        UT_ENSURE(num_tasks_blocked > 0);
    }

    num_tasks_ready = NUFR_NUM_TASKS - num_tasks_blocked;
    num_nominal_tasks_ready = num_nominal_tasks - num_nominals_blocked;

    // If any task is ready, head should be non-NULL
    if (num_tasks_ready > 0)
    {
        UT_ENSURE(nufr_ready_list != NULL);
    }

    // If any nominal is ready, should have its tail set
    if (num_nominal_tasks_ready > 0)
    {
         UT_ENSURE(nufr_ready_list_tail_nominal != NULL);
    }

    // One task only ready?
    if (1 == num_tasks_ready)
    {
        // Head and tail pointers should be the same
        UT_ENSURE(nufr_ready_list == nufr_ready_list_tail);

        if (num_nominal_tasks_ready == 1)
        {
            UT_ENSURE(num_nominal_tasks_ready == num_tasks_ready);

            UT_ENSURE(nufr_ready_list == nufr_ready_list_tail_nominal);
        }
    }
    // Multiple tasks ready?
    else if (num_tasks_ready > 1)
    {
        // Head and tail must point to different TCBs
        UT_ENSURE(nufr_ready_list != nufr_ready_list_tail);

        // Same for nominals
        if (num_nominal_tasks_ready > 1)
        {
            UT_ENSURE(nufr_ready_list != nufr_ready_list_tail_nominal);
        }
    }

    // Head and tails must be addresses in TCB block array
    if (nufr_ready_list != NULL)
    {
        UT_ENSURE(is_tcb_in_block(nufr_ready_list));
    }
    if (nufr_ready_list_tail != NULL)
    {
        UT_ENSURE(is_tcb_in_block(nufr_ready_list_tail));
    }
    if (nufr_ready_list_tail_nominal != NULL)
    {
        UT_ENSURE(is_tcb_in_block(nufr_ready_list_tail_nominal));
    }

    // Walk ready list:
    //   Check TCB pointers and flinks
    this_tcb = nufr_ready_list;
    for (i = 0; i < num_tasks_ready; i++)
    {
        UT_ENSURE(is_tcb_in_block(this_tcb));

        UT_ENSURE(NULL != this_tcb);
        
        next_tcb = this_tcb->flink;

        // Last iteration of loop?
        if (num_tasks_ready - 1 == i)
        {
            UT_ENSURE(NULL == next_tcb);

            // Verify that tail points to last TCB in list
            UT_ENSURE(nufr_ready_list_tail == this_tcb);
        }
        else
        {
            UT_ENSURE(is_tcb_in_block(next_tcb));
        }

        this_tcb = next_tcb;
    }

    // Walk ready list:
    //    Check descending priority levels
    //    Count ready tasks at nominal priority level
    this_tcb = nufr_ready_list;
    for (i = 0; i < num_tasks_ready; i++)
    {
        // Check for illegal priority
        UT_ENSURE(this_tcb->priority != NUFR_TPR_null);

        next_tcb = this_tcb->flink;

        // Not Last iteration of loop?
        if (i < num_tasks_ready - 1)
        {
            UT_ENSURE(this_tcb->priority <= next_tcb->priority);
        }

        this_tcb = next_tcb;
    }

    // If no nominal tasks on ready list, nominal tail should be NULL
    if (0 == num_nominal_tasks_ready)
    {
        UT_ENSURE(NULL == nufr_ready_list_tail_nominal);
    }
    // If all tasks are nominal priority, then the 2 tails should
    //   point to the same TCB.
    else if (num_nominal_tasks_ready == num_tasks_ready)
    {
        UT_ENSURE(nufr_ready_list_tail_nominal == nufr_ready_list_tail);
    }

    // Walk ready list:
    //    Check that nominal tail points to last nominal
    this_tcb = nufr_ready_list;
    for (i = 0; i < num_tasks_ready; i++)
    {
        next_tcb = this_tcb->flink;

        if (this_tcb->priority == NUFR_TPR_NOMINAL)
        {
            // If this is last TCB on list, make sure tails match
            if (next_tcb == NULL)
            {
                UT_ENSURE(nufr_ready_list_tail_nominal == nufr_ready_list_tail);
            }
            // Is this the last nominal TCB? Tail must be right
            else if (next_tcb->priority != NUFR_TPR_NOMINAL)
            {
                UT_ENSURE(nufr_ready_list_tail_nominal == this_tcb);
            }
        }

        this_tcb = next_tcb;
    }            
}

static void test_preliminaries(void)
{
    //###
    //### Setup
    //###
    ut_tcb_setup();

    nufr_init();
    clear_task_launched_bits();
    tcb_sanity_all();
}

nufr_tid_t exercise_ready_list1[] = {
    NUFR_TID_01, NUFR_TID_02, NUFR_TID_03, NUFR_TID_04, NUFR_TID_05,
    NUFR_TID_06, NUFR_TID_07, NUFR_TID_08, NUFR_TID_09, NUFR_TID_10,
    NUFR_TID_11, NUFR_TID_12, NUFR_TID_13, NUFR_TID_14, NUFR_TID_15,
    NUFR_TID_16, NUFR_TID_17, NUFR_TID_18, NUFR_TID_19, NUFR_TID_20
};

nufr_tid_t exercise_ready_list1a[] = {
    NUFR_TID_02, NUFR_TID_01, NUFR_TID_04, NUFR_TID_03, NUFR_TID_06,
    NUFR_TID_05
};

static void test_exercise_ready_list1(void)
{
    unsigned i;

    //....   re-init ....
    nufr_init();
    clear_task_launched_bits();
    tcb_sanity_all();

    // Add all tasks to ready list. Will be added in
    //   order of list.
    for (i = 0; i < ARRAY_SIZE(exercise_ready_list1); i++)
    {
        nufrkernel_add_task_to_ready_list(NUFR_TID_TO_TCB(exercise_ready_list1[i]));

        // 	Verify
        //	Need to add 1 to the index so that this actually returns
        //	true
        printf("Index: %u\r\n",i);
        printf("=========================================\r\n");
        match_tid_list_to_ready_list(exercise_ready_list1, (i));
        printf("=========================================\r\n");
    }

    // Verify
    tcb_sanity_all();
    ready_list_sanity();

    // Remove tasks starting from end of ready list to beginning
    for (i = 0; i < ARRAY_SIZE(exercise_ready_list1); i++)
    {
        unsigned k = ARRAY_SIZE(exercise_ready_list1) - i - 1;
        nufr_tcb_t *tcb = NUFR_TID_TO_TCB(exercise_ready_list1[k]);

        nufrkernel_delete_task_from_ready_list(tcb);

        // Must set a blocked bit to simulate blocked
        tcb->block_flags |= NUFR_TASK_BLOCKED_BOP;

        // Verify
        tcb_sanity_all();
        ready_list_sanity();
        match_tid_list_to_ready_list(exercise_ready_list1, k);
    }

    // Add high priority tasks in reverse order
    for (i = 1; i < NUFR_TID_06 + 1; i++)
    {
        unsigned k = NUFR_TID_06 - i;
        nufr_tid_t tid = exercise_ready_list1[k];
        nufr_tcb_t *tcb = NUFR_TID_TO_TCB(tid);

        // Must unblock bit manually set in previous loop
        tcb->block_flags |= NUFR_TASK_BLOCKED_BOP;

        nufrkernel_add_task_to_ready_list(tcb);
    }

    // Verify that high priority tasks got inserted at head, not tail
    tcb_sanity_all();
    ready_list_sanity();
    match_tid_list_to_ready_list(exercise_ready_list1a,
                                 ARRAY_SIZE(exercise_ready_list1a));
    UT_ENSURE(0 == ut_interrupt_count);

    // Now block these tasks one at a time
    for (i = 1; i < NUFR_TID_06 + 1; i++)
    {
        unsigned k = i - 1;
        match_tid_list_to_ready_list(&exercise_ready_list1a[k],
                                     ARRAY_SIZE(exercise_ready_list1a) - k);

        nufrkernel_block_running_task(NUFR_TASK_BLOCKED_BOP);
#ifdef RUNTIME_TESTS
        NUFR_INVOKE_CONTEXT_SWITCH();
#endif

        tcb_sanity_all();
        ready_list_sanity();
    }
}

#ifdef RUNTIME_TESTS
static void test_launch(void)
{
    nufr_launch_task(NUFR_TID_01, 10);
}

static void test_bop_contrived(void)
{
    //....   re-init ....
    nufr_init();
    clear_task_launched_bits();
    tcb_sanity_all();

    //###
    //### Block on bop, bypassing API calls
    //###
    block_on_bop_all();

    // verify
    ready_list_sanity();
}

static void test_bop_basic(void)
{
    unsigned k;

    //....   re-init ....
    nufr_init();
    clear_task_launched_bits();
    tcb_sanity_all();

    //###
    //### Block all tasks on bop, using API calls
    //###
    for (k = 0; k < NUFR_NUM_TASKS; k++)
    {
        nufr_bop_wait_rtn_t bop_rc;

        // Simulate running in context of corresponding task
        // Required for most API calls.
        nufr_running = NUFR_TID_TO_TCB(k + 1);
        nufrkernel_add_task_to_ready_list(nufr_running);

        // API call
        bop_rc = nufr_bop_waitW(NUFR_MSG_PRI_CONTROL);

        // First level checks
        UT_ENSURE(NUFR_BOP_WAIT_OK == bop_rc);
        UT_ENSURE(0 == ut_interrupt_count);
    }

    tcb_sanity_all();
    ready_list_sanity();

    //###
    //### Unblock all tasks one at a time,
    //###   from lowest to highest priorities.
    //###
    for (k = NUFR_NUM_TASKS; k >= 1; k--)
    {
        nufr_bop_rtn_t bop_rc;

        bop_rc = nufr_bop_send_with_key_override((nufr_tid_t)k);

        // First level checks
        UT_ENSURE(NUFR_BOP_RTN_TAKEN == bop_rc);
        UT_ENSURE(0 == ut_interrupt_count);

        tcb_sanity_all();
        ready_list_sanity();
    }
}

static void test_bop_keyed(void)
{
    uint16_t keys[NUFR_NUM_TASKS];
    unsigned i;

    //....   re-init ....
    nufr_init();
    clear_task_launched_bits();
    tcb_sanity_all();

    // Simulate being in BG task
    // Required for API call.
    nufr_running = (nufr_tcb_t *)nufr_bg_sp;

    //###
    //### Block all tasks on bop, using API calls
    //###
    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        nufr_bop_wait_rtn_t bop_rc;

        // API call
        keys[i] = nufr_bop_get_key();

        bop_rc = nufr_bop_waitW(NUFR_MSG_PRI_CONTROL);

        // First level checks
        UT_ENSURE(NUFR_BOP_WAIT_OK == bop_rc);
        UT_ENSURE(0 == ut_interrupt_count);
    }

    tcb_sanity_all();
    ready_list_sanity();

    //###
    //### Unblock all tasks one at a time
    //###
    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        nufr_bop_rtn_t bop_rc;
        nufr_tid_t tid = (nufr_tid_t)(i + 1);

        bop_rc = nufr_bop_send(tid, keys[i]);

        // Checks
        UT_ENSURE(NUFR_BOP_RTN_TAKEN == bop_rc);
        UT_ENSURE(NUFR_IS_TASK_NOT_BLOCKED(NUFR_TID_TO_TCB(tid)));
        UT_ENSURE(0 == ut_interrupt_count);

        tcb_sanity_all();
        ready_list_sanity();
    }
}

static void test_sleep(void)
{
    nufr_tcb_t *tcb1;
    nufr_tcb_t *tcb2;
    nufr_tcb_t *tcb3;

    //....   re-init ....
    nufr_init();
    clear_task_launched_bits();
    tcb_sanity_all();

    tcb3 = &nufr_tcb_block[NUFR_TID_03 - 1];
    tcb2 = &nufr_tcb_block[NUFR_TID_02 - 1];
    tcb1 = &nufr_tcb_block[NUFR_TID_01 - 1];

    // Add tasks to ready list, so sleep can remove them
    nufrkernel_add_task_to_ready_list(tcb3);
    nufrkernel_add_task_to_ready_list(tcb2);
    nufrkernel_add_task_to_ready_list(tcb1);

    // Set 'nufr_running' to simulate execution in sleep task's context,
    //   then call API. Stagger tasks to tick out at different times.
    nufr_running = nufr_ready_list;
    nufr_sleep(1, (nufr_msg_pri_t)0);

    nufr_running = nufr_ready_list;
    nufr_sleep(2, (nufr_msg_pri_t)0);

    nufr_running = nufr_ready_list;
    nufr_sleep(3, (nufr_msg_pri_t)0);

    // Verify
    UT_ENSURE(NULL == nufr_ready_list);
    UT_ENSURE(1 == tcb1->timer);
    UT_ENSURE(2 == tcb2->timer);
    UT_ENSURE(3 == tcb3->timer);
    sanity_check_timer_list();
    UT_ENSURE(is_task_on_timer_list(tcb1));
    UT_ENSURE(is_task_on_timer_list(tcb2));
    UT_ENSURE(is_task_on_timer_list(tcb3));
    UT_ENSURE(!is_task_on_ready_list(tcb1));
    UT_ENSURE(!is_task_on_ready_list(tcb2));
    UT_ENSURE(!is_task_on_ready_list(tcb3));
    UT_ENSURE((tcb1->block_flags & NUFR_TASK_BLOCKED_ASLEEP) != 0);
    UT_ENSURE((tcb1->statuses & NUFR_TASK_TIMER_RUNNING) != 0);
    UT_ENSURE((tcb1->notifications & NUFR_TASK_TIMEOUT) == 0);
    UT_ENSURE((tcb2->block_flags & NUFR_TASK_BLOCKED_ASLEEP) != 0);
    UT_ENSURE((tcb2->statuses & NUFR_TASK_TIMER_RUNNING) != 0);
    UT_ENSURE((tcb2->notifications & NUFR_TASK_TIMEOUT) == 0);
    UT_ENSURE((tcb3->block_flags & NUFR_TASK_BLOCKED_ASLEEP) != 0);
    UT_ENSURE((tcb3->statuses & NUFR_TASK_TIMER_RUNNING) != 0);
    UT_ENSURE((tcb3->notifications & NUFR_TASK_TIMEOUT) == 0);

    // Simulate a clock tick
    nufrplat_systick_handler();

    // tcb1 should had its timer expire
    // tcb2,3 are still running
    UT_ENSURE(0 == tcb1->timer);
    UT_ENSURE(1 == tcb2->timer);
    UT_ENSURE(2 == tcb3->timer);
    sanity_check_timer_list();
    UT_ENSURE(!is_task_on_timer_list(tcb1));
    UT_ENSURE(is_task_on_timer_list(tcb2));
    UT_ENSURE(is_task_on_timer_list(tcb3));
    UT_ENSURE(is_task_on_ready_list(tcb1));
    UT_ENSURE(!is_task_on_ready_list(tcb2));
    UT_ENSURE(!is_task_on_ready_list(tcb3));
    UT_ENSURE(NUFR_IS_TASK_NOT_BLOCKED(tcb1));
    UT_ENSURE(NUFR_IS_TASK_BLOCKED(tcb2));
    UT_ENSURE(NUFR_IS_TASK_BLOCKED(tcb3));
    UT_ENSURE((tcb1->block_flags & NUFR_TASK_BLOCKED_ASLEEP) == 0);
    UT_ENSURE((tcb1->statuses & NUFR_TASK_TIMER_RUNNING) == 0);
    UT_ENSURE((tcb1->notifications & NUFR_TASK_TIMEOUT) != 0);
    UT_ENSURE((tcb2->block_flags & NUFR_TASK_BLOCKED_ASLEEP) != 0);
    UT_ENSURE((tcb2->statuses & NUFR_TASK_TIMER_RUNNING) != 0);
    UT_ENSURE((tcb2->notifications & NUFR_TASK_TIMEOUT) == 0);
    UT_ENSURE((tcb3->block_flags & NUFR_TASK_BLOCKED_ASLEEP) != 0);
    UT_ENSURE((tcb3->statuses & NUFR_TASK_TIMER_RUNNING) != 0);
    UT_ENSURE((tcb3->notifications & NUFR_TASK_TIMEOUT) == 0);

    // Simulate a clock tick
    nufrplat_systick_handler();

    // tcb1,2 should had its timer expire
    // tcb3 still running
    UT_ENSURE(0 == tcb1->timer);
    UT_ENSURE(0 == tcb2->timer);
    UT_ENSURE(1 == tcb3->timer);
    sanity_check_timer_list();
    UT_ENSURE(!is_task_on_timer_list(tcb1));
    UT_ENSURE(!is_task_on_timer_list(tcb2));
    UT_ENSURE(is_task_on_timer_list(tcb3));
    UT_ENSURE(is_task_on_ready_list(tcb1));
    UT_ENSURE(is_task_on_ready_list(tcb2));
    UT_ENSURE(!is_task_on_ready_list(tcb3));
    UT_ENSURE(NUFR_IS_TASK_NOT_BLOCKED(tcb1));
    UT_ENSURE(NUFR_IS_TASK_NOT_BLOCKED(tcb2));
    UT_ENSURE(NUFR_IS_TASK_BLOCKED(tcb3));
    UT_ENSURE((tcb1->block_flags & NUFR_TASK_BLOCKED_ASLEEP) == 0);
    UT_ENSURE((tcb1->statuses & NUFR_TASK_TIMER_RUNNING) == 0);
    UT_ENSURE((tcb1->notifications & NUFR_TASK_TIMEOUT) != 0);
    UT_ENSURE((tcb2->block_flags & NUFR_TASK_BLOCKED_ASLEEP) == 0);
    UT_ENSURE((tcb2->statuses & NUFR_TASK_TIMER_RUNNING) == 0);
    UT_ENSURE((tcb2->notifications & NUFR_TASK_TIMEOUT) != 0);
    UT_ENSURE((tcb3->block_flags & NUFR_TASK_BLOCKED_ASLEEP) != 0);
    UT_ENSURE((tcb3->statuses & NUFR_TASK_TIMER_RUNNING) != 0);
    UT_ENSURE((tcb3->notifications & NUFR_TASK_TIMEOUT) == 0);

    // Simulate a clock tick
    nufrplat_systick_handler();

    // tcb1,2,3 should had its timer expire
    UT_ENSURE(0 == tcb1->timer);
    UT_ENSURE(0 == tcb2->timer);
    UT_ENSURE(0 == tcb3->timer);
    sanity_check_timer_list();
    UT_ENSURE(!is_task_on_timer_list(tcb1));
    UT_ENSURE(!is_task_on_timer_list(tcb2));
    UT_ENSURE(!is_task_on_timer_list(tcb3));
    UT_ENSURE(is_task_on_ready_list(tcb1));
    UT_ENSURE(is_task_on_ready_list(tcb2));
    UT_ENSURE(is_task_on_ready_list(tcb3));
    UT_ENSURE(NUFR_IS_TASK_NOT_BLOCKED(tcb1));
    UT_ENSURE(NUFR_IS_TASK_NOT_BLOCKED(tcb2));
    UT_ENSURE(NUFR_IS_TASK_NOT_BLOCKED(tcb3));
    UT_ENSURE((tcb1->block_flags & NUFR_TASK_BLOCKED_ASLEEP) == 0);
    UT_ENSURE((tcb1->statuses & NUFR_TASK_TIMER_RUNNING) == 0);
    UT_ENSURE((tcb1->notifications & NUFR_TASK_TIMEOUT) != 0);
    UT_ENSURE((tcb2->block_flags & NUFR_TASK_BLOCKED_ASLEEP) == 0);
    UT_ENSURE((tcb2->statuses & NUFR_TASK_TIMER_RUNNING) == 0);
    UT_ENSURE((tcb2->notifications & NUFR_TASK_TIMEOUT) != 0);
    UT_ENSURE((tcb3->block_flags & NUFR_TASK_BLOCKED_ASLEEP) == 0);
    UT_ENSURE((tcb3->statuses & NUFR_TASK_TIMER_RUNNING) == 0);
    UT_ENSURE((tcb3->notifications & NUFR_TASK_TIMEOUT) != 0);
}
#endif  // RUNTIME_TESTS

// !!!! these are repeated in ./tests/ready_list_delete_tests.c, etc!!!
// !!!!  should probably delete here !!!
#if 0
//  Ready List Insert Unit Tests

void ut_clean_list(void)
{
    nufr_ready_list = nufr_ready_list_tail = nufr_ready_list_tail_nominal = NULL;
}

void ut_clean_tcbs(void)
{
    memset(nufr_tcb_block, 0, sizeof(nufr_tcb_block));
}

bool ut_insert_before_head(void)
{
    bool result = false;
    
    nufr_tcb_t *tcb_head = &nufr_tcb_block[0];
    nufr_tcb_t *tcb_new_head = &nufr_tcb_block[1];

    tcb_head->priority = 4;
    tcb_new_head->priority = 2;
        
    nufrkernel_add_task_to_ready_list(tcb_head);
    nufrkernel_add_task_to_ready_list(tcb_new_head);
    
    if( (NULL != nufr_ready_list) && (NULL == nufr_ready_list_tail_nominal) && (NULL != nufr_ready_list_tail))
    {
        if((nufr_ready_list == tcb_new_head) && (nufr_ready_list_tail == tcb_head))
        {
            result = true;
        }
    }
    
    ut_clean_tcbs();

    return result;
}

bool ut_insert_after_before_nominal_with_no_nominal_set(void)
{
    bool result = false;
    
    nufr_tcb_t *tcb_head = &nufr_tcb_block[0];
    nufr_tcb_t *tcb_before_nominal = &nufr_tcb_block[1];
    tcb_head->priority = 2;
    tcb_before_nominal->priority = NUFR_TPR_NOMINAL - 1;
    
    nufrkernel_add_task_to_ready_list(tcb_head);
    nufrkernel_add_task_to_ready_list(tcb_before_nominal);
    
    if((NULL != nufr_ready_list) && (NULL == nufr_ready_list_tail_nominal) && (NULL != nufr_ready_list_tail))
    {
        if((nufr_ready_list->flink == tcb_before_nominal) && (nufr_ready_list_tail == tcb_before_nominal))
        {
            result = true;
        }
    }
    
    ut_clean_tcbs();

    return result;
}

bool ut_insert_after_nominal_before_end(void)
{
    bool result = false;
    
    nufr_tcb_t *tcb_head = &nufr_tcb_block[0]; 
    nufr_tcb_t *tcb_nominal = &nufr_tcb_block[1];
    nufr_tcb_t *tcb_between = &nufr_tcb_block[2];
    nufr_tcb_t *tcb_tail = &nufr_tcb_block[3];
    tcb_head->priority = 2; 
    tcb_nominal->priority = NUFR_TPR_NOMINAL;
    tcb_between->priority = NUFR_TPR_NOMINAL + 2;
    tcb_tail->priority = NUFR_TPR_NOMINAL + 4;
        
    nufrkernel_add_task_to_ready_list(tcb_head);
    nufrkernel_add_task_to_ready_list(tcb_nominal);
    nufrkernel_add_task_to_ready_list(tcb_tail);
    
    nufrkernel_add_task_to_ready_list(tcb_between);
    
    if((NULL != nufr_ready_list) && (NULL != nufr_ready_list_tail_nominal) && (NULL != nufr_ready_list_tail))
    {
        if((nufr_ready_list == tcb_head) && (nufr_ready_list_tail_nominal == tcb_nominal) && (nufr_ready_list_tail == tcb_tail))
        {
            if(nufr_ready_list_tail_nominal->flink == tcb_between)
            {
                result = true;
            }
        }
    }
    
    ut_clean_tcbs();

    return result;
}

bool ut_insert_before_nominal(void)
{
    bool result = false;
    
    nufr_tcb_t *tcb_head = &nufr_tcb_block[0];
    nufr_tcb_t *tcb_between = &nufr_tcb_block[1];
    nufr_tcb_t *tcb_tail = &nufr_tcb_block[2];
    tcb_head->priority = 2;
    tcb_between->priority = 4;
    tcb_tail->priority = NUFR_TPR_NOMINAL;
        
    nufrkernel_add_task_to_ready_list(tcb_head);
    nufrkernel_add_task_to_ready_list(tcb_tail);
    nufrkernel_add_task_to_ready_list(tcb_between);
    
    if((NULL != nufr_ready_list ) && (NULL != nufr_ready_list_tail) && (NULL != nufr_ready_list_tail_nominal))
    {
        if((nufr_ready_list == tcb_head) && (nufr_ready_list_tail == tcb_tail) && (nufr_ready_list_tail_nominal == tcb_tail))
        {
            if((NULL != nufr_ready_list->flink) && (nufr_ready_list->flink == tcb_between))
            {            
                result = true;
            }
        }
    }    
    
    ut_clean_tcbs();

    return result;
}

bool ut_insert_at_ready_list_tail(void)
{
    bool result = false;
    
    nufr_tcb_t *tcb_head = &nufr_tcb_block[0];
    nufr_tcb_t *tcb_tail = &nufr_tcb_block[1];
    tcb_head->priority = 2;
    tcb_tail->priority = 16;
        
    nufrkernel_add_task_to_ready_list(tcb_head);
    nufrkernel_add_task_to_ready_list(tcb_tail);
    
    if((NULL != nufr_ready_list ) && (NULL != nufr_ready_list_tail) && (NULL == nufr_ready_list_tail_nominal))
    {
        if((nufr_ready_list == tcb_head) && (nufr_ready_list_tail == tcb_tail))
        {
            result = true;
        }
    }    
    
    ut_clean_tcbs();

    return result;
}

bool ut_insert_nominal_at_ready_list_head(void)
{
    bool result = false;
    nufr_tcb_t *tcb = &nufr_tcb_block[0];
    tcb->priority = NUFR_TPR_NOMINAL;
        
    nufrkernel_add_task_to_ready_list(tcb);
    
    if((NULL != nufr_ready_list) && (NULL != nufr_ready_list_tail) && (nufr_ready_list_tail_nominal))
    {
        if((nufr_ready_list == tcb) && (nufr_ready_list_tail == tcb) && (nufr_ready_list_tail_nominal == tcb))
        {
            result = true;
        }
    }
    
    ut_clean_tcbs();

    return result;
}

bool ut_insert_at_ready_list_head(void)
{
  bool result = false;
  
  nufr_tcb_t *tcb = &nufr_tcb_block[0];
  tcb->priority = 2;
    
  nufrkernel_add_task_to_ready_list(tcb);
  
  if((NULL != nufr_ready_list) && (NULL != nufr_ready_list_tail))
  {
      if((nufr_ready_list == tcb) && (nufr_ready_list_tail == tcb))
      {
        result = true;
      }
  }
  
  ut_clean_tcbs();

  return result;
}

bool ut_ready_list_insert_tests(void)
{
    bool result = false;
    bool pass_insert_at_head;
    bool pass_insert_nominal_at_head;
    bool pass_insert_at_ready_list_tail;
    bool pass_insert_before_nominal;
    bool pass_insert_after_nominal;
    bool pass_insert_before_non_existant_nominal;
    bool pass_insert_before_head;

    pass_insert_at_head = ut_insert_at_ready_list_head();
    ut_clean_list();
    
    pass_insert_nominal_at_head = ut_insert_nominal_at_ready_list_head();
    ut_clean_list();
    
    pass_insert_at_ready_list_tail = ut_insert_at_ready_list_tail();
    ut_clean_list();
    
    pass_insert_before_nominal = ut_insert_before_nominal();
    ut_clean_list();
    
    pass_insert_after_nominal = ut_insert_after_nominal_before_end();
    ut_clean_list();
    
    pass_insert_before_non_existant_nominal = ut_insert_after_before_nominal_with_no_nominal_set();
    ut_clean_list();
    
    pass_insert_before_head = ut_insert_before_head();
    ut_clean_list();
    
    result = pass_insert_at_head && pass_insert_nominal_at_head && 
             pass_insert_at_ready_list_tail && pass_insert_before_nominal &&
             pass_insert_after_nominal && pass_insert_before_non_existant_nominal &&
             pass_insert_before_head;
    
    return result;
}

//  Ready List Delete Unit Tests

bool ut_ready_list_delete_between_head_and_nominal(void)
{
    bool result = false;
    
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *between = &nufr_tcb_block[1];
    nufr_tcb_t *nominal = &nufr_tcb_block[2];
    nufr_tcb_t *tail = &nufr_tcb_block[3];
    head->priority = 2;
    between->priority = 4;
    nominal->priority = NUFR_TPR_NOMINAL;
    tail->priority = NUFR_TPR_NOMINAL + 5;
    
    nufrkernel_add_task_to_ready_list(head);
    nufrkernel_add_task_to_ready_list(between);
    nufrkernel_add_task_to_ready_list(nominal);
    nufrkernel_add_task_to_ready_list(tail);
    
    if((NULL != nufr_ready_list) && (NULL != nufr_ready_list_tail_nominal) && (NULL != nufr_ready_list_tail))
    {
        if((nufr_ready_list == head) && (nufr_ready_list_tail_nominal == nominal) && (nufr_ready_list_tail == tail) && (nufr_ready_list->flink == between))
        {
            nufrkernel_delete_task_from_ready_list(between);
            
            if((nufr_ready_list == head) && (nufr_ready_list_tail_nominal == nominal) && (nufr_ready_list_tail == tail) && (nufr_ready_list->flink == nominal))
            {
                result = true;
            }
            
        }
    }
    
    ut_clean_list();
    ut_clean_tcbs();
    
    return result;
}

bool ut_ready_list_delete_at_head(void)
{
    bool result = false;
    
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *tail = &nufr_tcb_block[1];
    head->priority = 2;
    tail->priority = 4;

    nufrkernel_add_task_to_ready_list(head);
    nufrkernel_add_task_to_ready_list(tail);
    
    
    if((NULL != nufr_ready_list) && (NULL == nufr_ready_list_tail_nominal) && (NULL != nufr_ready_list_tail))
    {
        nufrkernel_delete_task_from_ready_list(head);
        
        if((nufr_ready_list == tail) && (nufr_ready_list_tail == tail) && (NULL == nufr_ready_list_tail_nominal))
        {
            result = true;
        }
    }
    
    ut_clean_list();
    ut_clean_tcbs();

    return result;
}

bool ut_ready_list_delete_tests(void)
{
    bool result = false;
    bool pass_delete_at_list_head;
    bool pass_delete_between_head_nominal;
    
    pass_delete_at_list_head = ut_ready_list_delete_at_head();
    ut_clean_list();
    
    pass_delete_between_head_nominal = ut_ready_list_delete_between_head_and_nominal();
    ut_clean_list();
    
    result = pass_delete_at_list_head && pass_delete_between_head_nominal;
    
    return result;
}

bool ut_ready_list_tests_old(void)
{
    bool result = false;
    
    bool pass_insert_tests = ut_ready_list_insert_tests();
    bool pass_delete_tests = ut_ready_list_delete_tests();
    
    result = pass_insert_tests && pass_delete_tests;
    
    return result;
}
#endif   //!!!!!!! 

void ut_tasks(void)
{
    unsigned i;
    const unsigned LOOPS = 3;

    for (i = 0; i < LOOPS; i++)
    {
        test_preliminaries();
    }

    for (i = 0; i < LOOPS; i++)
    {
        test_exercise_ready_list1();
    }

// These tests disabled. They can't work without pthreads,
// and they interfere with pthreads config.
#ifdef RUNTIME_TESTS
    test_launch();

    for (i = 0; i < LOOPS; i++)
    {
        test_bop_contrived();
    }

    for (i = 0; i < LOOPS; i++)
    {
        test_bop_basic();
    }

    for (i = 0; i < LOOPS; i++)
    {
        test_bop_keyed();
    }
#endif  //RUNTIME_TESTS
}
