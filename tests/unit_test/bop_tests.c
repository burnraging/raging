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


void ut_ready_list_bop_get_key_tests(void)
{
    nufr_tcb_t task1 = {0};
    nufr_tcb_t task2 = {0};
    
    nufrkernel_add_task_to_ready_list(&task1);
    nufrkernel_add_task_to_ready_list(&task2);
    
    nufr_running = &task1;
    uint16_t result = nufr_bop_get_key();
    TEST_REQUIRE(0 == result);
}

void ut_ready_list_bop_wait_w_tests(void)
{
    nufr_bop_wait_rtn_t result = nufr_bop_waitW(NUFR_MSG_PRI_CONTROL);
    TEST_REQUIRE(NUFR_BOP_WAIT_INVALID == result);
}

void ut_ready_list_bop_wait_t_tests(void)
{
    nufr_bop_wait_rtn_t result = nufr_bop_waitT(NUFR_MSG_PRI_CONTROL, 0);
    TEST_REQUIRE(NUFR_BOP_WAIT_INVALID == result);
}

void ut_ready_list_bop_send_tests(void)
{
    nufr_bop_rtn_t result = nufr_bop_send(NUFR_TID_01, 0);
    TEST_REQUIRE(NUFR_BOP_RTN_INVALID == result);
}

void ut_ready_list_bop_send_override_tests(void)
{
    nufr_bop_rtn_t result = nufr_bop_send_with_key_override(NUFR_TID_01);
    TEST_REQUIRE(NUFR_BOP_RTN_INVALID == result);
}

void ut_ready_list_bop_lock_waiter_tests(void)
{
    nufr_bop_rtn_t result = nufr_bop_lock_waiter(NUFR_TID_01,0);
    TEST_REQUIRE(NUFR_BOP_RTN_INVALID == result);
}

void ut_ready_list_bop_unlocker_waiter_tests(void)
{
    nufr_bop_unlock_waiter(NUFR_TID_01);
}

void ut_ready_list_bop_tests(void)
{
    //  TODO: Get this test to work
    /*
    ut_ready_list_bop_get_key_tests();
    ut_ready_list_bop_wait_w_tests();
    ut_ready_list_bop_wait_t_tests();
    ut_ready_list_bop_send_tests();
    ut_ready_list_bop_send_override_tests();
    ut_ready_list_bop_lock_waiter_tests();
    ut_ready_list_bop_unlocker_waiter_tests();
    */
}
