//!
//!  @brief       Disco board project
//!

#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-platform-export.h"
#include "raging-contract.h"
#include "nsvc-api.h"
#include "rnet-top.h"
#include "ssp-driver.h"

#include "base-task.h"
#include "global-msg-id.h"
#include "rx-driver.h"
#include "tx-task.h"
#include "ssp-driver.h"
#include "ssp-assignments.h"

#include "bsp.h"

#include "disco-feature-switches.h"


nsvc_timer_t *led_timer;

bsp_leds_t current_led = LED_GREEN;

static void led_msg_handler(led_id_t led_id);
static void ssp_msg_handler(ssp_rx_id_t ssp_id, ssp_buf_t *buf);
static void global_msg_handler_for_base_task(global_msg_id_t global_id);


// Task for 'NUFR_TID_BASE'
void entry_base_task(unsigned parm)
{
#if DISCO_CS_RNET==1
    const unsigned    PREALLOC_COUNT = 3;
#endif
    nsvc_msg_prefix_t msg_prefix;
    uint16_t          msg_id_uint16;
    uint32_t          optional_parameter;
    rnet_id_t         rnet_id;
    led_id_t          led_id;
    global_msg_id_t   global_id;

    UNUSED(parm);

    led_timer = nsvc_timer_alloc();

#if DISCO_CS_RNET==1
    // Init the rx driver
    rx_handler_init(NUFR_SET_MSG_FIELDS(NSVC_MSG_RNET_STACK,
                                        RNET_ID_RX_BUF_ENTRY,
                                        nufr_self_tid(),
                                        NUFR_MSG_PRI_MID),
                    nufr_self_tid(),
                    RNET_INTFC_USB_SERIAL1);
    rx_handler_enqueue_buf(PREALLOC_COUNT);

    // Start PPP negotiating. This will generate self-sent message
    rnet_intfc_start_or_restart_l2(RNET_INTFC_USB_SERIAL1);
#endif

    // Self-sent message to start light blinking sequence
    // immediately.
    (void)nsvc_msg_send_argsW(NSVC_MSG_BLINK_LEDS,
                              ID_LED_START,
                              NUFR_MSG_PRI_MID,
                              NUFR_TID_null,
                              0);                          

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
        case NSVC_MSG_RNET_STACK:
            rnet_id = (rnet_id_t)msg_id_uint16;

            if ((RNET_ID_RX_BUF_ENTRY == rnet_id) &&
                (0 != optional_parameter))
            {
                // We received a packet from the rx driver, therefore
                //  must Replenish packet buffer back to driver.

                rx_handler_enqueue_buf(1);
            }

            // Inject newly received packet into stack
            rnet_msg_processor(rnet_id, optional_parameter);

            break;

        case NSVC_MSG_BLINK_LEDS:
            led_id = (led_id_t)msg_id_uint16;
            led_msg_handler(led_id);
            break;

        case NSVC_MSG_SSP_RX:
            ssp_msg_handler((ssp_rx_id_t)msg_id_uint16, (ssp_buf_t *)optional_parameter);
            break;

        case NSVC_MSG_GLOBAL:
            global_id = (global_msg_id_t)msg_id_uint16;
            global_msg_handler_for_base_task(global_id);
            break;

        default:
            break;
        }
    }
}

void led_start(unsigned delay_millisecs)
{
    bsp_led_disable(Green_Led);
    bsp_led_disable(Red_Led);

    led_timer->mode = NSVC_TMODE_CONTINUOUS;
    led_timer->duration = delay_millisecs;
    led_timer->msg_fields = NUFR_SET_MSG_FIELDS(
                                 NSVC_MSG_BLINK_LEDS,
                                 ID_LED_TIMEOUT,
                                 nufr_self_tid(),
                                 NUFR_MSG_PRI_MID);
    led_timer->msg_parameter = 0;
    led_timer->dest_task_id = NUFR_TID_null;  //defaults to self

    nsvc_timer_start(led_timer);
}

void led_timeout(void)
{
    if (Green_Led == current_led)
    {
        bsp_led_disable(Green_Led);
        bsp_led_enable(Red_Led);

        current_led = Red_Led;
    }
    else
    {
        bsp_led_disable(Red_Led);
        bsp_led_enable(Green_Led);

        current_led = Green_Led;
    }
}

static void led_msg_handler(led_id_t led_id)
{
    switch (led_id)
    {
    case ID_LED_START:
        led_start(500);
        break;
    case ID_LED_TIMEOUT:
        led_timeout();
        break;
    default:
        break;
    }
}

void ssp_rx(ssp_rx_id_t ssp_id, ssp_buf_t *buf)
{
    uint8_t               *payload_ptr;
    ssp_dapp_wellknowns_t  dapp;

    (void)ssp_id;

    APP_REQUIRE_API(NULL != buf);

    payload_ptr = SSP_PAYLOAD_PTR(buf);

    // Must have an L3 header min: dest app + circuit
    if (buf->header.length < 2)
    {
        ssp_free_buffer_from_task(buf);
        return;
    }

    dapp = *payload_ptr;

    // Echo request packet?
    switch (dapp)
    {
    case SSP_DAPP_CLEAR_ECHO_REQUEST:

        // Convert packet to echo response and send back
        *payload_ptr = SSP_DAPP_CLEAR_ECHO_RESPONSE;

        nsvc_msg_send_argsW(NSVC_MSG_SSP_TX,
                            ID_TX_SSP_PACKET_SEND,
                            NUFR_MSG_PRI_MID,
                            NUFR_TID_null,
                            (uint32_t)buf);

        break;

    // Bad packet, discard
    default:
        
        ssp_free_buffer_from_task(buf);

        break;
    }
}

static void ssp_msg_handler(ssp_rx_id_t ssp_id, ssp_buf_t *buf)
{
    switch (ssp_id)
    {
    case ID_SSP_RX_ENTRY:
        ssp_rx(ssp_id, buf);
        break;

    default:
        // Safety
        if (NULL != buf)
        {
            ssp_free_buffer_from_task(buf);
        }
        break;
    }
}

static void global_msg_handler_for_base_task(global_msg_id_t global_id)
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