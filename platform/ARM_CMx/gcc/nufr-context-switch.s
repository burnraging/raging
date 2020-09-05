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

@//! @file    nufr-context-switch.s
@//! @authors Chris Martin, Bernie Woodland
@//! @date    5Aug17
@
@ PendSV handler


/******************************************************************************
 *
 *      Assembly Prologue
 *
 *****************************************************************************/

#if defined(__GNUC__)

.syntax unified
.cpu cortex-m3
.thumb

.extern nufr_running
.extern nufr_ready_list

#elif defined(__clang__)

.syntax unified
.cpu cortex-m3
.thumb

#else
#error Unknown Compiler for Cortex-m3
#endif

#include "nufr-kernel-base-task.h"



@//! @name      nufr_context_switch
@
@//! @brief     Switches the "in-task" task in and the "out-task" out.
@//! @brief     On ARM Cortex CPU, this is done in PendSV handler
@//
@//! param[in]  global 'nufr_running': in-task
@//! param[in]  global 'nufr_ready_list': out-task
@
@//! param[out] global 'nufr_running': set to 'nufr_ready_list' afterwards


         .extern   nufr_ready_list

        @   These are IAR specific, do not belong in __GNUC__ or __clang__
         @RSEG CODE:CODE:NOROOT(2)    
         @SECTION   SWITAB:CODE:NOROOT(2)
         @SECTION   .text:CODE:NOROOT(2)


         .global  nufr_context_switch
         .type nufr_context_switch, %function   @ instructs compiler to set bit 0 to 1 (use this as a function)
nufr_context_switch:

@ Load R0, R1 with pointer to pointer to TCB
         LDR    R0, =nufr_running
         LDR    R1, =nufr_ready_list

@ Disable interrupts:
@  an ISR can at any time preempt PendSV and therein
@  change 'nufr_ready_list' value.
@ Not saving old BASEPRI. No chance of nesting.
@ Should match value in int_lock() in nufr-platform-import.h
         MOV    R12, #0x60
         MSR    BASEPRI, R12

@ Load TCB pointers
         LDR    R2, [R0]
         LDR    R3, [R1]

@ If 'nufr_ready_list' is NULL, then BG task is being switched in;
@ special case. Go get the special TCB stub for the BG task
         CMP    R3, #0
         BNE    NoNullReady
         LDR    R3, =nufr_bg_sp
NoNullReady:

@ Sanity check: make sure in-task and out-task are different
         CMP    R2, R3
         BEQ    abort

@ Offset in TCB to in-task/out-task's stack ptrs
        @ Error on cannot honor width suffix
        @ MOV   R12, #NUFR_SP_OFFSET_IN_TCB
         MOV    R12, #12     @ instead of above
         ADD    R2, R12
         ADD    R3, R12

@ Load into R12 out-task's current SP
         MRS    R12, PSP

@ Stack manual store registers to out-task's stack
         STMDB  R12!, {R4-R11}

@ Adjust out-task's stack ptr for manual push reg's
         STR    R12, [R2]

@ Unstack in-task's manual store registers
@ When finished, R12 will point to autosave registers
         LDR    R12, [R3]
         LDMIA  R12!, {R4-R11}

@ Change PSP to point to in-task's stack
         MSR    PSP, R12

@ Readjust R3 to point back to top of in-task's TCB
         @ Error on cannot honor width suffix
         @MOV   R12, #NUFR_SP_OFFSET_IN_TCB
         MOV    R12, #12     @ instead of above
         SUBS   R3, R12

@ Set 'nufr_running' value to in-task's TCB
         STR    R3, [R0]

abort:
@ Enable interrupts
         MOV    R0, #0
         MSR    BASEPRI, R0

@ Return from PendSV. This will unstack autosave regs
@   and cause in-task to start executing.
@   The commented out instruction ** MOV PC, LR **  works on M0, M3
@      (see QEMU and other RTOSs), but will not work on the M4
@         MOV     PC, LR
         BX      LR
         @  End is invalid instruction via gas
         @END
         .end
