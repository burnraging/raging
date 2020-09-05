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

//! @file    msp430-peripherals.c
//! @authors Bernie Woodland
//! @date    15Sep18
//!
//! @brief   MSP430 peripheral interactions
//!

#define MSP430_PERIPHERALS_GLOBAL_DEFS

// This file is in ./bin/msp430-gcc-6.0.1.0/include. It includes ./bin/msp430f5529.h
#include <msp430.h>

#include "nufr-kernel-task.h"
#include "msp430-peripherals.h"
#include "msp430-irq-c-context-switch.h"

#include "nsvc-api.h"


// Map quantum timer to timer of choice.
// Currently to Timer0_A5 (Timer A0)
#define Q_TACTL     TA0CTL           /* Timer0_A5 Control */
#define Q_TACCTL0   TA0CCTL0         /* Timer0_A5 Capture/Compare Control 0 */
#define Q_TACCTL1   TA0CCTL1         /* Timer0_A5 Capture/Compare Control 1 */
#define Q_TACCTL2   TA0CCTL2         /* Timer0_A5 Capture/Compare Control 2 */
#define Q_TACCTL3   TA0CCTL3         /* Timer0_A5 Capture/Compare Control 3 */
#define Q_TACCTL4   TA0CCTL4         /* Timer0_A5 Capture/Compare Control 4 */
#define Q_TAR       TA0R             /* Timer0_A5 Count Value */
#define Q_TACCR0    TA0CCR0          /* Timer0_A5 Capture/Compare 0 */
#define Q_TACCR1    TA0CCR1          /* Timer0_A5 Capture/Compare 1 */
#define Q_TACCR2    TA0CCR2          /* Timer0_A5 Capture/Compare 2 */
#define Q_TACCR3    TA0CCR3          /* Timer0_A5 Capture/Compare 3 */
#define Q_TACCR4    TA0CCR4          /* Timer0_A5 Capture/Compare 4 */
#define Q_TAIV      TA0IV            /* Timer0_A5 Interrupt Vector Word */
#define Q_TAEX0     TA0EX0           /* Timer0_A5 Expansion Register 0 */

// Compile switch that allows the OS clock to run in freerunning when not
// in use. This will keep 'msp_current_time' up to date, in particular
// when device is asleep.
#define FREERUNNING_MODE          0

#define FREE_RUN_MAX    0xFFFF

// Max value in pseudo-millisecs (1.024-to-1.000) that a timeout can be
// specified for.
// Value comes out to be 4,194.240 seconds (1 hr + 9 min + 54 secs)
#define MAX_TIMEOUT_MILLISECS   ((uint32_t)FREE_RUN_MAX << 6)

uint32_t              msp_current_time;           // 32-bit reference time
uint32_t              msp_current_duration;       // timeout configured for
unsigned              msp_current_divide_shifts;  // number of bits to shift to reach divisor
bool                  msp_freerunning_only;       // OS timer used for reference time only

// CPU initializations that just can't wait.
// NOTE: Cannot use any global variables here!
void msp_early_init(void)
{
    _IMPORT_STATUS_REG_TYPE sr_value;

    // Lock interrupts
    sr_value = int_lock();

    // Disable the watchdog
    // WDTPW <== password, must always write this
    WDTCTL = WDTPW | WDTHOLD;
}

void msp_init(void)
{
    uint16_t value;

    // Set up aux clock. Aux clock will feed timer source for NUFR app timers
    // Aux clock will be supplied by REFOCLK clock (32,768 Hz internal clock)
    // REFOCLK clock divided by 32: 1 Aux clock tick will take 1.024 millisecs
    // Doing a read-modify-write on these reg's
    value = UCSCTL5;
    value &= BITWISE_NOT16(DIVA0 | DIVA1 | DIVA2 | DIVPA0 | DIVPA1 | DIVPA2);
    value |= DIVA__32 | DIVPA__32;
    UCSCTL5 = value;
    value = UCSCTL4;
    value &= BITWISE_NOT16(SELA_0 | SELA_1 | SELA_2 | SELA_3 |
                           SELA_4 | SELA_5 | SELA_6 | SELA_7);
    value |= SELA__REFOCLK;
    UCSCTL4 = UCSCTL4;

    msp_qtm_init();
}

// See nsvc-timer.c for use of the quantum timer.
// The msp_qtm_xxx() APIs provide the MSP430-specific fulfillment of the quantum
// timer.

// Init the quantum timer.
void msp_qtm_init(void)
{
    uint16_t tm_ctl_reg;

    msp_current_time = 0;
    msp_current_divide_shifts = 0;
    msp_freerunning_only = false;
    msp_current_duration = 0;

    // turn off all captures
    Q_TACCR0 = 0;    // turn off all captures
    Q_TACCR1 = 0;
    Q_TACCR2 = 0;
    Q_TACCR3 = 0;
    Q_TACCR4 = 0;
       // will setting a 1 to interrupt pending bit clear it? don't know
    Q_TACCTL0 = CCIFG;
    Q_TACCTL1 = CCIFG;
    Q_TACCTL2 = CCIFG;
    Q_TACCTL3 = CCIFG;
    Q_TACCTL4 = CCIFG;

    // NOTE: setting TAIFG==0 clears pending interrupts
    // See https://www.embedded.fm/blog/ese101-msp430-timer-example
    Q_TAEX0 = TAIDEX_7;                           // 2nd divide == 8
    tm_ctl_reg = TASSEL__ACLK            |        // drive from aux clock
                 ID__8                   |        // 1st divide == 8
                 MC_0                    |        // halt timer
                 TACLR                            // clear counter
                                                  // disable interrupts (no TAIE set)
                 ;                                // clear pending interrupts (no TAIFG set)
    Q_TACTL = tm_ctl_reg;

#if FREERUNNING_MODE == 1
    // Now, set timer in freerunning mode
    msp_freerunning_only = true;
    Q_TACCTL0 = CCIE_BIT;                            // enable CC0 reg for interrupts
    Q_TACCR0 = FREE_RUN_MAX;
    tm_ctl_reg = TASSEL__ACLK            |           // drive from aux clock
                 ID__8                   |           // 1st divide == 8
                 MC_1                                // start timer
                 ;                                   // set TAIE==0
    Q_TACTL = tm_ctl_reg;
#endif
}

void msp_qtm_halt_timer(void)
{
    uint16_t tm_ctl_reg;

    // Clear divide bits: ID__1 | ID__2 | ID__4 | ID__8
    // Clear mode bits: MC__UP | MC__CONTINUOUS | MC__UPDOWN
    // Disable timer interrupts by clearing TAIE
    // Clear pending interrupt by clearing TAIFG bit
    tm_ctl_reg = TASSEL__ACLK                     |  // Keep Aux clock as source
                 TACLR                            |  // clear counter
                 MC__STOP;                           // halt counter
    Q_TACTL = tm_ctl_reg;
}

// Update current time based on elapsed time
void msp_qtm_update_current_time_at_timeout(void)
{
    msp_current_time += msp_current_duration;
}

// Update current time while timer is running, without stopping timer
// This should only be called once before reconfiguring the timer
void msp_qtm_update_current_time_while_running(void)
{
    uint16_t              timer_counter_reg;

    timer_counter_reg = Q_TAR;

    msp_current_time += ((uint32_t)timer_counter_reg << msp_current_divide_shifts);
}

// Get current time.
// Do not use this function for timer control, only use
// if for a time reference of a sort.
uint32_t msp_qtm_retrieve_current_time(void)
{
#if FREERUNNING_MODE == 1
    uint16_t              timer_counter_reg;

    timer_counter_reg = Q_TAR;

    return msp_current_time +
          ((uint32_t)timer_counter_reg << msp_current_divide_shifts);
#else
    // FIXME: without freerunning mode, value is choppy at best
    return msp_current_time;
#endif
}

// 'new_timeout_millisecs' is in pseudo-millisecs, 1.024-to-1.000 ratio
// Valid Range on 'new_timeout_millisecs' is 1 millisec to 4,194.240 secs
// Divisors are adjusted to scale according to size of timeout
void msp_qtm_configure_timeout_and_start(uint32_t new_timeout_millisecs)
{
    uint16_t        divisor1;
    uint16_t        divisor2;
    uint16_t        tm_ctl_reg;
    uint16_t        count_value;

    msp_current_duration = new_timeout_millisecs;

    // Is timeout < 65,536 (65.536 secs)? Then no clock divides needed;
    //   optimize for max resolution.
    if (new_timeout_millisecs < 0x00010000)
    {
        msp_current_divide_shifts = 0;
        count_value = (uint16_t)new_timeout_millisecs;
        divisor1 = ID__1;         // divide by 1
        divisor2 = TAIDEX_0;      // divide by 1
    }
    // Is timeout < 524,288 (8 min, 44 sec)?
    // Step up divider, but not too much, to retain some resolution.
    else if (new_timeout_millisecs < 0x00080000)
    {
        msp_current_divide_shifts = 3;
        count_value = (uint16_t)(new_timeout_millisecs >> 3);
        divisor1 = ID__8;         // divide by 8
        divisor2 = TAIDEX_0;      // divide by 1
    }
    // Otherwise, max out divisors; forget about resolution.
    else
    {
        msp_current_divide_shifts = 6;
        if (count_value < MAX_TIMEOUT_MILLISECS)
        {
            count_value = (uint16_t)(new_timeout_millisecs >> 6);
        }
        // If timeout exceeds max, set to max. Otherwise,
        // 16-bit counter will wrap and delay will be shorter than requested.
        else
        {
            count_value = (uint16_t)(MAX_TIMEOUT_MILLISECS >> 6);
        }
        divisor1 = ID__8;         // divide by 8
        divisor2 = TAIDEX_7;      // divide by 8
    }

    // Safety check: setting a timeout of zero halts timer
    if (count_value <= 1)
    {
        count_value = 1;
    }
    // The TAIFG interrupt in up mode is triggered when timer reaches
    // timeout value, then resets to zero. Need to subtract by one
    // to account for this.
    else
    {
        count_value--;
    }

    Q_TAEX0 = divisor2;
    Q_TACCR0 = count_value;

    // Clear any unused divide bits: ID__1 | ID__2 | ID__4 | ID__8
    // Clear unused mode bits: MC__CONTINUOUS | MC__UPDOWN
    // Clear pending interrupt (uncertain if pending interrupt
    //    is even possible) by clearing TAIFG bit.
    tm_ctl_reg = TASSEL__ACLK                 | // Keep Aux clock as source
                 divisor1                     | // set new divisor1
                 TACLR                        | // clear counter
                 MC__UP                       | // Start timer; use "Up" mode.
                 TAIE                           // enable interrupts
                 ;
    Q_TACTL = tm_ctl_reg;
}

// Callin from nsvc-timer.c, from task level
// Call will start OS timer or change timeout of timer
// If 'new_timeout'==0, this means to halt timer
void msp_qtm_reconfigure_by_task(uint32_t new_timeout)
{
    uint16_t   saved_sr;

    saved_sr = int_lock();

    msp_qtm_halt_timer();

    int_unlock(saved_sr);

    msp_qtm_update_current_time_while_running();

    // Reconfigure to new timeout
    if (new_timeout > 0)
    {
        msp_qtm_configure_timeout_and_start(new_timeout);
    }
    // Timer halt command from task level
    else
    {
#if FREERUNNING_MODE == 1
        // No app timer running, so run in freerunning mode
        msp_freerunning_only = true;

        msp_qtm_configure_timeout_and_start(MAX_TIMEOUT_MILLISECS);
#endif
    }
}

// Interrupt handler for MSP's TA0, which is used as the quantum timer
// Hooked up to TA0, vector's address = 0x0FFEA. Priority = 53.
// See "MSP430F552x, MSP430F551x Mixed-Signal Microcontrollers Data Sheet"
// This uses a 64 interrupt vector scheme, instead of the older 32 scheme.
//
// When this fcn. is called, it will almost certainly because an app
// timer expired. This fcn. calls the SL timer handler for app timer
// expiration. This handler will send a message to the expiring task.
// The message send could necessitate a context switch. This fcn.
// does calculations in C to spare the IRQ stub from having to do them
// in assembler or having to do them under the restrictions of a
// naked-directive function. Therefore, this fcn. does the following:
//  1) Determines if the app timer callin triggered the need for
//     a context switch.
//  2) Looks at the IRQ nesting level of this invocation, to see if
//     this IRQ needs to do the context switch (lowest level), or
//     in the case that we're nested, defer the context switch to
//     the lowest nest level.
//  3) In the case that another IRQ, one that was nested inside
//     of this IRQ, triggered a context, and that context switch was
//     deferred to use (we're the lowest nest level), then we'll
//     have to handle the context switch on their behalf.
//  4) Prepare the context info for the stub, when doing a switch.
//
// Returns a '0' if not context switch should be done, a '1' if context
// switch should be done.
// If context switch should be done, it will use the following global
// variables as return values:
//   'msp_switchin_sp'
//   'msp_switchout_sp'
//   'msp_bg_task_switching_in'
//
uint16_t msp_qtm_irq_handler(void)
{
    uint16_t                tm_ctl_reg;
    uint32_t                new_timeout;
    nsvc_timer_callin_return_t callin_rv;
    bool                    rv;
    _IMPORT_STATUS_REG_TYPE saved_sr;

    // Halt the timer, to prevent an accidental 2nd OS timer interrupt timeout
    // Be on safe side and do this before enabling interrupts
    msp_qtm_halt_timer();

    msp430_irq_c_prelude();

    // This re-enables interrupts. Necessary because of the long
    //  duration of this fcn. It also updates logic that detects
    // if IRQ nesting and if this call needs to terminate in a
    // context switch rather than a return from interrupt.
    saved_sr = _IMPORT_INTERRUPT_ENABLE();

    // Advance current time
    msp_qtm_update_current_time_at_timeout();

    // Was this just a freerunning timeout? Restart in freerunning mode then.
    if (msp_freerunning_only)
    {
    #if FREERUNNING_MODE == 1
        msp_qtm_configure_timeout_and_start(MAX_TIMEOUT_MILLISECS);
    #endif
    }
    // Else, timeout from an app timer
    else
    {
        // Call into SL to update app timers and send message to
        // destination task.
        callin_rv = nsvc_timer_expire_timer_callin(msp_current_time,
                                                   &new_timeout);

        // No more app timers? Then restart in freerunning mode
        if (NSVC_TCRTN_DISABLE_QUANTUM_TIMER == callin_rv)
        {
        #if FREERUNNING_MODE == 1
            msp_freerunning_only = true;

            msp_qtm_configure_timeout_and_start(MAX_TIMEOUT_MILLISECS);
        #else
            msp_current_duration = 0;
        #endif
        }
        // Restart timer for next app timer
        else if (NSVC_TCRTN_RECONFIGURE_QUANTUM_TIMER == callin_rv)
        {
            msp_qtm_configure_timeout_and_start(new_timeout);
        }
        // Collision between app updating timers an us trying to
        // update timers. Delay by 1 millisec and call back in.
        else if (NSVC_TCRTN_BACKOFF_QUANTUM_TIMER == callin_rv)
        {
            msp_qtm_configure_timeout_and_start(1);
        }
        else
        {
            // can't get here
        }
    }

    // Must appear at end of IRQ handler which makes a NUFR call
    rv = msp430_irq_c_context_switch_conditional(saved_sr);

    return rv;
}
