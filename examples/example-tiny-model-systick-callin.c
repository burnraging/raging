/*
Copyright (c) 2019, Bernie Woodland
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

//! @file     example-tiny-model-systick-callin.c
//! @authors  Bernie Woodland
//! @date     14Dec19
//!
//! @brief   Example of how to use the tiny model w.r.t. to OS tick callin
//!
//! @details Since the tiny model doesn't have a SL, you have to make some
//! @details compromises and create timers and switch debouncers with
//! @details customized code. This shows you the best model for doing this.
//! @details 
//! @details 
//! @details 
//! @details 
//!

#include "nufr-global.h"
#include "nufr-api.h"


// Export these in other C files
unsigned tiny_timer1;
unsigned tiny_timer2;
unsigned tiny_timer3;

unsigned last_pass_switch1;
unsigned last_pass_switch2;

bool tiny_alternate_state;

void example_tiny_model_systick_callin(void)
{

    //**********  Code which executes once per clock tick  *******

    if (tiny_timer1 > 0)
    {
        tiny_timer1--;
        if (0 == tiny_timer1)
        {
            nufr_msg_send(NUFR_SET_MSG_FIELDS(PREFIX1,
                                              ID_A,
                                              NUFR_TID_null,
                                              NUFR_MSG_PRI_MID),
                          0,
                          NUFR_TID_FOO);
        }
    }
    if (tiny_timer2 > 0)
    {
        tiny_timer2--;
        if (0 == tiny_timer2)
        {
            nufr_msg_send(NUFR_SET_MSG_FIELDS(PREFIX2,
                                              ID_B,
                                              NUFR_TID_null,
                                              NUFR_MSG_PRI_MID),
                          NUFR_TID_FOO);
        }
    }

    //**********  Code which executes once every other clock tick *****
    // Do stuff at 50% OS clock rate for these reasons:
    // 1) For switch debounces, to ensure we have a long enough
    //    debounce interval. I prefer 20 millisecs over 10
    // 2) To save CPU cycles, in case we end up having a lot of timers.
    //    Timer resolution will be 1/2 of full-rate timers, of course.

    // Odd passes
    if (tiny_alternate_state)
    {
        if (tiny_timer3 > 0)
        {
            tiny_timer3--;
            if (0 == tiny_timer3)
            {
                nufr_msg_send(NUFR_SET_MSG_FIELDS(PREFIX3,
                                                  ID_C,
                                                  NUFR_TID_null,
                                                  NUFR_MSG_PRI_MID),
                              0,
                              NUFR_TID_FOO);
            }
        }
    }
    // Even passes
    // Do h/w switch stuff here.
    else
    {
        // Raw h/w switch posit reads
        bool switch1_on= HW_READ_SWITCH1_DI();
        bool switch2_on= HW_READ_SWITCH2_DI();

        // Switch1 just switched on event
        if (switch1_on && !last_pass_switch1)
        {
            nufr_msg_send(NUFR_SET_MSG_FIELDS(SWITCH1_PREFIX,
                                              ID_SWITCH1_ON_EVENT,
                                              NUFR_TID_null,
                                              NUFR_MSG_PRI_MID),
                          0,
                          NUFR_TID_FOO);
        }
        // Switch1 just switched off event
        else if (!switch1_on && last_pass_switch1)
        {
            nufr_msg_send(NUFR_SET_MSG_FIELDS(SWITCH1_PREFIX,
                                              ID_SWITCH1_OFF_EVENT,
                                              NUFR_TID_null,
                                              NUFR_MSG_PRI_MID),
                          0,
                          NUFR_TID_FOO);
        }

        // Switch2 just switched on event
        if (switch2_on && !last_pass_switch2)
        {
            nufr_msg_send(NUFR_SET_MSG_FIELDS(SWITCH2_PREFIX,
                                              ID_SWITCH2_ON_EVENT,
                                              NUFR_TID_null,
                                              NUFR_MSG_PRI_MID),
                          0,
                          NUFR_TID_FOO);
        }
        // Switch2 just switched off event
        else if (!switch2_on && last_pass_switch2)
        {
            nufr_msg_send(NUFR_SET_MSG_FIELDS(SWITCH2_PREFIX,
                                              ID_SWITCH2_OFF_EVENT,
                                              NUFR_TID_null,
                                              NUFR_MSG_PRI_MID),
                          0,
                          NUFR_TID_FOO);
        }

        // Update last-pass switch values
        last_pass_switch1 = switch1_on;
        last_pass_switch2 = switch2_on;
    }

    //****** Update state

    tiny_alternate_state = !tiny_alternate_state;
}


