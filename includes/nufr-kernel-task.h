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

//! @file     nufr-kernel-task.h
//! @authors  Bernie Woodland
//! @date     7Aug17
//!
//! @brief   Task-related definitions that are only exported
//! @brief   to nufr platform and nufr kernel, but not to app layers
//!
//! @details 

#ifndef NUFR_KERNEL_TASK_H
#define NUFR_KERNEL_TASK_H

#include "nufr-global.h"
#include "nufr-kernel-base-task.h"
#include "nufr-platform.h"

#define NUFR_TID_TO_TCB(x)   ( &nufr_tcb_block[(x) - 1] )
#define NUFR_TCB_TO_TID(y)   ( (nufr_tid_t) ((unsigned)((y) - &nufr_tcb_block[0]) + 1) )

#define NUFR_IS_TCB(x)       ( ((x) >= nufr_tcb_block) &&                  \
                               ((x) <= &nufr_tcb_block[NUFR_NUM_TASKS - 1]) )

#ifndef NUFR_TASK_GLOBAL_DEFS
extern nufr_tcb_t nufr_tcb_block[NUFR_NUM_TASKS];

extern nufr_tcb_t *nufr_running;
extern nufr_tcb_t *nufr_ready_list;
extern nufr_tcb_t *nufr_ready_list_tail_nominal;
extern nufr_tcb_t *nufr_ready_list_tail;
extern unsigned *nufr_bg_sp[NUFR_SP_INDEX_IN_TCB + 1];
extern uint16_t nufr_bop_key;

// fixme (if possible): put here to prevent instead of in nufr-platform-import.h
//  to prevent circular include problem
#ifdef USING_MSP430_CONTEXT_ASSIST_FILE
    #include "msp430-context-assist.h"
#endif

#endif

//  APIs
RAGING_EXTERN_C_START
bool nufrkernel_add_task_to_ready_list(nufr_tcb_t *tcb);
void nufrkernel_block_running_task(unsigned block_flag);
void nufrkernel_remove_head_task_from_ready_list(void);
void nufrkernel_delete_task_from_ready_list(nufr_tcb_t *tcb);
void nufrkernel_exit_running_task(void);
RAGING_EXTERN_C_END

#endif  //NUFR_KERNEL_TASK_H