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

//! @file     reset-handler.c
//! @authors  Bernie Woodland
//! @date     15Sep18
//!
//! @brief    Reset vector handler
//!
// Based on ./platform/ARM_CMx/gcc/Reset_Handler.c

#include "raging-utils-mem.h"
#include "msp430-peripherals.h"
#include "nufr-platform-export.h"
#include "msp430-base.h"

#include <msp430.h>


/* entry point into application */
int main(void);

//*****  main() / Background Task stack. This is our stack
extern uint16_t Bg_Stack[BG_STACK_SIZE / BYTES_PER_WORD16];

//*****  Linker-defined variables

/* start of .data in the linker script */
extern unsigned __datastart;

/* end of .data in the linker script */
extern unsigned __dataend;

/* initialization values for .data  */
extern unsigned const __romdatastart;

/* start of .bss in the linker script */
extern unsigned __bssstart;

/* end of .bss in the linker script */
extern unsigned __bssend;


// Reset vector handler
// Prepares C programming environment, so normal C code
// can start running. Resetting consists of initializing global
// variables, plus executing any board-specific initialization
// that absolutely has to be completed early on.
//
// gcc manual says that 'reset' should resolve to interrupt 31
// spec sheet for msp430f5529 says reset vector is 63, not 31
// Using 'naked' because register saves aren't necessary
//
__attribute__((interrupt(RESET_VECTOR), naked)) void reset_handler(void)
{
    // 'naked' attribute must prevent a stack adjustment to allocate
    // room for 'length' on the stack and thereby store 'length' on
    // stack, instead of assigning it to a register.
    // Debug build is most likely culprit.
    unsigned length;

#if defined(CS_MSP430X_20BIT)
    // Based on: '__set_SP_register()' defined in in430.h
    // '__set_SP_register()' was never updated for 20-bit mode!

    // This inline assembler replaced this, which was working before:
    //__set_SP_register((unsigned int)Bg_Stack + BG_STACK_SIZE);

    // Set the stack pointer (SP register) to the bottom of the area
    // designate for it, which is the array 'Bg_Stack'.
    // Set it to point to 1 byte beyond end of array. First push will
    // increment it into the array before pushing data.
    asm volatile("movx.a %[stack_size], r12\n\t"
                 "movx.a #Bg_Stack, r13\n\t"
                 "addx.a r12, r13\n\t"
                 "movx.a r13, sp\n\t"
                 :                                     // no outputs
                 : [stack_size] "r" (BG_STACK_SIZE)
                 :);                                   // no clobbers
#else
    asm volatile("mov.w %[stack_size], r12\n\t"
                 "mov.w #Bg_Stack, r13\n\t"
                 "add.w r12, r13\n\t"
                 "mov.w r13, sp\n\t"
                 :                                     // no outputs
                 : [stack_size] "r" (BG_STACK_SIZE)
                 :);                                   // no clobbers
#endif

    msp_early_init();

    /* copy the data segment initializers from flash to RAM... */
    length = (unsigned) ((uint8_t *)&__dataend - (uint8_t *)&__datastart);
    rutils_memcpy(&__datastart, & __romdatastart, length);

    /* zero fill the .bss segment in RAM... */
    length = (unsigned) ((uint8_t *)&__bssend - (uint8_t *)&__bssstart);
    rutils_memset(&__bssstart, 0, length);

    /* application's entry point; should never return! */
    (void)main();

	// Safety mechanism, should force-crash if you get here
    while (1)
    {
    }
}