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

//! @file     nvm-desc.h
//! @authors  Bernie Woodland
//! @date     1May19
//!
//! @brief    Describes Disco platform flash part
//!

#ifndef NVM_DESC_H_
#define NVM_DESC_H_

#include "nvm-base.h"
#include "nvm-platform.h"
#include "nvm-tag.h"

typedef enum
{
    NVM_LOW_LEVEL_SUCCESS,
    NVM_LOW_LEVEL_FAILURE
} nvm_low_level_status_t;

RAGING_EXTERN_C_START
void nvm_register_fatal_error(unsigned error_reason);
tag_space_t nvm_GetTagSpace(unsigned index);
uint32_t *nvm_GetTagPtrBase(tag_space_t space, uint16_t *maxTags);
const space_desc_t *nvm_GetSpaceDesc(tag_space_t space);
space_vitals_t *nvm_GetSpaceVitals(tag_space_t space);
space_stats_t *nvm_GetSpaceStats(tag_space_t space);
sector_stats_t *nvm_GetSectorStatsBase(tag_space_t space);
sector_vitals_t *nvm_GetSectorVitalsBase(tag_space_t space);

void nvm_low_level_init(void);
void nvm_low_level_flash_hardware_reset(void);
nvm_low_level_status_t nvm_low_level_flash_write(uint8_t *address,
                                                 uint8_t *data,
                                                 unsigned data_length);
nvm_low_level_status_t nvm_low_level_flash_erase(tag_space_t  space,
                                                 uint16_t     sectorNumber);
RAGING_EXTERN_C_END

#endif  // NVM_DESC_H_