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

//! @file     nufr-kernel-message-blocks.h
//! @authors  Bernie Woodland
//! @date     22Dec19
//!
//! @brief   message block pool direct access routines
//!

#ifndef NUFR_KERNEL_MESSAGE_BLOCKS_H_
#define NUFR_KERNEL_MESSAGE_BLOCKS_H_

#include "nufr-global.h"
#include "nufr-kernel-base-messaging.h"

#if NUFR_CS_MESSAGING == 1


#ifndef NUFR_MESSAGE_BLOCKS_GLOBAL_DEFS
    extern nufr_msg_t *nufr_msg_free_head;
    extern nufr_msg_t *nufr_msg_free_tail;
    extern unsigned nufr_msg_pool_empty_count;
#endif  //NUFR_MESSAGE_BLOCKS_GLOBAL_DEFS


RAGING_EXTERN_C_START

void nufr_msg_bpool_init(void);
nufr_msg_t *nufr_msg_get_block(void);
void nufr_msg_free_block(nufr_msg_t *msg_ptr);
unsigned nufr_msg_free_count(void);

RAGING_EXTERN_C_END

#endif  // NUFR_CS_MESSAGING == 1
#endif  //NUFR_KERNEL_MESSAGE_BLOCKS_H_