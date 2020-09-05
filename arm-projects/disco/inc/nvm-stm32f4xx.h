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

//! @file     nvm-stm32f4xx.h
//! @authors  Bernie Woodland
//! @date     11May19
//!
//! @brief    STM32F4xx series-specific FLASH programming interface
//!

#ifndef NVM_STM32F4XX_H_
#define NVM_STM32F4XX_H_

#include "raging-global.h"

typedef enum
{
    STM_FLASH_SUCCESS,
    STM_FLASH_FAILURE
} stm_flash_status_t;

typedef enum
{
    STM_FLASH_VOLTAGE_1,         // Maps to 'VoltageRange_1'
    STM_FLASH_VOLTAGE_2,         //         'VoltageRange_2'
    STM_FLASH_VOLTAGE_3,         //         'VoltageRange_3'
    STM_FLASH_VOLTAGE_4,         //         'VoltageRange_4'
} stm_flash_voltage_t;

// APIs

RAGING_EXTERN_C_START
bool stm_is_valid_address_bank0(uint8_t *address);
uint8_t *stm_sector_number_to_address_bank0(unsigned sector_number);
unsigned stm_address_to_sector_number(uint8_t *address);
void stm_flash_init(stm_flash_voltage_t voltage_level);
stm_flash_status_t stm_flash_write(uint8_t *address,
                                   uint8_t *data,
                                   unsigned data_length);
stm_flash_status_t stm_flash_erase(unsigned bank, uint16_t sector_number);
RAGING_EXTERN_C_END

#endif  //NVM_STM32F4XX_H_