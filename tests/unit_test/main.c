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
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

/*  include tests for test suites */
#include <task_tests.h>
#include <test_helper.h>
#include "test_list.h"

int ut_interrupt_count;

CU_ErrorCode ut_setup_ready_list_tests(void);



int main(int argc, char *arg[])
{
    (void)(argc);
    (void)(arg);
   
    CU_ErrorCode result = CUE_SUCCESS;

    /*  Initialize the CUnit Test Registry */
    result = CU_initialize_registry();
    if (CUE_SUCCESS == result)
    {
        result = ut_setup_ready_list_tests();
     
        ut_kernel_timer_tests();   
        ut_kernel_semaphore_tests();
    }
    else
    {
        result = CU_get_error();
    }

    /*  Run all tests using the basic interface */

    /* Verbose output */
    //CU_basic_set_mode(CU_BRM_VERBOSE);
    
    /* Summary and failures only */
    CU_basic_set_mode(CU_BRM_NORMAL);

    /* Basic Run and exit */
    CU_basic_run_tests();
    
    /*  Interactive run */

    /*  Console Mode */
    //CU_console_run_tests();

    /*  NCurses Mode */
    //CU_curses_run_tests();

    CU_cleanup_registry();
    result = CU_get_error();

    return result;
}