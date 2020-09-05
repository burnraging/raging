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

//! @file    nsvc.c
//! @authors Bernie Woodland
//! @date    17Sep17
//!
//! @brief   Initialization for common code used NUFR SL (Service Layer).
//!
//! @details APIs here are called by SL, not by app code
//!

#include "nsvc-app.h"
#include "nsvc.h"
#include "nsvc-api.h"
#include "nufr-api.h"
#include "nufr-platform-app.h"
#include "raging-contract.h"


// Imposes a pool size limitation of 32
static uint32_t nsvc_sema_pool_alloc_bit_map;

//!
//! @name      nsvc_init
//
//! @brief     Nufr SL common init.
//! @brief     Must be called after nufr_init()
//!
// @param[in] 'msg_pool_empty_fcn_ptr'-- callback fcn to invoke
// @param[in]     when message block pool is empty on a msg send
//!
//void nsvc_init(nufr_msg_t *(*msg_pool_empty_fcn_ptr)(void))
void nsvc_init(void)
{
    // Clear sema pool allocations
    nsvc_sema_pool_alloc_bit_map = 0;

//    nsvc_msg_pool_empty_fcn_ptr = msg_pool_empty_fcn_ptr;
}

//!
//! @name      nsvc_sema_pool_alloc
//
//! @brief     Allocate sema needed by SL object from dynamic pool.
//!
//! @param[in] 'sema'--
//!
//! @return    'true' if success
//!
bool nsvc_sema_pool_alloc(nufr_sema_t *sema)
{
    unsigned i;

    SL_REQUIRE(NULL != sema);

    for (i = 0; i < NUFR_SEMA_POOL_SIZE; i++)
    {
        unsigned bit = 1 << i;

        if (ARE_BITS_CLR(bit, nsvc_sema_pool_alloc_bit_map))
        {
            nsvc_sema_pool_alloc_bit_map |= bit;

            *sema = (nufr_sema_t)(NUFR_SEMA_POOL_START + i);

            return true;
        }
    }

    return false;
}