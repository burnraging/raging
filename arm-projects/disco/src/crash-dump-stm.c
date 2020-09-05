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

//! @file     crash-dump-stm.c
//! @authors  Bernie Woodland
//! @date     24May19
//!
//! @brief    STM32F4xx series-specific crash dump save to FLASH
//!


#include "raging-global.h"
#include "nvm-stm32f4xx.h"

#include <system.h>
#include <bsp.h>

//-----------------    Defines   ----------------------

#define CODE_HARD_FAULT           1
#define CODE_MEM_MANAGER_FAULT    2
#define CODE_BUS_FAULT            3
#define CODE_USAGE_FAULT          4

#define CRASH_DUMP_FLASH_ADDRESS  ((uint8_t *)0x800C000)
#define CRASH_DUMP_CAPACITY         (16*1024)                // sector size

#define NVIC_BASE_ADDRESS         0xE000E000

#define READ_NVIC_REG(offset)   (*(volatile uint32_t *)(NVIC_BASE_ADDRESS + (offset)))

//-----------------    Local Structs & Enums    ----------------------

// Don't leave any gaps/bytes between variables
// Keep 4-byte sized
typedef struct
{
    uint8_t  fault_type;
    uint8_t  was_from_exc;
    uint8_t  was_from_process;
    uint8_t  unused1;
    uint32_t fault_frame_sp;

    // NVIC
    uint32_t nvic_abr0;        // 0x300 Active Bit Register 0
    uint32_t nvic_abr1;        // 0x304                     1
    uint32_t nvic_abr2;        // 0x308                     2
    uint32_t nvic_abr3;        // 0x30C                     3
    uint32_t nvic_hfsr;        // 0xD2C Hard Fault Status Reg
    uint32_t nvic_mmfar;       // 0xD34 Mem Mgmt Fault Addr Reg
    uint32_t nvic_bfar;        // 0xD38 Bus Fault Addr Reg
    uint32_t nvic_afst;        // 0xD3C Aux Fault Status Reg
} crash_data_t;

crash_data_t crash_data;

//-----------------    Global Variables  ----------------------

unsigned hard_fault_count;
unsigned memmanage_fault_count;
unsigned bus_fault_count;
unsigned usage_fault_count;


/* start of .data in the linker script */
extern unsigned __data_start;

/* end of .data in the linker script */
extern unsigned __data_end__;

/* initialization values for .data  */
extern unsigned const __data_load;

/* start of .bss in the linker script */
extern unsigned __bss_start__;

/* end of .bss in the linker script */
extern unsigned __bss_end__;

// Hacked fcn. declarations
void bsp_led_toggle(bsp_leds_t led);

//!
//! @name      crash_handler
//!
//! @brief     All crashes get directed here
//!
//! @details   Stores off all of RAM in use to a
//! @details   dedicated FLASH location.
//!
//! @param[in] 'code'-- Where were we called from?
//! @param[in]     CODE_HARD_FAULT(1) = hard fault exc.
//! @param[in]     CODE_MEM_MANAGER_FAULT(2) = mem mgr fault exc.
//! @param[in]     CODE_BUS_FAULT(3) = bus fault exc.
//! @param[in]     CODE_USAGE_FAULT(4) = usage fautl exc.
//! @param[in]  'sp'-- Stack pointer as entered fault handler
//! @param[in]  'lr'-- Link register as entered fault handler
//! @param[in]     0xFFFFFFFD if fault happened in process mode
//! @param[in]     0xFFFFFFF1 if fault happened while handling
//! @param[in]                an exc./interrupt
//!
void crash_handler(uint32_t code, uint32_t sp, uint32_t lr)
{
    uint8_t *sector_addr = CRASH_DUMP_FLASH_ADDRESS;
    uint8_t *write_addr;
    bool     is_ff = true;
    uint32_t length;
    uint32_t truncated_length;
    unsigned remaining_size;
    unsigned i;

    // Turn off all interrupts
    __asm volatile(
        "  cpsid i      "                     // set bit 0 of PRIMASK
        : );

    if (CODE_HARD_FAULT == code)
    {
        hard_fault_count++;
    }
    else if (CODE_MEM_MANAGER_FAULT == code)
    {
        memmanage_fault_count++;
    }
    else if (CODE_BUS_FAULT == code)
    {
        bus_fault_count++;
    }
    else if (CODE_USAGE_FAULT == code)
    {
        usage_fault_count++;
    }
    else
    {
        // should never get here
    }

    // Save off critical registers, etc.
    crash_data.fault_type = code;
    crash_data.fault_frame_sp = sp;
    crash_data.was_from_exc = 0xFFFFFFF1 == lr;
    crash_data.was_from_process = 0xFFFFFFFD == lr;
    crash_data.fault_frame_sp = sp;

    crash_data.nvic_abr0 = READ_NVIC_REG(0x300);
    crash_data.nvic_abr1 = READ_NVIC_REG(0x304);
    crash_data.nvic_abr2 = READ_NVIC_REG(0x308);
    crash_data.nvic_abr3 = READ_NVIC_REG(0x30C);
    crash_data.nvic_hfsr = READ_NVIC_REG(0xD2C);
    crash_data.nvic_mmfar = READ_NVIC_REG(0xD34);
    crash_data.nvic_bfar = READ_NVIC_REG(0xD38);
    crash_data.nvic_afst = READ_NVIC_REG(0xD3C);

    // Check first so many bytes if there's a previous dump already
    for (i = 0; i < 1000; i++)
    {
        is_ff &= sector_addr[i];
    }

    // Wasn't? Write all of used RAM to FLASH
    if (is_ff)
    {
        stm_flash_init(STM_FLASH_VOLTAGE_3);

        write_addr = sector_addr;

        // Write BSS size
        length = (unsigned) ((uint8_t *)&__bss_end__ - (uint8_t *)&__bss_start__);
        stm_flash_write(write_addr, (uint8_t *)&length, sizeof(length));
        write_addr += sizeof(length);
        remaining_size = CRASH_DUMP_CAPACITY - (unsigned)(write_addr - sector_addr);

        if (length <= remaining_size)
        {
            truncated_length = length;
        }
        else
        {
            truncated_length = remaining_size;
        }

        // Write BSS section
        stm_flash_write(write_addr, (uint8_t *)&__bss_start__, truncated_length);
        write_addr += truncated_length;
        write_addr = (uint8_t *)ALIGNUP32(write_addr);
        remaining_size = CRASH_DUMP_CAPACITY - (unsigned)(write_addr - sector_addr);

        // Write Data size
        length = (unsigned) ((uint8_t *)&__data_end__ - (uint8_t *)&__data_start);
        if (remaining_size >= sizeof(length))
        {
            stm_flash_write(write_addr, (uint8_t *)&length, sizeof(length));
            write_addr += sizeof(length);
            remaining_size = CRASH_DUMP_CAPACITY - (unsigned)(write_addr - sector_addr);
        }

        // Write Data section
        if (length <= remaining_size)
        {
            truncated_length = length;
        }
        else
        {
            truncated_length = remaining_size;
        }

        stm_flash_write(write_addr, (uint8_t *)&__data_start, truncated_length);
        write_addr += length;
        write_addr = (uint8_t *)ALIGN32(write_addr);
        remaining_size = CRASH_DUMP_CAPACITY - (unsigned)(write_addr - sector_addr);
    }

    /* Go to infinite loop */
    while (1)
    {
        bsp_led_toggle(Blue_Led);
    }
}
