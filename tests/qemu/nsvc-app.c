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
//! @date     17Sep17
//!
//! @brief   Application settings for NUFR SL (Service Layer)
//!

#include "nufr-global.h"
#include "nufr-platform-app.h"
#include "nufr-platform.h"

#include "nsvc-app.h"
#include "nsvc-api.h"

#include "raging-contract.h"
#include "raging-utils-mem.h"

#if 0
//!
//! @brief     Fixed subscriber lists
//!
static const nufr_tid_t fslist_many_tasks[] =
{
    NUFR_TID_01,
    NUFR_TID_02,
    NUFR_TID_03,
//    NUFR_TID_null       ?end of list marker needed?
};

//!
//! @name      nsvc_msg_prefix_id_to_tid
//!
//! @brief     Binds a task to a message prefix
//!
//! @param[in]  'prefix'-- msg->fields from msg
//! @param[out] 'out_ptr'-- result of lookup
//!
//! @return    'true' if lookup found a destination
//!
bool nsvc_msg_prefix_id_lookup(nsvc_msg_prefix_t  prefix,
                               nsvc_msg_lookup_t *out_ptr)
{
    nufr_tid_t        tid;
    bool              success = true;

    SL_REQUIRE_API(NULL != out_ptr);

    rutils_memset(out_ptr, 0, sizeof(nsvc_msg_lookup_t));

    switch (prefix)
    {
    case NSVC_MSG_PREFIX_A:
        tid = NUFR_TID_01;
        break;

    case NSVC_MSG_PREFIX_B:
        tid = NUFR_TID_02;
        break;

    case NSVC_MSG_PREFIX_C:
        tid = NUFR_TID_null;
        out_ptr->tid_list_ptr = fslist_many_tasks;
        out_ptr->tid_list_length = sizeof(fslist_many_tasks);
        break;

    case NSVC_MSG_PREFIX_local:
    default:
        tid = NUFR_TID_null;
        success = false;
        break;
    }

    if (success)
    {
        SL_ENSURE(tid < NUFR_TID_max);
        SL_ENSURE(NUFR_TID_null == tid? NULL != out_ptr->tid_list_ptr : true);
        SL_ENSURE(NUFR_TID_null == tid? out_ptr->tid_list_length >= 1 : true);
        SL_ENSURE(NUFR_TID_null == tid?
                  out_ptr->tid_list_length <= NUFR_NUM_TASKS : true);
    }

    out_ptr->single_tid = tid;

    return success;
}
#endif