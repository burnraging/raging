//!
//!  @brief       Disco board project
//!

#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-platform-export.h"
#include "nsvc-api.h"
#include "rnet-top.h"
#include "rnet-dispatch.h"
#include "ssp-driver.h"
#include "raging-contract.h"

#include "tx-task.h"
#include "global-msg-id.h"

#include "bsp.h"

#include "disco-feature-switches.h"


rnet_buf_t *tx_queue_head;
rnet_buf_t *tx_queue_tail;

//#define USING_RNET_TEST_VECTOR

#ifdef  USING_RNET_TEST_VECTOR
// Extracted from intervaltty of LCP config request send from pppd
uint8_t pppd_conf_req[] = {
    0x7e, 0xff, 0x7d, 0x23, 0xc0, 0x21, 0x7d, 0x21, 0x7d, 0x21, 0x7d,
    0x20, 0x7d, 0x30, 0x7d, 0x22, 0x7d, 0x26, 0x7d, 0x20, 0x7d, 0x20,
    0x7d, 0x20, 0x7d, 0x20, 0x7d, 0x25, 0x7d, 0x26, 0x95, 0xc2, 0xd5,
    0x2b, 0xbc, 0x7d, 0x36, 0x7e
};
#endif

static void ssp_tx_msg_handler(id_tx_ssp_t msg_id, ssp_buf_t *buf);
static void tx_msg_handler(id_tx_t msg_id, rnet_buf_t *buf);
static void global_msg_handler_for_tx_task(global_msg_id_t global_id);

// Task for 'NUFR_TID_TX'
void entry_tx_task(unsigned parm)
{
    nsvc_msg_prefix_t msg_prefix;
    uint16_t          msg_id_uint16;
    uint32_t          optional_parameter;
    global_msg_id_t   global_id;

    UNUSED(parm);

    // Message pump
    while (1)
    {
        nsvc_msg_get_argsW(&msg_prefix,
                           &msg_id_uint16,
                           NULL,
                           NULL,
                           &optional_parameter);

        switch (msg_prefix)
        {
        case NSVC_MSG_SSP_TX:
            ssp_tx_msg_handler((id_tx_ssp_t)msg_id_uint16,
                               (ssp_buf_t *)optional_parameter);
            break;

        case NSVC_MSG_TX:
            tx_msg_handler((id_tx_t)msg_id_uint16,
                           (rnet_buf_t *)optional_parameter);
            break;

        case NSVC_MSG_GLOBAL:
            global_id = (global_msg_id_t)msg_id_uint16;
            global_msg_handler_for_tx_task(global_id);
            break;

        default:
            break;
        }
    }
}

void ssp_tx_send_packet(ssp_buf_t *buf)
{
    uint8_t  local_buffer[20];
    unsigned channel_number;
    unsigned bytes_to_tx = 0;
    unsigned i;

    APP_REQUIRE_API(NULL != buf);

    channel_number = buf->header.channel_number;

    ssp_tx_queue_packet(buf);

    while (1)
    {
        ssp_tx_obtain_next_bytes(channel_number,
                                 local_buffer,
                                 sizeof(local_buffer),
                                 &bytes_to_tx);

        if (0 == bytes_to_tx)
        {
            break;
        }

        for (i = 0; i < bytes_to_tx; i++)
        {
            bsp_uart_send(local_buffer[i]);
        }
    }
}

// Pass buffer to tx task. Called by other tasks.
// Uses RNET's buffer pool
void tx_send_packet(rnet_intfc_t intfc, rnet_buf_t *buf, bool is_pcl)
{
    nsvc_msg_send_return_t send_rv;

    UNUSED(intfc);
    UNUSED(is_pcl);

    // If RNET wasn't enabled, then RNET buffer pool wasn't initialized
    APP_REQUIRE_API(DISCO_CS_RNET==1);
    APP_REQUIRE_API(NULL != buf);

    send_rv = nsvc_msg_send_argsW(NSVC_MSG_TX,
                                  ID_TX_PACKET_SEND,
                                  NUFR_MSG_PRI_MID,
                                  NUFR_TID_null,
                                  (uint32_t)buf);                          

    if ((NSVC_MSRT_DEST_NOT_FOUND == send_rv) ||
        (NSVC_MSRT_ERROR == send_rv))
    {
        rnet_free_buf(buf);
    }
}

void ssp_tx_buffer_discard(ssp_buf_t *buf)
{
    APP_REQUIRE_API(NULL != buf);

    ssp_free_buffer_from_task(buf);
}

static void ssp_tx_msg_handler(id_tx_ssp_t msg_id, ssp_buf_t *buf)
{
    APP_REQUIRE_API(DISCO_CS_SSP==1);

    switch (msg_id)
    {
    case ID_TX_SSP_PACKET_SEND:
        ssp_tx_send_packet(buf);
        break;

    case ID_TX_SSP_BUFFER_DISCARD:
        ssp_tx_buffer_discard(buf);
        break;

    default:
        break;
    }
}

static void tx_msg_handler(id_tx_t msg_id, rnet_buf_t *buf)
{
    nsvc_msg_send_return_t send_rv;
    rnet_buf_t            *buf_being_sent;
    uint8_t               *ptr;
    unsigned               length;
    unsigned               i;
    uint8_t                character;

    APP_REQUIRE_API(DISCO_CS_RNET==1);

    switch (msg_id)
    {
    case ID_TX_PACKET_SEND:

        // Enqueue to empty queue
        if (NULL == tx_queue_head)
        {
            tx_queue_head = buf;
            tx_queue_tail = buf;

            send_rv = nsvc_msg_send_argsW(NSVC_MSG_TX,
                                          ID_TX_RESTART_TRANSMIT,
                                          NUFR_MSG_PRI_MID,
                                          nufr_self_tid(),
                                          0);
            UNUSED(send_rv);
        }
        // Enqueue to non-empty queue
        else
        {
            tx_queue_tail->flink = buf;
            tx_queue_tail = buf;
        }

        break;

    case ID_TX_RESTART_TRANSMIT:

        // Do while queue has 1 or more packet
        while (NULL != tx_queue_head)
        {
            // Current buf is head
            buf_being_sent = tx_queue_head;

            // Dequeue from head
            tx_queue_head = tx_queue_head->flink;
            if (NULL == tx_queue_head)
            {
                tx_queue_tail = NULL;
            }
            buf_being_sent->flink = NULL;

            // Sanity check: make sure packet is supposed to go out our
            // interface
            if (RNET_INTFC_USB_SERIAL1 != buf_being_sent->header.intfc)
            {
                rnet_free_buf(buf_being_sent);
                continue;
            }

            length = buf_being_sent->header.length;

            // Sanity check that offset and length are sane
            if (buf_being_sent->header.offset + length >= RNET_BUF_SIZE)
            {
                rnet_free_buf(buf_being_sent);
                continue;
            }

        #ifndef USING_RNET_TEST_VECTOR
            ptr = RNET_BUF_FRAME_START_PTR(buf_being_sent);
        #else
            ptr = pppd_conf_req;
            length = sizeof(pppd_conf_req);
        #endif

            // Send a byte at a time to BSP
            for (i = 0; i < length; i++)
            {
                character = *ptr++;

            // Quick hack to disable RNET tx packets, which
            // happen automatically with PPP negotiations
            #if DISCO_CS_SSP == 0
                bsp_uart_send(character);
            #else
                UNUSED(character);
            #endif
            }

            rnet_free_buf(buf_being_sent);
        }

        break;

    default:
        break;
    }
}

static void global_msg_handler_for_tx_task(global_msg_id_t global_id)
{
    switch (global_id)
    {
    case GLOBAL_ID_SHUTDOWN:
        //tbd
        break;
    default:
        break;
    }
}