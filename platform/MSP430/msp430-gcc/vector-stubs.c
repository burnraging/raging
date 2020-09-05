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

//! @file     vector-stubs.c
//! @authors  Bernie Woodland
//! @date     15Sep18
//!
//! @brief    Entry points for IRQ vectors
//!
//! @details  Default location for IRQ handler entry points.
//! @details  System developer can choose to locate handlers
//! @details  elsewhere.
//!
#include "raging-global.h"
#include "raging-contract.h"
#include "msp430-irq-entry-context-switch.h"
#include "msp430-peripherals.h"

#include <msp430.h>

unsigned watchdog_counts;
unsigned nmi_counts;
unsigned default_irq_counts;

__attribute__((interrupt(SYSNMI_VECTOR))) void nmi_handler(void)
{
    nmi_counts++;
}

__attribute__((interrupt(WDT_VECTOR))) void watchdog_handler(void)
{
    watchdog_counts++;
}



// Default handler for all other interrupts
#if 0
__attribute__((interrupt(!!insert number!!))) void default_handler(void)
{
    default_irq_counts++;
}
#endif

// This is the 2nd Timer0 (which is TimerA0) interrupt.
// This mode is never used, so should never get this interrupt.
// Vector 54/0xFFEA
// See this link clarifying vectors:
//   http://processors.wiki.ti.com/index.php/MSP430_FAQ#How_to_assign_the_correct_Timer_A_interrupt_vector.3F
__attribute__((interrupt(TIMER0_A0_VECTOR), naked)) void timer0_a0_handler(void)
{
    APP_REQUIRE_IL(false);
}

//!
//! @name      timer0_a0_handler
//!
//! @brief     NUFR Service Layer (SL) Quantum Timer
//!
//! @details   H/W timer used by SL as time base for app timers.
//! @details   This is hooked up to Timer0 A1.
//! @details   Vector 53/0xFFE8
//! @details   NOTE: The "naked" attribute means that compiler doesn't
//! @details         generate code to push registers and adjust SP
//! @details         upon entry/exit.
//!
__attribute__((interrupt(TIMER0_A1_VECTOR), naked)) void timer0_a1_handler(void)
{
    msp430_irq_entry_pre();

    // Call IRQ handler for this interrupt
    // NOTE: interrupts are disabled when
    //       we return from here.
    (void)msp_qtm_irq_handler();

    msp430_irq_entry_post();
}
