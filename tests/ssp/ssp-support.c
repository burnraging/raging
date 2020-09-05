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

//! @file     ssp-support.c
//! @authors  Bernie Woodland
//! @date     30Nov18
//!
//! @brief    Simple Serial Protocol Driver: example support task
//!

#include "raging-global.h"
#include "raging-utils-mem.h"
#include "raging-utils.h"
#include "raging-contract.h"

#include "ssp-support.h"

#define ONE_AT_A_TIME

uint8_t packet_loopback[500];
unsigned loopback_length;

uint8_t *ut_get_packet(unsigned packet_number, unsigned *length);
extern unsigned global_packet_number;

void packet_buffer_rx(ssp_buf_t *packet_buffer)
{
    uint8_t *byte_string;
	unsigned test_packet_length = 0;
	unsigned tx_packet_length = 0;
	unsigned cummulative_length = 0;

	rutils_memset(packet_loopback, 0, sizeof(packet_loopback));

	byte_string = ut_get_packet(global_packet_number, &test_packet_length);

	// Loop it back into Tx channel
	ssp_tx_queue_packet(packet_buffer);

#ifdef ONE_AT_A_TIME
	do
	{
		tx_packet_length = 0;
		ssp_tx_obtain_next_bytes(packet_buffer->header.channel_number,
			                     &packet_loopback[cummulative_length],
								 1,      //sizeof(packet_loopback) - cummulative_length,
								 &tx_packet_length);
		cummulative_length += tx_packet_length;
	} while (tx_packet_length > 0);
	tx_packet_length = cummulative_length;
#else
	(void)cummulative_length;

	ssp_tx_obtain_next_bytes(packet_buffer->header.channel_number,
		                     packet_loopback,
							 sizeof(packet_loopback),
							 &tx_packet_length);
#endif
	UT_ENSURE(test_packet_length == tx_packet_length);
	UT_ENSURE(rutils_memcmp(packet_loopback, byte_string, test_packet_length));
}

void packet_buffer_free(ssp_buf_t *packet_buffer)
{
	ssp_free_buffer_from_task(packet_buffer);
}

void ssp_message_handler(ssp_id_t id, uint32_t parameter)
{
    switch (id)
    {
    case SSP_ID_RX_MSG:
		packet_buffer_rx((ssp_buf_t *)parameter);
        break;

    case SSP_ID_FREE_MSG:
		packet_buffer_free((ssp_buf_t *)parameter);
        break;

	default:
		UT_ENSURE(false);
        break;
    }
}