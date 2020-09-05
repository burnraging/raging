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

#include "raging-global.h"
#include "raging-utils.h"
#include "raging-utils-scan-print.h"

#include <string.h>


#define CLEAR()              memset(output_buffer, 0, sizeof(output_buffer)); fail = false
#define CHECK(expected_str)  fail |= strcmp(output_buffer, expected_str) != 0; fail |= len != sizeof(expected_str)

char output_buffer[200];
bool fail;


void ut_raging_utils_hello_world(void)
{
    unsigned len;

    CLEAR();

    len = rutils_sprintf(output_buffer, sizeof(output_buffer), "hello world");

    CHECK("hello world");
}

void ut_raging_utils_1(void)
{
    unsigned len;

    CLEAR();
    len = rutils_sprintf(output_buffer, sizeof(output_buffer), "one: %u", 1);
    CHECK("one: 1");

    CLEAR();
    len = rutils_sprintf(output_buffer, sizeof(output_buffer), "one: %d", 1);
    CHECK("one: 1");

    CLEAR();
    len = rutils_sprintf(output_buffer, sizeof(output_buffer), "one: %x", 1);
    CHECK("one: 1");
}

void ut_raging_utils_medley(void)
{
    unsigned len;

    CLEAR();
    len = rutils_sprintf(output_buffer, sizeof(output_buffer),
                           "multiply %u times %u for %u", 7, 8, 56);
    CHECK("multiply 7 times 8 for 56");

    CLEAR();
    len = rutils_sprintf(output_buffer, sizeof(output_buffer), "%3u", 3);
    CHECK("  3");

    CLEAR();
    len = rutils_sprintf(output_buffer, sizeof(output_buffer), "%-3u", 3);
    CHECK("3  ");

    CLEAR();
    len = rutils_sprintf(output_buffer, sizeof(output_buffer), "%02X", 0xAB);
    CHECK("AB");

    CLEAR();
    len = rutils_sprintf(output_buffer, sizeof(output_buffer), "%04X", 1);
    CHECK("0001");
}

void ut_raging_utils_scan_print(void)
{
    ut_raging_utils_hello_world();
    ut_raging_utils_1();
    ut_raging_utils_medley();
}