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

//! @file     ut-nufr-wrapper.c
//! @authors  Bernie Woodland
//! @date     12Aug17
//!
//! @brief    Wrapper for ut-nufr* project
//!

#error  "!!!!!! this file is no longer used !!!!"
#error  "!!!!!! is being replaced by tests/test_main.c "
#error  " delete this file as soon as things are settled again"

#define EXIT_SUCCESS 0

#include "nufr-global.h"
#include "nufr-platform.h"



int ut_interrupt_count;

void ut_tasks(void);
void ut_timers(void);
void ut_semaphores(void);
bool ut_ready_list_tests(void);
void ut_nsvc_timers(void);
void ut_nsvc_pool(void);
void ut_nsvc_pool(void);

int main(void)
{

    if(ut_ready_list_tests())
    {
        ut_tasks();
    }

    ut_timers();
    ut_semaphores();
    ut_nsvc_timers();
    ut_nsvc_pool();
    ut_nsvc_pool();

    return EXIT_SUCCESS;
}
