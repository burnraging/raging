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

//! @file     ssp-packet.h
//! @authors  Bernie Woodland
//! @date     7Dec18
//!
//! @brief    
//!
//! @details  
//! @details  
//! @details  
//! @details  
//! @details  
//! @details  
//! @details  
//!

#ifndef SPP_PACKET_H_
#define SPP_PACKET_H_

#include "raging-global.h"

#include <vector>

#define SSP_DEFAULT_MAX_PACKET      2000

class ssp_packet_cl
{
public:
    ssp_packet_cl(unsigned max_packet_size = SSP_DEFAULT_MAX_PACKET,
                  unsigned channel_no = 0) :
        channel_number(channel_no),
        MAX_PACKET_SIZE(max_packet_size)
    {}

    // Copy constructor, Assignment operator
    ssp_packet_cl(const ssp_packet_cl &src);
    void operator=(const ssp_packet_cl &src);

    unsigned remaining_capacity(void);
    unsigned size(void) const;

    bool prepend_bytes(uint8_t single_byte);
    bool prepend_bytes(const uint8_t *byte_array, unsigned length);
    bool prepend_bytes(const std::vector<uint8_t> &byte_array);

    bool append_bytes(uint8_t single_byte);
    bool append_bytes(const uint8_t *byte_array, unsigned length);
    bool append_bytes(const std::vector<uint8_t> &byte_array);

    bool        pop_front(uint8_t *byte_ptr);
    unsigned    pop_front(uint8_t *output, unsigned max_bytes);
    unsigned    pop_front(std::vector<uint8_t> &output, unsigned max_bytes);



    std::vector<uint8_t> buffer;

    unsigned channel_number;
    unsigned MAX_PACKET_SIZE;

private:
};

#endif  // SPP_PACKET_H_