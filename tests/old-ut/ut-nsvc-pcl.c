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

//! @file    ut-nsvc-pcl.c
//! @authors Bernie Woodland
//! @date    24Nov17

// Tests SL Particles functionality (nsvc-pcl.c)

#include "nufr-global.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nsvc.h"
#include "nufr-api.h"
#include "nsvc-app.h"
#include "nsvc-api.h"

#include "raging-contract.h"

#include <string.h>

#define SUCCESS_ALLOC(rv)     ( (NUFR_SEMA_GET_OK_NO_BLOCK == (rv)) ||       \
                                (NUFR_SEMA_GET_OK_BLOCK == (rv)) )


static void write_predictable_pattern(uint8_t *ptr, unsigned size)
{
    unsigned i;

    for (i = 0; i < size; i++)
    {
        if ((i % 2) == 0)
        {
            ptr[i] = (uint8_t)(i / 256);
        }
        else
        {
            ptr[i] = (uint8_t)(i / 2);
        }
    }

    if (i < size)
    {
        ptr[i] = 0xFF;
    }
}

void test_pcl_single_alloc_free(void)
{
    nufr_sema_get_rtn_t alloc_rv;
    nsvc_pcl_t         *head_pcl;

    nsvc_init();
    nsvc_pcl_init();

    alloc_rv = nsvc_pcl_alloc_chainWT(&head_pcl, NULL, 201,
                                      NSVC_PCL_NO_TIMEOUT);
    UT_REQUIRE(NUFR_SEMA_GET_OK_NO_BLOCK == alloc_rv);
    UT_REQUIRE(NULL != head_pcl);

    nsvc_pcl_free_chain(head_pcl);
}

// Write 100 bytes of data to a short chain, read it back
void test_pcl_write_short_string_to_preallocated(void)
{
    nufr_sema_get_rtn_t   alloc_rv;
    nsvc_pcl_t           *head_pcl;
    uint8_t               write_data[100];
    uint8_t               read_back_data[100];
    unsigned              bytes_written;
    unsigned              bytes_read;
    nsvc_pcl_chain_seek_t write_seek;
    nsvc_pcl_chain_seek_t read_seek;

    nsvc_init();
    nsvc_pcl_init();

    // Allocate an extra pcl
    alloc_rv = nsvc_pcl_alloc_chainWT(&head_pcl, NULL, sizeof(write_data),
                                      NSVC_PCL_NO_TIMEOUT);
    UT_REQUIRE(NUFR_SEMA_GET_OK_NO_BLOCK == alloc_rv);
    UT_REQUIRE(NULL != head_pcl);

    // put dummy, predictable pattern in
    write_predictable_pattern(write_data, sizeof(write_data));

    memset(read_back_data, 0, sizeof(read_back_data));

    // Set 'write_seek' to the beginning of the 1st pcl.
    // Copy to 'read_seek'
    write_seek.current_pcl = head_pcl;
    write_seek.offset_in_pcl = 0;
    memcpy(&read_seek, &write_seek, sizeof(read_seek));

    // Write will occupy only 1st pcl in the chain
    bytes_written = nsvc_pcl_write_data_continue(&write_seek,
                                    write_data, sizeof(write_data));
    bytes_read = nsvc_pcl_read(&read_seek, read_back_data,
                               sizeof(read_back_data));

    // Read back data, compare to what was written
    UT_ENSURE(memcmp(write_data, read_back_data, sizeof(write_data)) == 0);

    nsvc_pcl_free_chain(head_pcl);
}    

// Same as previous, but with a longer test string, one that spans
//   3 pcl's.
void test_pcl_write_string_to_preallocated(void)
{
    nufr_sema_get_rtn_t   alloc_rv;
    nsvc_pcl_t           *head_pcl;
    uint8_t               write_data[201];
    uint8_t               read_back_data[201];
    unsigned              bytes_written;
    unsigned              bytes_read;
    nsvc_pcl_chain_seek_t write_seek;
    nsvc_pcl_chain_seek_t read_seek;

    nsvc_init();
    nsvc_pcl_init();

    alloc_rv = nsvc_pcl_alloc_chainWT(&head_pcl, NULL, sizeof(write_data),
                                      NSVC_PCL_NO_TIMEOUT);
    UT_REQUIRE(NUFR_SEMA_GET_OK_NO_BLOCK == alloc_rv);
    UT_REQUIRE(NULL != head_pcl);

    write_predictable_pattern(write_data, sizeof(write_data));

    memset(read_back_data, 0, sizeof(read_back_data));

    write_seek.current_pcl = head_pcl;
    write_seek.offset_in_pcl = 0;
    memcpy(&read_seek, &write_seek, sizeof(read_seek));

    bytes_written = nsvc_pcl_write_data_continue(&write_seek,
                                    write_data, sizeof(write_data));
    bytes_read = nsvc_pcl_read(&read_seek, read_back_data,
                                        sizeof(read_back_data));

    UT_ENSURE(memcmp(write_data, read_back_data, sizeof(write_data)) == 0);

    nsvc_pcl_free_chain(head_pcl);
}    

// Write 400 bytes in 1 byte/400 write calls
// Verify by reading the whole thing, then verify by
//   reading 1 byte at a time.
void test_pcl_write_1byte_at_a_time(void)
{
    nufr_sema_get_rtn_t   alloc_rv;
    nsvc_pcl_t           *head_pcl;
    uint8_t               write_data[400];
    uint8_t               read_back_data[400];
    unsigned              bytes_written;
    unsigned              bytes_read;
    nsvc_pcl_chain_seek_t write_seek;
    nsvc_pcl_chain_seek_t read_seek;
    unsigned              i;
    unsigned              j;

    nsvc_init();
    nsvc_pcl_init();

    alloc_rv = nsvc_pcl_alloc_chainWT(&head_pcl, NULL, sizeof(write_data),
                                      NSVC_PCL_NO_TIMEOUT);
    UT_REQUIRE(NUFR_SEMA_GET_OK_NO_BLOCK == alloc_rv);
    UT_REQUIRE(NULL != head_pcl);

    write_predictable_pattern(write_data, sizeof(write_data));

    memset(read_back_data, 0, sizeof(read_back_data));

    write_seek.current_pcl = head_pcl;
    write_seek.offset_in_pcl = 0;
    memcpy(&read_seek, &write_seek, sizeof(read_seek));

    for (i = 0; i < sizeof(write_data); i++)
    {
        bytes_written = nsvc_pcl_write_data_continue(&write_seek,
                                    &write_data[i], 1);
        UT_ENSURE(1 == bytes_written);
        UT_ENSURE(write_seek.offset_in_pcl == ((i + 1) % NSVC_PCL_SIZE));
    }

    // Verify all bytes in 1 shot
    bytes_read = nsvc_pcl_read(&read_seek, read_back_data,
                                        sizeof(read_back_data));

    UT_ENSURE(memcmp(write_data, read_back_data, sizeof(write_data)) == 0);

    // Verify 1 byte at a time, slewing to offset of byte
    // Skip over header.
    j = 0;
    for (i = sizeof(nsvc_pcl_header_t); i < sizeof(write_data); i++)
    {
        nsvc_pcl_chain_seek_t random_read_seek;
        uint8_t               single_byte_data;

        memset(&random_read_seek, 0xFF, sizeof(random_read_seek));
        single_byte_data = 0xFF;

        nsvc_pcl_set_seek_to_packet_offset(head_pcl, &random_read_seek, j);

        bytes_read = nsvc_pcl_read(&random_read_seek, &single_byte_data, 1);
        UT_ENSURE(1 == bytes_read);
        UT_ENSURE(single_byte_data == write_data[i]);

        j++;
    }

    nsvc_pcl_free_chain(head_pcl);
}    

// Same as test_pcl_write_short_string_to_preallocated(),
//   but with dynamic chain allocation+lengthening
void test_pcl_write_short_string(void)
{
    nsvc_pcl_t           *head_pcl = NULL;
    uint8_t               write_data[100];
    uint8_t               read_back_data[100];
    nufr_sema_get_rtn_t   write_rv;
    unsigned              bytes_read;
    nsvc_pcl_chain_seek_t write_seek;
    nsvc_pcl_chain_seek_t read_seek;
    unsigned              i;
    const unsigned        HEADER_OFFSET = sizeof(nsvc_pcl_header_t);

    nsvc_init();
    nsvc_pcl_init();

    // put dummy, predictable pattern in
    write_predictable_pattern(write_data, sizeof(write_data));

    memset(read_back_data, 0, sizeof(read_back_data));

    write_seek.current_pcl = NULL;
    write_seek.offset_in_pcl = 0xFF;

    // Chain is created by call, therefore write will start at 'HEADER_OFFSET'
    write_rv = nsvc_pcl_write_dataWT(&head_pcl,
                                     &write_seek,
                                     write_data,
                                     sizeof(write_data),
                                     NSVC_PCL_NO_TIMEOUT);
    UT_ENSURE(SUCCESS_ALLOC(write_rv));

    // Adjust read to skip over header
    read_seek.current_pcl = head_pcl;
    read_seek.offset_in_pcl = HEADER_OFFSET;

    bytes_read = nsvc_pcl_read(&read_seek, read_back_data,
                               sizeof(read_back_data));

    // Read back data, compare to what was written
    for (i = 0; i < sizeof(write_data); i++)
    {
        UT_ENSURE(write_data[i] == read_back_data[i]);
    }
    //UT_ENSURE(memcmp(&write_data[sizeof(nsvc_pcl_header_t)],
    //              &read_back_data[sizeof(nsvc_pcl_header_t)],
    //              sizeof(write_data) - sizeof(nsvc_pcl_header_t)) == 0);

    nsvc_pcl_free_chain(head_pcl);
}    

void ut_nsvc_pcl(void)
{
    test_pcl_single_alloc_free();
    test_pcl_write_short_string_to_preallocated();
    test_pcl_write_string_to_preallocated();
    test_pcl_write_1byte_at_a_time();
    test_pcl_write_short_string();
}