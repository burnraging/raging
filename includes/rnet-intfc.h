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

//! @file     rnet-intfc.h
//! @authors  Bernie Woodland
//! @date     10Mar18
//!
//! @brief    Interface Definitions
//!

#ifndef RNET_INTFC_H
#define RNET_INTFC_H

#include "raging-global.h"

#include "rnet-compile-switches.h"
#include "rnet-ip.h"
#include "rnet-app.h"
#include "rnet-buf.h"
#include "rnet-ppp.h"

//!
//! @enum      rnet_l2_t
//!
//! @brief     L2 protocol type
//!
typedef enum
{
    RNET_L2_PPP,
} rnet_l2_t;

//!
//! @enum      rnet_ipacq_t
//!
//! @brief     Method for acquiring IP address
//!
typedef enum
{
    RNET_IPACQ_HARD_CODED,      //prefix or suffix is hard-coded
    RNET_IPACQ_EUI64_DERIVED,   //suffix derived from MAC addr
    RNET_IPACQ_LEARNED,         //learned from 1st rx packet
} rnet_ipacq_t;

//!
//! @enum      rnet_notif_t
//!
//! @brief     Notification list specifier
//!
//! @details   RNET_NOTIF_INIT_COMPLETE
//! @details       RNET stack initialization occured.
//! @details       Parameter: Takes no parameter.
//! @details   
//! @details   RNET_NOTIF_INTFC_UP
//! @details       The L2 protocol came up/the intfc is up.
//! @details       Parameter: interface (rnet_intfc_t)
//! @details   
//! @details   RNET_NOTIF_INTFC_DOWN
//! @details       The L2 protocol went down/the intfc is down.
//! @details       Parameter: interface (rnet_intfc_t)
//! @details   
//!
// (bit of a hack...should be in a better .h file...)
typedef enum
{
    RNET_NOTIF_null = 0,
    RNET_NOTIF_INIT_COMPLETE,
    RNET_NOTIF_INTFC_UP,
    RNET_NOTIF_INTFC_DOWN,
    RNET_NOTIF_max
} rnet_notif_t;

#define RNET_NUM_NOTIFS         (RNET_NOTIF_max - 1)

//!
//! @enum      rnet_ppp_counters_t
//!
//! @brief     Counters for interface of type 'RNET_L2_PPP'
//!
typedef struct
{
    uint16_t              lcp_rx;
    uint16_t              lcp_tx;
    uint16_t              lcp_terminate_rx;
    uint16_t              lcp_terminate_tx;
    uint16_t              lcp_prot_rej_rx;
    uint16_t              ppp_rx_unknown;
    uint16_t              ipcp_rx;
    uint16_t              ipcp_tx;
    uint16_t              ipv6cp_rx;
    uint16_t              ipv6cp_tx;
    uint16_t              ipv4_rx;
    uint16_t              ipv4_tx;
    uint16_t              ipv6_rx;
    uint16_t              ipv6_tx;
} rnet_ppp_counters_t;

//!
//! @brief    'rnet_intfc_rom_t' 'option_flags' values
//!

//! @name      RNET_IOPT_RX_AHDLC_PRE_TRANSLATED
//! @brief     AHDLC Control Characters translated+removed in IRQ/
//! @brief       outside of RNET.
#define RNET_IOPT_RX_AHDLC_PRE_TRANSLATED                       0x0001

//! @name      RNET_IOPT_RX_AHDLC_PRE_CRC_VERIFIED
//! @brief     AHDLC CRC verified and removed in IRQ
#define RNET_IOPT_RX_AHDLC_PRE_CRC_VERIFIED                     0x0002

//! @name      RNET_IOPT_OMIT_TX_AHDLC_TRANSLATION
//! @brief     AHDLC Control Characters will be added by IRQ,
//! @brief     so don't add them in RNET.
#define RNET_IOPT_OMIT_TX_AHDLC_TRANSLATION                     0x0004

//! @name      RNET_IOPT_OMIT_TX_AHDLC_CRC_APPEND
//! @brief     AHDLC CRC16 will be added by IRQ,
//! @brief     so don't add it in RNET.
//  NOTE: not used, assumed always done!!!
//#define RNET_IOPT_OMIT_TX_AHDLC_CRC_APPEND                      0x0008

//! @name      RNET_IOPT_PPP_IPCP
//! @brief     Require IPCP protocol during PPP negotiations
#define RNET_IOPT_PPP_IPCP                                      0x0010

//! @name      RNET_IOPT_PPP_IPV6CP
//! @brief     Require IPV6CP protocol during PPP negotiations
#define RNET_IOPT_PPP_IPV6CP                                    0x0020

//!
//! @name      rnet_tx_api_t
//!
//! @brief     Function ptr type for packet tx fcn prototypes
//!
typedef void (*rnet_tx_api_t)(rnet_intfc_t intfc, void *packet, bool is_pcl);

//!
//! @struct    rnet_intfc_rom_t
//!
//! @brief     Interface descriptor: unchanged/default/read-only memory
//!
typedef struct
{
    rnet_l2_t             l2_type;
    rnet_subi_t           subi1;
    rnet_subi_t           subi2;
    rnet_subi_t           subi3;
    nsvc_timer_t        **timer_ptr;
    void                 *counters;
    unsigned              counters_size;
    rnet_tx_api_t         tx_packet_api;
    uint16_t              option_flags;
} rnet_intfc_rom_t;

//!
//! @struct    rnet_intfc_rom_t
//!
//! @brief     Interface PPP state machine & counters
//!
typedef struct
{
    rnet_ppp_state_t      state;
    uint8_t               completion_counter;
    bool                  lcp_tx_closed;
    bool                  lcp_rx_closed;
    bool                  ipcp_tx_closed;
    bool                  ipcp_rx_closed;
    bool                  ipv6cp_tx_closed;
    bool                  ipv6cp_rx_closed;
    uint8_t               rx_id;
    uint8_t               tx_id;
} rnet_ppp_intfc_state_t;

// All interface L2 state machines
typedef union
{
    rnet_ppp_intfc_state_t ppp;
} rnet_l2_intfc_state_t;

//!
//! @struct    rnet_intfc_ram_t
//!
//! @brief     Interface descriptor: real-time values
//!
typedef struct
{
    bool                  l2_up;
    rnet_l2_intfc_state_t l2_state;
} rnet_intfc_ram_t;

//!
//! @struct    rnet_subi_rom_t
//!
//! @brief     Sub-interface: read-only settings
//!
typedef struct
{
    rnet_ip_traffic_t     type;
    rnet_ipacq_t          acquisition_method;
    rnet_intfc_t          parent;
    uint8_t               prefix_length;
    char                 *ip_addr;          // for RNET_IPACQ_HARD_CODED mode
} rnet_subi_rom_t;

//!
//! @struct    rnet_subi_ram_t
//!
//! @brief     Sub-interface: dynamic settings
//!
typedef struct
{
    uint8_t               prefix_length;
    rnet_ip_addr_union_t  ip_addr;          // actual IP address
} rnet_subi_ram_t;

//!
//! @struct    rnet_cir_rom_t
//!
//! @brief     Circuit: read-only settings
//!
typedef struct
{
    rnet_ip_traffic_t     type;
    rnet_ip_protocol_t    protocol;
    uint16_t              self_port;
    uint16_t              peer_port;        // if hard-coded
    rnet_subi_t           subi;
    char                 *peer_ip_addr;     // if hard-coded
    // nufr 'msg->fields' value for listener messages.
    // Set to RNET_LISTENER_MSG_DISABLED to disable
    uint32_t              buf_listener_msg; // nufr 'msg->fields' for RNET buffers
    uint32_t              pcl_listener_msg; //                    for particles
    nufr_tid_t            listener_task;    // 'NUFR_TID_null' means self-task
} rnet_cir_rom_t;

//!
//! @name      RNET_LISTENER_MSG_DISABLED
//!
//! @brief     Set in rnet_cit_rom_t->buf_listener_msg/pcl_listener
//! @brief     fields to disable msg.
//!
#define RNET_LISTENER_MSG_DISABLED       0xFFFFFFFF

//!
//! @struct    rnet_cir_ram_t
//!
//! @brief     Circuit: dynamic settings
//!
typedef struct
{
    bool                  is_active;
    rnet_ip_traffic_t     type;
    uint16_t              option_flags;
    rnet_ip_protocol_t    protocol;
    uint16_t              self_port;
    uint16_t              peer_port;
    rnet_subi_t           subi;
    rnet_ip_addr_union_t  peer_ip_addr;
    uint32_t              buf_listener_msg;
    uint32_t              pcl_listener_msg;
    nufr_tid_t            listener_task;
} rnet_cir_ram_t;

//!
//! @name      RNET_CIR_INDEX_SWAP_SRC_DEST
//!
//! @brief     Special value for circuit index:
//! @brief       (Insert this into RNET buffer/particle header circuit value)
//! @brief        Swap source and destination IP addresses
//!
#define RNET_CIR_INDEX_SWAP_SRC_DEST         255

//!
//! @struct    rnet_notif_list_t
//!
//! @brief     Generic notification type
//!
typedef struct
{
    uint32_t   msg_fields;      // nufr 'msg->fields' value for message
    nufr_tid_t tid;             // task to send msg to
} rnet_notif_list_t;

// APIs
RAGING_EXTERN_C_START
void rnet_intfc_init(void);
void rnet_intfc_start_or_restart_l2(rnet_intfc_t intfc);
bool rnet_intfc_is_valid(rnet_intfc_t intfc);
const rnet_intfc_rom_t *rnet_intfc_get_rom(rnet_intfc_t intfc);
rnet_l2_t rnet_intfc_get_type(rnet_intfc_t intfc);
uint16_t rnet_intfc_get_options(rnet_intfc_t intfc);
nsvc_timer_t *rnet_intfc_get_timer(rnet_intfc_t intfc);
unsigned rnet_intfc_get_counters(rnet_intfc_t intfc, void **counter_ptr);
rnet_intfc_ram_t *rnet_intfc_get_ram(rnet_intfc_t intfc);
const rnet_subi_rom_t *rnet_subi_get_rom(rnet_subi_t subi);
rnet_subi_ram_t *rnet_subi_get_ram(rnet_subi_t subi);
bool rnet_subi_lookup(rnet_intfc_t                intfc,
                      rnet_ip_addr_union_t       *ip_addr,
                      bool                        is_ipv6,
                      uint8_t                    *subi_value_ptr);
rnet_subi_t rnet_subi_attempt_and_learn_address(rnet_intfc_t intfc,
                                                rnet_ip_addr_union_t *ip_addr,
                                                bool                  is_ipv6);
rnet_cir_ram_t *rnet_circuit_get(unsigned circuit_index);
int rnet_circuit_index_lookup(rnet_subi_t           subi,
                              rnet_ip_protocol_t    l4_protocol,
                              uint16_t              self_port,
                              uint16_t              peer_port,
                              rnet_ip_addr_union_t *peer_ip_addr);
bool rnet_subi_is_ipv6(rnet_subi_t subi);
bool rnet_circuit_is_ipv4(unsigned circuit_index);
bool rnet_circuit_is_ipv6(unsigned circuit_index);
RAGING_EXTERN_C_END

#endif  // RNET_INTFC_H