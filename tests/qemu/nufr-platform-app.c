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

//! @file     nufr-platform-app.c
//! @authors  Bernie Woodland
//! @date     30Sep17
//!
//! @brief   Application-specific platform settings.
//!
//! @details Specification of task stacks, task entry points, and
//! @details task priorities.

#define NUFR_PLAT_APP_GLOBAL_DEFS

#include "nufr-global.h"
#include "nufr-api.h"
#include "nufr-platform.h"
#include "nufr-platform-export.h"
#include "nufr-platform-app.h"

//! @brief        Background Task
uint32_t Bg_Stack[BG_STACK_SIZE / BYTES_PER_WORD32];

// Sleeper
#if QEMU_PROJECT == 1

//!
//!  @brief       Task stacks
//!
#define STACK_SIZE 512
uint32_t Stack_01[STACK_SIZE / BYTES_PER_WORD32];
uint32_t Stack_02[STACK_SIZE / BYTES_PER_WORD32];
uint32_t Stack_03[STACK_SIZE / BYTES_PER_WORD32];
uint32_t Stack_04[STACK_SIZE / BYTES_PER_WORD32];
uint32_t Stack_05[STACK_SIZE / BYTES_PER_WORD32];

//!
//!  @brief       Task descriptors
//!
const nufr_task_desc_t nufr_task_desc[NUFR_NUM_TASKS] = {
    {"task 01", entry_01, Stack_01, STACK_SIZE, NUFR_TPR_HIGHER, 0},
    {"task 02", entry_02, Stack_02, STACK_SIZE, NUFR_TPR_HIGHER, 0},
    {"task 03", entry_03, Stack_03, STACK_SIZE, NUFR_TPR_HIGHER, 0},
    {"task 04", entry_04, Stack_04, STACK_SIZE, NUFR_TPR_HIGHER, 0},
    {"task 05", entry_05, Stack_05, STACK_SIZE, NUFR_TPR_HIGHER, 0},
};


// messager
#elif QEMU_PROJECT == 2

//!
//!  @brief       Task stacks
//!
// Stack_event_task uses about 172 out of 256 bytes when compiled
// in non-optimized mode.
#define STACK_SIZE 256
uint32_t Stack_01[STACK_SIZE / BYTES_PER_WORD32];
uint32_t Stack_event_task[STACK_SIZE / BYTES_PER_WORD32];
uint32_t Stack_state_task[STACK_SIZE / BYTES_PER_WORD32];


//!
//!  @brief       Task descriptors
//!
const nufr_task_desc_t nufr_task_desc[NUFR_NUM_TASKS] = {
    {"task 01", entry_01, Stack_01, STACK_SIZE, NUFR_TPR_HIGH, 0},
    {"task 02", entry_event_task, Stack_event_task, STACK_SIZE, NUFR_TPR_NOMINAL, 0},
    {"task 03", entry_state_task, Stack_state_task, STACK_SIZE, NUFR_TPR_NOMINAL, 0},
};

#endif