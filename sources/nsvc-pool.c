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

//! @file    nsvc-pool.c
//! @authors Bernie Woodland
//! @date    1Oct17
//!
//! @brief   NUFR SL Generic Pool
//!
//! @details    nsvc-pool manages a pool of elements.
//! @details    The pool is comprised of elements.
//! @details    The elements lie as an array in a contiguous chunk of memory.
//! @details    Each element is the same size.
//! @details    Each element has an flink ptr. Flink ptr is at a fixed offset.
//! @details 

#include "nsvc.h"
#include "nsvc-app.h"
#include "nsvc-api.h"

#include "nufr-api.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nufr-kernel-semaphore.h"

#include "raging-contract.h"
#include "raging-utils-mem.h"


#define SUCCESS_ALLOC(rv)     ( (NUFR_SEMA_GET_OK_NO_BLOCK == (rv)) ||       \
                                (NUFR_SEMA_GET_OK_BLOCK == (rv)) )

//!
//! @name      nsvc_pool_init
//!
//! @brief     Initialize a pool
//!
//! @param[in] 'pool_ptr'--pool to be initialized.
//! @param[in]    These members must be initialized prior to init:
//! @param[in]       ->pool_size
//! @param[in]       ->element_size
//! @param[in]       ->element_index_size
//! @param[in]       ->base_ptr
//! @param[in]       ->flink_offset
//! @param[in]    Caller must clear all other members
//!
void nsvc_pool_init(nsvc_pool_t *pool_ptr)
{
    unsigned           i;
    bool               rv;
    void              *element_ptr;

    SL_REQUIRE_API(NULL != pool_ptr);
    SL_REQUIRE_API(pool_ptr->pool_size > 0);
    //must be big enough to hold an flink ptr, then 1 spare byte min
    SL_REQUIRE_API(pool_ptr->element_size > sizeof(uint32_t *));
    //element index size is element size aligned up to 4-byte word boundary
    SL_REQUIRE_API(pool_ptr->element_index_size >= pool_ptr->element_size);
    SL_REQUIRE_API(pool_ptr->element_index_size - pool_ptr->element_size < BYTES_PER_WORD32);
    SL_REQUIRE_API(pool_ptr->flink_offset <= (pool_ptr->element_size - BYTES_PER_WORD32));
    SL_REQUIRE_API(ALIGN32(pool_ptr->flink_offset) == pool_ptr->flink_offset);

    rv = nsvc_sema_pool_alloc(&pool_ptr->sema);
    SL_REQUIRE_API(rv);
    UNUSED_BY_ASSERT(rv);
    pool_ptr->sema_block = NUFR_SEMA_ID_TO_BLOCK(pool_ptr->sema);

    // One sema count per block in pool. No mutual exclusion possible.
    nufrkernel_sema_reset(pool_ptr->sema_block, NUFR_MAX_MSGS, false);

    // Clear entire element array
    rutils_memset(pool_ptr->base_ptr, 0,
                  pool_ptr->element_index_size * pool_ptr->pool_size);

    // Populate pool
    for (i = 0; i < pool_ptr->pool_size; i++)
    {
        element_ptr = (uint8_t *)pool_ptr->base_ptr +
                                 pool_ptr->element_index_size * i;

        nsvc_pool_free(pool_ptr, element_ptr);
    }
}

//!
//! @name      nsvc_pool_is_element
//!
//! @brief     Sanity check that element is valid
//!
//! @param[in] 'pool_ptr'--
//! @param[in] 'element_ptr'-- pool element to check validity of
//!
//! @return    'true' upon success
//!
bool nsvc_pool_is_element(nsvc_pool_t *pool_ptr, void *element_ptr)
{
    unsigned delta;
    unsigned index;
    unsigned index_size;
    uint8_t *base_ptr = (uint8_t *)pool_ptr->base_ptr;
    uint8_t *e_ptr = (uint8_t *)element_ptr;

    SL_REQUIRE_API(NULL != pool_ptr);
    SL_REQUIRE_API(NULL != element_ptr);

    // Within min bound?
    if (e_ptr < base_ptr)
    {
        return false;
    }

    delta = (unsigned)(e_ptr - base_ptr);

    index_size = pool_ptr->element_index_size;
    SL_REQUIRE_API(index_size > 0);

    index = delta / index_size;

    // Within max bound?
    if (index >= pool_ptr->pool_size)
    {
        return false;
    }
    // Aligned?
    else if (index * index_size != delta)
    {
        return false;
    }

    return true;
}

//!
//! @name      nsvc_pool_free
//!
//! @brief     Return an element back to the pool
//!
//! @details   Callable from ISR...may not be prudent though.
//! @details   When element is returned to pool, the pool
//! @details   sema must be incremented.
//!
//! @param[in] 'pool_ptr'--
//! @param[in] 'element_ptr'-- pool element to return
//!
void nsvc_pool_free(nsvc_pool_t *pool_ptr, void *element_ptr)
{
    nufr_sr_reg_t   saved_psr;
    void          **element_flink_ptr;
    void          **tail_flink_ptr;

    SL_REQUIRE_API(nsvc_pool_is_element(pool_ptr, element_ptr));

    element_flink_ptr = NSVC_POOL_FLINK_PTR(pool_ptr, element_ptr);
    *element_flink_ptr = NULL;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    // If head is null, tail must be also
    // If head is non-null, tail must be also
    SL_ENSURE_IL((NULL == pool_ptr->head_ptr) == (NULL == pool_ptr->tail_ptr));

    // Free list empty? Assign head and tail to this pcl
    if (NULL == pool_ptr->head_ptr)
    {
        pool_ptr->head_ptr = element_ptr;
        pool_ptr->tail_ptr = element_ptr;
    }
    // Else, 1+ items on list. Append to tail.
    else
    {
        tail_flink_ptr = NSVC_POOL_FLINK_PTR(pool_ptr, pool_ptr->tail_ptr);
        SL_ENSURE_IL(NULL == *tail_flink_ptr);
        *tail_flink_ptr = element_ptr;

        pool_ptr->tail_ptr = element_ptr;
    }

    pool_ptr->free_count++;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    // Callable from ISR, but consider CPU cycles
    (bool)nufr_sema_release(pool_ptr->sema);
}

//!
//! @name      nsvc_pool_allocate
//!
//! @brief     Allocated element from pool.
//!
//! @details   Callable from ISR.
//! @details   No blocking.
//!
//! @param[in] 'pool_ptr'--
//! @param[in] 'called_from_ISR'-- set 'true' if pool's sema
//! @param[in]         should be decremented by this alloc, if
//! @param[in]       'nsvc_pool_allocate()' is called directly, skipping
//! @param[in]       'nsvc_pool_allocateW()'
//!
//! @return    pointer to element allocated. NULL if no allocation.
//!
void *nsvc_pool_allocate(nsvc_pool_t *pool_ptr, bool called_from_ISR)
{
    nufr_sr_reg_t     saved_psr;
    void             *element_ptr = NULL;
    void            **element_flink_ptr = NULL;

    SL_REQUIRE_API(NULL != pool_ptr);

    // Lock interrupts while called from ISR too;
    //  can have nested interrupts
    saved_psr = NUFR_LOCK_INTERRUPTS();

    // If head is null, tail must be also
    // If head is non-null, tail must be also
    SL_ENSURE_IL((NULL == pool_ptr->head_ptr) == (NULL == pool_ptr->tail_ptr));

    // Any elements available? If so, take from head
    if (NULL != pool_ptr->head_ptr)
    {
        element_ptr = pool_ptr->head_ptr;
        element_flink_ptr = NSVC_POOL_FLINK_PTR(pool_ptr, element_ptr);

        // No elements after head element?
        if (NULL == *element_flink_ptr)
        {
            pool_ptr->head_ptr = NULL;
            pool_ptr->tail_ptr = NULL;
        }
        else
        {
            pool_ptr->head_ptr = *element_flink_ptr;
        }

        SL_ENSURE_IL(pool_ptr->free_count > 0);
        pool_ptr->free_count--;

        // ISR makes direct alloc call; must update sema.
        // Sema count must be mainted in sync with free count
        if (called_from_ISR)
        {
            SL_REQUIRE_IL(pool_ptr->sema_block->count > 0);

            // Bypass nufr API calls to save CPU cycles
            pool_ptr->sema_block->count--;
        }
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    // Allocate successful?
    if (NULL != element_ptr)
    {
        SL_REQUIRE(nsvc_pool_is_element(pool_ptr, element_ptr));
        SL_REQUIRE(NULL != element_flink_ptr);

        if (called_from_ISR)
        {
            // Skip memory clearing at ISR level, takes too long.
            // But we do need to clear element's flink member.
            *element_flink_ptr = 0;
        }
        else
        {
            rutils_memset(element_ptr, 0, pool_ptr->element_size);

            // above memset will have cleared element's flink
            SL_REQUIRE(NULL == *NSVC_POOL_FLINK_PTR(pool_ptr, element_ptr));
        }
    }

    return element_ptr;
}

//!
//! @name      nsvc_pool_allocateW
//!
//! @brief     Wrapper for 'nsvc_pool_allocateW()' which makes calling
//! @brief     task block until an element is freed by another task,
//! @brief     if pool is deleted.
//!
//! @details   Not callable from ISR, from BG Task, or from OS Tick handler
//!
//! @param[in]  'pool_ptr'--
//! @param[out] 'element_ptr'-- allocated element.
//! @param[out]      If NUFR_CS_TASK_KILL is turned off, will always get
//! @param[out]      assigned. 
//!
//! @return    Result of alloc op. Indicates if message abort
//! @return    happened (relevant onl if NUFR_CS_TASK_KILL is on).
//!
nufr_sema_get_rtn_t nsvc_pool_allocateW(nsvc_pool_t   *pool_ptr,
                                        void         **element_ptr)
{
    nufr_sema_get_rtn_t  return_value;

    SL_REQUIRE_API(NULL != pool_ptr);
    SL_REQUIRE_API(NULL != element_ptr);

#if NUFR_CS_TASK_KILL == 1
    // assume highest priority message is a message abort
    return_value = nufr_sema_getW(pool_ptr->sema, (nufr_msg_pri_t) 1);

    // If sema wait aborted, mustn't try to alloc buffer, as
    //   sema count vs. free count could get out of sync.
    if (!SUCCESS_ALLOC(return_value))
    {
        return return_value;
    }
#else
    // no message abort
    return_value = nufr_sema_getW(pool_ptr->sema, NUFR_NO_ABORT);
#endif  //NUFR_CS_TASK_KILL

    *element_ptr = nsvc_pool_allocate(pool_ptr, false);
    SL_ENSURE(NULL != *element_ptr);

    return return_value;
}

//!
//! @name      nsvc_pool_allocateT
//!
//! @brief     Same as 'nsvc_pool_allocateW()', but with timeout.
//!
//! @details   Not callable from ISR, from BG Task, or from OS Tick handler.
//!
//! @param[in]  'pool_ptr'--
//! @param[out] 'element_ptr'-- 
//! @param[in]  'timeout_ticks'-- timeout to wait for message.
//! @param[in]        Set to '0' for element get with no blocking allowed.
//!
//! @return    (see above)
//!
nufr_sema_get_rtn_t nsvc_pool_allocateT(nsvc_pool_t   *pool_ptr,
                                        void         **element_ptr,
                                        unsigned       timeout_ticks)
{
    nufr_sema_get_rtn_t  return_value;

    SL_REQUIRE_API(NULL != pool_ptr);
    SL_REQUIRE_API(NULL != element_ptr);

#if NUFR_CS_TASK_KILL == 1
    // assume highest priority message is a message abort
    return_value = nufr_sema_getT(pool_ptr->sema, (nufr_msg_pri_t) 1,
                                  timeout_ticks);
#else
    // no message abort
    return_value = nufr_sema_getT(pool_ptr->sema, NUFR_NO_ABORT,
                                  timeout_ticks);
#endif  //NUFR_CS_TASK_KILL

    if (SUCCESS_ALLOC(return_value))
    {
        *element_ptr = nsvc_pool_allocate(pool_ptr, false);
        SL_ENSURE(NULL != *element_ptr);
    }

    return return_value;
}