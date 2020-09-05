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

//! @file     rnet-udp.h
//! @authors  Bernie Woodland
//! @date     25Mar18
//!
//! @brief    UDP headers
//!

#ifndef RNET_UDP_H
#define RNET_UDP_H

#include "raging-global.h"
#include "rnet-compile-switches.h"
#include "rnet-buf.h"
#include "nsvc-api.h"

//!
//! @struct    rnet_udp_header_t
//!
//! @brief     Summarized struct of a UDP header
//!
typedef struct
{
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;
} rnet_udp_header_t;

#define UDP_HEADER_SIZE    8

// APIs
RAGING_EXTERN_C_START
void rnet_msg_rx_buf_udp(rnet_buf_t *buf);
void rnet_msg_rx_pcl_udp(nsvc_pcl_t *head_pcl);
void rnet_msg_tx_buf_udp(rnet_buf_t *buf);
void rnet_msg_tx_pcl_udp(nsvc_pcl_t *head_pcl);
RAGING_EXTERN_C_END

#endif  // RNET_UDP_H