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

//! @file     ssp-assignments.h
//! @authors  Bernie Woodland
//! @date     19Dec18
//!
//! @brief    Simple Serial Protocol Driver
//!
//! @details  L3/Payload well-known values
//!

#ifndef SSP_ASSIGNMENTS_H_
#define SSP_ASSIGNMENTS_H_

//!
//! @name      SSP_DAPP_RESPONSE_OFFSET;
//!
//! @details   For a server request/response cycle, all
//! @details   response dapp values are a fixed offset from
//! @details   request values.
//! @details   
//!
#define SSP_DAPP_RESPONSE_OFFSET      127

//!
//! @enum      ssp_dapp_wellknowns_t;
//!
//! @details   dapp = destination app. First byte in payload
//!
//! @details   'CLEAR'== clear-text/unsecured
//!
typedef enum
{
    SSP_DAPP_CLEAR_ECHO_REQUEST = 1,    // Clear-text loopback
    SSP_DAPP_last = SSP_DAPP_CLEAR_ECHO_REQUEST,  // Last used

    SSP_DAPP_CLEAR_ECHO_RESPONSE = SSP_DAPP_CLEAR_ECHO_REQUEST + SSP_DAPP_RESPONSE_OFFSET,

} ssp_dapp_wellknowns_t;


//!
//! @name      SSP_DAPP_START_USER_DEFINED;
//!
//! @details   For a server request/response cycle, all
//! @details   response dapp values are a fixed offset from
//! @details   request values.
//! @details   
//!
#define SSP_DAPP_START_USER_DEFINED      127


//!
//! @enum      ssp_cir_wellknowns_t;
//!
//! @details   cir = peer circuit
//! @details   Connections to other IP address/UDP port pairs
//!
typedef enum
{
    SSP_CIR_PRIMARY_PEER = 1,    // Single peer default
} ssp_cir_wellknowns_t;

#endif  //#ifndef SSP_ASSIGNMENTS_H_