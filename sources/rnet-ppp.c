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

//! @file     rnet-ppp.c
//! @authors  Bernie Woodland
//! @date     9Apr18
//!
//! @brief    PPP Protocol
//!
//! @details  References:
//! @details    RFC 1661: The Point-to-Point Protocol
//! @details    RFC 1662: PPP in HDLC-like Framing
//! @details    RFC 2472: PPP over IPv6
//!

#include "rnet-ppp.h"
#include "rnet-intfc.h"
#include "rnet-dispatch.h"

#include "raging-utils-mem.h"
#include "raging-utils.h"
#include "raging-contract.h"

//
// <AHDLC-FRAME-START (1)> <AHDLC-CHARACTER-TRANSLATED-FRAME> <AHDLC-CRC (2)> <AHDLC-FRAME-END (1)>
//
// PPP-FRAME:
//    <PPP-ACFC (2)> <PPP-PROTOCOL (2)> <PPP-PAYLOAD (N)>
//
// PPP-PAYLOAD: PPP-PROTOCOL=XCP (LCP,IPCP,IPV6CP)
//    <XCP-CODE (1)> <XCP-ID> <XCP-LENGTH (2)> <XCP-PAYLOAD (N)>
//        (NOTE: XCP-LENGTH value includes XCP-CODE, XCP-ID, XCP-LENGTH, XCP-PAYLOAD fields)
//
// XCP-PAYLOAD:
//    <XCP-CONFIG-OPTION>
//          -- or --
//    <TBD/other>
//
// XCP-CONFIG-OPTION
//    <XCP-TYPE (1)> <XCP-OPTION-LENGTH (1)> <XCP-OPTION-VALUE (N)>
//         (NOTE: XCP-OPTION-LENGTH value includes XCP-TYPE, XCP-OPTION-LENGTH, XCP-OPTION-VALUE fields)
//
// PPP-PAYLOAD: IPv4
//    <IPv4 packet>
//
// PPP-PAYLOAD: IPv6
//    <IPv6 packet>
//

#define PPP_ACFC              0xFF03

//!
//! @name      RECOVERY_CYCLES
//!
//! @details   Max. number of iterations in recovery state
//!
#define RECOVERY_CYCLES             2

//!
//! @name      NEGOTIATION_CYCLES
//!
//! @details   Max. number of iterations in negotiation mode
//! @details   (TBD!!!)
//!
#define NEGOTIATION_CYCLES         20

// Offset for PPP protocol packets
// Must be >= PPP_PREFIX_LENGTH + XCP_LENGTH_ADJUSTMENT
#define TX_PPP_PROTOCOL_OFFSET         10

// Adjustment to XCP-LENGTH value, so length is payload only
#define XCP_LENGTH_ADJUSTMENT           4

// XCP-LENGTH field length
#define XCP_LENGTH_LENGTH               2

// Adjustment to XCP-OPTION-LENGTH value, so length is payload only
#define XCP_OPTION_LENGTH_ADJUSTMENT    2

//!
//! @name      TIMEOUT_RECOVERY
//! @name      TIMEOUT_PROBING
//! @name      TIMEOUT_NEGOTIATING
//!
//! @details   Timer intervals between iterations in a given state
//! @details    (All timeouts in millisecs)
//!
#define TIMEOUT_RECOVERY          200
#define TIMEOUT_PROBING          1000
#define TIMEOUT_NEGOTIATING       100


// Internal functions
static bool ppp_is_supported_protocol(rnet_ppp_protocol_t protocol);
/*static*/ bool ppp_is_xcp_protocol(rnet_ppp_protocol_t protocol);
static bool ppp_is_supported_xcp_code(rnet_xcp_code_t code);
static bool ppp_is_supported_ipcp_code(rnet_xcp_code_t code);
static bool ppp_is_ack_code(rnet_xcp_code_t code);
static rnet_ph_t ppp_protocol_to_ph(rnet_ppp_protocol_t protocol);
static rnet_ppp_protocol_t ppp_ph_to_ppp_protocol(rnet_ph_t ph);
static bool ppp_state_recovery(rnet_intfc_t intfc, rnet_ppp_event_t event);
static bool ppp_state_probing(rnet_intfc_t intfc, rnet_ppp_event_t event);
static bool ppp_state_negotiating(rnet_intfc_t intfc, rnet_ppp_event_t event);
static bool ppp_state_up(rnet_intfc_t intfc, rnet_ppp_event_t event);
static void ppp_state_restart_recovery(rnet_intfc_t intfc);
static bool rx_ppp(uint8_t             *stream,
                   uint8_t             *end_stream,
                   rnet_intfc_t         intfc,
                   rnet_ppp_protocol_t *protocol_ptr);
static void ppp_tx_lcp_config_req(rnet_intfc_t intfc);
static void ppp_tx_ipcp_config_req(rnet_intfc_t intfc);
static void ppp_tx_ipv6cp_config_req(rnet_intfc_t intfc);
static void ppp_tx_lcp_term_req(rnet_intfc_t intfc);
static void ppp_tx_xcp_request(rnet_intfc_t        intfc,
                               rnet_xcp_code_t     code,
                               rnet_ppp_protocol_t protocol,
                               const uint8_t      *data_string,
                               unsigned            data_string_length);
static void ppp_tx_add_code_id_length_wrapper(uint8_t         *buffer,
                                              rnet_intfc_t     intfc,
                                              rnet_xcp_code_t  code,
                                              unsigned         data_length);
static void ppp_tx_add_ppp_wrapper(uint8_t            *buffer,
                                   rnet_ppp_protocol_t protocol);
#if RNET_CS_USING_BUFS_FOR_TX == 1
    static rnet_buf_t *ppp_tx_buf_alloc(rnet_intfc_t intfc);
#elif RNET_CS_USING_PCLS_FOR_TX == 1
    static nsvc_pcl_t *ppp_tx_pcl_alloc(rnet_intfc_t intfc);
#else
    #error "Neither buffer nor pcl!"
#endif  // RNET_CS_USING_BUFS_FOR_TX == 1

//!
//! @name      ppp_is_supported_protocol
//!
//! @brief     Tests if RNET PPP supports given protocol
//!
//! @param[in] 'protocol'-- PPP protocol type
//!
//! @return    'true' if supported
//!
static bool ppp_is_supported_protocol(rnet_ppp_protocol_t protocol)
{
    switch (protocol)
    {
    case RNET_PPP_PROTOCOL_LCP:
    case RNET_PPP_PROTOCOL_IPCP:
    case RNET_PPP_PROTOCOL_IPV6CP:
    case RNET_PPP_PROTOCOL_IPV4:
    case RNET_PPP_PROTOCOL_IPV6:
        return true;
        break;
    default:
        break;
    }

    return false;
}

//!
//! @name      ppp_is_xcp_protocol
//!
//! @brief     Tests if RNET PPP supports given PPP sub-protocol
//!
//! @param[in] 'protocol'-- PPP sub-protocol type (LCP,IPCP,IPV6CP,etc)
//!
//! @return    'true' if supported
//!
/*static*/ bool ppp_is_xcp_protocol(rnet_ppp_protocol_t protocol)
{
    switch (protocol)
    {
    case RNET_PPP_PROTOCOL_LCP:
    case RNET_PPP_PROTOCOL_IPCP:
    case RNET_PPP_PROTOCOL_IPV6CP:
        return true;
        break;
    default:
        break;
    }

    return false;
}

//!
//! @name      ppp_is_supported_xcp_code
//!
//! @brief     Tests if any RNET PPP sub-protocol (LCP,etc) supports a given code
//!
//! @param[in] 'code'-- PPP sub-protocol code
//!
//! @return    'true' if supported
//!
static bool ppp_is_supported_xcp_code(rnet_xcp_code_t code)
{
    switch (code)
    {
    case RNET_XCP_CONF_REQ:
    case RNET_XCP_CONF_ACK:
    case RNET_XCP_CONF_REJ:
    case RNET_XCP_TERM_REQ:
    case RNET_XCP_TERM_ACK:
    case RNET_XCP_PROT_REJ:
    case RNET_XCP_ECHO_REQ:
    case RNET_XCP_ECHO_ACK:
        return true;
        break;
    case RNET_XCP_CONF_NAK:
    default:
        break;
    }

    return false;
}

//!
//! @name      ppp_is_supported_ipcp_code
//!
//! @brief     Tests if RNET PPP IPCP or IPV6CP supports code value
//!
//! @param[in] 'code'-- PPP sub-protocol code
//!
//! @return    'true' if supported
//!
static bool ppp_is_supported_ipcp_code(rnet_xcp_code_t code)
{
    switch (code)
    {
    case RNET_XCP_CONF_REQ:
    case RNET_XCP_CONF_ACK:
    case RNET_XCP_CONF_REJ:
        return true;
        break;
    case RNET_XCP_TERM_REQ:
    case RNET_XCP_TERM_ACK:
    case RNET_XCP_PROT_REJ:
    case RNET_XCP_ECHO_REQ:
    case RNET_XCP_ECHO_ACK:
    case RNET_XCP_CONF_NAK:
    default:
        break;
    }

    return false;
}

//!
//! @name      ppp_is_ack_code
//!
//! @brief     Tests if this is an LCP, IPCP, IPV6CP ack code
//!
//! @param[in] 'code'-- PPP sub-protocol code
//!
//! @return    'true' if is an ack code
//!
static bool ppp_is_ack_code(rnet_xcp_code_t code)
{
    switch (code)
    {
    case RNET_XCP_CONF_ACK:
    case RNET_XCP_TERM_ACK:
    case RNET_XCP_ECHO_ACK:
    case RNET_XCP_CONF_NAK:
        return true;
        break;
    default:
        break;
    }

    return false;
}

//!
//! @name      ppp_protocol_to_ph
//!
//! @brief     Converts PPP Protocol field value to buffer/pcl header
//! @brief     protocol value.
//!
//! @param[in] 'protocol'-- 
//!
//! @return    output
//!
static rnet_ph_t ppp_protocol_to_ph(rnet_ppp_protocol_t protocol)
{
    rnet_ph_t ph;

    switch (protocol)
    {
    case RNET_PPP_PROTOCOL_LCP:
        ph = RNET_PH_LCP;
        break;
    case RNET_PPP_PROTOCOL_IPCP:
        ph = RNET_PH_IPCP;
        break;
    case RNET_PPP_PROTOCOL_IPV6CP:
        ph = RNET_PH_IPV6CP;
        break;
    case RNET_PPP_PROTOCOL_IPV4:
        ph = RNET_PH_IPV4;
        break;
    case RNET_PPP_PROTOCOL_IPV6:
        ph = RNET_PH_IPV6;
        break;
    default:
        ph = RNET_PH_null;
        break;
    }

    return ph;
}

//!
//! @name      ppp_ph_to_ppp_protocol
//!
//! @brief     Converts buffer/pcl header protocol value to PPP Protocol
//! @brief     field value.
//!
//! @param[in] 'ph'-- 
//!
//! @return    PPP protocol value
//!
static rnet_ppp_protocol_t ppp_ph_to_ppp_protocol(rnet_ph_t ph)
{
    rnet_ppp_protocol_t ppp_protocol;

    switch (ph)
    {
    //case RNET_PH_AHDLC:
    //case RNET_PH_PPP:
    case RNET_PH_LCP:
        ppp_protocol = RNET_PPP_PROTOCOL_LCP;
        break;
    case RNET_PH_IPCP:
        ppp_protocol = RNET_PPP_PROTOCOL_IPCP;
        break;
    case RNET_PH_IPV6CP:
        ppp_protocol = RNET_PPP_PROTOCOL_IPV6CP;
        break;
    case RNET_PH_IPV4:
        ppp_protocol = RNET_PPP_PROTOCOL_IPV4;
        break;
    case RNET_PH_IPV6:
        ppp_protocol = RNET_PPP_PROTOCOL_IPV6;
        break;
    default:
        ppp_protocol = RNET_PH_null;
        break;
    }

    return ppp_protocol;
}

//!
//! @name      rnet_msg_ppp_init
//!
//! @brief     Initialize PPP session on an interface
//!
//! @param[in] 'parameter'-- maps to an interface
//!
void rnet_msg_ppp_init(uint32_t parameter)
{
    const rnet_intfc_rom_t *intfc_rom_ptr;
    rnet_intfc_ram_t       *intfc_ram_ptr;
    rnet_intfc_t            intfc;

    intfc = (rnet_intfc_t)parameter;
    if (!rnet_intfc_is_valid(intfc))
    {
        return;
    }

    intfc_rom_ptr = rnet_intfc_get_rom(intfc);
    intfc_ram_ptr = rnet_intfc_get_ram(intfc);

    if (RNET_L2_PPP != intfc_rom_ptr->l2_type)
    {
        return;
    }

    // init ppp state
    intfc_ram_ptr->l2_state.ppp.state = RNET_PPP_STATE_RECOVERY;

    // Notify apps that PPP went down
    rnet_send_msgs_to_event_list(RNET_NOTIF_INTFC_DOWN, intfc);

    // restart PPP
    rnet_ppp_state_machine(intfc, RNET_PPP_EVENT_INIT);
}

//!
//! @name      rnet_ppp_timeout
//!
//! @brief     Handles generic RNET PPP interface timeout event
//!
//! @param[in] 'event'-- timeout event
//! @param[in] 'timer'-- interface timer
//!
//! @return    'true' if is an ack code
//!
void rnet_ppp_timeout(rnet_ppp_event_t event, uint32_t msg_parm)
{
    rnet_intfc_t intfc;
    bool         is_valid_intfc;

    // This timer could be bound to any interface
    intfc = msg_parm;

    is_valid_intfc = RNET_INTFC_null != intfc;
    SL_REQUIRE(is_valid_intfc);

    if (is_valid_intfc)
    {
        // Inject in state machine
        rnet_ppp_state_machine(intfc, event);
    }
}

//!
//! @name      rnet_ppp_state_clear
//!
//! @brief     RNET PPP state machine
//!
//! @details   Receives event and changes state of PPP stack.
//! @details   One state variable per interface running PPP.
//! @details   
//! @details   
//!
//! @param[in] 'event'-- timeout event
//! @param[in] 'timer'-- interface timer
//!
void rnet_ppp_state_clear(rnet_intfc_t intfc)
{
    rnet_intfc_ram_t       *intfc_ram_ptr;
    rnet_ppp_intfc_state_t *ppp_state_ptr;

    intfc_ram_ptr = rnet_intfc_get_ram(intfc);
    ppp_state_ptr = &(intfc_ram_ptr->l2_state.ppp);

    // Clear all negotiation states
    ppp_state_ptr->lcp_tx_closed = false;
    ppp_state_ptr->lcp_rx_closed = false;
    ppp_state_ptr->ipcp_tx_closed = false;
    ppp_state_ptr->ipcp_rx_closed = false;
    ppp_state_ptr->ipv6cp_tx_closed = false;
    ppp_state_ptr->ipv6cp_rx_closed = false;
}

//!
//! @name      rnet_ppp_state_machine
//!
//! @brief     RNET PPP state machine
//!
//! @details   Receives event and changes state of PPP stack.
//! @details   One state variable per interface running PPP.
//! @details   
//! @details   
//!
//! @param[in] 'intfc'-- interface
//! @param[in] 'in_event'-- state event
//!
//! @return    'true' if ack packet should be sent. Only relevant
//! @return       for certain configs of states and events.
//!
bool rnet_ppp_state_machine(rnet_intfc_t intfc, rnet_ppp_event_t in_event)
{
    rnet_intfc_ram_t       *intfc_ram_ptr;
    rnet_ppp_intfc_state_t *ppp_state_ptr;
    bool                    send_ack = false;

    intfc_ram_ptr = rnet_intfc_get_ram(intfc);
    ppp_state_ptr = &(intfc_ram_ptr->l2_state.ppp);

    switch (ppp_state_ptr->state)
    {
    // In recovery state, we attempt to clear any previous connection
    case RNET_PPP_STATE_RECOVERY:
        send_ack = ppp_state_recovery(intfc, in_event);
        break;

    // In probing state, we attemtp to discover a peer which wants to negotiate
    case RNET_PPP_STATE_PROBING:
        send_ack = ppp_state_probing(intfc, in_event);
        break;

    // In negotiating state, peer has sent at least 1 negotiation packet
    case RNET_PPP_STATE_NEGOTIATING:
        send_ack = ppp_state_negotiating(intfc, in_event);
        break;

    // Negotiated up already
    case RNET_PPP_STATE_UP:
        send_ack = ppp_state_up(intfc, in_event);
        break;

    default:
        // Should never reach here
        break;
    }

    return send_ack;
}

//!
//! @name      ppp_state_recovery
//!
//! @brief     Handles recovery state
//!
//! @details   Recovery state attempts to recover from
//! @details   a "stuck" state, should one exist.
//! @details   
//! @details   
//!
//! @param[in] 'intfc'-- interface
//! @param[in] 'in_event'-- state event
//!
//! @return     'true' if ack packet should be sent
//!
static bool ppp_state_recovery(rnet_intfc_t intfc, rnet_ppp_event_t in_event)
{
    rnet_intfc_ram_t       *intfc_ram_ptr;
    rnet_ppp_intfc_state_t *ppp_state_ptr;
    bool                    send_ack = false;

    intfc_ram_ptr = rnet_intfc_get_ram(intfc);
    ppp_state_ptr = &(intfc_ram_ptr->l2_state.ppp);

    switch (in_event)
    {
    case RNET_PPP_EVENT_INIT:
        ppp_state_restart_recovery(intfc);
        break;

    case RNET_PPP_EVENT_TIMEOUT_RECOVERY:
        // Haven't run through enough iterations yet?
        if (ppp_state_ptr->completion_counter > 0)
        {
            (ppp_state_ptr->completion_counter)--;

        #if RNET_ENABLE_PPP_TEST_MODE == 0
            rnet_intfc_timer_set(intfc,
                                 RNET_ID_PPP_TIMEOUT_RECOVERY,
                                 TIMEOUT_RECOVERY);
        #else
            if (RNET_INTFC_TEST1 == intfc)
            {
                rnet_intfc_timer_set(intfc,
                                     RNET_ID_PPP_TIMEOUT_RECOVERY,
                                     TIMEOUT_RECOVERY - 20);
            }
            else
            {
                rnet_intfc_timer_set(intfc,
                                     RNET_ID_PPP_TIMEOUT_RECOVERY,
                                     TIMEOUT_RECOVERY);
            }
        #endif

            // Send out a terminate request to clear line
            ppp_tx_lcp_term_req(intfc);
        }
        // We're done with all attempts, go to probing
        else
        {
            rnet_intfc_timer_set(intfc,
                                 RNET_ID_PPP_TIMEOUT_PROBING,
                                 TIMEOUT_PROBING);

            ppp_state_ptr->state = RNET_PPP_STATE_PROBING;
            ppp_state_ptr->completion_counter = NEGOTIATION_CYCLES;

            // Send first config request
            ppp_tx_lcp_config_req(intfc);
        }
        break;

    // Either got a reply to one of our terminate requests,
    // or other side is initiating termination.
    // Either way, we can exit recovery.
    case RNET_PPP_EVENT_RX_TERMINATE_REQUEST:
    case RNET_PPP_EVENT_RX_TERMINATE_ACK:
        rnet_intfc_timer_kill(intfc);

        ppp_state_ptr->state = RNET_PPP_STATE_NEGOTIATING;
        ppp_state_ptr->completion_counter = NEGOTIATION_CYCLES;

        rnet_intfc_timer_set(intfc,
                             RNET_ID_PPP_TIMEOUT_NEGOTIATING,
                             TIMEOUT_RECOVERY);

        send_ack = RNET_PPP_EVENT_RX_TERMINATE_REQUEST == in_event;

        break;

    // Peer sent us a config request? It must be ready to go;
    //   accept request and advance to negotiationg state
    case RNET_PPP_EVENT_RX_LCP_CONFIG_REQUEST:
        rnet_intfc_timer_kill(intfc);

        // We started negotiations...
        ppp_state_ptr->lcp_rx_closed = true;

        send_ack = true;

        ppp_state_ptr->state = RNET_PPP_STATE_NEGOTIATING;
        ppp_state_ptr->completion_counter = NEGOTIATION_CYCLES;

        rnet_intfc_timer_set(intfc,
                             RNET_ID_PPP_TIMEOUT_NEGOTIATING,
                             TIMEOUT_RECOVERY);
    
        break;

    default:
        break;
    }

    return send_ack;
}

//!
//! @name      ppp_state_probing
//!
//! @brief     Handles probing state
//!
//! @details   Probing state attempts to make contact
//! @details   and begin negotiating with a peer
//! @details   
//! @details   
//!
//! @param[in] 'intfc'-- interface
//! @param[in] 'in_event'-- state event
//!
//! @return     'true' if ack packet should be sent
//!
static bool ppp_state_probing(rnet_intfc_t intfc, rnet_ppp_event_t in_event)
{
    rnet_intfc_ram_t       *intfc_ram_ptr;
    rnet_ppp_intfc_state_t *ppp_state_ptr;
    bool                    send_ack = false;

    intfc_ram_ptr = rnet_intfc_get_ram(intfc);
    ppp_state_ptr = &(intfc_ram_ptr->l2_state.ppp);

    // If init event, reset back to recovery
    switch (in_event)
    {
    case RNET_PPP_EVENT_INIT:
    case RNET_PPP_EVENT_RX_TERMINATE_REQUEST:
        ppp_state_restart_recovery(intfc);
        send_ack = RNET_PPP_EVENT_RX_TERMINATE_REQUEST == in_event;
        break;

    case RNET_PPP_EVENT_TIMEOUT_PROBING:
        // Haven't run through enough iterations yet?
        if (ppp_state_ptr->completion_counter > 0)
        {
            (ppp_state_ptr->completion_counter)--;

        #if RNET_ENABLE_PPP_TEST_MODE == 0
            rnet_intfc_timer_set(intfc,
                                 RNET_ID_PPP_TIMEOUT_PROBING,
                                 TIMEOUT_PROBING);
        #else
            if (RNET_INTFC_TEST1 == intfc)
            {
                rnet_intfc_timer_set(intfc,
                                     RNET_ID_PPP_TIMEOUT_PROBING,
                                     TIMEOUT_PROBING - 20);
            }
            else
            {
                rnet_intfc_timer_set(intfc,
                                     RNET_ID_PPP_TIMEOUT_PROBING,
                                     TIMEOUT_PROBING);
            }
        #endif

            // Try initiating a negotiation by sending
            //  a config request.
            ppp_tx_lcp_config_req(intfc);
        }
        // We're done with all attempts, go to recovery
        else
        {
            rnet_ppp_state_clear(intfc);

        #if RNET_ENABLE_PPP_TEST_MODE == 0
            rnet_intfc_timer_set(intfc,
                                 RNET_ID_PPP_TIMEOUT_RECOVERY,
                                 TIMEOUT_RECOVERY);
        #else
            if (RNET_INTFC_TEST1 == intfc)
            {
                rnet_intfc_timer_set(intfc,
                                     RNET_ID_PPP_TIMEOUT_RECOVERY,
                                     TIMEOUT_RECOVERY - 20);
            }
            else
            {
                rnet_intfc_timer_set(intfc,
                                     RNET_ID_PPP_TIMEOUT_RECOVERY,
                                     TIMEOUT_RECOVERY);
            }
        #endif

            ppp_state_ptr->state = RNET_PPP_STATE_RECOVERY;
        }
        break;

    // Peer sent us a config request? It must be ready to go;
    //   accept request and advance to negotiationg state
    case RNET_PPP_EVENT_RX_LCP_CONFIG_REQUEST:
        rnet_intfc_timer_kill(intfc);

        // We started negotiations...
        ppp_state_ptr->lcp_rx_closed = true;

        send_ack = true;

        ppp_state_ptr->state = RNET_PPP_STATE_NEGOTIATING;
        ppp_state_ptr->completion_counter = NEGOTIATION_CYCLES;
    
        rnet_intfc_timer_set(intfc,
                             RNET_ID_PPP_TIMEOUT_NEGOTIATING,
                             TIMEOUT_RECOVERY);
        break;

    // Got an ack back from one of our requests.
    // We're going.
    case RNET_PPP_EVENT_RX_LCP_CONFIG_ACK:
        rnet_intfc_timer_kill(intfc);

        // We started negotiations...
        ppp_state_ptr->lcp_tx_closed = true;

        ppp_state_ptr->state = RNET_PPP_STATE_NEGOTIATING;
        ppp_state_ptr->completion_counter = NEGOTIATION_CYCLES;

        rnet_intfc_timer_set(intfc,
                             RNET_ID_PPP_TIMEOUT_NEGOTIATING,
                             TIMEOUT_RECOVERY);
        break;
    
    default:
        break;
    }

    return send_ack;
}

//!
//! @name      ppp_state_negotiating
//!
//! @brief     Handles negotiation state
//!
//! @details   Actively bringing up PPP with peer
//! @details   
//! @details   
//!
//! @param[in] 'intfc'-- interface
//! @param[in] 'in_event'-- state event
//!
//! @return     'true' if ack packet should be sent
//!
static bool ppp_state_negotiating(rnet_intfc_t intfc, rnet_ppp_event_t in_event)
{
    rnet_intfc_ram_t       *intfc_ram_ptr;
    rnet_ppp_intfc_state_t *ppp_state_ptr;
    uint16_t                options;
    bool                    has_ipv4;
    bool                    has_ipv6;
    bool                    lcp_closed;
    bool                    ipcp_closed;
    bool                    ipv6cp_closed;
    bool                    send_ack = false;

    intfc_ram_ptr = rnet_intfc_get_ram(intfc);
    ppp_state_ptr = &(intfc_ram_ptr->l2_state.ppp);

    // Are IPv4 and IPv6 configured for this intfc?
    // Just one, not the other?
    options = rnet_intfc_get_options(intfc);
    has_ipv4 = (options & RNET_IOPT_PPP_IPCP) != 0;
    has_ipv6 = (options & RNET_IOPT_PPP_IPV6CP) != 0;

    // Which protocols are closed?
    // If protocol not configured, consider it closed
    lcp_closed = ppp_state_ptr->lcp_tx_closed &&
                 ppp_state_ptr->lcp_rx_closed;
    ipcp_closed = (ppp_state_ptr->ipcp_tx_closed &&
                   ppp_state_ptr->ipcp_rx_closed)
                   || !has_ipv4;
    ipv6cp_closed = (ppp_state_ptr->ipv6cp_tx_closed &&
                     ppp_state_ptr->ipv6cp_rx_closed)
                   || !has_ipv6;

    switch (in_event)
    {
    // If init event, reset back to recovery
    case RNET_PPP_EVENT_INIT:
    case RNET_PPP_EVENT_RX_TERMINATE_REQUEST:
        ppp_state_restart_recovery(intfc);

        send_ack = RNET_PPP_EVENT_RX_TERMINATE_REQUEST == in_event;

        break;

    // Timeout while negotiating means it's time to send another
    //   LCP/IPCP/IPV6CP request message
    case RNET_PPP_EVENT_TIMEOUT_NEGOTIATING:
        // 'completion_counter' is a sanity check to force
        //     restart of PPP if negotiations never complete.
        if (ppp_state_ptr->completion_counter > 0)
        {
            (ppp_state_ptr->completion_counter)--;

        #if RNET_ENABLE_PPP_TEST_MODE == 0
            rnet_intfc_timer_set(intfc,
                                 RNET_ID_PPP_TIMEOUT_NEGOTIATING,
                                 TIMEOUT_NEGOTIATING);
        #else
            if (RNET_INTFC_TEST1 == intfc)
            {
                rnet_intfc_timer_set(intfc,
                                     RNET_ID_PPP_TIMEOUT_NEGOTIATING,
                                     TIMEOUT_NEGOTIATING - 20);
            }
            else
            {
                rnet_intfc_timer_set(intfc,
                                     RNET_ID_PPP_TIMEOUT_NEGOTIATING,
                                     TIMEOUT_NEGOTIATING);
            }
        #endif

            // Time to send out next negotiation packet...
            // Which one shall we send?
            if (!lcp_closed)
            {
                if (!ppp_state_ptr->lcp_tx_closed)
                {
                    ppp_tx_lcp_config_req(intfc);
                }
                // 'else' omitted intentionally:
                //    waiting to close rx lcp
            }
            else if (!ipcp_closed)
            {
                if (!ppp_state_ptr->ipcp_tx_closed)
                {
                    ppp_tx_ipcp_config_req(intfc);
                }
                // 'else' omitted intentionally
            }
            else if (!ipv6cp_closed)
            {
                if (!ppp_state_ptr->ipv6cp_tx_closed)
                {
                    ppp_tx_ipv6cp_config_req(intfc);
                }
                // 'else' omitted intentionally
            }
            // 'else' omitted intentionally

        }
        // Negotiations timed out; kill negotiations, start over;
        // reset everything first.
        else
        {
            rnet_ppp_state_clear(intfc);

        #if RNET_ENABLE_PPP_TEST_MODE == 0
            rnet_intfc_timer_set(intfc,
                                 RNET_ID_PPP_TIMEOUT_RECOVERY,
                                 TIMEOUT_RECOVERY);
        #else
            if (RNET_INTFC_TEST1 == intfc)
            {
                rnet_intfc_timer_set(intfc,
                                     RNET_ID_PPP_TIMEOUT_RECOVERY,
                                     TIMEOUT_RECOVERY - 20);
            }
            else
            {
                rnet_intfc_timer_set(intfc,
                                     RNET_ID_PPP_TIMEOUT_RECOVERY,
                                     TIMEOUT_RECOVERY);
            }
        #endif

            ppp_state_ptr->state = RNET_PPP_STATE_RECOVERY;
        }
    
        break;

    case RNET_PPP_EVENT_RX_LCP_CONFIG_REQUEST:
        ppp_state_ptr->lcp_rx_closed = true;
        // Update variable
        lcp_closed = ppp_state_ptr->lcp_tx_closed;

        send_ack = true;

        break;

    case RNET_PPP_EVENT_RX_IPCP_CONFIG_REQUEST:
        ppp_state_ptr->ipcp_rx_closed = true;
        ipcp_closed = ppp_state_ptr->ipcp_tx_closed;

        send_ack = true;
    
        break;

    case RNET_PPP_EVENT_RX_IPV6CP_CONFIG_REQUEST:
        ppp_state_ptr->ipv6cp_rx_closed = true;
        ipv6cp_closed = ppp_state_ptr->ipv6cp_tx_closed;

        send_ack = true;

        break;

    case RNET_PPP_EVENT_RX_LCP_CONFIG_ACK:
        ppp_state_ptr->lcp_tx_closed = true;
        lcp_closed = ppp_state_ptr->lcp_rx_closed;
    
        break;

    case RNET_PPP_EVENT_RX_IPCP_CONFIG_ACK:
        ppp_state_ptr->ipcp_tx_closed = true;
        ipcp_closed = ppp_state_ptr->ipcp_rx_closed;
    
        break;

    case RNET_PPP_EVENT_RX_IPV6CP_CONFIG_ACK:
        ppp_state_ptr->ipv6cp_tx_closed = true;
        ipv6cp_closed = ppp_state_ptr->ipv6cp_rx_closed;
    
        break;

    default:
        break;
    }

    // Finished negotiating?
    if (lcp_closed && ipcp_closed && ipv6cp_closed)
    {
        rnet_intfc_timer_kill(intfc);

        // Notify stack that PPP came up
        rnet_msg_send_with_parm(RNET_ID_PPP_UP, intfc);

        ppp_state_ptr->state = RNET_PPP_STATE_UP;
        rnet_send_msgs_to_event_list(RNET_NOTIF_INTFC_UP, intfc);
    }

    return send_ack;
}

//!
//! @name      ppp_state_up
//!
//! @brief     Handles events when PPP state is up
//!
//! @param[in] 'intfc'-- interface
//! @param[in] 'in_event'-- state event
//!
//! @return    'true' to ack event
//!
static bool ppp_state_up(rnet_intfc_t intfc, rnet_ppp_event_t in_event)
{
    bool restart = false;
    bool send_ack = false;

    switch (in_event)
    {
    case RNET_PPP_EVENT_INIT:
    case RNET_PPP_EVENT_RX_LCP_CONFIG_REQUEST:
    case RNET_PPP_EVENT_RX_TERMINATE_REQUEST:
        restart = true;
        send_ack = RNET_PPP_EVENT_RX_TERMINATE_REQUEST == in_event;
        break;
    default:
        break;
    }

    // Do we need to tear down PPP and start over again?
    if (restart)
    {
        ppp_state_restart_recovery(intfc);

        // Notify stack that PPP was killed by peer
        rnet_msg_send_with_parm(RNET_ID_PPP_DOWN, intfc);

        // Notify apps that PPP went down
        rnet_send_msgs_to_event_list(RNET_NOTIF_INTFC_DOWN, intfc);
    }

    return send_ack;
}

//!
//! @name      ppp_state_restart_recovery
//!
//! @brief     Restart PPP recovery mode
//!
//! @param[in] 'intfc'-- interface
//!
static void ppp_state_restart_recovery(rnet_intfc_t intfc)
{
    rnet_intfc_ram_t       *intfc_ram_ptr;
    rnet_ppp_intfc_state_t *ppp_state_ptr;

    intfc_ram_ptr = rnet_intfc_get_ram(intfc);
    ppp_state_ptr = &(intfc_ram_ptr->l2_state.ppp);

    rnet_ppp_state_clear(intfc);

    ppp_state_ptr->completion_counter = RECOVERY_CYCLES;

    // Start timer so that state machine while recovery mode is hit
#if RNET_ENABLE_PPP_TEST_MODE == 0
    rnet_intfc_timer_set(intfc,
                         RNET_ID_PPP_TIMEOUT_RECOVERY,
                         TIMEOUT_RECOVERY);
#else
    if (RNET_INTFC_TEST1 == intfc)
    {
        rnet_intfc_timer_set(intfc,
                             RNET_ID_PPP_TIMEOUT_RECOVERY,
                             TIMEOUT_RECOVERY - 20);
    }
    else
    {
        rnet_intfc_timer_set(intfc,
                             RNET_ID_PPP_TIMEOUT_RECOVERY,
                             TIMEOUT_RECOVERY);
    }
#endif

    ppp_state_ptr->state = RNET_PPP_STATE_RECOVERY;
}

//!
//! @name      rnet_msg_rx_buf_ppp
//!
//! @brief     Entry point for all PPP frames received
//!
//! @param[in] 'buf'-- PPP frame, RNET buf format
//! @param[in]      'buf->header.offset' must point to PPP start in frame
//! @param[in]      'buf->header.length' must give length from offset
//!
void rnet_msg_rx_buf_ppp(rnet_buf_t *buf)
{
    uint8_t            *ptr;
    rnet_intfc_t        intfc;
    rnet_ppp_protocol_t protocol;
    bool                rv;
    unsigned            options;
    bool                ipv4_capable;
    bool                ipv6_capable;

    SL_REQUIRE(IS_RNET_BUF(buf));

    // 'ptr' points to beginning of frame
    ptr = RNET_BUF_FRAME_START_PTR(buf);

    intfc = (rnet_intfc_t)buf->header.intfc;

    rv = rx_ppp(ptr, ptr + buf->header.length, intfc, &protocol);

    if (!rv)
    {
        buf->header.code = RNET_BUF_CODE_PPP_HEADER_CORRUPTED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);

        return;
    }

    // Need to set for if/when packet is ack'ed
    buf->header.previous_ph = ppp_protocol_to_ph(protocol);

    // Remove PPP encapsulation from frame
    buf->header.offset += PPP_PREFIX_LENGTH;
    buf->header.length -= PPP_PREFIX_LENGTH;

    options = rnet_intfc_get_options(intfc);
    ipv4_capable = (options & RNET_IOPT_PPP_IPCP) != 0;
    ipv6_capable = (options & RNET_IOPT_PPP_IPV6CP) != 0;

    switch (protocol)
    {
    case RNET_PPP_PROTOCOL_LCP:
        rnet_msg_send(RNET_ID_RX_BUF_LCP, buf);
        break;

    case RNET_PPP_PROTOCOL_IPCP:
        if (ipv4_capable)
        {
            rnet_msg_send(RNET_ID_RX_BUF_IPV4CP, buf);
        }
        else
        {
            buf->header.code = RENT_BUF_CODE_PPP_IP_PROTOCOL_UNSUPPORTED;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        }
        break;

    case RNET_PPP_PROTOCOL_IPV6CP:
        if (ipv6_capable)
        {
            rnet_msg_send(RNET_ID_RX_BUF_IPV6CP, buf);
        }
        else
        {
            buf->header.code = RENT_BUF_CODE_PPP_IP_PROTOCOL_UNSUPPORTED;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        }
        break;

    case RNET_PPP_PROTOCOL_IPV4:
        if (ipv4_capable)
        {
            rnet_msg_send(RNET_ID_RX_BUF_IPV4, buf);
        }
        else
        {
            buf->header.code = RENT_BUF_CODE_PPP_IP_PROTOCOL_UNSUPPORTED;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        }
        break;

    case RNET_PPP_PROTOCOL_IPV6:
        if (ipv6_capable)
        {
            rnet_msg_send(RNET_ID_RX_BUF_IPV6, buf);
        }
        else
        {
            buf->header.code = RENT_BUF_CODE_PPP_IP_PROTOCOL_UNSUPPORTED;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        }
        break;

    default:
        buf->header.code = RENT_BUF_CODE_PPP_OTHER_PROTOCOL_UNSUPPORTED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        break;
    }
}

//!
//! @name      rnet_msg_rx_pcl_ppp
//!
//! @brief     Entry point for all PPP frames received
//!
//! @param[in] 'head_pcl'-- PPP frame, particle format
//! @param[in]       head PCL has header with offset and length values set
//! @param[in]      (see previous notes)
//!
void rnet_msg_rx_pcl_ppp(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t  *header;
    nsvc_pcl_chain_seek_t read_posit;
    uint8_t            *ptr;
    unsigned            remaining_in_pcl;
    rnet_intfc_t        intfc;
    rnet_ppp_protocol_t protocol;
    bool                rv;
    unsigned            options;
    bool                ipv4_capable;
    bool                ipv6_capable;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    // 'ptr' points to beginning of frame
    header = NSVC_PCL_HEADER(head_pcl);

    if (!nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                &read_posit,
                                                header->offset))
    {
        header->code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);

        return;
    }

    ptr = NSVC_PCL_SEEK_DATA_PTR(&read_posit);
    remaining_in_pcl = nsvc_pcl_chain_capacity(header->num_pcls, true);
    remaining_in_pcl -= header->offset + header->total_used_length;

    intfc = (rnet_intfc_t)header->intfc;

    // Assumes entire frame lies on head pcl
    rv = rx_ppp(ptr, ptr + remaining_in_pcl, intfc, &protocol);

    if (!rv)
    {
        header->code = RNET_BUF_CODE_PPP_HEADER_CORRUPTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);

        return;
    }

    // Need to set for if/when packet is ack'ed
    header->previous_ph = ppp_protocol_to_ph(protocol);

    // Remove PPP encapsulation from frame
    header->offset += PPP_PREFIX_LENGTH;
    header->total_used_length -= PPP_PREFIX_LENGTH;

    options = rnet_intfc_get_options(intfc);
    ipv4_capable = (options & RNET_IOPT_PPP_IPCP) != 0;
    ipv6_capable = (options & RNET_IOPT_PPP_IPV6CP) != 0;

    switch (protocol)
    {
    case RNET_PPP_PROTOCOL_LCP:
        rnet_msg_send(RNET_ID_RX_PCL_LCP, head_pcl);
        break;

    case RNET_PPP_PROTOCOL_IPCP:
        if (ipv4_capable)
        {
            rnet_msg_send(RNET_ID_RX_PCL_IPV4CP, head_pcl);
        }
        else
        {
            header->code = RENT_BUF_CODE_PPP_IP_PROTOCOL_UNSUPPORTED;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        }
        break;

    case RNET_PPP_PROTOCOL_IPV6CP:
        if (ipv6_capable)
        {
            rnet_msg_send(RNET_ID_RX_PCL_IPV6CP, head_pcl);
        }
        else
        {
            header->code = RENT_BUF_CODE_PPP_IP_PROTOCOL_UNSUPPORTED;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        }
        break;

    case RNET_PPP_PROTOCOL_IPV4:
        if (ipv4_capable)
        {
            rnet_msg_send(RNET_ID_RX_PCL_IPV4, head_pcl);
        }
        else
        {
            header->code = RENT_BUF_CODE_PPP_IP_PROTOCOL_UNSUPPORTED;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        }
        break;

    case RNET_PPP_PROTOCOL_IPV6:
        if (ipv6_capable)
        {
            rnet_msg_send(RNET_ID_RX_PCL_IPV6, head_pcl);
        }
        else
        {
            header->code = RENT_BUF_CODE_PPP_IP_PROTOCOL_UNSUPPORTED;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        }
        break;

    default:
        header->code = RENT_BUF_CODE_PPP_OTHER_PROTOCOL_UNSUPPORTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        break;
    }
}

//!
//! @name      rx_ppp
//!
//! @brief     Scans all PPP Rx frames. Does sanity checks.
//! @brief     Sets PPP protocol type
//!
//! @param[in] 'stream'-- start of where PPP packet is formed
//! @param[in] 'end_stream'-- bytes after last byte in PPP frame
//! @param[in] 'intfc'-- interface this frame came in on
//! @param[out] 'protocol_ptr'-- scanned PPP protocol value
//!
//! @return    'true' if sane
//!
static bool rx_ppp(uint8_t             *stream,
                   uint8_t             *end_stream,
                   rnet_intfc_t         intfc,
                   rnet_ppp_protocol_t *protocol_ptr)
{
    rnet_intfc_ram_t *intfc_ptr;
    uint8_t *ptr;
    bool acfc_ok;
    rnet_ppp_protocol_t protocol;
    rnet_xcp_code_t code;
    bool is_lcp;
    bool is_ipcp_ipv6cp;
    unsigned acfc;
    unsigned length;
    unsigned id;
    unsigned opt_type;
    unsigned opt_length;

    ptr = stream;

    // Strip FF03 prefix
    acfc = rutils_stream_to_word16(ptr);
    acfc_ok = PPP_ACFC == acfc;

    ptr += PPP_ACFC_LENGTH;

    if (!acfc_ok)
    {
        return false;
    }

    // Get PPP protocol
    protocol = rutils_stream_to_word16(ptr);
    ptr += PPP_PROTOCOL_VALUE_LENGTH;
    *protocol_ptr = protocol;

    // Sanity check protocol value
    if (!ppp_is_supported_protocol(protocol))
    {
        return false;
    }

    // Set flags for xCP protocol type
    is_lcp = RNET_PPP_PROTOCOL_LCP == protocol;
    is_ipcp_ipv6cp = (RNET_PPP_PROTOCOL_IPCP == protocol) ||
                     (RNET_PPP_PROTOCOL_IPV6CP == protocol);

    // Not doing an IPv4 or IPv6 packet? No more checks to do
    if (!(is_lcp || is_ipcp_ipv6cp))
    {
        return true;
    }

    // XCP-CODE value
    code = (rnet_xcp_code_t)*ptr++;

    if (!ppp_is_supported_xcp_code(code))
    {
        return false;
    }
    else if (is_ipcp_ipv6cp && !ppp_is_supported_ipcp_code(code))
    {
        return false;
    }

    // XCP-ID value
    id = *ptr++;

    intfc_ptr = rnet_intfc_get_ram(intfc);

    // Verify that ack ID is same as ID that we sent
    if (ppp_is_ack_code(code))
    {
        if (intfc_ptr->l2_state.ppp.tx_id != id)
        {
            return false;
        }
    }
    else
    {
        // Assumes peer closes ipv4 and ipv6 configs serially.
        intfc_ptr->l2_state.ppp.rx_id = id;
    }

    // XCP-LENGTH value
    length = rutils_stream_to_word16(ptr);
    ptr += XCP_LENGTH_LENGTH;

    // Sanity check XCP-LENGTH: minimum size according to protocol
    if (length < XCP_LENGTH_ADJUSTMENT)
    {
        return false;
    }

    // Adjust length to reflect XCP-PAYLOAD length only
    length -= XCP_LENGTH_ADJUSTMENT;

    // No content? Frame is sane, sanity checks pass
    if (0 == length)
    {
        return true;
    }
    // Sanity check 'length' to make sure we don't run over buffer
    // NOTE: if this fcn. called for a particle chain, caller must ensure
    //      we stay on first pcl
    else if (ptr + length > end_stream)
    {
        return false;
    }

    // Is this an xcp config request?
    if ((is_lcp || is_ipcp_ipv6cp) && (RNET_XCP_CONF_REQ == code))
    {
        // Sanity check formatting of config option list:
        //  -- can walk options
        //  -- sum of all option lengths == XCP length field
        while (length > 0)
        {
            // XCP-CONFIG-OPTION value
            opt_type = *ptr++;
            UNUSED(opt_type);                // suppress warning
            // XCP-OPTION-LENGTH value
            opt_length = *ptr++;

            // Pre-adjustment sanity check
            if (opt_length < XCP_OPTION_LENGTH_ADJUSTMENT)
            {
                return false;
            }

            // The only way out: if the last option matches remaining length
            if (opt_length == length)
            {
                return true;
            }

            length -= opt_length;
            ptr += opt_length - XCP_OPTION_LENGTH_ADJUSTMENT;
        }
    }

    return false;
}

//!
//! @name      rnet_msg_rx_buf_lcp
//!
//! @brief     Entry point for all PPP LCP frames
//!
//! @param[in] 'buf'-- PPP LCP frame, RNET buf format
//! @param[in]      'buf->header.offset' must point to LCP code in frame
//! @param[in]      'buf->header.length' must give length from offset
//!
void rnet_msg_rx_buf_lcp(rnet_buf_t *buf)
{
    rnet_xcp_code_t      code;
    unsigned             id;
    uint8_t             *ptr;
    uint8_t             *start_ptr;
    rnet_intfc_t         intfc;
    uint16_t             length;
    bool                 send_ack;
    rnet_ppp_counters_t *counters;

    SL_REQUIRE(IS_RNET_BUF(buf));

    // 'ptr' points to beginning of LCP code
    ptr = RNET_BUF_FRAME_START_PTR(buf);
    // for reference
    start_ptr = ptr;

    intfc = (rnet_intfc_t)buf->header.intfc;

    // bump counter(s)
    // 'void **' cast to supress compiler warning (hate doing it!)
    (void)rnet_intfc_get_counters(intfc, (void **)&counters);
    counters->lcp_rx++;

    // scan XCP-CODE, XCP-ID, XCP-LENGTH
    code = (rnet_xcp_code_t)*ptr++;
    id = *ptr++;
    UNUSED(id);       //suppress warning

    length = rutils_stream_to_word16(ptr);
    ptr += XCP_LENGTH_LENGTH;
    length -= XCP_LENGTH_ADJUSTMENT;

    switch (code)
    {
    case RNET_XCP_CONF_REQ:
        send_ack = rnet_ppp_state_machine(intfc,
                            RNET_PPP_EVENT_RX_LCP_CONFIG_REQUEST);

        if (send_ack)
        {
            // Turn packet around
            *start_ptr = RNET_XCP_CONF_ACK;
            rnet_msg_send(RNET_ID_TX_BUF_PPP, buf);
        }
        else
        {
            buf->header.code = RENT_BUF_CODE_PPP_XCP_PARSE_ERROR;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        }

        break;

    case RNET_XCP_CONF_ACK:
        (void)rnet_ppp_state_machine(intfc, RNET_PPP_EVENT_RX_LCP_CONFIG_ACK);

        // Quietly discard packet
        rnet_free_buf(buf);

        break;

    case RNET_XCP_ECHO_REQ:
        // Turn packet around
        *start_ptr = RNET_XCP_ECHO_ACK;
        rnet_msg_send(RNET_ID_TX_BUF_PPP, buf);

        break;

    case RNET_XCP_TERM_REQ:
        send_ack = rnet_ppp_state_machine(intfc,
                          RNET_PPP_EVENT_RX_TERMINATE_REQUEST);

        if (send_ack)
        {
            // Turn packet around
            *start_ptr = RNET_XCP_TERM_ACK;
            rnet_msg_send(RNET_ID_TX_BUF_PPP, buf);
        }
        else
        {
            buf->header.code = RENT_BUF_CODE_PPP_XCP_PARSE_ERROR;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        }

        break;

    case RNET_XCP_TERM_ACK:
        (void)rnet_ppp_state_machine(intfc, RNET_PPP_EVENT_RX_TERMINATE_ACK);

        // Quietly discard packet
        rnet_free_buf(buf);

        break;

    // currently not supported
    case RNET_XCP_CONF_NAK:
    case RNET_XCP_CONF_REJ:
    case RNET_XCP_PROT_REJ:
    default:
        buf->header.code = RENT_BUF_CODE_PPP_XCP_CODE_UNSUPPORTED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
    }
}

//!
//! @name      rnet_msg_rx_pcl_lcp
//!
//! @brief     Entry point for all PPP LCP frames received
//!
//! @param[in] 'head_pcl'-- PPP frame, particle format
//! @param[in]       head PCL has header with offset and length values set
//! @param[in]      (see previous notes)
//!
void rnet_msg_rx_pcl_lcp(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t   *header;
    nsvc_pcl_chain_seek_t read_posit;
    rnet_xcp_code_t      code;
    unsigned             id;
    uint8_t             *ptr;
    uint8_t             *start_ptr;
    rnet_intfc_t         intfc;
    uint16_t             length;
    bool                 send_ack;
    rnet_ppp_counters_t *counters;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    header = NSVC_PCL_HEADER(head_pcl);

    if (!nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                &read_posit,
                                                header->offset))
    {
        header->code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);

        return;
    }

    ptr = NSVC_PCL_SEEK_DATA_PTR(&read_posit);
    start_ptr = ptr;

    intfc = (rnet_intfc_t)header->intfc;

    // bump counter(s)
    // 'void **' cast to supress compiler warning (hate doing it!)
    (void)rnet_intfc_get_counters(intfc, (void **)&counters);
    counters->lcp_rx++;

    // scan XCP-CODE, XCP-ID, XCP-LENGTH
    code = (rnet_xcp_code_t)*ptr++;
    id = *ptr++;
    UNUSED(id);       // suppress warning

    length = rutils_stream_to_word16(ptr);
    ptr += XCP_LENGTH_LENGTH;
    length -= XCP_LENGTH_ADJUSTMENT;

    switch (code)
    {
    case RNET_XCP_CONF_REQ:
        send_ack = rnet_ppp_state_machine(intfc,
                          RNET_PPP_EVENT_RX_LCP_CONFIG_REQUEST);

        if (send_ack)
        {
            // Turn packet around
            *start_ptr = RNET_XCP_CONF_ACK;
            rnet_msg_send(RNET_ID_TX_PCL_PPP, head_pcl);
        }
        else
        {
            header->code = RENT_BUF_CODE_PPP_XCP_PARSE_ERROR;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        }

        break;

    case RNET_XCP_CONF_ACK:
        (void)rnet_ppp_state_machine(intfc, RNET_PPP_EVENT_RX_LCP_CONFIG_ACK);

        // Quietly discard packet
        nsvc_pcl_free_chain(head_pcl);

        break;

    case RNET_XCP_ECHO_REQ:
        // Turn packet around
        *start_ptr = RNET_XCP_ECHO_ACK;
        rnet_msg_send(RNET_ID_TX_PCL_PPP, head_pcl);

        break;

    case RNET_XCP_TERM_REQ:
        send_ack = rnet_ppp_state_machine(intfc,
                          RNET_PPP_EVENT_RX_TERMINATE_REQUEST);

        if (send_ack)
        {
            // Turn packet around
            *start_ptr = RNET_XCP_TERM_ACK;
            rnet_msg_send(RNET_ID_TX_PCL_PPP, head_pcl);
        }
        else
        {
            header->code = RENT_BUF_CODE_PPP_XCP_PARSE_ERROR;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        }

        break;

    case RNET_XCP_TERM_ACK:
        (void)rnet_ppp_state_machine(intfc, RNET_PPP_EVENT_RX_TERMINATE_ACK);

        // Quietly discard packet
        nsvc_pcl_free_chain(head_pcl);

        break;

    case RNET_XCP_CONF_NAK:
    case RNET_XCP_CONF_REJ:
    case RNET_XCP_PROT_REJ:
    default:
        header->code = RENT_BUF_CODE_PPP_XCP_CODE_UNSUPPORTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
    }
}

//!
//! @name      rnet_msg_rx_buf_ipcp
//!
//! @brief     Entry point for all PPP IPCP frames
//!
//! @param[in] 'buf'-- PPP IPCCP frame, RNET buf format
//! @param[in]      'buf->header.offset' must point to IPCP code in frame
//! @param[in]      'buf->header.length' must give length from offset
//!
void rnet_msg_rx_buf_ipcp(rnet_buf_t *buf)
{
    rnet_xcp_code_t      code;
    unsigned             id;
    uint8_t             *ptr;
    uint8_t             *start_ptr;
    rnet_intfc_t         intfc;
    uint16_t             length;
    bool                 send_ack;
    rnet_ppp_counters_t *counters;

    SL_REQUIRE(IS_RNET_BUF(buf));

    // 'ptr' points to beginning of LCP code
    ptr = RNET_BUF_FRAME_START_PTR(buf);
    // for reference
    start_ptr = ptr;

    intfc = (rnet_intfc_t)buf->header.intfc;

    // bump counter(s)
    // 'void **' cast to supress compiler warning (hate doing it!)
    (void)rnet_intfc_get_counters(intfc, (void **)&counters);
    counters->ipcp_rx++;

    // scan XCP-CODE, XCP-ID, XCP-LENGTH
    code = (rnet_xcp_code_t)*ptr++;
    id = *ptr++;
    UNUSED(id);         // suppress warning

    length = rutils_stream_to_word16(ptr);
    ptr += XCP_LENGTH_LENGTH;
    length -= XCP_LENGTH_ADJUSTMENT;

    switch (code)
    {
    case RNET_XCP_CONF_REQ:
        send_ack = rnet_ppp_state_machine(intfc,
                            RNET_PPP_EVENT_RX_IPCP_CONFIG_REQUEST);

        if (send_ack)
        {
            // Turn packet around
            *start_ptr = RNET_XCP_CONF_ACK;
            rnet_msg_send(RNET_ID_TX_BUF_PPP, buf);
        }
        else
        {
            buf->header.code = RENT_BUF_CODE_PPP_XCP_PARSE_ERROR;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        }

        break;

    case RNET_XCP_CONF_ACK:
        (void)rnet_ppp_state_machine(intfc, RNET_PPP_EVENT_RX_IPCP_CONFIG_ACK);

        // Quietly discard packet
        rnet_free_buf(buf);

        break;

    default:
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
    }
}

//!
//! @name      rnet_msg_rx_pcl_ipcp
//!
//! @brief     Entry point for all PPP IPCP frames received
//!
//! @param[in] 'head_pcl'-- PPP frame, particle format
//! @param[in]       head PCL has header with offset and length values set
//! @param[in]      (see previous notes)
//!
void rnet_msg_rx_pcl_ipcp(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t   *header;
    nsvc_pcl_chain_seek_t read_posit;
    rnet_xcp_code_t      code;
    unsigned             id;
    uint8_t             *ptr;
    uint8_t             *start_ptr;
    rnet_intfc_t         intfc;
    uint16_t             length;
    bool                 send_ack;
    rnet_ppp_counters_t *counters;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    header = NSVC_PCL_HEADER(head_pcl);

    if (!nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                &read_posit,
                                                header->offset))
    {
        header->code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);

        return;
    }

    ptr = NSVC_PCL_SEEK_DATA_PTR(&read_posit);
    start_ptr = ptr;

    intfc = (rnet_intfc_t)header->intfc;

    // bump counter(s)
    // 'void **' cast to supress compiler warning (hate doing it!)
    (void)rnet_intfc_get_counters(intfc, (void **)&counters);
    counters->lcp_rx++;

    // scan XCP-CODE, XCP-ID, XCP-LENGTH
    code = (rnet_xcp_code_t)*ptr++;
    id = *ptr++;
    (void)id;       // suppress warning

    length = rutils_stream_to_word16(ptr);
    ptr += XCP_LENGTH_LENGTH;
    length -= XCP_LENGTH_ADJUSTMENT;

    switch (code)
    {
    case RNET_XCP_CONF_REQ:
        send_ack = rnet_ppp_state_machine(intfc,
                          RNET_PPP_EVENT_RX_IPCP_CONFIG_REQUEST);

        if (send_ack)
        {
            // Turn packet around
            *start_ptr = RNET_XCP_CONF_ACK;
            rnet_msg_send(RNET_ID_TX_PCL_PPP, head_pcl);
        }
        else
        {
            header->code = RENT_BUF_CODE_PPP_XCP_PARSE_ERROR;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        }

        break;

    case RNET_XCP_CONF_ACK:
        (void)rnet_ppp_state_machine(intfc, RNET_PPP_EVENT_RX_IPCP_CONFIG_ACK);

        // Quietly discard packet
        nsvc_pcl_free_chain(head_pcl);

        break;

    default:
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
    }
}

//!
//! @name      rnet_msg_rx_buf_ipv7cp
//!
//! @brief     Entry point for all PPP IPV6CP frames
//!
//! @param[in] 'buf'-- PPP IPCCP frame, RNET buf format
//! @param[in]      'buf->header.offset' must point to IPV6CP code in frame
//! @param[in]      'buf->header.length' must give length from offset
//!
void rnet_msg_rx_buf_ipv6cp(rnet_buf_t *buf)
{
    rnet_xcp_code_t      code;
    unsigned             id;
    uint8_t             *ptr;
    uint8_t             *start_ptr;
    rnet_intfc_t         intfc;
    uint16_t             length;
    bool                 send_ack;
    rnet_ppp_counters_t *counters;

    SL_REQUIRE(IS_RNET_BUF(buf));

    // 'ptr' points to beginning of LCP code
    ptr = RNET_BUF_FRAME_START_PTR(buf);
    // for reference
    start_ptr = ptr;

    intfc = (rnet_intfc_t)buf->header.intfc;

    // bump counter(s)
    // 'void **' cast to supress compiler warning (hate doing it!)
    (void)rnet_intfc_get_counters(intfc, (void **)&counters);
    counters->ipcp_rx++;

    // scan XCP-CODE, XCP-ID, XCP-LENGTH
    code = (rnet_xcp_code_t)*ptr++;
    id = *ptr++;
    UNUSED(id);      //suppress warning

    length = rutils_stream_to_word16(ptr);
    ptr += XCP_LENGTH_LENGTH;
    length -= XCP_LENGTH_ADJUSTMENT;

    switch (code)
    {
    case RNET_XCP_CONF_REQ:
        send_ack = rnet_ppp_state_machine(intfc,
                            RNET_PPP_EVENT_RX_IPV6CP_CONFIG_REQUEST);

        if (send_ack)
        {
            // Turn packet around
            *start_ptr = RNET_XCP_CONF_ACK;
            rnet_msg_send(RNET_ID_TX_BUF_PPP, buf);
        }
        else
        {
            buf->header.code = RENT_BUF_CODE_PPP_XCP_PARSE_ERROR;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        }

        break;

    case RNET_XCP_CONF_ACK:
        (void)rnet_ppp_state_machine(intfc,
                      RNET_PPP_EVENT_RX_IPV6CP_CONFIG_ACK);

        // Quietly discard packet
        rnet_free_buf(buf);

        break;

    default:
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
    }
}

//!
//! @name      rnet_msg_rx_pcl_ipv6cp
//!
//! @brief     Entry point for all PPP IPV6CP frames received
//!
//! @param[in] 'head_pcl'-- PPP frame, particle format
//! @param[in]       head PCL has header with offset and length values set
//! @param[in]      (see previous notes)
//!
void rnet_msg_rx_pcl_ipv6cp(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t   *header;
    nsvc_pcl_chain_seek_t read_posit;
    rnet_xcp_code_t      code;
    unsigned             id;
    uint8_t             *ptr;
    uint8_t             *start_ptr;
    rnet_intfc_t         intfc;
    uint16_t             length;
    bool                 send_ack;
    rnet_ppp_counters_t *counters;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    header = NSVC_PCL_HEADER(head_pcl);
    if (!nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                &read_posit,
                                                header->offset))
    {
        header->code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);

        return;
    }

    ptr = NSVC_PCL_SEEK_DATA_PTR(&read_posit);
    start_ptr = ptr;

    intfc = (rnet_intfc_t)header->intfc;

    // bump counter(s)
    // 'void **' cast to supress compiler warning (hate doing it!)
    (void)rnet_intfc_get_counters(intfc, (void **)&counters);
    counters->lcp_rx++;

    // scan XCP-CODE, XCP-ID, XCP-LENGTH
    code = (rnet_xcp_code_t)*ptr++;
    id = *ptr++;
    (void)id;      //suppress warning

    length = rutils_stream_to_word16(ptr);
    ptr += XCP_LENGTH_LENGTH;
    length -= XCP_LENGTH_ADJUSTMENT;

    switch (code)
    {
    case RNET_XCP_CONF_REQ:
        send_ack = rnet_ppp_state_machine(intfc,
                          RNET_PPP_EVENT_RX_IPV6CP_CONFIG_REQUEST);

        if (send_ack)
        {
            // Turn packet around
            *start_ptr = RNET_XCP_CONF_ACK;
            rnet_msg_send(RNET_ID_TX_PCL_PPP, head_pcl);
        }
        else
        {
            header->code = RENT_BUF_CODE_PPP_XCP_PARSE_ERROR;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        }

        break;

    case RNET_XCP_CONF_ACK:
        (void)rnet_ppp_state_machine(intfc,
                              RNET_PPP_EVENT_RX_IPV6CP_CONFIG_ACK);

        // Quietly discard packet
        nsvc_pcl_free_chain(head_pcl);

        break;

    default:
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
    }
}

//!
//! @name      rnet_msg_tx_buf_ppp
//!
//! @brief     Transmit a PPP frame
//!
//! @details   Message handler for any packet that needs to be
//! @details   transmitted by PPP. This fcn. will:
//! @details   -- add the ACFC and PPP protocol fields (4 bytes)
//! @details      (NOTE: packet header must have previous layer
//! @details        protocol value set)
//! @details   -- send packet the AHDLC, then out interface
//!
//! @param[in] 'buf'-- PPP LCP frame, RNET buf format
//! @param[in]      'buf->header.offset' must point to PPP payload
//! @param[in]          Offset must be large enough to all pre-pending
//! @param[in]          of ACFC and PPP protocol values (4 bytes)
//! @param[in]      'buf->header.length' must be length of PPP payload
//!
void rnet_msg_tx_buf_ppp(rnet_buf_t *buf)
{
    rnet_ppp_protocol_t protocol;
    uint8_t            *offset_ptr;
    uint8_t            *start_ptr;

    SL_REQUIRE(IS_RNET_BUF(buf));

    protocol = ppp_ph_to_ppp_protocol(buf->header.previous_ph);

    offset_ptr = &buf->buf[buf->header.offset];
    start_ptr = offset_ptr - PPP_PREFIX_LENGTH;

    SL_REQUIRE(buf->header.offset >= PPP_PREFIX_LENGTH);

    buf->header.offset -= PPP_PREFIX_LENGTH;
    buf->header.length += PPP_PREFIX_LENGTH;

    ppp_tx_add_ppp_wrapper(start_ptr, protocol);

    rnet_msg_send(RNET_ID_TX_BUF_AHDLC_CRC, buf);
}

//!
//! @name      rnet_msg_tx_buf_ppp
//!
//! @brief     Transmit a PPP frame
//!
//! @details   (See above)
//!
//! @param[in] 'head_pcl'-- PPP LCP frame, particle format
//! @param[in]      header->offset' must point to PPP payload
//! @param[in]          Offset must be large enough to all pre-pending
//! @param[in]          of ACFC and PPP protocol values (4 bytes)
//! @param[in]      'buf->header.length' must be length of PPP payload
//!
void rnet_msg_tx_pcl_ppp(nsvc_pcl_t *head_pcl)
{
    rnet_ppp_protocol_t protocol;
    nsvc_pcl_header_t  *header;
    uint8_t            *offset_ptr;
    uint8_t            *start_ptr;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    header = NSVC_PCL_HEADER(head_pcl);

    protocol = ppp_ph_to_ppp_protocol(header->previous_ph);

    offset_ptr = &(head_pcl->buffer)[header->offset];
    start_ptr = offset_ptr - PPP_PREFIX_LENGTH;

    SL_REQUIRE(header->offset >= PPP_PREFIX_LENGTH);

    header->offset -= PPP_PREFIX_LENGTH;
    header->total_used_length += PPP_PREFIX_LENGTH;

    ppp_tx_add_ppp_wrapper(start_ptr, protocol);

    rnet_msg_send(RNET_ID_TX_PCL_AHDLC_CRC, head_pcl);
}

//!
//! @name      ppp_tx_lcp_config_req
//!
//! @brief     Transmit a PPP LCP Config Request frame
//!
//! @details   Allocates the buffer internally, builds, sends frame.
//!
//! @param[in] 'intfc'-- interface to send frame out of
//!
static void ppp_tx_lcp_config_req(rnet_intfc_t intfc)
{
    const uint8_t config_options_string[] = {
        RNET_LCP_TYPE_MAGIC_NUMBER,         // LCP magic number
        4 + XCP_OPTION_LENGTH_ADJUSTMENT,   // '4' is num. data bytes below
        0x11, 0x11, 0x11, 0x11              // value
    };

    rnet_ppp_counters_t *counters;

    // 'void **' cast to supress compiler warning (hate doing it!)
    (void)rnet_intfc_get_counters(intfc, (void **)&counters);
    counters->lcp_tx++;

    ppp_tx_xcp_request(intfc,
                       RNET_XCP_CONF_REQ,
                       RNET_PPP_PROTOCOL_LCP,
                       config_options_string,
                       sizeof(config_options_string));
}

//!
//! @name      ppp_tx_ipcp_config_req
//!
//! @brief     Transmit a PPP IPCP Config Request frame
//!
//! @details   Allocates the buffer internally, builds, sends frame.
//!
//! @param[in] 'intfc'-- interface to send frame out of
//!
static void ppp_tx_ipcp_config_req(rnet_intfc_t intfc)
{
    rnet_ppp_counters_t *counters;

    // 'void **' cast to supress compiler warning (hate doing it!)
    (void)rnet_intfc_get_counters(intfc, (void **)&counters);
    counters->ipcp_tx++;

    ppp_tx_xcp_request(intfc,
                       RNET_XCP_CONF_REQ,
                       RNET_PPP_PROTOCOL_IPCP,
                       NULL,
                       0);
}

//!
//! @name      ppp_tx_ipv6cp_config_req
//!
//! @brief     Transmit a PPP IPV6CP Config Request frame
//!
//! @details   Allocates the buffer internally, builds, sends frame.
//!
//! @param[in] 'intfc'-- interface to send frame out of
//!
static void ppp_tx_ipv6cp_config_req(rnet_intfc_t intfc)
{
    rnet_ppp_counters_t *counters;

    // 'void **' cast to supress compiler warning (hate doing it!)
    (void)rnet_intfc_get_counters(intfc, (void **)&counters);
    counters->ipv6cp_tx++;

    ppp_tx_xcp_request(intfc,
                       RNET_XCP_CONF_REQ,
                       RNET_PPP_PROTOCOL_IPV6CP,
                       NULL,
                       0);
}

//!
//! @name      ppp_tx_lcp_term_req
//!
//! @brief     Transmit a PPP LCP Terminate Request frame
//!
//! @details   Allocates the buffer internally, builds, sends frame.
//!
//! @param[in] 'intfc'-- interface to send frame out of
//!
static void ppp_tx_lcp_term_req(rnet_intfc_t intfc)
{
    rnet_ppp_counters_t *counters;

    // 'void **' cast to supress compiler warning (hate doing it!)
    (void)rnet_intfc_get_counters(intfc, (void **)&counters);
    counters->lcp_tx++;

    ppp_tx_xcp_request(intfc,
                       RNET_XCP_TERM_REQ,
                       RNET_PPP_PROTOCOL_LCP,
                       NULL,
                       0);
}

//!
//! @name      ppp_tx_xcp_request
//!
//! @brief     Sends a LCP/IPCP/IPV6CP config, etc. request packet
//! @brief     field value.
//!
//! @details   Internally chooses RNET buffer or particle as buffer
//! @details   format, depending on compile switch setting.
//!
//! @param[in] 'intfc'-- Interface to send frame out
//! @param[in] 'code'-- LCP/IPCP/IPV6CP code byte
//! @param[in] 'protocol'-- PPP protocol type
//! @param[in] 'data_string'-- payload of xCP config frame
//! @param[in]          gets copied into tx frame
//! @param[in]          If == NULL, ignore
//! @param[in] 'data_string_length'-- 
//!
static void ppp_tx_xcp_request(rnet_intfc_t        intfc,
                               rnet_xcp_code_t     code,
                               rnet_ppp_protocol_t protocol,
                               const uint8_t      *data_string,
                               unsigned            data_string_length)
{
    uint8_t    *offset_ptr;
    uint8_t    *start_ptr;

#if RNET_CS_USING_BUFS_FOR_TX == 1
    rnet_buf_t *buf;

    buf = ppp_tx_buf_alloc(intfc);

    buf->header.intfc = intfc;
    // '.previous_ph' value is only way PPP knows how to set PPP protocol
    buf->header.previous_ph = ppp_protocol_to_ph(protocol);

    offset_ptr = &(buf->buf)[buf->header.offset];
    start_ptr = offset_ptr - XCP_LENGTH_ADJUSTMENT;

    if (NULL != data_string)
    {
        rutils_memcpy(offset_ptr, data_string, data_string_length);
        buf->header.length += data_string_length;
    }

    // Pre-adjust/expand frame to give space for xCP code, request ID,
    //  and 2-byte length fields.
    // Make fcn. call to populate these values
    buf->header.offset -= XCP_LENGTH_ADJUSTMENT;
    buf->header.length += XCP_LENGTH_ADJUSTMENT;
    ppp_tx_add_code_id_length_wrapper(start_ptr, intfc, code, 0);

    // Send out interface...but still needs PPP encapsulation
    rnet_msg_send(RNET_ID_TX_BUF_PPP, buf);

#elif RNET_CS_USING_PCLS_FOR_TX == 1
    nsvc_pcl_t         *head_pcl;
    nsvc_pcl_header_t  *header;

    head_pcl = ppp_tx_pcl_alloc(intfc);

    header = NSVC_PCL_HEADER(head_pcl);

    header->intfc = intfc;
    header->previous_ph = ppp_protocol_to_ph(protocol);

    // Pre-pending at offset cannot underrun
    SL_REQUIRE(header->offset >= PPP_PREFIX_LENGTH + sizeof(nsvc_pcl_header_t));

    offset_ptr = &head_pcl->buffer[header->offset];
    start_ptr = offset_ptr - XCP_LENGTH_ADJUSTMENT;

    if (NULL != data_string)
    {
        // Data must not overrun this pcl
        SL_REQUIRE(header->offset + data_string_length < NSVC_PCL_SIZE);

        rutils_memcpy(offset_ptr, data_string, data_string_length);
        header->total_used_length += data_string_length;
    }

    header->offset -= XCP_LENGTH_ADJUSTMENT;
    header->total_used_length += XCP_LENGTH_ADJUSTMENT;
    ppp_tx_add_code_id_length_wrapper(start_ptr, intfc, code, 0);

    rnet_msg_send(RNET_ID_TX_PCL_AHDLC_CRC, head_pcl);

#else
    #error "Missing compile switch, either RNET_CS_USING_BUFS_FOR_TX or RNET_CS_USING_PCLS_FOR_TX");
#endif //RNET_CS_BUFS_FOR_TX
}

//!
//! @name      ppp_tx_add_code_id_length_wrapper
//!
//! @brief     Encode xCP code byte, ID byte, 2-byte data length values
//!
//! @param[in] 'buffer'-- data outputted here
//! @param[in] 'intfc'-- Interface to send frame out
//! @param[in] 'code'-- LCP/IPCP/IPV6CP code byte
//! @param[in] 'data_length'-- length of data payload MINUS
//! @param[in]      4-byte adjustment
//!
static void ppp_tx_add_code_id_length_wrapper(uint8_t         *buffer,
                                              rnet_intfc_t     intfc,
                                              rnet_xcp_code_t  code,
                                              unsigned         data_length)
{
    rnet_intfc_ram_t *ram_intfc_ptr;
    uint8_t           tx_id;

    // XCP-CODE
    *buffer++ = code;

    // XCP-ID
    // There is a storage value for last ID generated. Increment it,
    // save it, use it as this packet's ID.
    ram_intfc_ptr = rnet_intfc_get_ram(intfc);
    tx_id = ++(ram_intfc_ptr->l2_state.ppp.tx_id);
    *buffer++ = tx_id;

    // XCP-LENGTH
    // Add to length field sum of lengths of XCP-CODE, XCP-ID, XCP-LENGTH
    rutils_word16_to_stream(buffer, data_length + XCP_LENGTH_ADJUSTMENT);
    //buffer += XCP_LENGTH_LENGTH;
}

//!
//! @name      ppp_tx_add_ppp_wrapper
//!
//! @brief     Encode PPP Prefix (ACFC, PPP Protocol) to any outgoing PPP frame
//!
//! @param[in] 'buffer'-- data outputted here
//! @param[in] 'protocol'-- PPP protocol
//!
static void ppp_tx_add_ppp_wrapper(uint8_t            *buffer,
                                   rnet_ppp_protocol_t protocol)
{
    rutils_word16_to_stream(buffer, PPP_ACFC);
    buffer += PPP_ACFC_LENGTH;
    
    rutils_word16_to_stream(buffer, protocol);
    //buffer += PPP_PROTOCOL_VALUE_LENGTH;
}

#if RNET_CS_USING_BUFS_FOR_TX == 1
//!
//! @name      ppp_tx_buf_alloc
//!
//! @brief     Allocate an RNET buffer, to be used for a PPP frame
//!
//! @details   The frame is initialized to an offset which allows
//! @details   enough room to pre-pend a PPP header.
//!
//! @param[in] 'intfc'-- interface ID to embed in buffer header
//!
static rnet_buf_t *ppp_tx_buf_alloc(rnet_intfc_t intfc)
{
    rnet_buf_t *buf;

    // If a buffer isn't available now, fail call
    buf = rnet_alloc_bufT(0);

    if (NULL != buf)
    {
        buf->header.offset = TX_PPP_PROTOCOL_OFFSET;
        // buf->header.length will default to zero
        buf->header.intfc = intfc;

        return buf;
    }

    return NULL;
}

#elif RNET_CS_USING_PCLS_FOR_TX == 1
//!
//! @name      ppp_tx_pcl_alloc
//!
//! @brief     Allocate a 1-particle chain, to be used for a PPP frame
//!
//! @details   The frame is initialized to an offset which allows
//! @details   enough room to pre-pend a PPP header.
//!
//! @param[in] 'intfc'-- interface ID to embed in buffer header
//!
static nsvc_pcl_t *ppp_tx_pcl_alloc(rnet_intfc_t intfc)
{
    nsvc_pcl_t        *head_pcl;
    nsvc_pcl_header_t *header;

    // If a pcl isn't available now, fail call
    head_pcl = rnet_alloc_pclT(0);

    if (NULL != head_pcl)
    {
        header = NSVC_PCL_HEADER(head_pcl);
        header->offset = NSVC_PCL_OFFSET_PAST_HEADER(TX_PPP_PROTOCOL_OFFSET);
        header->intfc = intfc;

        return head_pcl;
    }

    return NULL;
}
#else
    #error "Neither buf nor pcl!"
#endif  // RNET_CS_USING_BUFS_FOR_TX == 1