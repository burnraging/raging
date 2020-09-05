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

//! @file     ut-nvm-tag.c
//! @authors  Bernie Woodland
//! @date     2May19
//!
//! @brief    FLASH driver for storing NV items in discrete chunks
//!

#include "raging-global.h"
#include "raging-utils.h"
#include "raging-utils-mem.h"
#include "raging-contract.h"

#include "nvm-tag.h"
#include "nvm-desc.h"
#include "nvm-tag.h"


//******************************************************************************
// Duplicate definitions (hacks)
//******************************************************************************

#define SECTOR_RESERVED_SIZE 16
#define VERSION_MIN           1
#define VERSION_MAX        0xFFFE
#define VERSION_MAX_SANE    65532
#define HEADER_SIZE          12

//******************************************************************************
// 
//******************************************************************************

#define MAX_ARRAY        100
uint32_t   addressArray[MAX_ARRAY];
uint16_t   versionArray[MAX_ARRAY];
uint16_t   lengthArray[MAX_ARRAY];

// STATUS_HEADER_WRITTEN    0x01
// STATUS_DATA_WRITTEN      0x02
// STATUS_DIRTY             0x04   //sane tag marked as obsolete
// STATUS_INSANE            0x08   //mark tag as only 1/2 written
//
// header only written
uint8_t partwayWrittenTag1[] = {0xA5, 0xFF, 0x00, 0x01, 0x00, 0x01, 0x00, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// all of tag written, including data, except status 'data written' not written
uint8_t partwayWrittenTag2[] = {0xA5, 0xFE, 0x00, 0x01, 0x00, 0x01, 0x00, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xBB, 0xCC};
    
    //uint8_t data1[] =  {1, 2, 3, 4, 5};
uint8_t data2[256];

uint8_t randomTo255[] = {129, 178, 0, 142, 118, 17, 163, 63, 182, 75, 213, 99};
uint8_t randomTo25[] = {0, 20, 9, 11, 0, 5, 14, 0, 14, 11, 25, 2, 10};

uint8_t lastRandom255 = 0;
uint8_t lastRandom25 = 0;

uint8_t LookupRandom255(void)
{
    uint8_t value = lastRandom255;
    value = WRAP(value + 1, sizeof(randomTo255));
    lastRandom255 = value;
    return randomTo255[value];
}

uint8_t LookupRandom25(void)
{
    uint8_t value = lastRandom25;
    value = WRAP(value + 1, sizeof(randomTo25));
    lastRandom25 = value;
    return randomTo25[value];
}

void PackData(uint8_t value)
{
    int i;

    rutils_memset(data2, 0, sizeof(data2));
    for (i = 0; i < value; i++)
    {
        data2[i] = value;
    }
}

bool CustomGarbageCollectThreshold(tag_space_t space)
{
    const space_desc_t *space_desc = nvm_GetSpaceDesc(space);
	sector_stats_t *stats_base = nvm_GetSectorStatsBase(space);
    sector_stats_t *stats;
    int           i;
    int           hits = 0;

    for (i = 0; i < space_desc->numberOfSectors; i++)
    {
        stats = &stats_base[i];

        if ((stats->freeSpaceBytes < 8144/4) ||
            (stats->uncleanTagBytes > 8144*2/3))
        {
            hits++;
        }
    }

    return hits >= 6;
}

//******************************************************************************
// Tests
//******************************************************************************

// Exercise NVMtotalReset(), NVMlatestTagInfo()
void test_total_resets(void)
{
    uint16_t data_length = 5;
    uint16_t tagId = 1;
	uint8_t *read_data_ptr;
	uint16_t read_length = 0xFFFF;
    uint16_t info_version = 0;
    uint16_t info_length = 0;
    uint32_t info_address = 0;
    bool     rv;

	NVMtotalReset(SPACE_MINI);

	// Can write & read a single tag
	NVMinit(true);

	PackData((uint8_t)data_length);
    
	NVMwriteTag(SPACE_MINI, tagId, data2, data_length);

    rv = NVMlatestTagInfo(SPACE_MINI, tagId, &info_version, &info_length, &info_address);
    UT_ENSURE(rv);
    UT_ENSURE(info_version != 0);
    UT_ENSURE(info_length == data_length);
    UT_ENSURE(info_address != 0);

	NVMreadTag(SPACE_MINI, tagId, (void **)&read_data_ptr, &read_length);
	UT_ENSURE(read_length == data_length);
	UT_ENSURE(rutils_memcmp(read_data_ptr, data2, read_length) < 0);

    // After reset, can't read that tag anymore
	NVMtotalReset(SPACE_MINI);

	NVMinit(true);

    info_version = 0;
    info_length = 0;
    info_address = 0;
    rv = NVMlatestTagInfo(SPACE_MINI, tagId, &info_version, &info_length, &info_address);
    UT_ENSURE(!rv);
    UT_ENSURE(info_version == 0);
    UT_ENSURE(info_length == 0);
    UT_ENSURE(info_address == 0);

	NVMreadTag(SPACE_MINI, tagId, (void **)&read_data_ptr, &read_length);
	UT_ENSURE(0 == read_length);
    UT_ENSURE(NULL == read_data_ptr);
}

// Writes a single tag 2000 times
void test_single_tag_writes(void)
{
    unsigned i;
    uint8_t  fixed_byte;
    unsigned data_length;
    unsigned tag_id;
    bool     rv;
	uint8_t *read_data_ptr;
	uint16_t read_length = 0xFFFF;
    uint16_t info_version = 0;
    uint16_t info_length = 0;
    uint32_t info_address = 0;
    uint16_t info_version_last = 0;
    uint32_t info_address_last = 0;

	NVMtotalReset(SPACE_MINI);

    NVMinit(true);

    for (i = 0; i < 2000; i++)
    {
        // write same tag length, but rotate data
        data_length = 5;
        fixed_byte = (uint8_t)(i & 0xFF);
        tag_id = 1;
        rutils_memset(data2, 0, sizeof(data2));
        rutils_memset(data2, fixed_byte, data_length);
        NVMwriteTag(SPACE_MINI, tag_id, data2, data_length);

        // Read back ok? Data as expected?
	    NVMreadTag(SPACE_MINI, tag_id, (void **)&read_data_ptr, &read_length);
	    UT_ENSURE(read_length == data_length);
	    UT_ENSURE(rutils_memcmp(read_data_ptr, data2, read_length) < 0);

        // Verify that in fact a new tag was written.
        // Verify that tag version number incremented as expected.
        rv = NVMlatestTagInfo(SPACE_MINI, tag_id, &info_version, &info_length, &info_address);
        UT_ENSURE(rv);
        UT_ENSURE((info_version != 0) && (info_version > info_version_last) && (info_version == i+1));
        UT_ENSURE(info_length == data_length);
        UT_ENSURE((info_address != 0) && (info_address != info_address_last));

        info_version_last = info_version;
        info_address_last = info_address;
    }
}

// If doing periodic garbage collection should be able to write
// forever.
// Check version number rollover too.
void test_indefinite_writes(void)
{
    unsigned i;
    uint8_t  fixed_byte;
    unsigned data_length;
    unsigned tag_id;
	uint8_t *read_data_ptr;
	uint16_t read_length = 0xFFFF;
    int      gc_result;
    bool     rv;
    bool     ein_result;
    unsigned gc_count = 0;
    uint16_t info_version = 0;
    uint16_t info_version_last = 0;

	NVMtotalReset(SPACE_MINI);

    NVMinit(true);

    for (i = 0; i < 100000; i++)
    {
        // write same tag length, but rotate data
        data_length = 5;
        fixed_byte = (uint8_t)(i & 0xFF);
        tag_id = 1;
        rutils_memset(data2, 0, sizeof(data2));
        rutils_memset(data2, fixed_byte, data_length);
        NVMwriteTag(SPACE_MINI, tag_id, data2, data_length);

        // Read back ok? Data as expected?
	    NVMreadTag(SPACE_MINI, tag_id, (void **)&read_data_ptr, &read_length);
	    UT_ENSURE(read_length == data_length);
	    UT_ENSURE(rutils_memcmp(read_data_ptr, data2, read_length) < 0);

        // Verify version number increments.
        // Verify version number rollover.
        rv = NVMlatestTagInfo(SPACE_MINI, tag_id, &info_version, NULL, NULL);
        UT_ENSURE(rv);
        UT_ENSURE((info_version >= VERSION_MIN) && (info_version <= VERSION_MAX));

        if (info_version_last == VERSION_MAX_SANE)
        {
            UT_ENSURE(info_version == VERSION_MIN);
        }
        else
        {
            UT_ENSURE(info_version == info_version_last + 1);
        }

        info_version_last = info_version;

        // Garbage collect every 100 writes
        if ((i % 100) == 0)
        {
            gc_result = NVMgarbageCollectNoErase(SPACE_MINI, SCORE_ASYMPTOTIC);

            if (gc_result >= 0)
            {
                ein_result = NVMeraseIfNeeded(SPACE_MINI);
                UT_ENSURE(ein_result);

                gc_count += ein_result? 1 : 0;

                // Corner-case: update version number change in case reclaim bumped it
                rv = NVMlatestTagInfo(SPACE_MINI, tag_id, &info_version_last, NULL, NULL);
                UT_ENSURE(rv);
            }
        }
    }
}

void test_random_writes(void)
{
    int i;
    int k;
    int sectorToErase;
    uint16_t dataLength;
    uint16_t tagId;
    uint16_t numResults;
    space_vitals_t *spaceVitals = nvm_GetSpaceVitals(SPACE_MINI);

	NVMtotalReset(SPACE_MINI);

    NVMinit(true);

//    NVMreadTag(SPACE_MINI, 1, (void **)&data2, &length);

    for (i = 0; i < 400000; i++)
    {
        dataLength = LookupRandom255();
        tagId = LookupRandom25();
        if (tagId == 0)
            tagId = 1;
        PackData((uint8_t)dataLength);
        NVMwriteTag(SPACE_MINI, tagId, data2, dataLength);

//        if (spaceVitals->currentWriteSector != 0)
//            k = i;

        if (i == 360000)
        {
            k = i;
        }

        if (CustomGarbageCollectThreshold(SPACE_MINI))
        {
            numResults = 0;
//            NVMnVersions(SPACE_MINI, tagId, 0, 0, addressArray, versionArray, lengthArray, MAX_ARRAY, &numResults);
            sectorToErase = NVMgarbageCollectNoErase(SPACE_MINI, SCORE_ASYMPTOTIC);
            if (RFAIL_NOT_FOUND != sectorToErase)
            {
                NVMeraseSectorForeground(SPACE_MINI, (uint16_t)sectorToErase);
            }
        }
    }

    sectorToErase = NVMgarbageCollectNoErase(SPACE_MINI, SCORE_ASYMPTOTIC);
    if (RFAIL_NOT_FOUND != sectorToErase)
    {
        NVMeraseSectorForeground(SPACE_MINI, (uint16_t)sectorToErase);
    }
    sectorToErase = NVMgarbageCollectNoErase(SPACE_MINI, SCORE_ASYMPTOTIC);
    if (RFAIL_NOT_FOUND != sectorToErase)
    {
        NVMeraseSectorForeground(SPACE_MINI, (uint16_t)sectorToErase);
    }
}

void test_partway_written_tag_header_only(void)
{
    const unsigned DATA_3_TAG_ID = 1;
    uint8_t data_of_3[3] = {1,2,3};
    const space_desc_t *desc = nvm_GetSpaceDesc(SPACE_MINI);
    sector_stats_t *sector_stats_base = nvm_GetSectorStatsBase(SPACE_MINI);
    sector_stats_t *sector_stats;
    uint32_t *tag_base_ptr;
    uint8_t  *tag_ptr;
    uint16_t max_tags = 0;

    tag_base_ptr = nvm_GetTagPtrBase(SPACE_MINI, &max_tags);
    sector_stats = &sector_stats_base[0];           // We're using sector 0

	NVMtotalReset(SPACE_MINI);

    UT_REQUIRE(0 == tag_base_ptr[DATA_3_TAG_ID - 1]);

    // Write header only. This simulates a powerdown while tag is being written.
    rutils_memcpy((uint8_t *)desc->startAddress + SECTOR_RESERVED_SIZE, partwayWrittenTag1, sizeof(partwayWrittenTag1));

    // Init with parm of 'true' should clean this tag up
    // We should be able to continue
    NVMinit(true);

    // Verify that we did successfully close tag
    UT_ENSURE(0 == tag_base_ptr[DATA_3_TAG_ID - 1]);
    UT_ENSURE((sector_stats->numInsaneTags == 1) && (sector_stats->numCleanTags == 0) &&
              (sector_stats->numDirtyTags == 0));

    // Sector should be ready to receive a valid write
    NVMwriteTag(SPACE_MINI, 1, data_of_3, sizeof(data_of_3));

    // Verify that write was successful
    tag_ptr = (uint8_t *)tag_base_ptr[DATA_3_TAG_ID - 1];
    tag_ptr += HEADER_SIZE;
    UT_ENSURE(NULL != tag_ptr);
    UT_ENSURE(rutils_memcmp(tag_ptr, data_of_3, sizeof(data_of_3)) < 0);
    UT_ENSURE((sector_stats->numInsaneTags == 1) && (sector_stats->numCleanTags == 1) &&
              (sector_stats->numDirtyTags == 0));
}

void test_partway_written_tag_data_missing(void)
{
    const unsigned DATA_3_TAG_ID = 1;
    uint8_t data_of_3[3] = {1,2,3};
    const space_desc_t *desc = nvm_GetSpaceDesc(SPACE_MINI);
    sector_stats_t *sector_stats_base = nvm_GetSectorStatsBase(SPACE_MINI);
    sector_stats_t *sector_stats;
    uint32_t *tag_base_ptr;
    uint8_t  *tag_ptr;
    uint16_t max_tags = 0;

    tag_base_ptr = nvm_GetTagPtrBase(SPACE_MINI, &max_tags);
    sector_stats = &sector_stats_base[0];           // We're using sector 0

	NVMtotalReset(SPACE_MINI);

    UT_REQUIRE(0 == tag_base_ptr[DATA_3_TAG_ID - 1]);

    // Write header only. This simulates a powerdown while tag is being written.
    rutils_memcpy((uint8_t *)desc->startAddress + SECTOR_RESERVED_SIZE, partwayWrittenTag2, sizeof(partwayWrittenTag2));

    // Init with parm of 'true' should clean this tag up
    // We should be able to continue
    NVMinit(true);

    // Verify that we did successfully close tag
    UT_ENSURE(0 == tag_base_ptr[DATA_3_TAG_ID - 1]);
    UT_ENSURE((sector_stats->numInsaneTags == 1) && (sector_stats->numCleanTags == 0) &&
              (sector_stats->numDirtyTags == 0));

    // Sector should be ready to receive a valid write
    NVMwriteTag(SPACE_MINI, 1, data_of_3, sizeof(data_of_3));

    // Verify that write was successful
    tag_ptr = (uint8_t *)tag_base_ptr[DATA_3_TAG_ID - 1];
    tag_ptr += HEADER_SIZE;
    UT_ENSURE(NULL != tag_ptr);
    UT_ENSURE(rutils_memcmp(tag_ptr, data_of_3, sizeof(data_of_3)) < 0);
    UT_ENSURE((sector_stats->numInsaneTags == 1) && (sector_stats->numCleanTags == 1) &&
              (sector_stats->numDirtyTags == 0));
}

void test_powerdown_during_erase(void)
{
    unsigned i;
    uint8_t  fixed_byte;
    unsigned data_length;
    unsigned tag_id;
	uint16_t read_length = 0xFFFF;
    const space_desc_t *desc = nvm_GetSpaceDesc(SPACE_MINI);
    sector_stats_t *sector_stats_base = nvm_GetSectorStatsBase(SPACE_MINI);
    sector_stats_t *sector_stats;
    uint32_t *tag_base_ptr;
    uint32_t *tag_ptr;
    uint16_t max_tags = 0;
    uint8_t  *sector_start_address;

    tag_base_ptr = nvm_GetTagPtrBase(SPACE_MINI, &max_tags);

    NVMtotalReset(SPACE_MINI);

    NVMinit(true);

    // Load up sector 0 with some tag writes, but don't run
    //  into another sector. Sector will have 1 good tag and
    //  and 99 dirty tags.
    for (i = 0; i < 100; i++)
    {
        // write same tag length, but rotate data
        data_length = 5;
        fixed_byte = (uint8_t)(i & 0xFF);
        tag_id = 1;
        rutils_memset(data2, 0, sizeof(data2));
        rutils_memset(data2, fixed_byte, data_length);
        NVMwriteTag(SPACE_MINI, tag_id, data2, data_length);
    }

    tag_ptr = &tag_base_ptr[tag_id];
    sector_stats = &sector_stats_base[0];           // We're using sector 0

    // preliminary sanity checks...
    UT_REQUIRE(NULL != tag_ptr);
    UT_ENSURE((sector_stats->numInsaneTags == 0) && (sector_stats->numCleanTags == 1) &&
              (sector_stats->numDirtyTags == 99));

    // Let's simulate an erase that didn't complete.
    // We'll assume erase wrote FF's starting at top of sectors but
    // didn't finish...
    sector_start_address = (uint8_t *)desc->startAddress;   // start of sector 0
    rutils_memset(sector_start_address, 0xFF, 32);

    // We just power cycled and came back up. Executing init.
    // This should clean up the bad sector
    NVMinit(true);

    // Was sector cleaned?
    tag_ptr = (uint32_t *)tag_base_ptr[tag_id];
    sector_stats = &sector_stats_base[0];           // We're using sector 0
    UT_REQUIRE(NULL == tag_ptr);
    UT_ENSURE((sector_stats->numInsaneTags == 0) && (sector_stats->numCleanTags == 0) &&
              (sector_stats->numDirtyTags == 0));
}

int main(void)
{
    test_total_resets();
    test_single_tag_writes();
    test_indefinite_writes();
    test_random_writes();
    test_partway_written_tag_header_only();
    test_partway_written_tag_data_missing();
    test_powerdown_during_erase();
}