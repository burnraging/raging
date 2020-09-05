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

//! @file     nsvc-app.h
//! @authors  Bernie Woodland
//! @date     15Sep18
//!
//! @brief   Application settings for NUFR SL (Service Layer)
//!

#ifndef NSVC_APP_H
#define NSVC_APP_H

#include "nufr-global.h"
#include "nufr-platform-app.h"

//!
//! @enum      nsvc_msg_prefix_t
//!
//! @brief     Values for field PREFIX
//!
typedef enum
{
    NSVC_MSG_PREFIX_local = 1,    //mandatory: defined at task level

    // Owned by 'NUFR_TID_BASE'
    NSVC_MSG_RNET_STACK,
    NSVC_MSG_BASE_TASK,

    // Owned by 'NUFR_TID_LOW'
    NSVC_MSG_LOW_TASK,

    // Replicate message to all tasks
    NSVC_MSG_GLOBAL,
} nsvc_msg_prefix_t;

//!
//! @enum     SL Mutexes
//!
typedef enum
{
    NSVC_MUTEX_null = 0,
    // Add mutexes here
    NSVC_MUTEX_max
} nsvc_mutex_t;

#define NSVC_NUM_MUTEX      (NSVC_MUTEX_max - 1)

//!
//! @name      NSVC_NUM_TIMER
//!
//! @brief     Number of app timers in pool
//!
#define NSVC_NUM_TIMER                                   2


//! @brief     APIs
//! @details   (Included in nsvc.h)

//!
//! @name      NSVC_PCL_SIZE
//!
//! @brief     Number of bytes which can be stored in a single particle,
//! @brief     not including header in chain head.
//!
#define NSVC_PCL_SIZE                    120

//!
//! @name      NSVC_PCL_NUM_PCLS
//!
//! @brief     Total number of particles
//!
#define NSVC_PCL_NUM_PCLS                  0

#endif  //NSVC_APP_H