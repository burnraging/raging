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

//! @file     nufr-platform.h
//! @authors  Bernie Woodland
//! @date     7Aug17
//!
//! @brief   Mandatory and optional platform extensions to the nufr kernel.
//!
//! @details The "mandatory" extensions are functions, variables, definitions,
//! @details etc that the kernel compiles against.
//! @details The platform part of the kernel also allows customization, to
//! @details scale nufr up or down, according to the needs of the project.

#ifndef NUFR_PLATFORM_H
#define NUFR_PLATFORM_H

#include "nufr-global.h"

#include "nufr-kernel-base-task.h"
#include "nufr-kernel-base-messaging.h"
#include "nufr-kernel-base-semaphore.h"

#include "nufr-platform-app.h"

#include "nufr-platform-import.h"

//!
//! @brief  period in milliseconds of OS tick
//!
#define NUFR_TICK_PERIOD                10


//!
//! @def      NUFR_INVOKE_CONTEXT_SWITCH
//!
//! @brief    Write to ICSR register invokes PendSV by setting "set bit"
//!
//! @details  For CPUs which don't have a software interrupt (like MSP430),
//! @details  this will just set a flag.
//!
#define NUFR_INVOKE_CONTEXT_SWITCH()     _IMPORT_PENDSV_ACTIVATE

//!
//! @def      NUFR_SECONDARY_CONTEXT_SWITCH
//!
//! @brief    Alternate means of doing a context switch, for CPUs which
//! @brief    don't have software interrupts.
//!
#define NUFR_SECONDARY_CONTEXT_SWITCH()  _IMPORT_ALT_CONTEXT_SWITCH

//!
//! @def      NUFR_SYSTICK_PREPROCESSING
//!
//! @brief    For CPUs that might need it, like MSP430
//!
#define NUFR_SYSTICK_PREPROCESSING()     _IMPORT_SYSTICK_PREPROCESSING

//!
//! @def      NUFR_SYSTICK_POSTPROCESSING
//!
//! @brief    For CPUs that might need it, like MSP430
//!
#define NUFR_SYSTICK_POSTPROCESSING()    _IMPORT_SYSTICK_POSTPROCESSING

//!
//! @def      NUFR_LOCK_INTERRUPTS
//! @def      NUFR_UNLOCK_INTERRUPTS
//!
//! @brief    For UT, just monitor counts, to detect a mismatch in
//! @brief    lock/unlock pairs.
//!

typedef _IMPORT_REGISTER_TYPE   nufr_register_t;     
typedef _IMPORT_STATUS_REG_TYPE nufr_sr_reg_t;     
  
#define NUFR_LOCK_INTERRUPTS            _IMPORT_INTERRUPT_LOCK
#define NUFR_UNLOCK_INTERRUPTS(x)       _IMPORT_INTERRUPT_UNLOCK(x)

// APIs
RAGING_EXTERN_C_START
void nufr_init(void);
const nufr_task_desc_t *nufrplat_task_get_desc(nufr_tcb_t *tcb,
                                               nufr_tid_t tid);
void nufrplat_task_exit_point(void);
RAGING_EXTERN_C_END

#endif  //NUFR_PLATFORM_H