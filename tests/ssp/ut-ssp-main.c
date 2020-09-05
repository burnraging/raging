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

//! @file     ut-ssp-main.c
//! @authors  Bernie Woodland
//! @date     30Nov18
//!
//! @brief    Unit test of SSP
//!


#include "raging-global.h"
#include "raging-contract.h"
#include "raging-utils.h"
#include "nsvc.h"
#include "nsvc-api.h"
#include "nufr-platform.h"
#include "nufr-kernel-message-blocks.h"

#include "ssp-driver.h"
#include "ssp-support.h"


#include <string.h>

// Tests
unsigned ut_get_num_test_packets(void);
uint8_t *ut_get_packet(unsigned packet_number, unsigned *length);
void ssp_tx_multi(void);

unsigned global_packet_number;


unsigned os_ticks_remaining;
unsigned millisecs_remaining;
unsigned elapsed_time_for_timer;


extern nufr_tcb_t nufr_tcb_block[NUFR_NUM_TASKS];
extern nufr_tcb_t *nufr_running;


// Run simulated real-time.
// Extract messages from message pump.
// Increment timers.
void message_loop(unsigned interval_millisecs)
{
    uint32_t    fields = 0;
    uint32_t    parameter = 0;
    ssp_id_t    id;
    uint32_t    reconfigured_time_ptr;

    os_ticks_remaining = NUFR_MILLISECS_TO_TICKS(interval_millisecs);
    millisecs_remaining = os_ticks_remaining * NUFR_TICK_PERIOD;

    do {

        // Exhaust all extant messages (This won't work in non UT nufr code!)
        while (NULL != nufr_msg_peek())
        {
            nufr_msg_getW(&fields, &parameter);

            if ((BIT_MASK32 == fields) && (BIT_MASK32 == parameter))
            {
                break;
            }

            id = (ssp_id_t)NUFR_GET_MSG_ID(fields);

            ssp_message_handler(id, parameter);
        }

        // This will time out timers
        (void)nsvc_timer_expire_timer_callin(elapsed_time_for_timer, &reconfigured_time_ptr);

        elapsed_time_for_timer += NUFR_TICK_PERIOD;

		if (0 == os_ticks_remaining)
		{
			break;
		}

        os_ticks_remaining--;
        millisecs_remaining = os_ticks_remaining * NUFR_TICK_PERIOD;

    } while (os_ticks_remaining > 0);
}

int main(void)
{
    nsvc_msg_fields_unary_t rx_fields[SSP_NUM_CHANNELS];
    nsvc_msg_fields_unary_t tx_fields[SSP_NUM_CHANNELS];
	unsigned       i, k;
	unsigned       num_packets;
	unsigned       packet_length = 0;

    // Fake out code for nsvc-timer.c's use later
    nufr_running = &nufr_tcb_block[0];

    // nufr, SL, RNET initializations
    nufr_msg_bpool_init();
    nsvc_init();
    nsvc_pcl_init();
    nsvc_timer_init(nufrplat_systick_get_reference_time, NULL);

    rx_fields[0].prefix = NSVC_MSG_PREFIX_A;
    rx_fields[0].id = SSP_ID_RX_MSG;
    rx_fields[0].priority = NUFR_MSG_PRI_MID;
    rx_fields[0].sending_task = NUFR_TID_null;
    rx_fields[0].destination_task = NUFR_TID_01;
    rx_fields[0].optional_parameter = 0;      // not used
  
    rx_fields[1].prefix = NSVC_MSG_PREFIX_A;
    rx_fields[1].id = SSP_ID_RX_MSG;
    rx_fields[1].priority = NUFR_MSG_PRI_MID;
    rx_fields[1].sending_task = NUFR_TID_null;
    rx_fields[1].destination_task = NUFR_TID_01;
    rx_fields[1].optional_parameter = 0;      // not used
  
    tx_fields[0].prefix = NSVC_MSG_PREFIX_A;
    tx_fields[0].id = SSP_ID_FREE_MSG;
    tx_fields[0].priority = NUFR_MSG_PRI_MID;
    tx_fields[0].sending_task = NUFR_TID_null;
    tx_fields[0].destination_task = NUFR_TID_01;
    tx_fields[0].optional_parameter = 0;      // not used
  
    tx_fields[1].prefix = NSVC_MSG_PREFIX_A;
    tx_fields[1].id = SSP_ID_FREE_MSG;
    tx_fields[1].priority = NUFR_MSG_PRI_MID;
    tx_fields[1].sending_task = NUFR_TID_null;
    tx_fields[1].destination_task = NUFR_TID_01;
    tx_fields[1].optional_parameter = 0;      // not used

	ssp_init(rx_fields, tx_fields);

	// Tests
	num_packets = ut_get_num_test_packets();
	for (i = 1; i <= num_packets; i++)
	{
		uint8_t *byte_string = ut_get_packet(i, &packet_length);

		global_packet_number = i;

		for (k = 0; k < packet_length; k++)
		{
			ssp_rx_entry(&ssp_desc[0], byte_string[k]);
		}

		// Run for x millisecs of simulated real-time
		message_loop(1);
	}
    
	ssp_tx_multi();

    return 0;
}