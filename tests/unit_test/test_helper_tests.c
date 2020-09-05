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

#include <nufr-platform.h>

void test_helper_tests(void)
{
    nufr_sr_reg_t saved_psr;

    //  Test / Cover Macros used for testing
    
    freopen("/dev/null", "w", stdout);
    
    TEST_REQUIRE(false);
    TEST_REQUIRE(true);
    
    TEST_ENSURE(false);
    TEST_ENSURE(true);
    
    freopen("/dev/stdout", "w", stdout);
    
    //  Test / Cover interrupt verification
    
    TEST_REQUIRE(ut_interrupt_count == 0);
    
    saved_psr = NUFR_LOCK_INTERRUPTS();    
    TEST_REQUIRE(ut_interrupt_count == 1);
    
    NUFR_UNLOCK_INTERRUPTS(saved_psr);    
    TEST_REQUIRE(ut_interrupt_count == 0);
    
}
