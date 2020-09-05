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

//! @file    nufr-platform-app-compile-switches.h
//! @authors Bernie Woodland
//! @date    27Jan20
//! @brief   Has project-specific compile switches,
//! @brief   both NUFR-defined and project-defined

#ifndef NUFR_PLATFORM_APP_COMPILE_SWITCHES_H_
#define NUFR_PLATFORM_APP_COMPILE_SWITCHES_H_

#include "nufr-model-names.h"

//******     NUFR-defined Compile Switches


// Needed in nufr-sanity-checks.c
//!
//! @name     NUFR_CS_WHICH_PLATFORM_MODE
//! @brief    Which ./nufr-platform/* files project is based on
//!
#define NUFR_CS_WHICH_PLATFORM_MODEL             NUFR_UT_MODEL

//!
//! @name     NUFR_CS_USING_OS_TICK_CALLIN
//! @brief    Compile switch: whether user includes an OS Tick callin fcn.
//!
#define NUFR_CS_USING_OS_TICK_CALLIN             0

//!
//! @name     NUFR_CS_USING_OS_TICK_CALLIN
//! @brief    Compile switch: disable internal OS ticking
//! @details  This disables nufr_sleep(), nufr_xxxT()
//!
#define NUFR_CS_EXCLUDING_OS_INTERNAL_TICKS      0

//!
//! @brief    Compile switch: Assert inclusion level
//!
//! @details  (See assert macros)
//!
// commented out was for QEMU, uncommented is for disco
//#define NUFR_ASSERT_LEVEL       0
#define NUFR_ASSERT_LEVEL       9


//******     Project-defined Compile Switches

// App developer to include

#endif  //NUFR_PLATFORM_APP_COMPILE_SWITCHES_H_