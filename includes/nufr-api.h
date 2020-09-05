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

//! @file     nufr-api.h
//! @authors  Bernie Woodland
//! @date     7Aug17
//!
//! @brief    API calls exported to application layer
//!
//! @details 

#ifndef NUFR_API_H
#define NUFR_API_H

#include "nufr-platform-app.h"
#include "nufr-platform.h"

//!
//! @brief   OS Tick conversion macros
//!
#define NUFR_MILLISECS_TO_TICKS(milliseconds)                              \
                                        ( (milliseconds) / NUFR_TICK_PERIOD )
#define NUFR_SECS_TO_TICKS(seconds)                                        \
                          ( (seconds) * MILLISECS_PER_SEC / NUFR_TICK_PERIOD )

// State of task, whether blocked or ready, what blocking reason.
typedef enum
{
    NUFR_BKD_NOT_LAUNCHED = 1, // task hasn't been launched yet/invalid tid
    NUFR_BKD_READY,            // task not blocked
    NUFR_BKD_ASLEEP,           // blocked while sleeping
    NUFR_BKD_BOP,              // blocked on bop with no timeout
    NUFR_BKD_BOP_TOUT,         // blocked on bop with timeout
    NUFR_BKD_MSG,              // blocked on msg receive with no timeout
    NUFR_BKD_MSG_TOUT,         // blocked on msg receive with timeout
    NUFR_BKD_SEMA,             // blocked on sema with no timeout
    NUFR_BKD_SEMA_TOUT         // blocked on sema with timeout
} nufr_bkd_t;

typedef enum
{
    NUFR_BOP_WAIT_OK = 1,
    NUFR_BOP_WAIT_TIMEOUT,
    NUFR_BOP_WAIT_ABORTED_BY_MESSAGE,
    NUFR_BOP_WAIT_INVALID
} nufr_bop_wait_rtn_t;

typedef enum
{
    NUFR_BOP_RTN_TAKEN = 1,         // receiving task unblocked by bop
    NUFR_BOP_RTN_TASK_NOT_WAITING,  // bop dropped because receiving task
                                    // not blocked on bop, or invalid task id
    NUFR_BOP_RTN_KEY_MISMATCH,      // bop dropped due to key mismatch
    NUFR_BOP_RTN_INVALID
} nufr_bop_rtn_t;

// Message priorities. 
typedef enum
{
    #if NUFR_CS_MSG_PRIORITIES == 1
        NUFR_MSG_PRI_MID = 0,
    #elif (NUFR_CS_MSG_PRIORITIES == 2) && (NUFR_CS_TASK_KILL == 1)
        NUFR_MSG_PRI_CONTROL = 0,    // only used for task abort, kills, etc
        NUFR_MSG_PRI_MID = 1,
    #elif (NUFR_CS_MSG_PRIORITIES == 2)
        NUFR_MSG_PRI_HIGH = 0,
        NUFR_MSG_PRI_MID = 1,
    #elif (NUFR_CS_MSG_PRIORITIES == 3) && (NUFR_CS_TASK_KILL == 1)
        NUFR_MSG_PRI_CONTROL = 0,    // only used for task abort, kills, etc
        NUFR_MSG_PRI_HIGH = 1,
        NUFR_MSG_PRI_MID = 2,
    #elif NUFR_CS_MSG_PRIORITIES == 3
        NUFR_MSG_PRI_HIGH = 0,
        NUFR_MSG_PRI_MID = 1,
        NUFR_MSG_PRI_LOW = 2,
    #elif (NUFR_CS_MSG_PRIORITIES == 4) && (NUFR_CS_TASK_KILL == 1)
        NUFR_MSG_PRI_CONTROL = 0,    // only used for task abort, kills, etc
        NUFR_MSG_PRI_HIGH = 1,
        NUFR_MSG_PRI_MID = 2,
        NUFR_MSG_PRI_LOW = 3
    #elif NUFR_CS_MSG_PRIORITIES == 4
        NUFR_MSG_PRI_HIGHEST = 0,
        NUFR_MSG_PRI_HIGH = 1,
        NUFR_MSG_PRI_MID = 2,
        NUFR_MSG_PRI_LOW = 3
    #else
        #error "Invalid NUFR_CS_MSG_PRIORITIES value"
    #endif
} nufr_msg_pri_t;

// Pass this to API for param 'abort_priority_of_rx_msg' if no
// abort override is desired.
#define NUFR_NO_ABORT      ( (nufr_msg_pri_t)0 )


//!
//! @brief     Macros for setting or extracting nufr_msg_t->fields member
//!
//! @details          nufr_msg_t->fields bit packing
//! @details          ==============================
//! @details     bit no's                   allocation
//! @details     --------                   ----------
//! @details     0- 2                    message priority
//! @details                               //
//! @details                               0 = lowest, 7 = highest
//! @details     3-10                    sending task (nufr_tid_t value)
//! @details       11                    (unused)
//! @details    12-31                    message ID field(s). Defined at SL.
//! @details                                in SL: 12-21   msg ID
//! @details                                       22-31   msg prefix
//! @details   
//! @details   
//!
//   NUFR_MSG_MAX_PRIORITY, NUFR_MSG_MAX_PREFIX,NUFR_MSG_MAX_ID must be
//      multiple of 2 minus 1 or other macros will break 
#define NUFR_MSG_MAX_PRIORITY                0x7     // 7 (3 bits, 0-based)
#define NUFR_MSG_MAX_PREFIX                0x3FF     // 1023 (10 bits, 0-based)
#define NUFR_MSG_MAX_ID                    0x3FF     // 1023 (10 bits, 0-based
#define NUFR_MSG_MAX_TASK_ID                0xFF     // 255 (8 bits, 0-based)
#define NUFR_GET_MSG_PRIORITY(fields)                             \
                               ( (fields) & NUFR_MSG_MAX_PRIORITY)
#define NUFR_SET_MSG_PRIORITY(fields, value)                      \
                               ( ((value) & NUFR_MSG_MAX_PRIORITY) | (fields) )
#define NUFR_GET_MSG_SENDING_TASK(fields)                         \
                               ( ((fields) >> 3) & NUFR_MSG_MAX_TASK_ID)
#define NUFR_SET_MSG_SENDING_TASK(fields, value)                  \
               ( (((uint32_t)(value) & NUFR_MSG_MAX_TASK_ID) << 3) | (fields) )
#define NUFR_SET_MSG_FIELDS(prefix, id, sending_task, priority)                \
      ( ((uint32_t)((prefix) & NUFR_MSG_MAX_PREFIX) << 22)       |             \
        ((uint32_t)((id) & NUFR_MSG_MAX_ID) << 12)               |             \
        ((uint32_t)((sending_task) & NUFR_MSG_MAX_TASK_ID) << 3) |             \
        ((priority) & NUFR_MSG_MAX_PRIORITY) )
//!
//! @brief     Message nufr_msg_t->fields bit packing
//!
//! @name      NUFR_GET_MSG_ID
//! @name      NUFR_SET_MSG_ID
//! @name      NUFR_GET_MSG_PREFIX
//! @name      NUFR_SET_MSG_PREFIX
//! @name      NUFR_GET_MSG_PREFIX_ID_PAIR
//! @name      NUFR_SET_MSG_PREFIX_ID_PAIR
//! @name      NUFR_SET_MSG_FIELDS
//!
//              'fields' bit assignments
//              ------------------------
//      31-22     21-12       10-3                  2-0
//     PREFIX      ID       SENDING TASK         PRIORITY
//
// fixme: collapse these??
#define NUFR_GET_MSG_ID(fields)                                   \
                               ( ((fields) >> 12) & NUFR_MSG_MAX_ID)
#define NUFR_SET_MSG_ID(fields, value)                            \
                ( (((uint32_t)(value) & NUFR_MSG_MAX_ID) << 12) | (fields) )
#define NUFR_GET_MSG_PREFIX(fields)                               \
                              ( ((fields) >> 22) )
#define NUFR_SET_MSG_PREFIX(fields, value)                        \
               ( (((uint32_t)(value) & NUFR_MSG_MAX_PREFIX) << 22) | (fields) )
      // next 2 normalize prefix|id to bit zero
#define NUFR_GET_MSG_PREFIX_ID_PAIR(fields)                       \
      ( ((uint32_t)(fields) >> 12) )
#define NUFR_SET_MSG_PREFIX_ID_PAIR(prefix, id)                  \
      ( ((uint32_t)(prefix) << 12) | ((uint32_t)(id))

typedef enum
{
    NUFR_MSG_SEND_OK = 1,
    NUFR_MSG_SEND_ERROR,
    NUFR_MSG_SEND_ABORTED_RECEIVER,
    NUFR_MSG_SEND_AWOKE_RECEIVER
} nufr_msg_send_rtn_t;

typedef enum
{
    NUFR_SEMA_GET_OK_NO_BLOCK = 1,  //got sema without having to block
    NUFR_SEMA_GET_OK_BLOCK,         //had to block waiting to get sema
    NUFR_SEMA_GET_MSG_ABORT, //message send causes blocking task to abort wait
    NUFR_SEMA_GET_TIMEOUT,
} nufr_sema_get_rtn_t;

//!
//! @brief   Task API's
//!
RAGING_EXTERN_C_START

void nufr_launch_task(nufr_tid_t task_id, unsigned parameter);
void nufr_exit_running_task(void);
void nufr_kill_task(nufr_tid_t task_id);
nufr_tid_t nufr_self_tid(void);
nufr_bkd_t nufr_task_running_state(nufr_tid_t task_id);
bool nufr_sleep(unsigned sleep_delay_in_ticks,
                nufr_msg_pri_t abort_priority_of_rx_msg);
bool nufr_yield(void);
void nufr_prioritize(void);
void nufr_unprioritize(void);
void nufr_change_task_priority(nufr_tid_t tid, unsigned new_priority);
uint16_t nufr_bop_get_key(void);
nufr_bop_wait_rtn_t nufr_bop_waitW(nufr_msg_pri_t abort_priority_of_rx_msg);
nufr_bop_wait_rtn_t nufr_bop_waitT(nufr_msg_pri_t abort_priority_of_rx_msg,
                                   unsigned timeoutTicks);
nufr_bop_rtn_t nufr_bop_send(nufr_tid_t task_id, uint16_t key);
nufr_bop_rtn_t nufr_bop_send_with_key_override(nufr_tid_t task_id);
#if NUFR_CS_LOCAL_STRUCT == 1
nufr_bop_rtn_t nufr_bop_lock_waiter(nufr_tid_t task_id, uint16_t key);
void nufr_bop_unlock_waiter(nufr_tid_t task_id);
#endif  //NUFR_CS_LOCAL_STRUCT

//!
//! @brief   OS Tick/timer API's
//!
uint32_t nufr_tick_count_get(void);
uint32_t nufr_tick_count_delta(uint32_t reference_count);

//!
//! @brief   Local Struct API's
//!
#if NUFR_CS_LOCAL_STRUCT == 1
void nufr_local_struct_set(void *local_struct_ptr);
void *nufr_local_struct_get(nufr_tid_t task_id);
#endif  //NUFR_CS_LOCAL_STRUCT

//!
//! @brief   Messaging API's
//!
#if NUFR_CS_MESSAGING == 1
void nufr_msg_drain(nufr_tid_t task_id, nufr_msg_pri_t from_this_priority);
unsigned nufr_msg_purge(uint32_t msg_fields, bool do_all);
nufr_msg_send_rtn_t nufr_msg_send_by_block(nufr_msg_t *msg, nufr_tid_t dest_task_id);
nufr_msg_send_rtn_t nufr_msg_send(uint32_t   msg_fields,
                                  uint32_t   optional_parameter,
                                  nufr_tid_t dest_task_id);
void nufr_msg_getW(uint32_t *msg_fields_ptr, uint32_t *parameter_ptr);
bool nufr_msg_getT(unsigned timeout_ticks, uint32_t *msg_fields_ptr, uint32_t *parameter_ptr);
nufr_msg_t *nufr_msg_peek(void);
#endif  //NUFR_CS_MESSAGING

//!
//! @brief   Semaphore API's
//!
#if NUFR_CS_SEMAPHORE == 1
unsigned nufr_sema_count_get(nufr_sema_t sema);
nufr_sema_get_rtn_t nufr_sema_getW(nufr_sema_t    sema,
                                   nufr_msg_pri_t abort_priority_of_rx_msg);
nufr_sema_get_rtn_t nufr_sema_getT(nufr_sema_t    sema,
                                   nufr_msg_pri_t abort_priority_of_rx_msg,
                                   unsigned       timeout_ticks);
bool nufr_sema_release(nufr_sema_t sema);
#endif  //NUFR_CS_SEMAPHORE

RAGING_EXTERN_C_END

#endif  //NUFR_API_H
