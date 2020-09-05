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

#include "base-task.h"
#include "low-task.h"

//! @brief        Background Task
uint32_t Bg_Stack[BG_STACK_SIZE / BYTES_PER_WORD32];

//!
//!  @brief       Task stacks
//!
#define STACK_SIZE 384
uint32_t Stack_base_task[STACK_SIZE / BYTES_PER_WORD32];
uint32_t Stack_low_task[STACK_SIZE / BYTES_PER_WORD32];


//!
//!  @brief       Task descriptors
//!
const nufr_task_desc_t nufr_task_desc[NUFR_NUM_TASKS] = {
    {"Base Task",          entry_base_task,          Stack_base_task,          STACK_SIZE, NUFR_TPR_NOMINAL, 0},
    {"Low Task",           entry_low_task,           Stack_low_task,           STACK_SIZE, NUFR_TPR_LOWER,   0},
};