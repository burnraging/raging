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

//! @file     example-serial-driver.c
//! @authors  Bernie Woodland
//! @date     10Jun18
//! @brief    Example driver(s) for RNET serial interface

#include "raging-global.h"
#include "raging-utils-os.h"
#include "nufr-platform.h"
#include "nufr-kernel-message-send-inline.h"
#include "nsvc-api.h"

#define SD_RX_CIRCULAR_BUFFER_SIZE   3000
#define TEMP_BUFFER_SIZE               40

#define SD_MIN_CONTIGUOUS               6

rutils_fifo_t sd_rx_fifo_control;
uint8_t       sd_rx_circular_buffer[SD_RX_CIRCULAR_BUFFER_SIZE];

#define BIT_RX_FIFO_NOT_EMPTY  0x00000001
nufr_sr_reg_t   sd_register1;      // uart status register
nufr_sr_reg_t   sd_register2;      // uart rx fifo register


// Called at task level
void sd_init(void)
{
    rutils_fifo_init(&sd_rx_fifo_control,
                     sd_rx_circular_buffer,
                     sizeof(sd_rx_circular_buffer));
}

// Called from Rx IRQ
void sd_unload_rx_fifo(uint8_t *string, unsigned length)
{
    uint8_t      temp_buffer[TEMP_BUFFER_SIZE];
    uint8_t      character;
    unsigned     temp_length;
    unsigned     bytes_written;
    bool         fifo_not_empty;
    bool         need_to_send_notification = false;
    nufr_msg_send_rtn_t srv;

    // Get all bytes out of h/w FIFO
    while (1)
    {
        temp_length = 0;
        fifo_not_empty = 0 != (BIT_RX_FIFO_NOT_EMPTY & sd_register1);

        if (!fifo_not_empty)
        {
            break;
        }

        // Pull bytes out of h/w FIFO and load into 'temp_buffer'
        while ( fifo_not_empty && (temp_length < TEMP_BUFFER_SIZE - 1) )
        {
            character = (uint8_t) sd_register2;
            temp_buffer[temp_length++] = character;

            if (RNET_AHDLC_FLAG_SEQUENCE == character)
            {
                if (sd_rx_contiguous_count > SD_MIN_CONTIGUOUS)
                {
                    need_to_send_notification = true;
                }

                sd_rx_contiguous_count = 0;
            }
            else
            {
                sd_rx_contiguous_count++;
            }

            fifo_not_empty = 0 != (BIT_RX_FIFO_NOT_EMPTY & sd_register1);
        }

        // Unload 'temp_buffer' into Rx FIFO
        bytes_written = rutils_fifo_write(&sd_rx_fifo_control,
                                          string,
                                          temp_length);
    }

    // Do we need to inform task?
    // If yes, send a message
    if (need_to_send_notification)
    {
        // Fast message send, used in IRQ's
        NUFR_MSG_SEND_INLINE(FILL_IN_DEST_TID,
                             FILL_IN_MSG_PREFIX,
                             FILL_IN_MSG_ID,
                             NUFR_MSG_PRI_MID,
                             0);
    }
}