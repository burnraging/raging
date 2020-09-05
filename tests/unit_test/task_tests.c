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
// By Chris Martin

#include <task_tests.h>
#include <inttypes.h>
#include <nufr-kernel-message-blocks.h>

void ut_clean_list(void)
{
    nufr_ready_list = nufr_ready_list_tail = nufr_ready_list_tail_nominal = NULL;
    memset(nufr_tcb_block, 0, sizeof(nufr_tcb_block));

    nufr_msg_free_head = nufr_msg_free_tail = NULL;
    //nufr_msg_free_count = 0;
    nufr_msg_pool_empty_count = 0;
    nufr_msg_bpool_init();

}

void ut_launch_task(void)
{
    ut_clean_list();
    nufr_tcb_t *task = NUFR_TID_TO_TCB(NUFR_TID_01);

    CU_ASSERT_TRUE_FATAL(0 == ut_interrupt_count);
    nufr_launch_task(NUFR_TID_01, 0);
    CU_ASSERT_TRUE_FATAL(0 == ut_interrupt_count);

    CU_ASSERT_TRUE(NULL != nufr_ready_list);
    CU_ASSERT_TRUE(nufr_ready_list == task);
    CU_ASSERT_TRUE(NULL != nufr_running);
    CU_ASSERT_TRUE(nufr_running == task);
}

void ut_launch_non_init_task(void)
{
    ut_clean_list();
    nufr_tcb_t *task = NUFR_TID_TO_TCB(NUFR_TID_02);
    task->statuses = NUFR_TASK_BLOCKED_ALL;

    CU_ASSERT_TRUE_FATAL(0 == ut_interrupt_count);
    nufr_launch_task(NUFR_TID_02, 0);
    CU_ASSERT_TRUE_FATAL(0 == ut_interrupt_count);
}

void ut_insert_nominal_after_causing_ready_list_walk(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *tail = &nufr_tcb_block[1];
    nufr_tcb_t *nominal = &nufr_tcb_block[2];

    head->priority = NUFR_TPR_HIGHEST;
    tail->priority = NUFR_TPR_LOWEST;
    nominal->priority = NUFR_TPR_NOMINAL;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == head);

    nufrkernel_add_task_to_ready_list(tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list->flink);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list->flink == tail);
    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail->flink);

    nufrkernel_add_task_to_ready_list(nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);
    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail->flink);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list->flink);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list->flink == nominal);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == nominal);
}

void ut_insert_at_head_of_list_but_as_nominal(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *nominal_head = &nufr_tcb_block[1];

    head->priority = NUFR_TPR_LOW;
    nominal_head->priority = NUFR_TPR_NOMINAL;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == head);

    nufrkernel_add_task_to_ready_list(nominal_head);

    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == nominal_head);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
    CU_ASSERT_TRUE(nufr_ready_list_tail == head);
}

void ut_insert_after_nominal_before_tail_with_multiples(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *next = &nufr_tcb_block[1];
    nufr_tcb_t *target = &nufr_tcb_block[2];
    nufr_tcb_t *nominal = &nufr_tcb_block[3];
    nufr_tcb_t *tail = &nufr_tcb_block[4];

    head->priority = NUFR_TPR_HIGHEST;
    next->priority = NUFR_TPR_NOMINAL + 1;
    target->priority = NUFR_TPR_NOMINAL + 2;
    nominal->priority = NUFR_TPR_NOMINAL;
    tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);

    nufrkernel_add_task_to_ready_list(nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == nominal);

    nufrkernel_add_task_to_ready_list(next);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal->flink);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal->flink == next);

    nufrkernel_add_task_to_ready_list(tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);

    nufrkernel_add_task_to_ready_list(target);

    CU_ASSERT_TRUE(NULL != nufr_ready_list);
    CU_ASSERT_TRUE(nufr_ready_list == head);

    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail_nominal->flink);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal->flink == next);

    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail_nominal->flink->flink);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal->flink->flink == target);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail_nominal->flink->flink->flink);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal->flink->flink->flink == tail);

    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == nominal);

    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tail);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

void ut_insert_after_before_nominal_with_no_nominal_set(void)
{
    ut_clean_list();
    nufr_tcb_t *tcb_head = &nufr_tcb_block[0];
    nufr_tcb_t *tcb_before_nominal = &nufr_tcb_block[1];
    tcb_head->priority = NUFR_TPR_HIGHEST;
    tcb_before_nominal->priority = NUFR_TPR_HIGH;

    nufrkernel_add_task_to_ready_list(tcb_head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == tcb_head);

    nufrkernel_add_task_to_ready_list(tcb_before_nominal);

    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tcb_head);
    CU_ASSERT_TRUE(NULL != nufr_ready_list->flink);
    CU_ASSERT_TRUE(nufr_ready_list->flink == tcb_before_nominal);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tcb_before_nominal);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

void ut_insert_after_nominal_before_end(void)
{
    ut_clean_list();
    nufr_tcb_t *tcb_head = &nufr_tcb_block[0];
    nufr_tcb_t *tcb_nominal = &nufr_tcb_block[1];
    nufr_tcb_t *tcb_between = &nufr_tcb_block[2];
    nufr_tcb_t *tcb_tail = &nufr_tcb_block[3];
    tcb_head->priority = NUFR_TPR_HIGHEST;
    tcb_nominal->priority = NUFR_TPR_NOMINAL;
    tcb_between->priority = NUFR_TPR_LOWER;
    tcb_tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(tcb_head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == tcb_head);

    nufrkernel_add_task_to_ready_list(tcb_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == tcb_nominal);

    nufrkernel_add_task_to_ready_list(tcb_tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tcb_tail);

    nufrkernel_add_task_to_ready_list(tcb_between);

    CU_ASSERT_TRUE(nufr_ready_list == tcb_head);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == tcb_nominal);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tcb_tail);

    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail_nominal->flink);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal->flink == tcb_between);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

void ut_insert_tail_last(void)
{
    ut_clean_list();
    nufr_tcb_t *tcb_head = &nufr_tcb_block[0];
    nufr_tcb_t *tcb_nom = &nufr_tcb_block[1];
    nufr_tcb_t *tcb_higher = &nufr_tcb_block[2];
    nufr_tcb_t *tcb_high = &nufr_tcb_block[4];
    nufr_tcb_t *tcb_tail = &nufr_tcb_block[5];

    tcb_head->priority = NUFR_TPR_HIGHEST;
    tcb_nom->priority = NUFR_TPR_NOMINAL;
    tcb_higher->priority = NUFR_TPR_HIGHER;
    tcb_high->priority = NUFR_TPR_HIGH;
    tcb_tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(tcb_head);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == tcb_head);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tcb_head);

    nufrkernel_add_task_to_ready_list(tcb_nom);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == tcb_head);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == tcb_nom);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tcb_nom);

    nufrkernel_add_task_to_ready_list(tcb_higher);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == tcb_head);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == tcb_nom);
    CU_ASSERT_TRUE(nufr_ready_list->flink == tcb_higher);
    CU_ASSERT_TRUE(nufr_ready_list->flink->flink == tcb_nom);

    nufrkernel_add_task_to_ready_list(tcb_high);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == tcb_head);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == tcb_nom);
    CU_ASSERT_TRUE(nufr_ready_list->flink == tcb_higher);
    CU_ASSERT_TRUE(nufr_ready_list->flink->flink == tcb_high);
    CU_ASSERT_TRUE(nufr_ready_list->flink->flink->flink == tcb_nom);

    nufrkernel_add_task_to_ready_list(tcb_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == tcb_head);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == tcb_nom);
    CU_ASSERT_TRUE(nufr_ready_list->flink == tcb_higher);
    CU_ASSERT_TRUE(nufr_ready_list->flink->flink == tcb_high);
    CU_ASSERT_TRUE(nufr_ready_list->flink->flink->flink == tcb_nom);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tcb_tail);
}

void ut_insert_before_nominal(void)
{
    ut_clean_list();
    nufr_tcb_t *tcb_head = &nufr_tcb_block[0];
    nufr_tcb_t *tcb_between = &nufr_tcb_block[1];
    nufr_tcb_t *tcb_tail = &nufr_tcb_block[2];
    tcb_head->priority = NUFR_TPR_HIGHEST;
    tcb_between->priority = NUFR_TPR_HIGH;
    tcb_tail->priority = NUFR_TPR_NOMINAL;

    nufrkernel_add_task_to_ready_list(tcb_head);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == tcb_head);

    nufrkernel_add_task_to_ready_list(tcb_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tcb_tail);

    nufrkernel_add_task_to_ready_list(tcb_between);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tcb_head);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == tcb_tail);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tcb_tail);

    CU_ASSERT_TRUE(NULL != nufr_ready_list->flink);
    CU_ASSERT_TRUE(nufr_ready_list->flink == tcb_between);
}

void ut_insert_at_ready_list_tail(void)
{
    ut_clean_list();
    nufr_tcb_t *tcb_head = &nufr_tcb_block[0];
    nufr_tcb_t *tcb_tail = &nufr_tcb_block[1];
    tcb_head->priority = NUFR_TPR_HIGHEST;
    tcb_tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(tcb_head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == tcb_head);

    nufrkernel_add_task_to_ready_list(tcb_tail);

    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tcb_head);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tcb_tail);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

void ut_insert_nominal_to_ready_list_with_non_nominal_tail(void)
{
    ut_clean_list();
    nufr_tcb_t *tcbOne = &nufr_tcb_block[0];
    nufr_tcb_t *tcbTwo = &nufr_tcb_block[1];
    nufr_tcb_t *tcbThree = &nufr_tcb_block[2];
    tcbOne->priority = NUFR_TPR_NOMINAL;
    tcbTwo->priority = NUFR_TPR_LOWER;
    tcbThree->priority = NUFR_TPR_NOMINAL;

    nufrkernel_add_task_to_ready_list(tcbOne);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tcbOne);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == tcbOne);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tcbOne);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);

    nufrkernel_add_task_to_ready_list(tcbTwo);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tcbOne);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == tcbOne);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tcbTwo);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);

    nufrkernel_add_task_to_ready_list(tcbThree);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tcbOne);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == tcbThree);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tcbTwo);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

void ut_insert_two_nominal_tasks_in_ready_list(void)
{
    ut_clean_list();
    nufr_tcb_t *tcbOne = &nufr_tcb_block[0];
    nufr_tcb_t *tcbTwo = &nufr_tcb_block[1];
    tcbOne->priority = NUFR_TPR_NOMINAL;
    tcbTwo->priority = NUFR_TPR_NOMINAL;

    nufrkernel_add_task_to_ready_list(tcbOne);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tcbOne);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == tcbOne);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tcbOne);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);

    nufrkernel_add_task_to_ready_list(tcbTwo);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tcbOne);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == tcbTwo);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tcbTwo);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

void ut_insert_nominal_at_ready_list_head(void)
{
    ut_clean_list();
    nufr_tcb_t *tcb = &nufr_tcb_block[0];
    tcb->priority = NUFR_TPR_NOMINAL;

    nufrkernel_add_task_to_ready_list(tcb);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tcb);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == tcb);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tcb);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

void ut_insert_at_ready_list_head(void)
{
    ut_clean_list();
    nufr_tcb_t *ptrHead = &nufr_tcb_block[0];

    ptrHead->priority = NUFR_TPR_HIGH;

    nufrkernel_add_task_to_ready_list(ptrHead);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == ptrHead);
    CU_ASSERT_TRUE(nufr_ready_list_tail == ptrHead);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

void ut_insert_before_head(void)
{
    ut_clean_list();
    nufr_tcb_t *tcb_head = &nufr_tcb_block[0];
    nufr_tcb_t *tcb_new_head = &nufr_tcb_block[1];

    tcb_head->priority = NUFR_TPR_HIGH;
    tcb_new_head->priority = NUFR_TPR_HIGHEST;

    nufrkernel_add_task_to_ready_list(tcb_head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == tcb_head);

    nufrkernel_add_task_to_ready_list(tcb_new_head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == tcb_new_head);

    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tcb_new_head);
    CU_ASSERT_TRUE(nufr_ready_list->priority == NUFR_TPR_HIGHEST);

    CU_ASSERT_TRUE(nufr_ready_list_tail == tcb_head);
    CU_ASSERT_TRUE(nufr_ready_list_tail->priority == NUFR_TPR_HIGH);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

/* Ready List Delete Tests */

void ut_ready_list_delete_last_task(void)
{
    ut_clean_list();
    nufr_tcb_t *task = &nufr_tcb_block[0];
    task->priority = NUFR_TPR_HIGHEST;

    nufrkernel_add_task_to_ready_list(task);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == task);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == task);

    nufr_running = (nufr_tcb_t *)nufr_bg_sp;

    nufrkernel_delete_task_from_ready_list(task);
    CU_ASSERT_TRUE(NULL == nufr_ready_list);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail);
}

void ut_ready_list_delete_from_multiple_nominal_tasks(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *nom_1 = &nufr_tcb_block[1];
    nufr_tcb_t *nom_2 = &nufr_tcb_block[2];
    nufr_tcb_t *nom_3 = &nufr_tcb_block[3];
    nufr_tcb_t *tail = &nufr_tcb_block[4];

    head->priority = NUFR_TPR_HIGHEST;
    nom_1->priority = NUFR_TPR_NOMINAL;
    nom_2->priority = NUFR_TPR_NOMINAL;
    nom_3->priority = NUFR_TPR_NOMINAL;
    tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(head);
    nufrkernel_add_task_to_ready_list(nom_1);
    nufrkernel_add_task_to_ready_list(nom_2);
    nufrkernel_add_task_to_ready_list(nom_3);
    nufrkernel_add_task_to_ready_list(tail);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == nom_3);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);

    nufrkernel_delete_task_from_ready_list(nom_2);
    CU_ASSERT_TRUE(NULL != nufr_ready_list);
    CU_ASSERT_TRUE(nufr_ready_list == head);
    CU_ASSERT_TRUE(NULL != nufr_ready_list->flink);
    CU_ASSERT_TRUE(nufr_ready_list->flink == nom_1);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == nom_3);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tail);
}

void ut_ready_list_delete_nominal_tail_from_multiple_nominal_tasks(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *nom_1 = &nufr_tcb_block[1];
    nufr_tcb_t *nom_2 = &nufr_tcb_block[2];
    nufr_tcb_t *nom_3 = &nufr_tcb_block[3];
    nufr_tcb_t *tail = &nufr_tcb_block[4];

    head->priority = NUFR_TPR_HIGHEST;
    nom_1->priority = NUFR_TPR_NOMINAL;
    nom_2->priority = NUFR_TPR_NOMINAL;
    nom_3->priority = NUFR_TPR_NOMINAL;
    tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(head);
    nufrkernel_add_task_to_ready_list(nom_1);
    nufrkernel_add_task_to_ready_list(nom_2);
    nufrkernel_add_task_to_ready_list(nom_3);
    nufrkernel_add_task_to_ready_list(tail);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == nom_3);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);

    nufrkernel_delete_task_from_ready_list(nom_3);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == nom_2);
}

void ut_ready_list_delete_not_found_task(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *nominal = &nufr_tcb_block[1];
    nufr_tcb_t *tail = &nufr_tcb_block[2];
    nufr_tcb_t *not_found_task = &nufr_tcb_block[3];

    head->priority = NUFR_TPR_HIGHEST;
    nominal->priority = NUFR_TPR_NOMINAL;
    tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);

    nufrkernel_add_task_to_ready_list(nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == nominal);

    nufrkernel_add_task_to_ready_list(tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);

    nufrkernel_delete_task_from_ready_list(not_found_task);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);
}

void ut_ready_list_delete_running_task(void)
{
    ut_clean_list();
    nufr_tcb_t *task = &nufr_tcb_block[0];
    task->priority = NUFR_TPR_NOMINAL;

    nufrkernel_add_task_to_ready_list(task);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == task);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == task);

    nufr_running = task;

    nufrkernel_delete_task_from_ready_list(task);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == task);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == task);
}

void ut_ready_list_delete_null_node(void)
{
    ut_clean_list();
    nufr_tcb_t *task = &nufr_tcb_block[0];
    nufrkernel_delete_task_from_ready_list(task);
}

void ut_ready_list_delete_at_tail(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *tail = &nufr_tcb_block[1];

    head->priority = NUFR_TPR_HIGHEST;
    tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);

    nufrkernel_add_task_to_ready_list(tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);
    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail->flink);
    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail_nominal);

    nufrkernel_delete_task_from_ready_list(tail);

    CU_ASSERT_TRUE(NULL != nufr_ready_list);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == head);
    CU_ASSERT_TRUE(nufr_ready_list_tail == head);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

void ut_ready_list_delete_between_nominal_and_tail(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *nominal = &nufr_tcb_block[1];
    nufr_tcb_t *between = &nufr_tcb_block[2];
    nufr_tcb_t *tail = &nufr_tcb_block[3];

    head->priority = NUFR_TPR_HIGHEST;
    nominal->priority = NUFR_TPR_NOMINAL;
    between->priority = NUFR_TPR_LOW;
    tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);

    nufrkernel_add_task_to_ready_list(nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == nominal);

    nufrkernel_add_task_to_ready_list(between);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal->flink);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal->flink == between);

    nufrkernel_add_task_to_ready_list(tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);
    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail->flink);

    nufrkernel_delete_task_from_ready_list(between);

    CU_ASSERT_TRUE(NULL != nufr_ready_list);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);

    CU_ASSERT_TRUE(nufr_ready_list == head);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == nominal);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail_nominal->flink);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal->flink == tail);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tail);
}

void ut_ready_list_delete_nominal(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *nominal = &nufr_tcb_block[1];
    nufr_tcb_t *tail = &nufr_tcb_block[2];

    head->priority = NUFR_TPR_HIGHEST;
    nominal->priority = NUFR_TPR_NOMINAL;
    tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);

    nufrkernel_add_task_to_ready_list(nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == nominal);

    nufrkernel_add_task_to_ready_list(tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);
    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail->flink);

    nufrkernel_delete_task_from_ready_list(nominal);

    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE(NULL != nufr_ready_list);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);

    CU_ASSERT_TRUE(nufr_ready_list == head);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tail);
}

void ut_ready_list_delete_between_head_and_nominal(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *between = &nufr_tcb_block[1];
    nufr_tcb_t *nominal = &nufr_tcb_block[2];
    nufr_tcb_t *tail = &nufr_tcb_block[3];
    head->priority = NUFR_TPR_HIGHEST;
    between->priority = NUFR_TPR_HIGH;
    nominal->priority = NUFR_TPR_NOMINAL;
    tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);

    nufrkernel_add_task_to_ready_list(between);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list->flink);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list->flink == between);

    nufrkernel_add_task_to_ready_list(nominal);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == nominal);

    nufrkernel_add_task_to_ready_list(tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);

    nufrkernel_delete_task_from_ready_list(between);

    CU_ASSERT_TRUE(NULL != nufr_ready_list);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == head);
    CU_ASSERT_TRUE(nufr_ready_list_tail_nominal == nominal);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tail);

    CU_ASSERT_TRUE(NULL != nufr_ready_list->flink);
    CU_ASSERT_TRUE(nufr_ready_list->flink == nominal);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

void ut_ready_list_delete_at_head(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *tail = &nufr_tcb_block[1];
    head->priority = NUFR_TPR_HIGHEST;
    tail->priority = NUFR_TPR_HIGH;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);

    nufrkernel_add_task_to_ready_list(tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);

    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail_nominal);

    nufrkernel_remove_head_task_from_ready_list();

    CU_ASSERT_TRUE(NULL != nufr_ready_list);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tail);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tail);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

void ut_ready_list_delete_at_head_alternate(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *tail = &nufr_tcb_block[1];
    head->priority = NUFR_TPR_HIGHEST;
    tail->priority = NUFR_TPR_HIGH;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);

    nufrkernel_add_task_to_ready_list(tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);

    CU_ASSERT_TRUE_FATAL(NULL == nufr_ready_list_tail_nominal);

    /*
     *  The only way to remove the head of the list without using
     *  the specifically designed method, is to make sure that the 
     *  background task is the currently running task.
     */
    nufr_running = ((nufr_tcb_t *)nufr_bg_sp);

    nufrkernel_delete_task_from_ready_list(head);

    CU_ASSERT_TRUE(NULL != nufr_ready_list);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail);

    CU_ASSERT_TRUE(nufr_ready_list == tail);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tail);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail->flink);
}

/* Remove Head Tests */

void ut_remove_head_from_ready_list(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *tail = &nufr_tcb_block[1];

    head->priority = NUFR_TPR_HIGHEST;
    tail->priority = NUFR_TPR_HIGHER;

    nufrkernel_add_task_to_ready_list(head);
    nufrkernel_add_task_to_ready_list(tail);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);

    nufrkernel_remove_head_task_from_ready_list();
    CU_ASSERT_TRUE(NULL != nufr_ready_list);
    CU_ASSERT_TRUE(nufr_ready_list == tail);
    CU_ASSERT_TRUE(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE(nufr_ready_list_tail == tail);
}

void ut_remove_head_from_single_task_list(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    head->priority = NUFR_TPR_HIGHEST;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);

    nufrkernel_remove_head_task_from_ready_list();
    CU_ASSERT_TRUE(NULL == nufr_ready_list);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail_nominal);
}

void ut_remove_last_nominal_from_task_list(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *nom_1 = &nufr_tcb_block[1];
    nufr_tcb_t *nom_2 = &nufr_tcb_block[3];
    nufr_tcb_t *tail = &nufr_tcb_block[2];

    head->priority = NUFR_TPR_HIGHEST;
    nom_1->priority = NUFR_TPR_NOMINAL;
    nom_2->priority = NUFR_TPR_NOMINAL;
    tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(head);
    nufrkernel_add_task_to_ready_list(nom_1);
    nufrkernel_add_task_to_ready_list(nom_2);
    nufrkernel_add_task_to_ready_list(tail);

    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail_nominal == nom_2);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == tail);

    nufrkernel_remove_head_task_from_ready_list();
    nufrkernel_remove_head_task_from_ready_list();
    nufrkernel_remove_head_task_from_ready_list();
    nufrkernel_remove_head_task_from_ready_list();

    CU_ASSERT_TRUE(NULL == nufr_ready_list);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail_nominal);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail);
}

/*  Task Blocking Tests */

void ut_block_task(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    head->priority = NUFR_TPR_HIGHEST;

    nufrkernel_add_task_to_ready_list(head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list_tail);
    CU_ASSERT_TRUE_FATAL(nufr_ready_list_tail == head);

    nufrkernel_block_running_task(NUFR_TASK_BLOCKED_MSG);
    CU_ASSERT_TRUE(NULL == nufr_ready_list);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail);
    CU_ASSERT_TRUE(NULL == nufr_ready_list_tail_nominal);
}

void ut_block_last_running_nominal_task(void)
{
    ut_clean_list();
    nufr_tcb_t *head = &nufr_tcb_block[0];
    nufr_tcb_t *nominal_tail = &nufr_tcb_block[1];
    nufr_tcb_t *tail = &nufr_tcb_block[2];

    head->priority = NUFR_TPR_HIGHEST;
    nominal_tail->priority = NUFR_TPR_NOMINAL;
    tail->priority = NUFR_TPR_LOWEST;

    nufrkernel_add_task_to_ready_list(head);
    nufrkernel_add_task_to_ready_list(nominal_tail);
    nufrkernel_add_task_to_ready_list(tail);

    nufr_running = head;

    CU_ASSERT_TRUE_FATAL(nufr_ready_list == head);
    nufrkernel_block_running_task(NUFR_TASK_BLOCKED_MSG);
    CU_ASSERT_TRUE(nufr_ready_list == nominal_tail);

    nufrkernel_block_running_task(NUFR_TASK_BLOCKED_MSG);
    CU_ASSERT_TRUE(nufr_ready_list == tail);
}

/*  Task Priority Tests */

void ut_make_running_task_highest_priority(void)
{
    ut_clean_list();
    nufr_tcb_t *task1 = &nufr_tcb_block[0];
    nufr_tcb_t *task2 = &nufr_tcb_block[1];
    task1->priority = NUFR_TPR_NOMINAL;
    task2->priority = NUFR_TPR_NOMINAL + 1;

    nufrkernel_add_task_to_ready_list(task1);
    nufrkernel_add_task_to_ready_list(task2);

    nufr_running = task1;

    CU_ASSERT_TRUE_FATAL(ut_interrupt_count == 0);
    nufr_prioritize();
    CU_ASSERT_TRUE(NULL != nufr_running);
    CU_ASSERT_TRUE(nufr_running->priority == NUFR_TPR_guaranteed_highest);
    CU_ASSERT_TRUE(nufr_running->priority_restore_prioritized == NUFR_TPR_NOMINAL);
    CU_ASSERT_TRUE(ut_interrupt_count == 0);
}

void ut_restore_single_task_priority(void)
{
    ut_clean_list();
    nufr_tcb_t *task1 = &nufr_tcb_block[0];
    nufr_tcb_t *task2 = &nufr_tcb_block[1];
    task1->priority = NUFR_TPR_NOMINAL;
    task2->priority = NUFR_TPR_NOMINAL + 1;

    nufrkernel_add_task_to_ready_list(task1);
    nufrkernel_add_task_to_ready_list(task2);

    nufr_running = task1;

    CU_ASSERT_TRUE_FATAL(0 == ut_interrupt_count);
    nufr_prioritize();
    CU_ASSERT_TRUE_FATAL(0 == ut_interrupt_count);

    CU_ASSERT_TRUE(NULL != nufr_running);
    CU_ASSERT_TRUE(nufr_running->priority == NUFR_TPR_guaranteed_highest);
    CU_ASSERT_TRUE(nufr_running->priority_restore_prioritized == NUFR_TPR_NOMINAL);

    CU_ASSERT_TRUE_FATAL(0 == ut_interrupt_count);
    nufr_unprioritize();
    CU_ASSERT_TRUE(0 == ut_interrupt_count);
    CU_ASSERT_TRUE(NULL != nufr_running);
    CU_ASSERT_TRUE(nufr_running->priority == NUFR_TPR_NOMINAL);
}

void ut_task_set_priority_lowest(void)
{
    ut_clean_list();
    nufr_tcb_t *task = NUFR_TID_TO_TCB(NUFR_TID_01);
    task->priority = NUFR_TPR_NOMINAL;

    nufr_running = task;
    nufrkernel_add_task_to_ready_list(task);

    CU_ASSERT_TRUE_FATAL(0 == ut_interrupt_count);
    nufr_change_task_priority(NUFR_TID_01, NUFR_TPR_LOWEST);
    CU_ASSERT_TRUE(0 == ut_interrupt_count);

    CU_ASSERT_TRUE(NULL != nufr_ready_list);
    CU_ASSERT_TRUE(nufr_ready_list->priority == NUFR_TPR_LOWEST);
}

void ut_task_set_priority_blocked_task(void)
{
    ut_clean_list();
    nufr_tid_t task_tid;
    nufr_tcb_t *task = NUFR_TID_TO_TCB(NUFR_TID_01);
    nufr_tcb_t *task2 = NUFR_TID_TO_TCB(NUFR_TID_02);

    task->priority = NUFR_TPR_HIGH;
    task2->priority = NUFR_TPR_LOWER;

    nufrkernel_add_task_to_ready_list(task);
    nufrkernel_add_task_to_ready_list(task2);
    task_tid = NUFR_TCB_TO_TID(task);

    nufr_running = task2;
    nufrkernel_block_running_task(NUFR_TASK_BLOCKED_MSG);

    CU_ASSERT_TRUE_FATAL(NUFR_IS_TASK_BLOCKED(task));
    CU_ASSERT_TRUE_FATAL(task == NUFR_TID_TO_TCB(task_tid));
    CU_ASSERT_TRUE_FATAL(NULL != nufr_ready_list);

    nufr_change_task_priority(task_tid, NUFR_TPR_LOWEST);
    CU_ASSERT_TRUE(task->priority == NUFR_TPR_LOWEST);
}

void ut_task_set_priority_of_non_blocked_task(void)
{
    ut_clean_list();
    nufr_tcb_t *task = NUFR_TID_TO_TCB(NUFR_TID_01);
    task->priority = NUFR_TPR_HIGH;
    nufr_tid_t task_tid = NUFR_TCB_TO_TID(task);

    nufrkernel_add_task_to_ready_list(task);
    // Since we're in the mode of changing another task's/thread's priority,
    // must make it appear that this is done from the BG task,
    // otherwise, will violate an assert in nufrkernel_delete_task_from_ready_list()
    nufr_running = (nufr_tcb_t *)nufr_bg_sp;

    nufr_change_task_priority(task_tid, NUFR_TPR_LOWEST);
    CU_ASSERT_TRUE(task->priority == NUFR_TPR_LOWEST);
}

/*  Misc Tests */

void ut_exit_running_task(void)
{
    ut_clean_list();
    nufr_tcb_t *task = &nufr_tcb_block[0];

    nufrkernel_add_task_to_ready_list(task);
    nufr_running = task;

    CU_ASSERT_TRUE_FATAL(ut_interrupt_count == 0);
    nufrkernel_exit_running_task();
    CU_ASSERT_TRUE(NUFR_IS_BLOCK_SET(task, NUFR_TASK_NOT_LAUNCHED));
    CU_ASSERT_TRUE(ut_interrupt_count == 0);
}

void ut_exit_running_task_with_semaphore(void)
{
    ut_clean_list();
    nufr_tcb_t *task_1 = &nufr_tcb_block[0];
    nufr_tcb_t *task_2 = &nufr_tcb_block[1];
    nufr_tcb_t *task_3 = &nufr_tcb_block[2];

    task_1->priority = NUFR_TPR_NOMINAL;
    // Ensure task 2 is at top of running list,
    //  for nufrkernel_block_running_task() later on.
    task_2->priority = NUFR_TPR_HIGH;
    task_3->priority = NUFR_TPR_LOW;

    nufrkernel_add_task_to_ready_list(task_1);
    nufrkernel_add_task_to_ready_list(task_2);
    nufrkernel_add_task_to_ready_list(task_3);

    nufr_running = task_1;
    nufr_sema_block_t *semaphore = NUFR_SEMA_ID_TO_BLOCK(NUFR_SEMA_X);
    nufrkernel_sema_reset(semaphore, 0, true);    
    task_1->sema_block = semaphore;
    semaphore->owner_tcb = task_1;

    // Put Task 2's priority back to nominal, otherwise
    // When Task 1 exits, it'll context swith in the 
    // middle of the exit, and UT environment can't
    // handle that.
    task_2->priority = NUFR_TPR_NOMINAL;

    // Better to call 'nufrkernel_block_running_task()' before
    //   'nufrkernel_sema_link_task()', as flink is shared between
    //   them. If this test had multiple tasks waiting on sema list,
    //   it would be a problem.
    nufrkernel_block_running_task(NUFR_TASK_BLOCKED_SEMA);
    nufrkernel_sema_link_task(semaphore,task_2);

    CU_ASSERT_TRUE_FATAL(ut_interrupt_count == 0);

    // Manually set 'nufr_running' since no Context Switch (PendSV)
    //   available in UT environment
    nufr_running = nufr_ready_list;
    CU_ASSERT_TRUE(task_1 == nufr_running);
    CU_ASSERT_TRUE(task_1 == nufr_ready_list);

    nufrkernel_exit_running_task();

    CU_ASSERT_TRUE(NUFR_IS_BLOCK_SET(task_1, NUFR_TASK_NOT_LAUNCHED));
    CU_ASSERT_TRUE(ut_interrupt_count == 0);
}

/*  Test Suite Tasks */

int ReadyListTestSuite_Initialize(void)
{
    ut_clean_list();

    return 0;
}

int ReadListTestSuite_CleanUp(void)
{
    ut_clean_list();

    return 0;
}

CU_ErrorCode ut_setup_ready_list_tests(void)
{
    CU_pSuite ptrReadyListSuite = NULL;
    CU_ErrorCode result = CUE_SUCCESS;

    ptrReadyListSuite = CU_add_suite(READY_LIST_TEST_SUITE, ReadyListTestSuite_Initialize, ReadListTestSuite_CleanUp);
    if (NULL != ptrReadyListSuite)
    {
        /*  Adding Launch Tests */
        CU_pTest outcome = NULL;
        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_launch_task);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_launch_non_init_task);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        /*  Adding Insert Tests */
        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_before_head);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_nominal_after_causing_ready_list_walk);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_at_head_of_list_but_as_nominal);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_after_before_nominal_with_no_nominal_set);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_after_nominal_before_end);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_tail_last);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_before_nominal);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_at_ready_list_tail);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_two_nominal_tasks_in_ready_list);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_nominal_at_ready_list_head);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_at_ready_list_head);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_insert_before_head);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        /*  Adding Delete Tests */
        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_last_task);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_from_multiple_nominal_tasks);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_nominal_tail_from_multiple_nominal_tasks);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_not_found_task);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_running_task);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_null_node);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_at_tail);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_between_nominal_and_tail);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_nominal);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_between_head_and_nominal);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_at_head);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_ready_list_delete_at_head_alternate);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        /*  Adding Remove Head Tests */
        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_remove_head_from_ready_list);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_remove_head_from_single_task_list);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_remove_last_nominal_from_task_list);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        /*  Adding Task Blocking Tests */
        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_block_task);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_block_last_running_nominal_task);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        /* Adding Task Priority Tests */
        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_make_running_task_highest_priority);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_restore_single_task_priority);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_task_set_priority_lowest);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_task_set_priority_blocked_task);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_task_set_priority_of_non_blocked_task);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        /* adding misc tests */

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_exit_running_task);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }

        outcome = CU_ADD_TEST(ptrReadyListSuite, ut_exit_running_task_with_semaphore);
        if (NULL == outcome)
        {
            CU_cleanup_registry();
            result = CU_get_error();
        }
    }
    else
    {
        CU_cleanup_registry();
        result = CU_get_error();
    }
    return result;
}
