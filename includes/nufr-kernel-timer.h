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

//! @file    nufr-kernel-timer.h
//! @authors Bernie Woodland
//! @date    11Aug17

#ifndef NUFR_KERNEL_TIMER_H
#define NUFR_KERNEL_TIMER_H

#include "nufr-global.h"
#include "nufr-kernel-base-task.h"

#ifndef NUFR_TIMER_GLOBAL_DEFS
extern nufr_tcb_t *nufr_timer_list;
extern nufr_tcb_t *nufr_timer_list_tail;
#endif  //NUFR_TIMER_GLOBAL_DEFS

RAGING_EXTERN_C_START
void nufrkernel_update_task_timers(void);
void nufrkernel_add_to_timer_list(nufr_tcb_t *task,
                                  uint32_t    initial_timer_value);
bool nufrkernel_purge_from_timer_list(nufr_tcb_t *task);
RAGING_EXTERN_C_END

#endif  //NUFR_KERNEL_TIMER_H