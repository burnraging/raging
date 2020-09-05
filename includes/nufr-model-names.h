/*
Copyright (c) 2020, Bernie Woodland
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

//! @file     nufr-model-names.h
//! @authors  Bernie Woodland
//! @date     27Jan20
//!
//! @brief    Master list of nufr-platform names
//! @brief    Include only in nufr-platform-app-compile-switches.h


#ifndef NUFR_MODEL_NAMES_H_
#define NUFR_MODEL_NAMES_H_

//! @name     NUFR_TINY_MODEL
//! @name     NUFR_SMALL_MODEL
//! @name     NUFR_UT_MODEL
//! @name     NUFR_PTHREAD_MODEL
//! @name     NUFR_MSP430_MODEL
//!
//! @brief    nufr-platform derivation specifiers
//! @details  Must be defined ahead of nufr-platform-app-compile-switches.h
//!
#define NUFR_TINY_MODEL          1  // ./nufr-platform/tiny-model/
#define NUFR_SMALL_MODEL         2  // ./nufr-platform/small-model/
#define NUFR_UT_MODEL            3  // ./nufr-platform/pc-ut/
#define NUFR_PTHREAD_MODEL       4  // ./nufr-platform/pc-pthread/
#define NUFR_MSP430_MODEL        5  // ./nufr-platform/msp430/

#endif //  NUFR_MODEL_NAMES_H_