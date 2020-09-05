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

//! @file     raging-utils-scan-print.c
//! @authors  Bernie Woodland
//! @date     1Jan18
//! @brief    Utilities common to all projects


#include "raging-global.h"
#include "raging-utils.h"
#include "raging-utils-mem.h"
#include "raging-utils-scan-print.h"

#include <stdarg.h>


// Maximum representation of a decimal value
// The +1 is for the terminating null char
// Used by rutils_unsigned64_to_decimal_ascii()
#define RUTILS_MAX_DECIMAL_REP    (20 + 1)

// Same, but for hex (unsigned64_to_hex_ascii())
#define RUTILS_MAX_HEX_REP        (16 + 1)


// Data size specifier, for'rutils_sprintf()'
typedef enum
{
    RUTILS_PST_NONE,              //
    RUTILS_PST_SHORT,             // h
    RUTILS_PST_LONG_LONG,         // ll
} rutils_printf_size_t;

// Data type specifier, for'rutils_sprintf()'
typedef enum
{
    RUTILS_PTT_INT,               // i
    RUTILS_PTT_UNSIGNED,          // u
    RUTILS_PTT_HEX_CAPS,          // X
    RUTILS_PTT_HEX_LOWER,         // x
    RUTILS_PTT_CHAR,              // c
    RUTILS_PTT_STRING             // s
} rutils_printf_type_t;

// Parsed conversion specifier, for'rutils_sprintf()'
typedef struct
{
    bool     percent_escape_only;
    bool     left_justify;
    bool     leading_zeros;
    bool     has_width;
    bool     has_precision;
    bool     has_type_modifier;
    bool     has_size_specifier;
    unsigned width;
    unsigned precision;
    rutils_printf_size_t size_specifier;
    rutils_printf_type_t type;
} rutils_printf_specifier_t;



//!
//! @name      rutils_unsigned64_to_decimal_ascii
//! @name      rutils_unsigned32_to_decimal_ascii
//! @name      rutils_signed64_to_decimal_ascii
//! @name      rutils_signed32_to_decimal_ascii
//!
//! @brief     Converts an unsigned value to its ASCII decimal equivalent
//!
//! @details   This is the lowest level fcn. call for ASCII decimal
//! @details   printing.
//! @details   
//!
//! @param[out] 'stream'-- converted ASCII equivalent
//! @param[in] 'max_stream'-- length of 'stream': will not exceed
//! @param[in]       If 'max_stream' is set to RUTILS_MAX_DECIMAL_REP,
//! @param[in]       then 'max_stream' will never be exceeded
//! @param[in]       (never return 0).
//! @param[in] 'value'-- 
//! @param[in] 'append_null'-- 'true' means append a '\0' char to end of string
//! @param[in]       Appending null will not exceed string length
//
//! @return    number of chars put on stream, not including terminating null
//! @return    If 'max_stream' is exceeded, returns 0
unsigned rutils_unsigned64_to_decimal_ascii(char    *stream,
                                            unsigned max_stream,
                                            uint64_t value,
                                            bool     append_null)
{
    unsigned i;
    unsigned count;
    char *stream_ptr;
    char *stream_fwd_ptr;

    if (max_stream == 0)
        return 0;

    if (value == 0)
    {
        *(stream++) = '0';
        count = 1;
    }
    else
    {
        stream_ptr = stream;

        // Build the chars, backwards stream.
        for (count = 0; value > 0; count++)
        {
            *(stream_ptr++) = (value % 10) + '0';
            value /= 10;

            // Check for 'max_stream' violation:
            //   more chars to convert, but no room to put them
            if ((count == max_stream) && (value != 0))
            {
                return 0;
            }
        }

        stream_fwd_ptr = stream;

        // reverse the stream
        for (i = 0; i < count/2; i++)
        {
            char x = *(--stream_ptr);
            *stream_ptr = *stream_fwd_ptr;
            *(stream_fwd_ptr++) = x;
        }
    }

    if (append_null)
    {
        // If we've hit the max size already, then there's no
        //   room for the null char
        if (count == max_stream)
        {
            return 0;
        }

        stream[count] = '\0';

        // note: count does not included null char
    }

    return count;
}

unsigned rutils_unsigned32_to_decimal_ascii(char    *stream,
                                            unsigned max_stream,
                                            uint32_t value,
                                            bool     append_null)
{
    unsigned i;
    unsigned count;
    char *stream_ptr;
    char *stream_fwd_ptr;

    if (max_stream == 0)
        return 0;

    if (value == 0)
    {
        *(stream++) = '0';
        count = 1;
    }
    else
    {
        stream_ptr = stream;

        // Build the chars, backwards stream.
        for (count = 0; value > 0; count++)
        {
            *(stream_ptr++) = (value % 10) + '0';
            value /= 10;

            // Check for 'max_stream' violation:
            //   more chars to convert, but no room to put them
            if ((count == max_stream) && (value != 0))
            {
                return 0;
            }
        }

        stream_fwd_ptr = stream;

        // reverse the stream
        for (i = 0; i < count/2; i++)
        {
            char x = *(--stream_ptr);
            *stream_ptr = *stream_fwd_ptr;
            *(stream_fwd_ptr++) = x;
        }
    }

    if (append_null)
    {
        // If we've hit the max size already, then there's no
        //   room for the null char
        if (count == max_stream)
        {
            return 0;
        }

        stream[count] = '\0';

        // note: count does not included null char
    }

    return count;
}

unsigned rutils_signed64_to_decimal_ascii(char    *stream,
                                          unsigned max_stream,
                                          int64_t  value,
                                          bool     append_null)
{
    uint64_t positive_value;

    if (0 == max_stream)
    {
        return 0;
    }

    if (value >= 0)
    {
        positive_value = (uint64_t)value;
    }
    else
    {
        // 2's complement to extract unsigned equivalent
        positive_value = BITWISE_NOT64(value) + 1;

        *stream++ = '-';
        max_stream--;
    }

    return rutils_unsigned64_to_decimal_ascii(stream, max_stream,
                                   positive_value, append_null);
}

unsigned rutils_signed32_to_decimal_ascii(char    *stream,
                                          unsigned max_stream,
                                          int32_t  value,
                                          bool     append_null)
{
    uint32_t positive_value;

    if (0 == max_stream)
    {
        return 0;
    }

    if (value >= 0)
    {
        positive_value = (uint32_t)value;
    }
    else
    {
        // 2's complement to extract unsigned equivalent
        positive_value = BITWISE_NOT64(value) + 1;

        *stream++ = '-';
        max_stream--;
    }

    return rutils_unsigned32_to_decimal_ascii(stream, max_stream,
                                   positive_value, append_null);
}

//!
//! @name      rutils_unsigned64_to_hex_ascii
//! @name      rutils_unsigned32_to_hex_ascii
//! @name      rutils_signed64_to_decimal_ascii
//! @name      rutils_signed32_to_decimal_ascii
//
//! @brief     Converts an unsigned value to its ASCII hex equivalent
//
//! @details   Hex equivalent of rutils_unsigned64_to_hex_ascii().
//! @details   
//
//! @param[out] 'stream'-- converted ASCII equivalent
//! @param[in] 'max_stream'-- length of 'stream': will not exceed
//! @param[in]       If 'max_stream' is set to RUTILS_MAX_DECIMAL_REP,
//! @param[in]       then 'max_stream' will never be exceeded
//! @param[in]       (never return 0).
//! @param[in] 'value'-- 
//! @param[in] 'append_null'-- 'true' means append a '\0' char to end of string
//! @param[in]  'zero_pad_length'-- total num. digits, remainder will be
//! @param[in]           zero-padded
//! @param[in]  'upper_case'-- ??
//
//! @return    number of chars put on stream, not including terminating null
//! @return    If 'max_stream' is exceeded, returns 0
unsigned rutils_unsigned64_to_hex_ascii(char    *stream,
                                        unsigned max_stream,
                                        uint64_t value,
                                        bool     append_null,
                                        unsigned zero_pad_length,
                                        bool     upper_case)
{
    unsigned bytes_consumed = 0;
    unsigned nibble;
    unsigned swap_length;
    unsigned i;
    char     character;
    char    *ptr = stream;

    if (0 == max_stream)
    {
        return 0;
    }

    if (0 == value)
    {
        *ptr++ = '0';
        max_stream--;
    }
    else
    {
        // Write hex equivalent in reverse order, no
        // zero padding
        while ((value > 0) && (max_stream > 0))
        {
            nibble = value & BIT_MASK_NIBBLE;
            value >>= BITS_PER_NIBBLE;

            if (nibble < 0xA)
            {
                character = '0' + nibble;
            }
            else if (upper_case)
            {
                character = 'A' + nibble - 0xA;
            }
            else
            {
                character = 'a' + nibble - 0xA;
            }

            *ptr++ = character;

            max_stream--;
        }
    }

    // Check if 'max_stream' was/will be exceeded
    if (0 == max_stream)
    {
        if (append_null)
        {
            return 0;
        }
        else if (0 != value)
        {
            return 0;
        }
    }

    bytes_consumed = (unsigned)(ptr - stream);

    // If 'zero_pad_length' > 'bytes_consumed', then some
    // zero padding needs to be added. (We're still in reverse order).
    for (i = bytes_consumed; i < zero_pad_length; i++)
    {
        if (0 == max_stream)
        {
            return 0;
        }

        *ptr++ = '0';
        max_stream--;
    }

    // recalculate
    bytes_consumed = (unsigned)(ptr - stream);

    // Reverse order of string
    swap_length = bytes_consumed / 2;
    for (i = 0; i < swap_length; i++)
    {
        char temp = stream[i];
        stream[i] = stream[bytes_consumed - i - 1];
        stream[bytes_consumed - i - 1] = temp;
    }

    if (append_null)
    {
        stream[bytes_consumed] = '\0';
    }

    return bytes_consumed;
}

unsigned rutils_unsigned32_to_hex_ascii(char    *stream,
                                        unsigned max_stream,
                                        uint32_t value,
                                        bool     append_null,
                                        unsigned zero_pad_length,
                                        bool     upper_case)
{
    unsigned bytes_consumed = 0;
    unsigned nibble;
    unsigned swap_length;
    unsigned i;
    char     character;
    char    *ptr = stream;

    if (0 == max_stream)
    {
        return 0;
    }

    if (0 == value)
    {
        *ptr++ = '0';
        max_stream--;
    }
    else
    {
        // Write hex equivalent in reverse order, no
        // zero padding
        while ((value > 0) && (max_stream > 0))
        {
            nibble = value & BIT_MASK_NIBBLE;
            value >>= BITS_PER_NIBBLE;

            if (nibble < 0xA)
            {
                character = '0' + nibble;
            }
            else if (upper_case)
            {
                character = 'A' + nibble - 0xA;
            }
            else
            {
                character = 'a' + nibble - 0xA;
            }

            *ptr++ = character;

            max_stream--;
        }
    }

    // Check if 'max_stream' was/will be exceeded
    if (0 == max_stream)
    {
        if (append_null)
        {
            return 0;
        }
        else if (0 != value)
        {
            return 0;
        }
    }

    bytes_consumed = (unsigned)(ptr - stream);

    // If 'zero_pad_length' > 'bytes_consumed', then some
    // zero padding needs to be added. (We're still in reverse order).
    for (i = bytes_consumed; i < zero_pad_length; i++)
    {
        if (0 == max_stream)
        {
            return 0;
        }

        *ptr++ = '0';
        max_stream--;
    }

    // recalculate
    bytes_consumed = (unsigned)(ptr - stream);

    // Reverse order of string
    swap_length = bytes_consumed / 2;
    for (i = 0; i < swap_length; i++)
    {
        char temp = stream[i];
        stream[i] = stream[bytes_consumed - i - 1];
        stream[bytes_consumed - i - 1] = temp;
    }

    if (append_null)
    {
        stream[bytes_consumed] = '\0';
    }

    return bytes_consumed;
}

//!
//! @name      rutils_decimal_ascii_to_unsigned64
//! @name      rutils_decimal_ascii_to_unsigned32
//
//! @brief     Convert an ASCII stream of contiguos decimal
//! @brief     values to a binary value.
//
//! @param[in]  'stream'-- 
//! @param[out] 'value'-- ptr to result
//! @param[out] 'failure'-- (optional) ptr to flag indicating a failed result
//
//! @return    Number of chars
unsigned rutils_decimal_ascii_to_unsigned64(const char *stream,
                                            uint64_t   *value,
                                            bool       *failure)
{
    const char *ptr;
    unsigned count;
    unsigned i;
    uint64_t accumulator = 0;
    uint64_t multiplier = 1;
    uint64_t last_accumulator = 0;
    uint64_t last_multiplier = 0;

    count = rutils_count_of_decimal_ascii_span(stream);

    ptr = &stream[count - 1];

    for (i = 0; i < count; i++)
    {
        char digit = *(ptr--);
        unsigned digit_value = rutils_decimal_digit_to_value(digit);

        accumulator += digit_value * multiplier;
        multiplier *= 10;

        // If there was a 64-bit overflow, then the following test will catch it
        if ((accumulator < last_accumulator) || (multiplier < last_multiplier))
        {
            if (NULL != failure)
            {
                *failure = true;
                return count;
            }
        }

        last_accumulator = accumulator;
        last_multiplier = multiplier;
    }

    *value = accumulator;

    if (NULL != failure)
    {
        *failure = 0 == count;
    }

    return count;
}

unsigned rutils_decimal_ascii_to_unsigned32(const char *stream,
                                            uint32_t   *value,
                                            bool       *failure)
{
    const char *ptr;
    unsigned count;
    unsigned i;
    uint32_t accumulator = 0;
    uint32_t multiplier = 1;
    uint32_t last_accumulator = 0;
    uint32_t last_multiplier = 0;

    count = rutils_count_of_decimal_ascii_span(stream);

    ptr = &stream[count - 1];

    for (i = 0; i < count; i++)
    {
        char digit = *(ptr--);
        unsigned digit_value = rutils_decimal_digit_to_value(digit);

        accumulator += digit_value * multiplier;
        multiplier *= 10;

        // If there was a 64-bit overflow, then the following test will catch it
        if ((accumulator < last_accumulator) || (multiplier < last_multiplier))
        {
            if (NULL != failure)
            {
                *failure = true;
                return count;
            }
        }

        last_accumulator = accumulator;
        last_multiplier = multiplier;
    }

    *value = accumulator;

    if (NULL != failure)
    {
        *failure = 0 == count;
    }

    return count;
}

//! @name      rutils_hex_ascii_to_unsigned64
//
//! @brief     Convert an ASCII stream of contiguos hex
//! @brief     values to a binary value.
//
//! @param[in]  'stream'-- 
//! @param[out] 'value'-- ptr to result
//! @param[out] 'failure'-- (optional) ptr to flag indicating a failed result
//
//! @return    Number of chars
unsigned rutils_hex_ascii_to_unsigned64(const char *stream,
                                        uint64_t   *value,
                                        bool       *failure)
{
    unsigned count;
    int      int_i;
    uint64_t value64 = 0;
    unsigned shift = 0;
    unsigned digit_value;

    count = rutils_count_of_hex_ascii_span(stream);

    if (count > 0)
    {
        for (int_i = count - 1; int_i >= 0; int_i--)
        {
            digit_value = rutils_hex_digit_to_value(stream[int_i]);
            value64 += (uint64_t)digit_value << shift;

            shift += BITS_PER_NIBBLE;
        }
    }

    *value = value64;

    if (NULL != failure)
    {
        *failure = 0 == count;
    }

    return count;
}

unsigned rutils_hex_ascii_to_unsigned32(const char *stream,
                                        uint32_t   *value,
                                        bool       *failure)
{
    unsigned count;
    int      int_i;
    uint32_t value32 = 0;
    unsigned shift = 0;
    unsigned digit_value;

    count = rutils_count_of_hex_ascii_span(stream);

    if (count > 0)
    {
        for (int_i = count - 1; int_i >= 0; int_i--)
        {
            digit_value = rutils_hex_digit_to_value(stream[int_i]);
            value32 += (uint32_t)digit_value << shift;

            shift += BITS_PER_NIBBLE;
        }
    }

    *value = value32;

    if (NULL != failure)
    {
        *failure = 0 == count;
    }

    return count;
}

//! @name      rutils_is_decimal_digit
//! @name      rutils_is_hex_digit
//
//! @brief     Tests single char if it's a legitimate decimal/hex ASCII digit
//
//! @param[in] 'digit'-- 
//
//! @return    bool
bool rutils_is_decimal_digit(char digit)
{
    return ( (digit >= '0') && (digit <= '9') );
}

bool rutils_is_hex_digit(char digit)
{
    bool is_hex_digit = ((digit >= '0') && (digit <= '9')) ||
                        ((digit >= 'a') && (digit <= 'f')) ||
                        ((digit >= 'A') && (digit <= 'F'));
    return is_hex_digit;
}

//! @name      rutils_decimal_digit_to_value
//! @name      rutils_hex_digit_to_value
//
//! @brief     Convert a single ASCII digit in decimal/hex format to non-ASCII
//! @brief     equivalent.
//
//! @param[in] 'digit'-- 
//
//! @return    converted value
unsigned rutils_decimal_digit_to_value(char digit)
{
    return (unsigned)(digit - '0');
}

unsigned rutils_hex_digit_to_value(char digit)
{
    unsigned value;

    if ((digit >= '0') && (digit <= '9'))
    {
        value = digit - '0';
    }
    else if ((digit >= 'a') && (digit <= 'f'))
    {
        value = digit - 'a' + 0xA;
    }
    else
    {
        value = digit - 'A' + 0xA;
    }

    return value;
}

//! @name      rutils_count_of_decimal_ascii_span
//! @name      rutils_count_of_hex_ascii_span
//
//! @brief     Gives count of number of contiguous chars that
//! @brief     are legitimate decimal or hex ASCII values
//
//! @param[in] 'stream'-- 
//
//! @return    Number of chars
unsigned rutils_count_of_decimal_ascii_span(const char *stream)
{
    unsigned count;

    for (count = 0; true; count++)
    {
        if (!rutils_is_decimal_digit(*(stream++)))
        {
            return count;
        }
    }
}

// (see above)
unsigned rutils_count_of_hex_ascii_span(const char *stream)
{
    unsigned count;

    for (count = 0; true; count++)
    {
        bool is_hex_digit = rutils_is_hex_digit(*(stream++));

        if (!is_hex_digit)
        {
            return count;
        }
    }
}

//!
//! @name      scan_printf_specifier
//
//! @brief     parse printf "%" conversion specifier. Limited functionality.
//
//! @param[in]  'stream'-- beginning of a "%s" specifier string
//! @param[out] 'specifier_ptr'-- results of specifier parse
//! @param[out] 'failure_ptr'-- set to 'true' if failure
//
//! @return    Length of conversion specifier.
//!
//
// https://www.gnu.org/software/libc/manual/pdf/libc.pdf
// 12.12.2 Output Conversion Syntax
//
// Quoted from glibc docs:
// https://www.gnu.org/software/libc/manual/html_mono/libc.html
//
// Here is an example. Using the template string:
// 
// "|%5d|%-5d|%+5d|%+-5d|% 5d|%05d|%5.0d|%5.2d|%d|\n"
// 
// to print numbers using the different options for the ‘%d’ conversion
// gives results like:
// 
// |    0|0    |   +0|+0   |    0|00000|     |   00|0|
// |    1|1    |   +1|+1   |    1|00001|    1|   01|1|
// |   -1|-1   |   -1|-1   |   -1|-0001|   -1|  -01|-1|
// |100000|100000|+100000|+100000| 100000|100000|100000|100000|100000|
// 
// In particular, notice what happens in the last case where the number is
// too large to fit in the minimum field width specified.
// 
// Here are some more examples showing how unsigned integers print under
// various format options, using the template string:
// 
// "|%5u|%5o|%5x|%5X|%#5o|%#5x|%#5X|%#10.8x|\n"
// 
// |    0|    0|    0|    0|    0|    0|    0|  00000000|
// |    1|    1|    1|    1|   01|  0x1|  0X1|0x00000001|
// |100000|303240|186a0|186A0|0303240|0x186a0|0X186A0|0x000186a0|
//
static unsigned scan_printf_specifier(const char *stream,
         rutils_printf_specifier_t *specifier_ptr,
         bool *failure_ptr)
{
    const char *stream_ptr = stream;
    uint64_t  value64 = 0;
    unsigned  scan_length;
    rutils_printf_type_t type_specifier;
    bool      invalid_type_specifier = false;

    rutils_memset(specifier_ptr, 0, sizeof(rutils_printf_specifier_t));
    *failure_ptr = false;

    if ('%' != *stream_ptr++)
    {
        *failure_ptr = true;
        return 0;
    }

    if ('%' == *stream_ptr)
    {
        specifier_ptr->percent_escape_only = true;
        return 2;
    }

    if ('-' == *stream_ptr)
    {
        specifier_ptr->left_justify = true;
        stream_ptr++;
    }

    if ('0' == *stream_ptr)
    {
        specifier_ptr->leading_zeros = true;
        stream_ptr++;
    }

    if (rutils_is_decimal_digit(*stream_ptr))
    {
        scan_length = rutils_decimal_ascii_to_unsigned64(
                                         stream_ptr, &value64, failure_ptr);

        if (true == *failure_ptr)
        {
            return 0;
        }

        specifier_ptr->has_width = true;
        specifier_ptr->width = (unsigned)value64;

        stream_ptr += scan_length;
    }

    if ('.' == *stream_ptr)
    {
        stream_ptr++;

        if (rutils_is_decimal_digit(*stream_ptr))
        {
            scan_length = rutils_decimal_ascii_to_unsigned64(
                                         stream_ptr, &value64, failure_ptr);

            if (true == *failure_ptr)
            {
                return 0;
            }

            specifier_ptr->has_precision = true;
            specifier_ptr->precision = (unsigned)value64;

            stream_ptr += scan_length;
        }
    }

    if (rutils_strncmp("h", stream_ptr, sizeof("h")) == false)
    {
        specifier_ptr->has_size_specifier = true;
        specifier_ptr->size_specifier = RUTILS_PST_SHORT;
        stream_ptr++;
    }
    else if (rutils_strncmp("ll", stream_ptr, sizeof("ll")) == false)
    {
        specifier_ptr->has_size_specifier = true;
        specifier_ptr->size_specifier = RUTILS_PST_LONG_LONG;
        stream_ptr += sizeof("ll");
    }
    else
    {
        specifier_ptr->size_specifier = RUTILS_PST_NONE;
    }

    switch (*stream_ptr)
    {
    case 'i':
    case 'd':
        type_specifier = RUTILS_PTT_INT;
        break;
    case 'u':
        type_specifier = RUTILS_PTT_UNSIGNED;
        break;
    case 'X':
        type_specifier = RUTILS_PTT_HEX_CAPS;
        break;
    case 'x':
        type_specifier = RUTILS_PTT_HEX_LOWER;
        break;
    case 'c':
        type_specifier = RUTILS_PTT_CHAR;
        break;
    case 's':
        type_specifier = RUTILS_PTT_STRING;
        break;
    default:
        invalid_type_specifier = true;
        break;
    }

    if (invalid_type_specifier)
    {
        *failure_ptr = true;
        return 0;
    }
    else
    {
        specifier_ptr->type = type_specifier;

        stream_ptr++;
    }

    return (unsigned)(stream_ptr - stream);
}

//!
//! @name      rutils_sprintf
//
//! @brief     sprintf-equivalent. Limited functionality.
//
//! @details   examples:
//! @details      rutils_sprintf(ptr, 100, "%d", 10);
//
//! @param[out] 'out_stream'-- output
//! @param[in]  'out_stream_size'-- max number bytes that can be written to
//! @param[in]                'out_stream'
//! @param[in]  'control'--  "  %d example"
//! @param[in]  (parameter list)
//
//! @return    Number of chars placed on 'out_stream'
unsigned rutils_sprintf(char       *out_stream,
                        unsigned    out_stream_size,
                        const char *control,
                        ...)
{
    unsigned result = 0;
    va_list arguments;

    va_start(arguments, control);
    result = rutils_sprintf_args(out_stream,
                                 out_stream_size,
                                 control,
                                 arguments);
    va_end(arguments);
    return result;
}
unsigned rutils_sprintf_args(char       *out_stream,
                             unsigned    out_stream_size,
                             const char *control,
                             va_list    arguments)
{
    int64_t   value_i64;
    uint64_t  value_u64;
    unsigned  remaining_len = out_stream_size;
    unsigned  copy_len;
    unsigned  scan_len;
    unsigned  value_len;
    unsigned  padding_len;
    bool      scan_fail = false;
    char      this_char;
    char     *stream = out_stream;
    const char *control_ptr = control;
    char     *percent_ptr;
    char     *string_ptr = NULL;
    char      conv_string[24];
    rutils_printf_specifier_t specifier;

    while (('\0' != *control_ptr) && (remaining_len > 0))
    {
        this_char = *control_ptr;
        if ('%' == this_char)
        {
            scan_len = scan_printf_specifier(control_ptr, &specifier, &scan_fail);

            if (scan_fail)
            {
                *out_stream = '\0';
                return 0;
            }

            control_ptr += scan_len;

            value_i64 = 0;
            value_u64 = 0;

            if (RUTILS_PTT_CHAR == specifier.type)
            {
                value_u64 = (uint64_t)va_arg(arguments, int);
                value_len = 1;
                conv_string[0] = (char)value_u64;
                conv_string[1] = '\0';
            }
            else if (RUTILS_PTT_STRING == specifier.type)
            {
                string_ptr = (char *)va_arg(arguments, char *);
                value_len = rutils_strlen(string_ptr);
            }
            else
            {
                if (RUTILS_PST_SHORT == specifier.size_specifier)
                {
                    if (RUTILS_PTT_INT == specifier.type)
                    {
                        value_i64 = (uint64_t)va_arg(arguments, int);
                    }
                    else
                    {
                        value_u64 = (uint64_t)va_arg(arguments, int);
                    }
                }
                else if (RUTILS_PST_LONG_LONG == specifier.size_specifier)
                {
                    if (RUTILS_PTT_INT == specifier.type)
                    {
                        value_i64 = va_arg(arguments, int64_t);
                    }
                    else
                    {
                        value_u64 = va_arg(arguments, uint64_t);
                    }
                }
                else
                {
                    if (RUTILS_PTT_INT == specifier.type)
                    {
                        value_i64 = (int64_t)va_arg(arguments, int);
                    }
                    else
                    {
                        value_u64 = (uint64_t)va_arg(arguments, unsigned);
                    }
                }

                if ((RUTILS_PTT_HEX_CAPS == specifier.type) ||
                    (RUTILS_PTT_HEX_LOWER == specifier.type))
                {
                    value_len = rutils_unsigned64_to_hex_ascii(conv_string,
                                        sizeof(conv_string),
                                        value_u64,
                                        true,
                                        false,
                                        RUTILS_PTT_HEX_CAPS == specifier.type);
                }
                else if (RUTILS_PTT_INT == specifier.type)
                {
                    value_len = rutils_signed64_to_decimal_ascii(conv_string,
                                        sizeof(conv_string),
                                        value_i64,
                                        true);
                }
                else
                {
                    value_len = rutils_unsigned64_to_decimal_ascii(conv_string,
                                        sizeof(conv_string),
                                        value_u64,
                                        true);
                }
            }

            if (value_len > remaining_len)
            {
                return 0;
            }

            // Could be larger than simple print?
            // Do we need to add some sort of padding?
            if (specifier.has_width && (specifier.width > value_len))
            {
                if (specifier.width > remaining_len)
                {
                    return 0;
                }

                padding_len = specifier.width - value_len;

                // Copy 'conv_string' then copy padding
                if (specifier.left_justify)
                {
                    // should copy 'value_len' bytes
                    if (RUTILS_PTT_STRING == specifier.type)
                    {
                    //    STRNCPY_S(stream, remaining_len, string_ptr,
                    //              value_len);
                        rutils_strncpy(stream, string_ptr, value_len);
                    }
                    else
                    {
                    //    STRNCPY_S(stream, remaining_len, conv_string,
                    //              value_len);
                        rutils_strncpy(stream, conv_string, value_len);
                    }
                    stream += value_len;
                    rutils_memset(stream, ' ', padding_len);
                    stream += padding_len;

                    remaining_len -= value_len + padding_len;
                }
                // Copy padding then copy 'conv_string'
                else
                {
                    if (specifier.leading_zeros)
                    {
                        rutils_memset(stream, '0', padding_len);
                    }
                    else
                    {
                        rutils_memset(stream, ' ', padding_len);
                    }
                    stream += padding_len;
                    remaining_len -= padding_len;

                    if (RUTILS_PTT_STRING == specifier.type)
                    {
                    //    STRNCPY_S(stream, remaining_len, string_ptr,
                    //              value_len);
                        rutils_strncpy(stream, string_ptr, value_len);
                    }
                    else
                    {
                    //    STRNCPY_S(stream, remaining_len, conv_string,
                    //              value_len);
                        rutils_strncpy(stream, conv_string, value_len);
                    }
                    stream += value_len;

                    remaining_len -= value_len + padding_len;
                }
            }
            // else, no width specifier
            else
            {
                if (RUTILS_PTT_STRING == specifier.type)
                {
                //    STRNCPY_S(stream, remaining_len, string_ptr,
                //              value_len);
                    rutils_strncpy(stream, string_ptr, value_len);
                }
                else
                {
                //    STRNCPY_S(stream, remaining_len, conv_string,
                //              value_len);
                    rutils_strncpy(stream, conv_string, value_len);
                }

                stream += value_len;
                remaining_len -= value_len;
            }
        }
        // Copy text between "%" specifiers
        else
        {
            // Get ptr to next specifier
            percent_ptr = rutils_strchr(control_ptr, '%');
            // No more specifiers
            if (NULL == percent_ptr)
            {
                copy_len = rutils_strlen(control_ptr);
            }
            // There is another specifier.
            else
            {
                copy_len = (unsigned)(percent_ptr - control_ptr);
            }

            // Will this copy run over buffer?
            if (copy_len > remaining_len)
            {
                copy_len = remaining_len;
            }

            //STRNCPY_S(stream, remaining_len, control_ptr, copy_len);
            rutils_strncpy(stream, control_ptr, copy_len);

            control_ptr += copy_len;
            stream += copy_len;
            remaining_len -= copy_len;
        }
    }

    if (remaining_len > 0)
    {
        *stream++ = '\0';
        remaining_len--;
    }
    else
    {
        // We're out of space. Back up to last byte and add safety-null
        *(stream - 1) = '\0';
    }

    return (unsigned)(stream - out_stream);
}
