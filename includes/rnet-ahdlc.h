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

//! @file     rnet-ahdlc.h
//! @authors  Bernie Woodland
//! @date     9Apr18
//!
//! @brief    HDLC/AHDCL protocol for PPP and whoeever else may want to use it
//!

#ifndef RNET_AHDLC_H_
#define RNET_AHDLC_H_

#define RNET_AHDLC_FLAG_SEQUENCE      0x7E
#define RNET_AHDLC_CONTROL_ESCAPE     0x7D
#define RNET_AHDLC_MAGIC_EOR          0x20

// Size of a single start or end 0x7E char
#define AHDLC_FLAG_CHAR_SIZE             1

#define RNET_AHDLC_FORMATTING_ERROR (-1)

#include "raging-global.h"
#include "rnet-compile-switches.h"

#include "nsvc-api.h"
#include "rnet-buf.h"

// APIs
RAGING_EXTERN_C_START
void rnet_ahdlc_strip_delimiters_buf(rnet_buf_t *buf);
void rnet_ahdlc_strip_delimiters_pcl(nsvc_pcl_t *head_pcl);
void rnet_ahdlc_encode_delimiters_buf(rnet_buf_t *buf);
void rnet_ahdlc_encode_delimiters_pcl(nsvc_pcl_t *head_pcl);
int rnet_ahdlc_strip_control_chars_linear(uint8_t *buffer, unsigned length,
                                          bool      data_will_continue);
bool rnet_ahdlc_strip_control_chars_buf(rnet_buf_t *buf);
bool rnet_ahdlc_strip_control_chars_pcl(nsvc_pcl_t *head_pcl);
bool rnet_ahdlc_encode_control_chars_dual(const uint8_t *src_buffer,
                                          unsigned       src_buffer_length,
                                          uint8_t       *dest_buffer,
                                          unsigned      *dest_buffer_length);
bool rnet_ahdlc_encode_control_chars_buf(rnet_buf_t *buf,
                                         unsigned    translation_count);
unsigned rnet_ahdlc_translation_count_linear(uint8_t *buffer,
                                             unsigned length);
unsigned rnet_ahdlc_translation_count_pcl(nsvc_pcl_t *head_pcl);
bool rnet_ahdlc_encode_control_chars_pcl(nsvc_pcl_t *head_pcl,
                                         unsigned    translation_count);
void rnet_msg_rx_buf_ahdlc_strip_cc(rnet_buf_t *buf);
void rnet_msg_rx_pcl_ahdlc_strip_cc(nsvc_pcl_t *head_pcl);
void rnet_msg_rx_buf_ahdlc_verify_crc(rnet_buf_t *buf);
void rnet_msg_rx_pcl_ahdlc_verify_crc(nsvc_pcl_t *head_pcl);
void rnet_msg_tx_buf_ahdlc_crc(rnet_buf_t *buf);
void rnet_msg_tx_pcl_ahdlc_crc(nsvc_pcl_t *head_pcl);
void rnet_msg_tx_buf_ahdlc_encode_cc(rnet_buf_t *buf);
void rnet_msg_tx_pcl_ahdlc_encode_cc(nsvc_pcl_t *head_pcl);
RAGING_EXTERN_C_END

#endif //RNET_AHDLC_H_