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

//! @file     msp430-irq-c-context-switch.h
//! @authors  Bernie Woodland
//! @date     26Oct18
//!
//! @brief    MSP430 IRQ handler context switch inline fcns
//!
//! @details  Use according to rules specified in
//! @details  'msp430-irq-entry-context-switch.h'
//!


#ifndef MSP430_IRQ_C_CONTEXT_SWITCH_H_
#define MSP430_IRQ_C_CONTEXT_SWITCH_H_

#include "raging-global.h"
#include "nufr-kernel-base-task.h"

#include "nufr-platform-import.h"
#include "nufr-kernel-task.h"

//!
//! @name    msp430_irq_c_prelude
//!
//! @brief   Required API for Stackable Handlers
//!
//! @details  Use according to rules specified in
//! @details  'msp430-irq-entry-context-switch.h'
//! @details  Must appear before interrpts are enabled.
//!
__attribute__((always_inline)) inline void msp430_irq_c_prelude(void)
{
    extern uint16_t msp430_irq_nest_level;

    msp430_irq_nest_level++;
}

//!
//! @name    msp430_irq_c_context_switch_conditional
//!
//! @brief   Required API for Stackable Handlers
//!
//! @details  Use according to rules specified in
//! @details  'msp430-irq-entry-context-switch.h'
//!
//! @param[in] 'saved_sr'-- Calling fcn's status register current value
//!
__attribute__((always_inline)) inline bool
msp430_irq_c_context_switch_conditional(_IMPORT_STATUS_REG_TYPE saved_sr)
{
    extern uint16_t   msp430_irq_nest_level;
    extern unsigned **msp_switchin_sp;
    extern unsigned **msp_switchout_sp;
    extern bool       msp_bg_task_switching_in;

    _IMPORT_INTERRUPT_DISABLE(saved_sr);

    msp430_irq_nest_level--;

    // When 'msp430_irq_nest_level'==0, then we're exiting the lowest
    // nested IRQ handler. 
    if (msp430_pending_context_switch && (0 == msp430_irq_nest_level))
    {
        msp430_pending_context_switch = false;

        if (nufr_running != nufr_ready_list)
        {
            msp_switchout_sp = &(nufr_running->stack_ptr);

            msp_bg_task_switching_in = NULL == nufr_ready_list;
            // Check for BG Task switchin to prevent crash: BG Task
            // will never get switched in from IRQ level
            if (msp_bg_task_switching_in)
            {
                nufr_running = (nufr_tcb_t *)nufr_bg_sp;
                msp_switchin_sp = &nufr_bg_sp[NUFR_SP_INDEX_IN_TCB];
            }
            else
            {
                nufr_running = nufr_ready_list;
                msp_switchin_sp = &nufr_running->stack_ptr;
            }

            // Call to _IMPORT_INTERRUPT_ENABLE() purposely omitted

            return true;
        }
    }

    // Call to _IMPORT_INTERRUPT_ENABLE() purposely omitted

    return false;
}

#endif //MSP430_IRQ_C_CONTEXT_SWITCH_H_