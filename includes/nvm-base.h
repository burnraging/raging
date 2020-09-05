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

//! @file     nvm-base.h
//! @authors  Bernie Woodland
//! @date     2May19
//!
//! @brief    Common includes throughout NVM files
//!

#ifndef NVM_BASE_H_
#define NVM_BASE_H_

#include "raging-global.h"

//******************************************************************************
// Constants and accessors
//******************************************************************************

//-------------- Translated Fatal Codes -----------------

#define REASON_BAD_TAG_PTR_ENUM               1
#define REASON_BAD_SPACE_DESC_ENUM            2
#define REASON_BAD_SPACE_VITALS_ENUM          3
#define REASON_BAD_SPACE_STATS_ENUM           4
#define REASON_BAD_SECTOR_STATS_ENUM          5
#define REASON_BAD_SECTOR_VITALS_BASE_ENUM    6
#define REASON_2ND_WRITE_FAILED               7
#define REASON_WRITE_TO_UNFRESH_FLASH         8
#define REASON_0_TO_1_WRITE_ATTEMPT           9
#define REASON_OVERRUN_SECTOR_WHILE_WRITING  10
#define REASON_FAILED_VERIFY_OF_HEADER_WRITE 11
#define REASON_SECTORNUM_OVERRUN             12
#define REASON_BAD_LAST_TAG_ADDRESS          13
#define REASON_WRITE_PARMS_SANITY_CHECK      14
#define REASON_WRITE_SANITY_CHECK            15
#define REASON_PAST_SECTOR_NUMBER_INVALID    16
#define REASON_CANT_FIX_PARTIAL_TAG          17
#define REASON_TAG_FIX_FAILED                18
#define REASON_SECTOR_UNFIXABLE              19
#define REASON_ERASE_VERIFY_FAIL             20
#define REASON_BGND_ERASE_FAIL               21
#define REASON_NO_MORE_ROOM_FOR_WRITE        22
#define REASON_WRITE_FAILED_WHILE_ABANDONING 23
#define REASON_INVALID_TAG_NUMBER            24
#define REASON_AVAILABLE_SPACE_SANITY_ERROR  25

//defines which sectors have been allocated to a given tag space
typedef struct SPACE_DESC_ 
{
    uint32_t startAddress;
    uint32_t sectorLength;
    uint16_t numberOfSectors;
} space_desc_t;

typedef struct SECTOR_VITALS_
{
    uint8_t        *lastTagAddress;   //address of last sane tag in sector
} sector_vitals_t;

typedef struct SPACE_VITALS_
{
    uint16_t     currentWriteSector;
    bool         digDeeperIntoGarbage;
    uint16_t     sectorErasing;
    uint16_t     sectorAbandoning;
} space_vitals_t;


#endif  // NVM_BASE_H_