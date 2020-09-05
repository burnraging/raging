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

//! @file     ssp-app.h
//! @authors  Bernie Woodland
//! @date     30Nov18
//!
//! @brief    Simple Serial Protocol Driver
//! @brief    Per-application config settings
//!

#ifndef SSP_APP_H_
#define SSP_APP_H_

// Num bytes consumed by 'Src Circuit' and 'Dest Circuit' fields
//    embedded in payload
#define SSP_APP_SPECIFIERS_SIZE    2

// Size of SSP packet, which is frame payload.
// This is max size which comes out of rx driver
// It includes SSP_APP_SPECIFIERS_SIZE/Src+Dest Circuit bytes
#define SSP_MAX_PAYLOAD_SIZE     100

// Number of interfaces running this protocol over
#define SSP_NUM_CHANNELS           2

// Number of packet buffers in global pool
#define SSP_POOL_SIZE             10

#endif  //SSP_APP_H_