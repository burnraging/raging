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

//! @file     rnet-icmp.h
//! @authors  Bernie Woodland
//! @date     12May18
//!
//! @brief    ICMP Headers
//!
//! @details  RFC 792, 4443
//!

#ifndef RNET_ICMP_H
#define RNET_ICMP_H

#include "raging-global.h"
#include "rnet-compile-switches.h"
#include "rnet-buf.h"
#include "nsvc-api.h"

//!
//! @enum      rnet_icmp_type_t
//!
//! @brief     ICMPv4 'type' field
//!
typedef enum
{
    RNET_IT_ECHO_REPLY = 0,
    RNET_IT_DEST_UNREACHABLE = 1,
    RNET_IT_REDIRECT = 5,
    RNET_IT_ECHO_REQUEST = 8,
    RNET_IT_ROUTER_ADVERTISEMENT = 8,
    RNET_IT_ROUTER_SOLICITATION = 9,
    RNET_IT_TIME_EXCEEDED = 11,
    RNET_IT_TRACEROUTE = 30
} rnet_icmp_type_t;

//!
//! @enum      rnet_icmp_type_t
//!
//! @brief     ICMPv4 'code' field
//!
typedef enum
{
    RNET_IC_DEST_NET_UNREACHABLE = 0,
    RNET_IC_DEST_HOST_UNREACHABLE = 0,
} rnet_icmp_code_t;

//!
//! @enum      rnet_icmpv6_type_t
//!
//! @brief     ICMPv6 'type' field
//!
typedef enum
{
    RNET_ITV6_DEST_UNREACHABLE = 1,
    RNET_ITV6_PACKET_TOO_BIG = 2,
    RNET_ITV6_TIME_EXCEEDED = 3,
    RNET_ITV6_ROUTER_SOLICITATION = 133,
    RNET_ITV6_ROUTER_ADVERTISEMENT = 134,
    RNET_ITV6_NEIGHBOR_SOLICITATION = 135,
    RNET_ITV6_NEIGHBOR_ADVERTISEMENT = 136,
    RNET_ITV6_ECHO_REQUEST = 128,
    RNET_ITV6_ECHO_REPLY = 129,
} rnet_icmpv6_type_t;

//!
//! @enum      rnet_icmpv6_code_t
//!
//! @brief     ICMPv6 'code' field
//!
typedef enum
{
    RNET_IC_NO_ROUTE_TO_DESTINATION = 0,
} rnet_icmpv6_code_t;

//!
//! @struct      icmp_echo_request_header_t
//!
//! @brief     ICMP Echo request fields
//!
typedef struct
{
    uint16_t identifier;
    uint16_t sequence_number;
} icmp_echo_request_header_t;

// must be 4 bytes
typedef union
{
    icmp_echo_request_header_t echo_request;
} rnet_icmp_rest_of_header_t;

//!
//! @struct    rnet_icmp_header_t
//!
//! @brief     ICMPv4 header
//!
typedef struct
{
    rnet_icmp_type_t           type;
    uint8_t                    code;
    uint16_t                   checksum;
    rnet_icmp_rest_of_header_t rest_of_header;
} rnet_icmp_header_t;

//!
//! @struct    rnet_icmpv6_header_t
//!
//! @brief     ICMPv6 header
//!
typedef struct
{
    rnet_icmpv6_type_t type;
    uint8_t            code;
    uint16_t           checksum;
} rnet_icmpv6_header_t;

// Serialized header sizes
#define ICMP_HEADER_SIZE      8   // Must have 4 bytes for "rest of header"
#define ICMPV6_HEADER_SIZE    4

// APIs
RAGING_EXTERN_C_START
void rnet_msg_rx_buf_icmp(rnet_buf_t *buf);
void rnet_msg_rx_pcl_icmp(nsvc_pcl_t *head_pcl);
void rnet_msg_rx_buf_icmpv6(rnet_buf_t *buf);
void rnet_msg_rx_pcl_icmpv6(nsvc_pcl_t *head_pcl);
RAGING_EXTERN_C_END

#endif  // RNET_ICMP_H