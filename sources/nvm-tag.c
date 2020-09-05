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
//! @details  
//! @details  This module contains the tag management code for a flash
//! @details  tagging system. It is intended to be used along with these
//! @details  other components:
//! @details  
//! @details  - An optional task wrapper. The wrapper arbitrates between
//! @details    various tasks.
//! @details  - A platform layer. The platform layer defines a tag "SPACE":
//! @details    a space is a single tag system consisting of a collection
//! @details    of flash sectors and tag numbering.
//! @details  - A flash driver. Handles the flash writes and erases.
//! @details  
//! @details  The tag management system is single-threaded (no-reentrant);
//! @details  without the task wrapper, the callers must ensure that reads,
//! @details  writes, and erases occur atomically.
//! @details  
//! @details  The APIs are at the bottom of the file and are prefixed with "NVM"
//!

#include "raging-global.h"

#include "nvm-tag.h"
#include "nvm-desc.h"

#include "raging-utils.h"
#include "raging-utils-mem.h"

//******************************************************************************
// Constants and accessors
//******************************************************************************

//----------- Tag header defines----------------
//
//   -- tag header is 12 bytes (4 bytes reserved)
//   -- header must be aligned on lword (4-byte) boundary

#define MAGIC_NUMBER_SIZE        1
#define STATUS_SIZE              1
#define TAG_NUMBER_SIZE          2
#define VERSION_SIZE             2
#define LENGTH_SIZE              2
#define RESERVED_SIZE            4

#define MAGIC_NUMBER_OFFSET      0
#define STATUS_OFFSET            (MAGIC_NUMBER_SIZE + MAGIC_NUMBER_OFFSET)
#define TAG_NUMBER_OFFSET        (STATUS_SIZE + STATUS_OFFSET)
#define VERSION_OFFSET           (TAG_NUMBER_SIZE + TAG_NUMBER_OFFSET)
#define LENGTH_OFFSET            (VERSION_SIZE + VERSION_OFFSET)
#define RESERVED_OFFSET          (LENGTH_SIZE + LENGTH_OFFSET)
#define HEADER_SIZE              (RESERVED_SIZE + RESERVED_OFFSET)

#define TAGNUM_UNDERRANGE        0x0000
#define TAGNUM_MIN               (TAGNUM_UNDERRANGE + 1)
#define TAGNUM_OVERRANGE         0xFFFF
#define TAGNUM_MAX               (TAGNUM_OVERRANGE - 1)
#define TAGNUM_INSANE            (TAGNUM_OVERRANGE - 2) //missing tag number supplied
#define TAGNUM_MAX_SANE          (TAGNUM_INSANE - 1)

#define VERSION_UNDERRANGE       0x0000
#define VERSION_MIN              (VERSION_UNDERRANGE + 1)
#define VERSION_OVERRANGE        0xFFFF
#define VERSION_MAX              (VERSION_OVERRANGE - 1)
#define VERSION_INSANE           (VERSION_OVERRANGE - 2) //missing version number supplied
#define VERSION_MAX_SANE         (VERSION_INSANE - 1)    //normal max, when assigning version no's
#define VERSION_WRAP_THRESHOLD   0xF000

#define LENGTH_OVERRANGE         0xFFFF

// Tag status values.
//    Remember--bits as they exist in FLASH are active-0, 
//              whereas these defines are active-1
#define STATUS_HEADER_WRITTEN    0x01
#define STATUS_DATA_WRITTEN      0x02
#define STATUS_DIRTY             0x04   //sane tag marked as obsolete
#define STATUS_INSANE            0x08   //mark tag as only 1/2 written
#define STATUS_ALL               (STATUS_HEADER_WRITTEN |                     \
                                  STATUS_DATA_WRITTEN   |                     \
                                  STATUS_DIRTY          |                     \
                                  STATUS_INSANE)
// Tag write cycles:
//   -- write tag header, except for status bits
//   -- set STATUS_HEADER_WRITTEN
//   -- write tag data
//   -- set STATUS_DATA_WRITTEN
//   -- when tag is obsolete, set STATUS_DIRTY
//  Notes:
//    -- if tag header not finished write, on restore, tag header finished filling
//            in and next tag appears 12 bytes later
//    -- if header finished writing but not data, next tag appears length bytes later

//--------------- Default Values --------------------

#define MAGIC_NUMBER             0xA5

#define BYTE_NEVER_WRITTEN       0xFF
#define WORD_NEVER_WRITTEN       0xFFFF

#define SLEEP_DELAY_MILLISECS    100

//---------------- Sector Defines ------------------------

#define SECTOR_RESERVED_SIZE     16     //at top and bottom of sector

//cushion at top of sector, in case write goes beyond this
#define SECTOR_HEADROOM          16

#define SECTOR_ERASE_TIMEOUT    (10*1000)  //millisecondsseconds

//---------------- Garbage Collect Defines ---------------

//defines upper range of normalized values. Normalized values are used
//to insure resolution with integer math
#define NORMALIZED_MAX           1000

#define SINGLE_SECTOR_THRESHOLD   400     //40% garbage threshold for any single sector

#define FREE_SPACE_HI             400     //40% free space in tag-space
#define FREE_SPACE_LO              50     // 5%

#define THRESHOLD_HI              600     //60% garbage in a given sector
#define THRESHOLD_LO               50     // 5% garbage in a given sector

//---------------- Other Defines -------------------------

//number of comparisons done in optimized 'for' loop
#define CHECKS_IN_FOR_LOOP             4

#define INVALID                  (-1)
#define INVALID_UINT16           (0xFFFF)

//---------------- Macros --------------------------------

//******************************************************************************
// Local Structures
//******************************************************************************

//results of sector sanity scan
typedef enum
{
    SECTOR_SANITY_FAILED,       //sector has unrecoverable error
    SECTOR_SANITY_RECOVERABLE,  //last tag partially written + needs to be fixed
    SECTOR_SANITY_SANE          //everything's OK
} sector_sanity_t;

typedef enum
{
    SELECT_FULLEST,
    SELECT_LAST_AND_INCREMENT,
    SELECT_NEXT_AND_INCREMENT
} write_select_t;

typedef enum
{
    LOCK_NULL = 0,
    LOCK_NONE,
    LOCK_READ,
    LOCK_WRITE,
    LOCK_GARBAGE_COLLECT
} space_lock_t;

//******************************************************************************
// Variables
//******************************************************************************

//------------------------ General -------------------------------

bool    NVMinitComplete;
uint8_t NVMflashResets;
uint8_t NVMstatsRepair;

//******************************************************************************
// Local Functions
//******************************************************************************

// Fast check to see if a span of memory, potentially large, is
// equal to a certain byte.
// (tbd...make generic and move to a common file)
static bool IsMemSetToValue(uint8_t *startAddress,
                            uint8_t  value,
                            uint32_t length)
{
    uint32_t *address32;
    uint8_t  *address;
    uint32_t  chunkLength;
    uint32_t  bytesCheckedAlready;
    uint32_t  value32;
    uint32_t  loopBound;
    uint32_t  i;
    bool    miscompare;

    address = startAddress;
    address32 = (uint32_t *)startAddress;
    chunkLength = (uint32_t)((uint32_t)startAddress & 3);

    //check any lword misaligned bytes
    for (i = 0; i < chunkLength; i++)
    {
        if (*(address++) != value)
            return false;
    }

    bytesCheckedAlready = chunkLength;

    //calculate num. of lwords  remaining
    chunkLength = (length - bytesCheckedAlready) / sizeof(uint32_t);

    //if > 4 lwords, use optimized path
    if (chunkLength > CHECKS_IN_FOR_LOOP)
    {
        address32 = (uint32_t *)address;
        //replicate byte 4 times within lword
        value32 = value | (value << 8);
        value32 |= value32 << 16;

        miscompare = false;
        loopBound = chunkLength / CHECKS_IN_FOR_LOOP;

        for (i = 0; i < loopBound; i++)
        {
            //cut down on loop overhead
            miscompare |= *(address32++) != value32;
            miscompare |= *(address32++) != value32;
            miscompare |= *(address32++) != value32;
            miscompare |= *(address32++) != value32;
            if (miscompare)
                return false;
        }

        bytesCheckedAlready += ( loopBound * CHECKS_IN_FOR_LOOP * sizeof(uint32_t) );
    }

    //calculate number of bytes remaining
    chunkLength = length - bytesCheckedAlready;
    address = (uint8_t *)address32;

    for (i = 0; i < chunkLength; i++)
    {
        if (*(address++) != value)
            return false;
    }

    return true;
}

//too many issues with too many compilers
//brought this fcn. into being
static uint8_t FlipBits(uint8_t input)
{
    return  (uint8_t) ( (~(uint32_t)input) & 0xFF );
}

// Flips status bits to active-1
static uint8_t InvertedStatusBits(uint8_t *headerAddress)
{
    uint8_t status;

    status = headerAddress[STATUS_OFFSET];
    status = FlipBits(status);
    
    return status;
}

// Returns true if all the bits in 'bits' are found in status byte
static bool VerifyStatusBitsSet(uint8_t *headerAddress, uint8_t bits)
{
    uint8_t status;

    status = InvertedStatusBits(headerAddress);

    if ((status & bits) != bits)
        return false;

    return true;
}

// Returns true if all bits in 'bits' are not set in status word
static bool VerifyStatusBitsNotSet(uint8_t *headerAddress, uint8_t bits)
{
    uint8_t status;

    status = InvertedStatusBits(headerAddress);

    if ((status & bits) != 0)
        return false;

    return true;
}

// Get pointer to the latest version of a tag
// Returns NULL if no latest version found yet.
// note...tag numbers start at 1 (TAGNUM_MIN)
static uint8_t *GetTagPtr(tag_space_t space, uint16_t tagNumber)
{
    uint32_t *tagPtrBase;
    uint16_t  maxTagNumber;

    //sanity check, all spaces
    if ( (tagNumber < TAGNUM_MIN) || (tagNumber > TAGNUM_MAX) )
        return NULL;

    tagPtrBase = nvm_GetTagPtrBase(space, &maxTagNumber);
    if (tagNumber > maxTagNumber)
        return NULL;

    return (uint8_t *)( tagPtrBase[tagNumber - 1] );
}

//set tag pointer at index 'tagNumber' to 'address'
static void SetTagPtr(tag_space_t space, uint16_t tagNumber, uint8_t *address)
{
    uint32_t *tagPtrBase;
    uint16_t  maxTagNumber;

    //sanity check, all spaces
    if ( (tagNumber < TAGNUM_MIN) || (tagNumber > TAGNUM_MAX) )
        return;

    tagPtrBase = nvm_GetTagPtrBase(space, &maxTagNumber);
    if (tagNumber > maxTagNumber)
        return;

    tagPtrBase[tagNumber - 1] = (uint32_t)address;
}

static void ClearAllTagPtrs(tag_space_t space)
{
    uint32_t *tagPtr;
    uint16_t  maxTagNumber;
    
    tagPtr = nvm_GetTagPtrBase(space, &maxTagNumber);

    rutils_memset(tagPtr, 0, sizeof(uint32_t) * maxTagNumber);
}

static uint8_t *GetSectorAddress(tag_space_t space, uint16_t sectorNumber)
{
    const space_desc_t *spaceDesc = nvm_GetSpaceDesc(space);

    if (sectorNumber > spaceDesc->numberOfSectors)
        return NULL;

    return ((uint8_t *)(spaceDesc->startAddress + (uint32_t)sectorNumber * spaceDesc->sectorLength));
}

bool GetSectorAddressAndPlusOne(tag_space_t space,
                                uint16_t    sectorNumber,
                                uint8_t   **sectorAddress,
                                uint8_t   **sectorEndAddressPlusOne)
{
    const space_desc_t *spaceDesc = nvm_GetSpaceDesc(space);

    if (sectorNumber > spaceDesc->numberOfSectors)
        return false;

    *sectorAddress = (uint8_t *)(spaceDesc->startAddress + (uint32_t)sectorNumber * spaceDesc->sectorLength);
    *sectorEndAddressPlusOne = *sectorAddress + spaceDesc->sectorLength;

    return true;
}

// Set 'sectorNumber' to sector that address falls within
// Return false if outside this space's range
static bool GetSectorNumberFromAddress(tag_space_t space,
                                       uint8_t    *address,
                                       uint16_t   *sectorNumber)
{
    const space_desc_t *desc = nvm_GetSpaceDesc(space);
    uint8_t *sectorAddress = (uint8_t *)(desc->startAddress);
    uint16_t numberOfSectors = desc->numberOfSectors;
    uint32_t sectorLength = desc->sectorLength;
    uint32_t offset;

    if (address < sectorAddress)
        return false;

    offset = (uint32_t)(address - sectorAddress);

    if (offset >= numberOfSectors * sectorLength)
        return false;

    *sectorNumber = offset / sectorLength;

    return true;
}

//Returns true if 'tagNumber' is within range, and 'space' is a legal space
static bool VerifyTagNumberIsWithinRange(tag_space_t space, uint16_t tagNumber)
{
    uint16_t  maxTags = 0;   //init to rid warning

    //zero is an illegal tag number
    if (tagNumber == 0)
        return false;

    (void)nvm_GetTagPtrBase(space, &maxTags);

    //'tagNumber' out of bounds?
    if (tagNumber > maxTags)
        return false;

    return true;
}

//  Sanity checks tag header for possible illegal bits written
//  At a minimum, tag header must've completed its write
//  Returns true if sane.
static bool BasicSanityCheckTagHeader(uint8_t *address)
{
    uint16_t tagNumber;
    uint16_t version;
    uint16_t length;

    //header aligned on lword adddress?
    if (ALIGN32(address) != (ptrdiff_t)address)
        return false;

    //1st byte is magic number?
    if (address[MAGIC_NUMBER_OFFSET] != MAGIC_NUMBER)
        return false;

    //reserved status bits never written?
    if (!VerifyStatusBitsNotSet(address, FlipBits(STATUS_ALL)))
        return false;

    //at a minimum, must have finished writing header
    if (!VerifyStatusBitsSet(address, STATUS_HEADER_WRITTEN))
        return false;

    //tag number sane?
    tagNumber = rutils_stream_to_word16(address + TAG_NUMBER_OFFSET);
    if ((tagNumber < TAGNUM_MIN) || (tagNumber > TAGNUM_MAX))
        return false;
        
    //version value sane?
    version = rutils_stream_to_word16(address + VERSION_OFFSET);
    if ((version < VERSION_MIN) || (version > VERSION_MAX))
        return false;

    //length value sane?
    length = rutils_stream_to_word16(address + LENGTH_OFFSET);
    if (length == LENGTH_OVERRANGE)
        return false;

    //reserved area not corrupted?
    if (!IsMemSetToValue(address + RESERVED_OFFSET, BYTE_NEVER_WRITTEN, RESERVED_SIZE))
        return false;

    return true;
}

// Sanity check tag, that it was completely written
// Return true if sane
static bool SanityCheckTagHeader(uint8_t *address)
{
    if (!BasicSanityCheckTagHeader(address))
        return false;

    if (!VerifyStatusBitsSet(address, STATUS_HEADER_WRITTEN | STATUS_DATA_WRITTEN))
        return false;
    
    return true;
}

// Assumes that tag header was never finished
// This infers that data was never written, not even partially
// this fcn. returns true if tag can be marked insane, i.e.
// no bits are set to zero that should never be
//
// This fcn. should only be called when a tag has been flagged 
// as corrupeted. Doesn't work for a sane tag!
static bool IsPartiallyWrittenHeaderCorrectable(uint8_t *address)
{
    uint8_t  magicNumberFlipped;
    uint16_t tagNumber;
    uint16_t version;

    //header aligned on lword adddress?
    if (ALIGN32(address) != (ptrdiff_t)address)
        return false;

    //no bits in magic number set to zero when they shouldn't be?
    magicNumberFlipped = FlipBits(address[MAGIC_NUMBER_OFFSET]);
    if ((magicNumberFlipped & MAGIC_NUMBER) != 0)
        return false;

    //reserved status bits never written?
    if (!VerifyStatusBitsNotSet(address, FlipBits(STATUS_ALL)))
        return false;

    //header written, data written NOT set?
    if (!VerifyStatusBitsNotSet(address, STATUS_HEADER_WRITTEN | STATUS_DATA_WRITTEN))
        return false;
    
    //can tag number be recovered?
    tagNumber = rutils_stream_to_word16(address + TAG_NUMBER_OFFSET);
    if (tagNumber < TAGNUM_MIN)
        return false;
        
    //can version be recovered?
    version = rutils_stream_to_word16(address + VERSION_OFFSET);
    if (version < VERSION_MIN)
        return false;

    //length is a don't-care

    //reserved area not corrupted?
    if (!IsMemSetToValue(address + RESERVED_OFFSET, BYTE_NEVER_WRITTEN, RESERVED_SIZE))
        return false;

    return true;
}


// Returns true if 'thisVersion' is unambiguously a later version
// than 'otherVersion'. Takes into consideration version wrapping.
static bool IsLatestVersion(uint16_t thisVersion, uint16_t otherVersion)
{
    bool   thisVersionSane;
    bool   otherVersionSane;
    bool   thisInWrapThreshold;
    bool   otherInWrapThreshold;
    bool   rc;

    thisVersionSane = (thisVersion >= VERSION_MIN) &&
                      (thisVersion < VERSION_MAX_SANE);

    otherVersionSane = (otherVersion >= VERSION_MIN) &&
                       (otherVersion < VERSION_MAX_SANE);

    if (thisVersionSane && !otherVersionSane)
        return true;
    else if (!thisVersionSane)
        return true;

    
    //Check if version is very close to wrappping back to one.
    //In this case, when comparing versions, you have to assume
    //that the lower version is the latest because it has wrapped
    //around already, and the higher one hasn't yet.
    thisInWrapThreshold = thisVersion > VERSION_WRAP_THRESHOLD;
    otherInWrapThreshold = otherVersion > VERSION_WRAP_THRESHOLD;

    if ((thisInWrapThreshold == otherInWrapThreshold)    &&
        (thisVersion > otherVersion))
    {
        rc = true;
    }
    else
    {
        //This is a rare corner-case
        rc = !thisInWrapThreshold && otherInWrapThreshold;
    }

    return rc;
}    

// See if existing tag at 'tagAddress' is in fact the
// most recent version of the tag. Check it against the
// latest as indicatedby the TagPtr
static bool IsTagLatestVersion(tag_space_t space, uint8_t *tagAddress)
{
    uint8_t    status;
    uint16_t   tagNumber;
    uint16_t   version, currentLatestVersion;
    uint8_t   *tagPtr;

    if (SanityCheckTagHeader(tagAddress))
    {
        status = InvertedStatusBits(tagAddress);
        if ((status & (STATUS_DIRTY | STATUS_INSANE)) == 0)
        {
            tagNumber = rutils_stream_to_word16(tagAddress + TAG_NUMBER_OFFSET);

            tagPtr = GetTagPtr(space, tagNumber);
            if ((tagPtr == NULL) || (tagPtr == tagAddress))
                return true;

            version = rutils_stream_to_word16(tagAddress + VERSION_OFFSET);
            currentLatestVersion = rutils_stream_to_word16(tagPtr + VERSION_OFFSET);
            if (IsLatestVersion(version, currentLatestVersion))
            {
                //unusual case...old tag not marked as dirty
                return true;
            }
        }
    }

    return false;
}

// Increment to next version number
// A 'currentVersion' means there is no version
static uint16_t IncrementVersion(uint16_t currentVersion)
{
    if (currentVersion == VERSION_MAX_SANE)
        return VERSION_MIN;
    else if (currentVersion == 0)
        return VERSION_MIN;

    return currentVersion + 1;
}

// Are these 12 bytes fresh (all 0xFF's)?
static bool FreshSpan_NoTagHeader(uint8_t *address)
{
    if (IsMemSetToValue(address, BYTE_NEVER_WRITTEN, RESERVED_SIZE))
        return true;

    return false;
}

// 'startingTagAddress' is address of header of current tag
// returns byte offset from 'startingTagAddress' to start of next tag.
static uint32_t OffsetToNextTag_SaneOnly(uint8_t *startingTagAddress)
{
    uint16_t length;
    uint32_t offset;

    length = rutils_stream_to_word16(startingTagAddress + LENGTH_OFFSET);

    offset = length + HEADER_SIZE;
    offset = ALIGNUP32(offset);

    return offset;
}

// Returns amount of free space in a fresh sector
// (one that has just been erased)
static uint32_t MaxSectorFreeSpace(tag_space_t space)
{
    const space_desc_t *spaceDesc = nvm_GetSpaceDesc(space);

    return (spaceDesc->sectorLength -
            2*SECTOR_RESERVED_SIZE - SECTOR_HEADROOM);
}

// Returns amount of free space in an entirely fresh tag space
static uint32_t MaxSpaceFreeSpace(tag_space_t space)
{
    const space_desc_t *spaceDesc = nvm_GetSpaceDesc(space);

    return MaxSectorFreeSpace(space) * spaceDesc->numberOfSectors;
}

// Calculate tag space's free and unclean byte usage statistics
//
//   Output parms (if set to NULL, won't set):
//      spaceStats->freeSpace
//      spaceStats->totalCleanBytes
//      spaceStats->totalUncleanBytes
//      spaceStats->maxUncleanTagSectorNumber
//                      sector number of sector with most unclean
//                      bytes. Set to INVALID_UINT16 if none of the
//                      sectors have any unclean bytes.
//
//      maxUncleanTagBytes-- number of unclean bytes in 'maxUncleanTagSector' sector
//
static void AvailableRoomStats(tag_space_t    space,
                               space_stats_t *spaceStats,
                               uint32_t      *maxUncleanTagBytes)
{
    const space_desc_t *spaceDesc = nvm_GetSpaceDesc(space);
    sector_stats_t     *statsBase = nvm_GetSectorStatsBase(space);
    sector_stats_t     *stats;
    uint32_t            uncleanTagBytes = 0;
    uint16_t            sectorNumber = INVALID_UINT16;
    uint16_t            i;

    spaceStats->freeSpace = 0;
    spaceStats->totalCleanBytes = 0;
    spaceStats->totalUncleanBytes = 0;

    for (i = 0; i < spaceDesc->numberOfSectors; i++)
    {
        stats = &statsBase[i];

        if (stats->uncleanTagBytes > uncleanTagBytes)
        {
            uncleanTagBytes = stats->uncleanTagBytes;
            sectorNumber = i;
        }

        spaceStats->freeSpace += stats->freeSpaceBytes;
        spaceStats->totalCleanBytes += stats->cleanTagBytes;
        spaceStats->totalUncleanBytes += stats->uncleanTagBytes;
    }

    spaceStats->maxUncleanTagSectorNumber = sectorNumber;

    if (maxUncleanTagBytes != NULL)
        *maxUncleanTagBytes = uncleanTagBytes;
}

// remaining usable tag space (not including header size) in this sector
// 'startingTagAddress' is the last tag in this sector
// 'lastAddressPlusOne' defines the sector end
// returns INVALID if no space is available or sanity failure
static int RemainingSpaceAfterThisTag(uint8_t *startingTagAddress,
                                      uint8_t *lastAddressPlusOne)
{
    uint32_t            offset;
    uint8_t            *nextDataAddress;

    if (!SanityCheckTagHeader(startingTagAddress))
        return INVALID;

    offset = OffsetToNextTag_SaneOnly(startingTagAddress);

    lastAddressPlusOne -= SECTOR_RESERVED_SIZE + SECTOR_HEADROOM;
    nextDataAddress = startingTagAddress + offset + HEADER_SIZE;

    if (nextDataAddress >= lastAddressPlusOne)
        return INVALID;

    return (uint32_t)(lastAddressPlusOne - nextDataAddress - 1);
}

// step through a part of a sector and verify sanity of sector
// start address must be lword aligned
// tags with incomplete headers don't violate sanity of sector
//
// Return values:
//  SECTOR_SANITY_FAILED
//  SECTOR_SANITY_RECOVERABLE
//  SECTOR_SANITY_SANE
//
// tagAddressToRecover-- if SECTOR_SANITY_RECOVERABLE, sets this to
//             tag address
static sector_sanity_t SanityCheckTagLayoutInSector(uint8_t  *startAddress,
                                                  uint8_t  *lastAddressPlusOne,
                                                  uint8_t **tagAddressToRecover)
{
    bool    freshToEnd;
    bool    saneTagDetected;
    bool    basicSaneTagDetected;
    uint32_t  lengthToEnd;
    uint32_t  nextTagOffset;
    uint8_t  *currentAddress = startAddress;

    if (tagAddressToRecover != NULL)
        *tagAddressToRecover = NULL;

    while (currentAddress < lastAddressPlusOne)
    {
        //must always start on lword aligned address
        if (ALIGN32(currentAddress) != (ptrdiff_t)currentAddress)
            return SECTOR_SANITY_FAILED;

        //have we reached the end of where the sector has tags filled in it?
        if (FreshSpan_NoTagHeader(currentAddress))
        {
            lengthToEnd = (uint32_t)(lastAddressPlusOne - currentAddress);

            //must be fresh from here to end of sector
            freshToEnd = IsMemSetToValue(currentAddress, BYTE_NEVER_WRITTEN, lengthToEnd);
            if (!freshToEnd)
                return SECTOR_SANITY_FAILED;

            //we reached the end, and all is good
            return SECTOR_SANITY_SANE;
        }

        saneTagDetected = SanityCheckTagHeader(currentAddress);
        basicSaneTagDetected = BasicSanityCheckTagHeader(currentAddress);

        //make sure tag's length is sane
        if (saneTagDetected || basicSaneTagDetected)
        {
            nextTagOffset = OffsetToNextTag_SaneOnly(currentAddress);

            //does this tag length indicate tag exceeding upper boundary?
            if (currentAddress + nextTagOffset > lastAddressPlusOne) 
                return SECTOR_SANITY_FAILED;

            // latch tag's address while it still points to problem tag,
            // in case this falls into below header-only-written 'if'
            if (tagAddressToRecover != NULL)
                *tagAddressToRecover = currentAddress;

            currentAddress += nextTagOffset;

            //case: only header written, powered off before data was
            if (!saneTagDetected && basicSaneTagDetected)
            {
                //this must be last tag in sector, so
                //must be fresh from end of tag to end of sector
                lengthToEnd = (uint32_t)(lastAddressPlusOne - currentAddress);

                freshToEnd = IsMemSetToValue(currentAddress, BYTE_NEVER_WRITTEN, lengthToEnd);
                if (!freshToEnd)
                    return SECTOR_SANITY_FAILED;

                return SECTOR_SANITY_RECOVERABLE;
            }
        }
        //case: only part of the header written, power off before all of header written
        else
        {
            if (!IsPartiallyWrittenHeaderCorrectable(currentAddress))
                return SECTOR_SANITY_FAILED;

            // latch tag's address while it still points to problem tag
            if (tagAddressToRecover != NULL)
                *tagAddressToRecover = currentAddress;

            //this must be last tag in sector. tag's data could've been written, so it
            //must be fresh from after this tag's header to end
            currentAddress += HEADER_SIZE;

            lengthToEnd = (uint32_t)(lastAddressPlusOne - currentAddress);

            freshToEnd = IsMemSetToValue(currentAddress, BYTE_NEVER_WRITTEN, lengthToEnd);
            if (!freshToEnd)
                return SECTOR_SANITY_FAILED;

            return SECTOR_SANITY_RECOVERABLE;
        }
    }

    return SECTOR_SANITY_SANE;
}

// Find address of last tag, sane or not, from the point
// 'startAddress' in this sector.
// Returns NULL if no tags in sector.
//
// This fcn. assumes a sane sector
static uint8_t *LastTagInSector(uint8_t *startAddress, uint8_t *lastAddressPlusOne)
{
    bool    saneTagDetected;
    uint8_t  *currentAddress;
    uint8_t  *lastTagAddress;

    currentAddress = startAddress;
    lastTagAddress = NULL;

    while (currentAddress < lastAddressPlusOne)
    {
        //have we reached the end of where the sector has tags filled in it?
        if (FreshSpan_NoTagHeader(currentAddress))
            return lastTagAddress;

        saneTagDetected = SanityCheckTagHeader(currentAddress);
        if (saneTagDetected)
        {
            lastTagAddress = currentAddress;

            currentAddress += OffsetToNextTag_SaneOnly(currentAddress);
        }
        //case where tag isn't sane (1/2 written tag)
        else
        {
            return lastTagAddress;
        }
    }

    return lastTagAddress;
}

// number of bytes consumed by this tag
// includes header, data, and unaligned bytes at end
static uint32_t TagByteConsumption(uint16_t length)
{
    return ALIGNUP32(HEADER_SIZE + (ptrdiff_t)length);
}

// Verify that this attempted modication to flash would only change
// 1's to 0's and not 0's to 1's
bool IsFlashModifyLegit(uint8_t *data, uint32_t dataLength, uint8_t *address)
{
    uint32_t  i;
    uint8_t   datum;
    uint8_t   targetDatum;

    for (i = 0; i < dataLength; i++)
    {
        datum = (uint32_t)data[i];

        targetDatum = (uint32_t)address[i] | (uint32_t)0xFFFFFF00;

        if ( (datum & FlipBits(targetDatum)) != 0)
            return false;
    }

    return true;
}

// Modify 'data' so that if any bits in 'targetAddress' which are zeros
// and are ones in 'data', then they'll be set to zeros in data.
void MergeDataWithExisting(uint8_t *data, uint32_t dataLength, uint8_t *flashAddress)
{
    uint32_t i;
    uint8_t  flashValue, flippedFlashValue;
    uint8_t  writeValue, flippedWriteValue;
    uint8_t  finalValue;

    for (i = 0; i < dataLength; i++)
    {
        flashValue = flashAddress[i];
        flippedFlashValue = FlipBits(flashValue);
        writeValue = data[i];
        flippedWriteValue = FlipBits(writeValue);

        finalValue = flippedFlashValue | flippedWriteValue;
        finalValue = FlipBits(finalValue);

        data[i] = finalValue;
    }
}

// random write to flash
static void WriteToFlash(uint8_t *data, uint32_t dataLength, uint8_t *address)
{
        //****** hypothetical h/w driver

        nvm_low_level_status_t status;

        //interrupts are disabled during this call, so be careful with
        //long writes
        status = nvm_low_level_flash_write(address, data, dataLength);

        if (status != NVM_LOW_LEVEL_SUCCESS)
        {
            //try resetting the flash...
            nvm_low_level_flash_hardware_reset();
            NVMflashResets++;

            status = nvm_low_level_flash_write(address, data, dataLength);

            if (status != NVM_LOW_LEVEL_SUCCESS)
            {
                //can't write? game's over
                nvm_register_fatal_error(REASON_2ND_WRITE_FAILED);
            }
        }
}

// Write to flash. Assumes target location on flash is fresh
static void WriteToFreshFlash(uint8_t *data, uint32_t dataLength, uint8_t *address)
{
    if (!IsMemSetToValue(address, BYTE_NEVER_WRITTEN, dataLength))
    {
        nvm_register_fatal_error(REASON_WRITE_TO_UNFRESH_FLASH);
    }

    WriteToFlash(data, dataLength, address);
}

// Does a read-modify-write to flash
static void WriteModifyingFlash(uint8_t *data, uint32_t dataLength, uint8_t *address)
{
    if (!IsFlashModifyLegit(data, dataLength, address))
    {
        nvm_register_fatal_error(REASON_0_TO_1_WRITE_ATTEMPT);
    }

    MergeDataWithExisting(data, dataLength, address);

    WriteToFlash(data, dataLength, address);
}

// Write tag. assume sane parms.
static void WriteTag(uint16_t tagNumber,
                     uint16_t version, 
                     uint16_t length,
                     uint8_t *data,
                     uint8_t *address)
{
    uint8_t  tagHeader[HEADER_SIZE];
    uint8_t  status;

    //fill in header
    rutils_memset(tagHeader, BYTE_NEVER_WRITTEN, HEADER_SIZE);
    tagHeader[MAGIC_NUMBER_OFFSET] = MAGIC_NUMBER;
    tagHeader[STATUS_OFFSET] = BYTE_NEVER_WRITTEN;
    rutils_word16_to_stream(tagHeader + TAG_NUMBER_OFFSET, tagNumber);
    rutils_word16_to_stream(tagHeader + VERSION_OFFSET, version);
    rutils_word16_to_stream(tagHeader + LENGTH_OFFSET, length);

    //write initial header...no status bits yet
    WriteToFreshFlash(tagHeader, HEADER_SIZE, address);

    //update status bit indicating that header has been written
    status = FlipBits(STATUS_HEADER_WRITTEN);
    WriteToFreshFlash(&status, sizeof(status), address + STATUS_OFFSET); 

    //write data
    WriteToFreshFlash(data, length, address + HEADER_SIZE);

    //update status bit indicating that data has been written
    status = FlipBits(status);
    status |= STATUS_DATA_WRITTEN;
    status = FlipBits(status);
    WriteModifyingFlash(&status, sizeof(status), address + STATUS_OFFSET); 
}

static bool SanityCheckTagWriteParms(uint16_t tagNumber,
                                     uint16_t version, 
                                     uint16_t length,
                                     uint8_t *address)
{
    uint16_t  totalBytesConsumed;

    if ( ALIGN32(address) != (ptrdiff_t)address)
        return false;

    if ( (tagNumber < TAGNUM_MIN) || (tagNumber > TAGNUM_MAX) )
        return false;

    if ( (version < VERSION_MIN) || (version > VERSION_MAX) )
        return false;

    if (length == LENGTH_OVERRANGE)
        return false;

    totalBytesConsumed = TagByteConsumption(length);

    //be sure target flash area is completely fresh
    if (IsMemSetToValue(address, BYTE_NEVER_WRITTEN, totalBytesConsumed))
        return true;

    return false;
}

// 'address' -- address in flash where tag is to be written, starting
//              with header
// 'endOfSectorPlusOne' -- usable end of sector...make sure tag write
//                         doesn't exceed this address
void WriteTagWithSanityChecks(uint16_t tagNumber,
                              uint16_t version, 
                              uint16_t length,
                              uint8_t *data,
                              uint8_t *address,
                              uint8_t *endOfSectorPlusOne)
{
    uint16_t consumption;

    consumption = TagByteConsumption(length);

    //final check...write must be within same sector
    if (address + consumption > endOfSectorPlusOne)
    {
        nvm_register_fatal_error(REASON_OVERRUN_SECTOR_WHILE_WRITING);
    }

    //final check...write must create a sane tag
    if ( !SanityCheckTagWriteParms(tagNumber, version, length, address) )
    {
        nvm_register_fatal_error(REASON_FAILED_VERIFY_OF_HEADER_WRITE);
    }

    WriteTag(tagNumber, version, length, data, address);
}

// Write a tag to specified sector 
// If there is a previous version, mark it as obsolete
// Update vitals, tagPtr, statistics
static void WriteTagToThisSector(tag_space_t space,
                                 uint16_t    tagNumber,
                                 uint16_t    sectorNumber,
                                 uint8_t    *data,
                                 uint16_t    dataLength)
{
    space_vitals_t  *spaceVitals = nvm_GetSpaceVitals(space);
    sector_stats_t  *statsBase = nvm_GetSectorStatsBase(space);
    sector_stats_t  *stats = &statsBase[sectorNumber];
    sector_stats_t  *oldStats;
    sector_vitals_t *vitalsBase = nvm_GetSectorVitalsBase(space);
    sector_vitals_t *vitals = vitalsBase + sectorNumber;
    uint8_t  *sectorAddress = NULL;
    uint8_t  *sectorEndAddressPlusOne = NULL;
    uint8_t  *lastTagAddress;
    uint8_t  *newTagAddress;
    uint8_t  *tagPtr;
    uint16_t  oldSectorNumber;
    uint16_t  bytesConsumed;
    uint16_t  oldBytesConsumed;
    uint16_t  currentVersion;
    uint16_t  newVersion;
    uint8_t   status;
    bool    rc;
    int     remainingSpace;

    rc = GetSectorAddressAndPlusOne(space, sectorNumber, &sectorAddress,
                                    &sectorEndAddressPlusOne);
    if (!rc)
    {
        //basic bad parm
        nvm_register_fatal_error(REASON_SECTORNUM_OVERRUN);
    }

    //it's quicker to use 'vitals->lastTagAddress' rather than to call 'LastTagInSector'
    lastTagAddress = vitals->lastTagAddress;

    if (lastTagAddress != NULL)
    {
        if ( (lastTagAddress < sectorAddress + SECTOR_RESERVED_SIZE) ||
             (lastTagAddress + HEADER_SIZE + SECTOR_RESERVED_SIZE >=
                                                    sectorEndAddressPlusOne) )
        {
            //vitals corrupted
            nvm_register_fatal_error(REASON_BAD_LAST_TAG_ADDRESS);
            return;
        }

        remainingSpace = RemainingSpaceAfterThisTag(lastTagAddress,
                                                    sectorEndAddressPlusOne);
        if ((remainingSpace < dataLength) && (remainingSpace == INVALID))
        {
            //available space sanity fail
            nvm_register_fatal_error(REASON_AVAILABLE_SPACE_SANITY_ERROR);
            return;
        }

        newTagAddress = OffsetToNextTag_SaneOnly(lastTagAddress) + lastTagAddress;
    }
    else
    {
        //fresh sector. write tag to lowest address.
        newTagAddress = sectorAddress + SECTOR_RESERVED_SIZE;
    }

    tagPtr = GetTagPtr(space, tagNumber);
    if (tagPtr != NULL)
    {
        currentVersion = rutils_stream_to_word16(tagPtr + VERSION_OFFSET);
    }
    else
    {
        currentVersion = VERSION_MIN - 1;
    }

    newVersion = IncrementVersion(currentVersion);

    rc = SanityCheckTagWriteParms(tagNumber, newVersion, dataLength, newTagAddress);
    if (!rc)
    {
        //should not fail sanity check
        nvm_register_fatal_error(REASON_WRITE_PARMS_SANITY_CHECK);
    }

    WriteTagWithSanityChecks(tagNumber, newVersion, dataLength,
                             data, newTagAddress, sectorEndAddressPlusOne);

    SetTagPtr(space, tagNumber, newTagAddress);

    // double check that tag we just wrote is sane
    if ( !SanityCheckTagHeader(newTagAddress) )
    {
            nvm_register_fatal_error(REASON_WRITE_SANITY_CHECK);
            return;
    }

    vitals->lastTagAddress = newTagAddress;
    spaceVitals->currentWriteSector = sectorNumber;

    bytesConsumed = TagByteConsumption(dataLength);
    stats->numCleanTags++;
    stats->cleanTagBytes += bytesConsumed;
    stats->freeSpaceBytes -= bytesConsumed;

    //mark previous version of tag as obsolete
    if (tagPtr != NULL)
    {
        oldBytesConsumed = rutils_stream_to_word16(tagPtr + LENGTH_OFFSET);
        oldBytesConsumed = TagByteConsumption(oldBytesConsumed);

        status = FlipBits(tagPtr[STATUS_OFFSET]);
        status |= STATUS_DIRTY;
        status = FlipBits(status);

        WriteModifyingFlash(&status, sizeof(status), tagPtr + STATUS_OFFSET);

        rc = GetSectorNumberFromAddress(space, tagPtr, &oldSectorNumber);
        if (!rc)
        {
            nvm_register_fatal_error(REASON_PAST_SECTOR_NUMBER_INVALID);
            return;
        }

        oldStats = nvm_GetSectorStatsBase(space) + oldSectorNumber;
        oldStats->numDirtyTags++;
        oldStats->numCleanTags--;
        oldStats->uncleanTagBytes += oldBytesConsumed;
        oldStats->cleanTagBytes -= oldBytesConsumed;
    }
}

// Calculate new header for a partially-written tag whose
// header is at 'address'
// Returns false if tag header is beyond repair
bool NewHeaderForInsaneTag(uint8_t *header, uint8_t *address)
{
    uint8_t  status;
    uint16_t value16;
    uint16_t tagNumber;
    uint16_t version;
    uint16_t length;

    //sanity check lword alignment of address
    if ( ALIGN32(address) != (ptrdiff_t)address)
        return false;

    // what's currently in flash
    status = InvertedStatusBits(address);
    UNUSED(status);                                     //rid compiler warning
    tagNumber = rutils_stream_to_word16(address + TAG_NUMBER_OFFSET);
    version = rutils_stream_to_word16(address + VERSION_OFFSET);
    length = rutils_stream_to_word16(address + LENGTH_OFFSET);
    rutils_memset(header, BYTE_NEVER_WRITTEN, HEADER_SIZE);

    // what we want to write there
    header[MAGIC_NUMBER_OFFSET] = MAGIC_NUMBER;
    header[STATUS_OFFSET] = FlipBits(STATUS_INSANE | STATUS_DIRTY);

    if (tagNumber == WORD_NEVER_WRITTEN)
    {
        value16 = TAGNUM_INSANE;
    }
    else
    {
        value16 = tagNumber;
    }
    rutils_word16_to_stream(header + TAG_NUMBER_OFFSET, value16);

    if (version == WORD_NEVER_WRITTEN)
    {
        value16 = VERSION_INSANE;
    }
    else
    {
        value16 = version;
    }
    rutils_word16_to_stream(header + VERSION_OFFSET, value16);

    if (length == WORD_NEVER_WRITTEN)
    {
        rutils_word16_to_stream(header + LENGTH_OFFSET, 0);
    }

    // shouldn't be necessary...but just in case
    MergeDataWithExisting(header, HEADER_SIZE, address);

    return true;
}

// assuming 'address' points to tag that whose write was interrupted
// at powerdown, calculate values to set header to and write,
// marking tag as insane.
bool MarkPartiallyWrittenTag(uint8_t *address)
{
    uint8_t  header[HEADER_SIZE];
    uint8_t  status;

    //determine how to mark up header
    if ( !NewHeaderForInsaneTag(header, address) )
        return false;

    //write header
    WriteModifyingFlash(header, HEADER_SIZE, address);

    //finalize status: in addition to 'header written',
    //   set 'data written' just to close out tag
    status = FlipBits(header[STATUS_OFFSET]);
    status |= STATUS_HEADER_WRITTEN | STATUS_DATA_WRITTEN;
    status = FlipBits(status);

    WriteModifyingFlash(&status, sizeof(status), address + STATUS_OFFSET);

    // this shouldn't fail...but just in case
    if (!BasicSanityCheckTagHeader(address))
        return false;

    return true;
}

// Go into sector, and if there's a partially written
// tag, close it.
void FixSectorIfNecessary(uint8_t *sectorAddress,
                          uint8_t *endAddressPlusOne)
{
    uint8_t        *beginAddress;
    uint8_t        *finishAddress;
    uint8_t        *problemTagAddress = NULL;   //init to rid warning
    sector_sanity_t sanity;

    beginAddress = sectorAddress + SECTOR_RESERVED_SIZE;
    finishAddress = endAddressPlusOne - SECTOR_RESERVED_SIZE;

    //skip if sector is completely fresh
    if (IsMemSetToValue(beginAddress, BYTE_NEVER_WRITTEN,
                        (uint32_t)(finishAddress - beginAddress)))
    {
        return;
    }

    sanity = SanityCheckTagLayoutInSector(beginAddress, finishAddress,
                                          &problemTagAddress);

    if (sanity != SECTOR_SANITY_SANE)
    {
        //the flash chip's state machine might be hung, causing read failures.
        nvm_low_level_flash_hardware_reset();
        NVMflashResets++;

        sanity = SanityCheckTagLayoutInSector(beginAddress, finishAddress,
                                              &problemTagAddress);
    }

    if (sanity == SECTOR_SANITY_RECOVERABLE)
    {
        if ( !MarkPartiallyWrittenTag(problemTagAddress) )
        {
            //sector is trashed :(
            nvm_register_fatal_error(REASON_CANT_FIX_PARTIAL_TAG);
        }

        //must pass this 2nd time around
        sanity = SanityCheckTagLayoutInSector(beginAddress, finishAddress,
                                              &problemTagAddress);
        if (sanity != SECTOR_SANITY_SANE)
        {
            nvm_register_fatal_error(REASON_TAG_FIX_FAILED);
        }
    }
    else if (sanity == SECTOR_SANITY_FAILED)
    {
        nvm_register_fatal_error(REASON_SECTOR_UNFIXABLE);
    }
}


// Scores stats for this sector
// Assumes a sane sector
static void CalculateSectorStats(uint8_t *sectorAddress,
                                 uint8_t *sectorEndAddressPlusOne,
                                 sector_stats_t *stats)
{
    uint8_t  *currentAddress = sectorAddress + SECTOR_RESERVED_SIZE;
    uint8_t  *lastAddress = sectorEndAddressPlusOne - SECTOR_RESERVED_SIZE;
    uint8_t  *nextAddress;
    uint32_t  freeSpace;
    uint8_t   status;
    uint16_t  length;
    uint16_t  bytesConsumed;
    bool    insane;
    bool    dirty;
    bool    clean;

    rutils_memset(stats, 0, sizeof(sector_stats_t));

    //in case last tag erroneously ran into the headroom, don't include
    //headroom in 'lastAddress', but take headroom out of 'freeSpace'
    freeSpace = (uint32_t)(lastAddress - currentAddress - SECTOR_HEADROOM);

    //walk all tags from low to high, stop when unused space encountered
    while ((currentAddress < lastAddress) &&
           !FreshSpan_NoTagHeader(currentAddress))
    {
        status = FlipBits(currentAddress[STATUS_OFFSET]);
        length = rutils_stream_to_word16(currentAddress + LENGTH_OFFSET);
        bytesConsumed = TagByteConsumption(length);

        insane = (status & STATUS_INSANE) != 0;
        dirty = (status & STATUS_DIRTY) != 0;
        clean = (status & STATUS_DATA_WRITTEN) && !(insane || dirty);

        //good tag, still active
        if (clean)
        {
            stats->numCleanTags += 1;
            stats->cleanTagBytes += bytesConsumed;
        }
        //tag not written completely from last powerdown
        else if (insane)
        {
            stats->numInsaneTags += 1;
            stats->uncleanTagBytes += bytesConsumed;
        }
        //dirty tag...marked obsolete
        else
        {
            stats->numDirtyTags += 1;
            stats->uncleanTagBytes += bytesConsumed;
        }

        if (freeSpace >= bytesConsumed)
        {
            freeSpace -= bytesConsumed;
        }
        //this case is when tag has infringed into headroom.
        //should never hit.
        else
        {
            freeSpace = 0;
        }

        nextAddress = OffsetToNextTag_SaneOnly(currentAddress) + currentAddress;
        if (nextAddress <= currentAddress)
            break;          //safety mechanism...should never get here!!!

        currentAddress = nextAddress;
    }

    stats->freeSpaceBytes = freeSpace;
}

// Update the Tag Pointers from all the clean tags in a sector.
// Will only update a tag pointer if the candidate tag is 
static void UpdateTagPtrsFromSector(tag_space_t space,
                                    uint8_t *sectorAddress,
                                    uint8_t *sectorEndAddressPlusOne)
{
    uint8_t  *currentAddress = sectorAddress + SECTOR_RESERVED_SIZE;
    uint8_t  *finishAddress = sectorEndAddressPlusOne - SECTOR_RESERVED_SIZE;
    uint8_t  *lastTagPtr;
    uint8_t  *obsoleteTagAddress;
    uint8_t   status;
    uint8_t   bits;
    bool    dirty;
    bool    finishedWrite;
    uint16_t  version;
    uint16_t  lastVersion;
    uint16_t  tagNumber;

    //leapfrog from tag to tag, from bottom to where sector is unused
    while ((currentAddress < finishAddress) &&
           !FreshSpan_NoTagHeader(currentAddress))
    {
        if (!BasicSanityCheckTagHeader(currentAddress))
            break;              //shouldn't get here...tag should already by sane

        status = FlipBits(currentAddress[STATUS_OFFSET]);
        tagNumber = rutils_stream_to_word16(currentAddress + TAG_NUMBER_OFFSET);
        version = rutils_stream_to_word16(currentAddress + VERSION_OFFSET);

        //compare to latest Tag Pointer, update if this is latest, skip if not
        bits = STATUS_HEADER_WRITTEN | STATUS_DATA_WRITTEN;
        finishedWrite = (status & bits) == bits;
        dirty = (status & (STATUS_DIRTY | STATUS_INSANE)) != 0;

        //is this tag clean?
        if (finishedWrite && !dirty && (tagNumber != TAGNUM_INSANE))
        {
            lastTagPtr = GetTagPtr(space, tagNumber);

            //is there an existing entry for this tag?
            // (this is actually rare...it can only happen if a power outage happened
            //  when the new tag was written but the old version version of the tag
            //  hadn't yet been marked dirty).
            if (lastTagPtr != NULL)
            {
                lastVersion = rutils_stream_to_word16(lastTagPtr + VERSION_OFFSET);

                //is the current tag being examined a later version than
                //one that was already found?
                if (IsLatestVersion(version, lastVersion))
                {
                    SetTagPtr(space, tagNumber, currentAddress);

                    //the previously found tag is the obsolete one
                    obsoleteTagAddress = lastTagPtr;
                }
                else
                {
                    //the current tag being examined is the obsolete one
                    obsoleteTagAddress = currentAddress;
                }

                //whichever of the 2 tags was found to be at a lower version,
                //mark that tag as obsolete, so it will never get confused with the
                //latest version.
                status = FlipBits(obsoleteTagAddress[STATUS_OFFSET]);
                status |= STATUS_DIRTY;
                status = FlipBits(status);

                WriteModifyingFlash(&status, sizeof(status), obsoleteTagAddress + STATUS_OFFSET);
            }
            //normal case: found tag and are storing version for it
            else
            {
                SetTagPtr(space, tagNumber, currentAddress);
            }
        }

        //advance to next tag
        currentAddress = OffsetToNextTag_SaneOnly(currentAddress) + currentAddress;
    }
}

// Check sector for sanity, fix if not sane
// Returns false if sector not sane and couldn't fix
static void SectorSurvey(uint8_t     *sectorAddress,
                         uint8_t     *sectorEndAddressPlusOne)
{
    uint8_t *startAddress = sectorAddress + SECTOR_RESERVED_SIZE;
    uint8_t *finishAddress = sectorEndAddressPlusOne - SECTOR_RESERVED_SIZE;
    sector_sanity_t sanity;

    sanity = SanityCheckTagLayoutInSector(startAddress, finishAddress, NULL);

    //make sure sector is sane, abort if it isn't
    if (sanity != SECTOR_SANITY_SANE)
    {
        FixSectorIfNecessary(sectorAddress, sectorEndAddressPlusOne);
    }
}

// Called when a background erasure has completed.
// This updates stats and clears erase state
static void SectorEraseCompletion(tag_space_t space)
{
    const space_desc_t *spaceDesc = nvm_GetSpaceDesc(space);
    space_vitals_t     *spaceVitals = nvm_GetSpaceVitals(space);
    sector_vitals_t    *vitals;
    sector_stats_t     *stats;
    uint16_t            sectorNumber;
    uint8_t            *sectorAddress;

    sectorNumber = spaceVitals->sectorErasing;

    //are we erasing a sector now?
    if (sectorNumber != INVALID_UINT16)
    {
        sectorAddress = GetSectorAddress(space, sectorNumber);

        vitals = nvm_GetSectorVitalsBase(space);
        vitals = &vitals[sectorNumber];

        //erased results verified?
        if (IsMemSetToValue(sectorAddress, BYTE_NEVER_WRITTEN, spaceDesc->sectorLength))
        {
            //init sector vitals
            vitals->lastTagAddress = NULL;

            stats = nvm_GetSectorStatsBase(space);
            stats = &stats[sectorNumber];

            //now that sector has been erased, initialize sector stats
            CalculateSectorStats(sectorAddress,
                                 sectorAddress + spaceDesc->sectorLength, stats);

            //clear sector erase in progress indication
            //do this last, since it's being called from a lower priority task
            spaceVitals->sectorErasing = INVALID_UINT16;
        }
        else
        {
            //fatal error...erasure verification failed
            nvm_register_fatal_error(REASON_ERASE_VERIFY_FAIL);
        }
    }
}

// Do a blocking sector erase
static void ForegroundSectorErase(tag_space_t     space,
                                  uint16_t        sectorNumber,
                                  uint8_t        *sectorAddress,
                                  uint8_t        *sectorEndAddressPlusOne,
                                  space_vitals_t *spaceVitals)
{
    UNUSED(sectorAddress);
    UNUSED(sectorEndAddressPlusOne);
    UNUSED(spaceVitals);

    (void)nvm_low_level_flash_erase(space, sectorNumber);
}

// Queue up erase to sector eraser task.
// CURRENTLY NOT USED!!
/*static*/ void BackgroundSectorErase(tag_space_t     space,
                                  uint16_t        sectorNumber,
                                  uint8_t        *sectorAddress,
                                  uint8_t        *sectorEndAddressPlusOne,
                                  space_vitals_t *spaceVitals)
{
    UNUSED(space);
    UNUSED(sectorNumber);
    UNUSED(sectorAddress);
    UNUSED(sectorEndAddressPlusOne);
    UNUSED(spaceVitals);

    // TBD
}

// Select a sector to write a tag of lenght 'bytesNeeded'
// Chose algorithm via 'method'
//      SELECT_FULLEST: select sector which is fullest, but still has
//              room to write this tag
//      SELECT_LAST_AND_INCREMENT: if last sector write occured in has
//              room, use it; otherwise, increment sector number until
//              a sector with enough spare space is found. Use first
//              one found.
//      SELECT_NEXT_AND_INCREMENT: starting with next sector after current
//              write sector, use SELECT_LAST_AND_INCREMENT algorithm. Error
//              if selected sector is current write sector.
// Returns false if all sectors are eith full or busy
// otherwise, writes sector number into 'sectorNumber'
static bool SelectWriteSector(tag_space_t    space,
                              uint16_t       bytesOfDataNeeded,
                              write_select_t method,
                              uint16_t      *sectorNumber)
{
    const space_desc_t *desc = nvm_GetSpaceDesc(space);
    space_vitals_t  *spaceVitals = nvm_GetSpaceVitals(space);
    sector_vitals_t *vitalsBase = nvm_GetSectorVitalsBase(space);
    sector_vitals_t *vitals;
    uint8_t         *sectorAddress;
    uint8_t         *lastTagAddress;
    int            minBytesRemaining;
    int            chosenSector = INVALID;
    int            bytesRemaining;
    uint32_t         sectorLength;
    uint16_t         numberOfSectors;
    uint16_t         sectorErasing;
    uint16_t         sectorAbandoning;
    uint16_t         lastWriteSector;
    uint16_t         thisSector;
    uint16_t         i;

    numberOfSectors = desc->numberOfSectors;
    sectorLength = desc->sectorLength;

    sectorErasing = spaceVitals->sectorErasing;
    sectorAbandoning = spaceVitals->sectorAbandoning;

    lastWriteSector = spaceVitals->currentWriteSector;

    // first method:
    //  find the fullest sector and write into that one
    //  by default it'll chose sector 0
    if (method == SELECT_FULLEST)
    {
        minBytesRemaining = INVALID;
        for (i = 0; i < numberOfSectors; i++)
        {
            sectorAddress = GetSectorAddress(space, i);

            vitals = &vitalsBase[i];

            lastTagAddress = vitals->lastTagAddress;

            //sector currently being reclaimed?
            if ((i == sectorErasing) || (i == sectorAbandoning))
            {
                bytesRemaining = INVALID;
            }
            //sector fresh?
            else if (lastTagAddress == NULL)
            {
                bytesRemaining = MaxSectorFreeSpace(space);
            }
            else
            {
                bytesRemaining = RemainingSpaceAfterThisTag(lastTagAddress,
                                  sectorAddress + sectorLength);
            }

            if ((bytesOfDataNeeded <= bytesRemaining)
                          &&
                (bytesRemaining != INVALID)
                          &&
                ( (minBytesRemaining == INVALID)   ||
                  (bytesRemaining < minBytesRemaining) ) )
            {
                minBytesRemaining = bytesRemaining;
                chosenSector = i;
            }
        }
    }

    // Second method:
    //  If last designated write sector has enough space, then use that
    //  otherwise, increment to next sector, check if that's available,
    //  and use that.
    else if ((method == SELECT_LAST_AND_INCREMENT) ||
             (method == SELECT_NEXT_AND_INCREMENT))
    {
        thisSector = spaceVitals->currentWriteSector;

        //reject current sector for 'next' mode
        if (method == SELECT_NEXT_AND_INCREMENT)
        {
            thisSector = WRAP(thisSector + 1, numberOfSectors);
        }
        
        for (i = 0; i < numberOfSectors; i++)
        {
            sectorAddress = GetSectorAddress(space, thisSector);

            vitals = &vitalsBase[thisSector];

            lastTagAddress = vitals->lastTagAddress;

            //sector currently being reclaimed?
            if ((thisSector == sectorErasing) || (thisSector == sectorAbandoning))
            {
                bytesRemaining = INVALID;
            }
            //sector fresh?
            else if (lastTagAddress == NULL)
            {
                bytesRemaining = MaxSectorFreeSpace(space);
            }
            else
            {
                bytesRemaining = RemainingSpaceAfterThisTag(lastTagAddress,
                                               sectorAddress + sectorLength);
            }

            if ((bytesRemaining >= bytesOfDataNeeded) && (bytesRemaining != INVALID))
            {
                chosenSector = thisSector;
                break;
            }

            thisSector = WRAP(thisSector + 1, numberOfSectors);
        }
    }

    if (chosenSector == INVALID)
        return false;
    //should never happen, but check anyways
    else if ((method == SELECT_NEXT_AND_INCREMENT) && (chosenSector == lastWriteSector))
        return false;

    *sectorNumber = (uint8_t) chosenSector;
    return true;
}

// Walk all tags in sector. If there's a valid+latest tag, then
// move it to another sector. When finished, all tags will have
// been marked obsolete and moved elsewhere if necessary.
static void AbandonSector(tag_space_t  space,
                          uint8_t     *sectorAddress,
                          uint8_t     *sectorEndAddressPlusOne)
{
    bool    rc;
    uint16_t tagNumber;
    uint16_t tagLength;
    uint16_t movetoSectorNumber = 0;     //init to rid warning
    uint8_t *data;
    uint8_t *address;

    //if we can't fix this sector, then we're hosed
    FixSectorIfNecessary(sectorAddress, sectorEndAddressPlusOne);

    //if sector is fresh, nothing to do
    address = sectorAddress + SECTOR_RESERVED_SIZE;
    if (FreshSpan_NoTagHeader(address))
        return;

    while (BasicSanityCheckTagHeader(address))
    {
        tagNumber = rutils_stream_to_word16(address + TAG_NUMBER_OFFSET);
        tagLength = rutils_stream_to_word16(address + LENGTH_OFFSET);
        data = address + HEADER_SIZE;

        if (IsTagLatestVersion(space, address))
        {
            //start looking to write at next sector, guaranteed
            rc = SelectWriteSector(space, tagLength,
                      SELECT_LAST_AND_INCREMENT, &movetoSectorNumber);
            if (!rc)
            {
                //serious error...no room to reclaim tags...NVM is trashed
                //no recovery
                nvm_register_fatal_error(REASON_NO_MORE_ROOM_FOR_WRITE);
            }

            WriteTagToThisSector(space,
                                 tagNumber,
                                 movetoSectorNumber,
                                 data,
                                 tagLength);
        }

        address += OffsetToNextTag_SaneOnly(address);
    }
}

// Do sanity checks on sectors at power-up
// Initialize 'vitals' and 'stats'
// Build the tag pointers list
static void InitializeSectors(tag_space_t space)
{
    const space_desc_t *descriptor = nvm_GetSpaceDesc(space);
    space_vitals_t  *spaceVitals = nvm_GetSpaceVitals(space);
    sector_stats_t  *statsBase = nvm_GetSectorStatsBase(space);
    sector_stats_t  *stats;
    sector_vitals_t *vitalsBase = nvm_GetSectorVitalsBase(space);
    sector_vitals_t *vitals;
    uint8_t         *sectorAddress = NULL;              //init to rid warning
    uint8_t         *sectorEndAddressPlusOne = NULL;    //init to rid warning
    uint16_t         i;
    uint16_t         numberOfSectors;
    uint16_t         sectorNumber = 0;                  //init to rid warning

    numberOfSectors = descriptor->numberOfSectors;

    for (i = 0; i < numberOfSectors; i++)
    {
        vitals = &vitalsBase[i];
        (void)GetSectorAddressAndPlusOne(space,
                                         i,
                                         &sectorAddress,
                                         &sectorEndAddressPlusOne);

        rutils_memset(vitals, 0, sizeof(sector_vitals_t));

        SectorSurvey(sectorAddress, sectorEndAddressPlusOne);

        vitals->lastTagAddress = LastTagInSector(sectorAddress + SECTOR_RESERVED_SIZE,
                                                 sectorEndAddressPlusOne);
    }

    ClearAllTagPtrs(space);

    for (i = 0; i < numberOfSectors; i++)
    {
        vitals = &vitalsBase[i];
        (void)GetSectorAddressAndPlusOne(space,
                                         i,
                                         &sectorAddress,
                                         &sectorEndAddressPlusOne);
        stats = &statsBase[i];

        CalculateSectorStats(sectorAddress, sectorEndAddressPlusOne, stats);

        UpdateTagPtrsFromSector(space, sectorAddress, sectorEndAddressPlusOne);
    }

    //select sector for next write op
    if (SelectWriteSector(space, 1, SELECT_FULLEST, &sectorNumber))
    {
        spaceVitals->currentWriteSector = sectorNumber;
    }
    else
    {
        //unrecoverable catastrophic error
    }

}

// Find sectors which are bad and erase them.
// Intented to be used when nvm is not running, as no
// stats, etc. are updated.
static void FindBadSectorsAndEraseThem(tag_space_t space,
                            const space_desc_t *spaceDesc)
{
    uint16_t         i;
    sector_sanity_t  sanity;
    uint8_t         *sectorAddress;
    uint8_t         *sectorEndAddressPlusOne;

    for (i = 0; i < spaceDesc->numberOfSectors; i++)
    {
        sectorAddress = GetSectorAddress(space, i);
        sectorEndAddressPlusOne = sectorAddress + spaceDesc->sectorLength;

        sanity = SanityCheckTagLayoutInSector(sectorAddress + SECTOR_RESERVED_SIZE,
                                    sectorEndAddressPlusOne - SECTOR_RESERVED_SIZE,
                                    NULL);

        if (SECTOR_SANITY_FAILED == sanity)
        {
            (void)nvm_low_level_flash_erase(space, i);
        }
    }
}

// If something got trashed and we've (supposedly) run out of storage
// space, this will recover, one way or another
static void RepairPhonySectorsFull(tag_space_t space,
                                   const space_desc_t *spaceDesc)
{
    sector_stats_t   currentStats;
    sector_stats_t  *stats;
    uint16_t         i;
    sector_sanity_t  sanity;
    uint8_t         *sectorAddress;
    uint8_t         *sectorEndAddressPlusOne;

    for (i = 0; i < spaceDesc->numberOfSectors; i++)
    {
        sectorAddress = GetSectorAddress(space, i);
        sectorEndAddressPlusOne = sectorAddress + spaceDesc->sectorLength;

        stats = nvm_GetSectorStatsBase(space);
        stats = &stats[i];

        sanity = SanityCheckTagLayoutInSector(sectorAddress + SECTOR_RESERVED_SIZE,
                                    sectorEndAddressPlusOne - SECTOR_RESERVED_SIZE,
                                    NULL);

        //sector insane? we've got to fix it here and now.
        if (sanity != SECTOR_SANITY_SANE)
        {
            FixSectorIfNecessary(sectorAddress, sectorEndAddressPlusOne);
        }

        //maybe the stats got messed up somewhere, and the flash system
        //thinks it's full when it's really not
        rutils_memcpy(&currentStats, stats, sizeof(currentStats));

        CalculateSectorStats(sectorAddress, sectorEndAddressPlusOne, stats);

        if (rutils_memcmp(stats, &currentStats, sizeof(currentStats)) != RFAIL_NOT_FOUND)
        {
            //keep track of this for debugging
            NVMstatsRepair++;
        }
   }
}

// Scan sector stats and, based on values, determin what (if any)
// sector needs to be reclaimed.
//
// Returns sector number of sector to be reclaimed, or
//  INVALID_UINT16 if none needed.
//
//    method:
//      SCORE_MOST_UNCLEAN
//          We must choose one sector, so pick the one with
//          the most garbage
//      SCORE_UNCLEAN_THRESHOLD
//          If the sector with the most garbage has more
//          than, say, 60% capacity garbage, then pick this
//      SCORE_ASYMPTOTIC
//          Like SCORE_THRESHOLD, but the algorithm more aggressively
//          removes garbage as the tag space runs out of free bytes
//
static uint16_t ReclaimScoreAlgorithm(tag_space_t    space,
                                    score_method_t method)
{
    space_stats_t      *stats;
    const uint32_t      DOWN_SCALER = 1024;
    uint32_t            maxUncleanTagBytes = 0;   //init to rid warning
    uint32_t            maxSectorFreeSpace;
    uint32_t            maxPossibleFreeSpace;
    uint16_t            reclaimSectorNumber;
    uint32_t            sectorGarbageRatioNM;
    uint32_t            totalGarbageRatioNM;
    uint32_t            freeSpaceNM;
    uint32_t            rampNM;
    uint32_t            thresholdNM;

    stats = nvm_GetSpaceStats(space);
    rutils_memset(stats, 0, sizeof(space_stats_t));

    AvailableRoomStats(space, stats, &maxUncleanTagBytes);

    switch (method)
    {
    case SCORE_MOST_UNCLEAN:
        reclaimSectorNumber = stats->maxUncleanTagSectorNumber;
        break;

    case SCORE_UNCLEAN_THRESHOLD:
        maxSectorFreeSpace = MaxSectorFreeSpace(space);

        // 'sectorGarbageRatio' is a normalized value in the range 0 - 1000
        sectorGarbageRatioNM = (maxUncleanTagBytes * NORMALIZED_MAX) / maxSectorFreeSpace;
        stats->garbageRatioNM = sectorGarbageRatioNM;

        // if worst sector exceeds garbage threshold, then erase this sector
        if (sectorGarbageRatioNM > SINGLE_SECTOR_THRESHOLD)
        {
            reclaimSectorNumber = stats->maxUncleanTagSectorNumber;
        }
        else
        {
            reclaimSectorNumber = INVALID_UINT16;
        }

        break;

    case SCORE_ASYMPTOTIC:
        maxPossibleFreeSpace = MaxSpaceFreeSpace(space);

        // calculate free space normalized (0-1000) 
        if (maxPossibleFreeSpace <= BYTES_1M)
        {
            freeSpaceNM = (stats->freeSpace * NORMALIZED_MAX) / maxPossibleFreeSpace;
        }
        else
        {
            // Using DOWN_SCALER to prevent uint32_t overflow after
            //    multiplication by NORMALIZED_MAX
            freeSpaceNM = ((stats->freeSpace / DOWN_SCALER) * NORMALIZED_MAX) /
                                                (maxPossibleFreeSpace / DOWN_SCALER);
        }

        // 'rampNM' is a normalized (0-1000) value between
        // the limits FREE_SPACE_HI and FREE_SPACE_LO
        if (freeSpaceNM > FREE_SPACE_HI)
        {
            rampNM = NORMALIZED_MAX;
        }
        else if (freeSpaceNM < FREE_SPACE_LO)
        {
            rampNM = 0;
        }
        else
        {
            rampNM = (freeSpaceNM - FREE_SPACE_LO) * NORMALIZED_MAX /
                                  (FREE_SPACE_HI - FREE_SPACE_LO);
        }
        stats->rampNM = rampNM;

        // 'thresholdNM' runs between 'THRESHOLD_HI' and 'THRESHOLD_LO'
        thresholdNM = rampNM * (THRESHOLD_HI - THRESHOLD_LO) / NORMALIZED_MAX + THRESHOLD_LO; 
        stats->thresholdNM = thresholdNM;

        // 'garbageRatioNM' is a percentage in the range 0 - 1000
        totalGarbageRatioNM = (stats->totalUncleanBytes * NORMALIZED_MAX) / maxPossibleFreeSpace;
        stats->garbageRatioNM = totalGarbageRatioNM;

        // normalized worst single sector garbage
        maxSectorFreeSpace = MaxSectorFreeSpace(space);
        sectorGarbageRatioNM = (maxUncleanTagBytes * NORMALIZED_MAX) / maxSectorFreeSpace;

        // if worst single sector garbage exceeds one threshold  OR
        // if total garbage exceeds floating threshold
        if ( (sectorGarbageRatioNM > SINGLE_SECTOR_THRESHOLD)  ||
             (totalGarbageRatioNM > thresholdNM) )
        {
            reclaimSectorNumber = stats->maxUncleanTagSectorNumber;
        }
        else
        {
            reclaimSectorNumber = INVALID_UINT16;
        }
        break;

    //will never hit default
    default:
        reclaimSectorNumber = INVALID_UINT16;
    }

    return reclaimSectorNumber;
}

// Abandon sector, then queue it for a background erase
static bool ReclaimSector(tag_space_t space, uint16_t sectorNumber)
{
    space_vitals_t   *spaceVitals = nvm_GetSpaceVitals(space);
    sector_vitals_t  *vitalsBase = nvm_GetSectorVitalsBase(space);
    sector_vitals_t  *vitals;
    uint8_t          *sectorAddress;
    uint8_t          *sectorEndAddressPlusOne;

    if ((sectorNumber != INVALID_UINT16)  &&
        (spaceVitals->sectorErasing == INVALID_UINT16))
    {
        //'sectorAbandoning' is mostly used just for debug or warm restarts
        spaceVitals->sectorAbandoning = sectorNumber;

        sectorAddress = NULL;                   // suppress warning
        sectorEndAddressPlusOne = NULL;         // suppress warning
        GetSectorAddressAndPlusOne(space,
                                   sectorNumber,
                                   &sectorAddress,
                                   &sectorEndAddressPlusOne);

        AbandonSector(space,
                      sectorAddress,
                      sectorEndAddressPlusOne);

        vitals = vitalsBase + sectorNumber;
        UNUSED(vitals);       // not used...but keeping around in case needed

        return true;
    }

    return false;
}

//******************************************************************************
// API Functions
//******************************************************************************

//!
//! @name      NVMinit
//!
//! @brief     Init all tag spaces.
//!
//! @details   In addition, it initializes bottom-level driver.
//! @details   Sanity check is made on all sectors.
//! @details   RAM pointers to tags point to latest versions of tags.
//! @details   Any 1/2 written tags are closed/discarded.
//! @details   
//!
//! @param[in] 'findAndEraseBadSectors'-- If an unreadable
//! @param[in]     non-all-FF sector is found, it's erased.
//! @param[in]     Assumption is that such a sector was partway
//! @param[in]     erased; device powered down during an erase.
//!
void NVMinit(bool findAndEraseBadSectors)
{
    tag_space_t         space;
    uint32_t           *tag_ptr_base;
    uint16_t            max_tags;
    const space_desc_t *spaceDesc;
    space_vitals_t     *spaceVitals;
    space_stats_t      *spaceStats;
    uint16_t            i;

    nvm_low_level_init();

    for (i = 0; i < SPACE_max; i++)
    {
        space = nvm_GetTagSpace(i);
        spaceDesc = nvm_GetSpaceDesc(space);
        spaceVitals = nvm_GetSpaceVitals(space);
        spaceStats = nvm_GetSpaceStats(space);

        // Clear out tag pointers
        max_tags = 0;
        tag_ptr_base = nvm_GetTagPtrBase(i, &max_tags);
        rutils_memset(tag_ptr_base, 0, max_tags * sizeof(uint32_t));

        //Was a sector erase interrupted before completed?
        //That would trash a sector.
        if (findAndEraseBadSectors)
        {
            FindBadSectorsAndEraseThem(space, spaceDesc);
        }

        //now we can kill everything in 'spaceVitals'
        rutils_memset(spaceVitals, 0, sizeof(space_vitals_t));
        spaceVitals->sectorAbandoning = INVALID_UINT16;
        spaceVitals->sectorErasing = INVALID_UINT16;

        //normal init stuff
        InitializeSectors(space);

        AvailableRoomStats(space, spaceStats, NULL);
    }

    NVMinitComplete = true;
}

//!
//! @name      NVMreadTag
//!
//! @brief     Read tag and/or tag attributes.
//!
//! @details   Must call NVMinit first.
//! @details   
//!
//! @param[in] 'space'-- 
//! @param[in] 'tagNumber'-- range 1 to N where N is defined in platform file.
//! @param[out 'dataPtr'-- is set to address in flash where tag data starts
//! @param[out]     if tag's never been written, set to NULL and return true
//! @param[out] 'dataLength'-- Filled in with length of tag
//! @param[out]      if tags's never been written, set to zero.
//! @param[out]    ..but you can have a tag that's been written of
//! @param[out]    length zero too!
//!
void NVMreadTag(tag_space_t space,
                uint16_t    tagNumber,
                void    **dataPtr,
                uint16_t   *dataLength)
{
    uint8_t *tagPtr;

    if (!NVMinitComplete)
        return;

    if (!VerifyTagNumberIsWithinRange(space, tagNumber))
    {
        nvm_register_fatal_error(REASON_INVALID_TAG_NUMBER);
        return;
    }

    tagPtr = GetTagPtr(space, tagNumber);
    if (tagPtr == NULL)
    {
        *dataPtr = NULL;
        *dataLength = 0;
        return;
    }

    *dataPtr = tagPtr + HEADER_SIZE;
    *dataLength = rutils_stream_to_word16(tagPtr + LENGTH_OFFSET);
}

//!
//! @name      NVMwriteTag
//!
//! @brief     Write tag.
//!
//! @details   Must call NVMinit first.
//! @details   
//!
//! @param[in] 'space'-- 
//! @param[in] 'tagNumber'-- range 1 to N where N is defined in platform file.
//! @param[in] 'dataPtr'-- caller's data to be written
//! @param[in] 'dataLength'-- length of caller's data
//!
void NVMwriteTag(tag_space_t space,
                 uint16_t    tagNumber,
                 void       *dataPtr,
                 uint16_t    dataLength)
{
    bool             rc;
    space_vitals_t *spaceVitals;
    uint16_t        writetoSectorNumber = 0;  //init to rid warning

    if (!NVMinitComplete)
        return;

    if (!VerifyTagNumberIsWithinRange(space, tagNumber))
    {
        nvm_register_fatal_error(REASON_INVALID_TAG_NUMBER);
        return;
    }

    //choose sector to write to
    rc = SelectWriteSector(space, dataLength,
                  SELECT_LAST_AND_INCREMENT, &writetoSectorNumber);
    if (!rc)
    {
        spaceVitals = nvm_GetSpaceVitals(space);
        spaceVitals->digDeeperIntoGarbage = true;
        return;    // fixme??
    }

    //write tag to that sector
    WriteTagToThisSector(space,
                         tagNumber,
                         writetoSectorNumber,
                         dataPtr,
                         dataLength);
}

//!
//! @name      NVMgarbageCollectNoErase
//!
//! @brief     Free up flash storage from write cycles
//!
//! @details   Must call NVMinit first.
//! @details   Check if any garbage collecting needs to be done, and do it.
//! @details   Garbage collection consists of:
//! @details    -- Moving any valid tags to another sector
//! @details    -- This will cause all tags to be marked invalid in sector
//! @details    -- Set internal states. Call to NVMeraseIfNeeded() will
//! @details        see sector needing to be erased
//! @details   Does not complete garbage collection, as erase is necessary.
//! @details   Erasures are deferred to dedicated APIs.
//!
//! @param[in] 'space'-- 
//! @param[in] 'score_method'-- 
//! @param[in]      Recommend 'score_method'==SCORE_ASYMPTOTIC by default,
//! @param[in]      SCORE_MOST_UNCLEAN at startup.
//!
//! @return    Sector that was reclaimed, to be erased, or
//! @return     RFAIL_NOT_FOUND if no sector was reclaimed.
//!
int NVMgarbageCollectNoErase(tag_space_t    space,
                             score_method_t score_method)
{
    const space_desc_t *spaceDesc = nvm_GetSpaceDesc(space);
    space_vitals_t     *spaceVitals;
    uint16_t            reclaimSectorNumber;

    if (!NVMinitComplete)
        return RFAIL_NOT_FOUND;

    //sanity check 'space'
    if (spaceDesc == NULL)
        return RFAIL_NOT_FOUND;   // won't happen

    spaceVitals = nvm_GetSpaceVitals(space);

    //have we run across unexpected errors?
    //if so, something bad must was going on--try to recover
    if (spaceVitals->digDeeperIntoGarbage)
    {
        RepairPhonySectorsFull(space, spaceDesc);
        spaceVitals->digDeeperIntoGarbage = false;
    }

    // Use 
    reclaimSectorNumber = ReclaimScoreAlgorithm(space, score_method);

    if (reclaimSectorNumber != INVALID_UINT16)
    {
        ReclaimSector(space, reclaimSectorNumber);

        return (int)reclaimSectorNumber;
    }

    return RFAIL_NOT_FOUND;
}

//!
//! @name      NVMeraseIfNeeded
//!
//! @brief     Does foreground erase on sectors which have been abandoned.
//!
//! @details   Must call NVMinit first.
//! @details   
//! @details   
//! @details   
//!
//! @param[in] 'space'-- 
//!
//! @return    'true' if erasure occured.
//!
bool NVMeraseIfNeeded(tag_space_t space)
{
    space_vitals_t     *spaceVitals = nvm_GetSpaceVitals(space);

    if (INVALID_UINT16 != spaceVitals->sectorAbandoning)
    {
        // TBD: check on spaceVitals->sectorAbandoning logic
        // Check entire fcn, come to think of it...

        NVMeraseSectorForeground(space, spaceVitals->sectorAbandoning);

        spaceVitals->sectorAbandoning = INVALID_UINT16;

        return true;
    }

    return false;
}


//!
//! @name      NVMeraseSectorForeground
//!
//! @brief     Does forced foreground erase on specific sector.
//!
//! @details   Must call NVMinit first.
//! @details   Caller blocks while erase happens
//! @details   
//! @details   
//!
//! @param[in] 'space'-- 
//! @param[in] 'sectorNumber'-- numbered 0, 1, ..., N
//!
void NVMeraseSectorForeground(tag_space_t space, uint16_t sectorNumber)
{
    space_vitals_t     *spaceVitals = nvm_GetSpaceVitals(space);
    uint8_t            *sectorAddress;
    uint8_t            *sectorEndAddressPlusOne;

    GetSectorAddressAndPlusOne(space,
                               sectorNumber,
                               &sectorAddress,
                               &sectorEndAddressPlusOne);

    spaceVitals->sectorErasing = sectorNumber;

    ForegroundSectorErase(space,
                          sectorNumber,
                          sectorAddress,
                          sectorEndAddressPlusOne,
                          spaceVitals);

    SectorEraseCompletion(space);
}

//!
//! @name      NVMeraseSectorBackground
//!
//! @brief     Does forced background erase on specific sector.
//!
//! @details   Must call NVMinit first.
//! @details   Erasure is deferred to another agent running at
//! @details   a lower priority, which will handle erase.
//!
//! @param[in] 'space'-- 
//! @param[in] 'sectorNumber'-- numbered 0, 1, ..., N
//!
void NVMeraseSectorBackground(tag_space_t space, uint16_t sectorNumber)
{
    UNUSED(space);
    UNUSED(sectorNumber);

    // TBD
}

//!
//! @name      NVMbackgroundEraseCompleteCallback
//!
//! @brief     Callback for agent to notify completion of erase
//!
//! @details   Must call NVMinit first.
//! @details   Notifies completion of erase started by NVMeraseSectorBackground().
//!
//! @param[in] 'space'-- 
//! @param[in] 'sectorNumber'-- numbered 0, 1, ..., N
//!
void NVMbackgroundEraseCompleteCallback(tag_space_t space)
{
    UNUSED(space);

    // TBD
}

//--------------------------  Maintenance API'S  -----------------------------

//!
//! @name      NVMtotalReset
//!
//! @brief     Complete clearing of entire tag space
//!
//! @details   Erases all sectors in a space.
//! @details   Does not require NVMinit call as prerequisite,
//! @details   but NVMinit must be called after this to resume
//! @details   usaage.
//! @details   USE WITH CAUTION!
//!
//! @param[in] 'space'-- 
//!
void NVMtotalReset(tag_space_t space)
{
    const space_desc_t *spaceDesc = nvm_GetSpaceDesc(space);
    space_vitals_t     *spaceVitals = nvm_GetSpaceVitals(space);
    uint32_t           *tagPtrBase;
    uint8_t            *sectorAddress = NULL;         //init to rid warning
    uint8_t            *sectorAddressPlusOne = NULL;  //init to rid warning
    uint16_t            maxTags = 0;                  //init to rid warning
    uint16_t            i;

    NVMinitComplete = false;

    nvm_low_level_init();

    //must reset 'sectorErasing' here, to prevent SectorEraser from possibly
    //finding a sector to erase.
    rutils_memset(spaceVitals, 0, sizeof(space_vitals_t));
    spaceVitals->sectorAbandoning = INVALID_UINT16;
    spaceVitals->sectorErasing = INVALID_UINT16;

    for (i = 0; i < spaceDesc->numberOfSectors; i++)
    {
        (void)GetSectorAddressAndPlusOne(space,
                                         i,
                                         &sectorAddress,
                                         &sectorAddressPlusOne);

        ForegroundSectorErase(space,
                              i,
                              sectorAddress,
                              sectorAddressPlusOne,
                              spaceVitals);
    }

    tagPtrBase = nvm_GetTagPtrBase(space, &maxTags);
    rutils_memset(tagPtrBase, 0, maxTags * sizeof(uint32_t));
}

//!
//! @name      NVMlatestTagInfo
//!
//! @brief     Get internal info about a tag
//!
//! @details   Get the following: 'version', 'length', 'address'
//! @details   It'll skip fetching any of the above if they are NULL.
//! @details   NVMinit must be called first.
//!
//! @param[in] 'space'-- 
//! @param[in] 'tagNumber'-- 
//! @param[out] 'version'--
//! @param[out] 'length'--
//! @param[out] 'address'--
//!
//! @return    Returns 0 if no clean version of tag exists
//!
bool NVMlatestTagInfo(tag_space_t space,
                      uint16_t    tagNumber,
                      uint16_t   *version,
                      uint16_t   *length,
                      uint32_t   *address)
{
    uint8_t *tagPtr;

    if (!NVMinitComplete)
        return false;

    tagPtr = GetTagPtr(space, tagNumber);

    if (tagPtr == NULL)
        return false;

    if (version != NULL)
        *version = rutils_stream_to_word16(tagPtr + VERSION_OFFSET);

    if (length != NULL)
        *length = rutils_stream_to_word16(tagPtr + LENGTH_OFFSET);

    if (address != NULL)
        *address = (uint32_t) tagPtr;

    return true;
}

//!
//! @name      NVMsanityCheckSector
//!
//! @brief     Sanity check sector
//!
//! @details   
//! @details   
//! @details   
//!
//! @param[in] 'space'-- 
//! @param[in] 'sectorNumber'-- 
//!
//! @return    Returns true if sector has no incorrectly formatted tags
//!
bool NVMsanityCheckSector(tag_space_t space, uint16_t sectorNumber)
{
    const space_desc_t *desc = nvm_GetSpaceDesc(space);
    uint8_t            *startAddress;
    uint8_t            *lastAddressPlusOne;
    sector_sanity_t     sanity;

    startAddress = GetSectorAddress(space, sectorNumber);
    if (startAddress == NULL)
        return false;

    //Top reserved area still fresh?
    if (!IsMemSetToValue(startAddress, BYTE_NEVER_WRITTEN, SECTOR_RESERVED_SIZE))
        return false;

    startAddress += SECTOR_RESERVED_SIZE;
    lastAddressPlusOne = startAddress + desc->sectorLength
                           - SECTOR_RESERVED_SIZE - SECTOR_HEADROOM;

    //Bottom reserved area + headroom still fresh?
    if (!IsMemSetToValue(lastAddressPlusOne, BYTE_NEVER_WRITTEN,
                                     SECTOR_RESERVED_SIZE + SECTOR_HEADROOM))
        return false;

     //do actual sector check
     sanity = SanityCheckTagLayoutInSector(startAddress, lastAddressPlusOne, NULL);

     return (sanity == SECTOR_SANITY_SANE);
}

sector_stats_t *NVMfetchSectorStats(tag_space_t     space,
                                 uint16_t        sectorNumber)
{
    const space_desc_t *desc = nvm_GetSpaceDesc(space);

    if (sectorNumber < desc->numberOfSectors)
        return nvm_GetSectorStatsBase(space) + sectorNumber;

    return NULL;
}

//!
//! @name      NVMnVersions
//!
//! @brief     Scans tag space for different versions of 'tagNumber'
//!
//! @param[in] 'space'-- 
//! @param[in] 'sectorNumber'-- 
//!
//! @details   Scans tag space for different versions of 'tagNumber'
//! @details
//! @details   Each element index in array is a matched set.
//! @details   Pass in a NULL for any parm not wanted.
//! @details   Array set:
//! @details    'addressArray[maxArray]'-- addresses of tags
//! @details     'versionArray[maxArray]'-- versions
//! @details     'lengthArray[maxArray]'-- lengths
//! @details
//! @details   'numResults' is number of elements pushed onto arrays
//! @details     (always <= 'maxArray')
//! @details
//! @details   It will only get the results for the range between
//! @details   'versionHi' and 'versionLo'.
//! @details     -- if 'versionHi' and 'versionLo' are both 0,
//! @details          match any version
//! @details     -- if 'versionHi' is zero and 'versionLo' is not,
//! @details          match from latest tag's version on down.
//! @details          'versionLo' is number of prior versions relative
//! @details          to latest version
//! @details     -- if 'versionHi' is not zero and 'versionLo' is,
//! @details          match from 'versionLo' and up
//!
//! @returns false if failure
//!
bool NVMnVersions(tag_space_t space,
                  uint16_t    tagNumber,
                  uint16_t    versionHi,
                  uint16_t    versionLo,
                  uint32_t   *addressArray,
                  uint16_t   *versionArray,
                  uint16_t   *lengthArray,
                  uint16_t    maxArray,
                  uint16_t   *numResults)
{
    const space_desc_t *desc = nvm_GetSpaceDesc(space);
    int               tagsScanned;
    uint8_t            *address;
    uint8_t            *lastAddressPlusOne;
    uint16_t            index = 0;
    uint16_t            thisTagNumber;
    uint16_t            version;
    uint16_t            versionLo2, versionHi2;
    uint16_t            length;
    uint16_t            i;
    bool              loZero, hiZero;

    if (!NVMinitComplete)
        return false;

    versionLo2 = VERSION_MIN;
    versionHi2 = VERSION_MAX;

    hiZero = versionHi == 0;
    loZero = versionLo == 0;

    if (hiZero && !loZero)
    {
        if (!NVMlatestTagInfo(space, tagNumber, &versionHi2, NULL, NULL))
            return false;

        versionLo2 = versionHi2 - versionLo;
    }
    else if (!hiZero && loZero)
    {
        versionHi2 = versionHi;
    }

    //scan all sectors
    for (i = 0; i < desc->numberOfSectors; i++)
    {
        //set to top/bottom of usable part of sector
        //include headroom (SECTOR_HEADROOM) just in case
        address = GetSectorAddress(space, i) + SECTOR_RESERVED_SIZE;
        lastAddressPlusOne = address + desc->sectorLength
                                  - SECTOR_RESERVED_SIZE;

        tagsScanned = 0;

        //scan this sector
        while (address < lastAddressPlusOne)
        {
            if (index == maxArray)
                goto NVMnVersions_abort;

            //reached last tag in sector?
            if (FreshSpan_NoTagHeader(address))
                break;

            //sector's insane...skip over it
            if (!BasicSanityCheckTagHeader(address))
                break;

            thisTagNumber = rutils_stream_to_word16(address + TAG_NUMBER_OFFSET);
            version = rutils_stream_to_word16(address + VERSION_OFFSET);
            length = rutils_stream_to_word16(address + LENGTH_OFFSET);

            //if this tag falls within matching criteria, then
            //push onto result array(s)
            if ( (thisTagNumber == tagNumber)       &&
                 (version >= versionLo2) && (version <= versionHi2) )
            {
                if (addressArray != NULL)
                    addressArray[index] = (uint32_t) address;

                if (versionArray != NULL)
                    versionArray[index] = version;

                if (lengthArray != NULL)
                    lengthArray[index] = length;

                index++;
            }

            //jump to start (header) of next tag
            address += OffsetToNextTag_SaneOnly(address);

            tagsScanned++;
        }
    }

    NVMnVersions_abort:

    *numResults = index;

    return true;
}