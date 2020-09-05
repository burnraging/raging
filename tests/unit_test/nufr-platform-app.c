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

#include "nufr-platform-app.h"

//!
//!  @brief       Task stacks
//!
#define STACK_SIZE   100
uint32_t Stack_01[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_02[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_03[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_04[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_05[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_06[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_07[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_08[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_09[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_10[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_11[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_12[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_13[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_14[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_15[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_16[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_17[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_18[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_19[STACK_SIZE/BYTES_PER_WORD32];
uint32_t Stack_20[STACK_SIZE/BYTES_PER_WORD32];

//!
//!  @brief       Dummy entry points
//!
void entry_01(unsigned parm)
{
    UNUSED(parm);
}
void entry_02(unsigned parm)
{
    UNUSED(parm);
}
void entry_03(unsigned parm)
{
    UNUSED(parm);
}
void entry_04(unsigned parm)
{
    UNUSED(parm);
}
void entry_05(unsigned parm)
{
    UNUSED(parm);
}
void entry_06(unsigned parm)
{
    UNUSED(parm);
}
void entry_07(unsigned parm)
{
    UNUSED(parm);
}
void entry_08(unsigned parm)
{
    UNUSED(parm);
}
void entry_09(unsigned parm)
{
    UNUSED(parm);
}
void entry_10(unsigned parm)
{
    UNUSED(parm);
}
void entry_11(unsigned parm)
{
    UNUSED(parm);
}
void entry_12(unsigned parm)
{
    UNUSED(parm);
}
void entry_13(unsigned parm)
{
    UNUSED(parm);
}
void entry_14(unsigned parm)
{
    UNUSED(parm);
}
void entry_15(unsigned parm)
{
    UNUSED(parm);
}
void entry_16(unsigned parm)
{
    UNUSED(parm);
}
void entry_17(unsigned parm)
{
    UNUSED(parm);
}
void entry_18(unsigned parm)
{
    UNUSED(parm);
}
void entry_19(unsigned parm)
{
    UNUSED(parm);
}
void entry_20(unsigned parm)
{
    UNUSED(parm);
}


//!
//!  @brief       Task descriptors
//!
const nufr_task_desc_t nufr_task_desc[NUFR_NUM_TASKS] = {
    {"task 01", entry_01, Stack_01, STACK_SIZE, NUFR_TPR_HIGHEST, 0},
    {"task 02", entry_02, Stack_02, STACK_SIZE, NUFR_TPR_HIGHEST, 0},
    {"task 03", entry_03, Stack_03, STACK_SIZE, NUFR_TPR_HIGHER, 0},
    {"task 04", entry_04, Stack_04, STACK_SIZE, NUFR_TPR_HIGHER, 0},
    {"task 05", entry_05, Stack_05, STACK_SIZE, NUFR_TPR_HIGH, 0},
    {"task 06", entry_06, Stack_06, STACK_SIZE, NUFR_TPR_HIGH, 0},
    {"task 07", entry_07, Stack_07, STACK_SIZE, NUFR_TPR_NOMINAL, 0},
    {"task 08", entry_08, Stack_08, STACK_SIZE, NUFR_TPR_NOMINAL, 0},
    {"task 09", entry_09, Stack_09, STACK_SIZE, NUFR_TPR_NOMINAL, 0},
    {"task 10", entry_10, Stack_10, STACK_SIZE, NUFR_TPR_NOMINAL, 0},
    {"task 11", entry_11, Stack_11, STACK_SIZE, NUFR_TPR_NOMINAL, 0},
    {"task 12", entry_12, Stack_12, STACK_SIZE, NUFR_TPR_NOMINAL, 0},
    {"task 13", entry_13, Stack_13, STACK_SIZE, NUFR_TPR_NOMINAL, 0},
    {"task 14", entry_14, Stack_14, STACK_SIZE, NUFR_TPR_NOMINAL, 0},
    {"task 15", entry_15, Stack_15, STACK_SIZE, NUFR_TPR_LOW, 0},
    {"task 16", entry_16, Stack_16, STACK_SIZE, NUFR_TPR_LOW, 0},
    {"task 17", entry_17, Stack_17, STACK_SIZE, NUFR_TPR_LOWER, 0},
    {"task 18", entry_18, Stack_18, STACK_SIZE, NUFR_TPR_LOWER, 0},
    {"task 19", entry_19, Stack_19, STACK_SIZE, NUFR_TPR_LOWEST, 0},
    {"task 20", entry_20, Stack_20, STACK_SIZE, NUFR_TPR_LOWEST, 0},
};
