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

//!
//! @file      raging-utils.h
//! @author    Bernie Woodland
//! @date      27Nov16
//! @brief     Global utilities
//!

#ifndef _RAGING_UTILS_H_
#define _RAGING_UTILS_H_

#include "raging-global.h"


//!
//! @name      STRNCPY_S
//!
//! @brief     compiler-independent std library string calls
//!
#if (__STDC_VERSION__ >= 201112L) || defined(VISUAL_C_COMPILER)
    #define STRNCPY_S(dest, destsz, src, count) strncpy_s(dest, destsz, src, count)
#else
    #define STRNCPY_S(dest, destsz, src, count) strncpy(dest, src, destsz)
#endif

RAGING_EXTERN_C_START
int rutils_memcmp(const void *dest_str, const void *src_str, unsigned length);
bool rutils_does_memory_overlap(const uint8_t *section1_ptr,
                                const uint8_t *section2_ptr,
                                unsigned       section1_size,
                                unsigned       section2_size);
unsigned rutils_strlen(const char *str);
char *rutils_strchr(const char *str, char search_char);
unsigned rutils_strcpy(char *dest_str, const char *src_str);
unsigned rutils_strncpy(char *dest_str, const char *src_str, unsigned length);
bool rutils_strncmp(const char *str1, const char *str2, unsigned length);

void rutils_word16_to_stream(uint8_t *stream, uint16_t word);
void rutils_word32_to_stream(uint8_t *stream, uint32_t word);
void rutils_word64_to_stream(uint8_t *stream, uint64_t word);

void rutils_word16_to_stream_little_endian(uint8_t *stream, uint16_t word);
void rutils_word32_to_stream_little_endian(uint8_t *stream, uint32_t word);
void rutils_word64_to_stream_little_endian(uint8_t *stream, uint64_t word);

uint16_t rutils_stream_to_word16(const uint8_t *stream);
uint32_t rutils_stream_to_word32(const uint8_t *stream);
uint64_t rutils_stream_to_word64(const uint8_t *stream);

uint16_t rutils_stream_to_word16_little_endian(uint8_t *stream);
uint32_t rutils_stream_to_word32_little_endian(uint8_t *stream);
uint64_t rutils_stream_to_word64_little_endian(uint8_t *stream);

int rutils_msb_bit_position8(uint8_t value8);
int rutils_msb_bit_position32(uint32_t value32);

uint16_t rutils_normalize_to_range(uint16_t input,
                                   uint16_t high_range,
                                   uint16_t low_range);
RAGING_EXTERN_C_END

#endif