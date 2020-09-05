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

#include <nufr-kernel-base-semaphore.h>
#include <nufr-platform.h>
#include <nufr-api.h>

void ut_nufr_sema_reset(void)
{    
    //  TODO: Get this test to work
    /*
    nufr_sema_block_t block = {0};
    nufrkernel_sema_reset(&block, 0);
    */
}

void ut_nufr_sema_purge_task(void)
{
 
    nufr_sema_block_t block = {0};
    nufr_tcb_t task = {0};
    //  nufrkernel_sema_link_task(&block,task);
    //  nufrkernel_sema_unlink_task(&block,task);
    UNUSED(block);
    UNUSED(task);
    
}

void ut_nufr_sema_count_get(void)
{
    nufr_sema_t semaphore = {0};
    
    //nufr_sema_count_get(semaphore);
    UNUSED(semaphore);
}

void ut_nufr_sema_getW(void)
{
    nufr_sema_t semaphore = {0};
    //nufr_sema_getW(semaphore, NUFR_MSG_PRI_CONTROL);
    UNUSED(semaphore);
}

void ut_nufr_sema_getT(void)
{
    nufr_sema_t semaphore = {0};
    //nufr_sema_getT(semaphore, NUFR_MSG_PRI_CONTROL, 0);
    UNUSED(semaphore);
}

void ut_nufr_sema_release(void)
{
    nufr_sema_t semaphore = {0};
    //nufr_sema_release(semaphore);
    UNUSED(semaphore);
}

void ut_kernel_semaphore_tests(void)
{
    ut_nufr_sema_reset();
    ut_nufr_sema_purge_task();
    ut_nufr_sema_count_get();
    ut_nufr_sema_getW();
    ut_nufr_sema_getT();
    ut_nufr_sema_release();
}
