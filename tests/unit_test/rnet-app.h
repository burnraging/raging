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

//! @file     rnet-app.h
//! @authors  Bernie Woodland
//! @date     10Mar18
//!
//! @brief   Application settings for Raging Networking (RNET)
//!

#ifndef RNET_APP_H
#define RNET_APP_H

#include "raging-global.h"
#include "nsvc-app.h"        // for 'nsvc_tm_divisor_t'


//!
//! @name      RNET_EVENT_LIST_SIZE_INTFC_UP
//! @name      RNET_EVENT_LIST_SIZE_INTFC_DOWN
//! @name      RNET_EVENT_LIST_SIZE_INIT_COMPLETE
//!
//! @brief     Event list sizes.
//!
//! @details   See event lists in rnet-app.c
//! @details   THESE SIZES MUST BE MANUALLY ADJUSTED WHEN EVENT
//! @details   LISTS GROW OR SHRINK.
//!
#define RNET_EVENT_LIST_SIZE_INIT_COMPLETE        1
#define RNET_EVENT_LIST_SIZE_INTFC_UP             1
#define RNET_EVENT_LIST_SIZE_INTFC_DOWN           1

//!
//! @enum      rnet_intfc_t
//!
//! @brief     Interfaces. All must be statically configured.
//!
typedef enum
{
    RNET_INTFC_null = 0,
    RNET_INTFC_TEST,
    RNET_INTFC_max
} rnet_intfc_t;

#define RNET_NUM_INTFC      (RNET_INTFC_max - 1)

//!
//! @enum      rnet_subi_t
//!
//! @brief     Subinterfaces. All must be statically configured.
//!
typedef enum
{
    RNET_SUBI_null = 0,
    RNET_SUBI_TEST_LL,        // RNET_INTFC_TEST's link local
    RNET_SUBI_TEST,           // RNET_INTFC_TEST's IPv6 global
    RNET_SUBI_max
} rnet_subi_t;

#define RNET_NUM_SUBI       (RNET_SUBI_max - 1)


//!
//! @enum      rnet_persist_cir_t
//!
//! @brief     Persistent circuits
//!
//! @details   A circuit is a peer-to-peer connection, a bit
//! @details   like a socket but more vague. Circuits specify
//! @details   self interface+sub-interface (sub-interface sp????, peer IP address,
//! @details   connection type (UDP or TCP), port numbers
//! @details   self and remote peer.
//!
typedef enum
{
    RNET_PCIR_null = 0,
    RNET_PCIR_TEST_SERVER,
    RNET_PCIR_max
} rnet_persist_cir_t;

#define RNET_NUM_PCIR         (RNET_PCIR_max - 1)

//!
//! @name      RNET_NUM_CIR
//!
//! @brief     Max number of circuits.
//!
#define RNET_NUM_CIR          (RNET_NUM_PCIR + 3)


//!
//! @name      RNET_BUF_SIZE
//! @name      RNET_NUM_BUFS
//!
//! @brief     RNET buffer definition
//!
#define RNET_BUF_SIZE   1000
#define RNET_NUM_BUFS      5

#endif  //RNET_APP_H