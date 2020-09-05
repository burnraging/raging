
#include "raging-global.h"
#include "raging-contract.h"
#include "raging-utils-mem.h"
#include "ssp-driver.h"


// Packet of single byte == 0x2
// Last 2 bytes are CRC from https://crccalc.com/
// Using CRC-16/X-25. And don't forget, it's little-endian
uint8_t packet1[] = { 0x7E, 0xA5, 0x00, 0x3, 0x2, 0x6A, 0xD3};

// Garbled nonsense
uint8_t packet2[] = { 18, 25, 242, 73, 12, 22, 22, 89, 15, 0, 0, 0, 12 };

// Bad CRC
uint8_t packet3[] = { 0x7E, 0xA5, 0x00, 0x5, 0x1, 0x2, 0x3, 0xAA, 0xBB};

// Good CRC
uint8_t packet4[] = { 0x7E, 0xA5, 0x00, 0x5, 0x1, 0x2, 0x3, 0x3B, 0x9D};

void message_loop(unsigned interval_millisecs);

unsigned ut_get_num_test_packets(void)
{
	return 4;
}

uint8_t *ut_get_packet(unsigned packet_number, unsigned *length)
{
	uint8_t *ptr = NULL;
	*length = 0;

	if (packet_number == 1)
	{
		ptr = packet1;
		*length = sizeof(packet1);
	}
	else if (packet_number == 2)
	{
		ptr = packet2;
		*length = sizeof(packet2);
	}
	else if (packet_number == 3)
	{
		ptr = packet3;
		*length = sizeof(packet3);
	}
	else if (packet_number == 4)
	{
		ptr = packet4;
		*length = sizeof(packet4);
	}

	return ptr;
}

uint8_t multi_pkt[] = { 1, 2, 3, 4, 5};
uint8_t multi_holder[200];

void ssp_tx_multi(void)
{
	unsigned   i;
	unsigned   fill_length = 0;
	unsigned   total_length = 0;
	unsigned   at_a_time;
	ssp_buf_t *packet_buffer;
	unsigned   channel_number = 1;

	for (i = 0; i < 3; i++)
	{
		packet_buffer = ssp_allocate_buffer_from_taskW(channel_number);

		rutils_memcpy(SSP_FREE_PAYLOAD_PTR(packet_buffer),
			          multi_pkt,
					  sizeof(multi_pkt));

		packet_buffer->header.length += sizeof(multi_pkt);

		ssp_tx_queue_packet(packet_buffer);
	}

#if 0
	(void)total_length;
	(void)at_a_time;

	ssp_tx_obtain_next_bytes(channel_number,
		                     multi_holder,
							 sizeof(multi_holder),
							 &fill_length);
#else

	at_a_time = 1;
	for (i = 0; i < 15; i += at_a_time)
	{
		fill_length = 0;
	    ssp_tx_obtain_next_bytes(channel_number,
			                     &multi_holder[total_length],
								 at_a_time,
								 &fill_length);
		UT_ENSURE(at_a_time == fill_length);
		total_length += fill_length;
	}

	at_a_time = 2;
	for (i = 0; i < 10; i += at_a_time)
	{
		fill_length = 0;
	    ssp_tx_obtain_next_bytes(channel_number,
			                     &multi_holder[total_length],
								 at_a_time,
								 &fill_length);
		UT_ENSURE(at_a_time == fill_length);
		total_length += fill_length;
	}

	at_a_time = 1;
	for (i = 0; i < 8; i += at_a_time)
	{
		fill_length = 0;
	    ssp_tx_obtain_next_bytes(channel_number,
			                     &multi_holder[total_length],
								 at_a_time,
								 &fill_length);
		UT_ENSURE(at_a_time == fill_length);
		total_length += fill_length;
	}

	UT_ENSURE(33 == total_length);

#endif

	// Must empty message queue and packet buffers
	message_loop(1);
}