//!
//!  @brief       MSP430 "simple" project
//!
//!  @details     Low-priority task
//!

#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-platform-export.h"
#include "nsvc-api.h"

#include "low-task.h"
#include "global-msg-id.h"
#include "msp430-peripherals.h"

#define LOW_TIMEOUT      CONVERT_TO_AUX_TICKS(2000)

nsvc_timer_t *low_task_timer;

uint32_t      some_random_value;
unsigned      low_task_count;

void low_task_start_timer(unsigned delay_millisecs);

static void low_task_msg_handler(low_id_t low_id);
static void global_msg_handler_for_low_task(global_msg_id_t global_id);


// Task for 'NUFR_TID_LOW'
void entry_low_task(unsigned parm)
{
    nsvc_msg_prefix_t msg_prefix;
    uint16_t          msg_id_uint16;
    uint32_t          optional_parameter;
    low_id_t          low_msg_id;
    global_msg_id_t   global_id;

    low_task_timer = nsvc_timer_alloc();

    low_task_start_timer(LOW_TIMEOUT);

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
        case NSVC_MSG_LOW_TASK:
            low_msg_id = (low_id_t)msg_id_uint16;
            low_task_msg_handler(low_msg_id);
            break;

        case NSVC_MSG_GLOBAL:
            global_id = (global_msg_id_t)msg_id_uint16;
            global_msg_handler_for_low_task(global_id);
            break;

        default:
            break;
        }
    }
}

void low_task_start_timer(unsigned delay_millisecs)
{
    low_task_timer->mode = NSVC_TMODE_SIMPLE;
    low_task_timer->duration = delay_millisecs;
    low_task_timer->msg_fields = NUFR_SET_MSG_FIELDS(
                                    NSVC_MSG_LOW_TASK,
                                    ID_LOW_TIMEOUT,
                                    nufr_self_tid(),
                                    NUFR_MSG_PRI_MID);
    low_task_timer->msg_parameter = 0;
    low_task_timer->dest_task_id = NUFR_TID_null;  //defaults to self

    nsvc_timer_start(low_task_timer);
}

void crunch_prime_numbers(void)
{
    uint32_t i;

    // TBD
    for (i = 0; i < 10000; i++)
    {
        some_random_value++;
    }

    low_task_count++;

    low_task_start_timer(LOW_TIMEOUT);
}

static void low_task_msg_handler(low_id_t low_id)
{
    switch (low_id)
    {
    case ID_LOW_TIMEOUT:
        crunch_prime_numbers();
        break;
    default:
        break;
    }
}

static void global_msg_handler_for_low_task(global_msg_id_t global_id)
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