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

//! @file     rcoap-protocol.h
//! @authors  Bernie Woodland
//! @date     27Mar18
//!
//! @brief    CoAP protocol (RFC 7252) Encoding and Decoding
//!

#ifndef RCOAP_PROTOCOL_H_
#define RCOAP_PROTOCOL_H_

#include "raging-global.h"

// RFC sec. 17.1.1 "Method Codes". 0.0x values
typedef enum
{
    RCOAP_MC_GET = 1,      // 0.01
    RCOAP_MC_POST = 2,     // 0.02
    RCOAP_MC_PUT = 3,      // 0.03
    RCOAP_MC_DELETE = 4    // 0.04
} rcoap_method_code_t;

//  +------+------------------------------+-----------+
//  | Code | Description                  | Reference |
//  +------+------------------------------+-----------+
//  | 2.01 | Created                      | [RFC7252] |
//  | 2.02 | Deleted                      | [RFC7252] |
//  | 2.03 | Valid                        | [RFC7252] |
//  | 2.04 | Changed                      | [RFC7252] |
//  | 2.05 | Content                      | [RFC7252] |
//  | 4.00 | Bad Request                  | [RFC7252] |
//  | 4.01 | Unauthorized                 | [RFC7252] |
//  | 4.02 | Bad Option                   | [RFC7252] |
//  | 4.03 | Forbidden                    | [RFC7252] |
//  | 4.04 | Not Found                    | [RFC7252] |
//  | 4.05 | Method Not Allowed           | [RFC7252] |
//  | 4.06 | Not Acceptable               | [RFC7252] |
//  | 4.12 | Precondition Failed          | [RFC7252] |
//  | 4.13 | Request Entity Too Large     | [RFC7252] |
//  | 4.15 | Unsupported Content-Format   | [RFC7252] |
//  | 5.00 | Internal Server Error        | [RFC7252] |
//  | 5.01 | Not Implemented              | [RFC7252] |
//  | 5.02 | Bad Gateway                  | [RFC7252] |
//  | 5.03 | Service Unavailable          | [RFC7252] |
//  | 5.04 | Gateway Timeout              | [RFC7252] |
//  | 5.05 | Proxying Not Supported       | [RFC7252] |
//  +------+------------------------------+-----------+

// 2.xx Success Response Codes
typedef enum
{
    RCOAP_SRC_CREATED = 1,
    RCOAP_SRC_DELETED = 2,
    RCOAP_SRC_VALID = 3,
    RCOAP_SRC_CHANGED = 4,
    RCOAP_SRC_CONTENT = 5
} rcoap_src_t;

// 4.xx Client Error Response Codes
typedef enum
{
    RCOAP_ERC_BAD_REQUEST = 0,
    RCOAP_ERC_UNAUTHORIZED = 1,
    RCOAP_ERC_BAD_OPTION = 2,
    RCOAP_ERC_FORBIDDEN = 3,
    RCOAP_ERC_NOT_FOUND = 4,
    RCOAP_ERC_METHOD_NOT_ALLOWED = 5
} rcoap_erc_t;

// 5.xx Server Error Response Codes
typedef enum
{
    RCOAP_SEC_INTERNAL_SERVER_ERROR = 0,
    RCOAP_SEC_NOT_IMPLEMENTED = 1,
    RCOAP_SEC_BAD_GATEWAY = 2,
    RCOAP_SEC_SERVICE_UNAVAILABLE = 3,
    RCOAP_SEC_GATEWAY_TIMEOUT = 4,
    RCOAP_SEC_PROXYING_NOT_SUPPORTED = 5
} rcoap_sec_t;

typedef union
{
    rcoap_method_code_t method;            // 0.0x codes
    rcoap_src_t         success_code;      // 2.0x codes
    rcoap_erc_t         client_error_code; // 4.0x codes
    rcoap_sec_t         server_error_code; // 5.0x codes
} rcoap_response_t;

//  +--------+------------------+-----------+
//  | Number | Name             | Reference |
//  +--------+------------------+-----------+
//  |      0 | (Reserved)       | [RFC7252] |
//  |      1 | If-Match         | [RFC7252] |
//  |      3 | Uri-Host         | [RFC7252] |
//  |      4 | ETag             | [RFC7252] |
//  |      5 | If-None-Match    | [RFC7252] |
//  |      7 | Uri-Port         | [RFC7252] |
//  |      8 | Location-Path    | [RFC7252] |
//  |     11 | Uri-Path         | [RFC7252] |
//  |     12 | Content-Format   | [RFC7252] |
//  |     14 | Max-Age          | [RFC7252] |
//  |     15 | Uri-Query        | [RFC7252] |
//  |     17 | Accept           | [RFC7252] |
//  |     20 | Location-Query   | [RFC7252] |
//  |     35 | Proxy-Uri        | [RFC7252] |
//  |     39 | Proxy-Scheme     | [RFC7252] |
//  |     60 | Size1            | [RFC7252] |
//  |    128 | (Reserved)       | [RFC7252] |
//  |    132 | (Reserved)       | [RFC7252] |
//  |    136 | (Reserved)       | [RFC7252] |
//  |    140 | (Reserved)       | [RFC7252] |
//  +--------+------------------+-----------+

typedef enum
{
    RCOAP_OPT_IF_MATCH = 1,
    RCOAP_OPT_URI_HOST = 3,
    RCOAP_OPT_ETAG = 4,
    RCOAP_OPT_IF_NONE_MATCH = 5,
    RCOAP_OPT_URI_PORT = 7,
    RCOAP_OPT_LOCATION_PATH = 8,
    RCOAP_OPT_URI_PATH = 11,
    RCOAP_OPT_CONTENT_FORMAT = 12,
    RCOAP_OPT_MAX_AGE = 14,
    RCOAP_OPT_URI_QUERY = 15,
    RCOAP_OPT_ACCEPT = 17,
    RCOAP_OPT_LOCATION_QUERY = 20,
    RCOAP_OPT_PROXY_URI = 35,
    RCOAP_OPT_PROXY_SCHEME = 39,
    RCOAP_OPT_SIZE1 = 60,
} 

//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |Ver| T |  TKL  |      Code     |          Message ID           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   Token (if any, TKL bytes) ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   Options (if any) ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |1 1 1 1 1 1 1 1|    Payload (if any) ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  NOTE: bit 0 is MSB, bit 7 is LSB

#define RCOAP_MAX_TOKEN_LENGTH     8

typedef enum
{
    RCOAP_TYPE_CONF = 0,
    RCOAP_TYPE_NON_CONF = 1,
    RCOAP_TYPE_ACK = 2,
    RCOAP_TYPE_RESEET = 3
} rcoap_type_t;

typedef struct
{
    rcoap_type_t     type;
    uint8_t          token_length;
    rcoap_response_t response_code;
    uint16_t         message_id;
    uint8_t          token[RCOAP_MAX_TOKEN_LENGTH];
} rcoap_header_t;

//    0   1   2   3   4   5   6   7
//   +---------------+---------------+
//   |               |               |
//   |  Option Delta | Option Length |   1 byte
//   |               |               |
//   +---------------+---------------+
//   \                               \
//   /         Option Delta          /   0-2 bytes
//   \          (extended)           \
//   +-------------------------------+
//   \                               \
//   /         Option Length         /   0-2 bytes
//   \          (extended)           \
//   +-------------------------------+
//   \                               \
//   /                               /
//   \                               \
//   /         Option Value          /   0 or more bytes
//   \                               \
//   /                               /
//   \                               \
//   +-------------------------------+
//
//      RFC 7252     Figure 8: Option Format

// Max number of URI Path options allowed.
// This limits sub-paths in URI path strings
//   ex: "/a/b/c" will form 3 URI subpaths
#define RCOAP_MAX_URI_PATH                5

#endif  // RCOAP_PROTOCOL_H_