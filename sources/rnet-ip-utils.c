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

//! @file     rnet-ip-utils.c
//! @authors  Bernie Woodland
//! @date     31Dec18
//!
//! @brief    IPv4+IPv6 ASCII conversions, other utility fcns.
//!

#include "raging-global.h"
#include "rnet-ip-utils.h"
#include "raging-utils.h"
#include "raging-utils-mem.h"
#include "raging-utils-scan-print.h"
#include "rnet-ip-base-defs.h"

// For ascii representations only
#define IPV6_MAX_VALUES    (IPV6_ADDR_SIZE / 2)
#define MAGIC_UNSET          100        // some crazy number


//!
//! @name      rnet_ip_match_is_exact_match
//!
//! @brief     Are 2 IP addresses the same?
//!
//! @param[in] 'is_ipv6'-- 'false' if this is IPv4
//! @param[in] 'ip_addr_ref'-- address #1 to be matched
//! @param[in] 'ip_addr'-- address #2 to be matched
//!
//! @return    'true' if both addresses are the same
//!
bool rnet_ip_match_is_exact_match(bool                  is_ipv6,
                                  rnet_ip_addr_union_t *ip_addr_ref,
                                  rnet_ip_addr_union_t *ip_addr)
{
    int      result;
    unsigned values_to_match;

    if (is_ipv6)
    {
        values_to_match = IPV6_ADDR_SIZE;
    }
    else
    {
        values_to_match = IPV4_ADDR_SIZE;
    }

    result = rutils_memcmp(
                 (uint8_t *)ip_addr_ref, (uint8_t *)ip_addr, values_to_match);

    return RFAIL_NOT_FOUND == result;
}

//!
//! @name      rnet_ip_is_null_address
//!
//! @brief     Is this IP address null (all zeros)?
//!
//! @param[in] 'is_ipv6'-- 'false' if this is IPv4
//! @param[in] 'ip_addr'-- 
//!
//! @return    'true' if address is null
//!
bool rnet_ip_is_null_address(bool                  is_ipv6,
                             rnet_ip_addr_union_t *ip_addr)
{
    static uint8_t null_address[IPV6_ADDR_SIZE] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    return rnet_ip_match_is_exact_match(is_ipv6,
                           (rnet_ip_addr_union_t *)null_address,
                           ip_addr);
}

//!
//! @name      rnet_is_link_local_address
//!
//! @brief     Is this IP address a link local address?
//!
//! @param[in] 'ip_addr'-- 
//!
//! @return    'true' if address is link local
//!
bool rnet_is_link_local_address(rnet_ip_addr_union_t *ip_addr)
{
    return (ip_addr->ipv6_addr[0] == 0xFE) && (ip_addr->ipv6_addr[0] == 0x80);
}

//!
//! @name      rnet_ip_match_prefix_length
//!
//! @brief     Finds length of prefix that matches between 2 IP addresses
//!
//! @param[in] 'is_ipv6'-- 'false' if this is IPv4
//! @param[in] 'ip_addr_ref'-- address #1 to be matched
//! @param[in] 'ip_addr'-- address #2 to be matched
//!
//! @return    Number of bits of address prefix matched.
//! @return    For example, if IPv6 and addresses are the same, will return 128
//!
unsigned rnet_ip_match_prefix_length(bool                  is_ipv6,
                                     rnet_ip_addr_union_t *ip_addr_ref,
                                     rnet_ip_addr_union_t *ip_addr)
{
    unsigned match_length = 0;
    unsigned values_to_match;
    unsigned ref;
    unsigned src;
    unsigned i;
    int      posit_int;

    if (is_ipv6)
    {
        values_to_match = IPV6_ADDR_SIZE;
    }
    else
    {
        values_to_match = IPV4_ADDR_SIZE;
    }

    // Examine each byte in address
    for (i = 0; i < values_to_match; i++)
    {
        ref = ip_addr_ref->ipv6_addr[i];
        src = ip_addr->ipv6_addr[i];

        if (ref == src)
        {
            match_length += BITS_PER_WORD8;
        }
        else
        {
            // EOR will set bits=='1' which aren't the same
            posit_int = rutils_msb_bit_position8(ref ^ src);

            // Bit position includes all bits before first bit set
            match_length += BITS_PER_WORD8 - (unsigned)posit_int - 1;

            return match_length;
        }
    }

    return match_length;
}

//!
//! @name      rnet_ipv4_ascii_to_binary
//!
//! @brief     Converts an IPv4 ascii string address to 4-byte equivalent
//!
//! @param[out] 'ip_addr_binary->ipv4_addr'--
//! @param[in] 'ip_addr_ascii'-- ascii form of address. Ex: "20.100.15.3"
//! @param[in] 'expect_null_terminated_string'-- if 'true', then address
//! @param[in]     string must have '\0', else will return an error.
//!
//! @return    Number of bytes consumed. If error: RFAIL_ERROR.
//!
int rnet_ipv4_ascii_to_binary(rnet_ip_addr_union_t *ip_addr_binary,
                              const char           *ip_addr_ascii,
                              bool                  expect_null_terminated_string)
{
    uint32_t value32;
    unsigned scan_length;
    unsigned bytes_consumed = 0;
    bool     failure = false;
    unsigned i;
    uint8_t  value8;
    const unsigned IPV4_MAX_SINGLE_VALUE = 255;

    for (i = 0; i < IPV4_ADDR_SIZE; i++)
    {
        scan_length = rutils_decimal_ascii_to_unsigned32(ip_addr_ascii,
                                                         &value32,
                                                         &failure);

        bytes_consumed += scan_length;
        ip_addr_ascii += scan_length;

        if (failure)
        {
            return RFAIL_ERROR;
        }

        // Single value is limited to 1 byte's length
        if (value32 > IPV4_MAX_SINGLE_VALUE)
        {
            return RFAIL_ERROR;
        }

        value8 = (uint8_t)value32;

        ip_addr_binary->ipv4_addr[i] = value8;

        // Make sure we're dot-delimited
        if (i < IPV4_ADDR_SIZE - 1)
        {
            if (*ip_addr_ascii != '.')
            {
                return RFAIL_ERROR;
            }

            bytes_consumed++;
            ip_addr_ascii++;
        }
    }

    // Error: cannot have trailing dot!
    if (*ip_addr_ascii == '.')
    {
        return RFAIL_ERROR;
    }

    // If string containing address was supposed to be null-terminated,
    // error check that there's no stray chars at end of string.
    if (expect_null_terminated_string && (*ip_addr_ascii != '\0'))
    {
        return RFAIL_ERROR;
    }

    // 'bytes_consumed' doesn't include '\0' when 'expect_null_terminated_string'
    //   is true.
    return (int)bytes_consumed;
}


//!
//! @name      rnet_ipv6_ascii_to_binary
//!
//! @brief     Converts an IPv6 ascii string address to 16-byte equivalent
//!
//! @param[out] 'ip_addr_binary->ipv6_addr'--
//! @param[in] 'ip_addr_ascii'-- ascii form of address. Ex: "200:123::1"
//! @param[in] 'expect_null_terminated_string'-- if 'true', then address
//! @param[in]     string must have '\0', else will return an error.
//!
//! @return    Number of bytes consumed. If error: RFAIL_ERROR.
//!
int rnet_ipv6_ascii_to_binary(rnet_ip_addr_union_t *ip_addr_binary,
                              const char           *ip_addr_ascii,
                              bool                  expect_null_terminated_string)
{
    uint32_t value32;
    unsigned scan_length;
    unsigned bytes_consumed = 0;
    bool     failure = false;
    unsigned i;
    // Max number of colon-delimited values
    // Max value of a single colon-delimited value
    const unsigned IPV6_MAX_SINGLE_VALUE = 0xFFFF;
    uint16_t value16;
    uint16_t single_values[IPV6_MAX_VALUES];
    unsigned num_single_values = 0;
    unsigned double_colon_before = MAGIC_UNSET;
    unsigned num_values_to_left;
    unsigned num_values_to_right;

    // Check if the "::" is in the leading position
    if (!rutils_strncmp(ip_addr_ascii, "::", sizeof("::") - 1))
    {
        double_colon_before = 0;
        bytes_consumed += sizeof("::") - 1;
        ip_addr_ascii += sizeof("::") - 1;
    }

    // Loop through and collect all address values
    // Address values are the hex numbers, 1-4 digits
    while (1)
    {
        scan_length = rutils_hex_ascii_to_unsigned32(ip_addr_ascii,
                                                     &value32,
                                                     &failure);

        // Expect last scan to fail: either we hit the '\0' or
        // we hit some unrecognizable character (not a colon
        // or valid hex number, however).
        if (failure)
        {
            break;
        }

        // Too many values?
        if (num_single_values == IPV6_MAX_VALUES)
        {
            return RFAIL_ERROR;
        }

        bytes_consumed += scan_length;
        ip_addr_ascii += scan_length;

        // Overrange on value?
        if (value32 > IPV6_MAX_SINGLE_VALUE)
        {
            return RFAIL_ERROR;
        }

        value16 = (uint16_t)value32;
        single_values[num_single_values++] = value16;

        // Value trailed by single or double colon?
        if (*ip_addr_ascii == ':')
        {
            bytes_consumed++;
            ip_addr_ascii++;

            // double colon?
            if (*ip_addr_ascii == ':')
            {
                bytes_consumed++;
                ip_addr_ascii++;

                // Cannot have 2 double colons in same address
                if (double_colon_before != MAGIC_UNSET)
                {
                    return RFAIL_ERROR;
                }

                double_colon_before = num_single_values;
            }
        }
    }

    // Error: cannot have single/double colon after last value scanned
    if (*ip_addr_ascii == ':')
    {
        return RFAIL_ERROR;
    }

    // Error: in absence of double-colon, not enough digits
    if ((double_colon_before == MAGIC_UNSET) &&
        (num_single_values != IPV6_MAX_VALUES))
    {
        return RFAIL_ERROR;
    }

    // Clear address. This ensures values implied by "::"
    // will stay zero-filled.
    rutils_memset(ip_addr_binary, 0, IPV6_ADDR_SIZE);

    // No double colon?
    if (MAGIC_UNSET == double_colon_before)
    {
        num_values_to_left = IPV6_MAX_VALUES;
        num_values_to_right = 0;
    }
    // Calculate number of values to left of and to right
    // of "::"
    else
    {
        num_values_to_left = double_colon_before;
        num_values_to_right = num_single_values - num_values_to_left;
    }

    // Pack values to left of "::"
    for (i = 0; i < num_values_to_left; i++)
    {
        uint16_t x = single_values[i];

        // Each 16-bit value packs into 2 address bytes
        ip_addr_binary->ipv6_addr[i*2] = (uint8_t)(x >> BITS_PER_WORD8);
        ip_addr_binary->ipv6_addr[i*2 + 1] = (uint8_t)(x & BIT_MASK8);
    }

    // Pack values to right of "::"
    for (i = 0; i < num_values_to_right; i++)
    {
        uint16_t x = single_values[i + num_single_values - num_values_to_right];
        unsigned start_index = (IPV6_MAX_VALUES - num_values_to_right) * 2;

        ip_addr_binary->ipv6_addr[start_index + i*2] =
                                           (uint8_t)(x >> BITS_PER_WORD8);
        ip_addr_binary->ipv6_addr[start_index + i*2 + 1] =
                                           (uint8_t)(x & BIT_MASK8);
    }

    // If string containing address was supposed to be null-terminated,
    // error check that there's no stray chars at end of string.
    if (expect_null_terminated_string && (*ip_addr_ascii != '\0'))
    {
        return RFAIL_ERROR;
    }

    // 'bytes_consumed' doesn't include '\0' when 'expect_null_terminated_string'
    //   is true.
    return (int)bytes_consumed;
}


//!
//! @name      rnet_ipv4_binary_to_ascii
//!
//! @brief     Converts an IPv4 binary address to ascii string
//!
//! @param[out] 'ip_addr_binary->ipv4_addr'--
//! @param[in] 'ip_addr_ascii'-- ascii form of address. Ex: "20.100.15.3"
//! @param[in]      requires IPV4_ADDR_ASCII_SIZE bytes, if null terminated,
//! @param[in]      requires IPV4_ADDR_ASCII_SIZE+1 bytes
//! @param[in] 'append_null'-- if 'true', then append '\0' to string
//! @param[in]     string must have '\0', else will return an error.
//!
//! @return    Bytes written to 'ip_addr_ascii', inc. '\0'.
//! @return    Never any error
//!
unsigned rnet_ipv4_binary_to_ascii(
                     const rnet_ip_addr_union_t *ip_addr_binary,
                     char                       *ip_addr_ascii,
                     bool                        append_null)
{
    unsigned bytes_written;
    unsigned bytes_consumed = 0;
    unsigned i;

    for (i = 0; i < IPV4_ADDR_SIZE; i++)
    {
        bytes_written = rutils_unsigned32_to_decimal_ascii(ip_addr_ascii,
                                           3,
                                           ip_addr_binary->ipv4_addr[i],
                                           false);

        ip_addr_ascii += bytes_written;
        bytes_consumed += bytes_written;

        if (i < IPV4_ADDR_SIZE - 1)
        {
            *ip_addr_ascii++ = '.';
            bytes_consumed += 1;
        }
    }

    if (append_null)
    {
        *ip_addr_ascii = '\0';
        bytes_consumed++;
    }

    return bytes_consumed;
}

//!
//! @name      rnet_ipv6_binary_to_ascii
//!
//! @brief     Converts an IPv6 binary address to ascii string
//!
//! @param[out] 'ip_addr_binary->ipv6_addr'--
//! @param[in] 'ip_addr_ascii'-- ascii form of address. Ex: "3000:10::FFFF:8"
//! @param[in]      requires IPV6_ADDR_ASCII_SIZE bytes, if null terminated,
//! @param[in]      requires IPV6_ADDR_ASCII_SIZE+1 bytes
//! @param[in] 'append_null'-- if 'true', then append '\0' to string
//! @param[in]     string must have '\0', else will return an error.
//!
//! @return    Bytes written to 'ip_addr_ascii', inc. '\0'.
//! @return    Never any error
//!
unsigned rnet_ipv6_binary_to_ascii(
                     const rnet_ip_addr_union_t *ip_addr_binary,
                     char                       *ip_addr_ascii,
                     bool                        append_null)
{
    unsigned bytes_consumed = 0;

    uint16_t single_values[IPV6_MAX_VALUES];
    unsigned i;
    bool     in_span = false;
    unsigned current_span = 0;
    unsigned current_span_start = 0;
    unsigned largest_span = 0;
    unsigned largest_span_start = 0;
    unsigned num_values_to_left;
    unsigned num_values_to_right;

    // Convert 8-bit values to 16-bit values
    for (i = 0; i < IPV6_MAX_VALUES; i++)
    {
        single_values[i] = (uint16_t)ip_addr_binary->ipv6_addr[i*2] << 8;
        single_values[i] |= ip_addr_binary->ipv6_addr[i*2 + 1];
    }


    // Algorithm to calculate the largest number of contiguous
    // zero values, and where that range starts
    // outputs:
    //    'largest_span'-- length of largest contiguous grouip of zeros
    //    'largest_span_start'-- index in 'single_values' where
    //                     the largest span starts
    for (i = 0; i < IPV6_MAX_VALUES; i++)
    {
        if (0 == single_values[i])
        {
            // This is zero and last was not?
            if (!in_span)
            {
                current_span_start = i;

                in_span = true;
            }

            current_span++;
        }
        else
        {
            // This is not a zero but last one was?
            // ...then we just completed a span.
            if (in_span)
            {
                // Is the span we completed larger than the
                // previous one? If yes, record it.
                if (current_span > largest_span)
                {
                    largest_span = current_span;
                    largest_span_start = current_span_start;
                }

                // Turn off span flag
                in_span = false;
                // Reset span length for next time
                current_span = 0;
            }
        }
    }

    // Finished values, but need to close out final span,
    //  if one was in progress.
    if (in_span)
    {
        if (current_span > largest_span)
        {
            largest_span = current_span;
            largest_span_start = current_span_start;
        }
    }
       

    // All zeros?
    if (IPV6_MAX_VALUES == largest_span)
    {
        const unsigned DOUBLE_COLON_LENGTH = 2;
        rutils_strncpy(ip_addr_ascii, "::", DOUBLE_COLON_LENGTH);

        bytes_consumed = DOUBLE_COLON_LENGTH;
        ip_addr_ascii += DOUBLE_COLON_LENGTH;
    }
    else
    {
        // Should we skip "::" and just print all values?
        if (largest_span < 2)
        {            
            num_values_to_left = IPV6_MAX_VALUES;
            num_values_to_right = 0;
        }
        else
        {
            num_values_to_left = largest_span_start;
            num_values_to_right = IPV6_MAX_VALUES -
                             largest_span_start - largest_span;
        }

        // Print values to the left of the double-colon
        // If no double-colon, then prints everything here
        for (i = 0; i < num_values_to_left; i++)
        {
            const unsigned SOME_LONG_LENGTH = 100;
            unsigned write_length;

            write_length = rutils_unsigned32_to_hex_ascii(ip_addr_ascii,
                                                 SOME_LONG_LENGTH,
                                                 single_values[i],
                                                 false,
                                                 0,
                                                 true);

            bytes_consumed += write_length;
            ip_addr_ascii += write_length;

            // More values to be written?
            if (i < num_values_to_left - 1)
            {
                *ip_addr_ascii++ = ':';
                bytes_consumed++;
            }
        }

        // Insert the "::" if used
        if (num_values_to_left != IPV6_MAX_VALUES)
        {
            const unsigned COPY_LEN = 2;

            rutils_strncpy(ip_addr_ascii, "::", COPY_LEN);

            ip_addr_ascii += COPY_LEN;
            bytes_consumed += COPY_LEN;
        }

        // Print values to the right of the double-colon
        for (i = 0; i < num_values_to_right; i++)
        {
            const unsigned SOME_LONG_LENGTH = 100;
            unsigned write_length;

            write_length = rutils_unsigned32_to_hex_ascii(ip_addr_ascii,
                                                 SOME_LONG_LENGTH,
                single_values[IPV6_MAX_VALUES - num_values_to_right + i],
                                                 false,
                                                 0,
                                                 true);

            bytes_consumed += write_length;
            ip_addr_ascii += write_length;

            // More values to be written?
            if (i < num_values_to_right - 1)
            {
                *ip_addr_ascii++ = ':';
                bytes_consumed++;
            }
        }
    }


    if (append_null)
    {
        *ip_addr_ascii = '\0';
        bytes_consumed++;
    }

    return bytes_consumed;
}