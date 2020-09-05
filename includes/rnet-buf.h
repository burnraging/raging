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

//! @file     rnet-buf.h
//! @authors  Bernie Woodland
//! @date     12May18
//!
//! @brief    RNET buffer type
//!

#ifndef  RNET_BUF_H_
#define  RNET_BUF_H_

#include "raging-global.h"
#include "rnet-compile-switches.h"
#include "rnet-app.h"

//!
//! @enum      rnet_ph_t
//!
//! @brief     protocol header type
//!
typedef enum
{
    RNET_PH_null = 0,
    RNET_PH_AHDLC,
    RNET_PH_PPP,
    RNET_PH_LCP,
    RNET_PH_IPCP,
    RNET_PH_IPV6CP,
    RNET_PH_IPV4,
    RNET_PH_IPV6,
    RNET_PH_UDP,
    RNET_PH_TCP,
    RNET_PH_ICMP,
    RNET_PH_ICMPv6
} rnet_ph_t;

//!
//! brief      Buffer discard reason codes
//!

//! @name      RNET_BUF_CODE_INTFC_NOT_CONFIGURED
//! @brief     rnet-app interface misconfigured
#define RNET_BUF_CODE_INTFC_NOT_CONFIGURED            1
//! @name      RNET_BUF_CODE_MTU_EXCEEDED
//! @brief     Attempt to build packet exceeding MTU
#define RNET_BUF_CODE_MTU_EXCEEDED                    2
//! @name      RNET_BUF_CODE_METADATA_CORRUPTED
//! @brief     buf/pcl header data corrupted
#define RNET_BUF_CODE_METADATA_CORRUPTED              3
//! @name      RNET_BUF_CODE_UNDERRUN
//! @brief     buf/pcl header offset too small
#define RNET_BUF_CODE_UNDERRUN                        4
//! @name      RNET_BUF_CODE_NO_MORE_PCLS
//! @brief     Pcl pool depletion while enlarging chain
#define RNET_BUF_CODE_NO_MORE_PCLS                    5
//! @name      RNET_BUF_CODE_PCL_OP_FAILED
//! @brief     nsvc pcl API call failed
#define RNET_BUF_CODE_PCL_OP_FAILED                   6
//! @name      RNET_BUF_CODE_AHDLC_RX_CC
//! @brief     AHDLC error while stripping cntrl chars
#define RNET_BUF_CODE_AHDLC_RX_CC                     7
//! @name      RNET_BUF_CODE_AHDLC_RX_BAD_CRC
//! @brief     AHDLC checksum error
#define RNET_BUF_CODE_AHDLC_RX_BAD_CRC                8
//! @name      RNET_BUF_CODE_AHDLC_TX_CC
//! @brief     AHDLC error while encoding cntrl chars
#define RNET_BUF_CODE_AHDLC_TX_CC                     9
//! @name      RNET_BUF_CODE_PPP_HEADER_CORRUPTED
//! @brief     PPP Packet malformed
#define RNET_BUF_CODE_PPP_HEADER_CORRUPTED           10
//! @name      RENT_BUF_CODE_PPP_IP_PROTOCOL_UNSUPPORTED
//! @brief     Trying to use IPv4/IPv6 and it's not supported.
#define RENT_BUF_CODE_PPP_IP_PROTOCOL_UNSUPPORTED    11
//! @name      RENT_BUF_CODE_PPP_OTHER_PROTOCOL_UNSUPPORTED
//! @brief     Some non-IP protocol not suppported by RNET PPP
#define RENT_BUF_CODE_PPP_OTHER_PROTOCOL_UNSUPPORTED 12
//! @name      RENT_BUF_CODE_PPP_XCP_CODE_UNSUPPORTED
//! @brief     An LCP/IPCP/IPV6CVP code not supported
#define RENT_BUF_CODE_PPP_XCP_CODE_UNSUPPORTED       13
//! @name      RENT_BUF_CODE_PPP_XCP_PARSE_ERROR
//! @brief     An LCP/IPCP/IPV6CVP parse error
#define RENT_BUF_CODE_PPP_XCP_PARSE_ERROR            14
//! @name      RNET_BUF_CODE_IP_PACKET_TOO_SMALL
//! @brief     Obvious IP packet undersized
#define RNET_BUF_CODE_IP_PACKET_TOO_SMALL            15
//! @name      RNET_BUF_CODE_IP_PACKET_HEADER_CORRUPTED
//! @brief     IP packet header corrupted
#define RNET_BUF_CODE_IP_PACKET_HEADER_CORRUPTED     16
//! @name      RNET_BUF_CODE_IP_INTFC_NOT_FOUND
//! @brief     Buf/pcl header intfc value doesn't match configured intfcs
#define RNET_BUF_CODE_IP_INTFC_NOT_FOUND             17
//! @name      RNET_BUF_CODE_IP_SUBI_NOT_FOUND
//! @brief     Buf/pcl header subi value doesn't match configured subis
#define RNET_BUF_CODE_IP_SUBI_NOT_FOUND              18
//! @name      RNET_BUF_CODE_IP_CIRCUIT_NOT_FOUND
//! @brief     Buf/pcl header circuit value doesn't match configured circuits
#define RNET_BUF_CODE_IP_CIRCUIT_NOT_FOUND           19
//! @name      RNET_BUF_CODE_IP_RX_BAD_CRC
//! @brief     L4 checksum failure
#define RNET_BUF_CODE_IP_RX_BAD_CRC                  20
//! @name      RNET_BUF_CODE_IP_UNSUPPORTED_L4
//! @brief     L4 protocol specified in IP header corrupted
#define RNET_BUF_CODE_IP_UNSUPPORTED_L4              21
//! @name      RNET_BUF_CODE_UDP_PACKET_TOO_SMALL
//! @brief     Obvious UDP packet undersized
#define RNET_BUF_CODE_UDP_PACKET_TOO_SMALL           22
//! @name      RNET_BUF_CODE_UDP_INTFC_NOT_FOUND
//! @brief     Buf/pcl header intfc value doesn't match configured intfcs
#define RNET_BUF_CODE_UDP_INTFC_NOT_FOUND            22
//! @name      RNET_BUF_CODE_UDP_SUBI_NOT_FOUND
//! @brief     Buf/pcl header subi value doesn't match configured subis
#define RNET_BUF_CODE_UDP_SUBI_NOT_FOUND             23
//! @name      RNET_BUF_CODE_UDP_CIRCUIT_NOT_FOUND
//! @brief     Buf/pcl header circuit value doesn't match configured circuits
#define RNET_BUF_CODE_UDP_CIRCUIT_NOT_FOUND          24


// recommend 4-byte alignment
typedef struct
{
    uint16_t             offset;
    uint16_t             length;
    uint8_t              intfc;         // cast to 'rnet_intfc_t' 
    uint8_t              subi;          // cast to 'rnet_subi_t'
    uint8_t              circuit;       // rnet circuit index
    uint8_t              previous_ph;   // cast to 'rnet_ph_t'; last protocol header type
    uint32_t             code;          // message-specific code
} rnet_buf_header_t;


typedef struct rnet_buf_t_
{
    struct rnet_buf_t_  *flink;
    rnet_buf_header_t    header;
    uint8_t              buf[RNET_BUF_SIZE];
} rnet_buf_t;


#ifndef RNET_BUF_GLOBAL_DEFS
    extern rnet_buf_t rnet_buf[RNET_NUM_BUFS];
#endif

//!
//! @name      IS_RNET_BUF
//!
//! @brief     Returns 'true' if 'x' is an RNET buffer
//!
#define IS_RNET_BUF(x)       ( ((x) >= rnet_buf) &&                  \
                               ((x) <= &rnet_buf[RNET_NUM_BUFS - 1]) )

//!
//! @name      RNET_BUF_FRAME_START_PTR
//! @name      RNET_BUF_FRAME_END_PTR
//!
//! @details   'input' is a type 'rnet_buf_t', output is
//! @details   a type 'uint8_t *' pointing to start of frame or to
//! @details   end of frame (points to next byte after last byte in frame)
//!
#define RNET_BUF_FRAME_START_PTR(input) ( &((input)->buf[(input)->header.offset]) )
#define RNET_BUF_FRAME_END_PTR(input)   ( &((input)->buf[(input)->header.offset + (input)->header.length]) )

#endif  // RNET_BUF_H_