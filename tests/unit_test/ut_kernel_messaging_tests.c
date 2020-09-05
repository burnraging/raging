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
#include <test_helper.h>
#include <nufr-kernel-base-messaging.h>
#include <nufr-platform.h>
#include <nufr-platform-app.h>
#include <nufr-api.h>
#include <nufr-kernel-task.h>
#include <nufr-kernel-message-send-inline.h>



#define PREFIX_DEFAULT_A                       1
#define PREFIX_DEFAULT_B                       2
#define ID_DEFAULT_A                           1
#define ID_DEFAULT_B                           2
#define PRIORITY_DEFAULT_A                     0 //NUFR_MSG_PRI_MID if NUFR_CS_MESSAGING == 1
#define PARAM_DEFAULT_A               0xFADEDFAD
#define PARAM_DEFAULT_B               0xFADEDBAD


void ut_nufr_msg_verify_setup(void)
{
    nufr_msg_t            **head_ptr;
    nufr_msg_t            **tail_ptr;
    unsigned                i;

    nufr_tcb_t *task1 = NUFR_TID_TO_TCB(NUFR_TID_01);
    nufr_tcb_t *task2 = NUFR_TID_TO_TCB(NUFR_TID_02);
    nufr_tcb_t *task3 = NUFR_TID_TO_TCB(NUFR_TID_03);

    UT_REQUIRE(NULL == nufr_running);
    UT_REQUIRE(NULL == nufr_ready_list);
    UT_REQUIRE(NULL == nufr_ready_list_tail);
    UT_REQUIRE(NULL == task1->flink);
    UT_REQUIRE(NULL == task1->blink);
    UT_REQUIRE(NULL == task2->flink);
    UT_REQUIRE(NULL == task2->blink);
    UT_REQUIRE(NULL == task3->flink);
    UT_REQUIRE(NULL == task3->blink);

    for (i = 0; i < NUFR_CS_MSG_PRIORITIES; i++)
    {
        head_ptr = &(&task1->msg_head0)[i];
        tail_ptr = &(&task1->msg_tail0)[i];

        UT_REQUIRE(NULL == *head_ptr);
        UT_REQUIRE(NULL == *tail_ptr);

        head_ptr = &(&task2->msg_head0)[i];
        tail_ptr = &(&task2->msg_tail0)[i];

        UT_REQUIRE(NULL == *head_ptr);
        UT_REQUIRE(NULL == *tail_ptr);

        head_ptr = &(&task3->msg_head0)[i];
        tail_ptr = &(&task3->msg_tail0)[i];

        UT_REQUIRE(NULL == *head_ptr);
        UT_REQUIRE(NULL == *tail_ptr);
    }
}

void ut_nufr_msg_send_one(void)
{
    nufr_tcb_t *task = NUFR_TID_TO_TCB(NUFR_TID_01);
    nufr_msg_send_rtn_t rc;

    uint32_t fields_a = NUFR_SET_MSG_FIELDS(PREFIX_DEFAULT_A,
                                            ID_DEFAULT_A,
                                            NUFR_TID_02,          // sending task
                                            PRIORITY_DEFAULT_A);

    task->block_flags |= NUFR_TASK_BLOCKED_MSG;

    rc = nufr_msg_send(fields_a, PARAM_DEFAULT_A, NUFR_TID_01);
    UT_ENSURE(NUFR_MSG_SEND_AWOKE_RECEIVER == rc);
    nufr_running = task;                 // simulate context switch to 'task'

    
    UT_ENSURE(nufr_ready_list == task);
    UT_ENSURE(nufr_ready_list_tail == task);
    UT_ENSURE(NULL == task->flink);
    UT_ENSURE(NULL == task->blink);

    UT_ENSURE(task->msg_head0->fields == fields_a);
    UT_ENSURE(task->msg_head0->flink == NULL);
    UT_ENSURE(task->msg_head0->parameter == PARAM_DEFAULT_A);

    UT_ENSURE(task->msg_tail0 == task->msg_head0);
}

void ut_nufr_msg_send_one_inline(void)
{
    nufr_tcb_t *task = NUFR_TID_TO_TCB(NUFR_TID_01);

    task->block_flags |= NUFR_TASK_BLOCKED_MSG;

    NUFR_MSG_SEND_INLINE(NUFR_TID_01,
                         PREFIX_DEFAULT_A,
                         ID_DEFAULT_A,
                         PRIORITY_DEFAULT_A,
                         PARAM_DEFAULT_A);
    nufr_running = task;                 // simulate context switch to 'task'

    
    UT_ENSURE(nufr_ready_list == task);
    UT_ENSURE(nufr_ready_list_tail == task);
    UT_ENSURE(NULL == task->flink);
    UT_ENSURE(NULL == task->blink);

    UT_ENSURE(task->msg_head0->flink == NULL);
    UT_ENSURE(task->msg_head0->parameter == PARAM_DEFAULT_A);

    UT_ENSURE(task->msg_tail0 == task->msg_head0);
}

void ut_nufr_msg_send_two(void)
{
    nufr_tcb_t *task = NUFR_TID_TO_TCB(NUFR_TID_01);
    nufr_msg_send_rtn_t rc;

    uint32_t fields_a = NUFR_SET_MSG_FIELDS(PREFIX_DEFAULT_A,
                                            ID_DEFAULT_A,
                                            NUFR_TID_02,          // sending task
                                            PRIORITY_DEFAULT_A);

    uint32_t fields_b = NUFR_SET_MSG_FIELDS(PREFIX_DEFAULT_B,
                                            ID_DEFAULT_B,
                                            NUFR_TID_03,          // sending task
                                            PRIORITY_DEFAULT_A);

    task->block_flags |= NUFR_TASK_BLOCKED_MSG;

    rc = nufr_msg_send(fields_a, PARAM_DEFAULT_A, NUFR_TID_01);
    UT_ENSURE(NUFR_MSG_SEND_AWOKE_RECEIVER == rc);
    rc = nufr_msg_send(fields_b, PARAM_DEFAULT_B, NUFR_TID_01);
    UT_ENSURE(NUFR_MSG_SEND_OK == rc);
    nufr_running = task;                 // simulate context switch to 'task'

    UT_ENSURE(nufr_ready_list == task);
    UT_ENSURE(nufr_ready_list_tail == task);
    UT_ENSURE(NULL == task->flink);
    UT_ENSURE(NULL == task->blink);

    UT_ENSURE(task->msg_head0->fields == fields_a);
    UT_ENSURE(task->msg_head0->flink != NULL);
    UT_ENSURE(task->msg_head0->parameter == PARAM_DEFAULT_A);

    UT_ENSURE(task->msg_tail0->fields == fields_b);
    UT_ENSURE(task->msg_tail0->flink == NULL);
    UT_ENSURE(task->msg_tail0->parameter == PARAM_DEFAULT_B);
}

void ut_nufr_msg_send_two_inline(void)
{
    nufr_tcb_t *task = NUFR_TID_TO_TCB(NUFR_TID_01);

    task->block_flags |= NUFR_TASK_BLOCKED_MSG;

    NUFR_MSG_SEND_INLINE(NUFR_TID_01,
                         PREFIX_DEFAULT_A,
                         ID_DEFAULT_A,
                         PRIORITY_DEFAULT_A,
                         PARAM_DEFAULT_A);
    NUFR_MSG_SEND_INLINE(NUFR_TID_01,
                         PREFIX_DEFAULT_B,
                         ID_DEFAULT_B,
                         PRIORITY_DEFAULT_A,
                         PARAM_DEFAULT_B);
    nufr_running = task;                 // simulate context switch to 'task'

    UT_ENSURE(nufr_ready_list == task);
    UT_ENSURE(nufr_ready_list_tail == task);
    UT_ENSURE(NULL == task->flink);
    UT_ENSURE(NULL == task->blink);

    UT_ENSURE(task->msg_head0->flink != NULL);
    UT_ENSURE(task->msg_head0->parameter == PARAM_DEFAULT_A);

    UT_ENSURE(task->msg_tail0->flink == NULL);
    UT_ENSURE(task->msg_tail0->parameter == PARAM_DEFAULT_B);
}

void ut_nufr_msg_purge_one(void)
{
    nufr_tcb_t *task = NUFR_TID_TO_TCB(NUFR_TID_01);
    nufr_msg_send_rtn_t rc;
    unsigned count;

    uint32_t fields_a = NUFR_SET_MSG_FIELDS(PREFIX_DEFAULT_A,
                                            ID_DEFAULT_A,
                                            NUFR_TID_02,          // sending task
                                            PRIORITY_DEFAULT_A);

    rc = nufr_msg_send(fields_a, PARAM_DEFAULT_A, NUFR_TID_01);
    UT_ENSURE(NUFR_MSG_SEND_AWOKE_RECEIVER == rc);
    nufr_running = task;                 // simulate context switch to 'task'

    
    UT_ENSURE(nufr_ready_list == task);
    UT_ENSURE(nufr_ready_list_tail == task);
    UT_ENSURE(NULL == task->flink);
    UT_ENSURE(NULL == task->blink);

    UT_ENSURE(task->msg_head0->fields == fields_a);
    UT_ENSURE(task->msg_head0->flink != NULL);
    UT_ENSURE(task->msg_head0->parameter == PARAM_DEFAULT_A);

    UT_ENSURE(task->msg_tail0 == task->msg_head0);

    count = nufr_msg_purge(fields_a, true);

    UT_ENSURE(1 == count);
    UT_ENSURE(NULL == task->msg_head0);
    UT_ENSURE(task->msg_tail0 == task->msg_head0);
}

void ut_nufr_msg_send_occurred_test(void)
{
    //  TODO: Get this test to work
    /*
    nufr_tcb_t task = {0};
    task.block_flags = NUFR_TASK_NOT_LAUNCHED;
    
    nufr_msg_t msg = {0};
    
    nufr_msg_send_by_block(&msg, &task);    
    */
}

void ut_nufr_msg_send_multiple_test(void)
{
    //  TODO: Get this test to work
    /*
    nufr_tcb_t task = {0};
    task.block_flags = NUFR_TASK_NOT_LAUNCHED;
    
    nufr_msg_t msg1 = {0};
    nufr_msg_t msg2 = {0};
    
    nufr_msg_send_rtn_t result = nufr_msg_send_by_block(&msg1, &task);
    TEST_ENSURE(NUFR_MSG_SEND_OK == result);
    
    result = nufr_msg_send_by_block(&msg2, &task);
    TEST_ENSURE(NUFR_MSG_SEND_OK == result);
    */
}

void ut_nufr_msg_send_blocked_asleep(void)
{
    //  TODO: Get this test to work
    /*
    nufr_tcb_t task = {0};    
    task.block_flags = NUFR_TASK_BLOCKED_MSG | NUFR_TASK_NOT_LAUNCHED;
    
    nufr_msg_t msg = {0};
    
    nufr_msg_send_rtn_t result = nufr_msg_send_by_block(&msg, &task);
    TEST_ENSURE(NUFR_MSG_SEND_AWOKE_RECEIVER == result);
    TEST_ENSURE(NULL != nufr_ready_list);
    TEST_ENSURE(nufr_ready_list == &task);
    */
}

void ut_nufr_msg_send_blocked_abortable(void)
{
    //  TODO: Get this test to work
    /*
    nufr_tcb_t task = {0};
    task.priority = NUFR_TPR_NOMINAL;
    task.abort_message_priority = NUFR_TPR_NOMINAL;
    task.block_flags = NUFR_TASK_NOT_LAUNCHED | NUFR_TASK_BLOCKED_ASLEEP;
    
    nufr_msg_t msg = {0};
    NUFR_SET_MSG_PRIORITY(msg.fields, NUFR_MSG_PRI_CONTROL);
    
    
    nufr_msg_send_rtn_t result = nufr_msg_send_by_block(&msg,&task);
    TEST_ENSURE(NUFR_MSG_SEND_ABORTED_RECEIVER == result);
    TEST_ENSURE(NULL != nufr_ready_list);
    TEST_ENSURE(nufr_ready_list == &task);    
    */
}

void ut_nufr_msg_send_blocked_purge_timer(void)
{
    //  TODO: Get this test to work
    /*
    nufr_tcb_t task = {0};
    task.priority = NUFR_TPR_NOMINAL;
    task.abort_message_priority = NUFR_TPR_NOMINAL;
    task.block_flags = NUFR_TASK_NOT_LAUNCHED | NUFR_TASK_BLOCKED_ASLEEP;
    task.statuses |= NUFR_TASK_TIMER_RUNNING;
    nufr_msg_t msg = {0};
    NUFR_SET_MSG_PRIORITY(msg.fields, NUFR_MSG_PRI_CONTROL);
    
    nufr_msg_send_rtn_t result = nufr_msg_send_by_block(&msg,&task);
    TEST_ENSURE(NUFR_MSG_SEND_OK == result);
    TEST_ENSURE(NULL != nufr_ready_list);
    TEST_ENSURE(nufr_ready_list == &task);
    */
}

void ut_nufr_msg_getW(void)
{
    //  TODO: Get this test to work
    /*
    nufr_tcb_t task1 = {0};
    nufr_tcb_t task2 = {0};
    nufr_running = task1;
    
    nufrkernel_add_task_to_ready_list(&task1);
    nufrkernel_add_task_to_ready_list(&task2);
    
    nufr_msg_t msg = {0};
    nufr_msg_send_by_block(&msg,&task2);
    nufr_msg_getW();
    */
}

void ut_nufr_msg_getT(void)
{
    //  TODO: Get this test to work
    /*
    nufr_tcb_t task1 = {0};
    nufr_tcb_t task2 = {0};
    nufr_running = task1;
    
    nufrkernel_add_task_to_ready_list(&task1);
    nufrkernel_add_task_to_ready_list(&task2);
    
    nufr_msg_t msg = {0};
    nufr_msg_send_by_block(&msg,&task2);
    
    nufr_msg_getT(0);
    */
}

void ut_nufr_msg_peek(void)
{
    //  TODO: Implement this test.
    //nufr_msg_peek();
}

void ut_kernel_messaging_tests(void)
{
    ut_clean_list();
    
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_send_one();
    ut_clean_list();
            
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_send_one_inline();
    ut_clean_list();
            
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_send_two();
    ut_clean_list();
            
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_send_two_inline();
    ut_clean_list();
            
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_send_occurred_test();
    ut_clean_list();
    
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_send_multiple_test();
    ut_clean_list();
    
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_send_blocked_asleep();
    ut_clean_list();
    
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_send_blocked_abortable();
    ut_clean_list();
    
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_send_blocked_purge_timer();
    ut_clean_list();
    
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_getW();
    ut_clean_list();
    
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_getT();
    ut_clean_list();
    
    ut_nufr_msg_verify_setup();
        ut_nufr_msg_peek();
    ut_clean_list();

}
