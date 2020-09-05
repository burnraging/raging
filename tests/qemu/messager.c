//!
//!  @brief       Sleeper project
//!

#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-platform-export.h"
#include "nufr-platform.h"
#include "nsvc-api.h"

#if QEMU_PROJECT == 2

//********* Knight Rider Light Bar *********/
/*
  This project lights up a sequence of lights, then
  shuffles them back and forth.

  something like:  
  https://www.youtube.com/watch?v=YxIYguP4GXs
*/

// total num. lights.
// Indexes range from 0 - NUM_LIGHTS-1
#define NUM_LIGHTS        10
// how many lights are lit at any given time
#define CLUSTER_SIZE       4



typedef enum
{
    ID_CONTROL_TICK,
    ID_CONTROL_TBD
} id_control_t;

typedef enum
{
    ID_EVENT_RUN_SEQUENCE,
    ID_EVENT_TURN_OFF_SEQUENCE,
    ID_EVENT_TIMER_EXPIRED,
} id_event_t;

typedef enum
{
    ID_STATE_TBD1,
    ID_STATE_TBD2,
} id_state_t;

void send_control_msg(id_control_t id);
void send_event_msg(id_event_t id);
void send_state_msg(id_state_t id);
void start_event_timer(uint32_t delayMillisecs);
void change_single_light(unsigned light_index, bool on);

uint32_t free_running_control_count;

nsvc_timer_t *control_timer;
nsvc_timer_t *event_timer;



// Control task. Highest priority task. It gets a tick msg once
// every OS tick. It 
void entry_01(unsigned parm)
{
    uint32_t       fields = 0;
    id_control_t   id;
    bool           pulse;
    bool           is_running = false;
    const uint32_t STROBE_INTERVAL_SECONDS = 10;

    UNUSED(parm);

    nufr_launch_task(NUFR_TID_EVENT_TASK, 0);
    nufr_launch_task(NUFR_TID_STATE_TASK, 0);

    control_timer = nsvc_timer_alloc();

    // Set up timer to expire once every OS tick
    control_timer->duration = NUFR_TICK_PERIOD;
    control_timer->mode = NSVC_TMODE_CONTINUOUS;
    control_timer->msg_fields = NUFR_SET_MSG_FIELDS(
                                    NSVC_MSG_PREFIX_CONTROL,
                                    ID_CONTROL_TICK,
                                    nufr_self_tid(),
                                    NUFR_MSG_PRI_MID);
    control_timer->msg_parameter = 0;
    control_timer->dest_task_id = NUFR_TID_null;  //defaults to self

    nsvc_timer_start(control_timer);

    while (1)
    {
        nufr_msg_getW(&fields, NULL);

        id = (id_control_t) NUFR_GET_MSG_ID(fields);

        switch (id)
        {
        case ID_CONTROL_TICK:
            free_running_control_count++;

            pulse = free_running_control_count %
                  (STROBE_INTERVAL_SECONDS * MILLISECS_PER_SEC / NUFR_TICK_PERIOD) == 0;

            // Time to turn display on or off?
            if (pulse)
            {
                if (!is_running)
                    send_event_msg(ID_EVENT_RUN_SEQUENCE);
                else
                    send_event_msg(ID_EVENT_TURN_OFF_SEQUENCE);

                is_running = !is_running;
            }

            break;

        case ID_CONTROL_TBD:
            break;
        }
    }
}



void entry_event_task(unsigned parm)
{
    uint32_t       fields = 0;
    id_event_t     id;
    unsigned       i;
    unsigned       trailing_index;
    unsigned       leading_index;
    bool           moving_left_now = false;
    const unsigned MOVE_TIME_MILLISECS = 200;

    UNUSED(parm);

    event_timer = nsvc_timer_alloc();

    while (1)
    {
        nufr_msg_getW(&fields, NULL);

        id = (id_event_t) NUFR_GET_MSG_ID(fields);

        switch (id)
        {
        case ID_EVENT_RUN_SEQUENCE:

            for (i = 0; i < CLUSTER_SIZE; i++)
            {
                change_single_light(i, true);
            }

            // indexes are current lit start/end
            // 'trailing_index' is always leftmost; 'leading_index'
            // is always rightmost, regardless of direction moving in
            trailing_index = 0;
            leading_index = CLUSTER_SIZE - 1;

            moving_left_now = false;

            start_event_timer(MOVE_TIME_MILLISECS);

            break;

        case ID_EVENT_TURN_OFF_SEQUENCE:
            (void)nsvc_timer_kill(event_timer);

            // hack: corner-case where there might be a timer
            //        expired msg waiting in queue;
            nufr_msg_drain(nufr_self_tid(), NUFR_MSG_PRI_MID);

            // just turn everything off
            for (i = 0; i < NUM_LIGHTS; i++)
            {
                change_single_light(i, true);
            }

            break;

        case ID_EVENT_TIMER_EXPIRED:

            if (moving_left_now && (trailing_index == 0))
            {
                moving_left_now = false;
            }
            else if (!moving_left_now && (leading_index == (NUM_LIGHTS - 1)))
            {
                moving_left_now = true;
            }

            if (moving_left_now)
            {
                change_single_light(leading_index, false);

                trailing_index--;
                leading_index--;

                change_single_light(trailing_index, true);
            }
            else
            {
                change_single_light(trailing_index, false);

                trailing_index++;
                leading_index++;

                change_single_light(leading_index, true);
            }

            // restart timer
            start_event_timer(MOVE_TIME_MILLISECS);

            break;
        }
    }
}



void entry_state_task(unsigned parm)
{
    uint32_t   fields = 0;
    id_state_t id;

    UNUSED(parm);

    while (1)
    {
        nufr_msg_getW(&fields, NULL);

        id = (id_state_t) NUFR_GET_MSG_ID(fields);

        switch (id)
        {
        case ID_STATE_TBD1:
            break;

        case ID_STATE_TBD2:
            break;
        }
    }
}

// Message send convenience wrappers

void send_control_msg(id_control_t id)
{
    uint32_t    fields = 0;

    fields = NUFR_SET_MSG_FIELDS(
                      NSVC_MSG_PREFIX_CONTROL,
                      id,
                      nufr_self_tid(),
                      NUFR_MSG_PRI_MID);

    (void)nufr_msg_send(fields, 0, NUFR_TID_01);
}

void send_event_msg(id_event_t id)
{
    uint32_t    fields = 0;

    fields = NUFR_SET_MSG_FIELDS(
                      NSVC_MSG_PREFIX_EVENT,
                      id,
                      nufr_self_tid(),
                      NUFR_MSG_PRI_MID);

    (void)nufr_msg_send(fields, 0, NUFR_TID_EVENT_TASK);
}

void send_state_msg(id_state_t id)
{
    nufr_msg_send(NUFR_SET_MSG_FIELDS(
                          NSVC_MSG_PREFIX_STATE,
                          id,
                          nufr_self_tid(),
                          NUFR_MSG_PRI_MID),
                  0,
                  NUFR_TID_STATE_TASK);
}

// Timer convenience wrappers

void start_event_timer(uint32_t delayMillisecs)
{
    event_timer->duration = delayMillisecs;
    event_timer->mode = NSVC_TMODE_SIMPLE;
    event_timer->msg_fields = NUFR_SET_MSG_FIELDS(
                                  NSVC_MSG_PREFIX_EVENT,
                                  ID_EVENT_TIMER_EXPIRED,
                                  nufr_self_tid(),
                                  NUFR_MSG_PRI_MID);
    event_timer->msg_parameter = 0;
    event_timer->dest_task_id = NUFR_TID_null;  //defaults to self

    nsvc_timer_start(event_timer);
}

// Set light indicated by 'light_index' on if 'on'==TRUE,
// off if 'on'==FALSE. Don't touch other lights
void change_single_light(unsigned light_index, bool on)
{
    // TBD

    UNUSED(light_index);
    UNUSED(on);
}

#endif