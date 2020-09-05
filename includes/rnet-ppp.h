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

//! @file     rnet-ppp.h
//! @authors  Bernie Woodland
//! @date     6May2018
//!
//! @brief    PPP Protocol
//!

#ifndef RNET_PPP_H_
#define RNET_PPP_H_

#include "raging-global.h"

#include "rnet-compile-switches.h"
#include "rnet-app.h"
#include "rnet-buf.h"
#include "nsvc-api.h"

//!
//! @name      PPP_ACFC_LENGTH
//! @name      PPP_PROTOCOL_VALUE_LENGTH
//! @name      PPP_PREFIX_LENGTH
//!
//! @details   PPP header lengths: ACFC length, PPP protocol field length,
//! @details   total PPP header length
//!
#define PPP_ACFC_LENGTH            2 // length of 'PPP_ACFC' value, bytes

#define PPP_PROTOCOL_VALUE_LENGTH  2 // length of PPP protocol field, bytes

// Num. bytes from start of PPP frame to PPP payload
#define PPP_PREFIX_LENGTH      (PPP_ACFC_LENGTH + PPP_PROTOCOL_VALUE_LENGTH)

//!
//! @enum      rnet_ppp_state_t
//!
//! @brief     PPP connection state (not exactly per RFC)
//!
typedef enum
{
    RNET_PPP_STATE_RECOVERY = 1, // Recovering from disruption
    RNET_PPP_STATE_PROBING,      // Waiting for peer to come online
    RNET_PPP_STATE_NEGOTIATING,
    RNET_PPP_STATE_UP
} rnet_ppp_state_t;

//!
//! @enum      rnet_ppp_event_t
//!
//! @brief     PPP state machine input event
//!
typedef enum
{
    RNET_PPP_EVENT_INIT,
    RNET_PPP_EVENT_RX_LCP_CONFIG_REQUEST,
    RNET_PPP_EVENT_RX_LCP_CONFIG_ACK,
    RNET_PPP_EVENT_RX_IPCP_CONFIG_REQUEST,
    RNET_PPP_EVENT_RX_IPCP_CONFIG_ACK,
    RNET_PPP_EVENT_RX_IPV6CP_CONFIG_REQUEST,
    RNET_PPP_EVENT_RX_IPV6CP_CONFIG_ACK,
    RNET_PPP_EVENT_RX_TERMINATE_REQUEST,
    RNET_PPP_EVENT_RX_TERMINATE_ACK,
    RNET_PPP_EVENT_TIMEOUT_RECOVERY,
    RNET_PPP_EVENT_TIMEOUT_PROBING,
    RNET_PPP_EVENT_TIMEOUT_NEGOTIATING,
} rnet_ppp_event_t;

//!
//! @enum      rnet_ppp_protocol_t
//!
//! @brief     PPP protocol field definitions
//!
typedef enum
{
    RNET_PPP_PROTOCOL_LCP = 0xC021,
    RNET_PPP_PROTOCOL_IPCP = 0x8021,
    RNET_PPP_PROTOCOL_IPV6CP = 0x8057,
    RNET_PPP_PROTOCOL_IPV4 = 0x0021,
    RNET_PPP_PROTOCOL_IPV6 = 0x0057,
} rnet_ppp_protocol_t;

//!
//! @enum      rnet_xcp_code_t
//!
//! @brief     LCP, IPCP, IPV6CP code values
//!
typedef enum
{
    RNET_XCP_CONF_REQ = 0x1,
    RNET_XCP_CONF_ACK = 0x2,
    RNET_XCP_CONF_NAK = 0x3,
    RNET_XCP_CONF_REJ = 0x4,
    RNET_XCP_TERM_REQ = 0x5,
    RNET_XCP_TERM_ACK = 0x6,
    RNET_XCP_PROT_REJ = 0x8,
    RNET_XCP_ECHO_REQ = 0x9,
    RNET_XCP_ECHO_ACK = 0xA
} rnet_xcp_code_t;


//!
//! @enum      rnet_lcp_type_t
//!
//! @brief     LCP type code values
//!
typedef enum
{
    RNET_LCP_TYPE_MAX_RECEIVE_UNIT = 1,
    RNET_LCP_TYPE_AUTHENTICATION_PROTOCOL = 3,
    RNET_LCP_TYPE_QUALITY_PROTOCOL = 4,
    RNET_LCP_TYPE_MAGIC_NUMBER = 5,
    RNET_LCP_TYPE_PROTOCOL_FIELD_COMPRESSION = 7,
    RNET_LCP_TYPE_ADDR_AND_CTRL_FIELD_COMPRESSION = 8,
} rnet_lcp_type_t;

// APIs
RAGING_EXTERN_C_START
void rnet_msg_ppp_init(uint32_t parameter);
void rnet_ppp_timeout(rnet_ppp_event_t event, uint32_t msg_parm);
void rnet_ppp_state_clear(rnet_intfc_t intfc);
bool rnet_ppp_state_machine(rnet_intfc_t intfc, rnet_ppp_event_t event);
void rnet_msg_rx_buf_ppp(rnet_buf_t *buf);
void rnet_msg_rx_pcl_ppp(nsvc_pcl_t *head_pcl);
void rnet_msg_rx_buf_lcp(rnet_buf_t *buf);
void rnet_msg_rx_pcl_lcp(nsvc_pcl_t *head_pcl);
void rnet_msg_rx_pcl_ipcp(nsvc_pcl_t *head_pcl);
void rnet_msg_rx_buf_ipcp(rnet_buf_t *buf);
void rnet_msg_rx_buf_ipv6cp(rnet_buf_t *buf);
void rnet_msg_rx_pcl_ipv6cp(nsvc_pcl_t *head_pcl);
void rnet_msg_tx_buf_ppp(rnet_buf_t *buf);
void rnet_msg_tx_pcl_ppp(nsvc_pcl_t *head_pcl);
RAGING_EXTERN_C_END

#endif  // RNET_PPP_H_