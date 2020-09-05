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

//! @file     rnet-top.h
//! @authors  Bernie Woodland
//! @date     12May18
//!
//! @brief    Top-level and message interface to RNET stack
//!

#ifndef RNET_TOP_H_
#define RNET_TOP_H_

#include "raging-global.h"
#include "rnet-compile-switches.h"

//!
//! @enum      rnet_id_t
//!
//! @brief     Messages for RNET stack, internal and API/external
//!
typedef enum
{
    RNET_ID_RX_BUF_ENTRY,             // Raw rx packets of type 'rnet_buf_t'
    RNET_ID_RX_BUF_AHDLC_STRIP_CC,    // Remove AHDLC control characters
    RNET_ID_RX_BUF_AHDLC_VERIFY_CRC,  // Remove+verify AHDLC CRC16
    RNET_ID_RX_BUF_PPP,               // Process rx PPP frame
    RNET_ID_RX_BUF_LCP,               // PPP LCP protocol frames
    RNET_ID_RX_BUF_IPV4CP,            // PPP IPv4 control protocol frames
    RNET_ID_RX_BUF_IPV6CP,            // PPP IPv6 control protocol frames
    RNET_ID_RX_BUF_IPV4,              // IPv4 packets
    RNET_ID_RX_BUF_IPV6,              // IPv6 packets
    RNET_ID_RX_BUF_UDP,               // UDP
    RNET_ID_RX_BUF_ICMP,              // ICMPv4
    RNET_ID_RX_BUF_ICMPV6,            // ICMPv6

    RNET_ID_TX_BUF_UDP,               // Entry point to stack to send a packet
    RNET_ID_TX_BUF_IPV4,              //
    RNET_ID_TX_BUF_IPV6,              //
    RNET_ID_TX_BUF_PPP,               // PPP encapsulation
    RNET_ID_TX_BUF_AHDLC_CRC,         // Entry to AHDLC tx: append CRC
    RNET_ID_TX_BUF_AHDLC_ENCODE_CC,   // Add AHDLC control characters
    RNET_ID_TX_BUF_DRIVER,            // Send frame to driver. Last step for Tx.

    RNET_ID_BUF_DISCARD,              // error, throw this buffer away!

    RNET_ID_RX_PCL_ENTRY,             // Raw rx packets of type 'nsvc_pcl_t'
    RNET_ID_RX_PCL_AHDLC_STRIP_CC,    // Remove AHDLC control characters
    RNET_ID_RX_PCL_AHDLC_VERIFY_CRC,  // Remove+verify AHDLC CRC16
    RNET_ID_RX_PCL_PPP,               // Process rx PPP frame
    RNET_ID_RX_PCL_LCP,               // PPP LCP protocol frames
    RNET_ID_RX_PCL_IPV4CP,            // PPP IPv4 control protocol frames
    RNET_ID_RX_PCL_IPV6CP,            // PPP IPv6 control protocol frames
    RNET_ID_RX_PCL_IPV4,              // IPv4 packets
    RNET_ID_RX_PCL_IPV6,              // IPv6 packets
    RNET_ID_RX_PCL_UDP,               // UDP
    RNET_ID_RX_PCL_ICMP,              // ICMPv4
    RNET_ID_RX_PCL_ICMPV6,            // ICMPv6

    RNET_ID_TX_PCL_UDP,               // Entry point to stack to send a packet
    RNET_ID_TX_PCL_IPV4,              //
    RNET_ID_TX_PCL_IPV6,              //
    RNET_ID_TX_PCL_PPP,               // PPP encapsulation
    RNET_ID_TX_PCL_AHDLC_CRC,         // Entry to AHDLC tx: append CRC
    RNET_ID_TX_PCL_AHDLC_ENCODE_CC,   // Add AHDLC control characters
    RNET_ID_TX_PCL_DRIVER,            // Send frame to driver. Last step for Tx.

    RNET_ID_PCL_DISCARD,              // error, throw this particle chain away!

    RNET_ID_PPP_INIT,                 // Init PPP on an intfc
    RNET_ID_PPP_UP,                   // if PPP, PPP just came up (finished negotiating)
    RNET_ID_PPP_DOWN,                 // if PPP, PPP was up and came down
    RNET_ID_PPP_TIMEOUT_RECOVERY,     // if PPP configured: timeout in recovery state
    RNET_ID_PPP_TIMEOUT_PROBING,      // if PPP configured: timeout in probing state
    RNET_ID_PPP_TIMEOUT_NEGOTIATING,  // if PPP configured: timeout in negotiating state
} rnet_id_t;

// APIs
RAGING_EXTERN_C_START
void rnet_msg_processor(rnet_id_t msg_id, uint32_t optional_parameter);
RAGING_EXTERN_C_END

#endif  // RNET_TOP_H_