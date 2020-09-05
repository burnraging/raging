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

//! @file     ut-rnet-test-vectors.c
//! @authors  Bernie Woodland
//! @date     5May18
//!
//! @brief    Test packets for RNET
//!

#include "ut-rnet-test-vectors.h"

// PPP LCP config request wrapped in full AHDLC encapsulation
// Has single option, 'magic number'==11111111
// UT_VECTOR_LCP_CONF_REQ
uint8_t ut_test_vector_lcp_config_request[] = {
    0x7E, 0xFF, 0x03, 0xC0, 0x21, 0x01, 0x00, 0x00, 0x0A, 0x05,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x66, 0xA7, 0x7E
};
// From Wireshark .pcap files on wireshark web page:

// Wireshark LCP Conf request
// UT_VECTOR_WIRESHARK_LCP_CONF_REQ
uint8_t ut_test_vector_wireshark_lcp_config_request[] = {
    0xff, 0x03, 0xc0, 0x21, 0x01, 0x04, 0x00, 0x0a, 0x02, 0x06,
    0x00, 0x0a, 0x00, 0x00, 0x3a, 0x7a
};

// Wireshark LCP conf ack (to UT_VECTOR_WIRESHARK_LCP_CONF_REQ/above)
// UT_VECTOR_WIRESHARK_LCP_CONF_ACK
uint8_t ut_test_vector_wireshark_lcp_config_ack[] = {
    0xff, 0x03, 0xc0, 0x21, 0x02, 0x04, 0x00, 0x0a, 0x02, 0x06,
    0x00, 0x0a, 0x00, 0x00, 0x53, 0x0e
};

// Wireshark IPCP conf request
// UT_VECTOR_WIRESHARK_IPCP_CONF_REQ
uint8_t ut_test_vector_wireshark_ipcp_config_request[] = {
    0xff, 0x03, 0x80, 0x21, 0x01, 0x00, 0x00, 0x0a, 0x03, 0x06,
    0x44, 0x1c, 0x71, 0x55, 0xb7, 0xcd
};

// Wireshark IPCP conf ack
// UT_VECTOR_WIRESHARK_IPCP_CONF_ACK
uint8_t ut_test_vector_wireshark_ipcp_config_ack[] = {
    0xff, 0x03, 0x80, 0x21, 0x02, 0x00, 0x00, 0x0a, 0x03, 0x06,
    0x44, 0x1c, 0x71, 0x55, 0xde, 0xb9
};

// Wireshark LCP protocol reject
// UT_VECTOR_WIRESHARK_LCP_PROTOCOL_REJECT
uint8_t ut_test_vector_wireshark_lcp_protocol_reject[] = {
    0xff, 0x03, 0xc0, 0x21, 0x08, 0x03, 0x00, 0x12, 0x80, 0xfd,
    0x01, 0x01, 0x00, 0x0c, 0x1a, 0x04, 0x78, 0x00, 0x18, 0x04,
    0x78, 0x00, 0xcd, 0x2c
};

// Taken from the Internet:
// A UDP over IPv4 packet minus the ethernet header
// UT_VECTOR_IPV4_UDP_INTERNET
uint8_t ut_internet_udp_packet[] = {
        // IPv4 header
        // Src addr: 192.168.2.145
        // Dest addr: 192.168.1.1
    0x45, 0x00, 0x00, 0x46, 0x5e, 0x71, 0x00, 0x00,
    0x80, 0x11, 0x57, 0x53, 0xc0, 0xa8, 0x02, 0x91,
    0xc0, 0xa8, 0x01, 0x01,
        // UDP header
        // Src port: 1560
        // Dest port: 53
        // length: 50
    0x06, 0x18, 0x00, 0x35, 0x00, 0x32, 0xe7, 0xdd,
    0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        // Payload
    0x00, 0x00, 0x01, 0x31, 0x01, 0x31, 0x03, 0x31, 0x36, 0x38,
    0x03, 0x31, 0x39, 0x32, 0x07, 0x69, 0x6e, 0x2d, 0x61, 0x64,
    0x64, 0x72, 0x04, 0x61, 0x72, 0x70, 0x61, 0x00, 0x00, 0x0c,
    0x00, 0x01
};

// Non-confirmable post with GPB payload
// CoAP over UDP over IPv4
// UT_VECTOR_IPV4_UDP_CONF_POST
// FIXME!!! THIS PACKET IS CORRUPTED! L4 CHECKSUM FAILS!
uint8_t ut_test_vector_ipv4_udp_non_conf_post_gpb[] = {
        // IPv4 header
        // Src addr: 192.168.144.11
        // Dest addr: 192.168.144.31
    0x45, 0x00, 0x00, 0x67, 0x3E, 0x80, 0x00, 0x00, 0x80, 0x11,
    0x00, 0x00, 0xC0, 0xA8, 0x90, 0x0B, 0xC0, 0xA8, 0x90, 0x1F,
    0xFD, 0xCC, 0xF0, 0xB8, 0x00, 0x53, 0xA1, 0xE0,                // UDP header
    0x51, 0x02, 0x06, 0x13, 0xC4, 0x3D,                            // CoAP header (??)
                                                                   // GPB payload
    0x0B, 0x72, 0x61, 0x6C, 0x2D, 0x72, 0x64, 0x2D, 0x7A, 0x69, 0x67, 0x62, 0x65, 0x65, 0x31, 0x2E,
    0x69, 0x74, 0x72, 0x6F, 0x6E, 0x2E, 0x63, 0x6F, 0x6D, 0x42, 0xF0, 0xB8, 0x4D, 0x00, 0x49, 0x74,
    0x72, 0x6F, 0x6E, 0x4E, 0x4D, 0x53, 0x41, 0x67, 0x65, 0x6E, 0x74, 0x01, 0x63, 0x03, 0x63, 0x61,
    0x6D, 0x01, 0x32, 0x11, 0x2A, 0xFF, 0x12, 0x0D, 0x08, 0x3C, 0x12, 0x09, 0x01, 0x04, 0x38, 0x39,
    0x3A, 0x37, 0x3C, 0x3D, 0x02
};

// CoAP ack over UDP over IPv6
//  src address =  2620:CB:0:1006:1480:100:0:292
//  dest address = 2620:CB:0:B063::1947
// UT_VECTOR_IPV6_UDP_COAP_ACK
uint8_t ut_test_vector_ipv6_udp_coap_ack[] = {
    0x60, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x11, 0x40, 0x26, 0x20,    // IPv6 header
    0x00, 0xCB, 0x00, 0x00, 0x10, 0x06, 0x14, 0x80, 0x01, 0x00,
    0x00, 0x00, 0x02, 0x92, 0x26, 0x20, 0x00, 0xCB, 0x00, 0x00,
    0xB0, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x47,
    0xF0, 0xB8, 0xF0, 0xB9, 0x00, 0x0C, 0x98, 0x68,                // UDP header
    0x60, 0x80, 0xE5, 0xE1                                         // CoAP ack
};

// ICMPv4 Pings from:
//http://www.hackingarticles.in/understanding-guide-icmp-protocol-wireshark/

//
// ICMPv4 ping request /w ethernet header
// length = 75, src=192.168.0.104, dest=192.168.0.105
// Identifier (BE): 1 (0x0001)
// Identifier (LE): 256 (0x0100)
// Sequence number (BE): 423 (0x01a7)
// Sequence number (LE): 42753 (0xa701)
// 33 bytes data (ICMP payload size)
//
// 00 0c 29 19 c2 8b fc aa    14 f2 d1 2a 08 00 45 00
// 00 3d 7d d1 00 00 80 01    3a cd c0 a8 00 68 c0 a8
// 00 69 08 00 e1 b3 00 01    01 a7 61 62 63 64 65 66
// 67 68 69 6a 6b 6c 6d 6e    6f 70 71 72 73 74 75 76
// 77 61 62 63 64 65 66 67    68 69 6a
//
//  UT_VECTOR_ICMP_ECHO_REQUEST
uint8_t ut_test_vector_icmpv4_echo_request[] = {
            // IPv4 header
                                                                                        0x45, 0x00,
    0x00, 0x3d, 0x7d, 0xd1, 0x00, 0x00, 0x80, 0x01, 0x3a, 0xcd, 0xc0, 0xa8, 0x00, 0x68, 0xc0, 0xa8,
    0x00, 0x69,
            // ICMP header
                0x08, 0x00, 0xe1, 0xb3, 0x00, 0x01, 0x01, 0xa7,
            // Payload
                                                                0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
    0x77, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a
};

// ICMPv4 ping reply, /w ethernet header
//  (not to request above)
// length = 74, src=192.168.0.105, dest=192.168.0.104
// Identifier (LE): 256 (0x0100)
// Sequence number (BE): 430 (0x01ae)
// Sequence number (LE): 44545 (0xae01)
// 32 bytes data (ICMP payload size)
//
// fc aa 14 f2 d1 2a 00 0c   29 19 c2 8b 08 00 45 00
// 00 3c 04 70 00 00 80 01   b4 2f c0 a8 00 69 c0 a8
// 00 68 00 00 53 ad 00 01   01 ae 61 62 63 64 65 66
// 67 68 69 6a 6b 6c 6d 6e   6f 70 71 72 73 74 75 76
// 77 61 62 63 64 65 66 67   68 69
//
// UT_VECTOR_ICMP_ECHO_REPLY
uint8_t ut_test_vector_icmpv4_echo_reply[] = {
            // IPv4 header
                                                                                        0x45, 0x00,
    0x00, 0x3c, 0x04, 0x70, 0x00, 0x00, 0x80, 0x01, 0xb4, 0x2f, 0xc0, 0xa8, 0x00, 0x69, 0xc0, 0xa8,
    0x00, 0x68,
            // ICMP header
                0x00, 0x00, 0x53, 0xad, 0x00, 0x01, 0x01, 0xae,
            // Payload
                                                                0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
    0x77, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69
};


// https://sharkfestus.wireshark.org/sharkfest.12/presentations/BI-5_ICMPv6.pdf

//
// ICMPv6 echo request, /w ethernet header
//  src=2001:5c0:8fff:fffe::3f53   dest=2001:5c0:8fff:fffe::3f52
// ICMP header:
//    type: 128 (echo request)
//    checksum: 0xacdb
//    ID: 0x0000
//    Sequence: 0x0031
//    data (13 bytes)
//
// 00 ff 8d 10 39 76 00 ff   8c 10 39 76 86 dd 60 00
// 00 00 00 15 3a 80 20 01   05 c0 8f ff ff fe 00 00
// 00 00 00 00 3f 53 20 01   05 c0 8f ff ff fe 00 00
// 00 00 00 00 3f 52 80 00   ac db 00 00 00 31 64 65
// 66 67 68 69 6a 6b 6c 6d   6e 6f 70
//
// UT_VECTOR_ICMPV6_ECHO_REQUEST
uint8_t ut_test_vector_icmpv6_echo_request[] = {
            // IPv6 header
                                                                                        0x60, 0x00,
    0x00, 0x00, 0x00, 0x15, 0x3a, 0x80, 0x20, 0x01, 0x05, 0xc0, 0x8f, 0xff, 0xff, 0xfe, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3f, 0x53, 0x20, 0x01, 0x05, 0xc0, 0x8f, 0xff, 0xff, 0xfe, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3f, 0x52,
            // ICMPv6 header
                                        0x80, 0x00, 0xac, 0xdb, 0x00, 0x00, 0x00, 0x31,
            // Payload
                                                                                        0x64, 0x65,
    0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70
};

// ICMPv6 echo reply, /w ethernet header
// (not to request above)
//   src=2001:5c0:8fff:fffe::3f52, dest=2001:5c0:8fff:fffe::3f53
// ICMP header:
//    type: 129 (echo reply)
//    code: 0
//    checksum: 0xabda
//    id: 0x0000
//    sequence: 0x0032
//    data (13 bytes)
//
// 00 ff 8c 10 39 76 00 ff   8d 10 39 76 86 dd 60 00
// 00 00 00 15 3a 40 20 01   05 c0 8f ff ff fe 00 00
// 00 00 00 00 3f 52 20 01   05 c0 8f ff ff fe 00 00
// 00 00 00 00 3f 53 81 00   ab da 00 00 00 32 64 65
// 66 67 68 69 6a 6b 6c 6d   6e 6f 70
//
// UT_VECTOR_ICMPV6_ECHO_REPLY
uint8_t ut_test_vector_icmpv6_echo_reply[] = {
            // IPv6 header
                                                                                        0x60, 0x00,
    0x00, 0x00, 0x00, 0x15, 0x3a, 0x40, 0x20, 0x01, 0x05, 0xc0, 0x8f, 0xff, 0xff, 0xfe, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3f, 0x52, 0x20, 0x01, 0x05, 0xc0, 0x8f, 0xff, 0xff, 0xfe, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3f, 0x53,
            // ICMPv6 header
                                        0x81, 0x00, 0xab, 0xda, 0x00, 0x00, 0x00, 0x32,
            // Payload
                                                                                        0x64, 0x65,
    0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70
};

// UT_VECTOR_SIMPLE
uint8_t ut_test_vector_simple[] = {
    0x01, 0x02, 0x03
};

uint8_t *ut_fetch_test_vector(ut_test_vector_t which_vector,
                              unsigned        *length_ptr)
{
     uint8_t *data_ptr;

    switch (which_vector)
    {
    case UT_VECTOR_LCP_CONF_REQ:
        data_ptr = ut_test_vector_lcp_config_request;
        *length_ptr = sizeof(ut_test_vector_lcp_config_request);
        break;
    case UT_VECTOR_WIRESHARK_LCP_CONF_REQ:
        data_ptr = ut_test_vector_wireshark_lcp_config_request;
        *length_ptr = sizeof(ut_test_vector_wireshark_lcp_config_request);
        break;
    case UT_VECTOR_WIRESHARK_LCP_CONF_ACK:
        data_ptr = ut_test_vector_wireshark_lcp_config_ack;
        *length_ptr = sizeof(ut_test_vector_wireshark_lcp_config_ack);
        break;
    case UT_VECTOR_WIRESHARK_IPCP_CONF_REQ:
        data_ptr = ut_test_vector_wireshark_ipcp_config_request;
        *length_ptr = sizeof(ut_test_vector_wireshark_ipcp_config_request);
        break;
    case UT_VECTOR_WIRESHARK_IPCP_CONF_ACK:
        data_ptr = ut_test_vector_wireshark_ipcp_config_ack;
        *length_ptr = sizeof(ut_test_vector_wireshark_ipcp_config_ack);
        break;
    case UT_VECTOR_WIRESHARK_LCP_PROTOCOL_REJECT:
        data_ptr = ut_test_vector_wireshark_lcp_protocol_reject;
        *length_ptr = sizeof(ut_test_vector_wireshark_lcp_protocol_reject);
        break;
    case UT_VECTOR_IPV4_UDP_INTERNET:
        data_ptr = ut_internet_udp_packet;
        *length_ptr = sizeof(ut_internet_udp_packet);
        break;
    case UT_VECTOR_IPV4_UDP_CONF_POST:
        data_ptr = ut_test_vector_ipv4_udp_non_conf_post_gpb;
        *length_ptr = sizeof(ut_test_vector_ipv4_udp_non_conf_post_gpb);
        break;
    case UT_VECTOR_IPV6_UDP_COAP_ACK:
        data_ptr = ut_test_vector_ipv6_udp_coap_ack;
        *length_ptr = sizeof(ut_test_vector_ipv6_udp_coap_ack);
        break;
    case UT_VECTOR_ICMP_ECHO_REQUEST:
        data_ptr = ut_test_vector_icmpv4_echo_request;
        *length_ptr = sizeof(ut_test_vector_icmpv4_echo_request);
        break;
    case UT_VECTOR_ICMP_ECHO_REPLY:
        data_ptr = ut_test_vector_icmpv4_echo_reply;
        *length_ptr = sizeof(ut_test_vector_icmpv4_echo_reply);
        break;
    case UT_VECTOR_ICMPV6_ECHO_REQUEST:
        data_ptr = ut_test_vector_icmpv6_echo_request;
        *length_ptr = sizeof(ut_test_vector_icmpv6_echo_request);
        break;
    case UT_VECTOR_ICMPV6_ECHO_REPLY:
        data_ptr = ut_test_vector_icmpv6_echo_reply;
        *length_ptr = sizeof(ut_test_vector_icmpv6_echo_reply);
        break;
    case UT_VECTOR_SIMPLE:
        data_ptr = ut_test_vector_simple;
        *length_ptr = sizeof(ut_test_vector_simple);
        break;
    default:
        data_ptr = NULL;
        *length_ptr = 0;
        break;
    }

    return data_ptr;
}