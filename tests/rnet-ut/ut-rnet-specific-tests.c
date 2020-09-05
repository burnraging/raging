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

#include <stdlib.h>         // rand()

#include "raging-global.h"
#include "raging-contract.h"
#include "raging-utils.h"
#include "raging-utils-mem.h"
#include "raging-utils-crc.h"
#include "nsvc-api.h"

#include "rnet-ip.h"
#include "rnet-ip-utils.h"
#include "rnet-dispatch.h"
#include "rnet-ahdlc.h"
#include "rnet-udp.h"
#include "rnet-app.h"
#include "nufr-platform.h"

#include "ut-rnet-test-vectors.h"
#include "../../arm-projects/disco/inc/rx-driver.h"

//#define IPV4_ADDR_STR1  "1.2.3.4"
//#define IPV4_ADDR_STR1  "1.210.3.4"
//#define IPV4_ADDR_STR1  "1.210.3.40"
#define IPV4_ADDR_STR1  "100.2.3.4"

void ut_rnet_ipv4_addr_ascii(void)
{
    int status;
    unsigned write_length;
    bool differences;
    char ascii_ip_addr[IPV4_ADDR_ASCII_SIZE + 1];   // +1 for '\0'
    rnet_ip_addr_union_t ip_addr;

    rutils_memset(&ip_addr, 0, sizeof(ip_addr));
    rutils_memset(ascii_ip_addr, 0xAA, sizeof(ascii_ip_addr));

    status = rnet_ipv4_ascii_to_binary(&ip_addr, IPV4_ADDR_STR1, true);

    write_length = rnet_ipv4_binary_to_ascii(&ip_addr,
                                             ascii_ip_addr,
                                             true);

    differences = rutils_strncmp(ascii_ip_addr, IPV4_ADDR_STR1, sizeof(IPV4_ADDR_STR1));

    UT_ENSURE(!differences);
}


//#define IPV6_ADDR_STR1  "20::10"
//#define IPV6_ADDR_STR1  "20:1::10"
//#define IPV6_ADDR_STR1  "2001:18::3:10"
//#define IPV6_ADDR_STR1  "2001:18::"
#define IPV6_ADDR_STR1  "::1:2"

void ut_rnet_ipv6_addr_ascii(void)
{
    int status;
    unsigned write_length;
    bool differences;
    char ascii_ip_addr[IPV6_ADDR_ASCII_SIZE + 1];   // +1 for '\0'
    rnet_ip_addr_union_t ip_addr;

    rutils_memset(&ip_addr, 0, sizeof(ip_addr));
    rutils_memset(ascii_ip_addr, 0xAA, sizeof(ascii_ip_addr));

    status = rnet_ipv6_ascii_to_binary(&ip_addr, IPV6_ADDR_STR1, true);

    write_length = rnet_ipv6_binary_to_ascii(&ip_addr,
                                             ascii_ip_addr,
                                             true);

    differences = rutils_strncmp(ascii_ip_addr, IPV6_ADDR_STR1, sizeof(IPV6_ADDR_STR1));

    UT_ENSURE(!differences);
}


void ut_rnet_crc16_test(void)
{
    uint16_t calculated_crc;

    uint8_t test_array[] = { 0xaa, 0x23 };

    calculated_crc = rutils_crc16_buffer(test_array, sizeof(test_array));
}

// IPv4 header (from wikipedia)
// 4500 0073 0000 4000 4011 b861 c0a8 0001
// c0a8 00c7 0035 e97c 005f 279f 1e4b 8180
// checksum is b861 (according to wikipedia)
//
uint8_t ut_ipv4_header[] =
    {0x45, 0x00, 0x00, 0x73, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11, 0xb8, 0x61, 0xc0, 0xa8, 0x00, 0x01,
     0xc0, 0xa8, 0x00, 0xc7, 0x00, 0x35, 0xe9, 0x7c, 0x00, 0x5f, 0x27, 0x9f, 0x1e, 0x4b, 0x81, 0x80};

void ut_ipv4_checksum_test(void)
{
    uint16_t calculated_checksum;
    
    calculated_checksum = rnet_ipv4_checksum(ut_ipv4_header);
}

//
// data:
//4500 + 0030 + 4422 + 4000 + 8006 + 0000 + 8c7c + 19ac + ae24 + 1e2b
// partial checksum: bbd1
uint8_t ut_partial_l4_checksum[] = {
    0x45, 0x00, 0x00, 0x30, 0x44, 0x22, 0x40, 0x00, 0x80, 0x06,
    0x00, 0x00, 0x8c, 0x7c, 0x19, 0xac, 0xae, 0x24, 0x1e, 0x2b
};

void ut_l4_checksum_partial_result(void)
{
    uint16_t           checksum;
    rnet_ipv4_header_t header;

    rnet_ipv4_deserialize_header(&header, ut_partial_l4_checksum);

    checksum = rnet_ipv4_pseudo_header_struct_checksum(&header);

//    checksum = rnet_ip_running_checksum(0,
//                                        ut_partial_l4_checksum,
//                                        sizeof(ut_partial_l4_checksum));
}

// For the UT_VECTOR_IPV4_UDP_INTERNET test packet,
//    to calculate the UDP checksum:
//
// Take the following fields from the packet contai ning 42 bytes of data shown in figure 4 above. 
//   All calculations are done using the hex values. 
//
//Field                                         Hex value                                           
//IP header: Source IP address                    c0a8 
//...                                             0291                                               
//IP header: Destination IP address               c0a8 
//...                                             0101                                               
//IP header: Protocol number(zero padded on left) 0011 
//16 bit UDP Length                               0032 
//UDP header: source port                         0618 
//UDP header: destination port                    0035 
//UDP header: length                              0032 
//UDP Data                                        0001 
//...                                             0100                                               
//                                                0001                                                
//                                                0000                                                
//                                                0000                                                
//                                                0000                                                
//                                                0131                                                
//                                                0131                                                
//                                                0331                                                
//                                                3638                                                
//                                                0331                                                
//                                                3932                                                
//                                                0769                                                
//                                                6e2d                                                
//                                                6164                                                
//                                                6472                                                
//                                                0461                                                
//                                                7270                                                
//                                                6100                                                
//                                                000c                                                
//                                                0001                                                
//Sum all hex values                              181e 
//Carry                                              4 
//Add in the carry                                1822 
//1s complement = checksum!                       E7dd 
//
void ut_ipv4_upd_packet_l4_checksum(void)
{
    uint8_t *ptr;
    unsigned packet_length;
    bool     rv;
    uint16_t             l4_checksum_sent;
    uint16_t             l4_checksum_calculated;
    unsigned             l4_checksum_offset;
    uint8_t             *l4_offset_ptr;
    rnet_ipv4_header_t   header;

    ptr = ut_fetch_test_vector(UT_VECTOR_IPV4_UDP_INTERNET, &packet_length);
    rv = rnet_ipv4_deserialize_header(&header, ptr);
    ptr += IPV4_HEADER_SIZE;

    l4_checksum_offset = rnet_ip_l4_checksum_offset(
                            rnet_ip_l4_ph_to_ip_protocol(header.ip_protocol));
    l4_offset_ptr = ptr + l4_checksum_offset;
    l4_checksum_sent = rutils_stream_to_word16(l4_offset_ptr);

    // Mask over L4 checksum, so it doesn't mess up calculation
    rutils_word16_to_stream(l4_offset_ptr, 0);

    l4_checksum_calculated = rnet_ipv4_pseudo_header_struct_checksum(&header);

    l4_checksum_calculated =
                rnet_ip_running_checksum(l4_checksum_calculated,
                                         ptr,
                                         header.total_length - IPV4_HEADER_SIZE);
    l4_checksum_calculated = BITWISE_NOT16(l4_checksum_calculated);

    // Restore checksum, so test vector won't stay corrupted
    rutils_word16_to_stream(l4_offset_ptr, l4_checksum_sent);

    // success if 'rv' == true
    rv = l4_checksum_calculated == l4_checksum_sent;
}

void consume_message(void)
{
    uint32_t    fields = 0;
    uint32_t    parameter = 0;

    nufr_msg_getW(&fields, &parameter);
}


// this generates an escape char in CRC
uint8_t ahdlc_test_packet[] = {
    0x1, 0x2, 0x7e, 0x3, 0x4, 0x5, 0x7d, 0x7e, 0x6, 0x7e
};

uint8_t problem_packet1[] = {
    0x71, 0x9a, 0xb0, 0xec, 0x35, 0x0f, 0x4e, 0x59, 0x50, 0xd6,
    0xf8, 0xa3, 0xde, 0x27, 0x55, 0xe0, 0x6c, 0xeb, 0xf7, 0x6b,
    0xbb, 0x74, 0x3d, 0x36, 0xc1, 0x6c, 0x77, 0x61
};
uint8_t problem_packet2[] = {
    0xeb, 0x63, 0xbf, 0x52, 0xd8, 0xf8, 0x31, 0xa8, 0x0e, 0xe8, 0xe7, 0x34, 0xa1, 0xcd, 0xa4, 0xfa,
    0x92, 0x6e, 0xa0, 0xa8, 0xe9, 0x89, 0x35, 0x79, 0x03, 0xd0, 0xdb, 0x59, 0x2e, 0x31, 0x56, 0x4a,
    0x73, 0x5a, 0x2e, 0x06, 0xb3, 0x7d, 0x60, 0xe6, 0xf6, 0x50, 0x53, 0x02, 0xff, 0x59, 0x0d, 0x9a,
    0x24, 0x68, 0xa7, 0xb5, 0x5c, 0x65, 0xfe, 0x07, 0x9a, 0x4b, 0xac, 0x18, 0x59, 0x75, 0x37, 0xa3,
    0x78, 0x19, 0x87, 0x3c, 0x49, 0x12, 0x9e, 0x36, 0xe5, 0xe2, 0x83, 0xc4, 0xc2, 0xf7, 0x7f, 0x5e,
    0x87, 0x2d, 0x8c, 0x67, 0x1e, 0x95, 0x0b, 0x0c, 0x0b, 0x75, 0xb5, 0x6f, 0xfd, 0x8f, 0xd4, 0x05,
    0xa3, 0xa6, 0xb3, 0x3c, 0xc1, 0x3f, 0x53, 0x62, 0x83, 0xa5, 0x5f, 0xc2, 0x11, 0x2f, 0x62, 0x10,
    0x63, 0xc5, 0x38, 0x07, 0x57
};

uint8_t packet_reference[2000];
uint8_t pcl_compare_copy[2000];
unsigned packet_reference_size;

// CRC=0x303C for length=256
void build_fixed_reference_packet(unsigned length)
{
    unsigned i;

    rutils_memset(packet_reference, 0, sizeof(packet_reference));

    for (i = 0; i < length; i++)
    {
        packet_reference[i] = (uint8_t)i;
    }
    packet_reference_size = i;
}

void build_random_reference_packet(void)
{
    uint16_t random_value;
    uint16_t max_length;
    uint16_t length;
    unsigned i;
    uint8_t  character;

    // Generate random length
    max_length = RNET_BUF_SIZE - 500 - PPP_PREFIX_LENGTH;
    random_value = rand();

    length = rutils_normalize_to_range(random_value, max_length, 0);

    rutils_memset(packet_reference, 0, sizeof(packet_reference));

    // Fill with random chars
    for (i = 0; i < length; i++)
    {
        random_value = rand();
        character = random_value & BIT_MASK8;

        packet_reference[i] = character;
    }

    packet_reference_size = length;
}

#define STR_PLUS_LEN(x)  x, sizeof(x)

void copy_fixed_string_to_reference_packet(const uint8_t *string,
                                           unsigned string_length)
{
    rutils_memset(packet_reference, 0, sizeof(packet_reference));

    rutils_memcpy(packet_reference, string, string_length);

    packet_reference_size = string_length;
}

void load_reference_packet_to_buf(rnet_buf_t *packet)
{
    uint8_t *ptr;

    packet->header.offset = PPP_PREFIX_LENGTH;
    packet->header.length = packet_reference_size;

    ptr = RNET_BUF_FRAME_START_PTR(packet);

    rutils_memcpy(ptr, packet_reference, packet_reference_size);
}

void load_reference_packet_to_pcl(nsvc_pcl_t **head_pcl_ptr)
{
    nsvc_pcl_header_t    *header;
    nufr_sema_get_rtn_t   alloc_rv;
    nsvc_pcl_chain_seek_t write_posit;
    unsigned              offset;
    bool                  rv;

    offset = NSVC_PCL_OFFSET_PAST_HEADER(PPP_PREFIX_LENGTH);

    // Allocate a chain large enough to handle the reference packet.
    // Size to allocate needs to take into account the offset.
    alloc_rv = nsvc_pcl_alloc_chainWT(head_pcl_ptr,
                                      NULL,
                                      packet_reference_size + offset,
                                      NSVC_PCL_NO_TIMEOUT);
    UT_ENSURE(NUFR_SEMA_GET_OK_NO_BLOCK == alloc_rv);

    header = NSVC_PCL_HEADER(*head_pcl_ptr);

    header->offset = offset;
    header->total_used_length = packet_reference_size;

    // Position to char before start of frame
    rv = nsvc_pcl_set_seek_to_headerless_offset(*head_pcl_ptr,
                                            &write_posit,
                                            header->offset);
    UT_ENSURE(rv);

    if (packet_reference_size > 0)
    {
        rv = nsvc_pcl_write_data_continue(&write_posit,
                                          packet_reference,
                                          packet_reference_size);
        UT_ENSURE(rv);
    }
}

#define ENABLE_RANDOM_PACKET

void ut_ahdlc_encode_decode_buf()
{
    rnet_buf_t *packet;
    uint8_t    *ptr;
    int         mismatch_offset;
    bool        same_length;
    bool        same_offset;
    bool        same_content;
    bool        same_all;
#ifdef ENABLE_RANDOM_PACKET
    unsigned    i;
#endif


#ifdef ENABLE_RANDOM_PACKET
    for (i = 0; i < 5000; i++) {
#endif

    packet = rnet_alloc_bufW();

#ifdef ENABLE_RANDOM_PACKET
//    build_random_reference_packet();
    build_fixed_reference_packet(256);
#else
//    copy_fixed_string_to_reference_packet(STR_PLUS_LEN(ahdlc_test_packet));
    copy_fixed_string_to_reference_packet(STR_PLUS_LEN(problem_packet1));

    ptr = RNET_BUF_FRAME_START_PTR(packet);
    rutils_memcpy(ptr, ahdlc_test_packet, sizeof(ahdlc_test_packet));
#endif

    load_reference_packet_to_buf(packet);

    rnet_msg_tx_buf_ahdlc_crc(packet);
    consume_message();

    rnet_msg_tx_buf_ahdlc_encode_cc(packet);
    consume_message();

    rnet_msg_rx_buf_ahdlc_strip_cc(packet);
    consume_message();

    rnet_msg_rx_buf_ahdlc_verify_crc(packet);
    consume_message();

    ptr = RNET_BUF_FRAME_START_PTR(packet);

    mismatch_offset = rutils_memcmp(ptr, packet_reference, packet_reference_size);

    same_length = packet->header.length == packet_reference_size;
    same_offset = PPP_PREFIX_LENGTH == packet->header.offset;
    same_content = mismatch_offset < 0;
    same_all = same_length && same_offset && same_content;

    if (!same_all)
    {
        return;
    }

    rnet_free_buf(packet);

#ifdef ENABLE_RANDOM_PACKET
    }
#endif
}

void ut_ahdlc_encode_decode_pcl()
{
    nsvc_pcl_t *packet;
    nsvc_pcl_header_t *header;
    nsvc_pcl_chain_seek_t read_posit;
    bool        rv;
    unsigned    read_length;
    int         mismatch_offset;
    bool        same_length;
    bool        same_offset;
    bool        same_content;
    bool        same_all;
#ifdef ENABLE_RANDOM_PACKET
    unsigned    i;
#else
    nufr_sema_get_rtn_t alloc_rv;
#endif


#ifdef ENABLE_RANDOM_PACKET
    for (i = 0; i < 5000; i++) {
#endif

#ifdef ENABLE_RANDOM_PACKET
    build_random_reference_packet();
//    build_fixed_reference_packet(256);
#else
//    copy_fixed_string_to_reference_packet(STR_PLUS_LEN(ahdlc_test_packet));
//    copy_fixed_string_to_reference_packet(STR_PLUS_LEN(problem_packet1));
    copy_fixed_string_to_reference_packet(STR_PLUS_LEN(problem_packet2));

    alloc_rv = nsvc_pcl_alloc_chainWT(&packet, NULL, packet_reference_size, 0);
    UT_ENSURE(NUFR_SEMA_GET_OK_NO_BLOCK != alloc_rv);
#endif

    load_reference_packet_to_pcl(&packet);

    header = NSVC_PCL_HEADER(packet);

    rnet_msg_tx_pcl_ahdlc_crc(packet);
    consume_message();

    rnet_msg_tx_pcl_ahdlc_encode_cc(packet);
    consume_message();

    rnet_msg_rx_pcl_ahdlc_strip_cc(packet);
    consume_message();

    rnet_msg_rx_pcl_ahdlc_verify_crc(packet);
    consume_message();

    // Seek to start of frame
    rv = nsvc_pcl_set_seek_to_headerless_offset(packet,
                                            &read_posit,
                                            header->offset);
    UT_ENSURE(rv);

    read_length = nsvc_pcl_read(&read_posit,
                                 pcl_compare_copy,
                                 packet_reference_size);
    UT_ENSURE(read_length == packet_reference_size);

    mismatch_offset = rutils_memcmp(packet_reference, pcl_compare_copy, packet_reference_size);

    same_length = header->total_used_length == packet_reference_size;
    same_offset = NSVC_PCL_OFFSET_PAST_HEADER(PPP_PREFIX_LENGTH) == header->offset;
    same_content = mismatch_offset < 0;
    same_all = same_length && same_offset && same_content;

    if (!same_all)
    {
        return;
    }

    nsvc_pcl_free_chain(packet);

#ifdef ENABLE_RANDOM_PACKET
    }
#endif
}


void ut_rx_driver_test(void)
{
    uint8_t  *frame;
    unsigned frame_length;
    const unsigned PREALLOC_COUNT = 3;

    rx_handler_init(NUFR_SET_MSG_FIELDS(0,
                                        RNET_ID_RX_BUF_ENTRY,
                                        NUFR_TID_null,
                                        NUFR_MSG_PRI_MID),
                    NUFR_TID_null,
                    RNET_INTFC_TEST1);
    rx_handler_enqueue_buf(PREALLOC_COUNT);

    frame = ut_fetch_test_vector(UT_VECTOR_LCP_CONF_REQ, &frame_length);
    copy_fixed_string_to_reference_packet(frame, frame_length);

    rx_handler_for_ahdlc(packet_reference, packet_reference_size);
    rx_handler_for_ahdlc(packet_reference, packet_reference_size);
}