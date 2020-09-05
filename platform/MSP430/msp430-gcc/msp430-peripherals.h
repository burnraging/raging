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

//! @file     msp430-peripherals.h
//! @authors  Bernie Woodland
//! @date     23Sep18
//!
//! @brief    MSP430 peripheral interactions
//!

#ifndef MSP430_PERIPHERALS_H_
#define MSP430_PERIPHERALS_H_

#include "raging-global.h"
#include "msp430-base.h"

// The MSP430 TimerA timer used to drive NUFR quantum timer runs
// off the MSP430 Aux clock, which ticks 1024 times a second instead
// of 1000.
// Only use this macro for constants, as a run-time conversion
// would be obnoxious--assuming it would even compile.
// The "+ 500" is a rounding adjustment
// The uint64_t conversion is to prevent overflows for large
// 'millisec_delay"
#define CONVERT_TO_AUX_TICKS(millisec_delay)  ( (uint32_t) (((uint64_t)(millisec_delay) * 1000ull + 500ull)/1024ull) )

void msp_early_init(void);
void msp_init(void);
void msp_qtm_init(void);
void msp_qtm_halt_timer(void);
void msp_qtm_update_current_time_at_timeout(void);
uint32_t msp_qtm_retrieve_current_time(void);
void msp_qtm_configure_timeout_and_start(uint32_t new_timeout_millisecs);
void msp_qtm_reconfigure_by_task(uint32_t new_timeout);
uint16_t msp_qtm_irq_handler(void);

#endif //MSP430_PERIPHERALS_H_