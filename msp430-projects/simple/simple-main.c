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

//! @file    simple-main.c
//! @authors Bernie Woodland
//! @date    15Sep18
//!
//! @brief   MSP430 simple test program
//!

#include "msp430-peripherals.h"
#include "msp430-assembler.h"

#include "raging-global.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nsvc.h"
#include "nsvc-api.h"
// RNET is TBD
//#include "rnet-app.h"
//#include "rnet-dispatch.h"
//#include "rnet-intfc.h"

#include <msp430.h>

unsigned main_count;


int main(void)
{
    msp_init();

    // Enable interrupts.
    // No power saving modes (CPUOFF, OSCOFF, SCG0, SCG1 cleared).
    msp430asm_set_sr(GIE);

    // Always call nufr_init before enabling quantum timer
    nufr_init();
    nsvc_init();
    // Not using particls
    //nsvc_pcl_init();
    nsvc_timer_init(msp_qtm_retrieve_current_time, msp_qtm_reconfigure_by_task);
    nsvc_mutex_init();
    // Not using RNET yet...
    //rnet_create_buf_pool();
    //rnet_set_msg_prefix(NUFR_TID_BASE, NSVC_MSG_RNET_STACK);
    //rnet_intfc_init();

    nufr_launch_task(NUFR_TID_BASE, 0);
    nufr_launch_task(NUFR_TID_LOW, 0);

    while (1)
    {
        // Insert an BG Task processing here

        // ...Finished with BG Task processing.
        // Nothing to do, so go into sleep mode.
        // Don't set to mode 4, that'll turn off aux clock,
        //  which quantum timer needs when asleep
        // GCC: defined in in430.h
        __low_power_mode_3();
        main_count++;

        // Got switched back in. Context switch logic
        // cleared all SR power bits, undoing above call.
    }

}