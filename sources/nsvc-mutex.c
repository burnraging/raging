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

//! @file    nsvc-mutex.c
//! @authors Bernie Woodland
//! @date    17Sep17


#include "nufr-global.h"

#include "nsvc-api.h"
#include "nsvc.h"
#include "nufr-api.h"
#include "nufr-kernel-semaphore.h"
#include "nufr-platform-app.h"

#include "raging-contract.h"

//!
//!  @name        nsvc_mutex_block_t
//!
//!  @brief       Mutex block type
//!
typedef struct
{
    nufr_sema_t        sema;
} nsvc_mutex_block_t;

//!
//!  @name        NSVC_MUTEX_ID_TO_BLOCK
//!  @name        NSVC_MUTEX_BLOCK_TO_ID
//!  @name        NSVC_IS_MUTEX_BLOCK
//!
//!  @brief       Mutex block accessors.
//!
//!  @details     'NSVC_MUTEX_ID_TO_BLOCK'-- converts 'nsvc_mutex_t'
//!  @details     to 'nsvc_mutex_block_t
//!  @details     
//!  @details     'NSVC_MUTEX_ID_TO_BLOCK'-- converts 'nsvc_mutex_block_t'
//!  @details     to 'nsvc_mutex_t
//!  @details     
//!  @details     'NSVC_IS_MUTEX_BLOCK'-- returns 'true' if input is
//!  @details         a valid ptr to 'nsvc_mutex_block_t'
//!
#define NSVC_MUTEX_ID_TO_BLOCK(x)   ( &nsvc_mutex_block[(x) - 1] )
#define NSVC_MUTEX_BLOCK_TO_ID(y)   ( (nufr_mutex_t) ((unsigned)((y) - &nsvc_mutex_block[0]) + 1) )

#define NSVC_IS_MUTEX_BLOCK(x)      ( ((x) >= nsvc_mutex_block) &&            \
                              ((x) <= &nsvc_mutex_block[NSVC_NUM_MUTEX - 1]) )

//!
//!  @brief       Mutex blocks
//!
nsvc_mutex_block_t nsvc_mutex_block[NSVC_NUM_MUTEX];

//! @name      nsvc_mutex_init_single_sema
//!
//! @brief     Initialize a single mutex. Called by common SL init fcn.
//!
//! @details   All mutexes have priority inversion protection
//! @details   enabled.
//!
//! @param[in] 'mutex'--mutex to be initialized
//! @param[in] 'sema'--semaphore with which mutex consists
//!
static void nsvc_mutex_init_single_sema(nsvc_mutex_t mutex)
{
    nufr_sema_t         sema = (nufr_sema_t)0;
    nsvc_mutex_block_t *mutex_block;
    bool                alloc_rv;

    mutex_block = NSVC_MUTEX_ID_TO_BLOCK(mutex);
    SL_REQUIRE_API(NSVC_IS_MUTEX_BLOCK(mutex_block));

    alloc_rv = nsvc_sema_pool_alloc(&sema);
    SL_REQUIRE(alloc_rv);
    UNUSED_BY_ASSERT(alloc_rv);

    mutex_block->sema = sema;

    nufrkernel_sema_reset(NUFR_SEMA_ID_TO_BLOCK(sema), 1, true);
}

//! @name      nsvc_mutex_init
//!
//! @brief     Initialize all mutexes. Called by common SL init fcn.
//!
//! @details   All mutexes have priority inversion protection
//! @details   enabled.
//!
void nsvc_mutex_init(void)
{
    unsigned i;

    // Initialize mutexes
    //for (i = 0; i < NSVC_NUM_MUTEX; i++)
    // using below instead to suppress warning
    for (i = 0; i != NSVC_NUM_MUTEX; i++)
    {
        nsvc_mutex_t mutex = (nsvc_mutex_t)(i + 1);

        nsvc_mutex_init_single_sema(mutex);
    }

}

//! @name      nsvc_mutex_getW
//!
//! @brief     Get ownership of mutex, block until resource is obtained,
//! @brief     or until a message of abort priority is sent
//!
//! @param[in] 'mutex'--mutex to get
//! @param[in] 'abort_priority_of_rx_msg'--priority of message send
//! @param[in]     which will abort wait.
//! @param[in]     NOTE: requires NUFR_CS_TASK_KILL
//!
//! @return    Status
nufr_sema_get_rtn_t nsvc_mutex_getW(nsvc_mutex_t   mutex,
                                    nufr_msg_pri_t abort_priority_of_rx_msg)
{
    nsvc_mutex_block_t  *mutex_block;
    nufr_sema_get_rtn_t  rv;

    mutex_block = NSVC_MUTEX_ID_TO_BLOCK(mutex);
    SL_REQUIRE_API(NSVC_IS_MUTEX_BLOCK(mutex_block));
    SL_REQUIRE_API(NUFR_IS_SEMA_BLOCK(NUFR_SEMA_ID_TO_BLOCK(mutex_block->sema)));

    rv = nufr_sema_getW(mutex_block->sema, abort_priority_of_rx_msg);

    return rv;
}

//! @name      nsvc_mutex_getT
//!
//! @brief     Get ownership of mutex, block until resource is obtained,
//! @brief     or until the specified timeout occurs,
//! @brief     or until a message of abort priority is sent
//!
//! @param[in] 'mutex'--mutex to get
//! @param[in] 'abort_priority_of_rx_msg'--priority of message send
//! @param[in]     which will abort wait.
//! @param[in]     NOTE: requires NUFR_CS_TASK_KILL
//! @param[in] 'timeout_ticks'-- wait timeout in OS clock ticks.
//! @param[in]     If == 0, no waiting for mutex
//!
//! @return    Status. Will indicate if timeout occured.
nufr_sema_get_rtn_t nsvc_mutex_getT(nsvc_mutex_t   mutex,
                                    nufr_msg_pri_t abort_priority_of_rx_msg,
                                    unsigned       timeout_ticks)
{
    nsvc_mutex_block_t  *mutex_block;
    nufr_sema_get_rtn_t  rv;

    mutex_block = NSVC_MUTEX_ID_TO_BLOCK(mutex);
    SL_REQUIRE_API(NSVC_IS_MUTEX_BLOCK(mutex_block));
    SL_REQUIRE_API(NUFR_IS_SEMA_BLOCK(NUFR_SEMA_ID_TO_BLOCK(mutex_block->sema)));

    rv = nufr_sema_getT(mutex_block->sema, abort_priority_of_rx_msg,
                        timeout_ticks);

    return rv;
}

//!
//! @name      nsvc_mutex_release
//
//! @brief     Release ownership of mutex
//!
//! @details   If another task is waiting for ownership, it will
//! @details   take it. If multiple tasks are waiting, the task
//! @details   at the highest priority will take it.
//!
//! @param[in] 'mutex'
//!
//! @return     'true' if another task was waiting on this mutex
bool nsvc_mutex_release(nsvc_mutex_t mutex)
{
    nsvc_mutex_block_t  *mutex_block;
    bool                 rv;

    mutex_block = NSVC_MUTEX_ID_TO_BLOCK(mutex);
    SL_REQUIRE_API(NSVC_IS_MUTEX_BLOCK(mutex_block));
    SL_REQUIRE_API(NUFR_IS_SEMA_BLOCK(NUFR_SEMA_ID_TO_BLOCK(mutex_block->sema)));

    rv = nufr_sema_release(mutex_block->sema);

    return rv;
}