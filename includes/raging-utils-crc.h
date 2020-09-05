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

//! @file     raging-utils-crc.h
//! @authors  Bernie Woodland
//! @date     8Apr18
//!
//! @brief    FCS/CRC/Checksum routines
//!

#ifndef RAGING_UTILS_CRC_H_
#define RAGING_UTILS_CRC_H_

#include "raging-global.h"

//!
//! @enum      RUTILS_CRC16_GOOD
//!
//! @brief     CRC valid result if CRC taken over buffer + CRC itself
//!
#define RUTILS_CRC16_GOOD                            0xF0B8

//!
//! @enum      RUTILS_CRC16_SIZE
//! @enum      RUTILS_CRC32_SIZE
//!
//! @brief     Number of bytes of a CRC16 value
//!
#define RUTILS_CRC16_SIZE                                  2
#define RUTILS_CRC32_SIZE                                  4

// APIs
RAGING_EXTERN_C_START
uint16_t rutils_crc16_add_string(uint16_t current_crc,
                               uint8_t *buffer,
                               unsigned length);
uint16_t rutils_crc16_start(void);
uint16_t rutils_crc16_buffer(uint8_t *buffer, unsigned length);
uint32_t rutils_crc32b(uint8_t *buffer, unsigned length);
RAGING_EXTERN_C_END

#endif //RAGING_UTILS_CRC_H_