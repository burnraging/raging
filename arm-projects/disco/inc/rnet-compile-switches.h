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

//! @file     rnet-compile-switches.h
//! @authors  Bernie Woodland
//! @date     12May18
//!
//! @brief    RNET compile-time settings
//!

#ifndef RNET_COMPILE_SWITCHES_H_
#define RNET_COMPILE_SWITCHES_H_

#include "raging-global.h"


// RNET support for RNET buffers ('rnet_buf_t')
#define RNET_CS_USING_BUFS               1

// RNET support for particles ('nsvc_pcl_t')
#define RNET_CS_USING_PCLS               1

// Tx of internal RNET packets prefers using RNET buffers
#define RNET_CS_USING_BUFS_FOR_TX        1

// Tx of internal RNET packets prefers using SL particles
#define RNET_CS_USING_PCLS_FOR_TX        0


//*** TEST MODE ONLY SWITCHES

// Enable test mode for PPP. Skews timers between interfaces.
#define RNET_ENABLE_PPP_TEST_MODE                0

// Loopback from rx UDP path to tx UDP path
#define RNET_SERVER_MODE_LOOPBACK                1

// Set to "1" to enable crossconnect from one intfc one
//  test intfc to another
#define RNET_INTFC_CROSSCONNECT_TEST_MODE        0

// Loopback IP packets from IPv4/IPv6 Tx to Rx
#define RNET_IP_L3_LOOPBACK_TEST_MODE            0



#endif  // RNET_COMPILE_SWITCHES_H_