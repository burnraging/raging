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

//! @file     ut-rnet-test-vectors.h
//! @authors  Bernie Woodland
//! @date     5May18
//!
//! @brief   RNET test vectors
//!

#ifndef UT_RNET_TEST_VECTORS_H_
#define UT_RNET_TEST_VECTORS_H_

#include "raging-global.h"

typedef enum
{
    UT_VECTOR_LCP_CONF_REQ,
    UT_VECTOR_WIRESHARK_LCP_CONF_REQ,
    UT_VECTOR_WIRESHARK_LCP_CONF_ACK,
    UT_VECTOR_WIRESHARK_IPCP_CONF_REQ,
    UT_VECTOR_WIRESHARK_IPCP_CONF_ACK,
    UT_VECTOR_WIRESHARK_LCP_PROTOCOL_REJECT,
    UT_VECTOR_IPV4_UDP_INTERNET,
    UT_VECTOR_IPV4_UDP_CONF_POST,  // corrupted!
    UT_VECTOR_IPV6_UDP_COAP_ACK,
    UT_VECTOR_ICMP_ECHO_REQUEST,
    UT_VECTOR_ICMP_ECHO_REPLY,
    UT_VECTOR_ICMPV6_ECHO_REQUEST,
    UT_VECTOR_ICMPV6_ECHO_REPLY,
    UT_VECTOR_SIMPLE,
} ut_test_vector_t;

// APIs
uint8_t *ut_fetch_test_vector(ut_test_vector_t which_vector,
                              unsigned        *length_ptr);

#endif  // UT_RNET_TEST_VECTORS_H_