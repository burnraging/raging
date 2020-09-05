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

//! @file     nufr-platform-export.h
//! @authors  Bernie Woodland
//! @date     30Jul18
//!
//! @brief   NUFR settings which need to be exported to the startup/CPU/BSP
//!
//! @details Single file that is to be included wherever these defines are
//! @details needed. They're needed where the OS Tick is configured in H/W,
//! @details and where the main()/BG Task stack is defined
//! @details 

#ifndef NUFR_PLATFORM_EXPORT_H
#define NUFR_PLATFORM_EXPORT_H

#include <stdint.h>
#include <raging-global.h>

#include "msp430-assembler.h"

//!
//! @brief  period in milliseconds of OS tick
//!
#define NUFR_TICK_PERIOD                10

//!
//! @brief        Background Task Size (bytes)
//!
#define BG_STACK_SIZE                  256u

//!
//! @brief        Background Task
//!
#ifndef NUFR_PLAT_APP_GLOBAL_DEFS
    extern uint16_t Bg_Stack[BG_STACK_SIZE / BYTES_PER_WORD16];
#endif

//!
//! @name         nufrplat_systick_handler()
//!
//! @brief        NUFR OS clock tick callin fcn.
//!
//! @param[in]    sp-- Stack Pointer register value set by wrapper. See asm code.
//!
void nufrplat_systick_handler(void);

#endif  //NUFR_PLATFORM_EXPORT_H