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
#include <nufr-kernel-message-blocks.h>

int ut_interrupt_count;

void onTestRequireFailure(uint8_t* file, uint32_t line)
{
    printf("<<<<< TEST UT_REQUIRE FAILURE >>>>> in %s on line %u\r\n",file,line);
    fflush(stdout);
}

void onTestEnsureFailure(uint8_t* file, uint32_t line)
{
    printf("<<<<< TEST UT_ENSURE FAILURE >>>>> in %s on line %u\r\n",file,line);
    fflush(stdout);
}


void ut_clean_list(void)
{
    int index;

    nufr_ready_list = nufr_ready_list_tail = nufr_ready_list_tail_nominal = nufr_running = NULL;
    memset(nufr_tcb_block, 0, sizeof(nufr_tcb_block));
    
    TEST_REQUIRE(NULL == nufr_ready_list);
    TEST_REQUIRE(NULL == nufr_ready_list_tail_nominal);
    TEST_REQUIRE(NULL == nufr_ready_list_tail);
    TEST_REQUIRE(NULL == nufr_running);
    
    for(index = 0; index < NUFR_NUM_TASKS; index++)
    {
        nufr_tcb_block[index].flink = NULL;
    }

    nufr_msg_free_head = nufr_msg_free_tail = NULL;
    //nufr_msg_free_count = 0;
    nufr_msg_pool_empty_count = 0;
    nufr_msg_bpool_init();
}
