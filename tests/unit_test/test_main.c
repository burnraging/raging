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
#include <test_list.h>

//****** start old tests *****
int ut_interrupt_count;

void ut_tasks(void);
void ut_timers(void);
void ut_semaphores(void);
bool ut_ready_list_tests_old(void);
void ut_nsvc_timers(void);
void ut_nsvc_pool(void);
void ut_nsvc_pcl(void);
void ut_examples_pcl_irq_handler();
void ut_raging_utils_scan_print(void);
//****** end old tests ***


int main(void)
{     
//****** start old tests *****
    // These are the old, original task tests
    ut_tasks();
    //ut_ready_list_tests_old();   these are repeats of new file, should probably delete them

    // these tests are actually new, need to keep them somewhere.
    // They were run as manual tests, except for asserts.
    ut_timers();          // ut-nufr-timer.c
    ut_semaphores();      // ut-nufr-semaphore.c
    ut_nsvc_timers();     // ut-nsvc-timer.c
    ut_nsvc_pool();       // ut-nsvc-pool.c
    ut_nsvc_pcl();        // ut-nsvc-pcl.c
    ut_examples_pcl_irq_handler();  // ut-examples-pcl-irq-handler.c
    ut_raging_utils_scan_print();   // ut-raging-utils-scan-print.c
//****** end old tests ***

    //test_raging_utils();
    //ut_platform_tests();
    ut_kernel_messaging_tests();
    //ut_kernel_semaphore_tests();
    //ut_kernel_timer_tests();
    //ut_ready_list_tests();    
    
    return EXIT_SUCCESS;
}
