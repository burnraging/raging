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

//! @file    nsvc-messaging.c
//! @authors Bernie Woodland
//! @date    30Sep17
//!
//! @brief   SL Message send routines
//!
//! @details 
//!

#include "nufr-global.h"

#include "nufr-api.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-message-blocks.h"
#include "nsvc-api.h"
#include "nsvc-app.h"
#include "nsvc.h"

#include "raging-contract.h"


//!
//! @name      nsvc_msg_struct_to_fields
//!
//! @brief     Build the 32-bit 'fields' member in 'nufr_msg_t'
//! @brief     from a 'nsvc_msg_fields_unary_t' struct
//!
//! @param[in] 'parms'-- message send parameters to be filled out by caller
//! @param[in] ' ->prefix'--
//! @param[in] ' ->id'--
//! @param[in] ' ->priority'--
//! @param[in] ' ->sending_task'-- tid if sent at task level,
//! @param[in]             'NUFR_TID_null' if from ISR/Systick handler
//! @param[in] ' ->destination_task'-- (not used)
//! @param[in] ' ->optional_parameter'-- (not used)
//!
//! @return    msg->fields usable value
//!
uint32_t nsvc_msg_struct_to_fields(const nsvc_msg_fields_unary_t *parms)
{
    uint32_t fields;

    fields = NUFR_SET_MSG_FIELDS(parms->prefix,
                                 parms->id,
                                 parms->sending_task,
                                 parms->priority);

    return fields;
}

//!
//! @name      nsvc_msg_args_to_fields
//!
//! @brief     Build the 32-bit 'fields' member in 'nufr_msg_t'
//! @brief     from paramters passed into this fcn
//!
//! @param[in] 'prefix'--
//! @param[in] 'id'--
//! @param[in] 'priority'--
//! @param[in] 'sending_task'-- tid if sent at task level,
//! @param[in]             'NUFR_TID_null' if from ISR/Systick handler
//!
//! @return    msg->fields usable value
//!
uint32_t nsvc_msg_args_to_fields(nsvc_msg_prefix_t prefix,
                                 uint16_t          id,
                                 nufr_msg_pri_t    priority,
                                 nufr_tid_t        sending_task)
{
    uint32_t fields;

    fields = NUFR_SET_MSG_FIELDS(prefix,
                                 id,
                                 sending_task,
                                 priority);

    return fields;
}

//!
//! @name      nsvc_msg_struct_to_fields
//!
//! @brief     Take a 32-bit 'fields' member in 'nufr_msg_t'
//! @brief     a build a 'nsvc_msg_fields_unary_t' struct from it
//!
//! @param[in]  'fields'--
//! @param[out] 'parms'-- message send parameters to be filled out by caller
//! @param[out] ' ->prefix'--
//! @param[out] ' ->id'--
//! @param[out] ' ->sending_task'-- tid of sending task
//! @param[out]             'NUFR_TID_null' if from ISR/Systick handler
//!
void nsvc_msg_fields_to_struct(uint32_t fields, nsvc_msg_fields_unary_t *parms)
{
    parms->prefix = (nsvc_msg_prefix_t)NUFR_GET_MSG_PREFIX(fields);
    parms->id = NUFR_GET_MSG_ID(fields);
    parms->priority = (nufr_msg_pri_t)NUFR_GET_MSG_PRIORITY(fields);
    parms->sending_task = (nufr_tid_t)NUFR_GET_MSG_SENDING_TASK(fields);
}

//!
//! @name      nsvc_msg_args_to_fields
//!
//! @brief     Take a 32-bit 'fields' member in 'nufr_msg_t'
//! @brief     a write to parms from it.
//!
//! @details   If any parm is NULL, skip write
//!
//! @param[in]  'fields'--
//! @param[out] 'prefix_ptr'--
//! @param[out] 'id_ptr'--
//! @param[out] 'priority_ptr'--
//! @param[out] 'sending_task_ptr'-- tid of sending task
//! @param[out]             'NUFR_TID_null' if from ISR/Systick handler
//!
void nsvc_msg_fields_to_args(uint32_t           fields,
                             nsvc_msg_prefix_t *prefix_ptr,
                             uint16_t          *id_ptr,
                             nufr_msg_pri_t    *priority_ptr,
                             nufr_tid_t        *sending_task_ptr)
{
    if (NULL != prefix_ptr)
        *prefix_ptr = (nsvc_msg_prefix_t)NUFR_GET_MSG_PREFIX(fields);

    if (NULL != id_ptr)
        *id_ptr = NUFR_GET_MSG_ID(fields);

    if (NULL != priority_ptr)
        *priority_ptr = (nufr_msg_pri_t)NUFR_GET_MSG_PRIORITY(fields);

    if (NULL != sending_task_ptr)
        *sending_task_ptr = (nufr_tid_t)NUFR_GET_MSG_SENDING_TASK(fields);
}

//!
//! @name      nsvc_msg_send_structW
//!
//! @brief     Send a message according to parameters specified in 'parms'.
//!
//! @details   Message will be sent to a single task or to multiple tasks.
//! @details   If 'msg_block' not provided, will allocated one--guaranteed.
//!
//! @details   Can be called from an ISR or from BG task, but not for
//! @details   multi-message sends.
//!
//! @param[in]  'parms'-- message send parameters to be filled out by caller
//! @param[in]  ' ->prefix'--
//! @param[in]  ' ->id'--
//! @param[in]  ' ->priority'--
//! @param[in]  ' ->destination_task'-- If tid is not 'NUFR_TID_null',
//! @param[in]              then send to that task. Otherwise, lookup
//! @param[in]              destination from prefix+ID.
//!
//! @return     send status
//!
nsvc_msg_send_return_t
nsvc_msg_send_structW(const nsvc_msg_fields_unary_t *parms)
{
    uint32_t          fields;
    nsvc_msg_lookup_t msg_route;
    nufr_tid_t        sending_task;
    nufr_tid_t        destination_task;
    nufr_msg_send_rtn_t send_status;
    nsvc_msg_send_return_t  return_value;
    bool              single_send;
    bool              status_bool;

    // Sending from a task?
    if (NUFR_IS_TCB(nufr_running))
    {
        sending_task = NUFR_TCB_TO_TID(nufr_running);
    }
    // Sending from BG Task?
    else
    {
        sending_task = NUFR_TID_null;
    }

    fields = NUFR_SET_MSG_FIELDS(parms->prefix,
                                 parms->id,
                                 sending_task,
                                 parms->priority);

    // Destination task specified by caller?
    if (NUFR_TID_null != parms->destination_task)
    {
        // Verify that destination is a valid task
        SL_REQUIRE_API(NUFR_IS_TCB(NUFR_TID_TO_TCB(parms->destination_task)));

        single_send = true;
        destination_task = parms->destination_task;
    }
    // No destination specified by caller: look it up.
    // The lookup may be a send to multiple tasks.
    else
    {
        status_bool = nsvc_msg_prefix_id_lookup(parms->prefix, &msg_route);

        SL_ENSURE(status_bool);
        UNUSED_BY_ASSERT(status_bool);

        single_send = NUFR_TID_null != msg_route.single_tid;
        if (single_send)
        {
            destination_task = msg_route.single_tid;
        }
    }

    // Send to a single task?
    if (single_send)
    {
        send_status = nufr_msg_send(fields,
                                    parms->optional_parameter,
                                    destination_task);

        // overlay return value of internal call to our return
        return_value = (nsvc_msg_send_return_t)send_status;
    }
    // Sending to multiple tasks
    else
    {
        return_value = nsvc_msg_send_multi(fields,
                                           parms->optional_parameter,
                                           &msg_route);
    }

    return return_value;
}

//!
//! @name      nsvc_msg_send_argsW
//!
//! @brief     Similar to 'nsvc_msg_send_structW()'
//!
//! @details   Can be called from an ISR or from BG task
//!
//! @param[in]  'prefix'--
//! @param[in]  'id'--
//! @param[in]  'priority'--
//! @param[in]  'destination_task'-- If tid is not 'NUFR_TID_null',
//! @param[in]              then send to that task. Otherwise, lookup
//! @param[in]              destination from prefix+ID.
//! @param[in]  'optional_parameter'-- 
//!
//! @return      send status
//!
nsvc_msg_send_return_t
nsvc_msg_send_argsW(nsvc_msg_prefix_t              prefix,
                    uint16_t                       id,
                    nufr_msg_pri_t                 priority,
                    nufr_tid_t                     destination_task,
                    uint32_t                       optional_parameter)
{
    uint32_t            fields;
    nsvc_msg_lookup_t   msg_route;
    nufr_tid_t          sending_task;
    nufr_msg_send_rtn_t send_status;
    nsvc_msg_send_return_t  return_value;
    bool                single_send;
    bool                status_bool;

    if (NUFR_IS_TCB(nufr_running))
    {
        sending_task = NUFR_TCB_TO_TID(nufr_running);
    }
    // Sending from BG Task
    else
    {
        sending_task = NUFR_TID_null;
    }

    fields = NUFR_SET_MSG_FIELDS(prefix,
                                 id,
                                 sending_task,
                                 priority);

    if (NUFR_TID_null != destination_task)
    {
        // Verify that destination is a valid task
        SL_REQUIRE_API(NUFR_IS_TCB(NUFR_TID_TO_TCB(destination_task)));

        single_send = true;
    }
    else
    {
        status_bool = nsvc_msg_prefix_id_lookup(prefix, &msg_route);

        SL_ENSURE(status_bool);
        UNUSED_BY_ASSERT(status_bool);

        single_send = NUFR_TID_null != msg_route.single_tid;
        if (single_send)
        {
            destination_task = msg_route.single_tid;
        }
    }

    if (single_send)
    {
        send_status = nufr_msg_send(fields,
                                    optional_parameter,
                                    destination_task);

        // overlay return value of internal call to our return
        return_value = (nsvc_msg_send_return_t)send_status;
    }
    else
    {
        return_value = nsvc_msg_send_multi(fields,
                                           optional_parameter,
                                           &msg_route);
    }

    return return_value;
}

//!
//! @name      nsvc_msg_send_multi
//!
//! @brief     Send a message to multiple tasks
//!
//! @details   Cannot be called from an ISR or from BG task
//!
//! @param[in]  'fields'-- completely prepared nufr_msg_t->fields value
//! @param[in]  'destination_list'-- List of tasks to send to
//!
//! @return      send status
//!
nsvc_msg_send_return_t
nsvc_msg_send_multi(uint32_t           fields,
                    uint32_t           optional_parameter,
                    nsvc_msg_lookup_t *destination_list)
{
    nufr_msg_t         *msg_holder_head_ptr = NULL;
    nufr_msg_t         *msg_holder_tail_ptr;
    nufr_msg_t         *msg;
    nufr_msg_t         *next_msg;
    nufr_tid_t          source_task;
    nufr_tid_t          destination_task;
    nufr_msg_send_rtn_t send_status = NUFR_MSG_SEND_OK;
    nsvc_msg_send_return_t  return_value;
    unsigned            i;
    unsigned            add_length;

    SL_REQUIRE_API(destination_list->tid_list_length > 0);
    SL_REQUIRE_API(NUFR_TID_null == destination_list->single_tid);

    // BG Task?
    if (NUFR_IS_TCB(nufr_running))
    {
        source_task = NUFR_TID_null;
    }
    else
    {
        source_task = NUFR_TCB_TO_TID(nufr_running);
    }
    // Poke source task into 'fields', keeping all other bits the same.
    fields = NUFR_SET_MSG_SENDING_TASK(fields, source_task);

    // Allocate the other blocks in the multi-send.
    add_length = destination_list->tid_list_length;

    // Queue up the message blocks beforehand
    for (i = 0; i < add_length; i++)
    {
        msg = nufr_msg_get_block();

        // If we failed to get a message block, assume a message
        //   abort occured. Recursively free all message blocks
        //   allocated up to that point before aborting.
        if (NULL == msg)
        {
            msg = msg_holder_head_ptr;
            while (NULL != msg)
            {
                next_msg = msg->flink;

                nufr_msg_free_block(msg);

                msg = next_msg;
            }

            return NSVC_MSRT_ABORTED;
        }

        // Append message to holder list
        if (NULL == msg_holder_head_ptr)
        {
            msg_holder_head_ptr = msg;
            msg_holder_tail_ptr = msg;
        }
        else
        {
            msg_holder_tail_ptr->flink = msg;
            msg_holder_tail_ptr = msg;
        }
    }

    // Set task to highest priority so it can send messages
    // all at once, so all messages get put in receiving
    // tasks' inboxes atomically (well...almost, there's still
    // IRQ message sends).
    nufr_prioritize();

    for (i = 0; i < destination_list->tid_list_length; i++)
    {
        destination_task = destination_list->tid_list_ptr[i];

        // Pop a cached msg block and attach to tx msg
        msg = msg_holder_head_ptr;
        SL_ENSURE(NULL != msg);
        next_msg = msg->flink;

        // Copy over message contents
        msg->fields = fields;
        msg->parameter = optional_parameter;

        send_status = nufr_msg_send_by_block(msg, destination_task);

        // This will occur is a destination task hasn't been launched,
        // which is a strong possibility.
        if (NUFR_MSG_SEND_ERROR == send_status)
        {
            // If send failed, must return block to pool to prevent memory leak
            nufr_msg_free_block(msg);
        }

        msg_holder_head_ptr = next_msg;
    }

    nufr_unprioritize();

    // overlay return value of internal call to our return
    return_value = (nsvc_msg_send_return_t)send_status;

    return return_value;
}

//!
//! @name      nufr_msg_send_and_bop_waitW
//!
//! @brief     Atomic 'nsvc_msg_send_argsW()' and 'nufr_bop_waitW()'
//!
//! @details   Closes a corner case where if these 2 fcns. are
//! @details   called non-atomically, there can be a context switch
//! @details   between the msg send and the bop wait that disrupts
//! @details   timing.
//!
//! @param[in] (see msg send and bop wait api's)
//!
//! @return    Same as 'nufr_bop_waitW()'
//!
nufr_bop_wait_rtn_t nsvc_msg_send_and_bop_waitW(
                            nsvc_msg_prefix_t  prefix,
                            uint16_t           id,
                            nufr_msg_pri_t     priority,
                            nufr_tid_t         destination_task,
                            uint32_t           optional_parameter,
                            nufr_msg_pri_t     abort_priority_of_rx_msg)
{
    nufr_tcb_t             *self_tcb;
    nufr_tcb_t             *dest_tcb;
    uint8_t                 self_task_priority;
    uint8_t                 dest_task_priority;
    bool                    need_to_boost_priority;
    nsvc_msg_send_return_t  msg_send_rc;
    nufr_bop_wait_rtn_t     bop_wait_rc = NUFR_BOP_WAIT_INVALID;

    // Some kernel internal macros to get self, dest tasks' TCBs
    self_tcb = NUFR_TID_TO_TCB(nufr_self_tid());
    dest_tcb = NUFR_TID_TO_TCB(destination_task);

    // Check here also excludes BG Task from using this API
    if (!NUFR_IS_TCB(dest_tcb))
    {
        SL_REQUIRE_API(false);
        return NUFR_BOP_WAIT_INVALID;
    }

    self_task_priority = self_tcb->priority;
    dest_task_priority = dest_tcb->priority;

    // Is current self task's priority lower than dest task's current priority?
    need_to_boost_priority = self_task_priority > dest_task_priority;

    // Boosting calling task's priority to be the same as dest task's
    // ensures that nufr_msg_send to dest task won't cause dest task
    // to preempt this task, process message, then send a bop to
    // this task, before this task had a chance to wait on the bop.
    if (need_to_boost_priority)
    {
        nufr_change_task_priority(nufr_self_tid(), dest_task_priority);
    }

    msg_send_rc = nsvc_msg_send_argsW(prefix,
                                      id,
                                      priority,
                                      destination_task,
                                      optional_parameter);

    // Make sure msg send was successful before bop wait    
    if ((NSVC_MSRT_ERROR != msg_send_rc) &&
        (NSVC_MSRT_DEST_NOT_FOUND != msg_send_rc))
    {
        bop_wait_rc = nufr_bop_waitW(abort_priority_of_rx_msg);
    }

    // Restore to priority which was in place when this api was called
    if (need_to_boost_priority)
    {
        nufr_change_task_priority(nufr_self_tid(), self_task_priority);
    }

    return bop_wait_rc;
}

//!
//! @name      nufr_msg_send_and_bop_waitT
//!
//! @brief     (See above)
//!
nufr_bop_wait_rtn_t nsvc_msg_send_and_bop_waitT(
                            nsvc_msg_prefix_t  prefix,
                            uint16_t           id,
                            nufr_msg_pri_t     priority,
                            nufr_tid_t         destination_task,
                            uint32_t           optional_parameter,
                            nufr_msg_pri_t     abort_priority_of_rx_msg,
                            unsigned           timeout_ticks)
{
    nufr_tcb_t             *self_tcb;
    nufr_tcb_t             *dest_tcb;
    uint8_t                 self_task_priority;
    uint8_t                 dest_task_priority;
    bool                    need_to_boost_priority;
    nsvc_msg_send_return_t  msg_send_rc;
    nufr_bop_wait_rtn_t     bop_wait_rc = NUFR_BOP_WAIT_INVALID;

    // Some kernel internal macros to get self, dest tasks' TCBs
    self_tcb = NUFR_TID_TO_TCB(nufr_self_tid());
    dest_tcb = NUFR_TID_TO_TCB(destination_task);

    // Check here also excludes BG Task from using this API
    if (!NUFR_IS_TCB(dest_tcb))
    {
        SL_REQUIRE_API(false);
        return NUFR_BOP_WAIT_INVALID;
    }

    self_task_priority = self_tcb->priority;
    dest_task_priority = dest_tcb->priority;

    // Is current self task's priority lower than dest task's current priority?
    need_to_boost_priority = self_task_priority > dest_task_priority;

    // Boosting calling task's priority to be the same as dest task's
    // ensures that nufr_msg_send to dest task won't cause dest task
    // to preempt this task, process message, then send a bop to
    // this task, before this task had a chance to wait on the bop.
    if (need_to_boost_priority)
    {
        nufr_change_task_priority(nufr_self_tid(), dest_task_priority);
    }

    msg_send_rc = nsvc_msg_send_argsW(prefix,
                                      id,
                                      priority,
                                      destination_task,
                                      optional_parameter);

    // Make sure msg send was successful before bop wait    
    if ((NSVC_MSRT_ERROR != msg_send_rc) &&
        (NSVC_MSRT_DEST_NOT_FOUND != msg_send_rc))
    {
        bop_wait_rc = nufr_bop_waitT(abort_priority_of_rx_msg, timeout_ticks);
    }

    // Restore to priority which was in place when this api was called
    if (need_to_boost_priority)
    {
        nufr_change_task_priority(nufr_self_tid(), self_task_priority);
    }

    return bop_wait_rc;
}

//!
//! @name      nsvc_msg_get_structW
//!
//! @brief     Get a message, blocking indefinitely. Parse.
//!
//! @details   Cannot be called from an ISR or from BG task
//!
//! @param[out]  'parms'-- message send parameters to be filled out by caller
//! @param[out]  ' ->prefix'--
//! @param[out]  ' ->id'--
//! @param[out]  ' ->priority'--
//! @param[out]  ' ->destination_task'-- If tid is not 'NUFR_TID_null',
//! @param[out]              then send to that task. Otherwise, lookup
//! @param[out]              destination from prefix+ID.
//!
void nsvc_msg_get_structW(nsvc_msg_fields_unary_t *msg_fields_ptr)
{
    uint32_t    fields = 0;                     // init to squelch warning

    SL_REQUIRE_API(NULL != msg_fields_ptr);
    msg_fields_ptr->optional_parameter = 0;     // init to squelch warning
        
    nufr_msg_getW(&fields,
                  &(msg_fields_ptr->optional_parameter));

    msg_fields_ptr->prefix = (nsvc_msg_prefix_t)NUFR_GET_MSG_PREFIX(fields);
    msg_fields_ptr->id = NUFR_GET_MSG_ID(fields);
    msg_fields_ptr->priority = (nufr_msg_pri_t)NUFR_GET_MSG_PRIORITY(fields);
    msg_fields_ptr->sending_task = (nufr_tid_t)NUFR_GET_MSG_SENDING_TASK(fields);
    msg_fields_ptr->destination_task = 0;
}

//!
//! @name      nsvc_msg_get_structT
//!
//! @brief     Same as 'nsvc_msg_get_structW()', but with timeout
//!
//! @details   If timeout is zero, will check for message
//! @details   without blocking and exit if none there.
//! @details   
//! @details   Cannot be called from an ISR or from BG task
//!
//! @param[out]  'msg_fields_ptr'-- passed in by caller, written here
//! @param[in]   'timeout_ticks'-- passed in by caller, written here
//!
//! @return    'false' if timeout occured or no message received
//!
bool nsvc_msg_get_structT(nsvc_msg_fields_unary_t *msg_fields_ptr,
                          unsigned                 timeout_ticks)
{
    bool        timed_out;
    uint32_t    fields = 0;                     // init to squelch warning

    SL_REQUIRE_API(NULL != msg_fields_ptr);
    msg_fields_ptr->optional_parameter = 0;     // init to squelch warning
        
    timed_out = nufr_msg_getT(timeout_ticks,
                              &fields,
                              &(msg_fields_ptr->optional_parameter));

    if (timed_out)
    {
        return false;
    }

    msg_fields_ptr->prefix = (nsvc_msg_prefix_t)NUFR_GET_MSG_PREFIX(fields);
    msg_fields_ptr->id = NUFR_GET_MSG_ID(fields);
    msg_fields_ptr->priority = (nufr_msg_pri_t)NUFR_GET_MSG_PRIORITY(fields);
    msg_fields_ptr->sending_task = (nufr_tid_t)NUFR_GET_MSG_SENDING_TASK(fields);
    msg_fields_ptr->destination_task = 0;

    return true;
}

//!
//! @name      nsvc_msg_get_argsW
//!
//! @brief     Get a message, blocking indefinitely. Parse.
//!
//! @details   Cannot be called from an ISR or from BG task
//! @details   Any parm set to NULL is ignored
//!
//! @param[out]  'prefix_ptr'--
//! @param[out]  'id_ptr'--
//! @param[out]  'priority_ptr'--
//! @param[out]  'source_task_ptr'-- If tid is not 'NUFR_TID_null',
//! @param[out]              then send to that task. Otherwise, lookup
//! @param[out]              destination from prefix+ID.
//! @param[out]  'optional_parameter_ptr'--
//!
void nsvc_msg_get_argsW(nsvc_msg_prefix_t  *prefix_ptr,
                        uint16_t           *id_ptr,
                        nufr_msg_pri_t     *priority_ptr,
                        nufr_tid_t         *source_task_ptr,
                        uint32_t           *optional_parameter_ptr)
{
    uint32_t    fields = 0;                     // init to squelch warning

    nufr_msg_getW(&fields, optional_parameter_ptr);

    if (NULL != prefix_ptr)
        *prefix_ptr = (nsvc_msg_prefix_t)NUFR_GET_MSG_PREFIX(fields);

    if (NULL != id_ptr)
        *id_ptr = NUFR_GET_MSG_ID(fields);

    if (NULL != priority_ptr)
        *priority_ptr = (nufr_msg_pri_t)NUFR_GET_MSG_PRIORITY(fields);

    if (NULL != source_task_ptr)
        *source_task_ptr = (nufr_tid_t)NUFR_GET_MSG_SENDING_TASK(fields);
}

//!
//! @name      nsvc_msg_get_argsT
//!
//! @brief     Same as 'nsvc_msg_get_argsT()', but with timeout.
//!
//! @details   If timeout is zero, will check for message
//! @details   without blocking and exit.
//! @details   
//! @details   Cannot be called from an ISR or from BG task
//!
//! @param[out]  'prefix_ptr'--
//! @param[out]  'id_ptr'--
//! @param[out]  'priority_ptr'--
//! @param[out]  'source_task_ptr'-- If tid is not 'NUFR_TID_null',
//! @param[out]              then send to that task. Otherwise, lookup
//! @param[out]              destination from prefix+ID.
//! @param[out]  'optional_parameter_ptr'--
//! @param[in]   'timeout_ticks'--
//!
//! @return    'false' if timeout occured or no message received
//!
bool nsvc_msg_get_argsT(nsvc_msg_prefix_t  *prefix_ptr,
                        uint16_t           *id_ptr,
                        nufr_msg_pri_t     *priority_ptr,
                        nufr_tid_t         *source_task_ptr,
                        uint32_t           *optional_parameter_ptr,
                        unsigned            timeout_ticks)
{
    uint32_t    fields = 0;                     // init to squelch warning
    bool        timed_out;

    timed_out = nufr_msg_getT(timeout_ticks, &fields, optional_parameter_ptr);

    if (timed_out)
    {
        return false;
    }

    if (NULL != prefix_ptr)
        *prefix_ptr = (nsvc_msg_prefix_t)NUFR_GET_MSG_PREFIX(fields);

    if (NULL != id_ptr)
        *id_ptr = NUFR_GET_MSG_ID(fields);

    if (NULL != priority_ptr)
        *priority_ptr = (nufr_msg_pri_t)NUFR_GET_MSG_PRIORITY(fields);

    if (NULL != source_task_ptr)
        *source_task_ptr = (nufr_tid_t)NUFR_GET_MSG_SENDING_TASK(fields);

    return true;
}