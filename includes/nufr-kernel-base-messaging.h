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

//! @file     nufr-kernel-base-messaging.h
//! @authors  Bernie Woodland
//! @date     7Aug17
//!
//! @brief   Messaging-related definitions that fit either of:
//! @brief     (1) Non-customizable platform layer constructs
//! @brief     (2) Defined in kernel but needed in platform layer
//!
//! @details 

#ifndef NUFR_KERNEL_BASE_MESSAGING_H
#define NUFR_KERNEL_BASE_MESSAGING_H

#include "nufr-global.h"

//              'fields' bit assignments
//              ------------------------
//      31-22     21-12       11-4           3-2         1-0
//     PREFIX      ID       SENDING TASK  [unused]     PRIORITY


//!
//! @struct   nufr_msg_t
//!
typedef struct nufr_msg_t_
{
    struct nufr_msg_t_ *flink;
    uint32_t            fields;
    uint32_t            parameter;
} nufr_msg_t;

// See 'nufr-api.h' for helper macros for nufr_msg_t->fields value

#endif  //NUFR_KERNEL_BASE_MESSAGING_H