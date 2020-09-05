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
// By Chris Martin
#include <test_helper.h>
#include <raging-utils.h>

#define TestStreamSize16  2
#define TestStreamSize32  4
#define TestStreamSize64 8

static const uint16_t TestWord16 = 0xA500;
static const uint32_t TestWord32 = 0xA500A500;
static const uint64_t TestWord64 = 0xA500A500A500A500;

void test_rutils_word16_to_stream(void)
{
    uint8_t stream[TestStreamSize16] = {0};    
    
    rutils_word16_to_stream(stream,TestWord16);
    
    TEST_ENSURE(0xA5 == stream[0]);
    TEST_ENSURE(0x00 == stream[1]);
}

void test_rutils_word32_to_stream(void)
{
    uint8_t stream[TestStreamSize32] = {0};
    
    rutils_word32_to_stream(stream, TestWord32);
    
    TEST_ENSURE(0xA5 == stream[0]);
    TEST_ENSURE(0x00 == stream[1]);
    TEST_ENSURE(0xA5 == stream[2]);
    TEST_ENSURE(0x00 == stream[3]);    
}

void test_rutils_word64_to_stream(void)
{
    uint8_t stream[TestStreamSize64] = {0};
    
    rutils_word64_to_stream(stream,TestWord64);
    //  TODO: Re-enable this test
    /*
    TEST_ENSURE(0xA5 == stream[0]);
    TEST_ENSURE(0x00 == stream[1]);
    TEST_ENSURE(0xA5 == stream[2]);
    TEST_ENSURE(0x00 == stream[3]);
    TEST_ENSURE(0xA5 == stream[4]);
    TEST_ENSURE(0x00 == stream[5]);
    TEST_ENSURE(0xA5 == stream[6]);
    TEST_ENSURE(0x00 == stream[7]);
    */
}

void test_rutils_word16_to_stream_little_endian(void)
{
    uint8_t stream[TestStreamSize16] = {0};
    
    rutils_word16_to_stream_little_endian(stream, TestWord16);
    //  TODO: Re-enable this test
    /*
    TEST_ENSURE(0xA5 == stream[0]);
    TEST_ENSURE(0xA5 == stream[1]);
    */
}

void test_rutils_word32_to_stream_little_endian(void)
{
    uint8_t stream[TestStreamSize32] = {0};
    
    rutils_word32_to_stream_little_endian(stream, TestWord32);
    
    //  TODO: Re-enable this test
    //TEST_ENSURE(0xA5 == stream[0]);
}

void test_rutils_word64_to_stream_little_endian(void)
{
    uint8_t stream[TestStreamSize64] = {0};
    
    rutils_word64_to_stream_little_endian(stream,TestWord64);
}

void test_rutils_stream_to_word16(void)
{
    uint8_t stream[TestStreamSize16] = {0};
    
    rutils_word16_to_stream(stream,TestWord16);
    
    uint16_t word = rutils_stream_to_word16(stream);
}

void test_rutils_stream_to_word32(void)
{
    uint8_t stream[TestStreamSize32] = {0};
    
    rutils_word32_to_stream(stream,TestWord32);
    
    uint32_t word = rutils_stream_to_word32(stream);
}

void test_rutils_stream_to_word64(void)
{
    uint8_t stream[TestStreamSize64] = {0};
    
    rutils_word64_to_stream(stream,TestWord64);
    
    uint64_t word = rutils_stream_to_word64(stream);
}

void test_rutils_stream_to_word16_little_endian(void)
{
    uint8_t stream[TestStreamSize16] = {0};
    
    rutils_word16_to_stream_little_endian(stream,TestWord16);
    
    uint16_t word = rutils_stream_to_word16_little_endian(stream);
}

void test_rutils_stream_to_word32_little_endian(void)
{
    uint8_t stream[TestStreamSize32] = {0};
    
    rutils_word32_to_stream_little_endian(stream,TestWord32);
    
    uint32_t word = rutils_stream_to_word32_little_endian(stream);
}

void test_rutils_stream_to_word64_little_endian(void)
{
    uint8_t stream[TestStreamSize64] = {0};
    
    rutils_word64_to_stream_little_endian(stream,TestWord64);
    
    uint64_t word = rutils_stream_to_word64_little_endian(stream);
}

void test_rutils_unsigned64_to_decimal_ascii(void)
{
    uint8_t stream[TestStreamSize64] = {0};
    rutils_unsigned64_to_decimal_ascii(stream,TestStreamSize64,1,false);
}

void test_rutils_unsigned64_to_hex_ascii(void)
{
    uint8_t stream[TestStreamSize64] = {0};
    rutils_unsigned64_to_hex_ascii(stream,TestStreamSize64,1,false,false);
}

void test_rutils_is_decimal_digit(void)
{
    rutils_is_decimal_digit(0);
}

void test_rutils_is_hex_digit(void)
{
    rutils_is_hex_digit(0xFF);
}

void test_rutils_decimal_digit_to_value(void)
{
    rutils_decimal_digit_to_value(9);
}

void test_rutils_hex_digit_to_value(void)
{
    rutils_hex_digit_to_value(0xFF);
}

void test_rutils_count_of_decimal_ascii_span(void)
{
    uint8_t stream[TestStreamSize64] = {0};
    rutils_count_of_decimal_ascii_span(stream);
}

void test_rutils_count_of_hex_ascii_span(void)
{
    uint8_t stream[TestStreamSize64] = {0};
    rutils_count_of_hex_ascii_span(stream);
}

void test_rutils_decimal_ascii_unsigned64(void)
{
    uint8_t stream[TestStreamSize64] = {0};
    uint64_t value = 0;
    bool failure = false;
    rutils_decimal_ascii_to_unsigned64(stream,&value,&failure);
}

void test_raging_utils(void)
{
    test_rutils_word16_to_stream();
    test_rutils_word32_to_stream();
    test_rutils_word64_to_stream();
    
    test_rutils_stream_to_word16();
    test_rutils_stream_to_word32();
    test_rutils_stream_to_word64();
    
    test_rutils_word16_to_stream_little_endian();
    test_rutils_word32_to_stream_little_endian();
    test_rutils_word64_to_stream_little_endian();
    
    test_rutils_stream_to_word16_little_endian();
    test_rutils_stream_to_word32_little_endian();
    test_rutils_stream_to_word64_little_endian();
    
    test_rutils_unsigned64_to_decimal_ascii();
    test_rutils_unsigned64_to_hex_ascii();
    
    test_rutils_is_decimal_digit();
    test_rutils_is_hex_digit();
    
    test_rutils_decimal_digit_to_value();
    test_rutils_hex_digit_to_value();
    
    test_rutils_count_of_decimal_ascii_span();
    test_rutils_count_of_hex_ascii_span();
    test_rutils_decimal_ascii_unsigned64();    
}



