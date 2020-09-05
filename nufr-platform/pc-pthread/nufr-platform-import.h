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

//! @file     nufr-platform-import.h
//! @authors  Bernie Woodland
//! @date     19Feb18
//!
//! @brief   NUFR settings which need to be imported from the startup/CPU/BSP
//!
//! @details There are certain NUFR settings which must be defined in the
//! @details startup code. This file contains allows these to be brought
//! @details into NUFR. The file works by having standard import macros,
//! @details prototypes, etc. that are filled out here.

#ifndef NUFR_PLATFORM_IMPORT_H
#define NUFR_PLATFORM_IMPORT_H

#include <stdint.h>

//!
//! @brief  ARM Cortex Mx imports
//!

// Type sized to ARM Cortex Mx register size
#define _IMPORT_REGISTER_TYPE      uint32_t
#define _IMPORT_STATUS_REG_TYPE    uint32_t

//#define _IMPORT_INTERRUPT_LOCK         int_lock
//#define _IMPORT_INTERRUPT_UNLOCK(y)    int_unlock(y)


//  Clock rate (Hz)
#define _IMPORT_CPU_CLOCK_SPEED      8000000

//! @brief   PendSV trigger  
//! @details http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0497a/Cihfaaha.html
//!
#define _IMPORT_PENDSV_ACTIVATE  *(volatile uint32_t *)(0xE000ED04) = 0x10000000

#define _IMPORT_PREPARE_STACK      Prepare_Stack

// For task launching
typedef struct
{
    _IMPORT_REGISTER_TYPE  *stack_base_ptr;
    _IMPORT_REGISTER_TYPE  **stack_ptr_ptr;
    unsigned   stack_length_in_bytes;
    void       (*entry_point_fcn_ptr)(unsigned);
    void       (*exit_point_fcn_ptr)(void);
    unsigned   entry_parameter;
} _IMPORT_stack_specifier_t;

void Prepare_Stack(_IMPORT_stack_specifier_t *ptr);

#endif  //NUFR_PLATFORM_IMPORT_H