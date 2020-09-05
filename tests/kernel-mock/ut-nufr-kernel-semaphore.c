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

//! @file    nufr-kernel-semaphore.c
//! @authors Bernie Woodland
//! @date    19May18

#define NUFR_SEMA_GLOBAL_DEFS

#include "nufr-global.h"

#if NUFR_CS_SEMAPHORE == 1

#include "nufr-kernel-base-semaphore.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-kernel-semaphore.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-timer.h"

#include "raging-contract.h"
#include "raging-utils.h"

//!
//!  @brief       Semaphore blocks
//!
nufr_sema_block_t nufr_sema_block[NUFR_NUM_SEMAS];


// API calls


//!
//! @name      nufrkernel_sema_reset
//!
//! @brief     Initializes a sema, to be used at bootup.
//!
//! @details   Does not do a warm reset of sema, so if tasks are
//! @details   already on sema's task wait list, they're not handled.
//! @details    Does not do a warm reset of sema, so if tasks are
//! 
//! @param[in] 'sema_block'-- task added to this sema's wait list
//! @param[in] 'initial_count'--
//!
void nufrkernel_sema_reset(nufr_sema_block_t *sema_block,
                           unsigned           initial_count,
                           bool    priority_inversion_protection)
{
}

//!
//! @name      nufrkernel_sema_link_task
//!
//! brief      Internal call to add a tcb to a sema's wait list.
//! brief      Since sema's wait list is priority sorted, insert
//! brief      must find slot in list to insert, according
//! brief      to priority.
//!
//! @details   Calling environment:
//! @details     (1) Caller must lock interrupts
//! @details     (2) This API intended for nufr kernel use
//! @details   
//!
//! @param[in] 'sema_block'-- task added to this sema's wait list
//! @param[in] 'add_tcb'--task to be added
//!
void nufrkernel_sema_link_task(nufr_sema_block_t *sema_block,
                               nufr_tcb_t        *add_tcb)
{
}

//!
//! @name      nufrkernel_sema_unlink_task
//!
//! brief      Internal call to remove a tcb to a sema's wait list.
//! brief      Assumes that tcb is on a sema wait list.
//! brief      Since sema wait list is doubly-linked (both flink and blink),
//! brief      List walk not necessary.
//!
//! @details   Calling environment:
//! @details     (1) Caller must lock interrupts
//! @details     (2) This API intended for nufr kernel use
//! @details   
//!
//! @param[in] 'sema_block'-- task removed from this sema's wait list
//! @param[in] 'add_tcb'--task to be removed
//!
void nufrkernel_sema_unlink_task(nufr_sema_block_t *sema_block,
                                 nufr_tcb_t        *delete_tcb)
{
}

//!
//! @name      nufr_sema_count_get
//
//! @details   
//!
//! @param[in] 'sema'
//!
//! @return    count
//!
unsigned nufr_sema_count_get(nufr_sema_t sema)
{
    nufr_sema_block_t *sema_block;

    sema_block = NUFR_SEMA_ID_TO_BLOCK(sema);

    UT_REQUIRE(NUFR_IS_SEMA_BLOCK(sema_block));

    // No interrupt locking needed
    return sema_block->count;
}

//!
//! @name      nufr_sema_getW
//
//! @details   Cannot be called from an ISR or from BG task
//! @details
//!
//! @param[in] 'sema'
//! @param[in] 'abort_priority_of_rx_msg'--  If a message that's of a priority greater than
//! @param[in]    'abort_priority_of_rx_msg' is sent to the waiting task's message queue, the
//! @param[in]    semaphore wait is aborted
//!
//! @return     Get result
//!
nufr_sema_get_rtn_t nufr_sema_getW(nufr_sema_t    sema,
                                   nufr_msg_pri_t abort_priority_of_rx_msg)
{
    nufr_sema_block_t *sema_block;

    sema_block = NUFR_SEMA_ID_TO_BLOCK(sema);

    UT_REQUIRE(NUFR_IS_SEMA_BLOCK(sema_block));

    // No interrupt locking needed
    sema_block->count--;

    return NUFR_SEMA_GET_OK_NO_BLOCK;
}

//!
//! @name      nufr_sema_getT
//
//! @details   Cannot be called from an ISR or from BG task
//! @details
//!
//! @param[in] 'sema'
//! @param[in] 'abort_priority_of_rx_msg'--  If a message that's of a
//! @param[in]     priority greater than 'abort_priority_of_rx_msg' is sent
//! @param[in]     to the waiting task's message queue, the semaphore wait
//! @param[in]     is aborted.
//! @param[in] 'timeout_ticks'-- Timeout in OS ticks. If ==0, no waiting
//! @param[in]                   if sema not available.
//!
//! @return     Get result
//!
nufr_sema_get_rtn_t nufr_sema_getT(nufr_sema_t    sema,
                                   nufr_msg_pri_t abort_priority_of_rx_msg,
                                   unsigned       timeout_ticks)
{
    nufr_sema_block_t *sema_block;

    sema_block = NUFR_SEMA_ID_TO_BLOCK(sema);

    UT_REQUIRE(NUFR_IS_SEMA_BLOCK(sema_block));

    // No interrupt locking needed
    sema_block->count--;

    return NUFR_SEMA_GET_OK_NO_BLOCK;
}

//!
//! @name      nufr_sema_release
//
//! @brief     Increment semaphore
//!
//! @details   Cannot be called from ISR or from Systick handler
//!
//! @param[in] 'sema'
//!
//! @return     'true' if another task was waiting on this sema
bool nufr_sema_release(nufr_sema_t sema)
{
    nufr_sema_block_t *sema_block;

    sema_block = NUFR_SEMA_ID_TO_BLOCK(sema);

    UT_REQUIRE(NUFR_IS_SEMA_BLOCK(sema_block));

    // No interrupt locking needed
    sema_block->count++;

    return false;
}

#endif  //NUFR_CS_SEMAPHORE