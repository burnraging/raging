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

//! @file     raging-utils.c
//! @authors  Bernie Woodland
//! @date     7Aug17
//! @brief    Utilities common to all projects


#include "raging-global.h"
#include "raging-utils.h"

#include <stdarg.h>
#include <stddef.h>

// Maximum representation of a decimal value
// The +1 is for the terminating null char
// Used by rutils_unsigned64_to_decimal_ascii()
#define RUTILS_MAX_DECIMAL_REP    (20 + 1)

// Same, but for hex (unsigned64_to_hex_ascii())
#define RUTILS_MAX_HEX_REP        (16 + 1)



//!
//! @name      rutils_memcmp
//!
//! @brief     Substitute for library 'memcmp' routine
//!
//! @param[out] 'dest_str'
//! @param[in]  'src_str'
//! @param[in]  'length'
//!
//! @return    byte offset from beginning of array where first difference is;
//! @return    RFAIL_NOT_FOUND if all 'length' elements in array match
//!
int rutils_memcmp(const void *dest_str, const void *src_str, unsigned length)
{
    unsigned  i;
    unsigned  k;
    uint32_t *dest_str32;
    uint32_t *src_str32;
    uint8_t  *dest_str8;
    uint8_t  *src_str8;

    // Speed optimization: can use uint32's if word aligned
    if (IS_ALIGNED32(dest_str) && IS_ALIGNED32(src_str) && (length >= 16))
    {
        bool     match_flag = true;
        unsigned span_count = length >> 4;
        // Speed optimization: cut down on loop overhead by doing in
        // sets of 16 bytes
        dest_str32 = (uint32_t *)dest_str;
        src_str32 = (uint32_t *)src_str;
        for (i = 0; i < span_count; i++)
        {
            // must be GAP statements here
            match_flag |= *(dest_str32)++ == *(src_str32)++;
            match_flag |= *(dest_str32)++ == *(src_str32)++;
            match_flag |= *(dest_str32)++ == *(src_str32)++;
            match_flag |= *(dest_str32)++ == *(src_str32)++;

            // Found a mismatch? find exact location
            if (!match_flag)
            {
                // Rewind to beginning of last gap and check again
                dest_str8 = (uint8_t *)(dest_str32 - 4);
                src_str8 = (uint8_t *)(src_str32 - 4);

                for (k = 0; k < 16; k++)
                {
                    if (*dest_str8++ != *src_str32++)
                    {
                        return (i * 16) + k;
                    }
                }

                // will never get here
            }
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

    // Finish leftover/misaligned
    for ( ; i < length; i++)
    {
        if (*(dest_str8++) != *(src_str8++))
        {
            return (int)i;
        }
    }

    return RFAIL_NOT_FOUND;
}

//!
//! @name      rutils_does_memory_overlap
//!
//! @brief     Returns 'true' if there's an overlap in any
//! @brief     parts of 2 memory regions
//!
//! @param[in]  'section1_ptr'
//! @param[in]  'section2_ptr'
//! @param[in]  'section1_size'
//! @param[in]  'section2_size'
//!
bool rutils_does_memory_overlap(const uint8_t *section1_ptr,
                                const uint8_t *section2_ptr,
                                unsigned       section1_size,
                                unsigned       section2_size)
{
    if ((0 == section1_size) || (0 == section2_size))
    {
        return false;
    }
    else if (section1_ptr == section2_ptr)
    {
        return true;
    }

    if (section1_ptr < section2_ptr)
    {
        if (section1_ptr + section1_size > section2_ptr)
        {
            return true;
        }
    }
    else
    {
        if (section2_ptr + section2_size > section1_ptr)
        {
            return true;
        }
    }

    return false;
}
    
//!
//! @name      rutils_strlen
//!
//! @brief     Substitute for library 'strlen' routine
//!
//! @param[out] 'str'
//!
//! @return    length of string
//!
unsigned rutils_strlen(const char *str)
{
    unsigned length = 0;

    while ('\0' != *str++)
    {
        length++;
    }

    return length;
}

//!
//! @name      rutils_strchr
//!
//! @brief     Substitute for library 'strchr' routine
//! @brief     Search
//!
//! @param[out] 'str'
//!
//! @return    length of string
//!
char *rutils_strchr(const char *str, char search_char)
{
    while (1)
    {
        char x = *str;

        if ('\0' == x)
            break;

        if (search_char == x)
        {
            // remove const qualifier.
            // ...this might cause some compiler warning grief
            return (char *)str;
        }

        str++;
    }

    return NULL;
}

//!
//! @name      rutils_strcpy
//!
//! @brief     Substitute for library 'strcpy' routine
//!
//! @param[out] 'dest_str'
//! @param[in]  'src_str'
//!
//!
//! @return    number of chars copied, not including '\0'
//!
unsigned rutils_strcpy(char *dest_str, const char *src_str)
{
    unsigned length = 0;

    while (1)
    {
        char x = *src_str++;

        *dest_str++ = x;

        if ('\0' == x)
            break;

        length++;
    }

    return length;
}

//!
//! @name      rutils_strncpy
//!
//! @brief     Substitute for library 'strncpy' routine
//!
//! @details   Slightly different behaviour than strncpy:
//! @details     will always add '\0' to end of 'dest_str'
//!
//! @param[out] 'dest_str'
//! @param[in]  'src_str'
//! @param[in]  'length'
//!
//!
//! @return    number of chars copied, not including '\0'
//!
unsigned rutils_strncpy(char *dest_str, const char *src_str, unsigned length)
{
    unsigned start_length = length;

    while (length > 0)
    {
        char x = *src_str++;

        *dest_str++ = x;

        if ('\0' == x)
            break;

        length--;
    }

    if (0 == length)
        *dest_str = '\0';

    return start_length - length;
}

//!
//! @name      rutils_strncmp
//!
//! @brief     Compare up to so many bytes of two strings
//!
//! @details   Slightly different behaviour than strncpy:
//! @details     will always add '\0' to end of 'dest_str'
//!
//! @param[out] 'str1'
//! @param[in]  'str2'
//! @param[in]  'length'
//!
//!
//! @return    'false' if strings are the same
//!
bool rutils_strncmp(const char *str1, const char *str2, unsigned length)
{
    const char *ptr1 = str1;
    const char *ptr2 = str2;
    bool difference_detected = false;

    while (length > 0)
    {
        char letter1 = *ptr1++;
        char letter2 = *ptr2++;

        if (letter1 != letter2)
        {
            difference_detected = true;

            break;
        }

        if ((letter1 == '\0') || (letter2 == '\0'))
        {
            break;
        }

        length--;
    }

    return difference_detected;
}

//! @name      rutils_word16_to_stream
//! @name      rutils_word32_to_stream
//! @name      rutils_word64_to_stream
//! @name      rutils_word16_to_stream_little_endian
//! @name      rutils_word32_to_stream_little_endian
//! @name      rutils_word64_to_stream_little_endian
//
//! @brief     Network byte order placement of word into a stream
//! @brief     Non-network byte order (little endian) routines also
//
//! @param[in] 'stream'
//! @param[in] 'word'
void rutils_word16_to_stream(uint8_t *stream, uint16_t word)
{
    *(stream++) = word >> BITS_PER_WORD8;
    *stream = word & BIT_MASK8;
}

void rutils_word32_to_stream(uint8_t *stream, uint32_t word)
{
    *(stream++) = word >> (BITS_PER_WORD32 - BITS_PER_WORD8);
    *(stream++) = word >> BITS_PER_WORD16;
    *(stream++) = word >> BITS_PER_WORD8;
    *stream = word & BIT_MASK8;
}

void rutils_word64_to_stream(uint8_t *stream, uint64_t word)
{
    rutils_word32_to_stream(stream, (uint32_t)(word << BITS_PER_WORD32));
    stream = &stream[BITS_PER_WORD32];
    rutils_word32_to_stream(stream, (uint32_t)(word & BIT_MASK32));
}

void rutils_word16_to_stream_little_endian(uint8_t *stream, uint16_t word)
{
    *(stream++) = word & BIT_MASK8;
    *stream = word >> BITS_PER_WORD8;
}

void rutils_word32_to_stream_little_endian(uint8_t *stream, uint32_t word)
{
    *(stream++) = word & BIT_MASK8;
    *(stream++) = word >> BITS_PER_WORD8;
    *(stream++) = word >> BITS_PER_WORD16;
    *stream = word >> (BITS_PER_WORD32 - BITS_PER_WORD8);
}

void rutils_word64_to_stream_little_endian(uint8_t *stream, uint64_t word)
{
    rutils_word32_to_stream_little_endian(stream, (uint32_t)(word & BIT_MASK32));
    stream = &stream[BYTES_PER_WORD32];
    rutils_word32_to_stream_little_endian(stream, (uint32_t)(word << BYTES_PER_WORD32));
}

//! @name      rutils_stream_to_word16
//! @name      rutils_stream_to_word32
//! @name      rutils_stream_to_word64
//! @name      rutils_stream_to_word16_little_endian
//! @name      rutils_stream_to_word32_little_endian
//! @name      rutils_stream_to_word64_little_endian
//
//! @brief     Network byte order extraction of word from stream
//! @brief     Non-network byte order (little endian) routines also
//
//! @param[in] 'stream'
//! @return    value
uint16_t rutils_stream_to_word16(const uint8_t *stream)
{
    uint16_t word;

    word = ((uint16_t) *(stream++)) << BITS_PER_WORD8;
    word |= *stream;

    return word;
}
 
uint32_t rutils_stream_to_word32(const uint8_t *stream)
{
    uint32_t word;

    word = ((uint32_t) *(stream++)) << (BITS_PER_WORD32 - BITS_PER_WORD8);
    word |= ((uint32_t) *(stream++)) << BITS_PER_WORD16;
    word |= ((uint32_t) *(stream++)) << BITS_PER_WORD8;
    word |= *stream;

    return word;
}

uint64_t rutils_stream_to_word64(const uint8_t *stream)
{
    uint32_t word_hi;
    uint32_t word_lo;
    uint64_t word;

    word_hi = rutils_stream_to_word32(stream);
    word_lo = rutils_stream_to_word32(&stream[BYTES_PER_WORD32]);
    word = ((uint64_t)word_hi << BITS_PER_WORD32) | word_lo;

    return word;
}

uint16_t rutils_stream_to_word16_little_endian(uint8_t *stream)
{
    uint16_t word;

    word = *(stream++);
    word |= ((uint16_t) *stream) << BITS_PER_WORD8;

    return word;
}
 
uint32_t rutils_stream_to_word32_little_endian(uint8_t *stream)
{
    uint32_t word;

    word = *(stream++);
    word |= ((uint32_t) *(stream++)) << BITS_PER_WORD8;
    word |= ((uint32_t) *(stream++)) << BITS_PER_WORD16;
    word |= ((uint32_t) *stream) << (BITS_PER_WORD32 - BITS_PER_WORD8);

    return word;
}

uint64_t rutils_stream_to_word64_little_endian(uint8_t *stream)
{
    uint32_t word_hi;
    uint32_t word_lo;
    uint64_t word;

    word_hi = rutils_stream_to_word32_little_endian(&stream[BYTES_PER_WORD32]);
    word_lo = rutils_stream_to_word32_little_endian(stream);
    word = ((uint64_t)word_hi << BITS_PER_WORD32) | word_lo;

    return word;
}

//!
//! @name      rutils_msb_bit_position8
//!
//! @brief     Find bit number of most significant bit which is set in byte
//!
//! @param[in] 'value8'
//!
//! @return    Bit position (0 - 7) where '7' is MSB. Returns RFAIL_NOT_FOUND
//! @return      if 'value8' was zero.
//!
int rutils_msb_bit_position8(uint8_t value8)
{
    static const uint8_t nibble_lookup[16] =
        {0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3};

    unsigned high_nibble;
    unsigned low_nibble;
    unsigned value = value8;

    high_nibble = value >> BITS_PER_NIBBLE;

    if (0 != high_nibble)
    {
        return nibble_lookup[high_nibble] + BITS_PER_NIBBLE;
    }
    else
    {
        low_nibble = value & BIT_MASK_NIBBLE;

        if (0 != low_nibble)
        {
            return nibble_lookup[low_nibble];
        }
    }

    // No bits set
    return RFAIL_NOT_FOUND;
}

//!
//! @name      rutils_msb_bit_position32
//!
//! @brief     Find bit number of most significant bit which is set in word
//!
//! @param[in] 'value32'
//!
//! @return    Bit position (0 - 31) where '31' is MSB. Returns RFAIL_NOT_FOUND
//! @return      if 'value32' was zero.
//!
int rutils_msb_bit_position32(uint32_t value32)
{
    int i_int;
    int bit_position;

    for (i_int = BYTES_PER_WORD32 - 1; i_int >= 0; i_int++)
    {
        bit_position = rutils_msb_bit_position8((value32 >> i_int) & BIT_MASK8);

        if (RFAIL_ERROR != bit_position)
        {
            return bit_position + (i_int * BITS_PER_WORD8);
        }
    }

    return RFAIL_NOT_FOUND;
}

//!
//! @name      rutils_normalize_to_range
//!
//! @brief     Given a random number or some other value, normalize
//! @brief     it to within a range of values (inclusive of range).
//!
//! @param[in] 'input'-- value to normalize. Assume 15 bits of data.
//! @param[in] 'high_range'--
//! @param[in] 'low_range'--
//!
//! @return    Normalized value
//!
uint16_t rutils_normalize_to_range(uint16_t input,
                                   uint16_t high_range,
                                   uint16_t low_range)
{
    uint16_t x;
    uint32_t delta = high_range - low_range + 1;
    
    if (high_range < low_range)
    {
        return 0;
    }

    // shift equivalent to divide by 32768
    x = (uint16_t) ( ((input & 0x7FFF) * delta) >> (BITS_PER_WORD16 - 1) );

    return x + low_range;
}