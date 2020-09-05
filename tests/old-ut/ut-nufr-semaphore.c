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

//! @file    ut-nufr-semaphore.c
//! @authors Bernie Woodland
//! @date    26Oct17

// Tests file nufr-kernel-semaphore.c

#include "nufr-global.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-semaphore.h"

#include "raging-contract.h"

#include <stdio.h>
#include <string.h>


#if 0
    NUFR_TID_01  //NUFR_TPR_HIGHEST,
    NUFR_TID_02  //NUFR_TPR_HIGHEST 
    NUFR_TID_03  //NUFR_TPR_HIGHER
    NUFR_TID_04  //NUFR_TPR_HIGHER
    NUFR_TID_05  //NUFR_TPR_HIGH
    NUFR_TID_06  //NUFR_TPR_HIGH
    NUFR_TID_07  //NUFR_TPR_NOMINAL
    NUFR_TID_08  //NUFR_TPR_NOMINAL
    NUFR_TID_09  //NUFR_TPR_NOMINAL
    NUFR_TID_10  //NUFR_TPR_NOMINAL
    NUFR_TID_11  //NUFR_TPR_NOMINAL
    NUFR_TID_12  //NUFR_TPR_NOMINAL
    NUFR_TID_13  //NUFR_TPR_NOMINAL
    NUFR_TID_14  //NUFR_TPR_NOMINAL
    NUFR_TID_15  //NUFR_TPR_LOW
    NUFR_TID_16  //NUFR_TPR_LOW
    NUFR_TID_17  //NUFR_TPR_LOWER
    NUFR_TID_18  //NUFR_TPR_LOWER
    NUFR_TID_19  //NUFR_TPR_LOWEST
    NUFR_TID_20  //NUFR_TPR_LOWEST
#endif

// Sema task-specific resets
// Only applied to:
//     NUFR_SEMA_X
//     NUFR_SEMA_Y
//     NUFR_SEMA_Z
void init_for_sema_tests(void)
{
    unsigned           i;
    nufr_tcb_t        *tcb;
    nufr_sema_block_t *sema_block;

    // Clear tcb's
    for (i = NUFR_TID_01; i < NUFR_TID_max; i++)
    { 
        tcb = NUFR_TID_TO_TCB((nufr_tid_t)i);

        // Simulate having been launched
        tcb->statuses &= BITWISE_NOT8(NUFR_TASK_NOT_LAUNCHED);

        tcb->flink = NULL;
        tcb->blink = NULL;
    }

    // Clear select sema's
    sema_block = NUFR_SEMA_ID_TO_BLOCK(NUFR_SEMA_X);
    memset(sema_block, 0, sizeof(nufr_sema_block_t));
    sema_block = NUFR_SEMA_ID_TO_BLOCK(NUFR_SEMA_Y);
    memset(sema_block, 0, sizeof(nufr_sema_block_t));
    sema_block = NUFR_SEMA_ID_TO_BLOCK(NUFR_SEMA_Z);
    memset(sema_block, 0, sizeof(nufr_sema_block_t));
}

// Sanity check a single sema's tcb list
void sanity_check_sema_list(nufr_sema_block_t *sema_block)
{
    nufr_tcb_t *tcb;
    nufr_tcb_t *next_tcb;
    nufr_tcb_t *tcbs_on_sema[NUFR_NUM_TASKS];
    unsigned    num_tcbs_on_sema = 0;
    bool        null_list;
    bool        head_equals_tail;
    unsigned    count = 0;
    unsigned    priority;
    unsigned    i;
    unsigned    k;

    UT_ENSURE((NULL == sema_block->task_list_head) ==
           (NULL == sema_block->task_list_tail));

    null_list = NULL == sema_block->task_list_head;
    head_equals_tail = !null_list &&
               (sema_block->task_list_head == sema_block->task_list_tail);

    // head must have null blink
    if (NULL != sema_block->task_list_head)
    {
        UT_ENSURE(NUFR_IS_TCB(sema_block->task_list_head));
        UT_ENSURE(NULL == sema_block->task_list_head->blink);
    }

    // tail must have null flink
    if (NULL != sema_block->task_list_head)
    {
        UT_ENSURE(NUFR_IS_TCB(sema_block->task_list_tail));
        UT_ENSURE(NULL == sema_block->task_list_tail->flink);
    }

    // > 1 task on list? Do walks
    if (!null_list && !head_equals_tail)
    {
        tcb = sema_block->task_list_head;

        // Basic sanities
        while (NULL != tcb)
        {
            UT_ENSURE(NUFR_IS_TCB(tcb));

            count++;
            // Make sure list doesn't point back to itself
            UT_ENSURE(count <= NUFR_NUM_TASKS);

            // last tcb on list?
            if (NULL == tcb->flink)
            {
                UT_ENSURE(tcb == sema_block->task_list_tail);
            }

            tcb = tcb->flink;
        }

        priority = NUFR_TPR_null;

        tcb = sema_block->task_list_head;

        // Second walk for other verifications
        while (NULL != tcb)
        {
            // put on list for later
            tcbs_on_sema[num_tcbs_on_sema++] = tcb;

            next_tcb = tcb->flink;

            // Another task on list after 'tcb' task?
            if (NULL != next_tcb)
            {
                // Verify priority order sorting
                UT_ENSURE(tcb->priority <= next_tcb->priority);
                // Verify that next task has a blink
                UT_ENSURE(NULL != next_tcb->blink);
                // Verify that next task's blink points to this task
                UT_ENSURE(tcb == next_tcb->blink);
            }

            tcb = next_tcb;
        }
    }

    // Verify that if task isn't on sema, then its flink and blink are null
    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        bool found_on_sema = false;

        for (k = 0; k < num_tcbs_on_sema; k++)
        {
            if (&nufr_tcb_block[i] == tcbs_on_sema[k])
            {
                found_on_sema = true;
                break;
            }
        }

        if (!found_on_sema)
        {
            UT_ENSURE(NULL == nufr_tcb_block[i].flink);
            UT_ENSURE(NULL == nufr_tcb_block[i].blink);
        }
    }
}

// Verify that 'sema' has a task list attached to it that matches
// 'match_list' of length 'match_list_length'
void match_sema_list(const nufr_tid_t  *match_list,
                     unsigned           match_list_length,
                     nufr_sema_block_t *sema_block)
{
    unsigned           index = 0;
    nufr_tcb_t        *tcb;
    nufr_tid_t         tid;

    UT_REQUIRE(NUFR_IS_SEMA_BLOCK(sema_block));

    sanity_check_sema_list(sema_block);

    UT_REQUIRE(match_list_length > 0);

    sema_block = sema_block;
    tcb = sema_block->task_list_head;

    while (NULL != tcb)
    {
        UT_REQUIRE(index < match_list_length);

        tid = NUFR_TCB_TO_TID(tcb);
        UT_REQUIRE(match_list[index] == tid);

        index++;
        tcb = tcb->flink;
    }

    UT_REQUIRE(index == match_list_length);
}

void sema_sequential_link_and_unlink(void)
{
    nufr_tid_t full_list[] =
    {
        NUFR_TID_01,  //NUFR_TPR_HIGHEST,
        NUFR_TID_02,  //NUFR_TPR_HIGHEST 
        NUFR_TID_03,  //NUFR_TPR_HIGHER
        NUFR_TID_04,  //NUFR_TPR_HIGHER
        NUFR_TID_05,  //NUFR_TPR_HIGH
        NUFR_TID_06,  //NUFR_TPR_HIGH
        NUFR_TID_07,  //NUFR_TPR_NOMINAL
        NUFR_TID_08,  //NUFR_TPR_NOMINAL
        NUFR_TID_09,  //NUFR_TPR_NOMINAL
        NUFR_TID_10   //NUFR_TPR_NOMINAL
    };

    nufr_tcb_t        *tcb;
    nufr_sema_block_t *sema_block;
    unsigned           i;

    init_for_sema_tests();

    // Only exercise NUFR_SEMA_X
    sema_block = NUFR_SEMA_ID_TO_BLOCK(NUFR_SEMA_X);

    // Add tasks from top to bottom
    for (i = 0; i < ARRAY_SIZE(full_list); i++)
    {
        tcb = NUFR_TID_TO_TCB(full_list[i]);

        nufrkernel_sema_link_task(sema_block, tcb);

        match_sema_list(full_list, i + 1, sema_block);
    }

    // Delete tasks from top to bottom
    for (i = 0; i < ARRAY_SIZE(full_list); i++)
    {
        tcb = NUFR_TID_TO_TCB(full_list[i]);

        nufrkernel_sema_unlink_task(sema_block, tcb);

        if (i < ARRAY_SIZE(full_list) - 1)
        {
            match_sema_list(&full_list[i + 1], ARRAY_SIZE(full_list) - i - 1, sema_block);
        }
    }

    // Add tasks from top to bottom
    for (i = 0; i < ARRAY_SIZE(full_list); i++)
    {
        tcb = NUFR_TID_TO_TCB(full_list[i]);

        nufrkernel_sema_link_task(sema_block, tcb);

        match_sema_list(full_list, i + 1, sema_block);
    }

    // Delete tasks from bottom to top
    for (i = 0; i < ARRAY_SIZE(full_list); i++)
    {
        tcb = NUFR_TID_TO_TCB(full_list[ARRAY_SIZE(full_list) - i - 1]);

        nufrkernel_sema_unlink_task(sema_block, tcb);

        if (i < ARRAY_SIZE(full_list) - 1)
        {
            match_sema_list(full_list, ARRAY_SIZE(full_list) - i - 1, sema_block);
        }
    }
}

void sema_random_unlink(void)
{
    nufr_tid_t full_list[] =
    {
        NUFR_TID_01,  //NUFR_TPR_HIGHEST,
        NUFR_TID_02,  //NUFR_TPR_HIGHEST 
        NUFR_TID_03,  //NUFR_TPR_HIGHER
        NUFR_TID_04,  //NUFR_TPR_HIGHER
        NUFR_TID_05,  //NUFR_TPR_HIGH
        NUFR_TID_06,  //NUFR_TPR_HIGH
        NUFR_TID_07,  //NUFR_TPR_NOMINAL
        NUFR_TID_08,  //NUFR_TPR_NOMINAL
        NUFR_TID_09,  //NUFR_TPR_NOMINAL
        NUFR_TID_10   //NUFR_TPR_NOMINAL
    };

    nufr_tid_t list_no_3[] =
    {
        NUFR_TID_01,  //NUFR_TPR_HIGHEST,
        NUFR_TID_02,  //NUFR_TPR_HIGHEST 
        //NUFR_TID_03,  //NUFR_TPR_HIGHER
        NUFR_TID_04,  //NUFR_TPR_HIGHER
        NUFR_TID_05,  //NUFR_TPR_HIGH
        NUFR_TID_06,  //NUFR_TPR_HIGH
        NUFR_TID_07,  //NUFR_TPR_NOMINAL
        NUFR_TID_08,  //NUFR_TPR_NOMINAL
        NUFR_TID_09,  //NUFR_TPR_NOMINAL
        NUFR_TID_10  //NUFR_TPR_NOMINAL
    };

    nufr_tid_t list_no_3_7_8[] =
    {
        NUFR_TID_01,  //NUFR_TPR_HIGHEST,
        NUFR_TID_02,  //NUFR_TPR_HIGHEST 
        //NUFR_TID_03,  //NUFR_TPR_HIGHER
        NUFR_TID_04,  //NUFR_TPR_HIGHER
        NUFR_TID_05,  //NUFR_TPR_HIGH
        NUFR_TID_06,  //NUFR_TPR_HIGH
        //NUFR_TID_07,  //NUFR_TPR_NOMINAL
        //NUFR_TID_08,  //NUFR_TPR_NOMINAL
        NUFR_TID_09,  //NUFR_TPR_NOMINAL
        NUFR_TID_10  //NUFR_TPR_NOMINAL
    };

    nufr_tcb_t        *tcb;
    nufr_sema_block_t *sema_block;
    unsigned           i;

    init_for_sema_tests();

    // Only exercise NUFR_SEMA_X
    sema_block = NUFR_SEMA_ID_TO_BLOCK(NUFR_SEMA_X);

    // Add tasks from top to bottom
    for (i = 0; i < ARRAY_SIZE(full_list); i++)
    {
        tcb = NUFR_TID_TO_TCB(full_list[i]);

        nufrkernel_sema_link_task(sema_block, tcb);

        match_sema_list(full_list, i + 1, sema_block);
    }

    // Delete just task 3. Check.
    tcb = NUFR_TID_TO_TCB(NUFR_TID_03);
    nufrkernel_sema_unlink_task(sema_block, tcb);
    match_sema_list(list_no_3, ARRAY_SIZE(list_no_3), sema_block);

    // Delete 7,8 in addition to 3. Check.
    tcb = NUFR_TID_TO_TCB(NUFR_TID_07);
    nufrkernel_sema_unlink_task(sema_block, tcb);
    tcb = NUFR_TID_TO_TCB(NUFR_TID_08);
    nufrkernel_sema_unlink_task(sema_block, tcb);
    match_sema_list(list_no_3_7_8, ARRAY_SIZE(list_no_3_7_8), sema_block);
}

void sema_prioritized_add(void)
{
    nufr_tid_t add_list[] =
    {
        NUFR_TID_07,  //NUFR_TPR_NOMINAL
        NUFR_TID_08,  //NUFR_TPR_NOMINAL
        NUFR_TID_05,  //NUFR_TPR_HIGH
        NUFR_TID_06,  //NUFR_TPR_HIGH
        NUFR_TID_01,  //NUFR_TPR_HIGHEST,
        NUFR_TID_09,  //NUFR_TPR_NOMINAL
    };

    nufr_tid_t match_list[] =
    {
        NUFR_TID_01,  //NUFR_TPR_HIGHEST,
        NUFR_TID_05,  //NUFR_TPR_HIGH
        NUFR_TID_06,  //NUFR_TPR_HIGH
        NUFR_TID_07,  //NUFR_TPR_NOMINAL
        NUFR_TID_08,  //NUFR_TPR_NOMINAL
        NUFR_TID_09,  //NUFR_TPR_NOMINAL
    };

    nufr_tcb_t        *tcb;
    nufr_sema_block_t *sema_block;
    unsigned           i;

    init_for_sema_tests();

    // Only exercise NUFR_SEMA_X
    sema_block = NUFR_SEMA_ID_TO_BLOCK(NUFR_SEMA_X);

    // Add tasks from top to bottom
    for (i = 0; i < ARRAY_SIZE(add_list); i++)
    {
        tcb = NUFR_TID_TO_TCB(add_list[i]);

        nufrkernel_sema_link_task(sema_block, tcb);
    }

    match_sema_list(match_list, ARRAY_SIZE(match_list), sema_block);
}

void ut_semaphores(void)
{
    sema_sequential_link_and_unlink();
    sema_random_unlink();
    sema_prioritized_add();
}