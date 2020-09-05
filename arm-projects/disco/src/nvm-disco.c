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

//! @file     nvm-disco.c
//! @authors  Bernie Woodland
//! @date     1May19
//!
//! @brief    Describes Disco platform flash part
//!

#include "raging-global.h"
#include "raging-contract.h"
#include "raging-utils-mem.h"

#include "nvm-platform.h"
#include "nvm-desc.h"
#include "nvm-tag.h"
#include "nvm-stm32f4xx.h"

//------------------ Temp flags ----------------------------------

// 22Jun2019: Can't get sectors 0-3 to work.
//  Every time changes are made to LinkerScript.ld to move
//  text segment from 0x8000000 to 0x8010000, debugger crashes
//  after a few lines of code. Using last 2 128k sectors as workaround.
//#define USE_REAR_SECTORS

//------------------ Data Sector's Space -------------------------

#ifdef USE_REAR_SECTORS
    #define NUM_SECTORS_DATA             2
#else
    // Not using 4th sector, saving that for crash dump
    #define NUM_SECTORS_DATA             3
#endif


#ifdef USE_REAR_SECTORS
    #define DATA_SECTOR_SIZE           0x20000   // 128k
#else
    #define DATA_SECTOR_SIZE           0x4000    //  16k
#endif


static uint32_t DataTagPtrs[MAX_TAGS_DATA];

static const space_desc_t DataDesc = 
{
#ifdef USE_REAR_SECTORS
    0x80C0000,                          // start address of sector 10
#else
    0x8000000,                          // start address of sector 0
#endif
    DATA_SECTOR_SIZE,                   //sector length
    NUM_SECTORS_DATA                    //number of sectors
};

static space_vitals_t    DataSpaceVitals;
static sector_vitals_t   DataVitals[NUM_SECTORS_DATA];
static sector_stats_t    DataStats[NUM_SECTORS_DATA];
static space_stats_t     DataSpaceStats;


const tag_space_t NVMallSpaces[] = {SPACE_DATA};

//------------------ API's -------------------------

void nvm_register_fatal_error(unsigned error_reason)
{
    UNUSED(error_reason);
    UT_REQUIRE(false);
}

tag_space_t nvm_GetTagSpace(unsigned index)
{
    if (index > SPACE_max)
        nvm_register_fatal_error(REASON_BAD_SPACE_DESC_ENUM);

    return NVMallSpaces[index];
}

//
uint32_t *nvm_GetTagPtrBase(tag_space_t space, uint16_t *maxTags)
{
    uint32_t *base;

    switch (space)
    {
        //STM data sector is only currently supported space
        case SPACE_DATA:
            base = DataTagPtrs;
            *maxTags = MAX_TAGS_DATA;
            break;

        default:
            nvm_register_fatal_error(REASON_BAD_TAG_PTR_ENUM);

            base = NULL;     //to rid warning
            *maxTags = 0;
            break;
    }

    return base;
}

const space_desc_t *nvm_GetSpaceDesc(tag_space_t space)
{
    switch (space)
    {
        case SPACE_DATA:
            return &DataDesc;
            break;

        default:
            break;
    }

    nvm_register_fatal_error(REASON_BAD_SPACE_DESC_ENUM);

    return NULL;       //to rid warning
}

space_vitals_t *nvm_GetSpaceVitals(tag_space_t space)
{
    switch (space)
    {
        case SPACE_DATA:
            return &DataSpaceVitals;
            break;

        default:
            break;
    }

    nvm_register_fatal_error(REASON_BAD_SPACE_VITALS_ENUM);

    return NULL;       //to rid warning
}

space_stats_t *nvm_GetSpaceStats(tag_space_t space)
{
    switch (space)
    {
        case SPACE_DATA:
            return &DataSpaceStats;
            break;

        default:
            break;
    }

    nvm_register_fatal_error(REASON_BAD_SPACE_STATS_ENUM);

    return NULL;       //to rid warning
}

sector_stats_t *nvm_GetSectorStatsBase(tag_space_t space)
{
    switch (space)
    {
        case SPACE_DATA:
            return DataStats;
            break;

        default:
            break;
    }

    nvm_register_fatal_error(REASON_BAD_SECTOR_STATS_ENUM);

    return NULL;       //to rid warning
}

sector_vitals_t *nvm_GetSectorVitalsBase(tag_space_t space)
{
    switch (space)
    {
        case SPACE_DATA:
            return DataVitals;
            break;

        default:
            break;
    }

    nvm_register_fatal_error(REASON_BAD_SECTOR_VITALS_BASE_ENUM);

    return NULL;       //to rid warning
}



void nvm_low_level_init(void)
{
    // The disco board is not capable of STM_FLASH_VOLTAGE_4, because that
    //  would require an external Vpp.
    stm_flash_init(STM_FLASH_VOLTAGE_3);
}

// Called if a write or flash failed.
// Rest flash part, if possible, so we can retry.
void nvm_low_level_flash_hardware_reset(void)
{
    // TBD
}

nvm_low_level_status_t nvm_low_level_flash_write(uint8_t *address,
                                                 uint8_t *data,
                                                 unsigned data_length)
{
    if (stm_flash_write(address, data, data_length) == STM_FLASH_FAILURE)
    {
        return NVM_LOW_LEVEL_FAILURE;
    }

    return NVM_LOW_LEVEL_SUCCESS;
}

nvm_low_level_status_t nvm_low_level_flash_erase(tag_space_t  space,
                                                 uint16_t     sector_number)
{
    stm_flash_status_t status;

    UNUSED(space);

    status = stm_flash_erase(0, sector_number);

    if (STM_FLASH_FAILURE == status)
    {
        //fixme: check status, etc

        APP_ENSURE(false);
        return NVM_LOW_LEVEL_FAILURE;
    }

    return NVM_LOW_LEVEL_SUCCESS;
}
