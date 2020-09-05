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

//! @file     ssp-driver.c
//! @authors  Bernie Woodland
//! @date     18Nov18
//!
//! @brief    Simple Serial Protocol Driver
//!
//! @details     
//! @details   A Simple Serial Protocol frame consists of:
//! @details   -----------------------------------------------------
//! @details   |                                                   |
//! @details   |  MAGIC_NUMBERS  |  LENGTH   |  PAYLOAD  |  CRC16  |
//! @details   |                                                   |
//! @details   -----------------------------------------------------
//! @details     
//! @details   MAGIC_NUMBERS (2 bytes):   0x7E, 0xA5
//! @details   LENGTH (2 bytes): network byte order length.
//! @details                       Length is PAYLOAD + 2 ("2" for CRC16)
//! @details   CRC16 (2 bytes):
//! @details     
//! @details     
//! @details   An SSP Payload (ref. PAYLOAD above) consists of:
//! @details     
//! @details   -----------------------------------------------------
//! @details   |                                                   |
//! @details   |  Dest App  |  Src Circuit                         |
//! @details   |                                                   |
//! @details   -----------------------------------------------------
//! @details
//! @details   This is the driver for an SSP (Simple Serial Protocol)
//! @details   implementation. This is not a complete driver, as it
//! @details   doesn't include the IRQ handler code, since handlers
//! @details   vary from platform to platform. By not including the IRQ
//! @details   handler code, this driver works as-is with any number of
//! @details   platforms.
//! @details   
//! @details   At a high level, the driver works as follows:
//! @details   - On the rx path it takes a stream of bytes and
//! @details     assembles them into packets
//! @details   - On the tx path, a task queues up a packet into the
//! @details     tx queue. The caller then takes the packet list
//! @details     and breaks it into packet strings in, sizing the strings
//! @details     according to the wishes of the the tx caller.
//! @details   - The driver converts SSP frames into packets and
//! @details     packets into SSP frames. In other words, it manages
//! @details     preambles and CRCs.
//! @details   - Both the SSP driver and the app developer manage packets in
//! @details     an SSP-specific global packet buffer pool. Each packet buffer
//! @details     has a meta-data header which allows for the creation of a
//! @details     "window": the active part of a frame.
//! @details   - The SSP driver supports multiple channels (serial interfaces).
//! @details     The data for each channel is contained in the channel-specific
//! @details     descriptor.
//! @details   
//! @details   Driver used as follows:
//! @details    - Create an ssp-app.h file. Set parameters according
//! @details      to project needs.     
//! @details    - Call ssp_init() at startup:
//! @details       o Designate message parameters
//! @details    - Create your own Tx driver
//! @details       o Use either a task or an IRQ, depending on platform
//! @details    - Pass SSP packets around to app layer as needed
//! @details    - Use nsvc_pool_*() API calls using ssp_pool global
//! @details       o nsvc_pool_allocateW(&ssp_pool, ...) for app layer buffer tx
//! @details       o nsvc_pool_free(&ssp_pool, ....) for discarding rx packets
//! @details    - 
//! @details    - 
//!

#define SSP_GLOBAL_DEFS

#include "raging-global.h"
#include "raging-utils.h"
#include "raging-utils-mem.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nsvc-api.h"
#include "raging-contract.h"

#include "ssp-app.h"
#include "ssp-driver.h"

#if 0
//*********   Example ssp-app.h
//*********   These config values must be placed in per-project ssp-app.h file

// Num bytes consumed by 'Src Circuit' and 'Dest Circuit' fields
//    embedded in payload
#define SSP_APP_SPECIFIERS_SIZE    2

// Size of SSP packet, which is frame payload.
// This is max size which comes out of rx driver
// It includes SSP_APP_SPECIFIERS_SIZE/Src+Dest Circuit bytes
#define SSP_MAX_PAYLOAD_SIZE     100

// Number of interfaces running this protocol over
#define SSP_NUM_CHANNELS           2

// Number of packet buffers in global pool
#define SSP_POOL_SIZE             10

//*********   End Example ssp-app.h
#endif

// 'true' if allocation was successful
#define SUCCESS_ALLOC(rv)     ( (NUFR_SEMA_GET_OK_NO_BLOCK == (rv)) ||       \
                                (NUFR_SEMA_GET_OK_BLOCK == (rv)) )

//   Driver Meta-Data
ssp_desc_t    ssp_desc[SSP_NUM_CHANNELS];

//   SSP buffer pool
nsvc_pool_t   ssp_pool;
ssp_buf_t     ssp_buf[SSP_POOL_SIZE];



//!
//! @name      ssp_init
//!
//! @details   Call once at bootup after SL init.
//!
//! @param[in] 'rx_msg_fields'-- Specifies where ssp_rx_entry() sends rx packet.
//! @param[in] 'tx_msg_fields'-- Specifies where ssp_tx_optain_bytes()
//! @param[in]                   sends an already tx'ed packet for disposal.
//! @param[in]    'rx_msg_fields' and 'tx_msg_fields' are array parameter:
//! @param[in]            Caller will set 'rx_msg_fields[SSP_NUM_CHANNELS]'
//! @param[in]            Caller will set 'tx_msg_fields[SSP_NUM_CHANNELS]'
//!
//! @details     Example for init parms:
//! @details       nsvc_msg_fields_unary_t rx_fields[SSP_NUM_CHANNELS];
//! @details       nsvc_msg_fields_unary_t tx_fields[SSP_NUM_CHANNELS];
//! @details     
//! @details       rx_fields[0].prefix = MY_PREFIX;
//! @details       rx_fields[0].id = MY_RX_ID;
//! @details       rx_fields[0].priority = NUFR_MSG_PRI_MID;
//! @details       rx_fields[0].sending_task = NUFR_TID_null;
//! @details       rx_fields[0].destination_task = NUFR_TID_MY_TASK;
//! @details       rx_fields[0].optional_parameter = 0;      // not used
//! @details     
//! @details       rx_fields[1].prefix = MY_PREFIX;
//! @details       rx_fields[1].id = MY_RX_ID;
//! @details       rx_fields[1].priority = NUFR_MSG_PRI_MID;
//! @details       rx_fields[1].sending_task = NUFR_TID_null;
//! @details       rx_fields[1].destination_task = NUFR_TID_MY_TASK;
//! @details       rx_fields[1].optional_parameter = 0;      // not used
//! @details     
//! @details       tx_fields[0].prefix = MY_PREFIX;
//! @details       tx_fields[0].id = MY_TX_ID;
//! @details       tx_fields[0].priority = NUFR_MSG_PRI_MID;
//! @details       tx_fields[0].sending_task = NUFR_TID_null;
//! @details       tx_fields[0].destination_task = NUFR_TID_MY_TASK;  // not used
//! @details       tx_fields[0].optional_parameter = 0;      // not used
//! @details     
//! @details       tx_fields[1].prefix = MY_PREFIX;
//! @details       tx_fields[1].id = MY_TX_ID;
//! @details       tx_fields[1].priority = NUFR_MSG_PRI_MID;
//! @details       tx_fields[1].sending_task = NUFR_TID_null;
//! @details       tx_fields[1].destination_task = NUFR_TID_MY_TASK;   // not used
//! @details       tx_fields[1].optional_parameter = 0;      // not used
//! @details       
//! @details       ssp_init(rx_msg_fields, tx_msg_fields, rx_tx_dest_tasks);
//! @details   
//!
void ssp_init(const nsvc_msg_fields_unary_t *rx_msg_fields,
              const nsvc_msg_fields_unary_t *tx_msg_fields)
{
    unsigned             i;

    rutils_memset(ssp_desc, 0, sizeof(ssp_desc));
    rutils_memset(&ssp_pool, 0, sizeof(ssp_desc));
    rutils_memset(&ssp_buf, 0, sizeof(ssp_buf));

    for (i = 0; i < SSP_NUM_CHANNELS; i++)
    {
        ssp_desc[i].rx_msg_fields =
                             nsvc_msg_struct_to_fields(&rx_msg_fields[i]);
        ssp_desc[i].tx_msg_fields =
                             nsvc_msg_struct_to_fields(&tx_msg_fields[i]);
        ssp_desc[i].dest_task = rx_msg_fields[i].destination_task;
        ssp_desc[i].channel_number = (uint8_t)i;
        ssp_desc[i].rx_mode = SSP_SYNC_MAGIC1;
    }

    // Initialize SSP pool
    ssp_pool.pool_size = SSP_POOL_SIZE;
    ssp_pool.base_ptr = ssp_buf;
    ssp_pool.element_size = sizeof(ssp_buf_t);
    ssp_pool.element_index_size = (unsigned)(
                 (uint8_t *)&ssp_buf[1] - (uint8_t *)&ssp_buf[0]
                                            );
    ssp_pool.flink_offset = 0;

    nsvc_pool_init(&ssp_pool);
}

//!
//! @name      ssp_get_descriptor
//!
//! @brief     Get descriptor for this channel number
//!
//! @details   Use case may not present itself.
//! @details   IRQ handlers unwind this fcn. to save CPU time
//!
//! @param[in] 'channel_number'--
//!
//! @return    pointer to descriptor
//!
ssp_desc_t *ssp_get_descriptor(unsigned channel_number)
{
    if (channel_number < SSP_NUM_CHANNELS)
    {
        return &ssp_desc[channel_number];
    }

    APP_ENSURE(false);

    // Error case, should never get here
    return &ssp_desc[0];
}

//!
//! @name      ssp_allocate_buffer_from_taskW
//!
//! @brief     Allocate buffer from SSP-dedicated pool
//!
//! @details   Task will wait indefinitely until a buffer
//! @details   becomes available, therefore this cannot
//! @details   be called from IRQ or BG Task.
//!
//! @return    buffer
//!
ssp_buf_t *ssp_allocate_buffer_from_taskW(unsigned channel_number)
{
    ssp_buf_t *buf;
    nufr_sema_get_rtn_t alloc_rv;


    // 'void **' cast to supress compiler warning (hate doing it!)
    alloc_rv = nsvc_pool_allocateW(&ssp_pool, (void **)&buf);
    if (SUCCESS_ALLOC(alloc_rv))
    {
        // Must init meta data
        buf->header.channel_number = channel_number;
          // Reserve enough space to prepend preamble when packet
          // gets tx'ed.
        buf->header.offset = SSP_PREAMBLE_SIZE;
        buf->header.length = 0;
    }

    // Error case, should never get here
    return buf;
}

//!
//! @name      ssp_free_buffer_from_task
//!
//! @brief     Free buffer to SSP-dedicated pool
//!
//! @details   Cannot be called from IRQ or BG Task
//!
//! @param[in] 'buffer'--
//!
void ssp_free_buffer_from_task(ssp_buf_t *buffer)
{
    nsvc_pool_free(&ssp_pool, buffer);
}

//!
//! @name      ssp_rx_entry
//!
//! @brief     SSP Rx IRQ handler, driver entry point
//!
//! @details   Serial IRQ handler will call this function 1 byte
//! @details   at a time. Driver here does the following
//! @details     1. Synchronizes on the start of a valid frame
//! @details     2. Gathers data into an SSP buffer
//! @details     3. Does CRC check on frame before committing buffer
//! @details     4. Forwards assembled packet to a task for consumption
//!
//! @param[in] 'desc'--
//! @param[in] 'this_char'--
//!
void ssp_rx_entry(ssp_desc_t *desc, uint8_t this_char)
{
    uint16_t             x;
    uint16_t             running_crc;
    ssp_buf_t           *rx_buffer;
//    uint8_t             *ptr;
    nufr_msg_send_rtn_t  send_rv;

    // What synchronization state are we in?
    switch (desc->rx_mode)
    {
    // Frame synchronized, and we're gathering payload bytes
    case SSP_SYNC_DATA:

        // Put character into packet buffer
        *(desc->rx_ptr_current++) = this_char;

        running_crc = desc->rx_running_crc;

        // Do CRC inline here
        // Equivalent to rutils_crc16_add_string() unwound
        x = (running_crc ^ this_char) & BIT_MASK8;
        x = (x ^ (x << 4)) & BIT_MASK8;
        x = (x << 8) ^ (x << 3) ^ (x >> 4);
        running_crc = (running_crc >> BITS_PER_WORD8) ^ x;

        desc->rx_frame_length_current++;

        // Reached end of frame?
        if (desc->rx_frame_length_field == desc->rx_frame_length_current)
        {
            if (RUTILS_CRC16_GOOD == running_crc)
            {
                rx_buffer = desc->rx_buffer;
                APP_ENSURE_IL(NULL != rx_buffer);

                // Write buffer fields.
                // Omit CRC from length, even though it's in buffer
                rx_buffer->header.length =
                           desc->rx_frame_length_current - RUTILS_CRC16_SIZE;

                send_rv = nufr_msg_send(desc->rx_msg_fields,
                                        (uint32_t)rx_buffer,
                                        desc->dest_task);
                if (NUFR_MSG_SEND_ERROR != send_rv)
                {
                    // We're no longer holding the pool buffer, so 
                    // notify self: next call we'll allocate another.
                    desc->rx_buffer = NULL;

                    desc->rx_frame_count++;
                }
            }

            // Regardless, configure to resync on next frame's magic number
            desc->rx_mode = SSP_SYNC_MAGIC1;
        }
        // We need more bytes to finish frame
        else
        {
            // Carry over CRC calculation for next string of the payload
            desc->rx_running_crc = running_crc;
        }

        break;

    // Synchronizing: scanning for magic number #1
    case SSP_SYNC_MAGIC1:

        if (SSP_MAGIC_NUMBER1 == this_char)
        {
            desc->rx_mode = SSP_SYNC_MAGIC2;
        }

        break;

    // Synchronizing: scanning for magic number #2
    case SSP_SYNC_MAGIC2:

        if (SSP_MAGIC_NUMBER2 == this_char)
        {
            desc->rx_mode = SSP_SYNC_LENGTH_HI;
        }
        else
        {
            desc->rx_mode = SSP_SYNC_MAGIC1;
        }

        break;

    // Synchronizing: this byte is frame length MSByte
    case SSP_SYNC_LENGTH_HI:

        desc->rx_frame_length_field = (unsigned)this_char << BITS_PER_WORD8;
        desc->rx_mode = SSP_SYNC_LENGTH_LO;

        break;

    // Synchronizing: this byte is frame length LSByte
    case SSP_SYNC_LENGTH_LO:

        desc->rx_frame_length_field |= this_char;

        // Sanity check length
        if ((desc->rx_frame_length_field <= SSP_MAX_FRAME_SIZE) &&
            (desc->rx_frame_length_field >= RUTILS_CRC16_SIZE))
        {
            // Frame sync successful. Set up for new frame now

            desc->rx_mode = SSP_SYNC_DATA;
            desc->rx_frame_length_current = 0;

            // Make sure a buffer isn't already being held already
            if (NULL == desc->rx_buffer)
            {
                rx_buffer = nsvc_pool_allocate(&ssp_pool, true);
                desc->rx_buffer = rx_buffer;

                // Pool empty? Drop this packet
                if (NULL == desc->rx_buffer)
                {
                    desc->rx_mode = SSP_SYNC_MAGIC1;
                }
                else
                {
                    rx_buffer->header.channel_number = desc->channel_number;
                }
            }
            // Else, we already have a packet buffer from before. Reuse it.

            // Are we holding a buffer?
            rx_buffer = desc->rx_buffer;
            if (NULL != rx_buffer)
            {
                // NOT NECESSARY, COMMENTED OUT TO SAVE CPU CYCLES:
                //
                //// Recreate preamble at beginning of buffer
                //ptr = SSP_PAYLOAD_PTR(rx_buffer);
                //*ptr++ = SSP_MAGIC_NUMBER1;
                //*ptr++ = SSP_MAGIC_NUMBER2;
                //// Save CPU cycles and unwind rutils_word16_to_stream() here
                //*ptr++ = (uint8_t)
                //         (desc->rx_frame_length_field >> BITS_PER_WORD8);
                //*ptr++ = (uint8_t)(desc->rx_frame_length_field & BIT_MASK8);

                // NOTE: nsvc_pool_allocate() won't clear these fields,
                //       must manually assign them
                rx_buffer->header.offset = SSP_PREAMBLE_SIZE;
                rx_buffer->header.length = 0;
                desc->rx_ptr_current = SSP_PAYLOAD_PTR(rx_buffer);
            }

            desc->rx_running_crc = 0xFFFF;   // rutils_crc16_start() unwound
            desc->rx_sync_count++;
        }
        else
        {
            // Failed length sanity check: resync on next packet
            desc->rx_mode = SSP_SYNC_MAGIC1;
        }

        break;

    default:
        // won't get here
        break;
    }
}

//!
//! @name      ssp_packet_to_frame
//!
//! @brief     Convert packet to SSP frame
//!
//! @details   Calculate CRC and append it to packet, at end of used payload
//! @details   Prepend preamble
//! @details   Calculates from '->header.offset' to '->header.length'
//! @details   Adjust length for CRC bytes added
//! @details   NOTE: caller must have window sized to accomodate these bytes
//!
//! @param[in] 'buffer'--
//!
bool ssp_packet_to_frame(ssp_buf_t  *buffer)
{
    unsigned             offset;
    unsigned             length;
    uint16_t             calculated_crc;
    uint8_t             *ptr;

    APP_REQUIRE_API(NULL != buffer);

    offset = buffer->header.offset;
    length = buffer->header.length;

    // Does buffer have enough bytes for Preamble and CRC?
    if (offset + length > SSP_MAX_FRAME_SIZE - RUTILS_CRC16_SIZE)
    {
        APP_REQUIRE_API(false);
        return false;
    }
    else if (offset < SSP_PREAMBLE_SIZE)
    {
        APP_REQUIRE_API(false);
        return false;
    }

    calculated_crc = rutils_crc16_buffer(SSP_PAYLOAD_PTR(buffer),
                                         buffer->header.length);
    // Must EOR with 0xFFFF!
    calculated_crc = 0xFFFF ^ calculated_crc;

    // Point to next byte after end of used payload
    ptr = SSP_FREE_PAYLOAD_PTR(buffer);

    // Write 2-byte CRC there. Must be little-endian.
    rutils_word16_to_stream_little_endian(ptr, calculated_crc);
    length += RUTILS_CRC16_SIZE;

    // Write preamble
    ptr = SSP_PAYLOAD_PTR(buffer) - SSP_PREAMBLE_SIZE;
    *ptr++ = SSP_MAGIC_NUMBER1;
    *ptr++ = SSP_MAGIC_NUMBER2;
    rutils_word16_to_stream(ptr, length);
    offset -= SSP_PREAMBLE_SIZE;
    buffer->header.offset = offset;
    length += SSP_PREAMBLE_SIZE;
    buffer->header.length = length;

    return true;
}

//!
//! @name      ssp_tx_queue_packet
//!
//! @brief     Add packet to transmit queue
//!
//! @details   Cannot be called from IRQ handler
//!
//! @param[in] 'tx_buffer'--
//!
void ssp_tx_queue_packet(ssp_buf_t  *tx_buffer)
{
    nufr_sr_reg_t        saved_psr;
    ssp_desc_t          *desc;

    APP_REQUIRE_API(NULL != tx_buffer);
    APP_REQUIRE_API(NULL == tx_buffer->flink);
    APP_REQUIRE_API(tx_buffer->header.channel_number < SSP_NUM_CHANNELS);

    // Add Preamble and CRC
    if (!ssp_packet_to_frame(tx_buffer))
    {
        ssp_free_buffer_from_task(tx_buffer);
        return;
    }

    // Look up descriptor for this channel
    desc = &ssp_desc[tx_buffer->header.channel_number];

    saved_psr = NUFR_LOCK_INTERRUPTS();

    if (NULL == desc->tx_head)
    {
        desc->tx_head = tx_buffer;
        desc->tx_tail = tx_buffer;
    }
    else
    {
        desc->tx_tail->flink = tx_buffer;
        desc->tx_tail = tx_buffer;
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);
}

//!
//! @name      ssp_tx_halt_and_purge
//!
//! @brief     Kill all Tx
//!
//! @details   
//!
//! @param[in] 'desc'-- channel to purge
//!
void ssp_tx_halt_and_purge(ssp_desc_t  *desc)
{
    nufr_sr_reg_t        saved_psr;
    ssp_buf_t           *head_ptr;
    ssp_buf_t           *this_buf_ptr;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    head_ptr = desc->tx_head;
    desc->tx_head = NULL;
    desc->tx_tail = NULL;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    // Free all packet buffers awaiting tx
    this_buf_ptr = head_ptr;
    while (NULL != this_buf_ptr)
    {
        nsvc_pool_free(&ssp_pool, this_buf_ptr);

        this_buf_ptr = this_buf_ptr->flink;
    }
}

//!
//! @name      ssp_tx_obtain_next_bytes
//!
//! @brief     Take next 1 to N bytes from tx queue for transmit
//!
//! @details   Can/will likely be called from IRQ.
//!
//! @param[in] 'channel_number'--
//! @param[out] 'tx_holder_ptr'-- buffer to put bytes to be transmitted
//! @param[in] 'max_length'-- max length of 'tx_holder' and/or max
//! @param[in]                 bytes to load into 'tx_holder'
//! @param[out] 'length_ptr'-- variable to write bytes placed in 'tx_holder'
//!
void ssp_tx_obtain_next_bytes(unsigned  channel_number,
                              uint8_t  *tx_holder_ptr,
                              unsigned  max_length,
                              unsigned *length_ptr)
{
    nufr_sr_reg_t        saved_psr;
    ssp_desc_t          *desc;
    ssp_buf_t           *this_buffer;
    ssp_buf_t           *free_buffer;
    uint8_t             *ptr;
    unsigned             this_copy_length;
    bool                 another_packet_to_tx;
    nufr_msg_send_rtn_t  send_rv;

    // Look up descriptor for this channel
    desc = &ssp_desc[channel_number];

    *length_ptr = 0;

    this_buffer = desc->tx_head;

    // Walk all tx buffers, if necessary
    while ((NULL != this_buffer) && (max_length > 0))
    {
        // Only copy up to the end of this buffer or to
        // caller's max limit, whichever is less
        this_copy_length = max_length;

        if (this_copy_length > this_buffer->header.length)
        {
            this_copy_length = this_buffer->header.length;
        }

        // Take from beginning of payload, as specified by
        // header.offset
        ptr = SSP_PAYLOAD_PTR(this_buffer);

        rutils_memcpy(tx_holder_ptr, ptr, this_copy_length);

        tx_holder_ptr += this_copy_length;
        max_length -= this_copy_length;
        this_buffer->header.offset += this_copy_length;
        this_buffer->header.length -= this_copy_length;
        *length_ptr += this_copy_length;

        // Did we get all the bytes from this buffer?
        // Flush it and get next buffer on queue
        if (0 == this_buffer->header.length)
        {
            free_buffer = this_buffer;

            desc->tx_count++;

            saved_psr = NUFR_LOCK_INTERRUPTS();

            // Is this last packet queued for tx?
            another_packet_to_tx = this_buffer != desc->tx_tail;
            if (!another_packet_to_tx)
            {
                desc->tx_head = NULL;
                desc->tx_tail = NULL;
                this_buffer = NULL;
            }
            // Else, another packet to tx...
            else
            {
                this_buffer = this_buffer->flink;
                desc->tx_head = this_buffer;

                APP_ENSURE(NULL != this_buffer);
            }

            NUFR_UNLOCK_INTERRUPTS(saved_psr);

            free_buffer->flink = NULL;

            send_rv = nufr_msg_send(desc->tx_msg_fields,
                                    (uint32_t)free_buffer,
                                    desc->dest_task);
            APP_ENSURE(NUFR_MSG_SEND_ERROR != send_rv);
            UNUSED_BY_ASSERT(send_rv);

            // Msg recipient must do this:
            //   this_buffer = (ssp_buf_t *)msg->parameter;
            //   nsvc_pool_free(&ssp_pool, this_buffer);
        }
    }
}