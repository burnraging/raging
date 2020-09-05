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

#include "nufr-global.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nsvc.h"
#include "nufr-api.h"
#include "nsvc-app.h"
#include "nsvc-api.h"

#include "nufr-kernel-task.h"  // for 'nufr_running'

#include "raging-contract.h"
#include "raging-utils-mem.h"
#include "raging-utils-os.h"

#include <string.h>

// duplicated from source
#define UART_ID_GOOD_PACKET                1
#define UART_ID_DISCARD_PACKET             1
#define NUFR_TID_UART_TASK     (nufr_tid_t)1
void uart_irq_packet_start(void);
void uart_irq_packet_end(void);
void uart_irq_data_rx(uint8_t *data_ptr, unsigned data_length);

static uint8_t unique_string[510];

// fixme: should have a new file for this
#include "raging-utils.h"
static void test_fifo(void)
{
    rutils_fifo_t fifo;
    uint8_t buffer[20];
    uint8_t write_data[25];
    uint8_t read_data[25];
    unsigned write_length;
    unsigned read_length;
    unsigned i;

    rutils_fifo_init(&fifo, buffer, sizeof(buffer));

    rutils_memset(write_data, 0, sizeof(write_data));
    rutils_memset(read_data, 0, sizeof(read_data));
    write_data[0] = 1;    write_data[1] = 2;    write_data[2] = 3;
    for (i = 0; i < 7; i++)
        write_length = rutils_fifo_write(&fifo, write_data, 3);
    read_length = rutils_fifo_read(&fifo, read_data, 3);

}

// Load a pattern into 'unique_string'
static void create_unique_string(void)
{
    unsigned i;

    for (i = 0; i < sizeof(unique_string); i++)
    {
        unique_string[i] = (uint8_t)(i & 0xFF);
    }
}

void test_pcl_irq_simple_write(void)
{
    uint32_t    fields = 0;
    uint32_t    parameter = 0;
    nsvc_pcl_t *chain;
    nsvc_pcl_chain_seek_t read_seeker;
    const unsigned READ1_LENGTH = 10;
    const unsigned READ2_LENGTH = 3;
    uint8_t temp_data[13];   // == READ1+2_LENGTH
    unsigned read_count;

    nufr_init();
    nsvc_init();
    nsvc_pcl_init();

    // hack to make us look like we're the right context
    nufr_running = NUFR_TID_TO_TCB(NUFR_TID_UART_TASK);
    nufr_running->block_flags &= BITWISE_NOT8(NUFR_TASK_NOT_LAUNCHED);

    uart_irq_packet_start();
    uart_irq_data_rx(unique_string, READ1_LENGTH);
    // Must have 2nd write, since msg alloc is only on 2nd call
    uart_irq_data_rx(&unique_string[READ1_LENGTH], READ2_LENGTH);
    uart_irq_packet_end();

    nufr_msg_getW(&fields, &parameter);
    chain = (nsvc_pcl_t *)parameter;
    UT_ENSURE(NULL != chain);

    // An offset of zero will automatically skip over chain head header
    nsvc_pcl_set_seek_to_packet_offset(chain, &read_seeker, 0);
    read_count = nsvc_pcl_read(&read_seeker, temp_data, sizeof(temp_data));
    UT_ENSURE(sizeof(temp_data) == read_count);
    UT_ENSURE(memcmp(temp_data, unique_string, sizeof(temp_data)) == 0);
}

void test_pcl_irq_long_first_write(void)
{
    uint32_t    fields = 0;
    uint32_t    parameter = 0;
    nsvc_pcl_t *chain;
    nsvc_pcl_chain_seek_t read_seeker;
    const unsigned READ1_LENGTH = 105;
    const unsigned READ2_LENGTH = 3;
    uint8_t temp_data[108];   // == READ1+2_LENGTH
    unsigned read_count;

    nufr_init();
    nsvc_init();
    nsvc_pcl_init();

    // hack to make us look like we're the right context
    nufr_running = NUFR_TID_TO_TCB(NUFR_TID_UART_TASK);
    nufr_running->block_flags &= BITWISE_NOT8(NUFR_TASK_NOT_LAUNCHED);

    uart_irq_packet_start();
    uart_irq_data_rx(unique_string, READ1_LENGTH);
    uart_irq_data_rx(&unique_string[READ1_LENGTH], READ2_LENGTH);
    uart_irq_packet_end();

    nufr_msg_getW(&fields, &parameter);
    chain = (nsvc_pcl_t *)parameter;
    UT_ENSURE(NULL != chain);

    // An offset of zero will automatically skip over chain head header
    nsvc_pcl_set_seek_to_packet_offset(chain, &read_seeker, 0);
    read_count = nsvc_pcl_read(&read_seeker, temp_data, sizeof(temp_data));
    UT_ENSURE(sizeof(temp_data) == read_count);
    UT_ENSURE(memcmp(temp_data, unique_string, sizeof(temp_data)) == 0);
}

void test_pcl_irq_long_second_write(void)
{
    uint32_t    fields = 0;
    uint32_t    parameter = 0;
    nsvc_pcl_t *chain;
    nsvc_pcl_chain_seek_t read_seeker;
    const unsigned READ1_LENGTH = 40;
    const unsigned READ2_LENGTH = 120;
    uint8_t temp_data[160];   // == READ1+2_LENGTH
    unsigned read_count;

    nufr_init();
    nsvc_init();
    nsvc_pcl_init();

    // hack to make us look like we're the right context
    nufr_running = NUFR_TID_TO_TCB(NUFR_TID_UART_TASK);
    nufr_running->block_flags &= BITWISE_NOT8(NUFR_TASK_NOT_LAUNCHED);

    uart_irq_packet_start();
    uart_irq_data_rx(unique_string, READ1_LENGTH);
    uart_irq_data_rx(&unique_string[READ1_LENGTH], READ2_LENGTH);
    uart_irq_data_rx(unique_string, READ2_LENGTH);
    uart_irq_packet_end();

    nufr_msg_getW(&fields, &parameter);
    chain = (nsvc_pcl_t *)parameter;
    UT_ENSURE(NULL != chain);

    // An offset of zero will automatically skip over chain head header
    nsvc_pcl_set_seek_to_packet_offset(chain, &read_seeker, 0);
    read_count = nsvc_pcl_read(&read_seeker, temp_data, sizeof(temp_data));
    UT_ENSURE(sizeof(temp_data) == read_count);
    UT_ENSURE(memcmp(temp_data, unique_string, sizeof(temp_data)) == 0);
}

void ut_examples_pcl_irq_handler(void)
{
    test_fifo();

    create_unique_string();

    test_pcl_irq_simple_write();
    test_pcl_irq_long_first_write();
    test_pcl_irq_long_second_write();
}