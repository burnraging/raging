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

//! @file     nufr-kernel-semaphore.h
//! @authors  Bernie Woodland
//! @date     30Aug17
//!
//! @brief   Sema-related definitions that are only exported
//! @brief   to nufr platform and nufr kernel, but not to app layers
//!
//! @details 

#ifndef NUFR_KERNEL_SEMAPHORE_H
#define NUFR_KERNEL_SEMAPHORE_H

#include "nufr-global.h"
#include "nufr-kernel-base-semaphore.h"
#include "nufr-platform-app.h"


#define NUFR_SEMA_ID_TO_BLOCK(x)   ( &nufr_sema_block[(x) - 1] )
#define NUFR_SEMA_BLOCK_TO_ID(y)   ( (nufr_sema_t) ((unsigned)((y) - &nufr_sema_block[0]) + 1) )

#define NUFR_IS_SEMA_BLOCK(x)      ( ((x) >= nufr_sema_block) &&              \
                              ((x) <= &nufr_sema_block[NUFR_NUM_SEMAS - 1]) )


#ifndef NUFR_SEMA_GLOBAL_DEFS
    extern nufr_sema_block_t nufr_sema_block[NUFR_NUM_SEMAS];
#endif  //NUFR_SEMA_GLOBAL_DEFS


//  APIs
RAGING_EXTERN_C_START
void nufrkernel_sema_reset(nufr_sema_block_t *sema_block,
                           unsigned           initial_count,
                           bool    priority_inversion_protection);
void nufrkernel_sema_link_task(nufr_sema_block_t *sema_block,
                               nufr_tcb_t        *add_tcb);
void nufrkernel_sema_unlink_task(nufr_sema_block_t *sema_block,
                                 nufr_tcb_t        *delete_tcb);
RAGING_EXTERN_C_END

#endif  //NUFR_KERNEL_SEMAPHORE_H