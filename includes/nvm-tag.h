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

//! @file     nvm-tag.c
//! @authors  Bernie Woodland
//! @date     17May18
//!
//! @brief    FLASH driver for storing NV items in discrte chunks
//!

#ifndef NVM_TAG_H
#define NVM_TAG_H

#include "raging-global.h"

#include "nvm-base.h"
#include "nvm-platform.h"

//******************************************************************************
// Constants and accessors
//******************************************************************************

//******************************************************************************
// Enums and Structs
//******************************************************************************

typedef struct SPACE_STATS_
{
    uint32_t freeSpace;
    uint32_t totalCleanBytes;
    uint32_t totalUncleanBytes;
    uint16_t rampNM;
    uint16_t thresholdNM;
    uint16_t garbageRatioNM;
    uint16_t maxUncleanTagSectorNumber;
} space_stats_t;

typedef struct SECTOR_STATS_
{
    uint16_t numCleanTags;
    uint16_t numDirtyTags;
    uint16_t numInsaneTags;
    uint32_t cleanTagBytes;    //sum of all clean tags' bytes occupied
    uint32_t uncleanTagBytes;  //sum of all dirty & insane tags' bytes occupied
    uint32_t freeSpaceBytes;   //usable space (minus headroom)
} sector_stats_t;

typedef enum
{
    SCORE_MOST_UNCLEAN,       //pick sector that has most garbage. must pick one.
    SCORE_UNCLEAN_THRESHOLD,  //pick sector /w most garbage, but only if above a threshold
    SCORE_ASYMPTOTIC          //like above, but decrease threshold as space becomes full
} score_method_t;

//******************************************************************************
// Exported Functions
//******************************************************************************

RAGING_EXTERN_C_START
void NVMinit(bool findAndEraseBadSectors);
void NVMreadTag(tag_space_t space,
                uint16_t    tagNumber,
                void    **dataPtr,
                uint16_t   *dataLength);
void NVMwriteTag(tag_space_t space,
                 uint16_t    tagNumber,
                 void     *dataPtr,
                 uint16_t    dataLength);
int NVMgarbageCollectNoErase(tag_space_t space, score_method_t score_method);
void NVMeraseSectorForeground(tag_space_t space, uint16_t sectorNumber);
void NVMeraseSectorBackground(tag_space_t space, uint16_t sectorNumber);
bool NVMeraseIfNeeded(tag_space_t space);
void NVMbackgroundEraseCompleteCallback(tag_space_t space);

void NVMtotalReset(tag_space_t space);
bool NVMlatestTagInfo(tag_space_t space,
                     uint16_t    tagNumber,
                     uint16_t   *version,
                     uint16_t   *length,
                     uint32_t   *address);
bool NVMsanityCheckSector(tag_space_t space, uint16_t sectorNumber);
sector_stats_t *NVMfetchSectorStats(tag_space_t     space,
                                 uint16_t        sectorNumber);
bool NVMnVersions(tag_space_t space,
                 uint16_t    tagNumber,
                 uint16_t    versionHi,
                 uint16_t    versionLo,
                 uint32_t   *addressArray,
                 uint16_t   *versionArray,
                 uint16_t   *lengthArray,
                 uint16_t    maxArray,
                 uint16_t   *numResults);
RAGING_EXTERN_C_END

#endif  /* NVM_TAG_H */
