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
#include <nufr-platform.h>
#include <nufr-kernel-message-blocks.h>
#include <raging-contract.h>
#include <stdio.h>
#include <stddef.h>

void entry_01(unsigned);
void entry_02(unsigned);
void entry_03(unsigned);
void entry_04(unsigned);
void entry_05(unsigned);
void entry_06(unsigned);
void entry_07(unsigned);
void entry_08(unsigned);
void entry_09(unsigned);
void entry_10(unsigned);
void entry_11(unsigned);
void entry_12(unsigned);
void entry_13(unsigned);
void entry_14(unsigned);
void entry_15(unsigned);
void entry_16(unsigned);
void entry_17(unsigned);
void entry_18(unsigned);
void entry_19(unsigned);
void entry_20(unsigned);    

void ut_contract_tests(void)
{
     freopen("/dev/null", "w", stdout);
    
    UT_REQUIRE(true);
    UT_REQUIRE(false);
    
    UT_ENSURE(true);
    UT_ENSURE(false);
    
    UT_INVARIANT(true);
    UT_INVARIANT(false);
    
    freopen("/dev/stdout", "w", stdout);
}

void ut_dummy_entry_point_test(void)
{
    entry_01(0);
    entry_02(0);
    entry_03(0);
    entry_04(0);
    entry_05(0);
    entry_06(0);
    entry_07(0);
    entry_08(0);
    entry_09(0);
    entry_10(0);
    entry_11(0);
    entry_12(0);
    entry_13(0);
    entry_14(0);
    entry_15(0);
    entry_16(0);
    entry_17(0);
    entry_18(0);
    entry_19(0);
    entry_20(0);    
}

void ut_platform_tests(void)
{
   //ut_contract_tests();
   ut_dummy_entry_point_test();
   
   nufr_init();
   nufrplat_task_exit_point();
   nufrplat_task_get_desc(NULL,NUFR_TID_null);
   nufrplat_systick_handler();
   nufr_msg_get_block();
   nufr_msg_free_block(NULL);
   nufr_msg_free_count();
   //nufrplat_msg_prefix_id_to_tid(0);
}
