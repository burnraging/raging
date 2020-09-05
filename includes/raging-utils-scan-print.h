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

/**
 *	@file		raging-utils-scan-print.h
 *	@author		Christopher R. Martin
 *	@date	 	8 Jan 2018
 *	@brief		This file provides access to the utilities
 *
 */
#ifndef _RAGING_UTILS_SCAN_PRINT_H_
#define _RAGING_UTILS_SCAN_PRINT_H_

#include "raging-global.h"
#include "stdarg.h"

RAGING_EXTERN_C_START
unsigned rutils_unsigned64_to_decimal_ascii(char *stream, unsigned max_stream, uint64_t value, bool append_null);
unsigned rutils_unsigned32_to_decimal_ascii(char *stream, unsigned max_stream, uint32_t value, bool append_null);
unsigned rutils_signed64_to_decimal_ascii(char    *stream,
                                          unsigned max_stream,
                                          int64_t  value,
                                          bool     append_null);
unsigned rutils_signed32_to_decimal_ascii(char    *stream,
                                          unsigned max_stream,
                                          int32_t  value,
                                          bool     append_null);
unsigned rutils_unsigned64_to_hex_ascii(char    *stream,
                                        unsigned max_stream,
                                        uint64_t value,
                                        bool     append_null,
                                        unsigned zero_pad_length,
                                        bool     upper_case);
unsigned rutils_unsigned32_to_hex_ascii(char    *stream,
                                        unsigned max_stream,
                                        uint32_t value,
                                        bool     append_null,
                                        unsigned zero_pad_length,
                                        bool     upper_case);
unsigned rutils_decimal_ascii_to_unsigned64(const char *stream,
                                            uint64_t   *value,
                                            bool       *failure);
unsigned rutils_decimal_ascii_to_unsigned32(const char *stream,
                                            uint32_t   *value,
                                            bool       *failure);
unsigned rutils_hex_ascii_to_unsigned64(const char *stream,
                                        uint64_t   *value,
                                        bool       *failure);
unsigned rutils_hex_ascii_to_unsigned32(const char *stream,
                                        uint32_t   *value,
                                        bool       *failure);

bool rutils_is_decimal_digit(char digit);
bool rutils_is_hex_digit(char digit);

unsigned rutils_decimal_digit_to_value(char digit);
unsigned rutils_hex_digit_to_value(char digit);

unsigned rutils_count_of_decimal_ascii_span(const char *stream);
unsigned rutils_count_of_hex_ascii_span(const char *stream);

unsigned rutils_sprintf(char       *out_stream,
                        unsigned    out_stream_size,
                        const char *control,
                        ...);
unsigned rutils_sprintf_args(char       *out_stream,
                             unsigned    out_stream_size,
                             const char *control,
                             va_list arguements);
                        
RAGING_EXTERN_C_END





#endif // _RAGING_UTILS_SCAN_PRINT_H_
