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

//! @file     ssp-driver.h
//! @authors  Bernie Woodland
//! @date     24Nov18
//!
//! @brief    Simple Serial Protocol Driver
//!

#ifndef SSP_DRIVER_H_
#define SSP_DRIVER_H_

#include "raging-global.h"
#include "raging-utils-crc.h"
#include "nufr-platform-app.h"
#include "nsvc-api.h"

#include "ssp-app.h"

#define SSP_MAGIC_NUMBER1        0x7E
#define SSP_MAGIC_NUMBER2        0xA5
#define SSP_MAGIC_NUMBER_SIZE       2
#define SSP_PREAMBLE_SIZE        (SSP_MAGIC_NUMBER_SIZE + BYTES_PER_WORD16)

// Max length of an SSP frame, as seen by Rx IRQ handler
#define SSP_MAX_FRAME_SIZE       (SSP_PREAMBLE_SIZE + SSP_MAX_PAYLOAD_SIZE + RUTILS_CRC16_SIZE)

//!
//! @struct    ssp_buf_header_t
//!
//! @brief     Meta data for each SSP packet
//!
//! @details   'offset'+'length' is "window" which helps in scanning packet
//!
typedef struct
{
    uint8_t              channel_number;
    uint16_t             offset;
    uint16_t             length;
} ssp_buf_header_t;

//!
//! @struct    ssp_buf_t
//!
//! @brief     An SSP buffer pool object
//!
typedef struct ssp_buf_t_
{
    struct ssp_buf_t_   *flink;
    ssp_buf_header_t     header;
    uint8_t              buf[SSP_MAX_FRAME_SIZE];
} ssp_buf_t;     

// Macro to find start of payload in SSP packet buffer
#define SSP_PAYLOAD_PTR(ssp_buffer)  ( &(ssp_buffer)->buf[(ssp_buffer)->header.offset] )

// Macro to find next free byte in SSP packet buffer
#define SSP_FREE_PAYLOAD_PTR(ssp_buffer)  ( &(ssp_buffer)->buf[(ssp_buffer)->header.offset + (ssp_buffer)->header.length] )

//!
//! @enum      ssp_rx_mode_t
//!
//! @brief     State machine for syncing on packets by rx handler
//!
typedef enum
{
    SSP_SYNC_DATA,        // scanning frame payload
    SSP_SYNC_MAGIC1,      // searching for magic #1 value
    SSP_SYNC_MAGIC2,      // searching for magic #2 value
    SSP_SYNC_LENGTH_HI,   // scanning MSByte of frame length field
    SSP_SYNC_LENGTH_LO    // scanning LSByte of frame length field
} ssp_rx_mode_t;

//!
//! @enum      ssp_desc_t
//!
//! @brief     Per-channel driver data
//!
//! @details   'rx_buffer'-- SSP buffer pool item held by Rx driver
//! @details   
//! @details   'rx_ptr_current'-- Pointer into 'rx_buffer', where rx bytes
//! @details       are being queued.
//! @details   
//! @details   'msg_fields'-- nufr_msg_t->msg_fields value to be used when
//! @details       rx interrupt handler sends message
//! @details   
//! @details   'frame_length_field'-- Length value as received in frame
//! @details   
//! @details   'frame_length_current'-- Current count of payload as bytes
//! @details       are received.
//! @details   
//! @details   'mode'-- Rx sync mode
//! @details       
//! @details   'dest_task'-- Task where rx driver will send message
//! @details       
//! @details   'running_crc'-- Cummulative CRC
//! @details       
//!
typedef struct
{
    ssp_buf_t     *rx_buffer;
    ssp_buf_t     *tx_head;
    ssp_buf_t     *tx_tail;
    uint8_t       *rx_ptr_current;
    uint32_t       rx_msg_fields;
    uint32_t       tx_msg_fields;
    unsigned       rx_frame_length_field;
    unsigned       rx_frame_length_current;
    uint16_t       rx_running_crc;
    ssp_rx_mode_t  rx_mode;
    uint8_t        channel_number;
    nufr_tid_t     dest_task;
    nufr_tid_t     tx_dest_task;

    // counters
    uint16_t       rx_sync_count;
    uint16_t       rx_frame_count;
    uint16_t       tx_count;
} ssp_desc_t;

#ifndef SSP_GLOBAL_DEFS
    // Prefer ssp_allocate_buffer_from_task() and ssp_free_buffer_from_task()
    // to using 'ssp_pool'
    extern nsvc_pool_t   ssp_pool;
    // Prefer ssp_get_descriptor() to using 'ssp_desc'
    extern ssp_desc_t    ssp_desc[SSP_NUM_CHANNELS];
#endif

// APIs
RAGING_EXTERN_C_START

void ssp_init(const nsvc_msg_fields_unary_t *rx_msg_fields,
              const nsvc_msg_fields_unary_t *tx_msg_fields);
ssp_desc_t *ssp_get_descriptor(unsigned channel_number);
ssp_buf_t *ssp_allocate_buffer_from_taskW(unsigned channel_number);
void ssp_free_buffer_from_task(ssp_buf_t *buffer);
void ssp_rx_entry(ssp_desc_t *desc, uint8_t this_char);
bool ssp_packet_to_frame(ssp_buf_t  *buffer);
void ssp_tx_queue_packet(ssp_buf_t  *tx_buffer);
void ssp_tx_obtain_next_bytes(unsigned  channel_number,
                              uint8_t  *tx_holder,
                              unsigned  max_length,
                              unsigned *length_ptr);

RAGING_EXTERN_C_END

#endif  //SSP_DRIVER_H_