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

//!
//! @file      raging-utils-mem.h
//! @author    Bernie Woodland
//! @date      17Apr19
//! @brief     Header for all .c files defining rutils_memcpy(), rutils_memset()
//!

#ifndef _RAGING_UTILS_MEM_H_
#define _RAGING_UTILS_MEM_H_

#include "raging-global.h"


RAGING_EXTERN_C_START

void rutils_memset(void *dest_str, uint8_t set_value, unsigned length);
void rutils_memcpy(void *dest_str, const void *src_str, unsigned length);
// Not doing this fcn:
//int rutils_memcmp(const void *dest_str, const void *src_str, unsigned length);

RAGING_EXTERN_C_END

#endif  // _RAGING_UTILS_MEM_H_