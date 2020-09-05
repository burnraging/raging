/*
Copyright (c) 2019, Bernie Woodland
All rights reserved.

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

//! @file     assembly-m0.s
//! @authors  Chris Martin
//! @date     30Dec2019
//!
//! @brief   ARM specific Stack Pointer functions for M0/M0+ *only*
//!
//! @details Setting of main and process stack pointers

@          Prototypes to use in .c sources
@
@ void Set_MSP(uint32_t msp_value);
@ uint32_t Get_MSP(void);
@ void Set_PSP(uint32_t psp_value);
@ uint32_t Get_PSP(void);
@ void Switch_To_Process_Stack(void);
@ void DisableInterruptsByPrimask(void);
@ void EnableInterruptsByPrimask(void);
@ void Set_PRIMASK(uint32_t primask_value);
@ uint32_t Get_PRIMASK(void);
@ void Set_CONTROL(uint32_t control_value);
@ uint32_t Get_CONTROL(void);
@ uint32_t Get_PSR(void);
@ uint32_t Get_PC(void);
@ void _SVC0(void);

#if defined(__GNUC__)

.syntax unified
.cpu cortex-m0
.thumb
.text

#elif defined(__clang__)

.syntax unified
.cpu cortex-m0
.thumb

#else
#error Unknown Compiler for Cortex-m0
#endif

.thumb_func
.global Set_MSP

@//!    @name Set_MSP
@//
@//!    @brief Sets the value of the main stack pointer for the ARM Cortex-m3 Processor
@//
@//!    param[in] Pointer address of new msp stack

Set_MSP:

        MSR msp, R0     //  R0 contains the actual pointer value
        bx lr           

.thumb_func
.global Get_MSP

@//!    @name Get_MSP
@//
@//!    @brief Gets the value of the main stack pointer for the ARM Cortex-m3 Processor
@//
@//!    Returns MSP value

Get_MSP:

        MRS R0, msp
        bx lr           

.thumb_func
.global Set_PSP

@//!    @name Set_PSP
@//
@//!    @brief Sets the value of the process stack pointer for the ARM Cortex-m3 Processor
@//
@//!    param[in] Pointer address of new psp stack

Set_PSP:
        MSR psp, R0
        bx lr

.thumb_func
.global Switch_To_Process_Stack

.thumb_func
.global Get_PSP

@//!    @name Get_PSP
@//
@//!    @brief Gets the value of the process stack pointer for the ARM Cortex-m3 Processor
@//
@//!    Returns PSP value

Get_PSP:

        MRS R0, psp
        bx lr           

@//!    @name Switch_To_Process_Stack
@//
@//!    @brief Forces the processor to switch to the process stack
@//

@//!    @name Switch_To_Process_Stack
@//
@//!    @brief Sets SPSEL (bit 1) of CONTROL register, to use PSP instead of MSP
@//
Switch_To_Process_Stack:
        MOVS R0, 0x02   @ cortex-m0 fix, this was MOV R0, 0x02 for Cortex-m3/m4.
        MRS R1, CONTROL 
        ORRS R1, R0     @ cortex-m0 fix, this was ORR R1, R0 for Cortex-m3/m4.
        MSR CONTROL, R1
        bx lr

.thumb_func
.global DisableInterruptsByPrimask

@//!    @name DisableInterruptsByPrimask
@//
@//!    @brief Disable interrupts by setting bit 0 of PRIMASK register
@//
@//!    no parameters, no return

DisableInterruptsByPrimask:
        CPSID I
        bx lr

.thumb_func
.global EnableInterruptsByPrimask

@//!    @name EnableInterruptsByPrimask
@//
@//!    @brief Enable interrupts by clearing bit 0 of PRIMASK register
@//
@//!    no parameters, no return

EnableInterruptsByPrimask:
        CPSIE I
        bx lr

.thumb_func
.global Set_PRIMASK

@//!    @name Set_PRIMASK
@//
@//!    @brief Sets the PRIMASK register
@//
@//!    param[in] PRIMASK value

Set_PRIMASK:
        MSR primask, R0
        bx lr

.thumb_func
.global Get_PRIMASK

@//!    @name Get_PRIMASK
@//
@//!    @brief Gets the PRIMASK register
@//
@//!    returns PRIMASK value

Get_PRIMASK:
        MRS R0, primask
        bx lr

.thumb_func
.global Set_CONTROL

@//!    @name Set_CONTROL
@//
@//!    @brief Sets the CONTROL register
@//
@//!    param[in] CONTROL value

Set_CONTROL:
        MSR control, R0
        bx lr

.thumb_func
.global Get_CONTROL

@//!    @name Get_CONTROL
@//
@//!    @brief Gets the CONTROL register
@//
@//!    returns CONTROL value

Get_CONTROL:
        MRS R0, control
        bx lr

.thumb_func
.global Get_PSR

@//!    @name Get_PSR
@//
@//!    @brief Gets the PSR register
@//
@//!    returns PSR value

Get_PSR:
        MRS R0, psr
        bx lr

.thumb_func
.global Get_PC

@//!    @name Get_PC
@//
@//!    @brief Gets the Program Counter (PC) register
@//!    @brief PC value is next assembler instruction after call to this fcn.
@//
@//!    returns PC value

Get_PC:
        mov R0, lr           // lr has caller's return address, which is PC
@ fixme: find workaround for M0
@        and R0, #0xFFFFFFFE  // Clear thumb mode bit
        bx lr

.thumb_func
.global _SVC0

@//!    @name _SVC0
@//
@//!    @brief Invokes SVC with argument==0
@//

_SVC0:
        SVC #0
        bx lr
