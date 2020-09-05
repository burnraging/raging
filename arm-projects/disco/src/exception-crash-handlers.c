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

//! @file     exception-crash-handlers.c
//! @authors  Bernie Woodland
//! @date     24May19
//!
//! @brief    Exception handlers stubs for crash handler
//!

#include "raging-global.h"

// Hack here. We're actually passing 3 arguments in.
void crash_handler(void);



void __attribute__((naked)) HardFault_Handler(void)
{
    __asm volatile(
        "  sub    r0, r1, r1\n\t"               // Zero out r0
        "  add    r0, #1\n\t"                   // 1 == CODE_HARD_FAULT
        "  mov    r1, sp\n\r"                   // Unmodified SP
        "  and    r2, lr\n\r"                   // Either 0xfffffffd or 0xfffffff1
        : );

    crash_handler();

infinite_loop:
    __asm volatile goto(
        "  b     %l[infinite_loop]\n\t"         // spin forever 
        "  bx     lr"                           // we'll never get here
        :
        :
        :
        : infinite_loop);
}


void __attribute__((naked)) MemManage_Handler(void)
{
    __asm volatile(
        "  sub    r0, r1, r1\n\t"               // Zero out r0
        "  add    r0, #2\n\t"                   // 2 == CODE_MEM_MANAGER_FAULT
        "  mov    r1, sp\n\r"                   // Unmodified SP
        "  and    r2, lr\n\r"                   // Either 0xfffffffd or 0xfffffff1
        : );

    crash_handler();

infinite_loop:
    __asm volatile goto(
        "  b     %l[infinite_loop]\n\t"         // spin forever 
        "  bx     lr"                           // we'll never get here
        :
        :
        :
        : infinite_loop);
}


void __attribute__((naked)) BusFault_Handler(void)
{
    __asm volatile(
        "  sub    r0, r1, r1\n\t"               // Zero out r0
        "  add    r0, #3\n\t"                   // 3 == CODE_BUS_FAULT
        "  mov    r1, sp\n\r"                   // Unmodified SP
        "  and    r2, lr\n\r"                   // Either 0xfffffffd or 0xfffffff1
        : );

    crash_handler();

infinite_loop:
    __asm volatile goto(
        "  b     %l[infinite_loop]\n\t"         // spin forever 
        "  bx     lr"                           // we'll never get here
        :
        :
        :
        : infinite_loop);
}


void __attribute__((naked)) UsageFault_Handler(void)
{
    __asm volatile(
        "  sub    r0, r1, r1\n\t"               // Zero out r0
        "  add    r0, #4\n\t"                   // 4 == CODE_USAGE_FAULT
        "  mov    r1, sp\n\r"                   // Unmodified SP
        "  and    r2, lr\n\r"                   // Either 0xfffffffd or 0xfffffff1
        : );

    crash_handler();

infinite_loop:
    __asm volatile goto(
        "  b     %l[infinite_loop]\n\t"         // spin forever 
        "  bx     lr"                           // we'll never get here
        :
        :
        :
        : infinite_loop);
}
