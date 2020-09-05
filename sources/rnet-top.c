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

//! @file     rnet-top.c
//! @authors  Bernie Woodland
//! @date     27Mar18
//!
//! @brief    Top-level and message interface to RNET stack
//!

#include "rnet-ahdlc.h"
#include "rnet-ip.h"
#include "rnet-dispatch.h"
#include "rnet-buf.h"
#include "rnet-ppp.h"
#include "rnet-udp.h"
#include "rnet-icmp.h"
#include "rnet-top.h"

#include "raging-utils.h"



//!
//! @name      rnet_msg_processor
//!
//! @brief     Message pump for RNET stack
//!
//! @param[in] 'msg_id'-- nufr 'msg->fields' formatted message parms
//! @param[in] 'optional_parameter'-- RNET buffer, particle, other, or not used
//!
void rnet_msg_processor(rnet_id_t msg_id, uint32_t optional_parameter)
{
    switch (msg_id)
    {
#if RNET_CS_USING_BUFS == 1
    case RNET_ID_RX_BUF_ENTRY:
        rnet_msg_rx_buf_entry((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_RX_BUF_AHDLC_STRIP_CC:
        rnet_msg_rx_buf_ahdlc_strip_cc((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_RX_BUF_AHDLC_VERIFY_CRC:
        rnet_msg_rx_buf_ahdlc_verify_crc((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_RX_BUF_PPP:
        rnet_msg_rx_buf_ppp((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_RX_BUF_LCP:
        rnet_msg_rx_buf_lcp((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_RX_BUF_IPV4CP:
        rnet_msg_rx_buf_ipcp((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_RX_BUF_IPV6CP:
        rnet_msg_rx_buf_ipv6cp((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_RX_BUF_IPV4:
        rnet_msg_rx_buf_ipv4((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_RX_BUF_IPV6:
        rnet_msg_rx_buf_ipv6((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_RX_BUF_UDP:
        rnet_msg_rx_buf_udp((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_RX_BUF_ICMP:
        rnet_msg_rx_buf_icmp((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_RX_BUF_ICMPV6:
        rnet_msg_rx_buf_icmpv6((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_TX_BUF_UDP:
        rnet_msg_tx_buf_udp((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_TX_BUF_IPV4:
        rnet_msg_tx_buf_ipv4((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_TX_BUF_IPV6:
        rnet_msg_tx_buf_ipv6((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_TX_BUF_PPP:
        rnet_msg_tx_buf_ppp((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_TX_BUF_AHDLC_CRC:
        rnet_msg_tx_buf_ahdlc_crc((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_TX_BUF_AHDLC_ENCODE_CC:
        rnet_msg_tx_buf_ahdlc_encode_cc((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_TX_BUF_DRIVER:
        rnet_msg_tx_buf_driver((rnet_buf_t *)optional_parameter);
        break;

    case RNET_ID_BUF_DISCARD:
        rnet_msg_buf_discard((rnet_buf_t *)optional_parameter);
        break;

#endif  //RNET_CS_USING_BUFS

#if RNET_CS_USING_PCLS == 1
    case RNET_ID_RX_PCL_ENTRY:
        rnet_msg_rx_pcl_entry((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_RX_PCL_AHDLC_STRIP_CC:
        rnet_msg_rx_pcl_ahdlc_strip_cc((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_RX_PCL_AHDLC_VERIFY_CRC:
        rnet_msg_rx_pcl_ahdlc_verify_crc((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_RX_PCL_PPP:
        rnet_msg_rx_pcl_ppp((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_RX_PCL_LCP:
        rnet_msg_rx_pcl_lcp((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_RX_PCL_IPV4CP:
        rnet_msg_rx_pcl_ipcp((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_RX_PCL_IPV6CP:
        rnet_msg_rx_pcl_ipv6cp((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_TX_PCL_UDP:
        rnet_msg_tx_pcl_udp((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_RX_PCL_IPV4:
        rnet_msg_rx_pcl_ipv4((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_RX_PCL_IPV6:
        rnet_msg_rx_pcl_ipv6((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_RX_PCL_UDP:
        rnet_msg_rx_pcl_udp((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_RX_PCL_ICMP:
        rnet_msg_rx_pcl_icmp((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_RX_PCL_ICMPV6:
        rnet_msg_rx_pcl_icmpv6((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_TX_PCL_IPV4:
        rnet_msg_tx_pcl_ipv4((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_TX_PCL_IPV6:
        rnet_msg_tx_pcl_ipv6((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_TX_PCL_PPP:
        rnet_msg_tx_pcl_ppp((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_TX_PCL_AHDLC_CRC:
        rnet_msg_tx_pcl_ahdlc_crc((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_TX_PCL_AHDLC_ENCODE_CC:
        rnet_msg_tx_pcl_ahdlc_encode_cc((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_TX_PCL_DRIVER:
        rnet_msg_tx_pcl_driver((nsvc_pcl_t *)optional_parameter);
        break;

    case RNET_ID_PCL_DISCARD:
        rnet_msg_pcl_discard((nsvc_pcl_t *)optional_parameter);
        break;

#endif  //RNET_CS_USING_PCLS

    case RNET_ID_PPP_INIT:
        rnet_msg_ppp_init(optional_parameter);
        break;

    case RNET_ID_PPP_UP:
        // TBD
        break;

    case RNET_ID_PPP_DOWN:
        // TBD
        break;

    case RNET_ID_PPP_TIMEOUT_RECOVERY:
        rnet_ppp_timeout(RNET_PPP_EVENT_TIMEOUT_RECOVERY,
                         optional_parameter);
        break;

    case RNET_ID_PPP_TIMEOUT_PROBING:
        rnet_ppp_timeout(RNET_PPP_EVENT_TIMEOUT_PROBING,
                         optional_parameter);
        break;

    case RNET_ID_PPP_TIMEOUT_NEGOTIATING:
        rnet_ppp_timeout(RNET_PPP_EVENT_TIMEOUT_NEGOTIATING,
                         optional_parameter);
        break;


    }
}