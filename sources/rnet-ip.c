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

//! @file     rnet-ip.c
//! @authors  Bernie Woodland
//! @date     10Mar18
//!
//! @brief    IPv4+IPv6 Headers
//!

#include "rnet-ip.h"
#include "rnet-ip-base-defs.h"
#include "rnet-ip-utils.h"
#include "rnet-dispatch.h"
#include "rnet-intfc.h"

#include "raging-utils.h"
#include "raging-utils-mem.h"
#include "raging-contract.h"

#define DEFAULT_TTL          128

#define TEMP_BUFFER_SIZE           40

//!
//! @name      rnet_msg_rx_buf_ipv4
//!
//! @brief     Entry point for IPv4 packet
//!
//! @param[in] 'buf'--
//!
void rnet_msg_rx_buf_ipv4(rnet_buf_t *buf)
{
    uint8_t             *ptr;
    bool                 rv;
    uint16_t             l4_checksum_sent;
    uint16_t             l4_checksum_calculated;
    unsigned             l4_checksum_offset;
    uint8_t             *l4_offset_ptr;
    rnet_ipv4_header_t   header;
    rnet_ph_t            previous_ph;
    rnet_ip_protocol_t   ip_protocol;

    SL_REQUIRE(IS_RNET_BUF(buf));

    // 'ptr' points to beginning of IPv4 header
    ptr = RNET_BUF_FRAME_START_PTR(buf);

    // Sanity check
    if (buf->header.length < IPV4_HEADER_SIZE)
    {
        buf->header.code = RNET_BUF_CODE_IP_PACKET_TOO_SMALL;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    rv = rnet_ipv4_deserialize_header(&header, ptr);
    if (!rv)
    {
        buf->header.code = RNET_BUF_CODE_IP_PACKET_HEADER_CORRUPTED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    // Adjust offset/length fields so they don't include IP header anymore
    buf->header.previous_ph = RNET_PH_IPV4;
    buf->header.offset += IPV4_HEADER_SIZE;
    buf->header.length -= IPV4_HEADER_SIZE;

    // Does the subinterface have an IP address yet? If not,
    //  learn it from this packet.
    (void)rnet_subi_attempt_and_learn_address((rnet_intfc_t)buf->header.intfc,
                                      (rnet_ip_addr_union_t *)header.dest_addr,
                                      false);

    // Match subinterface.
    // Set subinterface field in buffer header.
    if (!rnet_subi_lookup((rnet_intfc_t)buf->header.intfc,
                          (rnet_ip_addr_union_t *)header.dest_addr,
                          false,
                          &buf->header.subi))
    {
        // No subinterface matched...
        buf->header.code = RNET_BUF_CODE_IP_SUBI_NOT_FOUND;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    // Advance 'ptr' to beginning of UDP/ICMP/etc header
    ptr += IPV4_HEADER_SIZE;

    // Dig sent checksum out of L4 header
    l4_checksum_offset = rnet_ip_l4_checksum_offset(
                            rnet_ip_l4_ph_to_ip_protocol(header.ip_protocol));
    l4_offset_ptr = ptr + l4_checksum_offset;
    l4_checksum_sent = rutils_stream_to_word16(l4_offset_ptr);

    // Mask over L4 checksum, so it doesn't mess up calculation
    rutils_word16_to_stream(l4_offset_ptr, 0);

    // Validate L4 checksum
    if (0 != l4_checksum_sent)
    {
        if (RNET_IP_PROTOCOL_ICMP != header.ip_protocol)
        {
            l4_checksum_calculated =
                rnet_ipv4_pseudo_header_struct_checksum(&header);
        }
        // ICMPv4 doesn't include an IPv4 pseudo-header in L4 checksum calc
        else
        {
            l4_checksum_calculated = 0;
        }            

        // Do checksum over L4 header + data
        // Must user IPv4 header length, not actual packet length
        l4_checksum_calculated =
                rnet_ip_running_checksum(l4_checksum_calculated,
                                         ptr,
                                         header.total_length - IPV4_HEADER_SIZE);
        l4_checksum_calculated = BITWISE_NOT16(l4_checksum_calculated);

        // Restore previously masked over checksum
        rutils_word16_to_stream(l4_offset_ptr, l4_checksum_sent);

        if ((l4_checksum_sent != l4_checksum_calculated)
                                   &&
            !( (0xFFFF == l4_checksum_sent) && (0 == l4_checksum_calculated) ))
        {
            buf->header.code = RNET_BUF_CODE_IP_RX_BAD_CRC;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
            return;
        }
    }

    // Bump counter(s)
    previous_ph = buf->header.previous_ph;
    if (RNET_PH_PPP == previous_ph)
    {
        rnet_ppp_counters_t *ppp_counters;

        // 'void **' cast to supress compiler warning (hate doing it!)
        rnet_intfc_get_counters((rnet_intfc_t)buf->header.intfc,
                                (void **)&ppp_counters);

        (ppp_counters->ipv4_rx)++;
    }

    // Push packet up stack
    ip_protocol = header.ip_protocol;
    if (RNET_IP_PROTOCOL_UDP == ip_protocol)
    {
        rnet_msg_send(RNET_ID_RX_BUF_UDP, buf);
    }
    else if (RNET_IP_PROTOCOL_ICMP == ip_protocol)
    {
        rnet_msg_send(RNET_ID_RX_BUF_ICMP, buf);
    }
//    else if (RNET_IP_PROTOCOL_TCP == ip_protocol)
//    {
//        rnet_msg_send(RNET_ID_RX_BUF_TCP, buf);
//    }
    else
    {
        buf->header.code = RNET_BUF_CODE_IP_UNSUPPORTED_L4;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }
}

//!
//! @name      rnet_msg_rx_pcl_ipv4
//!
//! @brief     Entry point for IPv4 packet
//!
//! @param[in] 'head_pcl'--
//!
void rnet_msg_rx_pcl_ipv4(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t   *pcl_header;
    uint8_t             *ptr;
    bool                 rv;
    uint16_t             l4_checksum_sent;
    uint16_t             l4_checksum_calculated;
    unsigned             l4_checksum_offset;
    uint8_t             *l4_offset_ptr;
    rnet_ipv4_header_t   header;
    rnet_ph_t            previous_ph;
    rnet_ip_protocol_t   ip_protocol;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    // 'ptr' points to beginning of IPv4 header
    pcl_header = NSVC_PCL_HEADER(head_pcl);
    ptr = &(head_pcl->buffer)[pcl_header->offset];

    // Sanity check
    if (pcl_header->total_used_length < IPV4_HEADER_SIZE)
    {
        pcl_header->code = RNET_BUF_CODE_IP_PACKET_TOO_SMALL;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    rv = rnet_ipv4_deserialize_header(&header, ptr);
    if (!rv)
    {
        pcl_header->code = RNET_BUF_CODE_IP_PACKET_HEADER_CORRUPTED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, head_pcl);
        return;
    }

    // Adjust offset/length fields so they don't include IP header anymore
    pcl_header->previous_ph = RNET_PH_IPV4;
    pcl_header->offset += IPV4_HEADER_SIZE;
    pcl_header->total_used_length -= IPV4_HEADER_SIZE;

    // Does the subinterface have an IP address yet? If not,
    //  learn it from this packet.
    (void)rnet_subi_attempt_and_learn_address((rnet_intfc_t)pcl_header->intfc,
                                      (rnet_ip_addr_union_t *)header.dest_addr,
                                      false);

    // Match subinterface.
    // Set subinterface field in buffer header.
    if (!rnet_subi_lookup((rnet_intfc_t)pcl_header->intfc,
                          (rnet_ip_addr_union_t *)header.dest_addr,
                          false,
                          &pcl_header->subi))
    {
        // No subinterface matched...
        pcl_header->code = RNET_BUF_CODE_IP_SUBI_NOT_FOUND;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    // Advance 'ptr' to beginning of UDP/ICMP/etc header
    ptr += IPV4_HEADER_SIZE;

    // Dig sent checksum out of L4 header
    l4_checksum_offset = rnet_ip_l4_checksum_offset(
                            rnet_ip_l4_ph_to_ip_protocol(header.ip_protocol));
    l4_offset_ptr = ptr + l4_checksum_offset;
    l4_checksum_sent = rutils_stream_to_word16(l4_offset_ptr);

    // Mask over L4 checksum, so it doesn't mess up calculation
    rutils_word16_to_stream(l4_offset_ptr, 0);

    // Validate L4 checksum
    if (0 != l4_checksum_sent)
    {
        if (RNET_IP_PROTOCOL_ICMP != header.ip_protocol)
        {
            l4_checksum_calculated =
                rnet_ipv4_pseudo_header_struct_checksum(&header);
        }
        // ICMPv4 doesn't include an IPv4 pseudo-header in L4 checksum calc
        else
        {
            l4_checksum_calculated = 0;
        }            

        // Do checksum over L4 header + data
        l4_checksum_calculated =
               rnet_ip_pcl_add_data_to_checksum(l4_checksum_calculated,
                                                head_pcl,
                                                ptr,
                                       header.total_length - IPV4_HEADER_SIZE);
        l4_checksum_calculated = BITWISE_NOT16(l4_checksum_calculated);

        // Restore previously masked over checksum
        rutils_word16_to_stream(l4_offset_ptr, l4_checksum_sent);


        if ((l4_checksum_sent != l4_checksum_calculated)
                                   &&
            !( (0xFFFF == l4_checksum_sent) && (0 == l4_checksum_calculated) ))
        {
            pcl_header->code = RNET_BUF_CODE_IP_RX_BAD_CRC;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
            return;
        }
    }

    // Bump counter(s)
    previous_ph = pcl_header->previous_ph;
    if (RNET_PH_PPP == previous_ph)
    {
        rnet_ppp_counters_t *ppp_counters;

        // 'void **' cast to supress compiler warning (hate doing it!)
        (void)rnet_intfc_get_counters((rnet_intfc_t)pcl_header->intfc,
                                      (void **)&ppp_counters);

        (ppp_counters->ipv4_rx)++;
    }

    // Push packet up stack
    ip_protocol = header.ip_protocol;
    if (RNET_IP_PROTOCOL_UDP == ip_protocol)
    {
        rnet_msg_send(RNET_ID_RX_PCL_UDP, head_pcl);
    }
    else if (RNET_IP_PROTOCOL_ICMP == ip_protocol)
    {
        rnet_msg_send(RNET_ID_RX_PCL_ICMP, head_pcl);
    }
//    else if (RNET_IP_PROTOCOL_TCP == ip_protocol)
//    {
//        rnet_msg_send(RNET_ID_RX_PCL_TCP, head_pcl);
//    }
    else
    {
        pcl_header->code = RNET_BUF_CODE_IP_UNSUPPORTED_L4;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }
}

//!
//! @name      rnet_msg_rx_buf_ipv6
//!
//! @brief     Entry point for IPv6 packet
//!
//! @param[in] 'buf'--
//!
void rnet_msg_rx_buf_ipv6(rnet_buf_t *buf)
{
    uint8_t             *ptr;
    bool                 rv;
    uint16_t             l4_checksum_sent;
    uint16_t             l4_checksum_calculated;
    unsigned             l4_checksum_offset;
    uint8_t             *l4_offset_ptr;
    rnet_ipv6_header_t   header;
    rnet_ph_t            previous_ph;
    rnet_ip_protocol_t   ip_protocol;

    SL_REQUIRE(IS_RNET_BUF(buf));

    // 'ptr' points to beginning of IPv4 header
    ptr = RNET_BUF_FRAME_START_PTR(buf);

    // Sanity check
    if (buf->header.length < IPV6_HEADER_SIZE)
    {
        buf->header.code = RNET_BUF_CODE_IP_PACKET_TOO_SMALL;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    rv = rnet_ipv6_deserialize_header(&header, ptr);
    if (!rv)
    {
        buf->header.code = RNET_BUF_CODE_IP_PACKET_HEADER_CORRUPTED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    // Adjust offset/length fields so they don't include IP header anymore
    buf->header.previous_ph = RNET_PH_IPV6;
    buf->header.offset += IPV6_HEADER_SIZE;
    buf->header.length -= IPV6_HEADER_SIZE;

    // Does the subinterface have an IP address yet? If not,
    //  learn it from this packet.
    (void)rnet_subi_attempt_and_learn_address((rnet_intfc_t)buf->header.intfc,
                                      (rnet_ip_addr_union_t *)header.dest_addr,
                                      true);

    // Match subinterface.
    // Set subinterface field in buffer header.
    if (!rnet_subi_lookup((rnet_intfc_t)buf->header.intfc,
                          (rnet_ip_addr_union_t *)header.dest_addr,
                          true,
                          &buf->header.subi))
    {
        // No subinterface matched...
        buf->header.code = RNET_BUF_CODE_IP_SUBI_NOT_FOUND;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    // Advance 'ptr' to beginning of UDP/ICMP/etc header
    ptr += IPV6_HEADER_SIZE;

    // Dig sent checksum out of L4 header
    l4_checksum_offset = rnet_ip_l4_checksum_offset(
                            rnet_ip_l4_ph_to_ip_protocol(header.ip_protocol));
    l4_offset_ptr = ptr + l4_checksum_offset;
    l4_checksum_sent = rutils_stream_to_word16(l4_offset_ptr);

    // Mask over L4 checksum, so it doesn't mess up calculation
    rutils_word16_to_stream(l4_offset_ptr, 0);

    // Validate L4 checksum
    if (0 != l4_checksum_sent)
    {
        // Unlike ICMPv4, ICMPv6 includes IPv6 pseudo-header in checksum
        // Unlike IPv6, IPv6's length header is payload-only
        l4_checksum_calculated =
                rnet_ipv6_pseudo_header_struct_checksum(&header);
        l4_checksum_calculated =
                rnet_ip_running_checksum(l4_checksum_calculated,
                                         ptr,
                                         header.payload_length);
        l4_checksum_calculated = BITWISE_NOT16(l4_checksum_calculated);

        // Restore previously masked over checksum
        rutils_word16_to_stream(l4_offset_ptr, l4_checksum_sent);

        if ((l4_checksum_sent != l4_checksum_calculated)
                                   &&
            !( (0xFFFF == l4_checksum_sent) && (0 == l4_checksum_calculated) ))
        {
            buf->header.code = RNET_BUF_CODE_IP_RX_BAD_CRC;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
            return;
        }
    }

    // Bump counter(s)
    previous_ph = buf->header.previous_ph;
    if (RNET_PH_PPP == previous_ph)
    {
        rnet_ppp_counters_t *ppp_counters;
        // 'void **' cast to supress compiler warning (hate doing it!)
        (void)rnet_intfc_get_counters((rnet_intfc_t)buf->header.intfc,
                                      (void **)&ppp_counters);

        (ppp_counters->ipv6_rx)++;
    }

    // Push packet up stack
    ip_protocol = header.ip_protocol;
    if (RNET_IP_PROTOCOL_UDP == ip_protocol)
    {
        rnet_msg_send(RNET_ID_RX_BUF_UDP, buf);
    }
    else if (RNET_IP_PROTOCOL_ICMPv6 == ip_protocol)
    {
        rnet_msg_send(RNET_ID_RX_BUF_ICMPV6, buf);
    }
//    else if (RNET_IP_PROTOCOL_TCP == ip_protocol)
//    {
//        rnet_msg_send(RNET_ID_RX_BUF_TCP, buf);
//    }
    else
    {
        buf->header.code = RNET_BUF_CODE_IP_UNSUPPORTED_L4;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }
}

//!
//! @name      rnet_msg_rx_pcl_ipv6
//!
//! @brief     Entry point for IPv6 packet
//!
//! @param[in] 'head_pcl'--
//!
void rnet_msg_rx_pcl_ipv6(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t   *pcl_header;
    uint8_t             *ptr;
    bool                 rv;
    uint16_t             l4_checksum_sent;
    uint16_t             l4_checksum_calculated;
    unsigned             l4_checksum_offset;
    uint8_t             *l4_offset_ptr;
    rnet_ipv6_header_t   header;
    rnet_ph_t            previous_ph;
    rnet_ip_protocol_t   ip_protocol;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    // 'ptr' points to beginning of IPv4 header
    pcl_header = NSVC_PCL_HEADER(head_pcl);
    ptr = &(head_pcl->buffer)[pcl_header->offset];

    // Sanity check
    if (pcl_header->total_used_length < IPV6_HEADER_SIZE)
    {
        pcl_header->code = RNET_BUF_CODE_IP_PACKET_TOO_SMALL;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    rv = rnet_ipv6_deserialize_header(&header, ptr);
    if (!rv)
    {
        pcl_header->code = RNET_BUF_CODE_IP_PACKET_HEADER_CORRUPTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    // Adjust offset/length fields so they don't include IP header anymore
    pcl_header->previous_ph = RNET_PH_IPV6;
    pcl_header->offset += IPV6_HEADER_SIZE;
    pcl_header->total_used_length -= IPV6_HEADER_SIZE;

    // Does the subinterface have an IP address yet? If not,
    //  learn it from this packet.
    (void)rnet_subi_attempt_and_learn_address((rnet_intfc_t)pcl_header->intfc,
                                      (rnet_ip_addr_union_t *)header.dest_addr,
                                      true);

    // Match subinterface.
    // Set subinterface field in buffer header.
    if (!rnet_subi_lookup((rnet_intfc_t)pcl_header->intfc,
                          (rnet_ip_addr_union_t *)header.dest_addr,
                          true,
                          &pcl_header->subi))
    {
        // No subinterface matched...
        pcl_header->code = RNET_BUF_CODE_IP_SUBI_NOT_FOUND;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    // Advance 'ptr' to beginning of UDP/ICMP/etc header
    ptr += IPV6_HEADER_SIZE;

    // Dig sent checksum out of L4 header
    l4_checksum_offset = rnet_ip_l4_checksum_offset(
                            rnet_ip_l4_ph_to_ip_protocol(header.ip_protocol));
    l4_offset_ptr = ptr + l4_checksum_offset;
    l4_checksum_sent = rutils_stream_to_word16(l4_offset_ptr);

    // Mask over L4 checksum, so it doesn't mess up calculation
    rutils_word16_to_stream(l4_offset_ptr, 0);

    // Validate L4 checksum
    if (0 != l4_checksum_sent)
    {
        // Unlike ICMPv4, ICMPv6 includes IPv6 pseudo-header in checksum
        l4_checksum_calculated =
                rnet_ipv6_pseudo_header_struct_checksum(&header);
        l4_checksum_calculated =
               rnet_ip_pcl_add_data_to_checksum(l4_checksum_calculated,
                                                head_pcl,
                                                ptr,
                                       header.payload_length);
        l4_checksum_calculated = BITWISE_NOT16(l4_checksum_calculated);

        // Restore previously masked over checksum
        rutils_word16_to_stream(l4_offset_ptr, l4_checksum_sent);

        if ((l4_checksum_sent != l4_checksum_calculated)
                                   &&
            !( (0xFFFF == l4_checksum_sent) && (0 == l4_checksum_calculated) ))
        {
            pcl_header->code = RNET_BUF_CODE_IP_RX_BAD_CRC;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
            return;
        }
    }

    // Bump counter(s)
    previous_ph = pcl_header->previous_ph;
    if (RNET_PH_PPP == previous_ph)
    {
        rnet_ppp_counters_t *ppp_counters;

        // 'void **' cast to supress compiler warning (hate doing it!)
        (void)rnet_intfc_get_counters((rnet_intfc_t)pcl_header->intfc,
                                      (void **)&ppp_counters);

        (ppp_counters->ipv4_rx)++;
    }

    // Push packet up stack
    ip_protocol = header.ip_protocol;
    if (RNET_IP_PROTOCOL_UDP == ip_protocol)
    {
        rnet_msg_send(RNET_ID_RX_PCL_UDP, head_pcl);
    }
    else if (RNET_IP_PROTOCOL_ICMPv6 == ip_protocol)
    {
        rnet_msg_send(RNET_ID_RX_PCL_ICMPV6, head_pcl);
    }
//    else if (RNET_IP_PROTOCOL_TCP == ip_protocol)
//    {
//        rnet_msg_send(RNET_ID_RX_PCL_TCP, head_pcl);
//    }
    else
    {
        pcl_header->code = RNET_BUF_CODE_IP_UNSUPPORTED_L4;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }
}

//!
//! @name      rnet_msg_tx_buf_ipv4
//!
//! @brief     Entry point for sending IPv4 packet
//!
//! @param[in] 'buf'--
//!
void rnet_msg_tx_buf_ipv4(rnet_buf_t *buf)
{
    uint8_t             *ptr;
    rnet_ipv4_header_t   header;
    bool                 swap_circuit_value;
    bool                 do_swap;
    rnet_subi_ram_t     *subi_ram;
    const rnet_subi_rom_t *subi_rom;
    rnet_cir_ram_t      *circuit_ram;
    rnet_intfc_t         intfc;
    rnet_ip_protocol_t   ip_protocol;
    unsigned             l4_checksum_offset;
    uint8_t             *l4_offset_ptr;
    uint16_t             l4_checksum;

    SL_REQUIRE(IS_RNET_BUF(buf));

    // Sanity check
    if (buf->header.offset < IPV4_HEADER_SIZE)
    {
        buf->header.code = RNET_BUF_CODE_UNDERRUN;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    rutils_memset(&header, 0, sizeof(header));

    swap_circuit_value = RNET_CIR_INDEX_SWAP_SRC_DEST == buf->header.circuit;

    // Check for server mode
    if (!swap_circuit_value)
    {
        circuit_ram = rnet_circuit_get(buf->header.circuit);
        if (NULL == circuit_ram)
        {
            buf->header.code = RNET_BUF_CODE_IP_CIRCUIT_NOT_FOUND;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
            return;
        }

        do_swap = rnet_ip_is_null_address(false, &circuit_ram->peer_ip_addr);
    }
    // L4 layer marked swap?
    else
    {
        do_swap = true;
    }

    // Swap existing src & dest addresses?
    if (do_swap)
    {
        uint8_t ipv4_temp_addr[IPV4_ADDR_SIZE];

        intfc = buf->header.intfc;

        // 'ptr' points to beginning of IPv4 header
        ptr = RNET_BUF_FRAME_START_PTR(buf);

        // Adjust 'ptr' to point to start of IPv4 address header
        ptr -= IPV4_HEADER_SIZE;

        // Load in current IPv4 header
        (void)rnet_ipv4_deserialize_header(&header, ptr);

        // Swap addresses
        rutils_memcpy(ipv4_temp_addr, header.src_addr, IPV4_ADDR_SIZE);
        rutils_memcpy(header.src_addr, header.dest_addr, IPV4_ADDR_SIZE);
        rutils_memcpy(header.dest_addr, ipv4_temp_addr, IPV4_ADDR_SIZE);
    }
    // Look up address based on circuit, per usual
    else
    {
        subi_ram = rnet_subi_get_ram(circuit_ram->subi);
        if (NULL == subi_ram)
        {
            buf->header.code = RNET_BUF_CODE_IP_SUBI_NOT_FOUND;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
            return;
        }
        buf->header.subi = circuit_ram->subi;
        subi_rom = rnet_subi_get_rom(circuit_ram->subi);
        intfc = subi_rom->parent;
        buf->header.intfc = intfc;

        // Write IPv4 header struct fields.
        // Get info from circuit and subinterface.
        rutils_memcpy(header.src_addr, &subi_ram->ip_addr, IPV4_ADDR_SIZE);
        rutils_memcpy(header.dest_addr, &circuit_ram->peer_ip_addr, IPV4_ADDR_SIZE);
    }

    if (!rnet_intfc_is_valid(intfc))
    {
        buf->header.code = RNET_BUF_CODE_IP_INTFC_NOT_FOUND;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    ip_protocol = buf->header.previous_ph;
    header.ip_protocol = rnet_ip_ph_to_ip_protocol(ip_protocol);
    header.header_checksum = 0;      // will be filled in when serialized
    header.total_length = buf->header.length + IPV4_HEADER_SIZE;
    header.ttl = DEFAULT_TTL;
    header.header_checksum = 0;
    
    // Calculate byte offset that L4 header checksum value starts
    // Save ptr to L4 offset
    l4_checksum_offset = rnet_ip_l4_checksum_offset(buf->header.previous_ph);
    l4_offset_ptr = RNET_BUF_FRAME_START_PTR(buf) + l4_checksum_offset;

    // Adjust offset+length for pre-pending IPv4 serialized header
    buf->header.previous_ph = RNET_PH_IPV4;
    buf->header.offset -= IPV4_HEADER_SIZE;
    buf->header.length += IPV4_HEADER_SIZE;

    // 'ptr' points to beginning of IPv4 header
    ptr = RNET_BUF_FRAME_START_PTR(buf);

    // Checksum disabled
    rnet_ipv4_serialize_header(ptr, &header, false);

    // Calculate L4 checksum
    if (RNET_IP_PROTOCOL_ICMP != header.ip_protocol)
    {
        l4_checksum = rnet_ipv4_pseudo_header_struct_checksum(&header);
    }
    else
    {
        l4_checksum = 0;
    }
    l4_checksum = rnet_ip_running_checksum(l4_checksum,
                              RNET_BUF_FRAME_START_PTR(buf) + IPV4_HEADER_SIZE,
                              buf->header.length - IPV4_HEADER_SIZE);
    l4_checksum = BITWISE_NOT16(l4_checksum);
    if (0 == l4_checksum)
    {
        // '0' means "ignore checksum"; RFC says to flip it to 0xFFFF
        l4_checksum = 0xFFFF;
    }
    // Poke L4 checksum into L4 header
    rutils_word16_to_stream(l4_offset_ptr, l4_checksum);

    // Bump counter(s) and push packet down stack
    if (RNET_L2_PPP == rnet_intfc_get_type(intfc))
    {
        rnet_ppp_counters_t *ppp_counters;

        // 'void **' cast to supress compiler warning (hate doing it!)
        (void)rnet_intfc_get_counters(intfc,
                                      (void **)&ppp_counters);

        (ppp_counters->ipv4_tx)++;

    #if RNET_IP_L3_LOOPBACK_TEST_MODE == 0
        rnet_msg_send(RNET_ID_TX_BUF_PPP, buf);
    #else
        buf->header.intfc = RNET_INTFC_TEST2;
        rnet_msg_send(RNET_ID_RX_BUF_IPV4, buf);
    #endif
    }
    else
    {
        buf->header.code = RNET_BUF_CODE_INTFC_NOT_CONFIGURED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
    }
}

//!
//! @name      rnet_msg_tx_pcl_ipv4
//!
//! @brief     Entry point for sending IPv4 packet
//!
//! @param[in] 'head_pcl'--
//!
void rnet_msg_tx_pcl_ipv4(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t   *pcl_header;
    uint8_t             *ptr;
    rnet_ipv4_header_t   header;
    bool                 swap_circuit_value;
    bool                 do_swap;
    rnet_subi_ram_t     *subi_ram;
    const rnet_subi_rom_t *subi_rom;
    rnet_cir_ram_t      *circuit_ram;
    rnet_intfc_t         intfc;
    rnet_ip_protocol_t   ip_protocol;
    unsigned             l4_checksum_offset;
    uint8_t             *l4_offset_ptr;
    uint16_t             l4_checksum;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    pcl_header = NSVC_PCL_HEADER(head_pcl);

    // Sanity check
    if (pcl_header->offset < IPV4_HEADER_SIZE)
    {
        pcl_header->code = RNET_BUF_CODE_UNDERRUN;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    rutils_memset(&header, 0, sizeof(header));

    swap_circuit_value = RNET_CIR_INDEX_SWAP_SRC_DEST == pcl_header->circuit;

    // Check for server mode
    if (!swap_circuit_value)
    {
        circuit_ram = rnet_circuit_get(pcl_header->circuit);
        if (NULL == circuit_ram)
        {
            pcl_header->code = RNET_BUF_CODE_IP_CIRCUIT_NOT_FOUND;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
            return;
        }

        do_swap = rnet_ip_is_null_address(false, &circuit_ram->peer_ip_addr);
    }
    // L4 layer marked swap?
    else
    {
        do_swap = true;
    }

    // Swap existing src & dest addresses?
    if (do_swap)
    {
        uint8_t ipv4_temp_addr[IPV4_ADDR_SIZE];

        intfc = pcl_header->intfc;

        // 'ptr' points to beginning of IPv4 header
        ptr = &(head_pcl->buffer)[pcl_header->offset];

        // Adjust 'ptr' to point to start of IPv4 address header
        ptr -= IPV4_HEADER_SIZE;

        // Load in current IPv4 header
        (void)rnet_ipv4_deserialize_header(&header, ptr);

        // Swap addresses
        rutils_memcpy(ipv4_temp_addr, header.src_addr, IPV4_ADDR_SIZE);
        rutils_memcpy(header.src_addr, header.dest_addr, IPV4_ADDR_SIZE);
        rutils_memcpy(header.dest_addr, ipv4_temp_addr, IPV4_ADDR_SIZE);
    }
    // Look up address based on circuit, per usual
    else
    {
        subi_ram = rnet_subi_get_ram(circuit_ram->subi);
        if (NULL == subi_ram)
        {
            pcl_header->code = RNET_BUF_CODE_IP_SUBI_NOT_FOUND;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
            return;
        }
        pcl_header->subi = circuit_ram->subi;
        subi_rom = rnet_subi_get_rom(circuit_ram->subi);
        intfc = subi_rom->parent;
        pcl_header->intfc = intfc;

        // Write IPv4 header struct fields.
        // Get info from circuit and subinterface.
        rutils_memcpy(header.src_addr, &subi_ram->ip_addr, IPV4_ADDR_SIZE);
        rutils_memcpy(header.dest_addr, &circuit_ram->peer_ip_addr, IPV4_ADDR_SIZE);
    }

    if (!rnet_intfc_is_valid(intfc))
    {
        pcl_header->code = RNET_BUF_CODE_IP_INTFC_NOT_FOUND;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    ip_protocol = pcl_header->previous_ph;
    header.ip_protocol = rnet_ip_ph_to_ip_protocol(ip_protocol);
    header.header_checksum = 0;      // will be filled in when serialized
    header.total_length = pcl_header->total_used_length + IPV4_HEADER_SIZE;
    header.ttl = DEFAULT_TTL;
    header.header_checksum = 0;
    
    // Calculate byte offset that L4 header checksum value starts
    // Save ptr to L4 offset
    l4_checksum_offset = rnet_ip_l4_checksum_offset(pcl_header->previous_ph);
    l4_offset_ptr = &(head_pcl->buffer)[pcl_header->offset] + l4_checksum_offset;

    // Adjust offset+length for pre-pending IPv4 serialized header
    pcl_header->previous_ph = RNET_PH_IPV4;
    pcl_header->offset -= IPV4_HEADER_SIZE;
    pcl_header->total_used_length += IPV4_HEADER_SIZE;

    // 'ptr' points to beginning of IPv4 header
    ptr = &(head_pcl->buffer)[pcl_header->offset];

    // Checksum disabled
    rnet_ipv4_serialize_header(ptr, &header, false);

    // Calculate L4 checksum
    if (RNET_IP_PROTOCOL_ICMP != header.ip_protocol)
    {
        l4_checksum = rnet_ipv4_pseudo_header_struct_checksum(&header);
    }
    else
    {
        l4_checksum = 0;
    }
    l4_checksum =
               rnet_ip_pcl_add_data_to_checksum(l4_checksum,
                    head_pcl,
                    &(head_pcl->buffer)[pcl_header->offset] + IPV4_HEADER_SIZE,
                    pcl_header->total_used_length - IPV4_HEADER_SIZE);
    l4_checksum = BITWISE_NOT16(l4_checksum);
    if (0 == l4_checksum)
    {
        // '0' means "ignore checksum"; RFC says to flip it to 0xFFFF
        l4_checksum = 0xFFFF;
    }
    // Poke L4 checksum into L4 header
    rutils_word16_to_stream(l4_offset_ptr, l4_checksum);

    // Bump counter(s) and push packet down stack
    if (RNET_L2_PPP == rnet_intfc_get_type(intfc))
    {
        rnet_ppp_counters_t *ppp_counters;

        // 'void **' cast to supress compiler warning (hate doing it!)
        (void)rnet_intfc_get_counters((rnet_intfc_t)pcl_header->intfc,
                                      (void **)&ppp_counters);

        (ppp_counters->ipv4_tx)++;

    #if RNET_IP_L3_LOOPBACK_TEST_MODE == 0
        rnet_msg_send(RNET_ID_TX_PCL_PPP, head_pcl);
    #else
        pcl_header->intfc = RNET_INTFC_TEST2;
        rnet_msg_send(RNET_ID_RX_PCL_IPV4, head_pcl);
    #endif
    }
    else
    {
        pcl_header->code = RNET_BUF_CODE_INTFC_NOT_CONFIGURED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
    }
}

//!
//! @name      rnet_msg_tx_buf_ipv6
//!
//! @brief     Entry point for sending IPv6 packet
//!
//! @param[in] 'buf'--
//!
void rnet_msg_tx_buf_ipv6(rnet_buf_t *buf)
{
    uint8_t             *ptr;
    rnet_ipv6_header_t   header;
    bool                 swap_circuit_value;
    bool                 do_swap;
    rnet_subi_ram_t     *subi_ram;
    const rnet_subi_rom_t *subi_rom;
    rnet_cir_ram_t      *circuit_ram;
    rnet_intfc_t         intfc;
    rnet_ip_protocol_t   ip_protocol;
    unsigned             l4_checksum_offset;
    uint8_t             *l4_offset_ptr;
    uint16_t             l4_checksum;

    SL_REQUIRE(IS_RNET_BUF(buf));

    // Sanity check
    if (buf->header.offset < IPV6_HEADER_SIZE)
    {
        buf->header.code = RNET_BUF_CODE_UNDERRUN;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    rutils_memset(&header, 0, sizeof(header));

    swap_circuit_value = RNET_CIR_INDEX_SWAP_SRC_DEST == buf->header.circuit;

    // Check for server mode
    if (!swap_circuit_value)
    {
        circuit_ram = rnet_circuit_get(buf->header.circuit);
        if (NULL == circuit_ram)
        {
            buf->header.code = RNET_BUF_CODE_IP_CIRCUIT_NOT_FOUND;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
            return;
        }

        do_swap = rnet_ip_is_null_address(true, &circuit_ram->peer_ip_addr);
    }
    // L4 layer marked swap?
    else
    {
        do_swap = true;
    }

    // Swap existing src & dest addresses?
    if (do_swap)
    {
        uint8_t ipv6_temp_addr[IPV6_ADDR_SIZE];

        intfc = buf->header.intfc;

        // 'ptr' points to beginning of IPv6 header
        ptr = RNET_BUF_FRAME_START_PTR(buf);

        // Adjust 'ptr' to point to start of IPv6 address header
        ptr -= IPV6_HEADER_SIZE;

        // Load in current IPv4 header
        (void)rnet_ipv6_deserialize_header(&header, ptr);

        // Swap addresses
        rutils_memcpy(ipv6_temp_addr, header.src_addr, IPV6_ADDR_SIZE);
        rutils_memcpy(header.src_addr, header.dest_addr, IPV6_ADDR_SIZE);
        rutils_memcpy(header.dest_addr, ipv6_temp_addr, IPV6_ADDR_SIZE);
    }
    // Look up address based on circuit, per usual
    else
    {
        subi_ram = rnet_subi_get_ram(circuit_ram->subi);
        if (NULL == subi_ram)
        {
            buf->header.code = RNET_BUF_CODE_IP_SUBI_NOT_FOUND;
            rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
            return;
        }
        buf->header.subi = circuit_ram->subi;
        subi_rom = rnet_subi_get_rom(circuit_ram->subi);
        intfc = subi_rom->parent;
        buf->header.intfc = intfc;

        // Write IPv4 header struct fields.
        // Get info from circuit and subinterface.
        rutils_memcpy(header.src_addr, &subi_ram->ip_addr, IPV6_ADDR_SIZE);
        rutils_memcpy(header.dest_addr, &circuit_ram->peer_ip_addr, IPV6_ADDR_SIZE);
    }

    if (!rnet_intfc_is_valid(intfc))
    {
        buf->header.code = RNET_BUF_CODE_IP_INTFC_NOT_FOUND;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
        return;
    }

    ip_protocol = buf->header.previous_ph;
    header.ip_protocol = rnet_ip_ph_to_ip_protocol(ip_protocol);
    header.payload_length = buf->header.length;
    header.hop_limit = DEFAULT_TTL;
    
    l4_checksum_offset = rnet_ip_l4_checksum_offset(buf->header.previous_ph);
    l4_offset_ptr = RNET_BUF_FRAME_START_PTR(buf) + l4_checksum_offset;

    // Adjust offset+length for pre-pending IPv6 serialized header
    buf->header.previous_ph = RNET_PH_IPV6;
    buf->header.offset -= IPV6_HEADER_SIZE;
    buf->header.length += IPV6_HEADER_SIZE;

    // 'ptr' points to beginning of IPv4 header
    ptr = RNET_BUF_FRAME_START_PTR(buf);

    // Serialize
    rnet_ipv6_serialize_header(ptr, &header);

    // Calculate L4 checksum
    l4_checksum = rnet_ipv6_pseudo_header_struct_checksum(&header);
    l4_checksum = rnet_ip_running_checksum(l4_checksum,
                              RNET_BUF_FRAME_START_PTR(buf) + IPV6_HEADER_SIZE,
                              buf->header.length - IPV6_HEADER_SIZE);
    l4_checksum = BITWISE_NOT16(l4_checksum);
    if (0 == l4_checksum)
    {
        // '0' means "ignore checksum"; RFC says to flip it to 0xFFFF
        l4_checksum = 0xFFFF;
    }
    // Poke L4 checksum into L4 header
    rutils_word16_to_stream(l4_offset_ptr, l4_checksum);

    // Bump counter(s) and push packet down stack
    if (RNET_L2_PPP == rnet_intfc_get_type(intfc))
    {
        rnet_ppp_counters_t *ppp_counters;

        // 'void **' cast to supress compiler warning (hate doing it!)
        (void)rnet_intfc_get_counters((rnet_intfc_t)buf->header.intfc,
                                      (void **)&ppp_counters);

        (ppp_counters->ipv6_tx)++;

    #if RNET_IP_L3_LOOPBACK_TEST_MODE == 0
        rnet_msg_send(RNET_ID_TX_BUF_PPP, buf);
    #else
        buf->header.intfc = RNET_INTFC_TEST2;
        rnet_msg_send(RNET_ID_RX_BUF_IPV6, buf);
    #endif
    }
    else
    {
        buf->header.code = RNET_BUF_CODE_INTFC_NOT_CONFIGURED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
    }
}

//!
//! @name      rnet_msg_tx_pcl_ipv6
//!
//! @brief     Entry point for sending IPv6 packet
//!
//! @param[in] 'head_pcl'--
//!
void rnet_msg_tx_pcl_ipv6(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_header_t   *pcl_header;
    uint8_t             *ptr;
    rnet_ipv6_header_t   header;
    bool                 swap_circuit_value;
    bool                 do_swap;
    rnet_subi_ram_t     *subi_ram;
    const rnet_subi_rom_t *subi_rom;
    rnet_cir_ram_t      *circuit_ram;
    rnet_intfc_t         intfc;
    rnet_ip_protocol_t   ip_protocol;
    unsigned             l4_checksum_offset;
    uint8_t             *l4_offset_ptr;
    uint16_t             l4_checksum;

    SL_REQUIRE(nsvc_pcl_is(head_pcl));

    pcl_header = NSVC_PCL_HEADER(head_pcl);

    // Sanity check
    if (pcl_header->offset < IPV6_HEADER_SIZE)
    {
        pcl_header->code = RNET_BUF_CODE_UNDERRUN;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    rutils_memset(&header, 0, sizeof(header));

    swap_circuit_value = RNET_CIR_INDEX_SWAP_SRC_DEST == pcl_header->circuit;

    // Check for server mode
    if (!swap_circuit_value)
    {
        circuit_ram = rnet_circuit_get(pcl_header->circuit);
        if (NULL == circuit_ram)
        {
            pcl_header->code = RNET_BUF_CODE_IP_CIRCUIT_NOT_FOUND;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
            return;
        }

        do_swap = rnet_ip_is_null_address(true, &circuit_ram->peer_ip_addr);
    }
    // L4 layer marked swap?
    else
    {
        do_swap = true;
    }

    // Swap existing src & dest addresses?
    if (do_swap)
    {
        uint8_t ipv6_temp_addr[IPV6_ADDR_SIZE];

        intfc = pcl_header->intfc;

        // 'ptr' points to beginning of IPv6 header
        ptr = &(head_pcl->buffer)[pcl_header->offset];

        // Adjust 'ptr' to point to start of IPv6 address header
        ptr -= IPV6_HEADER_SIZE;

        // Load in current IPv6 header
        (void)rnet_ipv6_deserialize_header(&header, ptr);

        // Swap addresses
        rutils_memcpy(ipv6_temp_addr, header.src_addr, IPV6_ADDR_SIZE);
        rutils_memcpy(header.src_addr, header.dest_addr, IPV6_ADDR_SIZE);
        rutils_memcpy(header.dest_addr, ipv6_temp_addr, IPV6_ADDR_SIZE);
    }
    // Look up address based on circuit, per usual
    else
    {
        subi_ram = rnet_subi_get_ram(circuit_ram->subi);
        if (NULL == subi_ram)
        {
            pcl_header->code = RNET_BUF_CODE_IP_SUBI_NOT_FOUND;
            rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
            return;
        }
        pcl_header->subi = circuit_ram->subi;
        subi_rom = rnet_subi_get_rom(circuit_ram->subi);
        intfc = subi_rom->parent;
        pcl_header->intfc = intfc;

        // Write IPv6 header struct fields.
        // Get info from circuit and subinterface.
        rutils_memcpy(header.src_addr, &subi_ram->ip_addr, IPV6_ADDR_SIZE);
        rutils_memcpy(header.dest_addr, &circuit_ram->peer_ip_addr, IPV6_ADDR_SIZE);
    }

    if (!rnet_intfc_is_valid(intfc))
    {
        pcl_header->code = RNET_BUF_CODE_IP_INTFC_NOT_FOUND;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
        return;
    }

    ip_protocol = pcl_header->previous_ph;
    header.ip_protocol = rnet_ip_ph_to_ip_protocol(ip_protocol);
    header.payload_length = pcl_header->total_used_length;
    header.hop_limit = DEFAULT_TTL;
    
    // Calculate byte offset that L4 header checksum value starts
    // Save ptr to L4 offset
    l4_checksum_offset = rnet_ip_l4_checksum_offset(pcl_header->previous_ph);
    l4_offset_ptr = &(head_pcl->buffer)[pcl_header->offset] + l4_checksum_offset;

    // Adjust offset+length for pre-pending IPv4 serialized header
    pcl_header->previous_ph = RNET_PH_IPV6;
    pcl_header->offset -= IPV6_HEADER_SIZE;
    pcl_header->total_used_length += IPV6_HEADER_SIZE;

    // 'ptr' points to beginning of IPv4 header
    ptr = &(head_pcl->buffer)[pcl_header->offset];

    // Serialize
    rnet_ipv6_serialize_header(ptr, &header);

    // Calculate L4 checksum
    l4_checksum = rnet_ipv6_pseudo_header_struct_checksum(&header);
    l4_checksum =
               rnet_ip_pcl_add_data_to_checksum(l4_checksum,
                    head_pcl,
                    &(head_pcl->buffer)[pcl_header->offset] + IPV6_HEADER_SIZE,
                    pcl_header->total_used_length - IPV6_HEADER_SIZE);
    l4_checksum = BITWISE_NOT16(l4_checksum);
    if (0 == l4_checksum)
    {
        // '0' means "ignore checksum"; RFC says to flip it to 0xFFFF
        l4_checksum = 0xFFFF;
    }
    // Poke L4 checksum into L4 header
    rutils_word16_to_stream(l4_offset_ptr, l4_checksum);

    // Bump counter(s) and push packet down stack
    if (RNET_L2_PPP == rnet_intfc_get_type(intfc))
    {
        rnet_ppp_counters_t *ppp_counters;

        // 'void **' cast to supress compiler warning (hate doing it!)
        (void)rnet_intfc_get_counters((rnet_intfc_t)pcl_header->intfc,
                                      (void **)&ppp_counters);

        (ppp_counters->ipv6_tx)++;

    #if RNET_IP_L3_LOOPBACK_TEST_MODE == 0
        rnet_msg_send(RNET_ID_TX_PCL_PPP, head_pcl);
    #else
        pcl_header->intfc = RNET_INTFC_TEST2;
        rnet_msg_send(RNET_ID_RX_PCL_IPV6, head_pcl);
    #endif
    }
    else
    {
        pcl_header->code = RNET_BUF_CODE_INTFC_NOT_CONFIGURED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
    }
}

//!
//! @name      rnet_ip_is_valid_protocol
//!
//! @brief     Validate IP protocol type against what rnet supports
//!
//! @param[in] 'protocol'--
//!
//! @return    'true' if protocol is supported
//!
bool rnet_ip_is_valid_protocol(rnet_ip_protocol_t protocol)
{
    switch (protocol)
    {
    case RNET_IP_PROTOCOL_ICMP:
    //case RNET_IP_PROTOCOL_TCP:
    case RNET_IP_PROTOCOL_UDP:
    case RNET_IP_PROTOCOL_ICMPv6:
        return true;
        break;
    default:
        
        break;
    }

    return false;
}

//!
//! @name      ip_ph_to_ip_protocol
//!
//! @brief     Converts buffer/pcl header protocol value to IP Protocol
//! @brief     field value.
//!
//! @param[in] 'ph'--
//!
//! @return    output
//!
rnet_ip_protocol_t rnet_ip_ph_to_ip_protocol(rnet_ph_t ph)
{
    rnet_ip_protocol_t protocol;

    switch (ph)
    {
    case RNET_PH_UDP:
        protocol = RNET_IP_PROTOCOL_UDP;
        break;
    case RNET_PH_TCP:
        protocol = RNET_IP_PROTOCOL_TCP;
        break;
    case RNET_PH_ICMP:
        protocol = RNET_IP_PROTOCOL_ICMP;
        break;
    case RNET_PH_ICMPv6:
        protocol = RNET_IP_PROTOCOL_ICMPv6;
        break;
    default:
        protocol = RNET_IP_PROTOCOL_UDP;
        break;
    }

    return protocol;
}

//!
//! @name      ip_ph_to_ip_protocol
//!
//! @brief     Converts IP Protocol field value to buffer/pcl header
//! @brief     protocol value.
//!
//! @param[in] 'protocol'--
//!
//! @return    output
//!
rnet_ph_t rnet_ip_ip_protocol_to_ph(rnet_ip_protocol_t protocol)
{
    rnet_ph_t ph;

    switch (protocol)
    {
    case RNET_IP_PROTOCOL_ICMP:
        ph = RNET_PH_ICMP;
        break;
    case RNET_IP_PROTOCOL_TCP:
        ph = RNET_PH_TCP;
        break;
    case RNET_IP_PROTOCOL_UDP:
        ph = RNET_PH_UDP;
        break;
    case RNET_IP_PROTOCOL_ICMPv6:
        ph = RNET_PH_ICMPv6;
        break;
    default:
        ph = RNET_PH_null;
        break;
    }

    return ph;
}

//!
//! @name      ip_ph_to_ip_protocol
//!
//! @brief     Calculate byte offset that L4 header checksum value starts at
//!
//! @param[in] 'ph'--
//!
//! @return    offset from start of L4 header, in bytes
//!
unsigned rnet_ip_l4_checksum_offset(rnet_ph_t ph)
{
    unsigned l4_checksum_offset;

    if (RNET_PH_TCP == ph)
    {
        l4_checksum_offset = 16;
    }
    else if ((RNET_PH_ICMP == ph) || (RNET_PH_ICMPv6 == ph))
    {
        l4_checksum_offset = 2;
    }
    else
    {
        // UDP value
        l4_checksum_offset = 6;
    }

    return l4_checksum_offset;
}

//!
//! @name      rnet_ip_l4_ph_to_ip_protocol
//!
//! @brief     Conversion
//!
//! @param[in] 'protocol'--
//!
//! @return    ph number
//!
rnet_ph_t rnet_ip_l4_ph_to_ip_protocol(rnet_ip_protocol_t protocol)
{
    rnet_ph_t ph;

    if (RNET_IP_PROTOCOL_UDP == protocol)
        ph = RNET_PH_UDP;
    else if (RNET_IP_PROTOCOL_ICMP == protocol)
        ph = RNET_PH_ICMP;
    else if (RNET_IP_PROTOCOL_ICMPv6 == protocol)
        ph = RNET_PH_ICMPv6;
    else
        ph = RNET_PH_TCP;

    return ph;
}

//!
//! @name      rnet_ip_is_ipv6_traffic_type
//!
//! @brief     Is IPv4 or IPv6 based on traffic type?
//!
//! @param[in] 'traffic_type'--
//!
//! @return    'true' if IPv6; 'false' if IPv4
//!
bool rnet_ip_is_ipv6_traffic_type(rnet_ip_traffic_t traffic_type)
{
    switch (traffic_type)
    {
    case RNET_TR_IPV4_UNICAST:
        return false;
        break;
    default:
        break;
    }

    return true;
}

//!
//! @name      rnet_ipv4_serialize_header
//!
//! @brief     Convert an 'rnet_ipv4_header_t' struct to a byte stream
//!
//! @param[out] 'buffer'-- stream to write header onto
//! @param[out]           Consumes 'IPV4_HEADER_SIZE' bytes
//! @param[in] 'header'-- header info to serialize
//! @param[in] 'include_checksum'-- 'false' to set checksum field to zero
//!
void rnet_ipv4_serialize_header(uint8_t            *buffer,
                                rnet_ipv4_header_t *header,
                                bool                include_checksum)
{
    const unsigned ihl = IPV4_HEADER_SIZE/BYTES_PER_WORD32;  // no extensions
    uint8_t *start_ptr;
    uint8_t *checksum_ptr;
    uint16_t checksum;

    start_ptr = buffer;

    *buffer++ = (4 << 4) | (uint8_t)(ihl & BIT_MASK_NIBBLE);
    *buffer++ = header->dscp << 2;           // ignore ECN

    rutils_word16_to_stream(buffer, header->total_length);
    buffer += sizeof(uint16_t);

    *buffer++ = 0;                           // ignore identification
    *buffer++ = 0;                           // ignore identification
    *buffer++ = 0;                           // ignore flags, fragment offset
    *buffer++ = 0;                           // ignore fragment offset

    *buffer++ = header->ttl;
    *buffer++ = header->ip_protocol;

    checksum_ptr = buffer;
    *buffer++ = 0;                           // overwrite checksum later
    *buffer++ = 0;

    rutils_memcpy(buffer, header->src_addr, IPV4_ADDR_SIZE);
    buffer += IPV4_ADDR_SIZE;

    rutils_memcpy(buffer, header->dest_addr, IPV4_ADDR_SIZE);
    buffer += IPV4_ADDR_SIZE;

    // Poke in checksum if requested
    if (include_checksum)
    {
        checksum = rnet_ipv4_checksum(start_ptr);
        rutils_word16_to_stream(checksum_ptr, checksum);
        header->header_checksum = checksum;
    }
}

//!
//! @name      rnet_ipv6_serialize_header
//!
//! @brief     Convert an 'rnet_ipv6_header_t' struct to a byte stream
//!
//! @param[out] 'buffer'-- stream to write header onto
//! @param[out]           Consumes 'IPV6_HEADER_SIZE' bytes
//! @param[in] 'header'-- header info to serialize
//!
void rnet_ipv6_serialize_header(uint8_t            *buffer,
                                rnet_ipv6_header_t *header)
{
    *buffer++ = (6 << 4) | (header->traffic_class >> 4);
    *buffer++ = (header->traffic_class << 4);     // ignore flow label
    *buffer++ = 0;                                // ignore flow label
    *buffer++ = 0;                                // ignore flow label

    rutils_word16_to_stream(buffer, header->payload_length);
    buffer += sizeof(uint16_t);

    *buffer++ = header->ip_protocol;
    *buffer++ = header->hop_limit;

    rutils_memcpy(buffer, header->src_addr, IPV6_ADDR_SIZE);
    buffer += IPV6_ADDR_SIZE;
    rutils_memcpy(buffer, header->dest_addr, IPV6_ADDR_SIZE);
    //buffer += IPV6_ADDR_SIZE;
}

//!
//! @name      rnet_ipv4_deserialize_header
//!
//! @brief     Convert a byte stream to an 'rnet_ipv4_header_t' struct
//!
//! @param[out] 'header'-- header info to create
//! @param[in] 'buffer'-- stream containing actual header
//! @param[in]           Assumes 'IPV4_HEADER_SIZE' bytes on stream
//!
bool rnet_ipv4_deserialize_header(rnet_ipv4_header_t *header,
                                  uint8_t            *buffer)
{
    uint8_t *start_ptr;
    uint8_t       *ptr;
    uint16_t sent_checksum;
    uint16_t calculated_checksum;
    rnet_ip_protocol_t protocol;
    unsigned a_byte;
    unsigned ihl;

    ptr = buffer;
    start_ptr = ptr;

    a_byte = *ptr++;

    if ((a_byte >> 4) != 4)
    {
        return false;
    }

    // Sanity check ihl: no extra stuff in header
    ihl = (a_byte & BIT_MASK_NIBBLE);
    if (IPV4_HEADER_SIZE/BYTES_PER_WORD32 != ihl)
    {
        return false;
    }

    header->dscp = *ptr++;

    header->total_length = rutils_stream_to_word16(ptr);
    ptr += BYTES_PER_WORD16;

    // ignore identification, flags, fragment offset
    ptr += 4;

    header->ttl = *ptr++;

    protocol = (rnet_ip_protocol_t)(*ptr++);
    header->ip_protocol = protocol;

    if (!rnet_ip_is_valid_protocol(protocol))
    {
        return false;
    }

    sent_checksum = rutils_stream_to_word16(ptr);
    header->header_checksum = sent_checksum;
    ptr += BYTES_PER_WORD16;

    rutils_memcpy(header->src_addr, ptr, IPV4_ADDR_SIZE);
    ptr += sizeof(uint32_t);

    rutils_memcpy(header->dest_addr, ptr, IPV4_ADDR_SIZE);

    if (0 != sent_checksum)
    {
        calculated_checksum = rnet_ipv4_checksum(start_ptr);

        if (sent_checksum != calculated_checksum)
        {
            return false;
        }
    }

    return true;
}

//!
//! @name      rnet_ipv6_deserialize_header
//!
//! @brief     Convert a byte stream to an 'rnet_ipv6_header_t' struct
//!
//! @param[out] 'header'-- header info to create
//! @param[in] 'buffer'-- stream containing actual header
//! @param[in]           Assumes 'IPV6_HEADER_SIZE' bytes on stream
//!
bool rnet_ipv6_deserialize_header(rnet_ipv6_header_t *header,
                                  uint8_t            *buffer)
{
    uint8_t       *ptr;

    ptr = buffer;

    // verify that version is == 6
    if (6 != (*ptr >> 4))
    {
        return false;
    }

    header->traffic_class = (*ptr++) << 4;
    header->traffic_class |= (*ptr++) >> 4;

    ptr += 2;             // flow label, remaining bytes

    header->payload_length = rutils_stream_to_word16(ptr);
    ptr += BYTES_PER_WORD16;

    header->ip_protocol = (rnet_ip_protocol_t)(*ptr++);

    if (!rnet_ip_is_valid_protocol(header->ip_protocol))
    {
        return false;
    }

    header->hop_limit = *ptr++;

    rutils_memcpy(header->src_addr, ptr, IPV6_ADDR_SIZE);
    ptr += IPV6_ADDR_SIZE;

    rutils_memcpy(header->dest_addr, ptr, IPV6_ADDR_SIZE);

    return true;
}

//!
//! @name      rnet_ipv4_checksum
//!
//! @details   If header has a non-zero checksum value in it already,
//! @details   that value is saved, then zeroed out, the checksum
//! @details   is computed, then the value is restored.
//! @details   Per RFC 791
//!
//! @brief     Calculate IPv4 header checksum value on
//! @brief     an unserialized stream of an IPv4 header
//!
//! @param[in] 'header_start_ptr'-- header info to create
//!
//! @return    checksum
//!
uint16_t rnet_ipv4_checksum(uint8_t *header_start_ptr)
{
    uint32_t sum32 = 0;
    uint32_t x;
    uint32_t y;
    uint16_t value16;
    uint16_t checksum;
    uint16_t current_checksum;
    uint8_t *ptr;
    unsigned i;
    const unsigned CHECKSUM_OFFSET = 10;

    // Save current checksum, zero out value temporarily
    current_checksum = rutils_stream_to_word16(&header_start_ptr[CHECKSUM_OFFSET]);
    rutils_word16_to_stream(&header_start_ptr[CHECKSUM_OFFSET], 0);

    ptr = header_start_ptr;

    for (i = 0; i < IPV4_HEADER_SIZE/BYTES_PER_WORD16; i++)
    {
        value16 = rutils_stream_to_word16(ptr);
        ptr += BYTES_PER_WORD16;

        sum32 += value16;
    }

    // Restore current checksum
    rutils_word16_to_stream(&header_start_ptr[CHECKSUM_OFFSET], current_checksum);

    // Bits 19:16 are carry: take them and add them back in
    x = sum32 & BIT_MASK16;
    x += sum32 >> BITS_PER_WORD16;

    // If carry that was added back in causes sum to exceed 16 bits,
    //  add back in again.
    y = x >> BITS_PER_WORD16;
    x &= BIT_MASK16;
    x += y;

    // Flip every bit
    checksum = BITWISE_NOT16(x);

    return checksum;
}

//!
//! @name     rnet_ipv4_pseudo_header_struct_checksum
//!
//! @brief    Get IPv4 pseudo-header checksum contribution
//!
//! @details  Ref. RFC 768, 793, Wikipedia for IPv4 pseudo-header format
//! @details
//! @details          +--------+--------+--------+--------+
//! @details          |           Source Address          |
//! @details          +--------+--------+--------+--------+
//! @details          |         Destination Address       |
//! @details          +--------+--------+--------+--------+
//! @details          |  zero  |  PTCL  | TCP/UDP Length  |
//! @details          +--------+--------+--------+--------+
//!
//! @param[in] 'header'--
//!
//! @return    checksum
//!
uint16_t rnet_ipv4_pseudo_header_struct_checksum(rnet_ipv4_header_t *header)
{
    uint16_t running_checksum;
    uint8_t  fields[2];
    uint16_t udp_length;

    running_checksum = rnet_ip_running_checksum(0,
                                                header->src_addr,
                                                IPV4_ADDR_SIZE);
    running_checksum = rnet_ip_running_checksum(running_checksum,
                                                header->dest_addr,
                                                IPV4_ADDR_SIZE);

    fields[0] = 0;
    fields[1] = header->ip_protocol;

    running_checksum = rnet_ip_running_checksum(running_checksum,
                                                fields,
                                                sizeof(fields));

    // Technically, this is supposed to be the actual UDP length
    // field (for a UDP IPv4 compressed header), but cut corners a bit
    // here and assume IPv4 length and UDP length fields are same
    // minus IPv4 header size.
    // Doing this allows us to share this fcn. with TCP
    udp_length = header->total_length - IPV4_HEADER_SIZE;
    rutils_word16_to_stream(fields, udp_length);

    running_checksum = rnet_ip_running_checksum(running_checksum,
                                                fields,
                                                sizeof(fields));

    return running_checksum;
}

//!
//! @name     rnet_ipv6_pseudo_header_struct_checksum
//!
//! @brief    Get IPv6 pseudo-header checksum contribution
//!
//! @details  Ref. RFC 768, 793, Wikipedia for IPv4 pseudo-header format
//! @details
//! @details          +--------+--------+--------+--------+
//! @details          |           Source Address          |
//! @details          +--------+--------+--------+--------+
//! @details          |         Destination Address       |
//! @details          +--------+--------+--------+--------+
//! @details          |               length              |
//! @details          +--------+--------+--------+--------+
//! @details          |          zero   |   next header   |
//! @details          +--------+--------+--------+--------+
//!
//! @param[in] 'header'--
//!
//! @return    checksum
//!
uint16_t rnet_ipv6_pseudo_header_struct_checksum(rnet_ipv6_header_t *header)
{
    uint16_t running_checksum;
    uint8_t  fields[2];

    running_checksum = rnet_ip_running_checksum(0,
                                                header->src_addr,
                                                IPV6_ADDR_SIZE);
    running_checksum = rnet_ip_running_checksum(running_checksum,
                                                header->dest_addr,
                                                IPV6_ADDR_SIZE);

    fields[0] = (uint8_t)(header->payload_length >> BITS_PER_WORD8);
    fields[1] = (uint8_t)(header->payload_length & BIT_MASK8);

    running_checksum = rnet_ip_running_checksum(running_checksum,
                                                fields,
                                                sizeof(fields));

    fields[0] = 0;
    fields[1] = header->ip_protocol;

    running_checksum = rnet_ip_running_checksum(running_checksum,
                                                fields,
                                                sizeof(fields));

    return running_checksum;
}

//!
//! @name      rnet_ip_running_checksum
//!
//! @brief     Add part/all of a stream which contributes to an IP checksum
//!
//! @details   Checksum used in UDP, ICMP headers
//! @details   (See RFC 1071)
//! @details   Accumulates the checksum in a single or in successive calls.
//! @details   If done in successive calls, must take return value from 
//! @details   last call and pass it into next call.
//!
//! @param[in] 'running_sum'-- set to 0 if first time called, and set to
//! @param[in]      last return of 'rnet_ip_running_checksum' otherwise.
//! @param[in] 'stream'-- Bytes to include in checksum, this time
//! @param[in] 'length'-- Length of 'stream'
//!
//! @return    Updated checksum
//!
uint16_t rnet_ip_running_checksum(uint16_t  running_sum,
                                  uint8_t  *stream,
                                  unsigned  length)
{
    unsigned paired_length;
    unsigned i;
    uint16_t single_pair;
    bool     is_misaligned_length;

    paired_length = length / BYTES_PER_WORD16;

    // Handle the 16-bit pairs first...
    for (i = 0; i < paired_length; i++)
    {
        single_pair = rutils_stream_to_word16(stream);

        running_sum += single_pair;
        // Check for overflow, and add carry if overflow occured
        if (running_sum < single_pair)
        {
            running_sum++;
        }

        stream += BYTES_PER_WORD16;
    }

    is_misaligned_length = !((bool)IS_ALIGNED16(length));

    // ..and if there is an extra byte at the end, handle it.
    if (is_misaligned_length)
    {
        single_pair = (uint16_t)(*stream);
        single_pair <<= BITS_PER_WORD8;

        running_sum += single_pair;
        // Check for overflow again
        if (running_sum < single_pair)
        {
            running_sum++;
        }
    }

    return running_sum;
}

//!
//! @name      rnet_ip_finalize_checksum
//!
//! @brief     Finish IP checksum calculation
//!
//! @param[in] 'running_sum'-- set to 0 if first time called, and set to
//!
//! @return    Final checksum
//!
uint16_t rnet_ip_finalize_checksum(uint16_t running_sum)
{
    running_sum = (uint16_t)BITWISE_NOT16(running_sum);

    // A zero value is reserved, means "no checksum used"
    if (0 == running_sum)
    {
        running_sum = 0xFFFF;
    }

    return running_sum;
}

//!
//! @name      rnet_ip_pcl_add_data_to_checksum
//!
//! @brief     Add data from a pcl chain to a running checksum
//!
//! @param[in] 'running_sum'-- Previously accumulated checksum
//! @param[in] 'head_pcl'-- 
//! @param[in] 'start_ptr'-- Current data pointer which indicates
//! @param[in]       where to start checksum. Must point within
//! @param[in]       first pcl of chain.
//! @param[in] 'data_length'-- bytes to include in checksum
//!
//! @return    Updated checksum. Zero if error.
//!
uint16_t rnet_ip_pcl_add_data_to_checksum(uint16_t    running_sum,
                                          nsvc_pcl_t *head_pcl,
                                          uint8_t    *start_ptr,
                                          unsigned    data_length)
{
    nsvc_pcl_chain_seek_t read_posit;
    uint8_t               temp_buffer[TEMP_BUFFER_SIZE];
    uint8_t              *base_ptr;
    unsigned              start_offset;
    unsigned              read_length;
    unsigned              bytes_read;

    // 'base_ptr' is zero offset pointer within chain
    base_ptr = (uint8_t *)NSVC_PCL_HEADER(head_pcl);

    start_offset = (unsigned)(start_ptr - base_ptr);

    // Sanity check: is 'start_ptr' in 1st pcl?
    if ((base_ptr > start_ptr) || (start_offset >= NSVC_PCL_SIZE))
    {
        return 0;
    }

    // Set seek struct to agree with 'start_ptr'
    read_posit.current_pcl = head_pcl;
    read_posit.offset_in_pcl = start_offset;

    // Loop through expanse of data
    while (data_length > 0)
    {
        // Not final 'temp_buffer'?
        if (data_length <= sizeof(temp_buffer))
        {
            read_length = data_length;
        }
        // ..else, final 'temp_buffer'. Less than capacity
        else
        {
            read_length = sizeof(temp_buffer);
        }

        // Read data into 'temp_buffer'
        bytes_read = nsvc_pcl_read(&read_posit, temp_buffer, read_length);
        if (bytes_read != read_length)
        {
            return 0;
        }

        // Accumulate data into checksum
        running_sum = rnet_ip_running_checksum(running_sum,
                                               temp_buffer,
                                               read_length);

        data_length -= read_length;
    }

    return running_sum;
}