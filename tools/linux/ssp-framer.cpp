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

//! @file     ssp-framer.cpp
//! @authors  Bernie Woodland
//! @date     7Dec18
//!
//! @brief    Producer-Consumer object for tx and rx of SPP frames
//!
//! @details  Class to manage SPP running on some /dev/ttyX device.
//! @details  One instance per tty device.
//! @details  The user of this class will use this to do the following:
//! @details     (1) Mount a tty device
//! @details     (2) Send SPP frames to this device/receive frames from it
//! @details     (3) Manage queues so user can queue up packets for tx
//! @details         and this class queues up rx packets for user
//! @details     (4) Take packets from frames and create SPP frames out of
//! @details         them and vice-versa.
//! @details     (5) Notify user of rx packets, if user so desires
//! @details  
//! @details  This object can only be used on linux server
//! @details  Link dependencies: raging-utils.c, raging-utils-crc.c
//! @details  
//! @details  
//!

// based partly on:
// http://tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html

#include "raging-global.h"
#include "raging-utils.h"
#include "raging-utils-mem.h"
#include "raging-utils-crc.h"

#include "ssp-framer.h"
#include "linux-utils.h"

#include <vector>
#include <list>
#include <cstring>
#include <cstdlib>    // 'std::exit()'

#include <pthread.h>  // all 'pthread*' symbols
#include <termios.h>  // 'struct termios', 'tcgetattr()', 'B115200', 'tcdrain(), etc
#include <unistd.h>   // (same) + 'close()', 'read()', 'write()'
//#include <sys/types.h> // 'open()', 'select()'
#include <sys/stat.h>  // 'open()'
#include <fcntl.h>     // 'open()', O_* constants
#include <semaphore.h> // 'sem_init()', 'sem_wait()', 'sem_post()'
#include <time.h>      // 'select()'
#include <sys/select.h> // 'select()'
#include <unistd.h>    // 'select()', 'pipe()', 'fcntl()'

// **** Start Mocked values

//  Duplicated from 'ssp-driver.h"
#define SSP_MAGIC_NUMBER1        0x7E
#define SSP_MAGIC_NUMBER2        0xA5
#define SSP_MAGIC_NUMBER_SIZE       2

// **** End Mocked values


uint8_t  linear_buf[10000];
unsigned linear_length;


// Sole means of creating an ssp-framer-cl object.
// This creates object but doesn't start running anything
//
// 'tty_interface_name'-- "/dev/tty1" for example
// 'baud_rate'-- one of B115200, B38400, B19200, B9600, etc,
//               which are defined in termios.h
// 'callback_fcn_ptr'-- optional callback fcn upon packet rx
ssp_framer_cl::ssp_framer_cl(const char *tty_interface_name,
                             unsigned    baud_rate,
                             void (*callback_fcn_ptr)(void *),
                             unsigned channel_number)
{
    pthread_mutex_init(&m_rx_mutex, NULL);
    pthread_mutex_init(&m_tx_mutex, NULL);

    m_fd = -1;
    std::memset(&m_oldtios, 0, sizeof(m_oldtios));
    std::memset(&m_newtios, 0, sizeof(m_oldtios));
    std::memset(&m_rx_thread, 0, sizeof(m_rx_thread));
    std::memset(&m_tx_thread, 0, sizeof(m_tx_thread));

    m_tty_interface_name = tty_interface_name;
    m_tty_interface_name.push_back('\0');       // fully C compatible now
    m_baud_rate_actual = baud_rate;
    m_did_save_oldtios = false;
    m_rx_thread_active = false;
    m_tx_thread_active = false;
    m_channel_number = channel_number;

    m_rx_notify_callback_fcn_ptr = callback_fcn_ptr;
}

ssp_framer_cl::~ssp_framer_cl(void)
{
    pthread_mutex_destroy(&m_rx_mutex);
    pthread_mutex_destroy(&m_tx_mutex);
}

// Start running.
// This will spawn 2 threads: a tx thread and an rx thread
// Rx thread will listen on /dev/ttyX; tx thread will
// send bytes out /dev/ttyX
//
// Must call '::stop()' before calling '::start()' again
//
framer_error_t ssp_framer_cl::start(void)
{
    int  rv;

    m_kill_requested = false;

    m_baud_rate = baud_rate_lookup(m_baud_rate_actual);

    if (BAD_BAUD == m_baud_rate)
    {
        return FRAMER_ERROR_INVALID_BAUD_RATE;
    }

    if (sem_init(&m_tx_sema, 0, 0) != 0)
    {
        return FRAMER_ERROR_SEMA_INIT_FAILED;
    }

    if (pipe(m_rx_pipe_fd) == -1)
    {
        return FRAMER_ERROR_PIPE_CREATE_FAILED;
    }

    if (m_fd != -1)
    {
        return FRAMER_ERROR_START_SEQUENCING;
    }
            
    m_fd = open(m_tty_interface_name.c_str(), O_RDWR | O_NOCTTY);

    if (m_fd < 0)
    {
        return FRAMER_ERROR_TTY_DEVICE_FAILED_OPEN;
    }

    // fetch existing settings
    tcgetattr(m_fd, &m_oldtios);
    m_did_save_oldtios = true;

    // Init rx variables
    m_rx_mode = SSP_SYNC_MAGIC1;

    rv = pthread_create(&m_rx_thread, NULL, ssp_framer_cl::rx_thread, this);
    if (rv < 0)
    {
        return FRAMER_ERROR_PTHREAD;
    }
    m_rx_thread_active = true;


    rv = pthread_create(&m_tx_thread, NULL, ssp_framer_cl::tx_thread, this);
    if (rv < 0)
    {
        return FRAMER_ERROR_PTHREAD;
    }
    m_tx_thread_active = true;

#if 1
    std::memset(&m_newtios, 0, sizeof(m_newtios));
    // Won't work without CRTSCTS.
    // Maybe the USB part of the USB-serial has h/w flow cntrl
//    m_newtios.c_cflag = m_baud_rate | CS8 | CLOCAL | CREAD;
    m_newtios.c_cflag = m_baud_rate | CS8 | CLOCAL | CREAD | CRTSCTS;
    m_newtios.c_iflag = IGNPAR;
    m_newtios.c_oflag = 0;

    // set input mode (non-canonical, no echo,...)
    m_newtios.c_lflag = 0;

    // http://www.unixwiz.net/techtips/termios-vmin-vtime.html
    m_newtios.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    m_newtios.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */
#else
    cfmakeraw(&m_newtios);
    cfsetispeed(&m_newtios, m_baud_rate);
    cfsetospeed(&m_newtios, m_baud_rate);
    // Won't work without CRTSCTS.
    // Maybe the USB part of the USB-serial has h/w flow cntrl
    m_newtios.c_cflag |= CRTSCTS;
    // Explained in http://www.unixwiz.net/techtips/termios-vmin-vtime.html
    // If number of bytes in read() hasn't been met, then read() will return
    // when:
    //   1) Data is available but no data received for VTIME interval
    //      (units are 10's of millisecs
    //   2) VMIN bytes are received.
    //
    // Neither of these conditions should make any difference since
    // we're reading 1 byte at a time.
    m_newtios.c_cc[VTIME]    = 1;   // 10 millisecs
    m_newtios.c_cc[VMIN]     = 6;   // return from read after 6 bytes
#endif
  
    tcflush(m_fd, TCIOFLUSH);
    tcsetattr(m_fd, TCSANOW, &m_newtios);

    // Reset counters
    m_rx_packet_count = 0;
    m_rx_byte_count = 0;
    m_rx_error_count = 0;
    m_rx_sync_count = 0;
    m_rx_bad_crc_count = 0;
    m_tx_packet_count = 0;
    m_tx_byte_count = 0;

    return FRAMER_ERROR_NONE;
}

// Stops rx and tx, resets most everything
void ssp_framer_cl::stop(void)
{
    int sema_value = 0;

    // Tell tx, rx threads to terminate themselves
    m_kill_requested = true;

    // Kill rx thread
    if (m_rx_thread_active)
    {
        // Kick rx task, to get if off the 'select()' call
        char holder = 0xAA;
        write(m_rx_pipe_fd[1], &holder, 1);

        pthread_join(m_rx_thread, NULL);

        m_rx_thread_active = false;
    }

    // Kill tx thread
    if (m_tx_thread_active)
    {
        // Kick rx thread so it'll loop around, recognize 'm_kill_requested'
        // and exit thread safely
        sem_post(&m_tx_sema);

        pthread_join(m_tx_thread, NULL);

        m_tx_thread_active = false;
    }

    // Need is to reset semaphore count
    sem_destroy(&m_tx_sema);

    close(m_rx_pipe_fd[0]);   // read end
    close(m_rx_pipe_fd[1]);   // write end

    // Was /dev/ttyX successfully opened?
    if (m_fd != -1)
    {
        if (m_did_save_oldtios)
        {
            // restore /dev/ttyX device settings
            tcsetattr(m_fd, TCSANOW, &m_oldtios);
            m_did_save_oldtios = false;
        }

        // Problems closing file. Verify that it is still open!
        bool file_is_open = is_file_open(m_fd);

        if (file_is_open)
        {
            close(m_fd);
        }

        m_fd = -1;
    }

    // drain queues
    while (m_rx_packet_queue.size() > 0)
    {
        ssp_packet_cl *packet_ptr = m_rx_packet_queue.back();
        delete packet_ptr;
        m_rx_packet_queue.pop_back();
    }

    while (m_tx_packet_queue.size() > 0)
    {
        ssp_packet_cl *packet_ptr = m_tx_packet_queue.back();
        delete packet_ptr;
        m_tx_packet_queue.pop_back();
    }
}

// User must create 'packet_ptr' by doing a 'ssp_packet_cl x = new ssp_packet_cl()',
// fill out packet, then pass 'x' in as 'packet_ptr' parm
void ssp_framer_cl::tx_packet(ssp_packet_cl * packet_ptr)
{
    pthread_mutex_lock(&m_tx_mutex);

    // Enqueue packet for tx thread's consumption
    m_tx_packet_queue.push_back(packet_ptr);

    pthread_mutex_unlock(&m_tx_mutex);

    // Kick tx thread
    sem_post(&m_tx_sema);
}

// Take next packet off rx queue.
// Returns NULL if no packets waiting
ssp_packet_cl *ssp_framer_cl::get_rx_packet(void)
{
    ssp_packet_cl *packet_ptr = NULL;

    pthread_mutex_lock(&m_rx_mutex);

    if (m_rx_packet_queue.size() > 0)
    {
        packet_ptr = m_rx_packet_queue.front();

        m_rx_packet_queue.pop_front();
    }

    pthread_mutex_unlock(&m_rx_mutex);

    return packet_ptr;
}

unsigned ssp_framer_cl::baud_rate_lookup(unsigned baud_rate_actual)
{
    if (baud_rate_actual == 115200)
        return B115200;
    if (baud_rate_actual == 38400)
        return B38400;
    if (baud_rate_actual == 19200)
        return B19200;
    if (baud_rate_actual == 9600)
        return B9600;

    return BAD_BAUD;
}

void *ssp_framer_cl::rx_thread(void *arg)
{
    ssp_framer_cl       *iptr = reinterpret_cast<ssp_framer_cl *>(arg);
    ssp_rx_mode_t        rx_mode = SSP_SYNC_MAGIC1;
    ssp_packet_cl       *packet_ptr = NULL;
    fd_set               read_fds;
    int                  highest_fd;
    char                 this_char;
    uint16_t             x;
    uint16_t             running_crc = 0;
    uint16_t             rx_frame_length_field = 0;
    uint16_t             rx_frame_length_current = 0;

    FD_ZERO(&read_fds);
    FD_SET(iptr->m_fd, &read_fds);
    FD_SET(iptr->m_rx_pipe_fd[0], &read_fds);

    // calculate highest fd +1
    highest_fd = iptr->m_fd;
    if (iptr->m_rx_pipe_fd[0] > highest_fd)
    {
        highest_fd = iptr->m_rx_pipe_fd[0];
    }

    // Clear any data waiting on device
    clear_read_data(iptr->m_fd);

    while (1)
    {
        // Block until we receive an input on the pipe or the tty
        if (select(highest_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
        {
            // should never get here
            break;
        }

        // Were we kicked by pipe?
        if (FD_ISSET(iptr->m_rx_pipe_fd[0], &read_fds) || iptr->m_kill_requested)
        {
            // Normal exit path
            break;
        }

        // Get a single character off /dev/ttyX port
        // Shouldn't block, since 'select()' at this point must've been for the tty
        if (read(iptr->m_fd, &this_char, 1) < 0)
        {
            // should never get here
            iptr->m_rx_error_count++;
        }

        // Record debug
        if (linear_length < sizeof(linear_buf))
        {
            linear_buf[linear_length++] = this_char;
            iptr->m_rx_debug_snapshot.push_back(this_char);
        }

        // What synchronization state are we in?
        switch (iptr->m_rx_mode)
        {
        // Frame synchronized, and we're gathering payload bytes
        case SSP_SYNC_DATA:

            // Put character into packet buffer
            packet_ptr->append_bytes(this_char);
            rx_frame_length_current++; 

            // Do CRC inline here
            // Equivalent to rutils_crc16_add_string() unwound
            x = (running_crc ^ this_char) & BIT_MASK8;
            x = (x ^ (x << 4)) & BIT_MASK8;
            x = (x << 8) ^ (x << 3) ^ (x >> 4);
            running_crc = (running_crc >> BITS_PER_WORD8) ^ x;

            // Reached end of frame?
            if (rx_frame_length_field == rx_frame_length_current)
            {
                // CRC ok?
                if (RUTILS_CRC16_GOOD == running_crc)
                {
                    // Strip CRC off packet (2 bytes)
                    packet_ptr->buffer.pop_back();
                    packet_ptr->buffer.pop_back();

                    // Guard packet qeuue from collision from consumer thread
                    pthread_mutex_lock(&iptr->m_rx_mutex);

                    // Enqueue packet pointer (not packet itself) to rx queue
                    iptr->m_rx_packet_queue.push_back(packet_ptr);

                    pthread_mutex_unlock(&iptr->m_rx_mutex);

                    // Did ssp_framer_cl user register for a callback on rx packet?
                    if (NULL != iptr->m_rx_notify_callback_fcn_ptr)
                    {
                        // Notify ssp_framer_cl user of rx packet
                        iptr->m_rx_notify_callback_fcn_ptr(iptr);
                    }

                    iptr->m_rx_packet_count++;
                }
                // Otherwise, discard packet
                else
                {
                    delete packet_ptr;
                }

                // Regardless, configure to resync on next frame's magic number
                iptr->m_rx_mode = SSP_SYNC_MAGIC1;
            }
            // We need more bytes to finish frame
            else
            {
                // Carry over CRC calculation for next string of the payload
                running_crc = running_crc;
            }

            break;

        // Synchronizing: scanning for magic number #1
        case SSP_SYNC_MAGIC1:

            if (SSP_MAGIC_NUMBER1 == this_char)
            {
                 iptr->m_rx_mode = SSP_SYNC_MAGIC2;
            }

            break;

        // Synchronizing: scanning for magic number #2
        case SSP_SYNC_MAGIC2:

            if (SSP_MAGIC_NUMBER2 == this_char)
            {
                 iptr->m_rx_mode = SSP_SYNC_LENGTH_HI;
            }
            else
            {
                 iptr->m_rx_mode = SSP_SYNC_MAGIC1;
            }

            break;

        // Synchronizing: this byte is frame length MSByte
        case SSP_SYNC_LENGTH_HI:

            rx_frame_length_field = (unsigned)this_char << BITS_PER_WORD8;
            iptr->m_rx_mode = SSP_SYNC_LENGTH_LO;

            break;

        // Synchronizing: this byte is frame length LSByte
        case SSP_SYNC_LENGTH_LO:

            rx_frame_length_field |= this_char;

            // Sanity check length
            if ((rx_frame_length_field <= SSP_DEFAULT_MAX_PACKET) &&
                (rx_frame_length_field >= RUTILS_CRC16_SIZE))
            {
                // Frame sync successful. Set up for new frame now

                iptr->m_rx_mode = SSP_SYNC_DATA;
                rx_frame_length_current = 0;

                // Sanity check, should always pass
                if (NULL == packet_ptr)
                {
                    packet_ptr = new ssp_packet_cl(SSP_DEFAULT_MAX_PACKET,
                                                   iptr->m_channel_number);
                }
                // Else, fail
                else
                {
                    // TBD
                    packet_ptr->buffer.clear();
                }

                running_crc = 0xFFFF;   // rutils_crc16_start() unwound
                iptr->m_rx_sync_count++;
            }
            else
            {
                // Failed length sanity check: resync on next packet
                iptr->m_rx_mode = SSP_SYNC_MAGIC1;
            }

            break;

        default:
            // won't get here
            break;
        }
    }

    // Push the entire debug buffer onto 'm_rx_debug_snapshot'.
    // User of this object will decide what to do with it.
//    iptr->m_rx_debug_snapshot.insert(
//                    iptr->m_rx_debug_snapshot.end(),
//                    linear_buf,
//                    linear_buf + linear_length); 

    iptr->m_rx_thread_active = false;
}

void *ssp_framer_cl::tx_thread(void *arg)
{
    ssp_framer_cl       *iptr = reinterpret_cast<ssp_framer_cl *>(arg);
    ssp_packet_cl       *packet_ptr;

    // Wait for entity using this object to call
    // 'ssp_framer_cl::tx_packet'
    while (sem_wait(&iptr->m_tx_sema) >= 0)
    {
        packet_ptr = NULL;

        pthread_mutex_lock(&iptr->m_tx_mutex);

        // Sanity check. This should always pass
        if (iptr->m_tx_packet_queue.size() > 0)
        {
            packet_ptr = iptr->m_tx_packet_queue.front();

            iptr->m_tx_packet_queue.pop_front();
        }

        pthread_mutex_unlock(&iptr->m_tx_mutex);

        // Sanity check, should always pass except when thread is being killed
        if (NULL != packet_ptr)
        {
            // Send frame header magic numbers
            const uint8_t header[] = {SSP_MAGIC_NUMBER1, SSP_MAGIC_NUMBER2};
            write(iptr->m_fd, header, sizeof(header));
            iptr->m_tx_byte_count += sizeof(header);

            uint16_t packet_size = packet_ptr->size();
            uint16_t packet_size_with_crc = packet_size + RUTILS_CRC16_SIZE;

            // Calculate CRC
            uint16_t calculated_crc = rutils_crc16_start();

            for (unsigned i = 0; i < packet_size; i++)
            {
                uint8_t char_to_crc = packet_ptr->buffer.at(i);

                calculated_crc = rutils_crc16_add_string(calculated_crc, &char_to_crc, 1);
            }

            // Send frame size
            // Size must include CRC
            uint8_t length_buffer[BYTES_PER_WORD16];
            rutils_word16_to_stream(length_buffer, packet_size_with_crc);
            write(iptr->m_fd, length_buffer, BYTES_PER_WORD16);
            iptr->m_tx_byte_count += BYTES_PER_WORD16;

            // Transmit packet 1 byte at a time
            for (unsigned i = 0; i < packet_size; i++)
            {
                uint8_t character = 0;

                if (packet_ptr->pop_front(&character))
                {
                    // Write a single character to /dev/ttyX port
                    if (write(iptr->m_fd, &character, 1) == 1)
                    {
                        iptr->m_tx_byte_count++;
                    }
                }
            }

            // Send CRC
            // CRC on wire must be 1's complement and sent little-endian
            calculated_crc = 0xFFFF ^ calculated_crc;
            uint8_t crc_buffer[RUTILS_CRC16_SIZE];
            rutils_word16_to_stream_little_endian(crc_buffer, calculated_crc);
            write(iptr->m_fd, crc_buffer, RUTILS_CRC16_SIZE);
            iptr->m_tx_byte_count += RUTILS_CRC16_SIZE;

            // Problem: write() buffers short data transmits (like small
            // ssp-sanity frame) and doesn't
            // send them untils tty fd is closed. Ideally want to drain tx
            // buffer when last byte in frame is sent.
            //   https://stackoverflow.com/questions/3228681/c-write-doesnt-send-data-until-closefd-is-called
            //
            // Doing an open() on tty device with O_SYNC doesn't fix problem
            //
            // Problem: tcdrain() hangs
            // Problem: fsync() isn't working. Online comment is that fsync() doesn't apply
            //          to tty devices
            //
            //tcdrain(iptr->m_fd);
            fsync(iptr->m_fd);

            iptr->m_tx_packet_count++;

#if 0
            // Send extra bytes to force-drain packet transmit
            if (iptr->m_tx_packet_queue.size() == 0)
            {
                uint8_t padding[] = {0xBE, 0xEF, 0xCA, 0xFE};

                for (unsigned j = 0; j < 100/sizeof(padding); j++)
                {
                    write(iptr->m_fd, padding, sizeof(padding));
                }
            }
#endif

            delete packet_ptr;
        }
        else
        {
            // Failure, should never get here
        }

        if (iptr->m_kill_requested)
        {
            // Normal exit path
            break;
        }
    }

    iptr->m_tx_thread_active = false;
}