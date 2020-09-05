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

//! @file     nsvc.h
//! @authors  Bernie Woodland
//! @date     1Oct17
//!
//! @brief   Application settings for NUFR SL (Service Layer)
//!

#ifndef NSVC_H
#define NSVC_H

#include "nufr-global.h"
#include "nsvc-api.h"
#include "nsvc-app.h"
#include "nufr-platform-app.h"

#ifndef NSVC_MSG_BPOOL_GLOBAL_DEFS
    extern nufr_msg_t *(*nsvc_msg_pool_empty_fcn_ptr)(void);
#endif  //NSVC_MSG_BPOOL_GLOBAL_DEFS

#ifndef NSVC_PCL_GLOBAL_DEFS
    extern nsvc_pool_t nsvc_pcl_pool;
#endif  //NSVC_PCL_GLOBAL_DEFS

//! @brief     APIs
RAGING_EXTERN_C_START
//void nsvc_init(nufr_msg_t *(*msg_pool_empty_fcn_ptr)(void));
void nsvc_init(void);
bool nsvc_sema_pool_alloc(nufr_sema_t *sema);
RAGING_EXTERN_C_END

#endif  //NSVC_H