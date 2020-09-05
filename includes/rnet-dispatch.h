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

//! @file     rnet-dispatch.h
//! @authors  Bernie Woodland
//! @date     12May18
//!
//! @brief    Top-level and message interface to RNET stack
//!

#ifndef RNET_DISPATCH_H_
#define RNET_DISPATCH_H_

#include "raging-global.h"
#include "rnet-compile-switches.h"
#include "rnet-buf.h"
#include "rnet-top.h"
#include "rnet-app.h"
#include "rnet-intfc.h"

#include "nsvc-api.h"
#include "nufr-api.h"

// APIs

RAGING_EXTERN_C_START
const rnet_notif_list_t *rnet_retrieve_event_list(rnet_notif_t list_name,
                                                  unsigned *list_length_ptr);
void rnet_send_msgs_to_event_list(rnet_notif_t list_name,
                                  uint32_t     optional_parameter);
void rnet_set_msg_prefix(nufr_tid_t task_id, unsigned prefix);
void rnet_msg_send(rnet_id_t msg_id, void *buffer);
void rnet_msg_send_with_parm(rnet_id_t msg_id, uint32_t parameter);
void rnet_msg_send_buf_to_listener(uint32_t    msg_fields,
                                   nufr_tid_t  dest_tid,
                                   rnet_buf_t *buf);
void rnet_msg_send_pcl_to_listener(uint32_t    msg_fields,
                                   nufr_tid_t  dest_tid,
                                   nsvc_pcl_t *head_pcl);
void rnet_intfc_timer_set(rnet_intfc_t intfc,
                          rnet_id_t    expiration_msg,
                          uint32_t     timeout_millisecs);
void rnet_intfc_timer_kill(rnet_intfc_t intfc);
void rnet_create_buf_pool(void);
rnet_buf_t *rnet_alloc_bufW(void);
rnet_buf_t *rnet_alloc_bufT(uint32_t timeout_ticks);
nsvc_pcl_t *rnet_alloc_pclW(void);
nsvc_pcl_t *rnet_alloc_pclT(unsigned timeout_ticks);
void rnet_free_buf(rnet_buf_t *buf);
void rnet_msg_rx_buf_entry(rnet_buf_t *buf);
void rnet_msg_rx_pcl_entry(nsvc_pcl_t *head_pcl);
void rnet_msg_tx_buf_driver(rnet_buf_t *buf);
void rnet_msg_tx_pcl_driver(nsvc_pcl_t *head_pcl);
void rnet_msg_buf_discard(rnet_buf_t *buf);
void rnet_msg_pcl_discard(nsvc_pcl_t *head_pcl);
RAGING_EXTERN_C_END

#endif  // RNET_DISPATCH_H_