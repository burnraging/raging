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

//! @file     ssp-packet.cpp
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

#include "ssp-packet.h"


ssp_packet_cl::ssp_packet_cl(const ssp_packet_cl &src)
{
    this->channel_number = src.channel_number;
    this->MAX_PACKET_SIZE = src.MAX_PACKET_SIZE;
    this->buffer.resize(src.size());
    this->buffer.assign(src.buffer.begin(), src.buffer.end());
}

void ssp_packet_cl::operator=(const ssp_packet_cl &src)
{
    this->channel_number = src.channel_number;
    this->MAX_PACKET_SIZE = src.MAX_PACKET_SIZE;
    this->buffer.resize(src.size());
    this->buffer.assign(src.buffer.begin(), src.buffer.end());
}

unsigned ssp_packet_cl::remaining_capacity(void)
{
    return this->MAX_PACKET_SIZE - this->buffer.size();
}

unsigned ssp_packet_cl::size(void) const
{
    return this->buffer.size();
}

bool ssp_packet_cl::prepend_bytes(uint8_t single_byte)
{
    if (this->remaining_capacity() > 0)
    {
        this->buffer.insert(this->buffer.begin(), single_byte);

        return true;
    }

    return false;
}

bool ssp_packet_cl::prepend_bytes(const uint8_t *byte_array, unsigned length)
{
    if (this->remaining_capacity() >= length)
    {
        for (unsigned i = 0; i < length; i++)
        {
            this->buffer.insert(this->buffer.begin(), byte_array[i]);
        }

        return true;
    }

    return false;
}

bool ssp_packet_cl::prepend_bytes(const std::vector<uint8_t> &byte_array)
{
    if (this->remaining_capacity() >= byte_array.size())
    {
        for (unsigned i = 0; i < byte_array.size(); i++)
        {
            this->buffer.insert(this->buffer.begin(), byte_array[i]);
        }

        return true;
    }

    return false;
}

bool ssp_packet_cl::append_bytes(uint8_t single_byte)
{
    if (this->remaining_capacity() > 0)
    {
        this->buffer.push_back(single_byte);

        return true;
    }

    return false;
}

bool ssp_packet_cl::append_bytes(const uint8_t *byte_array, unsigned length)
{
    if (this->remaining_capacity() >= length)
    {
        for (unsigned i = 0; i < length; i++)
        {
            this->buffer.push_back(byte_array[i]);
        }

        return true;
    }

    return false;
}

bool ssp_packet_cl::append_bytes(const std::vector<uint8_t> &byte_array)
{
    if (this->remaining_capacity() >= byte_array.size())
    {
        for (unsigned i = 0; i < byte_array.size(); i++)
        {
            this->buffer.push_back(byte_array[i]);
        }

        return true;
    }

    return false;
}

bool ssp_packet_cl::pop_front(uint8_t *byte_ptr)
{
    if (this->buffer.size() > 0)
    {
        *byte_ptr = this->buffer[0];

        this->buffer.erase(this->buffer.begin());

        return true;
    }

    return false;
}

unsigned ssp_packet_cl::pop_front(uint8_t *output, unsigned max_bytes)
{
    unsigned i;
    unsigned max = this->buffer.size();

    if (max > max_bytes)
        max = max_bytes;

    for (i = 0; i < max; i++)
    {
        *output++ = this->buffer.at(0);

        this->buffer.erase(this->buffer.begin());
    }

    return i;
}

unsigned ssp_packet_cl::pop_front(std::vector<uint8_t> &output, unsigned max_bytes)
{
    unsigned i;
    unsigned max = this->buffer.size();

    output.clear();

    for (i = 0; i < max; i++)
    {
        output.push_back(this->buffer.at(0));

        this->buffer.erase(this->buffer.begin());
    }

    return i;
}
