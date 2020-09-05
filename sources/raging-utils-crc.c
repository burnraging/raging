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

//! @file     raging-utils-crc.c
//! @authors  Bernie Woodland
//! @date     7Apr18
//!
//! @brief    FCS/CRC/Checksum routines
//!

#include "raging-utils-crc.h"

//!
//! @name      rutils_crc16_add_string
//!
//! @brief     Calculate CRC16 over a partial fragment
//!
//! @param[in] 'current_crc'-- if first time called,
//! @param[in]     result from 'rutils_crc16_start()'; all other times,
//! @param[in]     result from last call to 'rutils_crc16_add_string()'
//! @param[in] 'buffer'--
//! @param[in] 'length'--
//!
//! @return    CRC
//!
uint16_t rutils_crc16_add_string(uint16_t current_crc,
                                 uint8_t *buffer,
                                 unsigned length)
{
    unsigned i;
    uint16_t x;
    uint8_t  character;

    for (i = 0; i < length; i++)
    {
        character = buffer[i];

        x = (current_crc ^ character) & BIT_MASK8;
        x = (x ^ (x << 4)) & BIT_MASK8;
        x = (x << 8) ^ (x << 3) ^ (x >> 4);

        current_crc = (current_crc >> BITS_PER_WORD8) ^ x;
    }

    return current_crc;
}

//!
//! @name      rutils_crc16_start
//!
//! @brief     Seed initial value for a CRC16 calculation
//!
//! @return    initial value
//!
uint16_t rutils_crc16_start(void)
{
    return 0xFFFF;
}

//!
//! @name      rutils_crc16_buffer
//!
//! @brief     Do a CRC16 over an entire buffer
//!
//! @details   Just this fcn. alone will do a CRC-16/MCRF4XX.
//! @details   AHDLC does a CRC-16/X-25 (see crccalc.com).
//! @details   To convert from this fcn. to CRC-16/X-25, EOR
//! @details   result with 0xFFFF.
//! @details   If this fcn. is called to validate a byte string,
//! @details   and the byte string includes the CRC itself, then
//! @details   result will be 'RUTILS_CRC16_GOOD'
//! @details   
//! @details   
//! @details   
//!
//! @param[in] 'buffer'--
//! @param[in] 'length'--
//!
//! @return    CRC
//!
uint16_t rutils_crc16_buffer(uint8_t *buffer, unsigned length)
{
    uint16_t crc;

    crc = rutils_crc16_start();

    crc = rutils_crc16_add_string(crc, buffer, length);

    return crc;
}

//!
//! @name      rutils_crc32b
//!
//! @brief     Performs a CRC32 type "b" on a buffer
//!
//! @details   Verification:
//! @details        Input: buffer[] = { 0x61, 0x73, 0x64, 0x0A }, length=4
//! @details                               "asd\n"
//! @details        Output: 0x152DDECE
//! @details             http://www.md5calc.com/
//!
//! @return    CRC
//!
uint32_t rutils_crc32b(uint8_t *buffer, unsigned length)
{
    int i;
    int j;
    // cppcheck-suppress variableScope
    uint32_t byte;
    uint32_t crc;
    uint32_t mask;
    unsigned counter;

    i = 0;
    crc = 0xFFFFFFFF;
    for (counter = 0; counter < length; counter++)
    {
        byte = buffer[i];
        crc = crc ^ byte;
        for (j = 7; j >= 0; j--)
        {
            // Compiler warning on this, replaced
            //mask = -(crc & 1);
            mask = (uint32_t)( -((int)(crc & 1)) );
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }

        i = i + 1;
   }

   return ~crc;
}