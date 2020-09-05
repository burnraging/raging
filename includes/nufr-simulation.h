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

//! @file    nufr-simulation.h
//! @authors Bernie Woodland
//! @date    19Oct17

//!
//! @brief     pthread layer which allows the nufr kernel+platform
//! @brief     to run in a PC environment
//!

#ifndef NUFR_SIMULATION_H_
#define NUFR_SIMULATION_H_

#include "nufr-global.h"

typedef void *(nufr_sim_generic_fcn_ptr)(void *);


void *nufr_sim_launch_wrapper(void *arg_ptr);
void nufr_sim_context_switch(void);
void *nufr_sim_background_task(void *void_ptr);
void nufr_sim_entry(nufr_sim_generic_fcn_ptr bg_fcn_ptr,
                    nufr_sim_generic_fcn_ptr tick_fcn_ptr);

#endif  //NUFR_SIMULATION_H_