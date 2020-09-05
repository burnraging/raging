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

//! @file     ssp-framer.h
//! @authors  Bernie Woodland
//! @date     12Dec18
//!

#ifndef SPP_FRAMER_H_
#define SPP_FRAMER_H_

#include "raging-global.h"
#include "ssp-packet.h"

#include <vector>
#include <list>
#include <string>
#include <termios.h> // 'struct termios'
#include <unistd.h>  // 'struct termios'
#include <pthread.h> // 'pthread_t', 'pthread_mutex_t'
#include <semaphore.h>  // sem_t


#define BAD_BAUD     0xFFFFFFFF


enum framer_error_t
{
    FRAMER_ERROR_NONE,
    FRAMER_ERROR_INVALID_BAUD_RATE,
    FRAMER_ERROR_SEMA_INIT_FAILED,
    FRAMER_ERROR_PIPE_CREATE_FAILED,
    FRAMER_ERROR_TTY_DEVICE_FAILED_OPEN,
    FRAMER_ERROR_START_SEQUENCING,
    FRAMER_ERROR_PTHREAD,
};

enum ssp_rx_mode_t
{
    SSP_SYNC_DATA,        // scanning frame payload
    SSP_SYNC_MAGIC1,      // searching for magic #1 value
    SSP_SYNC_MAGIC2,      // searching for magic #2 value
    SSP_SYNC_LENGTH_HI,   // scanning MSByte of frame length field
    SSP_SYNC_LENGTH_LO    // scanning LSByte of frame length field
};


class ssp_framer_cl
{
public:
    ssp_framer_cl(const char *tty_interface_name,
                  unsigned    baud_rate,
                  void (*callback_fcn_ptr)(void *) = NULL,
                  unsigned    channel_number = 0);
    ~ssp_framer_cl(void);

    framer_error_t start(void);
    void           stop(void);
    void           tx_packet(ssp_packet_cl * packet_ptr);
    ssp_packet_cl *get_rx_packet(void);

    static unsigned baud_rate_lookup(unsigned baud_rate_actual);

    std::vector<uint8_t>        m_rx_debug_snapshot;

    // counters
    unsigned       m_rx_packet_count;
    unsigned       m_rx_byte_count;
    unsigned       m_rx_error_count;
    unsigned       m_rx_sync_count;
    unsigned       m_rx_bad_crc_count;
    unsigned       m_tx_packet_count;
    unsigned       m_tx_byte_count;

private:
    // No default constructors allowed
    ssp_framer_cl()
    {}
    // No Copy constructor, Assignment operator
    ssp_framer_cl(const ssp_framer_cl &src)
    {}
    void operator=(const ssp_framer_cl &src)
    {}

    static void *rx_thread(void *arg);
    static void *tx_thread(void *arg);

    std::string    m_tty_interface_name;
    unsigned       m_baud_rate_actual;
    unsigned       m_baud_rate;
    bool           m_did_save_oldtios;
    struct termios m_oldtios;
    struct termios m_newtios;
    pthread_t      m_rx_thread;
    pthread_t      m_tx_thread;

    int             m_fd;
    pthread_mutex_t m_rx_mutex;
    pthread_mutex_t m_tx_mutex;
    bool            m_rx_thread_active;
    bool            m_tx_thread_active;
    int             m_rx_pipe_fd[2];
    unsigned        m_channel_number;
    bool            m_kill_requested;  // better than a pthread_cancel()

    std::list<ssp_packet_cl *>  m_rx_packet_queue;
    std::list<ssp_packet_cl *>  m_tx_packet_queue;

    void (*m_rx_notify_callback_fcn_ptr)(void *this_framer);

    sem_t          m_tx_sema;

    ssp_rx_mode_t  m_rx_mode;

};

#endif  // SPP_FRAMER_H_