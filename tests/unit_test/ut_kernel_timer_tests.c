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


void ut_nufrkernel_update_task_timers(void)
{
        nufrkernel_update_task_timers();
}

void ut_nufrkernel_add_to_timer_list(void)
{
    //  TODO: Get this test to work
    /*
    nufr_tcb_t task = {0};
    uint32_t value = 0;
    
    nufrkernel_add_to_timer_list(&task,value);
    */
}

void ut_nufrkernel_purge_from_timer_list(void)
{
    //  TODO: Get this test to work
    /*
    nufr_tcb_t task = {0};
    
    nufrkernel_purge_from_timer_list(&task);
    */
}

void ut_kernel_timer_tests(void)
{
    ut_nufrkernel_update_task_timers();
    ut_nufrkernel_add_to_timer_list();
    ut_nufrkernel_purge_from_timer_list();
}

