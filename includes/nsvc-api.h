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

//! @file     nsvc-api.h
//! @authors  Bernie Woodland
//! @date     1Oct17
//!
//! @brief   NUFR SL (Service Layer) APIs
//!

#ifndef NSVC_API_H
#define NSVC_API_H

#include "nsvc-app.h"
#include "nufr-global.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-kernel-semaphore.h"

typedef enum
{
    // START SECTION OVERLAY OF 'nufr_msg_send_rtn_t'
    NSVC_MSRT_OK = 1,
    NSVC_MSRT_ERROR,
    NSVC_MSRT_ABORTED,
    NSVC_MSRT_AWOKE_RECEIVER,
    // END SECTION OVERLAY OF 'nufr_msg_send_rtn_t'

    // Non-'nufr_msg_send_rtn_t' value(s)
    NSVC_MSRT_DEST_NOT_FOUND,
} nsvc_msg_send_return_t;


//!
//! @struct    nsvc_msg_lookup_t
//!
//! @brief     output of prefix+id lookup to task(s)
//!
//! @details   'single_tid'--destination task for message.
//! @details        Set to 'NUFR_TID_null' if dest is multiple tasks.
//! @details   'tid_list_ptr'--if multiple destination tasks, list
//! @details        of tasks.
//! @details   'tid_list_length'--length of 'tid_list_ptr'
//!
typedef struct
{
    nufr_tid_t        single_tid;
    const nufr_tid_t *tid_list_ptr;
    unsigned          tid_list_length;
} nsvc_msg_lookup_t;

//!
//! @struct    nsvc_msg_fields_unary_t
//!
//! @brief     Basic message parameters in expanded form
//!
typedef struct
{
    nsvc_msg_prefix_t prefix;
    uint16_t          id;           // probably cast to per-task defined enum
    nufr_msg_pri_t    priority;
    nufr_tid_t        sending_task;        // used on send, but not get
    nufr_tid_t        destination_task;    // used on get, but not send
    uint32_t          optional_parameter;  // can be cast to uint8_t* buffer, etc
} nsvc_msg_fields_unary_t;

//!
//! @name      nsvc_msg_struct_to_fields_inline
//!
INLINE uint32_t
nsvc_msg_struct_to_fields_inline(const nsvc_msg_fields_unary_t *parms)
{
    uint32_t fields;

    fields = NUFR_SET_MSG_FIELDS(parms->prefix,
                                 parms->id,
                                 parms->sending_task,
                                 parms->priority);

    return fields;
}

//!
//! @name      nsvc_msg_args_to_fields_inline
//!
INLINE uint32_t
nsvc_msg_args_to_fields_inline(nsvc_msg_prefix_t prefix,
                               uint16_t          id,
                               nufr_msg_pri_t    priority,
                               nufr_tid_t        sending_task)
{
    uint32_t fields;

    fields = NUFR_SET_MSG_FIELDS(prefix,
                                 id,
                                 sending_task,
                                 priority);

    return fields;
}

//!
//! @name      nsvc_msg_fields_to_struct_inline
//!
INLINE void nsvc_msg_fields_to_struct_inline(uint32_t                 fields,
                                             nsvc_msg_fields_unary_t *parms)
{
    parms->prefix = (nsvc_msg_prefix_t)NUFR_GET_MSG_PREFIX(fields);
    parms->id = NUFR_GET_MSG_ID(fields);
    parms->priority = (nufr_msg_pri_t)NUFR_GET_MSG_PRIORITY(fields);
    parms->sending_task = (nufr_tid_t)NUFR_GET_MSG_SENDING_TASK(fields);
}

//!
//! @name      nsvc_msg_fields_to_args_inline
//!
INLINE void nsvc_msg_fields_to_args_inline(uint32_t           fields,
                                           nsvc_msg_prefix_t *prefix_ptr,
                                           uint16_t          *id_ptr,
                                           nufr_msg_pri_t    *priority_ptr,
                                           nufr_tid_t        *sending_task_ptr)
{
    *prefix_ptr = (nsvc_msg_prefix_t)NUFR_GET_MSG_PREFIX(fields);
    *id_ptr = NUFR_GET_MSG_ID(fields);
    *priority_ptr = (nufr_msg_pri_t)NUFR_GET_MSG_PRIORITY(fields);
    *sending_task_ptr = (nufr_tid_t)NUFR_GET_MSG_SENDING_TASK(fields);
}

//!
//! @enum      nsvc_timer_mode_t
//!
//! @details   Mode timer runs in
//! @details   'NSVC_TMODE_SIMPLE'-- timer stops after timeout
//! @details   'NSVC_TMODE_CONTINUOUS'-- timer auto-restarts after timeout
//! @details   
//!
typedef enum
{
    NSVC_TMODE_null = 0,    //must be zero
    NSVC_TMODE_SIMPLE,
    NSVC_TMODE_CONTINUOUS,
} nsvc_timer_mode_t;

//!
//! @enum      nsvc_timer_callin_return_t
//!
//! @details   Return value to 'nsvc_timer_expire_timer_callin'
//! @details   Indicates action the quantum timer (if used) should take
//! @details    'NSVC_TCRTN_DISABLE_QUANTUM_TIMER'-- halt quantum timer
//! @details    'NSVC_TCRTN_RECONFIGURE_QUANTUM_TIMER'-- set quantum
//! @details        timeout to new value.
//! @details    'NSVC_TCRTN_BACKOFF_QUANTUM_TIMER'-- SL timer module busy
//! @details        at task level. Call back in again soon to complete action.
//!
typedef enum
{
    NSVC_TCRTN_DISABLE_QUANTUM_TIMER,
    NSVC_TCRTN_RECONFIGURE_QUANTUM_TIMER,
    NSVC_TCRTN_BACKOFF_QUANTUM_TIMER,
} nsvc_timer_callin_return_t;

//!
//! @enum      nsvc_timer_t
//!
//! @brief     SL timer block
//!
//! @details   Timer control block fields
//! @details   
//!
typedef struct nsvc_timer_t_
{
    struct nsvc_timer_t_ *flink;
    struct nsvc_timer_t_ *blink;
    uint32_t              duration;
    uint32_t              expiration_time;
    uint32_t              msg_fields;
    uint32_t              msg_parameter;
    uint8_t               dest_task_id;   //type 'nufr_tid_t'
    uint8_t               mode;           //type 'nsvc_timer_mode_t'
    bool                  is_active;
} nsvc_timer_t;

//!
//! @name      nsvc_timer_get_current_time_fcn_ptr_t
//!
//! @details   Retrieves the 32-bit time from H/W reference source
//!
//! @return   'uint32_t'-- current time
//!
typedef uint32_t (*nsvc_timer_get_current_time_fcn_ptr_t)(void);

//!
//! @name      nsvc_timer_quantum_device_reconfigure_fcn_ptr_t
//!
//! @param[in] 'uint32_t'-- parm specifying delay to next quantum timer callin.
//! @param[in]    A zero value means halt quantum timer (no active app timers).
//!
typedef void (*nsvc_timer_quantum_device_reconfigure_fcn_ptr_t)(uint32_t);

//!
//! @name      NSVC_TIMER_SET_ID
//! @name      NSVC_TIMER_SET_PREFIX_ID
//! @name      NSVC_TIMER_SET_PREFIX_ID_PRIORITY
//!
//! @brief     Macros for packing nsvc_timer_start() 'msg_fields' parm
//!
#define NSVC_TIMER_SET_ID(id)                                                \
    NUFR_SET_MSG_FIELDS(NSVC_MSG_PREFIX_local, (id), NUFR_TID_null, NUFR_MSG_PRI_MID)

#define NSVC_TIMER_SET_PREFIX_ID(prefix, id)                                 \
    NUFR_SET_MSG_FIELDS((prefix), (id), NUFR_TID_null, NUFR_MSG_PRI_MID)

#define NSVC_TIMER_SET_PREFIX_ID_PRIORITY(prefix, id, priority)              \
    NUFR_SET_MSG_FIELDS((prefix), (id), NUFR_TID_null, (priority))

//!
//! @name      FLINK_PTR
//!
//! @brief     macro which gets a pointer to the 'flink' variable
//! @brief     embedded in a pool element
//!
#define NSVC_POOL_FLINK_PTR(Xpool_ptr, Xelement_ptr)    (void **)              \
                       ((uint8_t *)(Xelement_ptr) + (Xpool_ptr)->flink_offset)

//!
//! @struct    nsvc_pool_t
//!
//! @brief     SL pool manager instance
//!
//! @details   'pool_size'-- Number of elements dedicated to pool. 
//! @details   'free_count'-- Elements which can be allocated at any given
//! @details              time. <= 'pool_size'
//! @details   'element_size'-- sizeof of a single element
//! @details   'element_index_size'-- offset of elements as they lie in array
//! @details              == (unsigned)(&element[1] - &element[0])
//! @details   'flink_offset'-- offset that flink ptr is from base of element
//! @details              == offsetof(element, flink)
//! @details   'base_ptr'-- address of first element
//! @details              == &element[0]
//! @details   'head_ptr'-- free list head
//! @details   'tail_ptr'-- free list tail
//! @details   'sema', 'sema_block'-- semaphore dedicated to pool
//! @details             Count of sema is equal to free count
//!
typedef struct
{
    nufr_sema_block_t *sema_block;
    void              *base_ptr;
    void              *head_ptr;
    void              *tail_ptr;
    // Data types specified as 'uint16_t' rather than 'unsigned' to save RAM
    uint16_t           pool_size;
    uint16_t           free_count;
    uint16_t           element_size;
    uint16_t           element_index_size;
    uint16_t           flink_offset;
    nufr_sema_t        sema;
} nsvc_pool_t;

//!
//! @name      NSVC_PCL_SIZE_AT_HEAD
//!
//! @brief     Number of bytes which can be stored in a single particle,
//! @brief     if this particle is the head of a chain.
//!
#define NSVC_PCL_SIZE_AT_HEAD    (NSVC_PCL_SIZE - sizeof(nsvc_pcl_header_t))

//!
//! @name      NSVC_PCL_NO_TIMEOUT
//!
//! @brief     Magic number: don't select timeout mode
//!
#define NSVC_PCL_NO_TIMEOUT              (-1)

//!
//! @name      NSVC_PCL_HEADER
//!
//! @brief     Create ptr to'nsvc_pcl_header_t', given chain head particle
//!
#define NSVC_PCL_HEADER(head_pcl)  ( (nsvc_pcl_header_t *)(head_pcl)->buffer )

//!
//! @name      NSVC_PCL_OFFSET_PAST_HEADER
//!
//! @brief     'offset'==0 is first byte after header (nsvc_pcl_header_t)
//!
#define NSVC_PCL_OFFSET_PAST_HEADER(offset)    ( (offset) + sizeof(nsvc_pcl_header_t) )

//!
//! @name      NSVC_PCL_SEEK_DATA_PTR
//!
//! @brief     Convert seek struct to 'uint8_t *' data ptr
//!
#define NSVC_PCL_SEEK_DATA_PTR(seek_ptr)  ( &((seek_ptr)->current_pcl->buffer[(seek_ptr)->offset_in_pcl]) )
#define NSVC_PCL_SEEK_DATA(seek_struct)  ( &(seek_struct.current_pcl->buffer[seek_struct.offset_in_pcl]) )

//!
//! @struct    nsvc_pcl_t
//!
//! @brief     Single particle (pcl)
//!
typedef struct nsvc_pcl_t_
{
    struct nsvc_pcl_t_  *flink;
    uint8_t              buffer[NSVC_PCL_SIZE];
} nsvc_pcl_t;

//!
//! @struct    nsvc_pcl_header_t
//!
//! @brief     Particle header. Embedded in first pcl of a chain.
//!
//! @details   Must be word (4-byte) aligned
//!
typedef struct
{
    nsvc_pcl_t          *tail;
    uint16_t             offset;
    uint16_t             total_used_length;
    uint8_t              num_pcls;      // includes head
    uint8_t              intfc;         // cast to 'rnet_intfc_t' 
    uint8_t              subi;          // cast to 'rnet_subi_t'
    uint8_t              circuit;       // rnet circuit index
    uint8_t              previous_ph;   // cast to 'rnet_ph_t'
    uint8_t              spare1;
    uint8_t              spare2;
    uint8_t              spare3;
    uint32_t             code;          // message-specific code
} nsvc_pcl_header_t;


//!
//! @struct    nsvc_pcl_chain_seek_t
//!
//! @brief     Indexing/keeps track of where you are in a chain.
//!
//! @details   Must be word (4-byte) aligned
//!
typedef struct
{
    nsvc_pcl_t  *current_pcl;
    uint16_t     offset_in_pcl;
} nsvc_pcl_chain_seek_t;


//! @brief     APIs
RAGING_EXTERN_C_START

//! @brief     APIs from nsvc-app.c
bool nsvc_msg_prefix_id_lookup(nsvc_msg_prefix_t  prefix,
                               nsvc_msg_lookup_t *out_ptr);

//! mutexes
void nsvc_mutex_init(void);
nufr_sema_get_rtn_t nsvc_mutex_getW(nsvc_mutex_t   mutex,
                                    nufr_msg_pri_t abort_priority_of_rx_msg);
nufr_sema_get_rtn_t nsvc_mutex_getT(nsvc_mutex_t   mutex,
                                    nufr_msg_pri_t abort_priority_of_rx_msg,
                                    unsigned       timeout_ticks);
bool nsvc_mutex_release(nsvc_mutex_t mutex);

//! generic pool
void nsvc_pool_init(nsvc_pool_t *pool_ptr);
bool nsvc_pool_is_element(nsvc_pool_t *pool_ptr, void *element_ptr);
void nsvc_pool_free(nsvc_pool_t *pool_ptr, void *element_ptr);
void *nsvc_pool_allocate(nsvc_pool_t *pool_ptr, bool called_from_ISR);
nufr_sema_get_rtn_t nsvc_pool_allocateW(nsvc_pool_t  *pool_ptr,
                                        void        **element_ptr);
nufr_sema_get_rtn_t nsvc_pool_allocateT(nsvc_pool_t  *pool_ptr,
                                        void        **element_ptr,
                                        unsigned      timeout_ticks);

//! messaging
uint32_t nsvc_msg_struct_to_fields(const nsvc_msg_fields_unary_t *parms);
uint32_t nsvc_msg_args_to_fields(nsvc_msg_prefix_t prefix,
                                 uint16_t          id,
                                 nufr_msg_pri_t    priority,
                                 nufr_tid_t        sending_task);
void nsvc_msg_fields_to_struct(uint32_t fields, nsvc_msg_fields_unary_t *parms);
void nsvc_msg_fields_to_args(uint32_t           fields,
                             nsvc_msg_prefix_t *prefix_ptr,
                             uint16_t          *id_ptr,
                             nufr_msg_pri_t    *priority_ptr,
                             nufr_tid_t        *sending_task_ptr);
nsvc_msg_send_return_t
nsvc_msg_send_structW(const nsvc_msg_fields_unary_t *parms);
nsvc_msg_send_return_t
nsvc_msg_send_argsW(nsvc_msg_prefix_t              prefix,
                    uint16_t                       id,
                    nufr_msg_pri_t                 priority,
                    nufr_tid_t                     destination_task,
                    uint32_t                       optional_parameter);
nsvc_msg_send_return_t
nsvc_msg_send_multi(uint32_t           fields,
                    uint32_t           optional_parameter,
                    nsvc_msg_lookup_t *destination_list);
nufr_bop_wait_rtn_t nsvc_msg_send_and_bop_waitW(
                            nsvc_msg_prefix_t  prefix,
                            uint16_t           id,
                            nufr_msg_pri_t     priority,
                            nufr_tid_t         destination_task,
                            uint32_t           optional_parameter,
                            nufr_msg_pri_t     abort_priority_of_rx_msg);
nufr_bop_wait_rtn_t nsvc_msg_send_and_bop_waitT(
                            nsvc_msg_prefix_t  prefix,
                            uint16_t           id,
                            nufr_msg_pri_t     priority,
                            nufr_tid_t         destination_task,
                            uint32_t           optional_parameter,
                            nufr_msg_pri_t     abort_priority_of_rx_msg,
                            unsigned           timeout_ticks);
void nsvc_msg_get_structW(nsvc_msg_fields_unary_t *msg_fields_ptr);
bool nsvc_msg_get_structT(nsvc_msg_fields_unary_t *msg_fields_ptr,
                          unsigned                 timeout_ticks);
void nsvc_msg_get_argsW(nsvc_msg_prefix_t  *prefix_ptr,
                        uint16_t           *id_ptr,
                        nufr_msg_pri_t     *priority_ptr,
                        nufr_tid_t         *source_task_ptr,
                        uint32_t           *optional_parameter_ptr);
bool nsvc_msg_get_argsT(nsvc_msg_prefix_t  *prefix_ptr,
                        uint16_t           *id_ptr,
                        nufr_msg_pri_t     *priority_ptr,
                        nufr_tid_t         *source_task_ptr,
                        uint32_t           *optional_parameter_ptr,
                        unsigned            timeout_ticks);

//! particles
void nsvc_pcl_init(void);
bool nsvc_pcl_is(void *ptr);
void nsvc_pcl_free_chain(nsvc_pcl_t *head_pcl);
nufr_sema_get_rtn_t nsvc_pcl_alloc_chainWT(nsvc_pcl_t       **head_pcl_ptr,
                                           nsvc_pcl_header_t *header,
                                           unsigned           capacity,
                                           int                timeout_ticks);
nufr_sema_get_rtn_t nsvc_pcl_lengthen_chainWT(nsvc_pcl_t   *head_pcl,
                                              unsigned      bytes_to_lengthen,
                                              int           timeout_ticks);
unsigned nsvc_pcl_chain_capacity(unsigned pcls_in_chain, bool include_head);
unsigned nsvc_pcl_pcls_for_capacity(unsigned capacity, bool include_head);
unsigned nsvc_pcl_count_pcls_in_chain(nsvc_pcl_t *head_pcl);
unsigned nsvc_pcl_write_data_no_continue(nsvc_pcl_t *pcl,
                                         unsigned    pcl_offset,
                                         uint8_t    *data,
                                         unsigned    data_length);
unsigned nsvc_pcl_write_data_continue(nsvc_pcl_chain_seek_t *seek_ptr,
                                      uint8_t               *data,
                                      unsigned               data_length);
nufr_sema_get_rtn_t nsvc_pcl_write_dataWT(nsvc_pcl_t           **head_pcl_ptr,
                                          nsvc_pcl_chain_seek_t *seek_ptr,
                                          uint8_t               *data,
                                          unsigned               data_length,
                                          int                    timeout_ticks);
unsigned nsvc_pcl_contiguous_count(nsvc_pcl_chain_seek_t *seek_ptr);
nsvc_pcl_t *nsvc_pcl_get_previous_pcl(nsvc_pcl_t *head_pcl,
                                      nsvc_pcl_t *current_pcl);
bool nsvc_pcl_seek_ffwd(nsvc_pcl_chain_seek_t *seek_ptr,
                       unsigned                ffwd_amount);
bool nsvc_pcl_seek_rewind(nsvc_pcl_t            *head_pcl,
                          nsvc_pcl_chain_seek_t *seek_ptr,
                          unsigned               rewind_amount);
bool nsvc_pcl_set_seek_to_packet_offset(nsvc_pcl_t            *head_pcl,
                                          nsvc_pcl_chain_seek_t *seek_ptr,
                                          unsigned               chain_offset);
bool nsvc_pcl_set_seek_to_headerless_offset(nsvc_pcl_t            *head_pcl,
                                            nsvc_pcl_chain_seek_t *seek_ptr,
                                            unsigned               chain_offset);
unsigned nsvc_pcl_read(nsvc_pcl_chain_seek_t *seek_ptr,
                       uint8_t               *data,
                       unsigned               read_length);


//! timers
void nsvc_timer_init(nsvc_timer_get_current_time_fcn_ptr_t fptr_current_time,
           nsvc_timer_quantum_device_reconfigure_fcn_ptr_t fptr_reconfigure);
nsvc_timer_t *nsvc_timer_alloc(void);
void nsvc_timer_free(nsvc_timer_t *tm);
void nsvc_timer_start(nsvc_timer_t     *tm);
bool nsvc_timer_kill(nsvc_timer_t     *tm);
uint32_t nsvc_timer_next_expiration_callin(void);
// Can't use 'nsvc_timer_callin_return_t' as return value--will
// cause circular include problem!
//nsvc_timer_callin_return_t nsvc_timer_expire_timer_callin(
uint8_t nsvc_timer_expire_timer_callin(
                                    uint32_t      current_time,
                                    uint32_t     *reconfigured_time_ptr);

RAGING_EXTERN_C_END

#endif  //NSVC_API_H