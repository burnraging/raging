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

//! @file     rnet-ip.h
//! @authors  Bernie Woodland
//! @date     10Mar18
//!
//! @brief    IPv4+IPv6 Headers
//!

#ifndef RNET_IP_H
#define RNET_IP_H

#include "raging-global.h"
#include "rnet-compile-switches.h"
#include "rnet-buf.h"
#include "nsvc-api.h"
#include "rnet-ip-base-defs.h"

// Header in serialized form
#define IPV4_HEADER_SIZE          20
#define IPV6_HEADER_SIZE          40

// Byte offsets in serialized header
#define IPV4_SRC_ADDR_OFFSET      12
#define IPV6_SRC_ADDR_OFFSET       8

// Headers are unserialized form

typedef struct
{
    uint8_t            dscp;
    uint16_t           total_length;     // includes IPv4 header
    uint8_t            ttl;
    rnet_ip_protocol_t ip_protocol;
    uint16_t           header_checksum;
    uint8_t            src_addr[IPV4_ADDR_SIZE];
    uint8_t            dest_addr[IPV4_ADDR_SIZE];
} rnet_ipv4_header_t;

typedef struct
{
    uint8_t             traffic_class;
    uint16_t            payload_length;   // does NOT include IPv6 header
    rnet_ip_protocol_t  ip_protocol;
    uint8_t             hop_limit;
    uint8_t             src_addr[IPV6_ADDR_SIZE];
    uint8_t             dest_addr[IPV6_ADDR_SIZE];
} rnet_ipv6_header_t;


//  APIs
RAGING_EXTERN_C_START
//void rnet_msg_rx_buf_ipv4(rnet_buf_t *buf);
void rnet_msg_rx_buf_ipv4(rnet_buf_t *buf);
void rnet_msg_rx_pcl_ipv4(nsvc_pcl_t *head_pcl);
void rnet_msg_rx_buf_ipv6(rnet_buf_t *buf);
void rnet_msg_rx_pcl_ipv6(nsvc_pcl_t *head_pcl);
void rnet_msg_tx_buf_ipv4(rnet_buf_t *buf);
void rnet_msg_tx_pcl_ipv4(nsvc_pcl_t *head_pcl);
void rnet_msg_tx_buf_ipv6(rnet_buf_t *buf);
void rnet_msg_tx_pcl_ipv6(nsvc_pcl_t *head_pcl);
bool rnet_ip_is_valid_protocol(rnet_ip_protocol_t protocol);
rnet_ip_protocol_t rnet_ip_ph_to_ip_protocol(rnet_ph_t ph);
rnet_ph_t rnet_ip_ip_protocol_to_ph(rnet_ip_protocol_t protocol);
unsigned rnet_ip_l4_checksum_offset(rnet_ph_t ph);
rnet_ph_t rnet_ip_l4_ph_to_ip_protocol(rnet_ip_protocol_t protocol);
bool rnet_ip_is_ipv6_traffic_type(rnet_ip_traffic_t traffic_type);
void rnet_ipv4_serialize_header(uint8_t            *buffer,
                                rnet_ipv4_header_t *header,
                                bool                include_checksum);
void rnet_ipv6_serialize_header(uint8_t            *buffer,
                                rnet_ipv6_header_t *header);
bool rnet_ipv4_deserialize_header(rnet_ipv4_header_t *header,
                                  uint8_t            *buffer);
bool rnet_ipv6_deserialize_header(rnet_ipv6_header_t *header,
                                  uint8_t            *buffer);
uint16_t rnet_ipv4_checksum(uint8_t *header_start_ptr);
uint16_t rnet_ipv4_pseudo_header_struct_checksum(rnet_ipv4_header_t *header);
uint16_t rnet_ipv6_pseudo_header_struct_checksum(rnet_ipv6_header_t *header);
uint16_t rnet_ip_running_checksum(uint16_t  running_sum,
                                  uint8_t  *stream,
                                  unsigned  length);
uint16_t rnet_ip_finalize_checksum(uint16_t running_sum);
uint16_t rnet_ip_pcl_add_data_to_checksum(uint16_t    running_sum,
                                          nsvc_pcl_t *head_pcl,
                                          uint8_t    *start_ptr,
                                          unsigned    data_length);
RAGING_EXTERN_C_END

#endif  // RNET_IP_H