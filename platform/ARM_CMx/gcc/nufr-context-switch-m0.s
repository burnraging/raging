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

@//! @file    nufr-context-switch-m0.s
@//! @authors Chris Martin, Bernie Woodland
@//! @date    30Dec19
@
@ PendSV handler

@ I see conflicting info on how the M0 should work. Some ARM and STM
@ docs have bugs. I'm trusting info here:
@ https://community.arm.com/developer/ip-products/processors/b/processors-ip-blog/posts/arm-cortex-m0-assembly-programming-tips-and-tricks


/******************************************************************************
 *
 *      Assembly Prologue
 *
 *****************************************************************************/

#if defined(__GNUC__)

.syntax unified
.cpu cortex-m0
.thumb

.extern nufr_running
.extern nufr_ready_list

#elif defined(__clang__)

.syntax unified
.cpu cortex-m0
.thumb

#else
#error Unknown Compiler for Cortex-m0
#endif

#include "nufr-kernel-base-task.h"

@ M0_FIX is original code from M3/M4, changes for M0 follow

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
@ Not saving old PRIMASK. No chance of nesting.
         CPSID  I         @ sets PRIMASK bit 0 (disable interrupts)

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
@M0_FIX         MOV    R12, #12     @ instead of above
@M0_FIX         ADD    R2, R12
@M0_FIX         Add    R3, R12
@  Ref: https://stackoverflow.com/questions/30980160/arm-assembly-cannot-use-immediate-values-and-adds-adcs-together
         MOVS   R1, #12         @ instead of above
         ADD    R2, R1
         ADD    R3, R1

@ Load into R12 out-task's current SP
@M0_FIX         MRS    R12, PSP
         MRS    R1, PSP

@ Stack manual store registers to out-task's stack
@M0_FIX         STMDB  R12!, {R4-R11}
@ M0 STM instr only supports IA (increment after) mode.
         SUBS   R1, #32         @ Push out to final SP address
         STMIA  R1!, {R4-R7}    @ Save the lower registers
         MOV    R4, R8          @ Safe to use R4-R8 as scratchpad now
         MOV    R5, R9          @ ...get high register down there
         MOV    R6, R10
         MOV    R7, R11
         STMIA  R1!, {R4-R7}
         SUBS   R1, #32         @ Rewind SP back to where it needs to be

@ Adjust out-task's stack ptr for manual push reg's
@M0_FIX         STR    R12, [R2]
         STR    R1, [R2]

@ Unstack in-task's manual store registers
@ When finished, R12 will point to autosave registers
@M0_FIX         LDR    R12, [R3]
@M0_FIX         LDMIA  R12!, {R4-R11}
         LDR    R1, [R3]

         ADDS   R1,  #16        @ Jump down to R8's place on stack
         LDMIA  R1!, {R4-R7}    @ Pop R8-R11, temp put them in R4-R7
         MOV    R8,  R4         @ Manually move R4-R7 to final home:
         MOV    R9,  R5         @          up to high registers R8-R11
         MOV    R10, R6
         MOV    R11, R7
         SUBS   R1,  #32        @ Rewind back to R4's place on stack/manual save top
         LDMIA  R1!, {R4-R7}    @ Pop R4-R7
         ADDS   R1,  #16        @ jump down to point to R0/autosave top

@ Change PSP to point to in-task's stack
@M0_FIX         MSR    PSP, R12
         MSR    PSP, R1

@ Readjust R3 to point back to top of in-task's TCB
         @ Error on cannot honor width suffix
         @MOV   R12, #NUFR_SP_OFFSET_IN_TCB
@M0_FIX         MOV    R12, #12     @ instead of above
@M0_FIX         SUBS   R3, R12
         MOVS   R1, #12         @ instead of above
         SUBS   R3, R1

@ Set 'nufr_running' value to in-task's TCB
         STR    R3, [R0]

abort:
@ Enable interrupts
         CPSIE  I

@ Return from PendSV. This will unstack autosave regs
@   and cause in-task to start executing.
@   The commented out instruction ** MOV PC, LR **  works on M0, M3
@      (see QEMU and other RTOSs), but will not work on the M4
@         MOV     PC, LR
         BX      LR
         @  End is invalid instruction via gas
         @END
         .end
