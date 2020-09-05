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

//! @file    nufr-compile-switches.h
//! @authors Bernie Woodland
//! @date    6Aug17

#ifndef NUFR_COMPILE_SWITCHES_H
#define NUFR_COMPILE_SWITCHES_H

//!
//! @brief    Compile switch: add local struct
//!
#define NUFR_CS_LOCAL_STRUCT     0

//!
//! @brief    Compile switch: add messaging
//!
#define NUFR_CS_MESSAGING        1

//!
//! @brief    Compile switch: number of messaging priority levels available
//! @brief       Select a value from 1-4 inclusive
//! @brief       Only relevant with NUFR_CS_MESSAGING on
//!
#define NUFR_CS_MSG_PRIORITIES   1

//!
//! @brief    Compile switch: task killing feature
//! @brief       Task kill API and message abort API extensions
//! @brief       Only usefult with NUFR_CS_MESSAGING on
//!
#define NUFR_CS_TASK_KILL                0

//!
//! @brief    Compile switch: add semaphores
//!
#define NUFR_CS_SEMAPHORE                0

//!
//! @brief    Compile switch: To save CPU cycles, use inline fcns
//!
//! @details  Consumes 4k of text space on size-optimized compile
//! @details  on ARM Cortex M
//!
#define NUFR_CS_OPTIMIZATION_INLINES     0


#endif  //NUFR_COMPILE_SWITCHES_H
