/*
Copyright (c) 2019, Bernie Woodland
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

//! @file    nufr-sanity-checks.c
//! @authors Bernie Woodland
//! @date    27Nov19
//!
//! @details This file contains compile-time and runtime checks of nufr
//! @details kernel and SL settings. It catches the following:
//! @details   #1 Misconfigurations. Error line will end with "!"
//! @details   #2 Going against recommended settings. Error will not end in "!"
//! @details Naturally, #2 is somewhat subjective, whereas #1 is not.
//! @details 
//! @details Assumptions:
//! @details   A) A "tiny" config is not used.
//! @details      We can afford to waste a bit of CPU, RAM, and codespace.
//! @details   B) All SL components are used
//! @details
//! @details Also, there are 2 levels of sanity checks: mandatory and optional.
//! @details A compile flag will turn off optional.
//! @details


#include "nufr-global.h"

#include "raging-contract.h"

#include "nufr-platform.h"
#include "nufr-platform-app.h"

#if NUFR_SANITY_TINY_MODEL_IN_USE == 0
    #include "nsvc.h"
    #include "nsvc-app.h"
    #include "nsvc-api.h"
#endif // NUFR_SANITY_TINY_MODEL_IN_USE

#include "nufr-sanity-checks.h"

#include "raging-utils.h"


//**** Set these compile switches in 'nufr-compile-switches.h"

//!
//! @brief     NUFR_SANITY_SKIP_OPTIONAL_CHECKS compile flag
//!
//! @details   Set to "1" to turn off optional compile- and run-time checks
//! @details   The mandatory check flag errors that are definitely bugs
//! @details   The optional checks are recommended settings, etc.
//! @details   
//!

//!
//! @brief     NUFR_SANITY_TINY_MODEL_IN_USE compile flag
//!
//! @details   Set to "1" to turn off optional compile- and run-time checks
//! @details   The mandatory check flag errors that are definitely bugs
//! @details   The optional checks are recommended settings, etc.
//! @details   
//!

//***** raging-contract.h settings

#if !defined(CONTRACT_ASSERT)
    #error "CONTRACT_ASSERT missing"
#elif !defined(NUFR_ASSERT_LEVEL)
    #error "NUFR_ASSERT_LEVEL missing"
#elif (NUFR_ASSERT_LEVEL > 9)
    #error "Set NUFR_ASSERT_LEVEL to one of the supported values"
#endif
 
//***** nufr-compile-switch.h settings

#if !defined(NUFR_CS_MESSAGING)
    #error "NUFR_CS_MESSAGING missing!"
#elif (NUFR_CS_MESSAGING == 0)
    #error "NUFR_CS_MESSAGING disabled"
#endif

#if !defined(NUFR_CS_MSG_PRIORITIES)
    #error "NUFR_CS_MSG_PRIORITIES missing!"
#elif (NUFR_CS_MSG_PRIORITIES < 1) || (NUFR_CS_MSG_PRIORITIES > 4)
    #error "NUFR_CS_MSG_PRIORITIES out of range!"
#endif

#if defined(NUFR_CS_WHICH_PLATFORM_MODEL)
    #if !defined(NUFR_CS_SEMAPHORE)
        #error "NUFR_CS_SEMAPHORE missing!"
    #elif (NUFR_CS_SEMAPHORE == 0) && (NUFR_CS_WHICH_PLATFORM_MODEL != NUFR_TINY_MODEL)
        #error "NUFR_CS_SEMAPHORE disabled"
    #endif
#endif

//*****  nufr-platform.h settings

#if !defined(NUFR_TICK_PERIOD)
    #error "NUFR_TICK_PERIOD missing!"
#elif (NUFR_TICK_PERIOD == 0)
    #error "NUFR_TICK_PERIOD cannot be zero!"
#elif (NUFR_TICK_PERIOD > 100)
    #error "Are you sure about the NUFR_TICK_PERIOD setting?"
#endif

//***** nufr-platform-app.h settings

static bool sanity_check_task_enums(void)
{
    if (0 == NUFR_NUM_TASKS)
    {
        // "You must have at least one task"
        CONTRACT_ASSERT(true);
        return false;
    }
    else if (NUFR_TID_null != 0)
    {
        // "You must keep NUFR_TID_null at zero!"
        CONTRACT_ASSERT(true);
        return false;
    }
    else if (NUFR_NUM_TASKS != (NUFR_TID_max - 1))
    {
        // "You cannot change definition for NUFR_NUM_TASKS!"
        CONTRACT_ASSERT(true);
        return false;
    }
    else if (NUFR_TPR_null != 0)
    {
        // "You must keep NUFR_TPR_null at zero!"
        CONTRACT_ASSERT(true);
        return false;
    }

    return true;
}

#if !defined(NUFR_MAX_MSGS)
    #error "NUFR_MAX_MSGS missing!"
#elif !defined(NUFR_SANITY_SKIP_OPTIONAL_CHECKS)
    #if (NUFR_MAX_MSGS < 10)
        #error "Recommend that there be at least 10 message blocks"
    #elif (NUFR_MAX_MSGS < (NUFR_NUM_TASKS * 5))
        #error "Recommend that there be at least 5 message blocks per task"
    #endif
#endif

#if !defined(NUFR_SANITY_TINY_MODEL_IN_USE)
static bool sanity_check_semas(void)
{
    if (0 != NUFR_SEMA_null)
    {
        // "You must keep NUFR_SEMA_null at zero!"
        CONTRACT_ASSERT(false);
        return false;
    }
    else if (NUFR_SEMA_null == NUFR_SEMA_max)
    {
        // "NUFR_SEMA_max cannot be zero!"
        CONTRACT_ASSERT(false);
        return false;
    }
    else if (NUFR_SEMA_max < (NUFR_SEMA_POOL_END + 1))
    {
        // "NUFR_SEMA_max was miscalculated!"
        CONTRACT_ASSERT(false);
        return false;
    }
    else if (NUFR_SEMA_POOL_START == NUFR_SEMA_null)
    {
        // "NUFR_SEMA_POOL_START cannot be zero!"
        CONTRACT_ASSERT(false);
        return false;
    }
    else if (NUFR_NUM_SEMAS != (NUFR_SEMA_max - 1))
    {
        // "You cannot change NUFR_NUM_SEMAS calculation!"
        CONTRACT_ASSERT(false);
        return false;
    }
    else if (NUFR_SEMA_POOL_END < NUFR_SEMA_POOL_START)
    {
        // "NUFR_SEMA_POOL_START and NUFR_SEMA_POOL_END mixed up!"
        CONTRACT_ASSERT(false);
        return false;
    }

    return true;
}
#endif

#if !defined(NUFR_SANITY_SKIP_OPTIONAL_CHECKS) && !defined(NUFR_SANITY_TINY_MODEL_IN_USE)
bool sanity_check_semas_optional(void)
{
    // Pools allocated for the following
    //
    //    qty            use
    //   -----          -----
    //     1          PCL pool
    //     1          RNET stack
    //     1          SSP driver
    //
    const unsigned NUM_POOLS_CURRENTLY_IN_CODEBASE = 3;

    if ( (NUFR_SEMA_POOL_END - NUFR_SEMA_POOL_START) <
         (NSVC_NUM_MUTEX + NUM_POOLS_CURRENTLY_IN_CODEBASE) )
    {
        // "Sema pool not big enough for worst-case code inclusion"
        CONTRACT_ASSERT(false);
        return false;
    }

    return true;
}
#endif

//***** nsvc-app.h settings

#if !defined(NUFR_SANITY_TINY_MODEL_IN_USE)
static bool sanity_check_mutexes(void)
{
    if (0 != NSVC_MUTEX_null)
    {
        // "You must keep NSVC_MUTEX_null at zero!"
        CONTRACT_ASSERT(false);
        return false;
    }
    else if (NSVC_NUM_MUTEX != (NSVC_MUTEX_max - 1))
    {
        // "You cannot change NSVC_NUM_MUTEX calculation!"
        CONTRACT_ASSERT(false);
        return false;
    }

    return true;
}
#endif

#if !defined(NUFR_SANITY_SKIP_OPTIONAL_CHECKS) && !defined(NUFR_SANITY_TINY_MODEL_IN_USE)
static bool sanity_check_mutexes_optional(void)
{
    // gcc 9.x gives a compile error if there isn't at least 1
    //   mutex defined
    if (NSVC_MUTEX_null == NSVC_MUTEX_max)
    {
        // "NSVC_MUTEX_max cannot be zero!"
        CONTRACT_ASSERT(false);
        return false;
    }

    return true;
}    
#endif

#if !defined(NUFR_SANITY_TINY_MODEL_IN_USE)
    #if !defined(NSVC_NUM_TIMER)
        #error "Missing definition of NSVC_NUM_TIMER!"
    #elif NSVC_NUM_TIMER < 5
        #error "Recommend that you have at least 5 app timers in pool"
    #endif

    #if !defined(NSVC_PCL_SIZE) != !defined(NSVC_PCL_NUM_PCLS)
        #error "Must define both NSVC_PCL_SIZE and NSVC_PCL_NUM_PCLS at same time!"
    #elif defined(NSVC_PCL_SIZE)
        #if NSVC_PCL_SIZE < 100
            #error "Recommend that NSVC_PCL_SIZE be 100 or more"
        #endif
        #if NSVC_PCL_NUM_PCLS < 20
            #error "Recommend that NSVC_PCL_NUM_PCLS be 20 or more"
        #endif
    #endif
#endif

//********  Check task stacks, task entry points.

bool sanity_check_tasks_allocations(void)
{
    uint32_t  *task_stack1_ptr;
    uint32_t  *task_stack2_ptr;
    unsigned   task_stack1_size;
    unsigned   task_stack2_size;
    unsigned   task_priority;
    unsigned   i, k;
    bool       rc;

    // Make sure 'nufr_task_desc[]' is defined 'nufr_task_desc[NUFR_NUM_TASKS]'
    if (ARRAY_SIZE(nufr_task_desc) != NUFR_NUM_TASKS)
    {
        CONTRACT_ASSERT(false);
        return false;
    }

    // Minimum stack size allowed
    // 64 bytes needed for register stacking on Cortex M
    // Throw in another 64 bytes for safe measure
    // (We can tweak this for MSP430, etc. later)
    const unsigned MIN_STACK_SIZE = 128;

    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        task_stack1_ptr = nufr_task_desc[i].stack_base_ptr;
        task_stack1_size = nufr_task_desc[i].stack_size;

        // Stack ptr must point into RAM somewhere
        // (We can tweak this in the future to be a compiler-specific
        //  check for RAM section)
        if (NULL == task_stack1_ptr)
        {
            CONTRACT_ASSERT(false);
            return false;
        }
        // Stack size sane?
        else if (task_stack1_size < MIN_STACK_SIZE)
        {
            CONTRACT_ASSERT(false);
            return false;
        }

        // Check all other tasks after this one in the list to be
        // sure we're not duplicating a stack variable
        for (k = i+1; k < NUFR_NUM_TASKS - 1; k++)
        {
            task_stack2_ptr = nufr_task_desc[k].stack_base_ptr;
            task_stack2_size = nufr_task_desc[k].stack_size;

            rc = rutils_does_memory_overlap((uint8_t *)task_stack1_ptr,
                                            (uint8_t *)task_stack2_ptr,
                                            task_stack1_size,
                                            task_stack2_size);

            if (rc)
            {
                CONTRACT_ASSERT(false);
                return false;
            }
        }

        task_priority = nufr_task_desc[i].start_priority;

        // Cannot use a nufr-reserved priority
        if ((NUFR_TPR_null == task_priority) ||
            (NUFR_TPR_guaranteed_highest == task_priority))
        {
            CONTRACT_ASSERT(false);
            return false;
        }

        // Must have a valid entry point
        // (We can tweak this in the future to ensure that
        //  the entry point is in FLASH.)
        if (NULL == nufr_task_desc[i].entry_point_fcn_ptr)
        {
            CONTRACT_ASSERT(false);
            return false;
        }
    }

    return true;
}

#if !defined(NUFR_SANITY_SKIP_OPTIONAL_CHECKS)
bool sanity_check_tasks_optional(void)
{
    unsigned   task_priority;
    unsigned   i;
    bool       is_there_a_nominal = false;

    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        task_priority = nufr_task_desc[i].start_priority;

        // Is there at least one task of nominal priority?
        if (NUFR_TPR_NOMINAL == task_priority)
        {
            is_there_a_nominal = true;
        }
    }

    // This isn't a must-have for nufr to run without errors,
    // but I've added enforcement anyways, so user
    // doesn't overlook an optimization.
    if (!is_there_a_nominal)
    {
        CONTRACT_ASSERT(false);
        return false;
    }

    return true;
}
#endif  // NUFR_SANITY_SKIP_OPTIONAL_CHECKS


#if NUFR_SANITY_TINY_MODEL_IN_USE == 0
//!
//! @name      nufr_sane_init
//!
//! @brief     Wrapper to do all nufr-related inits plus the runtime checks
//!
//! @param[in] 'fptr_current_time'-- See nsvc_timer_int()
//! @param[in] 'fptr_reconfigure'-- See nsvc_timer_int()
//!
bool nufr_sane_init(nsvc_timer_get_current_time_fcn_ptr_t fptr_current_time,
          nsvc_timer_quantum_device_reconfigure_fcn_ptr_t fptr_reconfigure)
{
    // Mandatory runtime checks
    if (!sanity_check_task_enums())
    {
        return false;
    }
    
    if (!sanity_check_semas())
    {
        return false;
    }
    
    if (!sanity_check_mutexes())
    {
        return false;
    }
    
    if (!sanity_check_tasks_allocations())
    {
        return false;
    }
    
#if !defined(NUFR_SANITY_SKIP_OPTIONAL_CHECKS)
    // Optional runtime checks
    if (!sanity_check_semas_optional())
    {
        return false;
    }
    
    if (!sanity_check_mutexes_optional())
    {
        return false;
    }
    
    if (!sanity_check_tasks_optional())
    {
        return false;
    }
#endif

    // Call kernel and all SL init fcns
    nufr_init();
    nsvc_init();
    nsvc_pcl_init();
    nsvc_timer_init(fptr_current_time, fptr_reconfigure);
    nsvc_mutex_init();
    
    return true;
}
#else
//!
//! @name      nufr_sane_init
//!
//! @brief     Wrapper to do all nufr-related inits plus the runtime checks
//!
bool nufr_sane_init(void)
{
    // Mandatory runtime checks
    if(!sanity_check_task_enums())
    {
        return false;
    }
    
    if(!sanity_check_tasks_allocations())
    {
        return false;
    }
    
#if !defined(NUFR_SANITY_SKIP_OPTIONAL_CHECKS)
    if(!sanity_check_tasks_optional())
    {
        return false;
    }
#endif

    nufr_init();
    
    return true;
}

#endif
