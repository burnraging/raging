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

//! @file    nufr-kernel-messaging.c
//! @authors Bernie Woodland
//! @date    19May18

#include "nufr-global.h"

#if NUFR_CS_MESSAGING == 1

#include "nufr-kernel-base-messaging.h"
#include "nufr-kernel-message-blocks.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-timer.h"
#include "nufr-kernel-semaphore.h"

#include "raging-contract.h"


nufr_msg_t *message_queue_head;
nufr_msg_t *message_queue_tail;

// API calls

void nufr_msg_drain(nufr_tid_t task_id, nufr_msg_pri_t from_this_priority)
{
}


//!
//! @name      nufr_msg_send_by_block
//!
//! @brief     Main kernel message sending API
//!
//! @details   Steps in calling environment:
//! @details     (1) nufr_msg_get_block() called to fetch a message
//! @details         block to be passed into nufr_msg_send_by_block().
//! @details     (2) SENDING-TASK value for 'dest_tcb' determined
//! @details         nufrplat_msg_prefix_id_to_tid() to determine this.
//! @details     (3) Priority, message prefixes and IDs chosen.
//! @details         msg->fields packed using NUFR_SET_MSG_FIELDS().
//! @details     (4) Sanity checks on parameters applied.
//!
//! @param[in] 'msg'-- Pointer to message block to be sent.
//! @param[in]         Must have been allocated from message pool.
//! @param[in]         If there was no error on send, message receiver
//! @param[in]         owns block, and therefore must free it.
//! @param[in]  Message fields that must be filled in:       
//! @param[in]        PREFIX (svc_msg_prefix_t)
//! @param[in]        ID (nufr_msg_id_t )
//! @param[in]        PRIORITY (nufr_msg_pri_t) 
//! @param[in]         
//! @param[in]        PREFIX, ID, PRIORITY are bit fields in 'msg->fields'
//! @param[in]        Recommend using macro NUFR_SET_MSG_FIELDS() or equivalent
//! @param[in]         
//! @param[in]  'msg->parameter' is optional
//! @param[in]         
//! @param[in]  'dest_task_id'--task that will receive message
//
//! @return      Action applied to receiving task
//!
nufr_msg_send_rtn_t nufr_msg_send(uint32_t msg_fields,
                                  uint32_t optional_parameter,
                                  nufr_tid_t dest_task_id)
{
    nufr_msg_t         *msg;
    nufr_msg_send_rtn_t rc;

    msg = nufr_msg_get_block();
    msg->fields = msg_fields;
    msg->parameter = optional_parameter;

    rc = nufr_msg_send_by_block(msg, dest_task_id);

    return rc;
}

//!
//! @name      nufr_msg_send_by_block
//!
//! @brief     Main kernel message sending API
//!
//! @details   Steps in calling environment:
//! @details     (1) nufr_msg_get_block() called to fetch a message
//! @details         block to be passed into nufr_msg_send_by_block().
//! @details     (2) SENDING-TASK value for 'dest_tcb' determined
//! @details         nufrplat_msg_prefix_id_to_tid() to determine this.
//! @details     (3) Priority, message prefixes and IDs chosen.
//! @details         msg->fields packed using NUFR_SET_MSG_FIELDS().
//! @details     (4) Sanity checks on parameters applied.
//!
//! @param[in] 'msg'-- Pointer to message block to be sent.
//! @param[in]         Must have been allocated from message pool.
//! @param[in]         If there was no error on send, message receiver
//! @param[in]         owns block, and therefore must free it.
//! @param[in]  Message fields that must be filled in:       
//! @param[in]        PREFIX (svc_msg_prefix_t)
//! @param[in]        ID (nufr_msg_id_t )
//! @param[in]        PRIORITY (nufr_msg_pri_t) 
//! @param[in]         
//! @param[in]        PREFIX, ID, PRIORITY are bit fields in 'msg->fields'
//! @param[in]        Recommend using macro NUFR_SET_MSG_FIELDS() or equivalent
//! @param[in]         
//! @param[in]  'msg->parameter' is optional
//! @param[in]         
//! @param[in]  'dest_task_id'--task that will receive message
//
//! @return      Action applied to receiving task
//!
nufr_msg_send_rtn_t nufr_msg_send_by_block(nufr_msg_t *msg, nufr_tid_t dest_task_id)
{
    if (NULL == message_queue_head)
    {
        message_queue_head = msg;
        msg->flink = NULL;
        message_queue_tail = msg;
    }
    else
    {
        message_queue_tail->flink = msg;
        message_queue_tail = msg;
    }

    return NUFR_MSG_SEND_OK;
}

//!
//! @name      nufr_msg_getW
//!
//! @brief     Get a message, block indefinitely until you get one
//!
//! @details   Cannot be called from an ISR or from BG task
//! @details
//! @details   Caller must free block.
//! @details   
//! @details   
//!
//! @return      Message received. Never NULL.
//!
void nufr_msg_getW(uint32_t *msg_fields_ptr, uint32_t *parameter_ptr)
{
    nufr_msg_t  *msg;

    *msg_fields_ptr = BIT_MASK32;
    if (NULL != parameter_ptr)
    {
        *parameter_ptr = BIT_MASK32;
    }

    if (NULL != message_queue_head)
    {
        msg = message_queue_head;
        message_queue_head = message_queue_head->flink;

        msg->flink = NULL;

        // Transfer over to called parameters
        *msg_fields_ptr = msg->fields;
        if (NULL != parameter_ptr)
        {
            *parameter_ptr = msg->parameter;
        }

        // Free block
        nufr_msg_free_block(msg);
    }

    if (NULL == message_queue_head)
    {
        message_queue_tail = NULL;
    }
}

//!
//! @name      nufr_msg_getT
//!
//! @brief     Get a message, block until 'timeout_ticks'
//!
//! @details   Cannot be called from an ISR or from BG task
//! @details
//! @details   Same as 'nufr_msg_getW()', except with timeout
//! @details   
//!
//! @params[in] timeout_ticks-- timeout. If == 0, return from API call
//! @params[in]       immediately if no msg waiting.
//!
//! @return      'true' if timeout
//!
bool nufr_msg_getT(unsigned  timeout_ticks,
                   uint32_t *msg_fields_ptr,
                   uint32_t *parameter_ptr)
{
    return true;
}

//!
//! @name      nufr_msg_peek
//!
//! @brief     Returns the first message block without dequeing it
//!
//! @details   Caller might want to lock/unlock interrupts around this
//! @details   call, as another task or an ISR may change the msg queue
//! @details   at an inconvenient time.
//! @details   
//!
//! @return      Message at head of list. NULL if no message.
//!
nufr_msg_t *nufr_msg_peek(void)
{
    if (NULL == message_queue_head)
    {
        return message_queue_head;
    }

    return NULL;
}

#endif  //NUFR_CS_MESSAGING