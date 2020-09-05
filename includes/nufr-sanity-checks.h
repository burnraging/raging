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

//! @file    nufr-sanity-checks.h
//! @authors Bernie Woodland
//! @date    27Nov19
//!

#ifndef NUFR_SANITY_CHECKS_H_
#define NUFR_SANITY_CHECKS_H_

#include "nufr-global.h"

#include "nufr-platform.h"
#include "nufr-platform-app.h"

#if NUFR_SANITY_TINY_MODEL_IN_USE == 0
    #include "nsvc.h"
    #include "nsvc-api.h"


    RAGING_EXTERN_C_START
        bool nufr_sane_init(nsvc_timer_get_current_time_fcn_ptr_t fptr_current_time,
            nsvc_timer_quantum_device_reconfigure_fcn_ptr_t fptr_reconfigure);
    RAGING_EXTERN_C_END
    
#else

    RAGING_EXTERN_C_START
        bool nufr_sane_init(void);
    RAGING_EXTERN_C_END

#endif // NUFR_SANITY_TINY_MODEL_IN_USE



#endif  // NUFR_SANITY_CHECKS_H_
