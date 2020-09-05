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

//! @file     ssp-sanity.cpp
//! @authors  Bernie Woodland
//! @date     15Dec18
//!
//! @brief    
//!
//! @details  ./ssp-sanity -i /dev/ttyUSB0 -b 115200 [-d 10] [-p 012345]
//! @details  Will send a single packet and monitor received packets 
//! @details    -i = what tty device to perform test on. Sends and receives on this device.
//! @details    -b = baud rate, in bits/sec
//! @details    -d = (optional) Delay to listen for receive characters
//! @details         If no delay selected, we'll simply send bytes to peer
//! @details         If delay selected, we'll listen that many seconds for any
//! @details         coherent SSP packets and either print the number of packets
//! @details         received, or spit out the garbage characters collected.
//! @details    -p   (optional) Packet payload. This is a string of bytes to wrap up in 
//! @details         a single packet and send. This app will add the SSP frame to it
//! @details         (header + CRC).
//! @details         If no payload specified, this app simply monitors for rx characters.
//! @details         
//! @details         
//! @details   to make:
//! @details      g++ -g -I../../includes ssp-sanity.cpp ssp-framer.cpp ssp-packet.cpp ../../sources/raging-utils.c ../../sources/raging-utils-mem.c ../../sources/raging-utils-crc.c ../../sources/raging-utils-scan-print.c ./linux-utils.cpp -lpthread -o ssp-sanity
//! @details  
//! @details   to debug:
//! @details      gdb ssp-sanity
//! @details        break main
//! @details        run -i /dev/ttyUSB0 -b 115200 -p 01020304
//! @details  
//!

#include "raging-global.h"

#include "ssp-packet.h"
#include "ssp-framer.h"
#include "linux-utils.h"

#include "raging-utils.h"
#include "raging-utils-mem.h"
#include "raging-utils-scan-print.h"

#include <stdexcept>
#include <cstdlib>    // 'std::exit()'

#include <unistd.h>   // getopt(), sleep()
//#include <sys/types.h> // 'open()'
#include <sys/stat.h>  // 'open()'
#include <fcntl.h>     // 'open()', O_* constants

// **** Start Mocked values

// **** End Mocked values


// Takes a string of ascii hex digits and converts to a packet payload
// Returns NULL if there was a scan error 
ssp_packet_cl *text_string_to_packet(const char *text_string)
{
    unsigned      text_length;
    char          single_char_string[3];
    uint32_t      value;
    bool          result = false;
    ssp_packet_cl *packet_ptr = new ssp_packet_cl();

    text_length = rutils_strlen(text_string);

    for (unsigned i = 0; i < text_length; i += 2)
    {
        rutils_memcpy(&single_char_string[0], &text_string[i], 1);
        rutils_memcpy(&single_char_string[1], &text_string[i+1], 1);
        single_char_string[2] = '\0';

        if (2 != rutils_hex_ascii_to_unsigned32(single_char_string, &value, &result))
        {
            delete packet_ptr;
            return NULL;
        }

        if (result)
        {
            delete packet_ptr;
            return NULL;
        }

        packet_ptr->append_bytes((uint8_t)value);
    }

    return packet_ptr;
}

// Parms for the test
// 'tty_device_name'-- "/dev/tty0" or equivalent
// 'baud_rate'-- B1152000, B9600, etc.
// 'delay_in_seconds'-- time to wait for rx packet(s)
//                if zero, we're not monitoring for rx packets
// 'packet_ptr'-- packet to send.
//                If NULL, we're not sending, only listening 
void execute_test(const char    *tty_device_name,
                  unsigned       baud_rate,
                  unsigned       delay_in_seconds,
                  ssp_packet_cl *packet_ptr)
{
    ssp_packet_cl *rx_packet_ptr;

    ssp_framer_cl framer(tty_device_name,
                         baud_rate,
                         NULL);

    framer_error_t rv;

    rv = framer.start();
                      
    if (rv != FRAMER_ERROR_NONE)
    {
        printf("Failed to start test, rv=%u\n", (unsigned)rv);
        std::exit(1);
    }

    // Send our packet
    if (NULL != packet_ptr)
    {
        framer.tx_packet(packet_ptr);
    }

    // Monitoring rx packet?
    if (delay_in_seconds > 0)
    {
        sleep(delay_in_seconds);

        unsigned packet_count = 0;

        while (NULL != (rx_packet_ptr = framer.get_rx_packet()))
        {
            packet_count++;

            printf("Rx packet of length %u:\n", rx_packet_ptr->size());

            for (unsigned i = 0; i < rx_packet_ptr->size(); i++)
            {
                printf("%02X ", rx_packet_ptr->buffer.at(i));
            }

            printf("\n");
        }

        // Fruitless exercise? Spit out what we got
        if ((0 == packet_count) || (framer.m_rx_error_count > 0))
        {
            std::string print_string;

            // Convert byte array to printable text representation
            debug_printable_byte_string(print_string,
                                        framer.m_rx_debug_snapshot,
                DEBUG_PRINT_LENGTH | DEBUG_PRINT_OFFSET | DEBUG_PRINT_COMMAS);


            printf("Data dump. Packet count = %u. Error count = %u\n",
                   packet_count, framer.m_rx_error_count);
            printf("%s", print_string.c_str());
        }
    }

    framer.stop();
}

// Parse CLI arguments, error check
int main(int argc, char **argv)
{
    int         option;
    int         temp_fd;
    std::string tty_device_name;
    std::string baud_rate_string;
    std::string delay_string;
    std::string payload_text;
    ssp_packet_cl *packet_ptr = NULL;
    unsigned    payload_text_size = 0;
    unsigned    baud_rate = 0;
    unsigned    baud_rate_b;
    unsigned    delay = 0;
    bool        failure = false;

    // Semicolon means preceding opt takes parm

    while ((option = getopt(argc, argv, "i:b:d:p:h")) != -1)
    {
        switch (option)
        {
        case 'i':
            tty_device_name = optarg;
            break;
        case 'b':
            baud_rate_string = optarg;
            break;
        case 'd':
            delay_string = optarg;
            break;
        case 'p':
            payload_text = optarg;
            break;
        case 'h':
            printf("./ssp-sanity -i /dev/tty0 [-d 10] [-p 012345]\n");
            std::exit(0);
            break;
        default:
            printf("Unknown option %c\n", option);
            std::exit(0);
            break;
        }
    }

    // Sanity checks
    if (tty_device_name.size() == 0)
    {
        printf("Missing -i parameter\n");
        std::exit(1);
    }

    // Pad '\0' to std::string
    tty_device_name.push_back('\0');

    // Let linux sanity check tty name
    temp_fd = open(tty_device_name.c_str(), O_RDWR | O_NOCTTY);
    if (temp_fd < 0)
    {
        printf("Can't open tty device <%s>\n", tty_device_name.c_str());
        std::exit(1);
    }
    close(temp_fd);

    // Sanity check baud rate values
    baud_rate_string.push_back('\0');
    (void)rutils_decimal_ascii_to_unsigned32(baud_rate_string.c_str(),
                                             &baud_rate,
                                             &failure);

    baud_rate_b = ssp_framer_cl::baud_rate_lookup(baud_rate);

    if (BAD_BAUD == baud_rate_b)
    {
        printf("Unsupported baud rate %u", baud_rate);
        std::exit(1);
    }


    if (delay_string.size() > 0)
    {
        delay_string.push_back('\0');
        (void)rutils_decimal_ascii_to_unsigned32(delay_string.c_str(),
                                                 &delay,
                                                 &failure);
        printf("Using delay of %u\n", delay);
    }

    payload_text_size = payload_text.size();
    if (payload_text_size > 0)
    {
        if ((payload_text_size & 1) == 1)
        {
            printf("Odd number of payload hex digits (%u)\n", payload_text_size);
            std::exit(1);
        }

        for (unsigned i = 0; i < payload_text_size; i++)
        {
            if (!rutils_is_hex_digit(payload_text.at(i)))
            {
                printf("Bad hex digit in payload at offset %u\n", i);
                std::exit(1);
            }
        }

        // Append a '\0' to std::string type, to make it fully C compatible
        payload_text.push_back('\0');

        // Convert payload to a packet
        packet_ptr = text_string_to_packet(payload_text.c_str());

        if (packet_ptr == NULL)
        {
            printf("Payload parsing error, aborting\n");
            std::exit(1);
        }
    }

    // party is on
    try
    {
        execute_test(tty_device_name.c_str(),
                     baud_rate,
                     delay,
                     packet_ptr);
    }
    catch (std::exception &exc)
    {
        printf("Exception caught for 'execute_test()', %s\n", exc.what()); 
    }

    return 0;
}