/*
Copyright (c) 2019, Bernie Woodland
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

//! @file     nufr-kernel-message-blocks.c
//! @authors  Bernie Woodland
//! @date     22Dec19
//!
//! @brief   message block pool direct access routines
//!
//! @details For those outside of kernel who need direct access to message
//! @details blocks. Note that kernel has allocations from block pool
//! @details which don't use these APIs throughout the kernel code.
//! @details 

#include "nufr-global.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nufr-kernel-message-blocks.h"
#include "raging-contract.h"
#include "raging-utils-mem.h"


#if NUFR_CS_MESSAGING == 1

#define NUFR_MESSAGE_BLOCKS_GLOBAL_DEFS
#include "nufr-kernel-base-messaging.h"


//! @name      NUFR_IS_MSG_BLOCK
//!
//! @brief     Sanity check that a msg block pointer is from pool
#define NUFR_IS_MSG_BLOCK(x)    ( ((x) >= nufr_msg_bpool) &&                  \
                               ((x) <= &nufr_msg_bpool[NUFR_MAX_MSGS - 1]) )
 

//! @name      nufr_msg_bpool
//!
//! @brief     Statically defined message pool

nufr_msg_t nufr_msg_bpool[NUFR_MAX_MSGS];

nufr_msg_t *nufr_msg_free_head;
nufr_msg_t *nufr_msg_free_tail;
// Means of detecting if a bpool depleted condition has occured
unsigned nufr_msg_pool_empty_count;
// Save some CPU cycles and don't update these variable
//   here and in the other code where message blocks are allocated
//unsigned    nufr_msg_free_count;
//unsigned    nufr_msg_pool_empty_count;


//! @name      nufr_msg_bpool_init
//!
//! @brief     Initialize the message block pool
//!
void nufr_msg_bpool_init(void)
{
    unsigned i;

    // Clear out all bpool variables
    rutils_memset(nufr_msg_bpool, 0, sizeof(nufr_msg_bpool));
    nufr_msg_free_head = NULL;
    nufr_msg_free_tail = NULL;
//    nufr_msg_free_count = 0;
//    nufr_msg_pool_empty_count = 0;

    for (i = 0; i < NUFR_MAX_MSGS; i++)
    {
        nufr_msg_free_block(&nufr_msg_bpool[i]);
    }
}

//! @name      nufr_msg_get_block
//!
//! @brief     Get a free message block from the pool
//!
//! @details   Can be called from task or from ISR level
//! @details   //
//! @details   
//! @details   
//!
//! @return    ptr to block, or NULL if pool depleted
nufr_msg_t *nufr_msg_get_block(void)
{
    nufr_sr_reg_t           saved_psr;
    nufr_msg_t             *msg_ptr;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    // Any messages left in pool?
    if (NULL != nufr_msg_free_head)
    {
        msg_ptr = nufr_msg_free_head;
        KERNEL_ENSURE_IL(NUFR_IS_MSG_BLOCK(msg_ptr));
        nufr_msg_free_head = msg_ptr->flink;

        if (NULL == nufr_msg_free_head)
        {
            nufr_msg_free_tail = NULL;
            KERNEL_ENSURE_IL(false);
//            nufr_msg_pool_empty_count++;
        }

//        nufr_msg_free_count--;
    }
    else
    {
        msg_ptr = NULL;
    }

    // If list is empty, both head and tail must be NULL;
    // If list is not empty, head and tail must be non-NULL.
    KERNEL_ENSURE_IL((NULL == nufr_msg_free_head) == (NULL == nufr_msg_free_tail));
    // If list is not empty, last msg on list must have a NULL flink
    KERNEL_ENSURE_IL((NULL != nufr_msg_free_tail)?
           (NULL == nufr_msg_free_tail->flink) : true);
    // If just 1 msg on list, then head's flink must be NULL
    KERNEL_ENSURE_IL((NULL != nufr_msg_free_head) &&
           (nufr_msg_free_head == nufr_msg_free_tail)?
           (NULL == nufr_msg_free_head->flink) : true);

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    if (NULL != msg_ptr)
    {
        // CPU optimization
        //rutils_memset(msg_ptr, 0, sizeof(nufr_msg_t));
        msg_ptr->flink = 0;
        msg_ptr->fields = 0;
        msg_ptr->parameter = 0;

        return msg_ptr;
    }

    return NULL;
}

//! @name      nufr_msg_free_block
//!
//! @brief
//!
//! @param[in] 'msg_ptr'
void nufr_msg_free_block(nufr_msg_t *msg_ptr)
{
    nufr_sr_reg_t         saved_psr;

    KERNEL_REQUIRE_API(NULL != msg_ptr);
    KERNEL_REQUIRE_API(NUFR_IS_MSG_BLOCK(msg_ptr));
    // Contract is that msg's flink must be cleared when msg
    // is received ('nufr_msg_getW()' and 'nufr_msg_getT()')
    // and SL and apps shouldn't mess with it.
    KERNEL_REQUIRE(NULL == msg_ptr->flink);

    if (NULL == msg_ptr)
    {
        return;
    }


    saved_psr = NUFR_LOCK_INTERRUPTS();

    if (NULL != nufr_msg_free_tail)
    {
        nufr_msg_free_tail->flink = msg_ptr;

    }
    else
    {
        nufr_msg_free_head = msg_ptr;
    }

    nufr_msg_free_tail = msg_ptr;

//    nufr_msg_free_count++;

    // If list is empty, both head and tail must be NULL;
    // If list is not empty, head and tail must be non-NULL.
    KERNEL_ENSURE_IL((NULL == nufr_msg_free_head) == (NULL == nufr_msg_free_tail));
    // If list is not empty, last msg on list must have a NULL flink
    KERNEL_ENSURE_IL((NULL != nufr_msg_free_tail)?
           (NULL == nufr_msg_free_tail->flink) : true);
    // If just 1 msg on list, then head's flink must be NULL
    KERNEL_ENSURE_IL((NULL != nufr_msg_free_head) &&
           (nufr_msg_free_head == nufr_msg_free_tail)?
           (NULL == nufr_msg_free_head->flink) : true);

    NUFR_UNLOCK_INTERRUPTS(saved_psr);
}

//!
//! @name      nufr_msg_free_count
//!
//! @brief
//!
//! @return    number of free message blocks in pool
//!
unsigned nufr_msg_free_count(void)
{
    nufr_sr_reg_t         saved_psr;
    nufr_msg_t           *msg_ptr;
    unsigned              count = 0;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    KERNEL_ENSURE_IL((NULL == nufr_msg_free_head)
                                  ==
                     (NULL == nufr_msg_free_tail));

    msg_ptr = nufr_msg_free_head;

    while (NULL != msg_ptr)
    {
        count++;
        KERNEL_ENSURE_IL(count <= NUFR_MAX_MSGS); 

        KERNEL_ENSURE_IL(NUFR_IS_MSG_BLOCK(msg_ptr));
        msg_ptr = msg_ptr->flink;
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    return count;
}

#endif  // NUFR_CS_MESSAGING