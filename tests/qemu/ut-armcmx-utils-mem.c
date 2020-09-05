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

//! @file     ut-armcmx-utils-mem.c
//! @authors  Bernie Woodland
//! @date     22Apr19
//!
//! @brief   Unit Test for fcns. in armcmx-utils-mem.c
//!
//! @details Tests for ARM assembler utils, specifically
//! @details rutils_memset(), rutils_memcpy().
//! @details Can run under any native ARM platform...qemu...disco...
//!

#include "raging-global.h"
#include "raging-utils-mem.h"
#include "raging-utils.h"


#define  MEM_BUF_SIZE_BYTES          512

// Use a 'uint32_t' so you know where the alignment is
uint32_t ut_mem_buf[MEM_BUF_SIZE_BYTES/BYTES_PER_WORD32];
uint32_t ut_mem_pattern[MEM_BUF_SIZE_BYTES/BYTES_PER_WORD32];


__attribute__((naked)) uint32_t ut_get_sp(void)
{
    __asm volatile(
        "  mov   r0, sp\n\t"       //
        "  bx    lr"
        : );
}


// A memset which is guaranteed to work.
void ut_safe_memset(void *dest_str, uint8_t set_value, unsigned length)
{
    unsigned i;
    uint8_t  *dest_str8 = (uint8_t *)dest_str;

    for (i = 0; i < length; i++)
    {
        *dest_str8++ = set_value;
    }
}

// Digs back 3 bytes
// Algorithm does the following:
// 1) Assumes that there's +/- 'check_padding_size' bytes of padding,
//    minimum before/after verification buffer
// 2) Makes sure padding == 0
// 3) Makes sure all values are as-expected by memset
//
bool ut_verify_prior_memset(void     *dest_str,
                            uint8_t   set_value,
                            unsigned  length,
                            unsigned  check_padding_size)
{
    unsigned i;
    uint8_t  *dest_str8 = (uint8_t *)dest_str;

    // Check that memset occured as it should've
    for (i = 0; i < length; i++)
    {
        if (*dest_str8++ != set_value)
            return false;
    }

    // Verify that padding on top is zero
    // Start at 'check_padding_size' bytes above
    // first byte copied
    dest_str8 = (uint8_t *)dest_str;
    dest_str8 -= check_padding_size;

    for (i = 0; i < check_padding_size; i++)
    {
        if (*dest_str8++ != 0)
            return false;
    }

    // Verify that padding on bottom is zero
    // Start at 1 byte past last copied byte
    dest_str8 = (uint8_t *)dest_str;
    dest_str8 = &dest_str8[length - 1];
    dest_str8++;

    for (i = 0; i < check_padding_size; i++)
    {
        if (*dest_str8++ != 0)
            return false;
    }

    return true;
}

// Similar to above
bool ut_verify_prior_memcpy(void     *dest_str,
                            void     *src_str,
                            unsigned  length,
                            unsigned  check_padding_size)
{
    unsigned i;
    uint8_t  *dest_str8 = (uint8_t *)dest_str;
    int      cmp_result;

    cmp_result = rutils_memcmp(dest_str, src_str, length);
    if (RFAIL_NOT_FOUND != cmp_result)
        return false;

    // Verify that padding on top is zero
    // Start at 'check_padding_size' bytes above
    // first byte copied
    dest_str8 = (uint8_t *)dest_str;
    dest_str8 -= check_padding_size;

    for (i = 0; i < check_padding_size; i++)
    {
        if (*dest_str8++ != 0)
            return false;
    }

    // Verify that padding on bottom is zero
    // Start at 1 byte past last copied byte
    dest_str8 = (uint8_t *)dest_str;
    dest_str8 = &dest_str8[length - 1];
    dest_str8++;

    for (i = 0; i < check_padding_size; i++)
    {
        if (*dest_str8++ != 0)
            return false;
    }

    return true;
}

bool ut_armcmx_tests(void)
{
    uint8_t  *mem_buf8 = (uint8_t *)ut_mem_buf;
    uint8_t  *mem_pattern8 = (uint8_t *)ut_mem_pattern;
    bool     sc = true;
    uint32_t saved_sp;
    unsigned offset;
    unsigned k;

    // multi bytes, odd offset
    for (k = 1; k < 300; k++)
    {
        offset = 5;
        ut_safe_memset(ut_mem_buf, 0, sizeof(ut_mem_buf));
        saved_sp = ut_get_sp();
        rutils_memset(&mem_buf8[offset], 0xA5, k);
        sc &= ut_verify_prior_memset(&mem_buf8[offset], 0xA5, k, offset - 1);
        sc &= saved_sp == ut_get_sp();

        if (!sc)
            break;
    }

    // Same thing, different offset
    for (k = 1; k < 300; k++)
    {
        offset = 6;
        ut_safe_memset(ut_mem_buf, 0, sizeof(ut_mem_buf));
        saved_sp = ut_get_sp();
        rutils_memset(&mem_buf8[offset], 0xA5, k);
        sc &= ut_verify_prior_memset(&mem_buf8[offset], 0xA5, k, offset - 1);
        sc &= saved_sp == ut_get_sp();

        if (!sc)
            break;
    }

    // Same thing, different offset
    for (k = 1; k < 300; k++)
    {
        offset = 7;
        ut_safe_memset(ut_mem_buf, 0, sizeof(ut_mem_buf));
        saved_sp = ut_get_sp();
        rutils_memset(&mem_buf8[offset], 0xA5, k);
        sc &= ut_verify_prior_memset(&mem_buf8[offset], 0xA5, k, offset - 1);
        sc &= saved_sp == ut_get_sp();

        if (!sc)
            break;
    }


    //***** rutils_memcpy

    for (k = 0; k < sizeof(ut_mem_pattern); k++)
    {
        mem_pattern8[k] = k;
    }

    // multi bytes, odd offset
    for (k = 1; k < 300; k++)
    {
        offset = 5;
        ut_safe_memset(ut_mem_buf, 0, sizeof(ut_mem_buf));
        saved_sp = ut_get_sp();
        rutils_memcpy(&mem_buf8[offset], ut_mem_pattern, k);
        sc &= ut_verify_prior_memcpy(&mem_buf8[offset], ut_mem_pattern, k, offset - 1);
        sc &= saved_sp == ut_get_sp();

        if (!sc)
            break;
    }

    // same thing, different offset
    for (k = 1; k < 300; k++)
    {
        offset = 6;
        ut_safe_memset(ut_mem_buf, 0, sizeof(ut_mem_buf));
        saved_sp = ut_get_sp();
        rutils_memcpy(&mem_buf8[offset], ut_mem_pattern, k);
        sc &= ut_verify_prior_memcpy(&mem_buf8[offset], ut_mem_pattern, k, offset - 1);
        sc &= saved_sp == ut_get_sp();

        if (!sc)
            break;
    }

    // Do copy from/to aligned addresses, so we don't just exercise corner cases!!
    for (k = 1; k < 300; k++)
    {
        offset = 8;
        ut_safe_memset(ut_mem_buf, 0, sizeof(ut_mem_buf));
        saved_sp = ut_get_sp();
        rutils_memcpy(&mem_buf8[offset], ut_mem_pattern, k);
        sc &= ut_verify_prior_memcpy(&mem_buf8[offset], ut_mem_pattern, k, offset - 1);
        sc &= saved_sp == ut_get_sp();

        if (!sc)
            break;
    }

    return sc;
}