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

//! @file     rnet-dispatch.c
//! @authors  Bernie Woodland
//! @date     29Apr18
//!
//! @brief    Top-level support functions and non-protocol message handlers
//!
#define RNET_BUF_GLOBAL_DEFS

#include "raging-global.h"
#include "raging-contract.h"

#include "rnet-dispatch.h"
#include "rnet-buf.h"
#include "rnet-intfc.h"
#include "rnet-app.h"

#include "nsvc-api.h"

extern const rnet_notif_list_t rnet_event_list_init_complete[RNET_EVENT_LIST_SIZE_INIT_COMPLETE];
extern const rnet_notif_list_t rnet_event_list_intfc_up[RNET_EVENT_LIST_SIZE_INTFC_UP];
extern const rnet_notif_list_t rnet_event_list_intfc_down[RNET_EVENT_LIST_SIZE_INTFC_DOWN];

static nufr_tid_t  rnet_task_id;
static unsigned    rnet_msg_prefix;
static bool        rnet_msg_info_set;

static nsvc_pool_t rnet_buf_pool;
       rnet_buf_t  rnet_buf[RNET_NUM_BUFS];
static bool        rnet_pool_init_done;

//!
//! @name      rnet_retrieve_event_list
//!
//! @brief     Set RNET global variables
//!
//! @param[in] 'list_name'--
//! @param[out] 'list_length_ptr'-- num. elements in list
//!
//! @return    list
//!
const rnet_notif_list_t *rnet_retrieve_event_list(rnet_notif_t list_name,
                                                  unsigned    *list_length_ptr)
{
    switch (list_name)
    {
        case RNET_NOTIF_INIT_COMPLETE:
            *list_length_ptr = ARRAY_SIZE(rnet_event_list_init_complete);
            return rnet_event_list_init_complete;
            break; 
        case RNET_NOTIF_INTFC_UP:
            *list_length_ptr = ARRAY_SIZE(rnet_event_list_intfc_up);
            return rnet_event_list_intfc_up;
            break; 
        case RNET_NOTIF_INTFC_DOWN:
            *list_length_ptr = ARRAY_SIZE(rnet_event_list_intfc_down);
            return rnet_event_list_intfc_down;
            break; 
        default:
            // should never reach this!
            SL_REQUIRE_API(0);
            break;
    }

    *list_length_ptr = 0;
    return NULL;
}

//!
//! @name      rnet_send_msgs_to_event_list
//!
//! @brief     Send messages to all recipients in notification list
//!
//! @details   If message field is set to RNET_LISTENER_MSG_DISABLED,
//! @details   or if destination task is set to NUFR_TID_null, then
//! @details   this message is skipped.
//!
//! @param[in] 'list_name'-- which event list to send
//! @param[in] 'optional_parameter'-- parameter to attach to message
//!
void rnet_send_msgs_to_event_list(rnet_notif_t list_name,
                                  uint32_t     optional_parameter)
{
    const rnet_notif_list_t *list;
    nsvc_msg_fields_unary_t  msg_parms;
    unsigned                 list_length = 0;
    uint32_t                 msg_fields;
    nufr_tid_t               dest_tid;
    unsigned                 i;

    list = rnet_retrieve_event_list(list_name, &list_length);

    if (NULL == list)
    {
        return;
    }

    for (i = 0; i < list_length; i++)
    {
        msg_fields = list[i].msg_fields;

        if (RNET_LISTENER_MSG_DISABLED != msg_fields)
        {
            dest_tid = list[i].tid;

            msg_parms.prefix = NUFR_GET_MSG_PREFIX(msg_fields);
            msg_parms.id = NUFR_GET_MSG_ID(msg_fields);
            msg_parms.priority = NUFR_GET_MSG_PRIORITY(msg_fields);
            //msg_parms.sending_task = nufr_self_tid();  // gets filled in by API
            msg_parms.destination_task = dest_tid;
            msg_parms.optional_parameter = optional_parameter;

            (void)nsvc_msg_send_structW(&msg_parms);
        }
    }
}

//!
//! @name      rnet_set_msg_prefix
//!
//! @brief     Set RNET global variables
//!
//! @details   'rnet_task_id'--task ID that RNET is hosted by
//! @details   'rnet_msg_prefix'--message prefix that all RNET
//! @details        messages are sent to.
//! @details   'rnet_message_info_set'--'true' when above are set here.
//!
//! @param[in] 'task_id'-- assigned to 'rnet_task_id'
//! @param[in] 'prefix'-- assigned to 'rnet_msg_prefix'
//!
void rnet_set_msg_prefix(nufr_tid_t task_id, unsigned prefix)
{
    rnet_task_id = task_id;
    rnet_msg_prefix = prefix;

    rnet_msg_info_set = true;
}

//!
//! @name      rnet_msg_send
//!
//! @brief     Universal API to send a message to RNET from outside RNET
//! @brief     to RNET or to send a message from within RNET.
//!
//! @details   Uses the following global variables to route message:
//! @details      'rnet_task_id','rnet_msg_prefix', 
//!
//! @param[in] 'msg_id'-- RNET message ID used in message
//! @param[in] 'buffer'-- packet, one of type 'rnet_buf_t' or 'nsvc_pcl_t'
//!
void rnet_msg_send(rnet_id_t msg_id, void *buffer)
{
    nsvc_msg_fields_unary_t msg_parms;
    nsvc_msg_send_return_t  send_rv = NSVC_MSRT_ERROR;
    rnet_buf_t             *x;

    if (rnet_msg_info_set)
    {
        msg_parms.prefix = rnet_msg_prefix;
        msg_parms.id = msg_id;
        msg_parms.priority = NUFR_MSG_PRI_MID;
        //msg_parms.sending_task = nufr_self_tid();  // gets filled in by API
        msg_parms.destination_task = rnet_task_id;
        msg_parms.optional_parameter = (uint32_t)buffer;

        send_rv = nsvc_msg_send_structW(&msg_parms);
    }

    // In case message failed to be sent for some reason, must
    // free RNET buffer or PCL chain. But we don't know what
    // type 'buffer' is...have to check
    if ( !((NSVC_MSRT_OK == send_rv) || (NSVC_MSRT_AWOKE_RECEIVER == send_rv)) )
    {
        x = (rnet_buf_t *)buffer;

        if (IS_RNET_BUF(x))
        {
            rnet_free_buf(x);
        }
        else if (nsvc_pcl_is(buffer))
        {
            nsvc_pcl_free_chain((nsvc_pcl_t *)buffer);
        }
        else
        {
            // omitted
        }
    }
}

//!
//! @name      rnet_msg_send_with_parm
//!
//! @brief     Universal API to send a message to RNET from outside RNET
//! @brief     to RNET or to send a message from within RNET.
//!
//! @details   Uses the following global variables to route message:
//! @details      'rnet_task_id','rnet_msg_prefix', 
//!
//! @param[in] 'msg_id'-- RNET message ID used in message
//! @param[in] 'parameter'-- anything
//!
void rnet_msg_send_with_parm(rnet_id_t msg_id, uint32_t parameter)
{
    nsvc_msg_fields_unary_t msg_parms;

    if (rnet_msg_info_set)
    {
        msg_parms.prefix = rnet_msg_prefix;
        msg_parms.id = msg_id;
        msg_parms.priority = NUFR_MSG_PRI_MID;
        //msg_parms.sending_task = nufr_self_tid();  // gets filled in by API
        msg_parms.destination_task = rnet_task_id;
        msg_parms.optional_parameter = parameter;

        (void)nsvc_msg_send_structW(&msg_parms);
    }
}

//!
//! @name      rnet_msg_send_buf_to_listener
//!
//! @brief     Sends a pre-formatted message to a listening app.
//! @brief     Packet is attached.
//!
//! @param[in] 'msg_fields'-- nufr 'msg->fields' formatted message parms
//! @param[in] 'buf'-- packet attached
//!
void rnet_msg_send_buf_to_listener(uint32_t    msg_fields,
                                   nufr_tid_t  dest_tid,
                                   rnet_buf_t *buf)
{
    nsvc_msg_fields_unary_t msg_parms;

    msg_parms.prefix = NUFR_GET_MSG_PREFIX(msg_fields);
    msg_parms.id = NUFR_GET_MSG_ID(msg_fields);
    msg_parms.priority = NUFR_GET_MSG_PRIORITY(msg_fields);
    //msg_parms.sending_task = nufr_self_tid();  // gets filled in by API
    msg_parms.destination_task = dest_tid;
    msg_parms.optional_parameter = (uint32_t)buf;

    (void)nsvc_msg_send_structW(&msg_parms);
}

//!
//! @name      rnet_msg_send_pcl_to_listener
//!
//! @brief     Sends a pre-formatted message to a listening app.
//! @brief     Packet is attached.
//!
//! @param[in] 'msg_fields'-- nufr 'msg->fields' formatted message parms
//! @param[in] 'buf'-- packet attached
//!
void rnet_msg_send_pcl_to_listener(uint32_t    msg_fields,
                                   nufr_tid_t  dest_tid,
                                   nsvc_pcl_t *head_pcl)
{
    nsvc_msg_fields_unary_t msg_parms;

    msg_parms.prefix = NUFR_GET_MSG_PREFIX(msg_fields);
    msg_parms.id = NUFR_GET_MSG_ID(msg_fields);
    msg_parms.priority = NUFR_GET_MSG_PRIORITY(msg_fields);
    //msg_parms.sending_task = nufr_self_tid();  // gets filled in by API
    msg_parms.destination_task = dest_tid;
    msg_parms.optional_parameter = (uint32_t)head_pcl;

    (void)nsvc_msg_send_structW(&msg_parms);
}

//!
//! @name      rnet_intfc_timer_set
//!
//! @brief     Set the timer which belongs to a particular interface.
//!
//! @details   If timer is already running, will kill it and restart it.
//!
//! @param[in] 'intfc'-- which interface's timer to set
//! @param[in] 'expiration_msg'-- message to send upon timer expiration
//! @param[in] 'timeout_millisecs'-- 
//!
void rnet_intfc_timer_set(rnet_intfc_t intfc,
                          rnet_id_t    expiration_msg,
                          uint32_t     timeout_millisecs)
{
    nsvc_timer_t     *timer_ptr;
    nufr_tid_t        self_tid;

    SL_REQUIRE(rnet_intfc_is_valid(intfc));

    if (0 == timeout_millisecs)
    {
        return;
    }

    timer_ptr = rnet_intfc_get_timer(intfc);

    (void)nsvc_timer_kill(timer_ptr);

    self_tid = nufr_self_tid();

    timer_ptr->duration = timeout_millisecs;
    timer_ptr->msg_fields = NUFR_SET_MSG_FIELDS(
                                rnet_msg_prefix,
                                expiration_msg,
                                self_tid,
                                NUFR_MSG_PRI_MID);
    timer_ptr->mode = NSVC_TMODE_SIMPLE;
    timer_ptr->msg_parameter = intfc;
    timer_ptr->dest_task_id = self_tid;

    nsvc_timer_start(timer_ptr);
}

//!
//! @name      rnet_intfc_timer_kill
//!
//! @brief     Kill the timer which belongs to a particular interface.
//!
//! @details   Doesn't matter if timer is running or not
//!
void rnet_intfc_timer_kill(rnet_intfc_t intfc)
{
    nsvc_timer_t *timer_ptr;

    SL_REQUIRE(rnet_intfc_is_valid(intfc));

    timer_ptr = rnet_intfc_get_timer(intfc);

    nsvc_timer_kill(timer_ptr);
}


//!
//! @name      rnet_create_buf_pool
//!
//! @brief     Initialize RNET's buffer pool
//!
//! @details   Only applicable if RNET buffer pool is used
//! @details   (can use pcls exclusively).
//!
void rnet_create_buf_pool(void)
{
    rnet_buf_pool.pool_size = RNET_NUM_BUFS;
    rnet_buf_pool.element_size = sizeof(rnet_buf_t);
    rnet_buf_pool.element_index_size = (unsigned)(
                 (uint8_t *)&rnet_buf[1] - (uint8_t *)&rnet_buf[0]
                                                 );
    rnet_buf_pool.base_ptr = rnet_buf;
    rnet_buf_pool.flink_offset = OFFSETOF(rnet_buf_t, flink);

    nsvc_pool_init(&rnet_buf_pool);

    rnet_pool_init_done = true;
}

//!
//! @name      rnet_alloc_bufW
//!
//! @brief     Allocate a buffer from RNET's buffer pool
//!
//! @details   Only applicable if RNET buffer pool is used.
//! @details   Will cause calling task to block until buffer
//! @details   becomes available.
//!
//! @details   Returns a buffer. Never returns NULL.
//!
rnet_buf_t *rnet_alloc_bufW(void)
{
    rnet_buf_t *buf = NULL;

    // Assume 'rnet_pool_init_done' is 'true'
    // 'void **' cast to supress compiler warning (hate doing it!)
    (void)nsvc_pool_allocateW(&rnet_buf_pool, (void **)&buf);

    // Won't return NULL if message abort is disabled
    return buf;
}

//!
//! @name      rnet_alloc_bufT
//!
//! @brief     Allocate a buffer from RNET's buffer pool
//!
//! @details   Same as 'rnet_alloc_bufW' except if not buffers
//! @details   are available, times out after duration.
//!
//! @param[in] 'timeout_ticks'-- If not buffer when called, wait
//! @param[in]     for this many OS clock ticks.
//! @param[in]     If 0 is passed in, if no buffer available when
//! @param[in]     called, returns NULL immediately.
//!
//! @details   Returns a buffer. Returns NULL if timeout/no buffer.
//!
rnet_buf_t *rnet_alloc_bufT(uint32_t timeout_ticks)
{
    rnet_buf_t *buf = NULL;
    nufr_sema_get_rtn_t rv;

    // Assume 'rnet_pool_init_done' is 'true'
    // If 'timeout_ticks'==0, must complete alloc immediately
    // 'void **' cast to supress compiler warning (hate doing it!)
    rv = nsvc_pool_allocateT(&rnet_buf_pool, (void **)&buf, timeout_ticks);

    if ((NUFR_SEMA_GET_OK_NO_BLOCK == rv) || (NUFR_SEMA_GET_OK_BLOCK == rv))
    {
        return buf;
    }

    return NULL;
}

//!
//! @name      rnet_alloc_pclW
//!
//! @brief     Allocate a particle chain SL particle pool
//!
//! @details   Only allocates a 1-particle long chain in this call.
//!
//! @details   Returns a particl. Never returns NULL.
//!
nsvc_pcl_t *rnet_alloc_pclW(void)
{
    nsvc_pcl_t *pcl_chain = NULL;
    nufr_sema_get_rtn_t rv;

    // Allocate a 1-pcl-long chain
    rv = nsvc_pcl_alloc_chainWT(&pcl_chain, NULL, 1, NSVC_PCL_NO_TIMEOUT);

    if ((NUFR_SEMA_GET_OK_NO_BLOCK == rv) || (NUFR_SEMA_GET_OK_BLOCK == rv))
    {
        return pcl_chain;
    }

    return NULL;
}

//!
//! @name      rnet_alloc_pclT
//!
//! @brief     Like 'rnet_alloc_bufT' but for particles
//!
//! @param[in] 'timeout_ticks'-- (see 'rnet_alloc_bufT')
//!
//! @details   Returns a buffer. Returns NULL if timeout/no buffer.
//!
nsvc_pcl_t *rnet_alloc_pclT(unsigned timeout_ticks)
{
    nsvc_pcl_t *pcl_chain = NULL;
    nufr_sema_get_rtn_t rv;
    int timeout_int = (int)timeout_ticks;

    // Make sure don't overflow int type
    // Assume 'timeout_ticks' converted to type int won't overflow
    SL_REQUIRE(timeout_int >= 0);

    // Allocate a 1-pcl-long chain; it'll grow as-needed later
    rv = nsvc_pcl_alloc_chainWT(&pcl_chain, NULL, 1, timeout_int);

    if ((NUFR_SEMA_GET_OK_NO_BLOCK == rv) || (NUFR_SEMA_GET_OK_BLOCK == rv))
    {
        return pcl_chain;
    }

    return NULL;
}

//!
//! @name      rnet_free_buf
//!
//! @brief     Free a RNET buffer pool buffer
//!
//! @param[in] 'buf'--
//!
void rnet_free_buf(rnet_buf_t *buf)
{
    nsvc_pool_free(&rnet_buf_pool, buf);
}

//!
//! @name      rnet_msg_rx_buf_entry
//!
//! @brief     RNET_ID_RX_BUF_ENTRY message handler
//!
//! @details   
//!
//! @param[in] 'buf'--
//!
void rnet_msg_rx_buf_entry(rnet_buf_t *buf)
{
    const rnet_intfc_rom_t *rom_intfc_ptr;
    rnet_l2_t intfc_l2_type;
    bool ok_path = false;
    unsigned options;

    if (NULL == buf)
    {
        return;
    }

    rom_intfc_ptr = rnet_intfc_get_rom((rnet_intfc_t)buf->header.intfc);
    if (NULL != rom_intfc_ptr)
    {
        intfc_l2_type = rom_intfc_ptr->l2_type;
        options = rom_intfc_ptr->option_flags;

        if (RNET_L2_PPP == intfc_l2_type)
        {
            bool pre_translated = (options & RNET_IOPT_RX_AHDLC_PRE_TRANSLATED) != 0;
            bool pre_crc_verified = (options & RNET_IOPT_RX_AHDLC_PRE_CRC_VERIFIED) != 0;

            if (pre_translated && pre_crc_verified)
            {
                rnet_msg_send(RNET_ID_RX_BUF_PPP, buf);
            }
            else if (pre_translated)
            {
                rnet_msg_send(RNET_ID_RX_BUF_AHDLC_VERIFY_CRC, buf);
            }
            else
            {
                rnet_msg_send(RNET_ID_RX_BUF_AHDLC_STRIP_CC, buf);
            }

            ok_path = true;
        }
    }

    if (!ok_path)
    {
        buf->header.code = 0;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
    }
}

//!
//! @name      rnet_msg_rx_pcl_entry
//!
//! @brief     RNET_ID_RX_PCL_ENTRY message handler
//!
//! @details   
//!
//! @param[in] 'buf'--
//!
void rnet_msg_rx_pcl_entry(nsvc_pcl_t *head_pcl)
{
    const rnet_intfc_rom_t *rom_intfc_ptr;
    nsvc_pcl_header_t      *header;
    rnet_l2_t intfc_l2_type;
    bool ok_path = false;
    unsigned options;

    if (NULL == head_pcl)
    {
        return;
    }

    header = NSVC_PCL_HEADER(head_pcl);

    rom_intfc_ptr = rnet_intfc_get_rom((rnet_intfc_t)header->intfc);
    if (NULL != rom_intfc_ptr)
    {
        intfc_l2_type = rom_intfc_ptr->l2_type;
        options = rom_intfc_ptr->option_flags;

        if (RNET_L2_PPP == intfc_l2_type)
        {
            bool pre_translated = (options & RNET_IOPT_RX_AHDLC_PRE_TRANSLATED) != 0;
            bool pre_crc_verified = (options & RNET_IOPT_RX_AHDLC_PRE_CRC_VERIFIED) != 0;

            if (pre_translated && pre_crc_verified)
            {
                rnet_msg_send(RNET_ID_RX_PCL_PPP, head_pcl);
            }
            else if (pre_translated)
            {
                rnet_msg_send(RNET_ID_RX_PCL_AHDLC_VERIFY_CRC, head_pcl);
            }
            else
            {
                rnet_msg_send(RNET_ID_RX_PCL_AHDLC_STRIP_CC, head_pcl);
            }

            ok_path = true;
        }
    }

    if (!ok_path)
    {
        header->code = 0;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
    }
}

//!
//! @name      rnet_msg_tx_buf_driver
//!
//! @brief     RNET_ID_TX_BUF_DRIVER message handler
//!
//! @details   
//!
//! @param[in] 'buf'--
//!
void rnet_msg_tx_buf_driver(rnet_buf_t *buf)
{
    rnet_intfc_t            intfc;

    intfc = buf->header.intfc;
    if (!rnet_intfc_is_valid(intfc))    
    {
        buf->header.code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_BUF_DISCARD, buf);
    }
    else
    {
    #if RNET_INTFC_CROSSCONNECT_TEST_MODE == 0
        const rnet_intfc_rom_t *rom_ptr;

        rom_ptr = rnet_intfc_get_rom(intfc);

        if (NULL != rom_ptr->tx_packet_api)
        {
            rom_ptr->tx_packet_api(intfc, buf, false);
        }
        else
        {
            rnet_msg_buf_discard(buf);
        }
    #else
        if (RNET_INTFC_TEST1 == intfc)
            intfc = RNET_INTFC_TEST2;
        else if (RNET_INTFC_TEST2 == intfc)
            intfc = RNET_INTFC_TEST1;

        buf->header.intfc = intfc;

        rnet_msg_send(RNET_ID_RX_BUF_ENTRY, buf);
    #endif
    }
}

//!
//! @name      rnet_msg_tx_pcl_driver
//!
//! @brief     RNET_ID_TX_PCL_DRIVER message handler
//!
//! @details   
//!
//! @param[in] 'head_pcl'--
//!
void rnet_msg_tx_pcl_driver(nsvc_pcl_t *head_pcl)
{
    rnet_intfc_t            intfc;
    nsvc_pcl_header_t      *header;

    header = NSVC_PCL_HEADER(head_pcl);

    intfc = header->intfc;
    if (!rnet_intfc_is_valid(intfc))    
    {
        header->code = RNET_BUF_CODE_METADATA_CORRUPTED;
        rnet_msg_send(RNET_ID_PCL_DISCARD, head_pcl);
    }
    else
    {
    #if RNET_INTFC_CROSSCONNECT_TEST_MODE == 0
        const rnet_intfc_rom_t *rom_ptr;

        rom_ptr = rnet_intfc_get_rom(intfc);

        if (NULL != rom_ptr->tx_packet_api)
        {
            rom_ptr->tx_packet_api(intfc, head_pcl, true);
        }
        else
        {
            rnet_msg_pcl_discard(head_pcl);
        }
    #else
        if (RNET_INTFC_TEST1 == intfc)
            intfc = RNET_INTFC_TEST2;
        else if (RNET_INTFC_TEST2 == intfc)
            intfc = RNET_INTFC_TEST1;

        header->intfc = intfc;

        rnet_msg_send(RNET_ID_RX_PCL_ENTRY, head_pcl);
    #endif
    }
}

//!
//! @name      rnet_msg_buf_discard
//!
//! @brief     RNET_ID_BUF_DISCARD message handler
//!
//! @details   Path for noisily discarding an RNET buffer   
//!
//! @param[in] 'buf'--
//!
void rnet_msg_buf_discard(rnet_buf_t *buf)
{
    rnet_free_buf(buf);
}

//!
//! @name      rnet_msg_pcl_discard
//!
//! @brief     RNET_ID_PCL_DISCARD message handler
//!
//! @details   
//!
//! @param[in] 'head_pcl'--
//!
void rnet_msg_pcl_discard(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_free_chain(head_pcl);
}