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

#include "nsvc-api.h"
#include "nufr-platform.h"

#include "raging-utils-mem.h"

// Assume 'UART_ID_GOOD_PACKET','UART_ID_DISCARD_PACKET'
//   are task-defined enums
// Assume NUFR_TID_UART_TASK is task receiving messages
#define UART_ID_GOOD_PACKET                1
#define UART_ID_DISCARD_PACKET             1
#define NUFR_TID_UART_TASK     (nufr_tid_t)1

extern nsvc_pool_t nsvc_pcl_pool;

// Hard-coding these message fields values saves a few
// CPU cycles
const uint32_t good_packet_fields =
               NUFR_SET_MSG_FIELDS(
                           NSVC_MSG_PREFIX_local,
                           UART_ID_GOOD_PACKET,
                           NUFR_TID_null,
                           NUFR_MSG_PRI_MID);

const uint32_t discard_packet_fields =
                NUFR_SET_MSG_FIELDS(
                           NSVC_MSG_PREFIX_local,
                           UART_ID_DISCARD_PACKET,
                           NUFR_TID_null,
                           NUFR_MSG_PRI_MID);

static nsvc_pcl_t *uart_rx_chain;
static nsvc_pcl_chain_seek_t uart_rx_seeker;

// Length of current packet
static unsigned uart_rx_packet_length;

// Set to 'true' when an alloc fails.
// This flag causes discarding of all rx chars until end of packet
static bool discarding;


//
// Call once at first rx of packet, before first 'uart_irq_data_rx()'
//
void uart_irq_packet_start(void)
{
    nsvc_pcl_header_t *header_ptr;

    // If 'uart_rx_chain' were non-null, assigning to it will cause a leak
    if (NULL == uart_rx_chain)
    {
        uart_rx_chain = nsvc_pool_allocate(&nsvc_pcl_pool, true);

        // If pool was empty, throw away packet (turn on discarding).
        // Otherwise, clear discarding if it was set before
        discarding = (NULL == uart_rx_chain);
        if (!discarding)
        {
            // Manually initialize header
            header_ptr = NSVC_PCL_HEADER(uart_rx_chain);
            header_ptr->num_pcls = 1;
            header_ptr->offset = 0;
            header_ptr->total_used_length = 0;
            header_ptr->tail = uart_rx_chain;

            // Initialize seek struct
            // Since we're head pcl in chain, offset must
            //  skip past header.
            uart_rx_seeker.current_pcl = uart_rx_chain;
            uart_rx_seeker.offset_in_pcl = NSVC_PCL_OFFSET_PAST_HEADER(0);
        }
    }
    // Should only get here if no message block could be allocated
    // and packet was never sent. Resync.
    else
    {
        // Though we're at start of packet, discard it anyways
        // just to be safe
        discarding = true;

        (void)nufr_msg_send(discard_packet_fields,
                            (uint32_t)uart_rx_chain,
                            NUFR_TID_UART_TASK);

        // finally we got resynced
    }
}

//
// Call once at last rx of packet, after last 'uart_irq_data_rx()'
//
void uart_irq_packet_end(void)
{
    nsvc_pcl_header_t *header_ptr;

    // Everything in order?
    if (NULL != uart_rx_chain)
    {
        header_ptr = NSVC_PCL_HEADER(uart_rx_chain);
        // Since we cut corners on length calculation, must update length
        //  this final time
        header_ptr->total_used_length = uart_rx_packet_length;

        (void)nufr_msg_send(good_packet_fields,
                            (uint32_t)uart_rx_chain,
                            NUFR_TID_UART_TASK);
        // Reset
        uart_rx_chain = NULL;
        uart_rx_packet_length = 0;

        // 'uart_rx_seeker' gets reset next alloc
    }
    // Must resync
    else
    {
        discarding = true;
    }
}

//
// Call each time packet data is received. Allows for multiple bytes
// to be processed in one invocation.
//
// It's required that a single invocation won't have a 'uart_rx_packet_length'
// that needs to write to more than 2 particles.
//
void uart_irq_data_rx(uint8_t *data_ptr, unsigned data_length)
{
    unsigned           space_remaining_in_pcl;
    uint8_t           *pcl_data_ptr;
    nsvc_pcl_t        *new_pcl;
    nsvc_pcl_header_t *header_ptr;

    if ((NULL != uart_rx_chain) && !discarding)
    {
        space_remaining_in_pcl = NSVC_PCL_SIZE - uart_rx_seeker.offset_in_pcl;

        // Will all the data fit in the current pcl?
        if (data_length <= space_remaining_in_pcl)
        {
            pcl_data_ptr = NSVC_PCL_SEEK_DATA(uart_rx_seeker);

            // It might be more efficient to manually copy the
            // data over, rather than using rutils_memcpy().
            rutils_memcpy(pcl_data_ptr, data_ptr, data_length);

            uart_rx_seeker.offset_in_pcl += data_length;

            uart_rx_packet_length += data_length;
        }
        // No...
        else
        {
            // Any more space in current pcl?
            // Fill it.
            if (space_remaining_in_pcl > 0)
            {
                pcl_data_ptr = NSVC_PCL_SEEK_DATA(uart_rx_seeker);

                // If 'data_length' values are small, it might be
                //   more efficient to manually copy the data over,
                //   rather than using rutils_memcpy().
                rutils_memcpy(pcl_data_ptr, data_ptr, space_remaining_in_pcl);

                uart_rx_seeker.offset_in_pcl += space_remaining_in_pcl;
                data_length -= space_remaining_in_pcl;
                data_ptr += space_remaining_in_pcl;

                uart_rx_packet_length += data_length;
            }

            // Allocate a new pcl, manually append it to chain,
            // finish writing data
            new_pcl = nsvc_pool_allocate(&nsvc_pcl_pool, true);
            if (NULL != new_pcl)
            {
                uart_rx_seeker.current_pcl->flink = new_pcl;
                uart_rx_seeker.current_pcl = new_pcl;
                uart_rx_seeker.offset_in_pcl = 0;

                header_ptr = NSVC_PCL_HEADER(uart_rx_chain);
                header_ptr->num_pcls++;
                // this is only approximate kept up to date
                header_ptr->total_used_length = data_length;
                header_ptr->tail = new_pcl;

                // Sanity check, should always be true
                if (data_length < NSVC_PCL_SIZE)
                {
                    pcl_data_ptr = new_pcl->buffer;

                    // If 'data_length' values are small, it might be
                    //   more efficient to manually copy the data over,
                    //   rather than using rutils_memcpy().
                    rutils_memcpy(pcl_data_ptr, data_ptr, data_length);

                    uart_rx_seeker.offset_in_pcl = data_length;

                    uart_rx_packet_length += data_length;
                }
                else
                {
                    discarding = true;
                }
            }
        }
    }
    else
    {
        discarding = true;
    }
}