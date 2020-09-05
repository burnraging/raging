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

//! @file     irq-context-switch.h
//! @authors  Bernie Woodland
//! @date     26Oct18
//!
//! @brief    MSP430 IRQ handler context switch inline fcns
//!
//!
//! @brief     Rules for coding MSP430 Interrupt Handlers with NUFR API calls
//!
//! @details   1) All IRQ handlers which make NUFR API calls which might
//! @details      result in a context switch must follow rules in vector-stubs.c
//! @details   2) All IRQ handlers /w NUFR calls must increment
//! @details      early in the handler, before first interrupt
//! @details      unlock msp430_irq_nest_level++
//! @details   3) All IRQ handlers /w NUFR calls must end with a
//! @details      call to irq_context_switch_conditional()
//! @details   
//! @details   Example prototype:
//! @details   
//! @details   uint16_t Some_timer_handler_in_C(void)
//! @details   {
//! @details       _IMPORT_STATUS_REG_TYPE saved_sr;
//! @details       bool rv;
//! @details   
//! @details       msp430_irq_nest_level++;
//! @details   
//! @details       saved_sr = _IMPORT_INTERRUPT_ENABLE();
//! @details   
//! @details       nufr_bop_send_with_key_override(SOME_TASK_TID);
//! @details       
//! @details       rv = irq_context_switch_conditional(saved_sr);
//! @details   
//! @details       return rv;
//! @details   }
//! @details   
//! @details   4) C IRQ handlers may enable interrupts, even if the handler
//! @details      makes a NUFR API call. Note that when interrupts get nested,
//! @details      the most shallow nest occurence is the only one which can/will
//! @details      do the actual context switch.
//! @details   5) C IRQ handlers that don't make NUFR API context switching calls
//! @details      may be written per usual.
//! @details   
//! @details   
//!



#ifndef IRQ_CONTEXT_SWITCH_H_
#define IRQ_CONTEXT_SWITCH_H_

#include "raging-global.h"
#include "nufr-kernel-base-task.h"

#include "nufr-platform-import.h"
#include "nufr-kernel-task.h"
#include "msp430-peripherals.h"
#include "msp430-assembler.h"

//!
//! @name    irq_context_switch_conditional
//!
//! @brief   context switch: C code + assembly context switch
//!
//! @details For CPUs which have no software interrupt capability.
//! @details This can only be invoked from an IRQ handler.
//! @details It must be last code in C handler.
//!
//! @details
//! @details
//!
//! @param[in] 'saved_sr'-- Calling fcn's status register current value
//!
__attribute__((always_inline)) inline bool
irq_context_switch_conditional(_IMPORT_STATUS_REG_TYPE saved_sr)
{
    _IMPORT_INTERRUPT_DISABLE(saved_sr);

    msp430_irq_nest_level--;

    // When 'msp430_irq_nest_level'==0, then we're exiting the lowest
    // nested IRQ handler. 
    if (msp430_pending_context_switch && (0 == msp430_irq_nest_level))
    {
        msp430_pending_context_switch = false;

        if (nufr_running != nufr_ready_list)
        {
            msp_qtm_switchout_sp = &(nufr_running->stack_ptr);

            msp_qtm_bg_task_switching_in = NULL == nufr_ready_list;
            // Check for BG Task switchin to prevent crash: BG Task
            // will never get switched in from IRQ level
            if (msp_qtm_bg_task_switching_in)
            {
                nufr_running = nufr_bg_sp;
                msp_qtm_switchin_sp = &nufr_bg_sp[NUFR_SP_INDEX_IN_TCB];
            }
            else
            {
                nufr_running = nufr_ready_list;
                msp_qtm_switchin_sp = &nufr_running->stack_ptr;
            }

            // Call to _IMPORT_INTERRUPT_ENABLE() purposely omitted

            return true;
        }
    }

    // Call to _IMPORT_INTERRUPT_ENABLE() purposely omitted

    return false;
}

#endif //IRQ_CONTEXT_SWITCH_H_