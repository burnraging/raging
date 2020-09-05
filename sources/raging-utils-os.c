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

//! @file     raging-utils-os.c
//! @authors  Bernie Woodland
//! @date     27Jan18
//! @brief    Utilities which require NUFR hooks or which support OS


#include "raging-global.h"
#include "raging-utils-mem.h"
#include "raging-utils-os.h"

#include "nufr-platform.h"
#include "nufr-api.h"

#include <stddef.h>

//!
//! @name      rutils_atomic_test_and_set
//!
//! @brief     Assume control of a global boolean
//!
//! @details   Global boolean has one task or IRQ which owns it.
//! @details   Owner sets value to 'true'. This fcn. allows
//! @details   a task to take ownership.
//!
//! @param[in] 'flag_ptr'-- global boolean
//!
//! @return    'true' if control is taken
//
bool rutils_atomic_test_and_set(bool *flag_ptr)
{
    nufr_sr_reg_t           saved_psr;
    bool                    local_flag;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    local_flag = *flag_ptr;

    if (!local_flag)
    {
        *flag_ptr = true;

        local_flag = true;
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    return local_flag;
}

//!
//! @name      rutils_atomic_test_and_setW
//!
//! @brief     Assume control of a global boolean.
//! @brief     Block (spin, actually) until control taken.
//!
//! @param[in] 'flag_ptr'-- global boolean
//! @param[in] 'abort_priority'-- msg priority to set if
//! @param[in]    for abort. Set to NUFR_NO_ABORT to ignore.
//!
//! @return    'true' if a message abort occured
//!
bool rutils_atomic_test_and_setW(bool          *flag_ptr,
                                 nufr_msg_pri_t abort_priority)
{
    bool ownership_flag;
    bool did_abort = false;

    do
    {
        ownership_flag = rutils_atomic_test_and_set(flag_ptr);

        if (!ownership_flag)
        {
            did_abort = nufr_sleep(1, abort_priority);

            if (did_abort)
            {
                break;
            }
        }

    } while (!ownership_flag);

    return did_abort;
}

//!
//! @name      rutils_fifo_init
//!
//! @brief     Circular buffer (FIFO) init
//!
//! @param[out] 'fifo_ptr'-- circular buffer
//! @param[in] 'buffer'-- RAM area for actual buffer, passed in
//! @param[in] 'buffer_length'-- size of 'buffer'
//!
void rutils_fifo_init(rutils_fifo_t *fifo_ptr,
                      uint8_t       *buffer,
                      uint16_t       buffer_length)
{
    rutils_memset(fifo_ptr, 0, sizeof(rutils_fifo_t));

    fifo_ptr->buffer = buffer;
    fifo_ptr->buffer_length = buffer_length;
}

//!
//! @name      rutils_fifo_flush
//!
//! @brief     Circular buffer (FIFO) flush. Clears all data
//!
//! @param[out] 'fifo_ptr'-- circular buffer
//!
void rutils_fifo_flush(rutils_fifo_t *fifo_ptr)
{
    nufr_sr_reg_t         saved_psr;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    fifo_ptr->head_index = 0;
    fifo_ptr->tail_index = 0;
    fifo_ptr->used_length = 0;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);
}

//!
//! @name      rutils_fifo_write
//!
//! @brief     Circular buffer (FIFO) write
//!
//! @details   Writes data to head index of CB.
//! @details   head index always points to next write location
//! @details   tail index always points to next read location
//! @details   If head and tail are the same, CB is either
//! @details   full or empty.
//!
//! @param[in] 'fifo_ptr'-- circular buffer
//! @param[out] 'fifo_ptr'-- ...fields updated
//! @param[in] 'data_ptr'-- data to be written
//! @param[in] 'data_length'-- bytes to write
//!
//! @return    Number of bytes written
//!
unsigned rutils_fifo_write(rutils_fifo_t *fifo_ptr,
                           const uint8_t *data_ptr,
                           unsigned       data_length)
{
    nufr_sr_reg_t         saved_psr;
    unsigned head_index;
    unsigned tail_index;
    unsigned used_length;
    unsigned buffer_length = fifo_ptr->buffer_length;
    unsigned original_data_length = data_length;
    unsigned len;


    saved_psr = NUFR_LOCK_INTERRUPTS();

    used_length = fifo_ptr->used_length;
    head_index = fifo_ptr->head_index;
    tail_index = fifo_ptr->tail_index;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    if (0 == data_length)
        return 0;
    else if (used_length == buffer_length)
        return 0;


    // Write from head to end of fifo?
    if (head_index >= tail_index)
    {
        len = buffer_length - head_index;
        if (len > data_length)
            len = data_length;

        rutils_memcpy(&fifo_ptr->buffer[head_index], data_ptr, len);

        data_ptr += len;
        data_length -= len;
        head_index += len;
        used_length += len;

        // Did we fill to end of CB? Wrap head
        if (head_index == buffer_length)
            head_index = 0;

    }

    // Start/continue write from tail to head?
    if (data_length > 0)
    {
        len = tail_index - head_index;
        if (len > data_length)
            len = data_length;

        rutils_memcpy(&fifo_ptr->buffer[head_index], data_ptr, len);

        //data_ptr += len;
        data_length -= len;
        head_index += len;
        used_length += len;
    }

    saved_psr = NUFR_LOCK_INTERRUPTS();

    fifo_ptr->head_index = (uint16_t)head_index;
    fifo_ptr->used_length = (uint16_t)used_length;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    return original_data_length - data_length;
}

//!
//! @name      rutils_fifo_read
//!
//! @brief     Circular buffer (FIFO) read
//!
//! @details   (see rutils_fifo_write)
//!
//! @param[in] 'fifo_ptr'-- circular buffer
//! @param[out] 'fifo_ptr'-- ...fields updated
//! @param[in] 'data_ptr'-- read data put here
//! @param[in] 'max_read_length'-- size of 'data_ptr'/max bytes to read
//!
//! @return    Number of bytes read
//!
unsigned rutils_fifo_read(rutils_fifo_t *fifo_ptr,
                          uint8_t       *data_ptr,
                          unsigned       max_read_length)
{
    nufr_sr_reg_t         saved_psr;
    unsigned head_index;
    unsigned tail_index;
    unsigned used_length;
    unsigned buffer_length = fifo_ptr->buffer_length;
    unsigned original_data_length = max_read_length;
    unsigned len;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    used_length = fifo_ptr->used_length;
    head_index = fifo_ptr->head_index;
    tail_index = fifo_ptr->tail_index;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    if (0 == max_read_length)
        return 0;
    else if (used_length == 0)
        return 0;


    // Read from head to end of fifo?
    if (tail_index >= head_index)
    {
        len = buffer_length - tail_index;
        if (len > max_read_length)
            len = max_read_length;

        rutils_memcpy(data_ptr, &fifo_ptr->buffer[tail_index], len);

        data_ptr += len;
        max_read_length -= len;
        tail_index += len;
        used_length -= len;

        // Did we fill to end of CB? Wrap head
        if (tail_index == buffer_length)
            tail_index = 0;

    }

    // Start/continue read from tail to head?
    if (max_read_length > 0)
    {
        len = head_index - tail_index;
        if (len > max_read_length)
            len = max_read_length;

        rutils_memcpy(data_ptr, &fifo_ptr->buffer[tail_index], len);

        //data_ptr += len;
        max_read_length -= len;
        tail_index += len;
        used_length -= len;
    }

    saved_psr = NUFR_LOCK_INTERRUPTS();

    fifo_ptr->tail_index = (uint16_t)tail_index;
    fifo_ptr->used_length = (uint16_t)used_length;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    return original_data_length - max_read_length;
}