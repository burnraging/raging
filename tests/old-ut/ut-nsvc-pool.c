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

//! @file    ut-nsvc-pool.c
//! @authors Bernie Woodland
//! @date    24Nov17

// Tests SL Generic Pool functionality (nsvc-pool.c)

#include "nufr-global.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nsvc-app.h"
#include "nsvc-api.h"

#include "raging-contract.h"

#define BUFFER_SIZE    52
#define BLOCK_SIZE     (BUFFER_SIZE + 4)
#define NUM_BLOCKS      5


nsvc_pool_t test_pool;

uint8_t test_blocks[NUM_BLOCKS][BLOCK_SIZE];



void pool_init(void)
{
    void     **flink_ptr;

    test_pool.pool_size = NUM_BLOCKS;
    test_pool.element_size = BLOCK_SIZE;
    test_pool.element_index_size = test_blocks[1] - test_blocks[0];
    test_pool.base_ptr = test_blocks[0];
    test_pool.flink_offset = BUFFER_SIZE;

    flink_ptr = NSVC_POOL_FLINK_PTR(&test_pool, test_blocks[0]);
    SL_INVARIANT(IS_ALIGNED32(flink_ptr));

    nsvc_pool_init(&test_pool);
}

void test_pool_alloc_free(void)
{
    uint8_t *block_ptr1;
    uint8_t *block_ptr2;
    uint8_t *block_ptr3;
    uint8_t *block_ptr4;
    uint8_t *block_ptr5;

    pool_init();

    block_ptr1 = nsvc_pool_allocate(&test_pool, false);
    block_ptr2 = nsvc_pool_allocate(&test_pool, false);
    block_ptr3 = nsvc_pool_allocate(&test_pool, false);
    block_ptr4 = nsvc_pool_allocate(&test_pool, false);
    block_ptr5 = nsvc_pool_allocate(&test_pool, false);

    nsvc_pool_free(&test_pool, block_ptr3);
    nsvc_pool_free(&test_pool, block_ptr2);
    nsvc_pool_free(&test_pool, block_ptr1);
}
    

void ut_nsvc_pool(void)
{
    test_pool_alloc_free();
}