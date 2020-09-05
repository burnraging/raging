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

//! @file     raging-utils-mem.c
//! @authors  Bernie Woodland
//! @date     17Apr19
//! @brief    Utilities common to all projects: memcpy, memset equivalents


#include "raging-global.h"
#include "raging-utils-mem.h"

//!
//! @name      rutils_memset
//!
//! @brief     Substitute for library 'memset' routine
//!
//! @param[out] 'dest_str'
//! @param[in]  'set_value'
//! @param[in]  'length'
//!
void rutils_memset(void *dest_str, uint8_t set_value, unsigned length)
{
    unsigned  i;
    uint32_t *dest_str32;
    uint8_t  *dest_str8;

    // Speed optimization: can use uint32's if word aligned
    if (IS_ALIGNED32(dest_str) && (length >= 16))
    {
        uint32_t set_value32 = set_value;
        unsigned span_count;

        if (0 != set_value32)
        {
            set_value32 |= (set_value32 << 8);
            set_value32 |= (set_value32 << 16);
        }

        // Speed optimization: cut down on loop overhead by doing in
        // sets of 16 bytes
        dest_str32 = (uint32_t *)dest_str;
        span_count = length >> 4;
        for (i = 0; i < span_count; i++)
        {
            *(dest_str32)++ = set_value32;
            *(dest_str32)++ = set_value32;
            *(dest_str32)++ = set_value32;
            *(dest_str32)++ = set_value32;
        }

        i = span_count << 4;
        dest_str8 = (uint8_t *)dest_str32;
    }
    else
    {
        i = 0;
        dest_str8 = (uint8_t *)dest_str;
    }

//   (skipping this optimization)
//
//    span_count = length >> 2;
//    length -= span_count << 2;
//
//    for ( ; i < span_count; i++)
//    {
//        *((uint8_t *)dest_str)++ = set_value;
//        *((uint8_t *)dest_str)++ = set_value;
//        *((uint8_t *)dest_str)++ = set_value;
//        *((uint8_t *)dest_str)++ = set_value;
//    }

    // Finish leftover/misaligned
    for ( ; i < length; i++)
    {
        *(dest_str8)++ = set_value;
    }
}

//!
//! @name      rutils_memcpy
//!
//! @brief     Substitute for library 'memcpy' routine
//!
//! @param[out] 'dest_str'
//! @param[in]  'src_str'
//! @param[in]  'length'
//!
void rutils_memcpy(void *dest_str, const void *src_str, unsigned length)
{
    unsigned  i;
    uint32_t *dest_str32;
    uint32_t *src_str32;
    uint8_t  *dest_str8;
    uint8_t  *src_str8;

    // Speed optimization: can use uint32's if word aligned
    if (IS_ALIGNED32(dest_str) && IS_ALIGNED32(src_str) && (length >= 16))
    {
        unsigned span_count = length >> 4;
        // Speed optimization: cut down on loop overhead by doing in
        // sets of 16 bytes
        dest_str32 = (uint32_t *)dest_str;
        src_str32 = (uint32_t *)src_str;
        for (i = 0; i < span_count; i++)
        {
            *(dest_str32)++ = *(src_str32)++;
            *(dest_str32)++ = *(src_str32)++;
            *(dest_str32)++ = *(src_str32)++;
            *(dest_str32)++ = *(src_str32)++;
        }

        i = span_count << 4;
        dest_str8 = (uint8_t *)dest_str32;
        src_str8 = (uint8_t *)src_str32;
    }
    else
    {
        i = 0;
        dest_str8 = (uint8_t *)dest_str;
        src_str8 = (uint8_t *)src_str;
    }

//   (skipping this optimization)
//
//    span_count = length >> 2;
//    length -= span_count << 2;
//
//    for ( ; i < span_count; i++)
//    {
//        *((uint8_t *)dest_str)++ = *((uint8_t *)src_str)++;
//        *((uint8_t *)dest_str)++ = *((uint8_t *)src_str)++;
//        *((uint8_t *)dest_str)++ = *((uint8_t *)src_str))++;
//        *((uint8_t *)dest_str)++ = *((uint8_t *)src_str++;
//    }

    // Finish leftover/misaligned
    for ( ; i < length; i++)
    {
        *(dest_str8)++ = *(src_str8)++;
    }
}