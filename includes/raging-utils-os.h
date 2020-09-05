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

//! @file     raging-utils-os.h
//! @authors  Bernie Woodland
//! @date     27Jan18
//! @brief    Utilities which require NUFR hooks or which support OS

#ifndef _RAGING_UTILS_OS_H_
#define _RAGING_UTILS_OS_H_

#include "raging-global.h"

#include "nufr-api.h"


//!
//! @struct    rutils_fifo_t
//!
//! @brief     Circular buffer handle
//!
typedef struct
{
    uint8_t *buffer;
    uint16_t buffer_length;
    uint16_t head_index;
    uint16_t tail_index;
    uint16_t used_length;
} rutils_fifo_t;


RAGING_EXTERN_C_START
bool rutils_atomic_test_and_set(bool *flag_ptr);
bool rutils_atomic_test_and_setW(bool          *flag_ptr,
                                 nufr_msg_pri_t abort_priority);

void rutils_fifo_init(rutils_fifo_t *fifo_ptr,
                      uint8_t       *buffer,
                      uint16_t       buffer_length);
void rutils_fifo_flush(rutils_fifo_t *fifo_ptr);
unsigned rutils_fifo_write(rutils_fifo_t *fifo_ptr,
                           const uint8_t *data_ptr,
                           unsigned       data_length);
unsigned rutils_fifo_read(rutils_fifo_t *fifo_ptr,
                          uint8_t       *data_ptr,
                          unsigned       max_read_length);
RAGING_EXTERN_C_END

#endif