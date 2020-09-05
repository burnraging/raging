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

//! @file     ut-rnet-wrapper.c
//! @authors  Bernie Woodland
//! @date     14Mar18
//!
//! @brief    Unit test of Raging Networking components
//!


#include "raging-global.h"
#include "raging-contract.h"
#include "raging-utils.h"
#include "nsvc.h"
#include "nsvc-api.h"
#include "nufr-platform.h"
#include "nufr-kernel-message-blocks.h"

#include "rnet-ip.h"
#include "rnet-top.h"
#include "rnet-buf.h"
#include "rnet-dispatch.h"
#include "rnet-intfc.h"
#include "rnet-app.h"

#include "ut-rnet-test-vectors.h"

#include <string.h>

unsigned os_ticks_remaining;
unsigned millisecs_remaining;
unsigned elapsed_time_for_timer;

// from 'ut-rnet-specific-tests.c'
void ut_rnet_ipv4_addr_ascii(void);
void ut_rnet_ipv6_addr_ascii(void);
void ut_rnet_crc16_test(void);
void ut_ipv4_checksum_test(void);
void ut_l4_checksum_partial_result(void);
void ut_ipv4_upd_packet_l4_checksum(void);
void ut_ahdlc_encode_decode_buf();
void ut_ahdlc_encode_decode_pcl();
void ut_rx_driver(void);

extern nufr_tcb_t nufr_tcb_block[NUFR_NUM_TASKS];
extern nufr_tcb_t *nufr_running;

// Loads a test vector into an RNET buffer, then sends message
// to RNET
void load_test_vector_buf(ut_test_vector_t which_vector,
                          rnet_id_t        inject_id,
                          bool             is_tx,
                          unsigned         circuit_index)
{
    const uint8_t *data_ptr;
    unsigned length;
    rnet_buf_t *buf;

    data_ptr = ut_fetch_test_vector(which_vector, &length);

    buf = rnet_alloc_bufW();

    // Set 'buf' data, meta-data
    if (!is_tx)
    {
        buf->header.offset = 0;
        buf->header.length = length;
        buf->header.intfc = RNET_INTFC_TEST1;
    }
    else
    {
        buf->header.offset = 60;     // must leave enough space to prepend headers
        buf->header.length = length;
        buf->header.intfc = 0;
        buf->header.circuit = circuit_index - 1;
    }
    memcpy(&(buf->buf[buf->header.offset]), data_ptr, length);

    rnet_msg_send(inject_id, buf);
}

// Same as above, but for particles
void load_test_vector_pcl(ut_test_vector_t which_vector,
                          rnet_id_t        inject_id,
                          bool             is_tx,
                          unsigned         circuit_index)
{
    uint8_t      *data_ptr;
    unsigned            length;
    nsvc_pcl_t         *head_pcl;
    nsvc_pcl_header_t  *pcl_header;
    nufr_sema_get_rtn_t alloc_rv;
    nsvc_pcl_chain_seek_t write_posit;
    nufr_sema_get_rtn_t   write_result;
    unsigned              write_offset;

    data_ptr = ut_fetch_test_vector(which_vector, &length);

    alloc_rv = nsvc_pcl_alloc_chainWT(&head_pcl, NULL, length, NSVC_PCL_NO_TIMEOUT);

    // Set 'buf' data, meta-data
    pcl_header = NSVC_PCL_HEADER(head_pcl);

    // Establish offset
    if (!is_tx)
    {
        write_offset = 0;
    }
    else
    {
        write_offset = 60;
    }
    (void)nsvc_pcl_set_seek_to_packet_offset(head_pcl, &write_posit, write_offset);

    // Write
    write_result = nsvc_pcl_write_dataWT(&head_pcl, &write_posit, data_ptr, length, NSVC_PCL_NO_TIMEOUT);

    pcl_header->offset = NSVC_PCL_OFFSET_PAST_HEADER(write_offset);
    pcl_header->total_used_length = length;

    if (!is_tx)
    {
        pcl_header->intfc = RNET_INTFC_TEST1;
    }
    else
    {
        pcl_header->intfc = 0;
        pcl_header->circuit = circuit_index - 1;
    }

    rnet_msg_send(inject_id, head_pcl);
}

// Run simulated real-time.
// Extract messages from message pump.
// Increment timers.
void message_loop(unsigned interval_millisecs)
{
    uint32_t    fields = 0;
    uint32_t    parameter = 0;
    rnet_id_t   id;
    uint32_t    reconfigured_time_ptr;

    os_ticks_remaining = NUFR_MILLISECS_TO_TICKS(interval_millisecs);
    millisecs_remaining = os_ticks_remaining * NUFR_TICK_PERIOD;

    do {

        // Exhaust all extant messages (This won't work in non UT nufr code!)
        while (NULL != nufr_msg_peek())
        {
            nufr_msg_getW(&fields, &parameter);

            if ((BIT_MASK32 == fields) && (BIT_MASK32 == parameter))
            {
                break;
            }

            id = (rnet_id_t)NUFR_GET_MSG_ID(fields);

            rnet_msg_processor(id, parameter);
        }

        // This will time out timers
        (void)nsvc_timer_expire_timer_callin(elapsed_time_for_timer, &reconfigured_time_ptr);

        elapsed_time_for_timer += NUFR_TICK_PERIOD;

        os_ticks_remaining--;
        millisecs_remaining = os_ticks_remaining * NUFR_TICK_PERIOD;

    } while (os_ticks_remaining > 0);
}

int main(void)
{
    // Fake out code for nsvc-timer.c's use later
    nufr_running = &nufr_tcb_block[0];

    // nufr, SL, RNET initializations
    nufr_msg_bpool_init();
    nsvc_init();
    nsvc_pcl_init();
    nsvc_timer_init(nufrplat_systick_get_reference_time, NULL);
    rnet_create_buf_pool();
    rnet_set_msg_prefix(NUFR_TID_01, 0);
    rnet_intfc_init();

#if 0
    // Specific tests 
    ut_rx_driver_test();
    ut_rnet_ipv4_addr_ascii();
    ut_rnet_ipv6_addr_ascii();
    ut_rnet_crc16_test();
    ut_ipv4_checksum_test();
    ut_l4_checksum_partial_result();
    ut_ipv4_upd_packet_l4_checksum();
    //ut_ahdlc_encode_decode_buf();
    ut_ahdlc_encode_decode_pcl();
#endif

    // inject single test vector
    // all commented out lines have been tested

    // reminder: for PPP testing, turn on RNET_ENABLE_PPP_TEST_MODE in rnet-compile-switches.h
//    load_test_vector_buf(UT_VECTOR_LCP_CONF_REQ, RNET_ID_RX_BUF_ENTRY, false, 0);
//    load_test_vector_buf(UT_VECTOR_WIRESHARK_LCP_CONF_REQ, RNET_ID_RX_BUF_ENTRY, false, 0);
//    load_test_vector_pcl(UT_VECTOR_WIRESHARK_LCP_CONF_REQ, RNET_ID_RX_PCL_ENTRY, false, 0);

    // reminder: for IP tests, change interface, sub-interface, circuits in rnet-app.c to match vector
    //           and turn on RNET_IP_L3_LOOPBACK_TEST_MODE
//    load_test_vector_buf(UT_VECTOR_IPV4_UDP_INTERNET, RNET_ID_RX_BUF_IPV4, false, 0);
//    load_test_vector_pcl(UT_VECTOR_IPV4_UDP_INTERNET, RNET_ID_RX_PCL_IPV4, false, 0);
//    load_test_vector_buf(UT_VECTOR_SIMPLE, RNET_ID_TX_BUF_UDP, true, RNET_PCIR_INTFC1_IPV4);
//    load_test_vector_pcl(UT_VECTOR_SIMPLE, RNET_ID_TX_PCL_UDP, true, RNET_PCIR_INTFC1_IPV4);
//    load_test_vector_buf(UT_VECTOR_IPV6_UDP_COAP_ACK, RNET_ID_RX_BUF_IPV6, false, 0);
//    load_test_vector_buf(UT_VECTOR_SIMPLE, RNET_ID_TX_BUF_UDP, true, RNET_PCIR_INTFC1_IPV6);
//    load_test_vector_pcl(UT_VECTOR_SIMPLE, RNET_ID_TX_PCL_UDP, true, RNET_PCIR_INTFC1_IPV6);
//    load_test_vector_buf(UT_VECTOR_ICMP_ECHO_REQUEST, RNET_ID_RX_BUF_IPV4, false, 0);
//    load_test_vector_pcl(UT_VECTOR_ICMP_ECHO_REQUEST, RNET_ID_RX_PCL_IPV4, false, 0);
//    load_test_vector_buf(UT_VECTOR_ICMPV6_ECHO_REQUEST, RNET_ID_RX_BUF_IPV6, false, 0);
//    load_test_vector_pcl(UT_VECTOR_ICMPV6_ECHO_REQUEST, RNET_ID_RX_PCL_IPV6, false, 0);

    
//    load_test_vector_buf(UT_VECTOR_IPV6_UPD_COAP_ACK, RNET_ID_RX_BUF_IPV6);
    
    // Run for x millisecs of simulated real-time
    message_loop(10000);

    return 0;
}