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

//! @file     nvm-stm32f4xx.c
//! @authors  Bernie Woodland
//! @date     11May19
//!
//! @brief    STM32F4xx series-specific FLASH programming interface
//!
//! @details  STM32F4XX-series flash programming interface. Guts copied from
//! @details  stm32f4xx_flash.c.
//! @details  This code is used for Bank0 operations only. It assumes that
//! @details  part is single-bank part, so actual write and erase routines
//! @details  are copied up to RAM and run there. Interrupts are locked when
//! @details  write and erases are happening, to prevent interrupt code from
//! @details  executing out of flash.
//! @details  Erases cannot be paused+resumed, so interrupts will be locked for
//! @details  a long time during erasures.


#include "raging-global.h"
#include "raging-contract.h"
#include "raging-utils-mem.h"

#include "stm32f4xx_flash.h"

#include "nvm-stm32f4xx.h"


//------------------ Local Defines  -------------------------

//#define FLASH_STATUS_OK(status)    ( (status) & (FLASH_FLAG_WRPERR | FLASH_FLAG_RDERR | (uint32_t)0xE0 | FLASH_FLAG_OPERR) )
#define FLASH_STATUS_OK(status)  ( ((status) & (                              \
                                  (uint32_t)FLASH_FLAG_RDERR  |               \
                                  (uint32_t)FLASH_FLAG_PGSERR |               \
                                  (uint32_t)FLASH_FLAG_PGPERR |               \
                                  (uint32_t)FLASH_FLAG_PGAERR |               \
                                  (uint32_t)FLASH_FLAG_WRPERR                 \
                                                )) == 0  )   

// Convert fcn ptrs to and from uint32_t's.
// Bit0/thumb mode is the problem: fcn ptr must have bit 0 set or will crash
#define VOID_PTR_TO_FCN_PTR(x)   ( (void *) ((uint32_t)(x) | 1) )
#define FCN_TO_VOID_PTR(x)       ( (void *) ((uint32_t)(&(x)) & 0xFFFFFFFE) )

// STM flash part does not allow copies across 64-bit boundary
#define  FLASH_PAGE_SIZE      8

// Copied from stm32f4xx_flash.c 
#define SECTOR_MASK               ((uint32_t)0xFFFFFF07)

//------------------ RAM reservation for write, erase fcns.  ------------------

// Fixed sized RAM buffers for RAM-resident write and erase fcns.
// Bit of a hack.
//
//               CAUTION!
// Both MAX_WRITE_FCN_SIZE_BYTES and MAX_ERASE_FCN_SIZE_BYTES
// values have been tuned by looking at disassembled length of
// stm_flash_write_doubleword_bank0 and stm_flash_erase_sector_bank0
// in debug build gdb session. An extra 20 bytes have been added for
// safety. If you add code to those fcns, then you must bump these
// values.
#define  MAX_WRITE_FCN_SIZE_BYTES   (108+20)
#define  MAX_WRITE_FCN_SIZE         (MAX_WRITE_FCN_SIZE_BYTES/BYTES_PER_WORD32)

#define  MAX_ERASE_FCN_SIZE_BYTES   (112+20)
#define  MAX_ERASE_FCN_SIZE         (MAX_ERASE_FCN_SIZE_BYTES/BYTES_PER_WORD32)
uint32_t stm_write_fcn_ram[MAX_WRITE_FCN_SIZE];
uint32_t stm_erase_fcn_ram[MAX_ERASE_FCN_SIZE];

//------------------ Function ptrs for RAM-resident ops  ----------------------

uint32_t (*stm_flash_write_byte_bank0_ptr)(uint32_t Address,
                                           uint8_t  Data,
                                           unsigned sector_number);
uint32_t (*stm_flash_write_halfword_bank0_ptr)(uint32_t Address,
                                               uint16_t Data,
                                               unsigned sector_number);
uint32_t (*stm_flash_write_word_bank0_ptr)(uint32_t Address,
                                           uint32_t Data,
                                           unsigned sector_number);
uint32_t (*stm_flash_write_doubleword_bank0_ptr)(uint32_t Address,
                                                 uint64_t Data,
                                                 unsigned sector_number);
uint32_t (*stm_flash_erase_bank0_ptr)(uint32_t sector_number, uint32_t psize);

//------------------ Local Variables  -----------------------------------------

bool    stm_unlocked;
uint8_t stm_voltage_level;

//------------------ Part-specific Info  ------------------
// From STM32f40x Reference Manual

// Bank Specific defines
#define STM_BASE_ADDRESS_BANK0       ((uint8_t *)0x8000000)
#define STM_BASE_ADDRESS_BANK1       ((uint8_t *)0x8100000)

// Either bank0 or bank1
#define STM_SIZE_BANK                 0x100000
#define STM_SIZE_DATA_SECTOR            0x4000
#define STM_SIZE_TEXT_SECTOR_FRONT      0x10000
#define STM_SIZE_TEXT_SECTOR_BACK       0x20000

// Layout of Either bank0 or bank1:
// index is sector number, value is starting offset
static const uint32_t stm_layout[] = {
    0*STM_SIZE_DATA_SECTOR,                                                              // 0
    1*STM_SIZE_DATA_SECTOR,                                                              // 1
    2*STM_SIZE_DATA_SECTOR,                                                              // 2
    3*STM_SIZE_DATA_SECTOR,                                                              // 3
    4*STM_SIZE_DATA_SECTOR + 0*STM_SIZE_TEXT_SECTOR_FRONT,                               // 4
    4*STM_SIZE_DATA_SECTOR + 1*STM_SIZE_TEXT_SECTOR_FRONT + 0*STM_SIZE_TEXT_SECTOR_BACK, // 5
    4*STM_SIZE_DATA_SECTOR + 1*STM_SIZE_TEXT_SECTOR_FRONT + 1*STM_SIZE_TEXT_SECTOR_BACK, // 6
    4*STM_SIZE_DATA_SECTOR + 1*STM_SIZE_TEXT_SECTOR_FRONT + 2*STM_SIZE_TEXT_SECTOR_BACK, // 7
    4*STM_SIZE_DATA_SECTOR + 1*STM_SIZE_TEXT_SECTOR_FRONT + 3*STM_SIZE_TEXT_SECTOR_BACK, // 8
    4*STM_SIZE_DATA_SECTOR + 1*STM_SIZE_TEXT_SECTOR_FRONT + 4*STM_SIZE_TEXT_SECTOR_BACK, // 9
    4*STM_SIZE_DATA_SECTOR + 1*STM_SIZE_TEXT_SECTOR_FRONT + 5*STM_SIZE_TEXT_SECTOR_BACK, //10
    4*STM_SIZE_DATA_SECTOR + 1*STM_SIZE_TEXT_SECTOR_FRONT + 6*STM_SIZE_TEXT_SECTOR_BACK  //11
};

//******************************************************************************
// Local Functions
//******************************************************************************

//!
//! @name      stm_interrupt_disable
//!
//! @brief     Full interrupt disable
//!
//! @details   NUFR uses the BASEPRI register to mask off interrupts,
//! @details   So PRIMASK should be not used. Using PRIMASK so that NO
//! @details   interrupts are enabled.
//!
//! @return    'true' if the add necessitates a context switch
//!
static void stm_interrupt_disable(void)
{
    __asm volatile(
        "  cpsid i      "                     // set bit 0 of PRIMASK
        : );
}

//!
//! @name      stm_interrupt_enable
//!
//! @brief     Inverse of disable
//!
static void stm_interrupt_enable(void)
{
    __asm volatile(
        "  cpsie i      "                     // clear bit 0 of PRIMASK
        : );
}

//!
//! @name      stm_are_interrupts_disabled
//!
//! @brief     Sanity check to ensure PRIMASK is not being used.
//!
//! @return    'true' if PRIMASK has interrupts disabled (not good)
//!
static bool __attribute__((naked)) stm_are_interrupts_disabled(void)
{
    __asm volatile(
        "  mrs    r0, primask\n\r"              // Move PRIMASK to r0/return
        "  and    r0, #1\n\r"                   // Mask out bits 31:1, leave 0
        "  bx     lr"
        : );

    return false;           // will never get here...supress compiler warning
}

//!
//! @name      stm_voltage_level_to_internal
//!
//! @brief     Type conversion from API voltage type to STM-type
//!
//! @return        'voltage'                Returns
//! @return    STM_FLASH_VOLTAGE_1      VoltageRange_1
//! @return    STM_FLASH_VOLTAGE_2      VoltageRange_2
//! @return    STM_FLASH_VOLTAGE_3      VoltageRange_3
//! @return    STM_FLASH_VOLTAGE_4      VoltageRange_4
//!
static uint8_t stm_voltage_level_to_internal(stm_flash_voltage_t voltage)
{
    uint8_t range;

    switch (voltage)
    {
    case STM_FLASH_VOLTAGE_1:
        range = VoltageRange_1;
        break;
    case STM_FLASH_VOLTAGE_2:
        range = VoltageRange_2;
        break;
    case STM_FLASH_VOLTAGE_3:
        range = VoltageRange_3;
        break;
    case STM_FLASH_VOLTAGE_4:
        range = VoltageRange_4;
        break;
    default:
        range = VoltageRange_1;
        APP_REQUIRE(false);
        break;
    }

    return range;
}

/**
  * @brief  Unlocks the FLASH control register access
  * @details Copied from 'FLASH_Unlock()' from STM code
  * @param  None
  * @retval None
  */
static void stm_flash_unlock(void)
{
    if ((FLASH->CR & FLASH_CR_LOCK) != RESET)
    {
        /* Authorize the FLASH Registers access */
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }  
}

//!
//! @name      stm_is_valid_address_bank0
//!
//! @brief     Sanity check
//!
//! @return    'true' if 'address' falls within range of bank0 flash address
//!
bool stm_is_valid_address_bank0(uint8_t *address)
{
    if ((address < STM_BASE_ADDRESS_BANK0) ||
        (address >= (STM_BASE_ADDRESS_BANK0 + STM_SIZE_BANK)))
    {
        return false;
    }

    return true;
}

//!
//! @name      stm_sector_number_to_address_bank0
//!
//! @brief     Sanity check
//!
//! @return    'true' if 'address' falls within range of bank0 flash address
//!
uint8_t *stm_sector_number_to_address_bank0(unsigned sector_number)
{
    // Sanity check
    if (sector_number >= ARRAY_SIZE(stm_layout))
    {
        return (uint8_t *)0xFFFFFFFF;
    }

    return STM_BASE_ADDRESS_BANK0 + stm_layout[sector_number];
}

//!
//! @name      stm_address_to_sector_number
//!
//! @brief     Sanity check
//!
//! @return    'true' if 'address' falls within range of bank0 flash address
//!
unsigned stm_address_to_sector_number(uint8_t *address)
{
    uint32_t offset;
    unsigned sector_number;
    const unsigned START_64k_SECTORS = 4;
    const unsigned START_128k_SECTORS = 5;

    if (stm_is_valid_address_bank0(address))
    {
        offset = address - STM_BASE_ADDRESS_BANK0;
    }
    else
    {
        offset = 0;         // make compiler happy
        return 0xFFFFFFFF;
    }

    if (offset < stm_layout[START_64k_SECTORS])
    {
        sector_number = offset / STM_SIZE_DATA_SECTOR;
    }
    else if (offset < stm_layout[START_128k_SECTORS])
    {
        sector_number = START_64k_SECTORS;
    }
    else
    {
        offset -= stm_layout[START_128k_SECTORS];
        sector_number = START_128k_SECTORS;
        sector_number += offset / STM_SIZE_TEXT_SECTOR_BACK;
    }

    return sector_number;
}

//!
//! @name      stm_voltage_level_to_psize
//!
//! @brief     Type conversion of voltage type to register bit field
//!
//! @return    
//! @return    FLASH_PSIZE_BYTE         (for VoltageRange_1 settings)
//! @return    FLASH_PSIZE_HALF_WORD    (for VoltageRange_2 settings or lower)
//! @return    FLASH_PSIZE_WORD         (for VoltageRange_3 settings or lower)
//! @return    FLASH_PSIZE_DOUBLE_WORD  (for VoltageRange_4 settings or lower)
//!
unsigned stm_voltage_level_to_psize(uint8_t voltage)
{
    unsigned psize;

    switch (voltage)
    {
    case VoltageRange_1:
        psize = FLASH_PSIZE_BYTE;
        break;
    case VoltageRange_2:
        psize = FLASH_PSIZE_HALF_WORD;
        break;
    case VoltageRange_3:
        psize = FLASH_PSIZE_WORD;
        break;
    case VoltageRange_4:
        psize = FLASH_PSIZE_DOUBLE_WORD;
        break;
    default:
        psize = 0;
        APP_REQUIRE(false);
        break;
    }

    return psize;
}

//!
//! @name      stm_flash_write_byte_bank0
//!
//! @brief     Single byte flash write
//!
//! @details   CAUTION note for MAX_WRITE_FCN_SIZE_BYTES applies here!
//!
//! @return    Register FLASH_SR contents
//!
uint32_t stm_flash_write_byte_bank0(uint32_t Address,
                                    uint8_t  Data,
                                    unsigned sector_number)
{
    uint32_t status_reg;
    uint32_t cr;

    cr = FLASH->CR;
    cr &= (CR_PSIZE_MASK & SECTOR_MASK);
    cr |= (FLASH_PSIZE_BYTE | FLASH_CR_PG | sector_number);
    FLASH->CR = cr;

    *(__IO uint8_t *)Address = Data;

    // Poll register until complete. Takes around 16 usecs.
    status_reg = FLASH->SR;
    while ((status_reg & FLASH_FLAG_BSY) != 0)
    {
        status_reg = FLASH->SR;
    }

    /* if the program operation is completed, disable the PG Bit */
    cr &= (~FLASH_CR_PG);
    FLASH->CR = cr;

    return status_reg;
}

//!
//! @name      stm_flash_write_halfword_bank0
//!
//! @brief     2-byte flash write
//!
//! @details   Can only be used for voltage >= VoltageRange_2
//! @details   CAUTION note for MAX_WRITE_FCN_SIZE_BYTES applies here!
//!
//! @param[in] 'Address'-- cannot cross program page (64-bit) boundary
//! @param[in] 'Data'-- 16-bit value to write. Little-endian write:
//! @param[in]    bits 0:7 at 'Address', bits 8:15 at 'Address'+1
//!
//! @return    Register FLASH_SR contents
//!
uint32_t stm_flash_write_halfword_bank0(uint32_t Address,
                                        uint16_t Data,
                                        unsigned sector_number)
{
    uint32_t status_reg;
    uint32_t cr;

    cr = FLASH->CR;
    cr &= (CR_PSIZE_MASK & SECTOR_MASK);
    cr |= (FLASH_PSIZE_HALF_WORD | FLASH_CR_PG | sector_number);
    FLASH->CR = cr;

    *(__IO uint16_t *)Address = Data;

    // Poll register until complete. Takes around 16 usecs.
    status_reg = FLASH->SR;
    while ((status_reg & FLASH_FLAG_BSY) != 0)
    {
        status_reg = FLASH->SR;
    }

    /* if the program operation is completed, disable the PG Bit */
    cr &= (~FLASH_CR_PG);
    FLASH->CR = cr;

    return status_reg;
}

//!
//! @name      stm_flash_write_word_bank0
//!
//! @brief     4-byte flash write
//!
//! @details   Can only be used for voltage >= VoltageRange_3
//! @details   CAUTION note for MAX_WRITE_FCN_SIZE_BYTES applies here!
//!
//! @param[in] 'Address'-- cannot cross program page (64-bit) boundary
//! @param[in] 'Data'-- 32-bit value to write. Little-endian write:
//! @param[in]    bits 0:7 at 'Address', bits 31:24 at 'Address'+3
//!
//! @return    Register FLASH_SR contents
//!
uint32_t stm_flash_write_word_bank0(uint32_t Address,
                                    uint32_t Data,
                                    unsigned sector_number)
{
    uint32_t status_reg;
    uint32_t cr;

    cr = FLASH->CR;
    cr &= (CR_PSIZE_MASK & SECTOR_MASK);
    cr |= (FLASH_PSIZE_WORD | FLASH_CR_PG | sector_number);
    FLASH->CR = cr;

    *(__IO uint32_t *)Address = Data;

    // Poll register until complete. Takes around 16 usecs.
    status_reg = FLASH->SR;
    while ((status_reg & FLASH_FLAG_BSY) != 0)
    {
        status_reg = FLASH->SR;
    }

    /* if the program operation is completed, disable the PG Bit */
    cr &= (~FLASH_CR_PG);
    FLASH->CR = cr;

    return status_reg;
}

//!
//! @name      stm_flash_write_doubleword_bank0
//!
//! @brief     8-byte flash write
//!
//! @details   Can only be used for voltage == VoltageRange_4
//! @details   CAUTION note for MAX_WRITE_FCN_SIZE_BYTES applies here!
//!
//! @param[in] 'Address'-- must be aligned to a program page (64-bit)
//! @param[in] 'Data'-- 64-bit value to write. Little-endian write:
//! @param[in]    bits 0:7 at 'Address', bits 63:56 at 'Address'+7
//! @param[in] 'sector_number'-- value of 0-11
//!
//! @return    Register FLASH_SR contents
//!
uint32_t stm_flash_write_doubleword_bank0(uint32_t Address,
                                          uint64_t Data,
                                          unsigned sector_number)
{
    uint32_t status_reg;
    uint32_t cr;

    cr = FLASH->CR;
    cr &= (CR_PSIZE_MASK & SECTOR_MASK);
    cr |= (FLASH_PSIZE_DOUBLE_WORD | FLASH_CR_PG | sector_number);
    FLASH->CR = cr;

    *(__IO uint64_t *)Address = Data;

    // Poll register until complete. Takes around 16 usecs.
    status_reg = FLASH->SR;
    while ((status_reg & FLASH_FLAG_BSY) != 0)
    {
        status_reg = FLASH->SR;
    }

    /* if the program operation is completed, disable the PG Bit */
    cr &= (~FLASH_CR_PG);
    FLASH->CR = cr;

    return status_reg;
}

//!
//! @name      stm_flash_erase_sector_bank0
//!
//! @brief     4-byte flash write
//!
//! @details   
//! @details   CAUTION note for MAX_ERASE_FCN_SIZE_BYTES applies here!
//!
//! @param[in] 'sector_number'-- 0, 1, 2, ...
//! @param[in]        0 is first 16k sector
//! @param[in] 'psize'-- current voltage.
//! @param[in]       'psize' value                  used when...
//! @param[in]    FLASH_PSIZE_BYTE         (for VoltageRange_1 settings)
//! @param[in]    FLASH_PSIZE_HALF_WORD    (for VoltageRange_2 settings or lower)
//! @param[in]    FLASH_PSIZE_WORD         (for VoltageRange_3 settings or lower)
//! @param[in]    FLASH_PSIZE_DOUBLE_WORD  (for VoltageRange_4 settings or lower)
//!
//! @return    Register FLASH_SR contents
//!
uint32_t stm_flash_erase_sector_bank0(uint32_t sector_number, uint32_t psize)
{
    uint32_t status_reg;
    uint32_t cr;

    cr = FLASH->CR;
    cr &= (CR_PSIZE_MASK & SECTOR_MASK);
    cr |= (FLASH_CR_SER | FLASH_CR_STRT | psize | sector_number);
    FLASH->CR = cr;

    // Poll until complete. For a 16k sector, takes around 400 millisecs
    status_reg = FLASH->SR;
    while ((status_reg & FLASH_FLAG_BSY) != 0)
    {
        status_reg = FLASH->SR;
    }

    /* if the erase operation is completed, disable the SER Bit */
    cr = FLASH->CR;
    cr &= (BITWISE_NOT32(FLASH_CR_SER) & SECTOR_MASK);
    FLASH->CR = cr;

    return status_reg;
}

//!
//! @name      stm_flash_page_write_bank0
//!
//! @brief     Write up to 1 flash page worth of data.
//!
//! @details   This fcn. figures out how much can be written, and
//! @details   this is based on alignment of 'address'
//! @details   and number of bytes to be written. It uses
//! @details   address to determine page, then writes
//! @details   all bytes it can fit into that page.
//! @details   Depending on voltage chip is running at, all
//! @details   writes will be at widest bit possible.
//!
//! @param[in] 'address'-- flash address to start write at
//! @param[in] 'data'-- byte array of data to write
//! @param[in] 'length_up_to'-- ideally write this many bytes
//! @param[in]         but expect that we won't be able to
//! @param[out] 'bytes_written_ptr'-- actual num. bytes written
//! @param[in] 'voltage'-- voltage that chip is set to. One of:
//! @param[in]              VoltageRange_1, VoltageRange_2,
//! @param[in]              VoltageRange_3, VoltageRange_4
//!
//! @return    STM_FLASH_SUCCESS or STM_FLASH_FAILURE
//!
stm_flash_status_t stm_flash_page_write_bank0(uint8_t  *address,
                                              uint8_t  *data,
                                              unsigned  length_up_to,
                                              unsigned *bytes_written_ptr,
                                  stm_flash_voltage_t   voltage)
{
    uint8_t     *base_address;
       // Defining 'shadow' and 'set_to" as uint32_t's forces alignment
    uint32_t     shadow[FLASH_PAGE_SIZE/BYTES_PER_WORD32];
    uint32_t     set_to[FLASH_PAGE_SIZE/BYTES_PER_WORD32];
    unsigned     align_offset;
    unsigned     page_remaining_length;
    unsigned     curtailed_length;
    unsigned     i;
    unsigned     sector_number;
    uint32_t     status_reg;

    if (0 == length_up_to)
    {
        *bytes_written_ptr = 0;
        return STM_FLASH_SUCCESS;
    }

    if (!stm_is_valid_address_bank0(address))
    {
        *bytes_written_ptr = 0;
        return STM_FLASH_FAILURE;
    }

    base_address = (uint8_t *)ALIGN64(address);

    // 'shadow' is pre-write contents of page
    // init 'set_to' to pre-write page bytes
    shadow[0] = ((uint32_t *)base_address)[0];
    set_to[0] = shadow[0];
    shadow[1] = ((uint32_t *)base_address)[1];
    set_to[1] = shadow[1];

    align_offset = (uint32_t)(address - base_address);
    page_remaining_length = FLASH_PAGE_SIZE - align_offset;

    // 'curtailed_length' is number of bytes that we can
    // write on 1 page.
    if (page_remaining_length <= length_up_to)
    {
        curtailed_length = page_remaining_length;
    }
    else
    {
        curtailed_length = length_up_to;
    }

    // 'set_to' is view of page contents after write
    // poke bytes to be written, leave other as it is
    rutils_memcpy( &((uint8_t *)set_to)[align_offset], data, curtailed_length);

    // From API's point of view, we're writing 'curtailed_length' bytes
    *bytes_written_ptr = curtailed_length;

    // If the flash op causes no change, then no need to do it
    if ((shadow[0] == set_to[0]) && (shadow[1] == set_to[1]))
    {
        return STM_FLASH_SUCCESS;
    }

    sector_number = stm_address_to_sector_number(base_address);

    // With 'STM_FLASH_VOLTAGE_1', only byte writes are allowed.
    // With 'STM_FLASH_VOLTAGE_2', both byte and halfword writes are allowed,
    //     but to make things simpler, just be halfwords, no bytes
    // Same for other ranges

    if (STM_FLASH_VOLTAGE_1 == voltage)
    {
        APP_REQUIRE(NULL != stm_flash_write_byte_bank0_ptr);

        for (i = 0; i < FLASH_PAGE_SIZE; i++)
        {
            // Case where we didn't poke into 'set_to' with
            //    write data.
            if (((uint8_t *)shadow)[i] != ((uint8_t *)set_to)[i])
            {
                // Since code is running out of bank 0, same
                // bank we're changing flash on, we can't
                // do code fetches while writing. Disable
                // interrupts to enforce this, then call
                // a fcn. which lives in ram.

                stm_interrupt_disable();

                status_reg = (*stm_flash_write_byte_bank0_ptr)(
                                                   (uint32_t)&base_address[i],
                                                   ((uint8_t *)set_to)[i],
                                                   sector_number);

                stm_interrupt_enable();

                if (!FLASH_STATUS_OK(status_reg))
                {
                    APP_ENSURE(false);
                    return STM_FLASH_FAILURE;
                }
            }
        }
    }

    else if (STM_FLASH_VOLTAGE_2 == voltage)
    {
        APP_REQUIRE(NULL != stm_flash_write_halfword_bank0_ptr);

        for (i = 0; i < FLASH_PAGE_SIZE/BYTES_PER_WORD16; i++)
        {
            if (((uint16_t *)shadow)[i] != ((uint16_t *)set_to)[i])
            {
                stm_interrupt_disable();

                status_reg = (*stm_flash_write_halfword_bank0_ptr)(
                                             (uint32_t)&base_address[i*sizeof(uint16_t)],
                                             ((uint16_t *)set_to)[i],
                                             sector_number);
                stm_interrupt_enable();

                if (!FLASH_STATUS_OK(status_reg))
                {
                    APP_ENSURE(false);
                    return STM_FLASH_FAILURE;
                }
            }
        }
    }

    else if (STM_FLASH_VOLTAGE_3 == voltage)
    {
        APP_REQUIRE(NULL != stm_flash_write_word_bank0_ptr);

        for (i = 0; i < FLASH_PAGE_SIZE/BYTES_PER_WORD32; i++)
        {
            if (shadow[i] != set_to[i])
            {
                stm_interrupt_disable();

                status_reg = (*stm_flash_write_word_bank0_ptr)(
                                             (uint32_t)&base_address[i*sizeof(uint32_t)],
                                             set_to[i],
                                             sector_number);
                stm_interrupt_enable();

                if (!FLASH_STATUS_OK(status_reg))
                {
                    APP_ENSURE(false);
                    return STM_FLASH_FAILURE;
                }
            }
        }
    }

    else if (STM_FLASH_VOLTAGE_4 == voltage)
    {
        APP_REQUIRE(NULL != stm_flash_write_doubleword_bank0_ptr);

        stm_interrupt_disable();

// gcc doesn't like casting of "*(uint64_t *)set_to" 
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
        status_reg = (*stm_flash_write_doubleword_bank0_ptr)(
                                         (uint32_t)base_address,
                                         *(uint64_t *)set_to,
                                         sector_number);
#pragma GCC diagnostic pop

        stm_interrupt_enable();

        if (!FLASH_STATUS_OK(status_reg))
        {
            APP_ENSURE(false);
            return STM_FLASH_FAILURE;
        }
    }

    // Illegal 'voltage' parm
    else
    {
        APP_REQUIRE(false);
        return STM_FLASH_FAILURE;
    }

    // Reload 'shadow' with newly written data
    shadow[0] = ((uint32_t *)base_address)[0];
    shadow[1] = ((uint32_t *)base_address)[1];

    // Verify that new contents are as expected
    if ((shadow[0] != set_to[0]) ||  (shadow[1] != set_to[1]))
    {
        return STM_FLASH_FAILURE;
    }

    return STM_FLASH_SUCCESS;
}

//******************************************************************************
// APIs
//******************************************************************************

//!
//! @name      stm_flash_init
//!
//! @brief     Init this module. Must call (once) before using.
//!
//! @details   Establishes voltage level settings.
//! @details   Copies write, erase fcns. from flash to RAM.
//!
//! @param[in] 'voltage_level'-- Voltage chip is running at.
//! @param[in]    NOTE: 
//!
void stm_flash_init(stm_flash_voltage_t voltage_level)
{
    stm_voltage_level = stm_voltage_level_to_internal(voltage_level);

    stm_unlocked = false;

    // If running single banked flash, need to copy all of the FLASH_
    // fcns. to ram, so we can execute out of there

    // Initialize fcn. pointers
    stm_flash_write_byte_bank0_ptr = NULL;
    stm_flash_write_halfword_bank0_ptr = NULL;
    stm_flash_write_word_bank0_ptr = NULL;
    stm_flash_write_doubleword_bank0_ptr = NULL;

    stm_flash_erase_bank0_ptr = VOID_PTR_TO_FCN_PTR(stm_erase_fcn_ram);

    // Copy fcns. up into ram.
    // So what if we copy too much?
    if (STM_FLASH_VOLTAGE_1 == voltage_level)
    {
        rutils_memcpy(&stm_write_fcn_ram,
                      FCN_TO_VOID_PTR(stm_flash_write_byte_bank0),
                      MAX_WRITE_FCN_SIZE_BYTES);
        stm_flash_write_byte_bank0_ptr = VOID_PTR_TO_FCN_PTR(stm_write_fcn_ram);
    }
    else if (STM_FLASH_VOLTAGE_2 == voltage_level)
    {
        rutils_memcpy(&stm_write_fcn_ram,
                      FCN_TO_VOID_PTR(stm_flash_write_halfword_bank0),
                      MAX_WRITE_FCN_SIZE_BYTES);
        stm_flash_write_halfword_bank0_ptr = VOID_PTR_TO_FCN_PTR(stm_write_fcn_ram);
    }
    else if (STM_FLASH_VOLTAGE_3 == voltage_level)
    {
        rutils_memcpy(&stm_write_fcn_ram,
                      FCN_TO_VOID_PTR(stm_flash_write_word_bank0),
                      MAX_WRITE_FCN_SIZE_BYTES);
        stm_flash_write_word_bank0_ptr = VOID_PTR_TO_FCN_PTR(stm_write_fcn_ram);
    }
    else if (STM_FLASH_VOLTAGE_4 == voltage_level)
    {
        rutils_memcpy(&stm_write_fcn_ram,
                      FCN_TO_VOID_PTR(stm_flash_write_doubleword_bank0),
                      MAX_WRITE_FCN_SIZE_BYTES);
        stm_flash_write_doubleword_bank0_ptr = VOID_PTR_TO_FCN_PTR(stm_write_fcn_ram);
    }
    else
    {
        APP_REQUIRE(false);
    }

    rutils_memcpy(&stm_erase_fcn_ram,
                  FCN_TO_VOID_PTR(stm_flash_erase_sector_bank0),
                  MAX_ERASE_FCN_SIZE_BYTES);

}

//!
//! @name      stm_flash_write
//!
//! @brief     Write string to STM flash
//!
//! @details   Interrupts will be disabled in 16usec (or so) chunks.
//!
//! @return    STM_FLASH_SUCCESS or STM_FLASH_FAILURE
//!
stm_flash_status_t stm_flash_write(uint8_t *address,
                                   uint8_t *data,
                                   unsigned data_length)
{
    stm_flash_status_t status;
    unsigned           bytes_written;

    // Sanity check to prevent crash
    if (stm_are_interrupts_disabled())
    {
        APP_REQUIRE(false);
        return STM_FLASH_FAILURE;
    }

    // Make sure all writes are within flash bank 0address space
    if (!stm_is_valid_address_bank0(address) ||
        ((data_length > 0)
        && !stm_is_valid_address_bank0(address + data_length - 1)))
    {
        APP_REQUIRE(false);
        return STM_FLASH_FAILURE;
    }

    stm_interrupt_disable();

    // Have we unlocked flash ops at any time? No--do it this once.
    if (!stm_unlocked)
    {
        stm_flash_unlock();
        stm_unlocked = true;
    }

    stm_interrupt_enable();

    // Step through writes according to pages.
    while (data_length > 0)
    {
        bytes_written = 0;

        status = stm_flash_page_write_bank0(address,
                                            data,
                                            data_length,
                                            &bytes_written,
                                            stm_voltage_level);
        if (STM_FLASH_SUCCESS != status)
        {
            return status;
        }

        // TBD: should we attempt a 2nd write if 1st fails?

        // Sanity check: let's not get stuck in a loop
        if (bytes_written == 0)
        {
            APP_ENSURE(bytes_written > 0);
            return STM_FLASH_FAILURE;
        }

        address += bytes_written;
        data += bytes_written;
        data_length -= bytes_written;
    }

    return STM_FLASH_SUCCESS;
}

//!
//! @name      stm_flash_erase
//!
//! @brief     Erase an STM flash sector
//!
//! @details   Interrupts will be disabled from 1/2 sec to
//! @details   several seconds during erasure.
//! @details   Use with discretion.
//!
//! @param[in] 'bank'- either 0 or 1 (1 is called "bank 2" in STM literature)
//! @param[in] 'sector_number'--
//!
//! @return    STM_FLASH_SUCCESS or STM_FLASH_FAILURE
//!
stm_flash_status_t stm_flash_erase(unsigned bank, uint16_t sector_number)
{
    uint32_t     status_reg;
    unsigned     psize;

    // Sanity check
    //if (bank0 > 1)
    // 2nd bank not supported yet
    if (bank > 0)
    {
        return STM_FLASH_FAILURE;
    }

    // Sanity check sector number
    if (sector_number >= ARRAY_SIZE(stm_layout))
    {
        APP_REQUIRE(false);
        return STM_FLASH_FAILURE;
    }

    // Convert to FLASH CR bits SNB, bits 6:3
    sector_number <<= 3;

    // Sanity check to prevent crash
    if (stm_are_interrupts_disabled())
    {
        APP_REQUIRE(false);
        return STM_FLASH_FAILURE;
    }

    // Make sure fcn. ptr is initialized
    APP_REQUIRE(NULL != stm_flash_erase_bank0_ptr);

    stm_interrupt_disable();

    // Have we unlocked flash ops at any time? No--do it this once.
    if (!stm_unlocked)
    {
        stm_flash_unlock();
        stm_unlocked = true;
    }

    stm_interrupt_enable();

    psize = stm_voltage_level_to_psize(stm_voltage_level);

    if (0 == bank)
    {
        // Have to lock interrupts, no choice
        // Get yourself a cup of coffee and a sandwich...
        stm_interrupt_disable();

        status_reg = (*stm_flash_erase_bank0_ptr)(sector_number, psize);

        stm_interrupt_enable();
    }
    else
    {
        // 2nd bank not supported yet
    }

    if (!FLASH_STATUS_OK(status_reg))
    {
        //TBD: check status, etc

        APP_ENSURE(false);
        return STM_FLASH_FAILURE;
    }

    return STM_FLASH_SUCCESS;
}
