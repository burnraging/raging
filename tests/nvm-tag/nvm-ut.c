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

//! @file     nvm-ut.c
//! @authors  Bernie Woodland
//! @date     2May19
//!
//! @brief    Describes Disco platform flash part
//!

#include "raging-global.h"
#include "raging-contract.h"
#include "raging-utils-mem.h"

#include "nvm-platform.h"
#include "nvm-desc.h"
#include "nvm-tag.h"

//******************************************************************************
// Constants and accessors
//******************************************************************************

#define NUM_SECTORS_MINI                   6

#define MINI_SECTOR_SIZE                8192


//******************************************************************************
// Variables
//******************************************************************************

//------------------ Variables to Support Offline Testing ---------------------

typedef struct
{
    uint8_t sector0[MINI_SECTOR_SIZE];
    uint8_t sector1[MINI_SECTOR_SIZE];
    uint8_t sector2[MINI_SECTOR_SIZE];
    uint8_t sector3[MINI_SECTOR_SIZE];
    uint8_t sector4[MINI_SECTOR_SIZE];
    uint8_t sector5[MINI_SECTOR_SIZE];
} all_sectors_t;

// *** This is the simulated flash storage:
all_sectors_t asecs;

#define UT_FLASH_8KB_SECTOR_START     ( (uint32_t) &asecs ) 



//------------------ Mini Sector's Space -------------------------

static uint32_t MiniTagPtrs[MAX_TAGS_MINI];

static const space_desc_t MiniDesc = 
{
    UT_FLASH_8KB_SECTOR_START,         // start address of sectors
                                        // (fixme...use  _APP_MINI_SPACE_START instead)
    MINI_SECTOR_SIZE,                   //sector length
    NUM_SECTORS_MINI                    //number of sectors
};

static space_vitals_t    MiniSpaceVitals;
static sector_vitals_t   MiniVitals[NUM_SECTORS_MINI];
static sector_stats_t    MiniStats[NUM_SECTORS_MINI];
static space_stats_t     MiniSpaceStats;


const tag_space_t NVMallSpaces[] = {SPACE_MINI};

//******************************************************************************
// Function Prototypes (ideally, would be in .h file)
//******************************************************************************

#define BYTE_NEVER_WRITTEN           0xFF   // duplicate define, bit of a hack

bool GetSectorAddressAndPlusOne(tag_space_t space,
                                uint16_t    sectorNumber,
                                uint8_t   **sectorAddress,
                                uint8_t   **sectorEndAddressPlusOne);
bool IsFlashModifyLegit(uint8_t *data, uint32_t dataLength, uint8_t *address);
void MergeDataWithExisting(uint8_t *data, uint32_t dataLength, uint8_t *flashAddress);

//******************************************************************************
// API Functions
//******************************************************************************

void nvm_register_fatal_error(unsigned error_reason)
{
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
        //Mini sector is only currently supported space
        case SPACE_MINI:
            base = MiniTagPtrs;
            *maxTags = MAX_TAGS_MINI;
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
        case SPACE_MINI:
            return &MiniDesc;
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
        case SPACE_MINI:
            return &MiniSpaceVitals;
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
        case SPACE_MINI:
            return &MiniSpaceStats;
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
        case SPACE_MINI:
            return MiniStats;
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
        case SPACE_MINI:
            return MiniVitals;
            break;

        default:
            break;
    }

    nvm_register_fatal_error(REASON_BAD_SECTOR_VITALS_BASE_ENUM);

    return NULL;       //to rid warning
}

void nvm_low_level_init(void)
{
    // Nothing to do
}

void nvm_low_level_flash_hardware_reset(void)
{
    // This will happen in UT environment
}

nvm_low_level_status_t nvm_low_level_flash_write(uint8_t *address,
                                                 uint8_t *data,
                                                 unsigned data_length)
{
    uint32_t i;
    uint8_t  currentValue;
    uint8_t  writeValue;

    for (i = 0; i < data_length; i++)
    {
        currentValue = address[i];
        writeValue = data[i];

        if ( !IsFlashModifyLegit(&writeValue, sizeof(writeValue), &currentValue) )
        {
            //trying to write a 0 to a 1?
            UT_REQUIRE(false);

            return NVM_LOW_LEVEL_FAILURE;
        }

        MergeDataWithExisting(&writeValue, sizeof(writeValue), &currentValue);

        address[i] = writeValue;
    }

    return NVM_LOW_LEVEL_SUCCESS;
}

nvm_low_level_status_t nvm_low_level_flash_erase(tag_space_t  space,
                                                 uint16_t     sectorNumber)
{
    uint8_t  *sectorAddress = NULL;
    uint8_t  *sectorEndAddressPlusOne = NULL;
    bool      rc;

    rc = GetSectorAddressAndPlusOne(space, sectorNumber, &sectorAddress,
                                    &sectorEndAddressPlusOne);

    rutils_memset(sectorAddress, BYTE_NEVER_WRITTEN,
           (uint32_t)(sectorEndAddressPlusOne - sectorAddress));

    return NVM_LOW_LEVEL_SUCCESS;
}
