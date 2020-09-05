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

//! @file     linux-utils.cpp
//! @authors  Bernie Woodland
//! @date     26Dec18
//!
//! @brief    Producer-Consumer object for tx and rx of SPP frames
//!

#include "linux-utils.h"
#include "raging-utils-scan-print.h"

#include <fcntl.h>     // 'fcntl()'
#include <stropts.h>   // 'ioctl()'

#include <string>



// Generic function to indicated if a file descriptor
// points to an open file
// https://stackoverflow.com/questions/12340695/how-to-check-if-a-given-file-descriptor-stored-in-a-variable-is-still-valid
bool is_file_open(int fd)
{
    return (fcntl(fd, F_GETFD) != -1) || (errno != EBADF);
}

// Delete any data on a tty device before reading     
void clear_read_data(int fd)
{
    ioctl(fd, FLUSHR);
}


// Print 
// Options: logical OR of:
//     DEBUG_PRINT_LENGTH   Add leading line of total length
//     DEBUG_PRINT_OFFSET   Print offset at beginning of each line
//     DEBUG_PRINT_COMMAS   Include commas between each value
//
// Ex (with DEBUG_PRINT_OFFSET, DEBUG_PRINT_COMMAS set):
//    0: 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A
//   10: 0xFF
//
void debug_printable_byte_string(std::string    &output,
                                 const std::vector<uint8_t> &hex_string,
                                 unsigned       options)
{
    unsigned size = hex_string.size();
    uint8_t temp_buffer[size];  //bit of a hack...

    for (unsigned i = 0; i < size; i++)
    {
        temp_buffer[i] = hex_string.at(i);
    }
//    std::vector<uint8_t>::iterator temp_it;
//    uint8_t *ptr_to_temp_it = &(*temp_it);  // Stack Overflow...how did we used to code without it?

    debug_printable_byte_string(output, temp_buffer, size, options);
}

void debug_printable_byte_string(std::string    &output,
                                 const uint8_t *string,
                                 unsigned       length,
                                 unsigned       options)
{
    char           temp_string[200];
    unsigned       temp_length;
    unsigned       base_offset;
    const unsigned MAX_CHARS_LINE = 10;
    unsigned       line_length = 0;

    output.clear();

    if (ANY_BITS_SET(options, DEBUG_PRINT_LENGTH))
    {
        rutils_sprintf(temp_string, sizeof(temp_string), "Length=%u\n", length);
        output += temp_string;
    }

    for (unsigned i = 0; i < length; i++)
    {
        bool is_not_last = i < length - 1;

        // Starting a new line?
        if (line_length == 0)
        {
            const unsigned TARGET_OFFSET_SIZE = 4;

            base_offset = i;

            if (ANY_BITS_SET(options, DEBUG_PRINT_OFFSET))
            {
                rutils_sprintf(temp_string, sizeof(temp_string),
                               " %4u:", base_offset);
                output += temp_string;
            }
        }

        rutils_sprintf(temp_string, sizeof(temp_string),
                       " 0x%02X", (unsigned)string[i]);
        output += temp_string;

        // If this is not last character, insert comma
        if (is_not_last && ANY_BITS_SET(options, DEBUG_PRINT_COMMAS))
        {
            output += ",";
        }

        line_length++;

        // Advance to next line?
        if (line_length == MAX_CHARS_LINE)
        {
            line_length = 0;

            if (is_not_last)
            {
                output += "\n";
            }
        }
    }

    // Add final linefeed
    if (line_length != 0)
    {
        output += "\n";
    }

    // Form into a C-compatible string
    output.push_back('\0');
}