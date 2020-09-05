//!
//!  @brief       MSP430 "simple" project
//!

#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-platform-export.h"
#include "nsvc-api.h"
//#include "rnet-top.h"

#include "base-task.h"
#include "global-msg-id.h"
#include "msp430-peripherals.h"


nsvc_timer_t *base_timer;

unsigned base_count;

static void base_task_msg_handler(base_id_t base_id);
static void global_msg_handler_for_base_task(global_msg_id_t global_id);


// Task for 'NUFR_TID_BASE'
void entry_base_task(unsigned parm)
{
    const unsigned    PREALLOC_COUNT = 3;
    nsvc_msg_prefix_t msg_prefix;
    uint16_t          msg_id_uint16;
    uint32_t          optional_parameter;
#if 0
    rnet_id_t         rnet_id;
#endif
    base_id_t         base_msg_id;
    global_msg_id_t   global_id;

    base_timer = nsvc_timer_alloc();

    // Start PPP negotiating. This will generate self-sent message
//    rnet_intfc_start_or_restart_l2(RNET_INTFC_USB_SERIAL1);

    // Self-sent message to start light blinking sequence
    // immediately.
    (void)nsvc_msg_send_argsW(NSVC_MSG_BASE_TASK,
                              ID_BASE_START,
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
#if 0
        case NSVC_MSG_RNET_STACK:
            rnet_id = (rnet_id_t)msg_id_uint16;

            if ((RNET_ID_RX_BUF_ENTRY == rnet_id) &&
                (NULL != optional_parameter))
            {
                // We received a packet from the rx driver, therefore
                //  must Replenish packet buffer back to driver.

                //rx_handler_enqueue_buf(1);
            }

            // Inject newly received packet into stack
            //rnet_msg_processor(rnet_id, optional_parameter);

            break;
#endif

        case NSVC_MSG_BASE_TASK:
            base_msg_id = (base_id_t)msg_id_uint16;
            base_task_msg_handler(base_msg_id);
            break;

        case NSVC_MSG_GLOBAL:
            global_msg_handler_for_base_task(global_id);
            break;

        default:
            break;
        }
    }
}

void base_start(unsigned delay_millisecs)
{
    base_timer->mode = NSVC_TMODE_CONTINUOUS;
    base_timer->duration = delay_millisecs;
    base_timer->msg_fields = NUFR_SET_MSG_FIELDS(
                                 NSVC_MSG_BASE_TASK,
                                 ID_BASE_TIMEOUT,
                                 nufr_self_tid(),
                                 NUFR_MSG_PRI_MID);
    base_timer->msg_parameter = 0;
    base_timer->dest_task_id = NUFR_TID_null;  //defaults to self

    nsvc_timer_start(base_timer);
}

void timer_timeout(void)
{
    base_count++;
}

static void base_task_msg_handler(base_id_t base_id)
{
    switch (base_id)
    {
    case ID_BASE_START:
        base_start(CONVERT_TO_AUX_TICKS(5000));
        break;
    case ID_BASE_TIMEOUT:
        timer_timeout();
        break;
    default:
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