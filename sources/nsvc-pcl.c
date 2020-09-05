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

//! @file    nsvc-pcl.c
//! @authors Bernie Woodland
//! @date    1Oct17
//!
//! @brief   NUFR SL Particle Support
//!
//                SINGLE PARTICLE PACKET
//
//                  ----------------------------------
//                  |           flink                |
//                  ----------------------------------
//                  |    nsvc_pcl_header_t           |
//                  ----------------------------------
//                  |                                |
//        offset:   ----------------------------------
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
// offset + length: ----------------------------------
//                  |                                |
//                  |                                |
//                  |                                |
//                  |                                |
//                  ----------------------------------

//                MULTI-PARTICLE PACKET
//
//                  ----------------------------------
//                  |           flink                |
//                  ----------------------------------
//                  |      nsvc_pcl_header_t         |
//                  ----------------------------------
//                  |                                |
//        offset:   ----------------------------------
//  (absorb sizeof) |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  ----------------------------------
//
//                  ----------------------------------
//                  |           flink                |
//                  ----------------------------------
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  ----------------------------------
//
//                  ----------------------------------
//                  |           flink                |
//                  ----------------------------------
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
//                  |  x x x x x x x x x x x x x x x |
// extended length: |--------------------------------|
//                  |                                |
//                  |                                |
//                  |                                |
//                  |                                |
//                  |                                |
//                  |                                |
//                  |                                |
//                  |                                |
//                  |                                |
//                  |                                |
//                  |                                |
//                  ----------------------------------

#include "nsvc-app.h"
#include "nsvc-api.h"

#include "nufr-platform.h"

#include "raging-contract.h"
#include "raging-utils-mem.h"

// 'true' if allocation was successful
#define SUCCESS_ALLOC(rv)     ( (NUFR_SEMA_GET_OK_NO_BLOCK == (rv)) ||       \
                                (NUFR_SEMA_GET_OK_BLOCK == (rv)) )


// 'true' if 'x' is a legitimate particle
#define NSVC_IS_PCL(x)     ( ((nsvc_pcl_t *)(x) >= nsvc_pcls) &&                  \
                         ((nsvc_pcl_t *)(x) <= &nsvc_pcls[NSVC_PCL_NUM_PCLS - 1]) )


// All particles defined here
static nsvc_pcl_t  nsvc_pcls[NSVC_PCL_NUM_PCLS];

// Particle pool
extern nsvc_pool_t nsvc_pcl_pool;

//!
//! @name      nsvc_pcl_init
//!
//! @brief     Initialize particle service
//!
void nsvc_pcl_init(void)
{
    // 'nsvc_pcl_header_t' must be aligned on a word boundary
    SL_INVARIANT(sizeof(nsvc_pcl_header_t) == ALIGN32(sizeof(nsvc_pcl_header_t)));
    // Buffer size must be evenly sized
    SL_INVARIANT(NSVC_PCL_SIZE == ALIGN32(NSVC_PCL_SIZE));
    // Header must fit in a particle
    SL_INVARIANT(sizeof(nsvc_pcl_header_t) < NSVC_PCL_SIZE);

    // Initialize particle pool
    rutils_memset(&nsvc_pcl_pool, 0, sizeof(nsvc_pcl_pool));
    nsvc_pcl_pool.base_ptr = nsvc_pcls;
    nsvc_pcl_pool.pool_size = NSVC_PCL_NUM_PCLS;
    nsvc_pcl_pool.element_size = sizeof(nsvc_pcl_t);
    nsvc_pcl_pool.element_index_size = (unsigned)((uint8_t *)&nsvc_pcls[1] -
                                                  (uint8_t *)&nsvc_pcls[0]);
    nsvc_pcl_pool.flink_offset = OFFSETOF(nsvc_pcl_t, flink);
    nsvc_pool_init(&nsvc_pcl_pool);
}

//!
//! @name      nsvc_pcl_is
//!
//! @brief     Returns 'true' if 'ptr' is a particle
//!
bool nsvc_pcl_is(void *ptr)
{
    return NSVC_IS_PCL(ptr);
}

// 
//!
//! @name      nsvc_pcl_free_chain
//!
//! @brief     Free a particle chain.
//!
//! @details   A chain is a linked list of 1 to N particles
//!
void nsvc_pcl_free_chain(nsvc_pcl_t *head_pcl)
{
    nsvc_pcl_t *current_pcl = head_pcl;
    nsvc_pcl_t *next_pcl;

    SL_REQUIRE_API(NSVC_IS_PCL(current_pcl));

    // Walk all pcls in chain, freeing them individually
    while (NULL != current_pcl)
    {
        SL_ENSURE(NSVC_IS_PCL(current_pcl));

        next_pcl = current_pcl->flink;

        nsvc_pool_free(&nsvc_pcl_pool, current_pcl);

        current_pcl = next_pcl;
    }
}

//!
//! @name      nsvc_pcl_alloc_chainWT
//!
//! @brief     Create a particle chain
//!
//! @details   A chain is a linked list of 1 to N particles
//!
//! @param[out] 'head_pcl_ptr'--pointer to a pointer to the chain
//! @param[out]        created.
//! @param[in] 'header'-- if ==NULL, assume chain created will
//! @param[in]            have a head created, and therefore first
//! @param[in]            particle will have its header updated.
//! @param[in]            If !=NULL, assume chain created is
//! @param[in]            headless. Fill in header info in this ptr
//! @param[in]            instead.
//! @param[in] 'capacity'-- Minimum number of bytes to allocate
//! @param[in]              Number of pcls chosen to fulfill this request.
//! @param[in] 'timeout_ticks'-- If set to NSVC_PCL_NO_TIMEOUT, call is
//! @param[in]                   blocking ("W" mode).
//! @param[in]                   If set to a positive value, the call is
//! @param[in]                   in "T" mode, and value is the timeout.
//!
//! @return    One of:
//! @return       NUFR_SEMA_GET_OK_NO_BLOCK: Allocation successful
//! @return       NUFR_SEMA_GET_OK_BLOCK: Allocation successful, but had to
//! @return                block waiting for a pcl/pcl(s) to become free
//! @return       NUFR_SEMA_GET_MSG_ABORT: Allocation unsuccessful due to an
//! @return                abort message send. Only relevant when
//! @return                NUFR_CS_TASK_KILL is enabled.
//! @return       NUFR_SEMA_GET_TIMEOUT: Allocation unsuccessful due to a
//! @return                timeout waiting for a pcl/pcls(s) to become free
//!
nufr_sema_get_rtn_t nsvc_pcl_alloc_chainWT(nsvc_pcl_t       **head_pcl_ptr,
                                           nsvc_pcl_header_t *header_ptr,
                                           unsigned           capacity,
                                           int                timeout_ticks)
{
    nsvc_pcl_t         *head_pcl = NULL;
    nsvc_pcl_t         *tail_pcl = NULL;
    nsvc_pcl_t         *this_pcl = NULL;
    nsvc_pcl_header_t  *fill_in_header;
    unsigned            pcls_needed;
    unsigned            i;
    uint32_t            start_time;
    unsigned            elapsed_time;
    unsigned            unsigned_timeout;
    unsigned            timeout_this_call;
    nufr_sema_get_rtn_t alloc_rv = NUFR_SEMA_GET_OK_NO_BLOCK;
    bool                included_header;

    included_header = NULL != header_ptr;

    SL_REQUIRE_API(NULL != head_pcl_ptr);
    SL_REQUIRE_API(timeout_ticks >= 0? true :
                                    NSVC_PCL_NO_TIMEOUT == timeout_ticks);

    // Calculate how many pcls in this chain required to fulfill byte
    // size request.
    pcls_needed = nsvc_pcl_pcls_for_capacity(capacity, !included_header);

    SL_ENSURE(pcls_needed > 0);

    start_time = nufr_tick_count_get();

    // Allocate pcls one-by-one, append them to chain's linked list
    for (i = 0; i < pcls_needed; i++)
    {
        if (NSVC_PCL_NO_TIMEOUT == timeout_ticks)
        {
            // 'void **' cast to supress compiler warning (hate doing it!)
            alloc_rv = nsvc_pool_allocateW(&nsvc_pcl_pool, (void **)&this_pcl);
        }
        else
        {
            // If there are multiple pcls to alloc, then timeout
            // must accumulate delay in allocating all of them.
            // Adjust timeout passed to alloc call based on elapsed
            // time.
            elapsed_time = nufr_tick_count_delta(start_time);

            unsigned_timeout = (unsigned)timeout_ticks;

            SL_ENSURE(unsigned_timeout >= elapsed_time);

            timeout_this_call = unsigned_timeout - elapsed_time;     

            // 'void **' cast to supress compiler warning (hate doing it!)
            alloc_rv = nsvc_pool_allocateT(&nsvc_pcl_pool, (void **)&this_pcl,
                                           timeout_this_call);
        }

        // If abort message received, or timeout occured, unallocate
        //   everything (which can be done quickly), and return out.
        if (!SUCCESS_ALLOC(alloc_rv))
        {
            // 'this_pcl' may be non-null if alloc failed for
            // other reason, like msg abort
            if (NULL != this_pcl)
            {
                nsvc_pcl_free_chain(this_pcl);
            }

            // If we're aborting, clean up the entire chain
            if (NULL != head_pcl)
            {
                nsvc_pcl_free_chain(head_pcl);
            }

            *head_pcl_ptr = NULL;

            return alloc_rv;
        }

        // Abort check above ensures this pcl is not null
        SL_ENSURE(NULL != this_pcl);

        // First block allocated?
        if (NULL == head_pcl)
        {
            head_pcl = this_pcl;
            tail_pcl = this_pcl;
        }
        // Otherwise, add to end of chain
        else
        {
            tail_pcl->flink = this_pcl;
            tail_pcl = this_pcl;
        }
    }

    // Populate header
    if (included_header)
    {
        fill_in_header = header_ptr;
    }
    else
    {
        SL_ENSURE(NSVC_IS_PCL(head_pcl));

        fill_in_header = NSVC_PCL_HEADER(head_pcl);
    }
    fill_in_header->num_pcls = pcls_needed;
    fill_in_header->offset = 0;
    fill_in_header->total_used_length = 0;
    fill_in_header->tail = tail_pcl;

    *head_pcl_ptr = head_pcl;

    return alloc_rv;
}

//!
//! @name      nsvc_pcl_lengthen_chainWT
//!
//! @brief     Add 1 or more particles to an existing chain
//!
//! @details   Assume that the existing chain has at least 1 pcl
//! @details   already, so therefore has a head.
//!
//! @param[out] 'head_pcl'--pointer the chain that's being lengthened.
//! @param[in] 'bytes_to_lengthen'-- Minimum number of bytes to allocate
//! @param[in]              in extension.
//! @param[in] 'timeout_ticks'-- (Same as 'nsvc_pcl_alloc_chainWT()')
//!
//! @return    (Same as 'nsvc_pcl_alloc_chainWT()')
//!
nufr_sema_get_rtn_t nsvc_pcl_lengthen_chainWT(nsvc_pcl_t   *head_pcl,
                                              unsigned      bytes_to_lengthen,
                                              int           timeout_ticks)
{
    nsvc_pcl_t         *add_pcl;
    nsvc_pcl_t         *tail;
    nsvc_pcl_header_t  *head_header_ptr;
    nsvc_pcl_header_t   ext_header;
    nufr_sema_get_rtn_t alloc_rv;

    SL_REQUIRE_API(bytes_to_lengthen > 0);
    SL_REQUIRE_API(NSVC_IS_PCL(head_pcl));

    // SL_REQUIRE check must pass before allocating
    head_header_ptr = NSVC_PCL_HEADER(head_pcl);
    SL_REQUIRE(NSVC_IS_PCL(head_header_ptr->tail));

    alloc_rv = nsvc_pcl_alloc_chainWT(&add_pcl, &ext_header,
                                      bytes_to_lengthen, timeout_ticks);

    if (!SUCCESS_ALLOC(alloc_rv))
    {
        return alloc_rv;
    }

    tail = ext_header.tail;

    SL_ENSURE(NULL != tail);
    SL_ENSURE(NULL == tail->flink);

    head_header_ptr->tail->flink = add_pcl;
    head_header_ptr->num_pcls += nsvc_pcl_count_pcls_in_chain(add_pcl);
    head_header_ptr->tail = tail;

    return alloc_rv;
}

//!
//! @name      nsvc_pcl_chain_capacity
//!
//! @brief     Calculate maximum number of data bytes which can be stored in
//! @brief     a hypothetical chain.
//!
//! @param[in] 'pcls_in_chain'-- Number of particles in a chain
//! @param[in] 'include_head'-- If 'true', assume calculations are done
//! @param[in]      on a chain, not a chain fragment. A chain has a head,
//! @param[in]      and the head has a header, so the header occupies
//! @param[in]      a few bytes, throwing off the calculation.
//!
//! @return    Chain capacity
//!
unsigned nsvc_pcl_chain_capacity(unsigned pcls_in_chain, bool include_head)
{
    unsigned            first_pcl_capacity;
    unsigned            additional_pcl_capacity;

    if (0 == pcls_in_chain)
    {
        return 0;
    }

    if (include_head)
    {
        first_pcl_capacity = NSVC_PCL_SIZE_AT_HEAD;
    }
    else
    {
        first_pcl_capacity = NSVC_PCL_SIZE;
    }

    additional_pcl_capacity = (pcls_in_chain - 1) * NSVC_PCL_SIZE;

    return first_pcl_capacity + additional_pcl_capacity;
}

//!
//! @name      nsvc_pcl_pcls_for_capacity
//!
//! @brief     Calculate number of pcls in a hypothetical chain needed to
//! @brief     accomodate 'capacity' bytes.
//!
//! @param[in] 'capacity'-- Number of bytes which need to be stored in
//! @param[in]              the chain/chain fragement
//! @param[in] 'include_head'-- If 'true', assume calculations are done
//! @param[in]      on a chain, not a chain fragment. A chain has a head,
//! @param[in]      and the head has a header, so the header occupies
//! @param[in]      a few bytes, throwing off the calculation.
//!
//! @return    Number of particles required
//!
unsigned nsvc_pcl_pcls_for_capacity(unsigned capacity, bool include_head)
{
    unsigned first_pcl_capacity;
    unsigned additional_pcls_needed;

    if (include_head)
    {
        first_pcl_capacity = NSVC_PCL_SIZE_AT_HEAD;
    }
    else
    {
        first_pcl_capacity = NSVC_PCL_SIZE;
    }


    // Can fit in head pcl?
    if (capacity <= first_pcl_capacity)
    {
        return 1;
    }

    capacity -= first_pcl_capacity;

    // Round up to nearest pcl end
    additional_pcls_needed = ROUND_UP(capacity, NSVC_PCL_SIZE);

    return additional_pcls_needed + 1;
}

//!
//! @name      nsvc_pcl_count_pcls_in_chain
//!
//! @brief     Calculate number of pcls in chain/chain fragment
//! @brief     by walking chain.
//!
//! @param[in] 'head_pcl'-- Chain/chain fragment to walk
//!
//! @return    Number of particles
//!
unsigned nsvc_pcl_count_pcls_in_chain(nsvc_pcl_t *head_pcl)
{
    unsigned    count = 0;
    nsvc_pcl_t *this_pcl;

    SL_REQUIRE_API(NSVC_IS_PCL(head_pcl));

    this_pcl = head_pcl;

    while (NULL != this_pcl)
    {
        SL_ENSURE(NSVC_IS_PCL(this_pcl));
        count++;
        this_pcl = this_pcl->flink;
    }

    return count;
}

//!
//! @name      nsvc_pcl_write_data_no_continue
//!
//! @brief     Attempt to write 'data_length' bytes to a pcl,
//! @brief     starting at 'pcl_offset'. Write as many bytes before
//! @brief     running off end of particle (don't write to next
//! @brief     particle).
//!
//! @param[in] 'pcl'-- Particle to write data to.
//! @param[in] 'pcl_offset'-- Offset in particle to write to.
//! @param[in]       If pcl is a chain head, and 'pcl_offset'==0, 
//! @param[in]       then you'll be writing over head.
//! @param[in] 'data'-- data to write
//! @param[in] 'data_length'-- number of bytes to write from 'data'
//!
//! @return    Number of bytes written. Will be <= 'data_length'
//!
unsigned nsvc_pcl_write_data_no_continue(nsvc_pcl_t *pcl,
                                         unsigned    pcl_offset,
                                         uint8_t    *data,
                                         unsigned    data_length)
{
    unsigned remaining_length;
    unsigned write_length;

    SL_REQUIRE(pcl_offset < NSVC_PCL_SIZE);

    if (0 == data_length)
    {
        return 0;
    }

    remaining_length = NSVC_PCL_SIZE - pcl_offset;
    if (data_length <= remaining_length)
    {
        write_length = data_length;
    }
    else
    {
        write_length = remaining_length;
    }

    rutils_memcpy(&pcl->buffer[pcl_offset], data, write_length);

    return write_length;
}

//!
//! @name      nsvc_pcl_write_data_continue
//!
//! @brief     Attempt to write 'data_length' bytes to a pcl chain.
//!
//! @details   Starting write at position indicated by 'seek_ptr'.
//! @details   If write request will overrun pcl pointed to by
//! @details   'seek_ptr', then continue write on next pcl(s) in
//! @details   chain. If there are no more pcls in chain to
//! @details   continue write on, then quit write operation.
//!
//! @param[in] 'seek_ptr'-- Indicates where to begin write.
//! @param[out] 'seek_ptr'-- Advanced by number of bytes written.
//! @param[out]    seek_ptr->current_pcl will be set to null and
//! @param[out]    seek_ptr->offset_in_pcl will be set to zero when
//! @param[out]    write goes to end of chain.
//! @param[in] 'data'-- data to write
//! @param[in] 'data_length'-- number of bytes to write from 'data'
//!
//! @return    Number of bytes written. Will be <= 'data_length'
//!
unsigned nsvc_pcl_write_data_continue(nsvc_pcl_chain_seek_t *seek_ptr,
                                      uint8_t               *data,
                                      unsigned               data_length)
{
    nsvc_pcl_t  *this_pcl;
    unsigned     write_count;
    unsigned     total_count = 0;

    if ((0 == data_length) ||
        (NULL == seek_ptr->current_pcl))
    {
        return 0;
    }

    this_pcl = seek_ptr->current_pcl;

    // Loop through as many pcls as needed to fulfill write
    while (1)
    {
        SL_ENSURE(NSVC_IS_PCL(this_pcl));

        // Attempt to write 'data_length' bytes to this pcl.
        // 'write_count' will return count of as many bytes
        // as was possible to write.
        write_count = nsvc_pcl_write_data_no_continue(this_pcl,
                                       seek_ptr->offset_in_pcl,
                                       data,
                                       data_length);
        SL_ENSURE(write_count > 0);
        SL_ENSURE(write_count <= data_length);

        // Advance everything
        total_count += write_count;
        data += write_count;
        data_length -= write_count;
        seek_ptr->offset_in_pcl += write_count;

        // Did this write reach the end of a pcl?
        if (NSVC_PCL_SIZE == seek_ptr->offset_in_pcl)
        {
            seek_ptr->offset_in_pcl = 0;
            this_pcl = this_pcl->flink;
            seek_ptr->current_pcl = this_pcl;
        }

        // Write completed?
        if (0 == data_length)
        {
            break;
        }
        // Current pcl was last in chain?
        else if (NULL == this_pcl)
        {
            break;
        }
    }

    return total_count;
}

//!
//! @name      nsvc_pcl_write_dataWT
//!
//! @brief     Write 'data_length' bytes to a pcl chain.
//!
//! @details   Write to chain indicated by '*head_pcl_ptr'.
//! @details   If '*head_pcl_ptr'==NULL, then allocate a chain
//! @details   of 'data_length' capacity, then write beginning
//! @details   after pcl header. seek_ptr->offset_in_pcl will
//! @details   include bytes for header, though.
//! @details   
//! @details   If '*head_pcl_ptr'!=NULL, then start write at position
//! @details   indicated by 'seek_ptr'.
//! @details   If write request will overrun end of chain, append
//! @details   to chain the pcl(s) needed to complete write.
//! @details   
//! @details   Update 'seek_ptr' with position after last write.
//!
//! @param[in] 'head_pcl_ptr'-- Where to begin write. If points to NULL...
//! @param[out] 'head_pcl_ptr'-- ...location to put new chain.
//! @param[in] 'seek_ptr'-- Indicates where to begin write.
//! @param[out] 'seek_ptr'-- Advanced by number of bytes written.
//! @param[out]    seek_ptr->current_pcl will be set to null and
//! @param[out]    seek_ptr->offset_in_pcl will be set to zero when
//! @param[out]        write goes to end of chain.
//! @param[in] 'data'-- data to write
//! @param[in] 'data_length'-- number of bytes to write from 'data'
//! @param[in] 'timeout_ticks'-- (Same as 'nsvc_pcl_alloc_chainWT()')
//!
//! @return    (Same as 'nsvc_pcl_alloc_chainWT()')
//!
nufr_sema_get_rtn_t nsvc_pcl_write_dataWT(nsvc_pcl_t           **head_pcl_ptr,
                                          nsvc_pcl_chain_seek_t *seek_ptr,
                                          uint8_t               *data,
                                          unsigned               data_length,
                                          int                    timeout_ticks)
{
    unsigned            write_count;
    nufr_sema_get_rtn_t rv = NUFR_SEMA_GET_OK_NO_BLOCK;
    nsvc_pcl_header_t  *header_ptr;
    nsvc_pcl_t         *previous_tail;
    bool                lengthened;

    SL_REQUIRE_API(NULL != head_pcl_ptr);
    SL_REQUIRE_API(*head_pcl_ptr != NULL? NSVC_IS_PCL(*head_pcl_ptr) : true);

    if (0 == data_length)
    {
        return NUFR_SEMA_GET_OK_NO_BLOCK;
    }

    // Caller wants to have this call create the chain
    if (NULL == *head_pcl_ptr)
    {
        // This call will make chain large enough to accomodate
        // this write, without having to alloc any more.
        rv = nsvc_pcl_alloc_chainWT(head_pcl_ptr,
                                    NULL,
                                    data_length,
                                    timeout_ticks);

        if (!SUCCESS_ALLOC(rv))
        {
            return rv;
        }

        lengthened = true;

        header_ptr = NSVC_PCL_HEADER(*head_pcl_ptr);

        // Set 'seek_ptr' to point to head pcl, just after header
        seek_ptr->current_pcl = *head_pcl_ptr;
        seek_ptr->offset_in_pcl = NSVC_PCL_OFFSET_PAST_HEADER(0);
    }
    // Chain already exists
    else
    {
        SL_ENSURE(seek_ptr->offset_in_pcl < NSVC_PCL_SIZE);

        header_ptr = NSVC_PCL_HEADER(*head_pcl_ptr);

        lengthened = false;

        // Is seek position indicating end of chain?
        if (NULL == seek_ptr->current_pcl)
        {
            // A seek ptr should have zero offset when pcl is null
            SL_ENSURE(0 == seek_ptr->offset_in_pcl);

            previous_tail = header_ptr->tail;
            SL_ENSURE(NULL == previous_tail->flink);

            // This will lengthen chain with the number of pcls
            // needed to fulfill write
            rv = nsvc_pcl_lengthen_chainWT(*head_pcl_ptr,
                                           data_length,
                                           timeout_ticks);
            if (!SUCCESS_ALLOC(rv))
            {
                return rv;
            }

            lengthened = true;

            // Lengthen op should've added link to old tail
            SL_ENSURE(NULL != previous_tail->flink);
            seek_ptr->current_pcl = previous_tail->flink;
        }
    }

    // First try at a write. If chain was alloc'ed or lengthened, then
    // write will complete in this call. Otherwise, see how far we get.
    write_count = nsvc_pcl_write_data_continue(seek_ptr, data, data_length);

    SL_ENSURE(write_count <= data_length);

    if (write_count == data_length)
    {
        // Write completed first try.
        return rv;
    }

    SL_ENSURE(!lengthened);
    UNUSED_BY_ASSERT(lengthened);

    // Assume we reached here because chain was not alloced/lengthened and
    // first write didn't get all the data.
    // Need to lengthen chain and attempt another write...

    // Write should've left a null 'current_pcl'
    SL_ENSURE(NULL == seek_ptr->current_pcl);

    data += write_count;
    data_length -= write_count;

    previous_tail = header_ptr->tail;

    // Lengthen chain by bytes remaining
    rv = nsvc_pcl_lengthen_chainWT(*head_pcl_ptr, data_length, timeout_ticks);
    if (!SUCCESS_ALLOC(rv))
    {
        return rv;
    }

    // Old tail will have been appended to
    SL_ENSURE(NSVC_IS_PCL(previous_tail->flink));
    // Step into first pcl of chain extension
    seek_ptr->current_pcl = previous_tail->flink;
    seek_ptr->offset_in_pcl = 0;

    // At this point, we should have enough pcls to complete write.
    write_count = nsvc_pcl_write_data_continue(seek_ptr, data, data_length);
    SL_ENSURE(write_count == data_length);

    // NOTE: not updating header->length in this call

    // Use return value from 'nsvc_pcl_lengthen_chainWT()' call 
    return rv;
}


//!
//! @name      nsvc_pcl_contiguous_count
//!
//! @brief     Given seek location, calculate number of contiguous
//! @brief     bytes available before running over end of pcl.
//!
//! @param[in] 'seek_ptr'-- Location in pcl
//!
//! @return    Contiguous bytes
//!
unsigned nsvc_pcl_contiguous_count(nsvc_pcl_chain_seek_t *seek_ptr)
{
    if (NULL == seek_ptr->current_pcl)
    {
        return 0;
    }

    return NSVC_PCL_SIZE - seek_ptr->offset_in_pcl;
}

//!
//! @name      nsvc_pcl_get_previous_pcl
//!
//! @brief     Retrieve previous pcl in chain
//!
//! @details   Have to manually walk chain to get this,
//! @details   since there's no back-link.
//!
//! @param[in] 'head_pcl'-- Start location for ffwd op.
//! @param[in] 'current_pcl'-- Number of bytest to fast-forward
//!
//! @return    previous pcl; NULL if not found
//!
nsvc_pcl_t *nsvc_pcl_get_previous_pcl(nsvc_pcl_t *head_pcl,
                                      nsvc_pcl_t *current_pcl)
{
    nsvc_pcl_t *this_pcl;
    nsvc_pcl_t *previous_pcl;

    if (current_pcl == head_pcl)
    {
        return NULL;
    }

    previous_pcl = head_pcl;
    this_pcl = head_pcl->flink;

    while (NULL != this_pcl)
    {
        if (this_pcl == current_pcl)
        {
            return previous_pcl;
        }

        previous_pcl = this_pcl;
        this_pcl = this_pcl->flink;
    }

    return NULL;
}

//!
//! @name      nsvc_pcl_seek_ffwd
//!
//! @brief     Advance seek location by so many bytes.
//!
//! @details   Allow fast-forwarding to walk pcl chain,
//! @details   to achieve ffwd location. If ffwd request
//! @details   cannot be satisfied with length of chain
//! @details   as-is, then return 'false' and don't update
//! @details   'seek_ptr'.
//!
//! @param[in] 'seek_ptr'-- Start location for ffwd op.
//! @param[out] 'seek_ptr'-- Updated to fast-forwarded
//! @param[in] 'ffwd_amount'-- Number of bytest to fast-forward
//!
//! @return    'true' if successful.
//!
bool nsvc_pcl_seek_ffwd(nsvc_pcl_chain_seek_t *seek_ptr,
                       unsigned                ffwd_amount)
{
    nsvc_pcl_t   *current_pcl;
    unsigned      pcl_ffwd_count;
    unsigned      remaining_in_pcl;

    // seek ptr at end of chain? No room to ffwd then.
    if (NULL == seek_ptr->current_pcl)
    {
        return false;
    }

    remaining_in_pcl = NSVC_PCL_SIZE - seek_ptr->offset_in_pcl;

    // Will we stay on the same pcl?
    if (ffwd_amount < remaining_in_pcl)
    {
        seek_ptr->offset_in_pcl += ffwd_amount;
        return true;
    }

    // Subtract remaining bytes in beginning pcl
    ffwd_amount -= remaining_in_pcl;

    // 'pcl_ffwd_count' == how many pcl's to walk past through past next pcl
    //    to get to final pcl
    pcl_ffwd_count = ffwd_amount / NSVC_PCL_SIZE;

    // Adjust 'ffwd_amount' to offset in last pcl
    ffwd_amount -= pcl_ffwd_count * NSVC_PCL_SIZE;

    // Step into next pcl, then walk that number of pcl's
    current_pcl = seek_ptr->current_pcl->flink;
    while ((NULL != current_pcl) && (pcl_ffwd_count > 0))
    {
        pcl_ffwd_count--;
        current_pcl = current_pcl->flink;
    }

    // If ffwd request would walk over end of last pcl, then fail
    if (NULL == current_pcl)
    {
        return false;
    }

    seek_ptr->current_pcl = current_pcl;
    seek_ptr->offset_in_pcl = ffwd_amount;

    return true;
}

//!
//! @name      nsvc_pcl_seek_rewind
//!
//! @brief     Rewind seek location by so many bytes.
//!
//! @details   Limited to a rewind of 1 particle backwards.
//! @details   This is at least NSVC_PCL_SIZE.
//! @details   Error if attempt to rewind past beginning of chain.
//!
//! @param[in] 'head_pcl'-- Chain which 'seek_ptr' points to
//! @param[in] 'seek_ptr'-- Start location for rewind op.
//! @param[out] 'seek_ptr'-- Updated to rewinded value
//! @param[in] 'rewind_amount'-- Number of bytest to rewnd
//!
//! @return    'true' if successful.
//!
bool nsvc_pcl_seek_rewind(nsvc_pcl_t            *head_pcl,
                          nsvc_pcl_chain_seek_t *seek_ptr,
                          unsigned               rewind_amount)
{
    nsvc_pcl_t   *previous_pcl;
    unsigned      remaining_in_pcl;

    // seek ptr at end of chain? No room to ffwd then.
    if (NULL == seek_ptr->current_pcl)
    {
        return false;
    }

    // How many bytes of data can we rewind and
    // still remain the same pcl?
    remaining_in_pcl = seek_ptr->offset_in_pcl;

    // Result of rewind op: will stay on the same pcl?
    if (rewind_amount <= remaining_in_pcl)
    {
        seek_ptr->offset_in_pcl -= rewind_amount;
        return true;
    }

    // Rewind bytes in this pcl first...
    seek_ptr->offset_in_pcl = 0;
    rewind_amount -= remaining_in_pcl;

    // Apply limit on max rewind in 1 call
    if (rewind_amount > NSVC_PCL_SIZE)
    {
        SL_REQUIRE_API(0);
        return false;
    }

    // Find the pcl in the chain which is before the one
    // we're at. Brute force walk chain.
    previous_pcl = nsvc_pcl_get_previous_pcl(head_pcl, seek_ptr->current_pcl);

    // Shouldn't happen
    if (NULL == previous_pcl)
    {
        SL_REQUIRE(0);
        return false;
    }

    seek_ptr->current_pcl = previous_pcl;
    seek_ptr->offset_in_pcl = NSVC_PCL_SIZE - rewind_amount;

    return true;
}

//!
//! @name      nsvc_pcl_set_seek_to_packet_offset
//!
//! @brief     Given a chain, set seek ptr to offset.
//!
//! @details   This seek-set operation sets 'seek_ptr' according
//! @details   to offset in chain specified by 'chain_offset'.
//! @details   'chain_offset' is an offset into the data which
//! @details   as though the data were in a large, contiguous
//! @details   buffer, instead of being spread out over pcls.
//! @details   Assumes first pcl has header and so 'chain_offset'
//! @details   ==0 skips over header.
//! @details   (i.e., do NOT use NSVC_PCL_OFFSET_PAST_HEADER())
//!
//! @param[in] 'head_pcl'-- Chain to seek into. Cannot be a chain
//! @param[in]              fragment.
//! @param[out] 'seek_ptr'-- Updated to offset location, if successful
//! @param[in] 'chain_offset'-- Absolute byte offset into data
//!
//! @return    'true' if successful.
//!
bool nsvc_pcl_set_seek_to_packet_offset(nsvc_pcl_t            *head_pcl,
                                          nsvc_pcl_chain_seek_t *seek_ptr,
                                          unsigned               chain_offset)
{
    bool                  success;

    SL_REQUIRE_API(NSVC_IS_PCL(head_pcl));

    success = nsvc_pcl_set_seek_to_headerless_offset(head_pcl,
                                                     seek_ptr, 
                                   NSVC_PCL_OFFSET_PAST_HEADER(chain_offset));
    if (!success)
    {
        return false;
    }

    return true;
}

//!
//! @name      nsvc_pcl_set_seek_to_headerless_offset
//!
//! @brief     Given a chain, set seek ptr to offset.
//!
//! @details   This seek-set operation sets 'seek_ptr' according
//! @details   to offset in chain specified by 'chain_offset'.
//! @details   'chain_offset' is an offset into the data which
//! @details   as though the data were in a large, contiguous
//! @details   buffer, instead of being spread out over pcls.
//! @details   Assumes first pcl has header and so 'chain_offset'
//! @details   ==0 sets to beginning of header (unlike previous API).
//!
//! @param[in] 'head_pcl'-- Chain to seek into. Cannot be a chain
//! @param[in]              fragment.
//! @param[out] 'seek_ptr'-- Updated to offset location, if successful
//! @param[in] 'chain_offset'-- Absolute byte offset into particl
//!
//! @return    'true' if successful.
//!
bool nsvc_pcl_set_seek_to_headerless_offset(nsvc_pcl_t            *head_pcl,
                                            nsvc_pcl_chain_seek_t *seek_ptr,
                                            unsigned               chain_offset)
{
    nsvc_pcl_chain_seek_t non_chain_seek;
    bool                  success;

    SL_REQUIRE_API(NSVC_IS_PCL(head_pcl));

    // Initialize to zero absolute offset, which is after head's header
    non_chain_seek.current_pcl = head_pcl;
    non_chain_seek.offset_in_pcl = 0;

    success = nsvc_pcl_seek_ffwd(&non_chain_seek, chain_offset);

    if (!success)
    {
        return false;
    }

    seek_ptr->current_pcl = non_chain_seek.current_pcl;
    seek_ptr->offset_in_pcl = non_chain_seek.offset_in_pcl;

    return true;
}

//!
//! @name      nsvc_pcl_read
//!
//! @brief     Given a location in a chain/chain fragment,
//! @brief     read/attempt to read so many bytes.
//!
//! @details   If read operation attempts to read beyond the end of chain,
//! @details   then read will stop at end and return a value < 'read_length'.
//! @details   Also, chain header's 'length' field does not bound the read.
//!
//! @param[in] 'seek_ptr'-- Start location for read op.
//! @param[in]         seek_ptr->offset_in_pcl==0 is header start
//! @param[out] 'seek_ptr'-- Advanced according to bytes read.
//! @param[in] 'data'-- Copy bytes read here
//! @param[in] 'read_length'-- Number of bytes to read
//!
//! @return    number of bytes read
//!
unsigned nsvc_pcl_read(nsvc_pcl_chain_seek_t *seek_ptr,
                       uint8_t               *data,
                       unsigned               read_length)
{
    unsigned      remaining_in_pcl;
    nsvc_pcl_t   *current_pcl;
    unsigned      current_offset;
    unsigned      current_read_length;
    uint8_t      *read_ptr;
    unsigned      read_count = 0;

    if (0 == read_length)
    {
        return 0;
    }

    current_pcl = seek_ptr->current_pcl;
    current_offset = seek_ptr->offset_in_pcl;

    remaining_in_pcl = NSVC_PCL_SIZE - current_offset;

    if (remaining_in_pcl >= read_length)
    {
        current_read_length = read_length;
    }
    else
    {
        current_read_length = remaining_in_pcl;
    }

    // Walk through multiple pcls to fulfill read, if multiple needed
    while (NULL != current_pcl)
    {
        read_ptr = &current_pcl->buffer[current_offset];

        rutils_memcpy(data, read_ptr, current_read_length);
        read_count += current_read_length;

        read_length -= current_read_length;
        // Read finished in this pcl?
        if (0 == read_length)
        {
            current_offset += current_read_length;

            // Did we reach the end of the pcl by this read?
            if (NSVC_PCL_SIZE == current_offset)
            {
                current_pcl = current_pcl->flink;

                current_offset = 0;
            }

            break;
        }

        // ...otherwise, previous read couldn't finish in same pcl

        data += current_read_length;
        current_offset = 0;

        // Point to next pcl        
        current_pcl = current_pcl->flink;

        // Update 'read_length' for next pass through loop:
        // Fill entire pcl?
        if (read_length >= NSVC_PCL_SIZE)
        {
            current_read_length = NSVC_PCL_SIZE;
        }
        // Partial write of pcl/last write
        else
        {
            current_read_length = read_length;
        }
    }

    seek_ptr->current_pcl = current_pcl;

    // Last read stayed within chain?
    if (NULL != current_pcl)
    {
        seek_ptr->offset_in_pcl = current_offset;
    }
    // Else, last read stepped past last pcl
    else
    {
        seek_ptr->offset_in_pcl = 0;
    }

    return read_count;
}