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
//******     Start: Move to a header file *****//

#include "raging-utils-os.h"
#include "raging-utils-mem.h"
#include "nsvc-app.h"
#include "nufr-api.h"

//*** Start hacks
#define SOME_PIPE_PREFIX   1   // change this!! See 'nsvc_msg_prefix_t'
#define MUTEX_GO_MAKE_ONE  10  // Add to 'nsvc_mutex_t'
#define MY_READ_TASK_TID   2   // Add to 'nufr_tid_t'
//*** End hacks

typedef enum
{
    ID_PIPE_START,
    ID_PIPE_FILLING,
    ID_PIPE_END
} id_pipe_t;

//!
//! @struct    xyz_pipe_t
//!
//! @brief     pipe handle
//!
typedef struct
{
    rutils_fifo_t  fifo;
    nsvc_mutex_t   pipe_mutex;
    nufr_msg_pri_t abort_priority;
    nufr_tid_t     consumer_task;
    uint16_t       msg_prefix;
    uint16_t       msg_id_start_fill;
    uint16_t       msg_id_filling;
    uint16_t       msg_id_end_fill;
    uint16_t       bop_key;
} xyz_pipe_t;

//!
//! @enum      xyz_pipe_write_rtn_t
//!
//! @brief     Return value for piep write API calls
//!
typedef enum
{
    XYZ_PWRTN_OK,
    XYZ_PWRTN_MSG_ABORT,
    XYZ_PWRTN_TIMEOUT
} xyz_pipe_write_rtn_t;

xyz_pipe_write_rtn_t xyz_pipe_writeW(xyz_pipe_t    *pipe_ptr,
                                     const uint8_t *buffer,
                                     unsigned       buffer_length);
xyz_pipe_write_rtn_t xyz_pipe_writeT(xyz_pipe_t    *pipe_ptr,
                                     const uint8_t *buffer,
                                     unsigned       buffer_length,
                                     unsigned      *write_count_ptr,
                                     unsigned       timeout_ticks);
unsigned xyz_pipe_read(xyz_pipe_t *pipe_ptr,
                       uint8_t    *buffer,
                       unsigned    buffer_length,
                       nufr_tid_t  task_to_ack);

//******     End: Move to a header file *****//

#include "nsvc-api.h"
#include "raging-utils.h"

const uint8_t dummy_data[] = {
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
  1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
};

//
// There is no 'xyz_pipe_init()' API
// Instead, to init do this:
//
//  (1) Make this call:
//     rutils_fifo_init(pipe_ptr->fifo, buffer_to_use, buffer_size);
//
//  (2) Assign values to these fields:
//     pipe_ptr->abort_priority
//     pipe_ptr->consumer_task
//     pipe_ptr->msg_prefix
//     pipe_ptr->msg_id_start_fill
//

//!
//! @name      xyz_pipe_writeW
//!
//! @brief     Push data into a pipe
//!
//! @details   Calling task will block until all bytes written.
//! @details   This API is reentrant/can be called by multiple tasks.
//!
//! @details   'pipe_ptr' fields:
//! @details      'abort_priority'-- if NUFR_CS_TASK_KILL enabled,
//! @details              then msg priority level that will allow a message
//! @details              send to abort any wait this API makes
//! @details      'consumer_task'-- task which reads fifo.
//! @details              Task to send fifo start, full messages.
//! @details      'msg_prefix'--
//! @details      'msg_id_start_fill'-- First write to fifo when empty
//! @details              sends 'msg_prefix'+ this ID. This wakes up consumer
//! @details              task.
//!
//! @param[in] 'pipe_ptr'-- pipe handle
//! @param[in] 'buffer'-- bytes to write
//! @param[in] 'buffer_length'-- size of 'buffer'/number of bytes to write
//!
//! @return     XYZ_PWRTN_OK. If NUFR_CS_TASK_KILL and there was a msg
//! @return       abort, will send XYZ_PWRTN_MSG_ABORT
//!
xyz_pipe_write_rtn_t xyz_pipe_writeW(xyz_pipe_t       *pipe_ptr,
                                     const uint8_t    *buffer,
                                     unsigned          buffer_length)
{
    nufr_sema_get_rtn_t mutex_return_value;
    nufr_bop_wait_rtn_t bop_return_value = NUFR_BOP_WAIT_OK;
    unsigned            write_length;

    // Is there a valid mutex associated with this pipe?
    // If there isn't, then we're a single task writer
    if (NSVC_MUTEX_null != pipe_ptr->pipe_mutex)
    {
        mutex_return_value = nsvc_mutex_getW(pipe_ptr->pipe_mutex,
                                             pipe_ptr->abort_priority);

    #if NUFR_CS_TASK_KILL == 1
        if (NUFR_SEMA_GET_MSG_ABORT == mutex_return_value)
        {
            return XYZ_PWRTN_MSG_ABORT;
        }
    #endif
    }

    // Initiate transaction
    (void)nsvc_msg_send_argsW(
                     pipe_ptr->msg_prefix,
                     pipe_ptr->msg_id_start_fill,
                     NUFR_MSG_PRI_MID,
                     pipe_ptr->consumer_task,
                     (uint32_t)pipe_ptr);

    // Write a chunk, wait for read task to consume it,
    //   write another chunk, etc, until finished    
    while (buffer_length > 0)
    {
        // Fill fifo with as many bytes as possible.
        write_length = rutils_fifo_write(&pipe_ptr->fifo, buffer, buffer_length);

        (void)nsvc_msg_send_and_bop_waitW(
                         pipe_ptr->msg_prefix,
                         pipe_ptr->msg_id_filling,
                         NUFR_MSG_PRI_MID,
                         pipe_ptr->consumer_task,
                         (uint32_t)pipe_ptr,
                         pipe_ptr->abort_priority);

    #if NUFR_CS_TASK_KILL == 1
        if (NUFR_BOP_WAIT_ABORTED_BY_MESSAGE == bop_return_value)
        {
            break;
        }
    #endif
        buffer_length -= write_length;
        buffer += write_length;
    }

    // Terminate transaction
    (void)nsvc_msg_send_argsW(
                     pipe_ptr->msg_prefix,
                     pipe_ptr->msg_id_end_fill,
                     NUFR_MSG_PRI_MID,
                     pipe_ptr->consumer_task,
                     (uint32_t)pipe_ptr);
    
    if (NSVC_MUTEX_null != pipe_ptr->pipe_mutex)
    {
        (void)nsvc_mutex_release(pipe_ptr->pipe_mutex);
    }

#if NUFR_CS_TASK_KILL == 1
    if (NUFR_BOP_WAIT_ABORTED_BY_MESSAGE == bop_return_value)
    {
        return XYZ_PWRTN_MSG_ABORT;
    }
#endif

    return XYZ_PWRTN_OK;
}

//!
//! @name      xyz_pipe_writeT
//!
//! @brief     Same as xyz_pipe_writeW(), but with a timeout
//!
//! @details   (see xyz_pipe_writeW())
//!
//! @param[in] 'pipe_ptr'-- pipe handle
//! @param[in] 'buffer'-- bytes to write
//! @param[in] 'buffer_length'-- size of 'buffer'/number of bytes to write
//! @param[out] 'write_count_ptr'-- number of bytes written.
//! @param[in] 'timeout_ticks'--
//!
//! @return     XYZ_PWRTN_OK. XYZ_PWRTN_TIMEOUT if timeout.
//! @return       If NUFR_CS_TASK_KILL and there was a msg
//! @return       abort, will send XYZ_PWRTN_MSG_ABORT
//!
xyz_pipe_write_rtn_t xyz_pipe_writeT(xyz_pipe_t    *pipe_ptr,
                                     const uint8_t *buffer,
                                     unsigned       buffer_length,
                                     unsigned      *write_count_ptr,
                                     unsigned       timeout_ticks)
{
    nufr_sema_get_rtn_t mutex_return_value;
    nufr_bop_wait_rtn_t bop_return_value = NUFR_BOP_WAIT_OK;
    unsigned            write_length;
    unsigned            start_buffer_length = buffer_length;
    uint32_t            start_ticks;
    uint32_t            elapsed_ticks;

    // Timeout value 'timeout_ticks' is total timeout for all
    // API wait calls. We must maintain elapsed ticks to get it right.
    start_ticks = nufr_tick_count_get();

    *write_count_ptr = 0;

    if (NSVC_MUTEX_null != pipe_ptr->pipe_mutex)
    {
        mutex_return_value = nsvc_mutex_getT(pipe_ptr->pipe_mutex,
                                             pipe_ptr->abort_priority,
                                             timeout_ticks);

        if (NUFR_BOP_WAIT_TIMEOUT == mutex_return_value)
        {
            return XYZ_PWRTN_TIMEOUT;
        }
    #if NUFR_CS_TASK_KILL == 1
        else if (NUFR_SEMA_GET_MSG_ABORT == mutex_return_value)
        {
            return XYZ_PWRTN_MSG_ABORT;
        }
    #endif
    }

    // Initiate transaction
    (void)nsvc_msg_send_argsW(
                     pipe_ptr->msg_prefix,
                     pipe_ptr->msg_id_start_fill,
                     NUFR_MSG_PRI_MID,
                     pipe_ptr->consumer_task,
                     (uint32_t)pipe_ptr);

    // Write a chunk, wait for read task to consume it,
    //   write another chunk, etc, until finished    
    while (buffer_length > 0)
    {
        // Fill fifo with as many bytes as possible.
        write_length = rutils_fifo_write(&pipe_ptr->fifo, buffer, buffer_length);

        // Calculate elapsed ticks since our entry, so we can adjust
        // timeout accordingly.
        elapsed_ticks = nufr_tick_count_delta(start_ticks);
        if (elapsed_ticks < timeout_ticks)
        {
            bop_return_value = XYZ_PWRTN_TIMEOUT;
            break;
        }

        (void)nsvc_msg_send_and_bop_waitT(
                         pipe_ptr->msg_prefix,
                         pipe_ptr->msg_id_filling,
                         NUFR_MSG_PRI_MID,
                         pipe_ptr->consumer_task,
                         (uint32_t)pipe_ptr,
                         pipe_ptr->abort_priority,
                         timeout_ticks - elapsed_ticks);

        if (NUFR_BOP_WAIT_TIMEOUT == bop_return_value)
        {
            break;
        }
    #if NUFR_CS_TASK_KILL == 1
        else if (NUFR_BOP_WAIT_ABORTED_BY_MESSAGE == bop_return_value)
        {
            break;
        }
    #endif

        buffer_length -= write_length;
        buffer += write_length;
    }

    // Terminate transaction
    (void)nsvc_msg_send_argsW(
                     pipe_ptr->msg_prefix,
                     pipe_ptr->msg_id_end_fill,
                     NUFR_MSG_PRI_MID,
                     pipe_ptr->consumer_task,
                     (uint32_t)pipe_ptr);
    
    if (NSVC_MUTEX_null != pipe_ptr->pipe_mutex)
    {
        (void)nsvc_mutex_release(pipe_ptr->pipe_mutex);
    }

    *write_count_ptr = start_buffer_length - buffer_length;

    if (NUFR_BOP_WAIT_TIMEOUT == bop_return_value)
    {
        return XYZ_PWRTN_TIMEOUT;
    }
#if NUFR_CS_TASK_KILL == 1
    else if (NUFR_BOP_WAIT_ABORTED_BY_MESSAGE == bop_return_value)
    {
        return XYZ_PWRTN_MSG_ABORT;
    }
#endif

    return XYZ_PWRTN_OK;
}

//!
//! @name      xyz_pipe_read
//!
//! @brief     Consumer task calls this to read from the pipe
//!
//! @details   (see xyz_pipe_writeW())
//! @details   It's assumed that the consumer will have been notified
//! @details   by message, and will send writer a bop if necessary,
//! @details   depending on message.
//!
//! @param[in] 'pipe_ptr'-- pipe handle
//! @param[in] 'buffer'-- place to put bytes read into
//! @param[in] 'buffer_length'-- size of 'buffer'/max bytes to read
//! @param[in] 'task_to_ack'-- writer task to send bop to.
//! @param[in]          Set to 'NUFR_TID_null' if no bop send 
//! @param[in] 'bop_key'-- if sending a bop, use this key
//!
//! @return     Number of bytes read
//!
unsigned xyz_pipe_read(xyz_pipe_t *pipe_ptr,
                       uint8_t    *buffer,
                       unsigned    buffer_length,
                       nufr_tid_t  task_to_ack)
{
    unsigned read_length;

    // Was this sent from a valid task?
    if (NUFR_TID_null != task_to_ack)
    {
        // Lock caller, since we're using data on caller's stack and
        //  caller could timeout or something
        if (NUFR_BOP_RTN_TAKEN ==
             nufr_bop_lock_waiter(task_to_ack, pipe_ptr->bop_key))
        {
            read_length = rutils_fifo_read(&pipe_ptr->fifo, buffer, buffer_length);

            nufr_bop_unlock_waiter(task_to_ack);

            // Release waiting task
            (void)nufr_bop_send(task_to_ack, pipe_ptr->bop_key);
        }
        // Caller timed out already
        else
        {
            read_length = 0;
        }
    }
    // Sent from elsewhere
    else
    {
        read_length = rutils_fifo_read(&pipe_ptr->fifo, buffer, buffer_length);
    }

    return read_length;
}


// Example of a task doing writes
void my_write_task_entry(unsigned parameter)
{
    xyz_pipe_t write_pipe;
    uint8_t    pipe_buffer[30];

    // Initialize 'write_pipe' members
    rutils_memset(&write_pipe, 0, sizeof(write_pipe));
    rutils_fifo_init(&(write_pipe.fifo), pipe_buffer, sizeof(pipe_buffer));
    write_pipe.pipe_mutex = MUTEX_GO_MAKE_ONE;
    write_pipe.abort_priority = NUFR_MSG_PRI_MID;
    write_pipe.consumer_task = MY_READ_TASK_TID;
    write_pipe.msg_id_start_fill = ID_PIPE_START;
    write_pipe.msg_id_filling = ID_PIPE_FILLING;
    write_pipe.msg_id_end_fill = ID_PIPE_END;

    // Write repeatedly
    while (1)
    {
        xyz_pipe_writeW(&write_pipe, dummy_data, sizeof(dummy_data));

        nufr_sleep(10, NUFR_NO_ABORT);
    }
}

// Example of a task doing reads
void my_read_task_entry(unsigned parameter)
{
    nsvc_msg_prefix_t prefix;
    uint16_t          msg_id;
    nufr_tid_t        source_task;
    uint32_t          msg_parameter;
    xyz_pipe_t       *pipe_ptr;
    uint8_t           read_buffer[10];

    while (1)
    {
        // Monitor all messages, not just from pipes
        nsvc_msg_get_argsW(&prefix,
                           &msg_id,
                           NULL,
                           &source_task,
                           &msg_parameter);


        if (SOME_PIPE_PREFIX == prefix)
        {
            switch (msg_id)
            {
            case ID_PIPE_START:
                // TBD: Some setup logic
                break;

            case ID_PIPE_FILLING:

                pipe_ptr = (xyz_pipe_t *)msg_parameter;

                xyz_pipe_read(pipe_ptr,
                              read_buffer,
                              sizeof(read_buffer),
                              source_task);

                // TBD: do something with 'buffer' here

                break;

            case ID_PIPE_END:
                // TBD: Some teardown logic
                break;

            }
        }
    }
}