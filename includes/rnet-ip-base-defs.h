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

//! @file     rnet-ip-base-defs.h
//! @authors  Bernie Woodland
//! @date     31Dec18
//!
//! @brief    IPv4+IPv6 Fundamental definitions
//!

#ifndef RNET_IP_BASE_DEFS_H_
#define RNET_IP_BASE_DEFS_H_

#include "raging-global.h"

#define IPV4_ADDR_SIZE             4
#define IPV6_ADDR_SIZE            16

// Max IP address ascii text length (not inc. \0)
#define IPV4_ADDR_ASCII_SIZE      15
#define IPV6_ADDR_ASCII_SIZE      39

typedef enum
{
    RNET_TR_IPV4_UNICAST,
    RNET_TR_IPV6_GLOBAL,       //unicast by implication
    RNET_TR_IPV6_LINK_LOCAL,
} rnet_ip_traffic_t;

typedef enum
{
    RNET_IP_PROTOCOL_ICMP = 1,
    RNET_IP_PROTOCOL_TCP = 6,
    RNET_IP_PROTOCOL_UDP = 17,
    RNET_IP_PROTOCOL_ICMPv6 = 58
} rnet_ip_protocol_t;

typedef union
{
    uint8_t    ipv6_addr[IPV6_ADDR_SIZE];
    uint8_t    ipv4_addr[IPV4_ADDR_SIZE];
} rnet_ip_addr_union_t;

#endif  // RNET_IP_BASE_DEFS_H_