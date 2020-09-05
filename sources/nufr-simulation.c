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

//! @file    nufr-simulation.c
//! @authors Bernie Woodland
//! @date    19Oct17

//!
//! @brief     pthread layer which allows the nufr kernel+platform
//! @brief     to run in a PC environment
//!

#include "nufr-global.h"
#include "nufr-api.h"
#include "nufr-platform-app.h"
#include "nufr-platform.h"
#include "nsvc-app.h"
#include "nsvc-api.h"
#include "nsvc.h"
#include "nufr-simulation.h"

#include "nufr-kernel-task.h"

#include "raging-global.h"
#include "raging-utils.h"
#include "raging-contract.h"

#include <semaphore.h>
#include <pthread.h>
#include <sched.h>

bool pthreads_disabled = true;

//!
//! @struct    pthread_task_t
//!
//! @brief     Task simulation state
//!
typedef struct
{
    pthread_t  thread;
    sem_t      sema;
    int        expected_sema_count;
    bool       is_launched;
} pthread_task_t;


//!
//! @name       os_tick_thread
//! @name       nufr_sim_os_tick_sem
//!
//! @brief      semaphore, pthread for thread handling OS tick (SYSTICK) simulation
//!
static pthread_t os_tick_thread;
sem_t            nufr_sim_os_tick_sem;

//!
//! @name       nufr_sim_bg_sem
//!
//! @brief      BG task pthread
//! @brief      BG task semaphore
//!
sem_t            nufr_sim_bg_sem;

//!
//! @name       pthread_task_objs
//!
//! @brief      Array of task simulation objects, one per task
//!
static pthread_task_t pthread_task_objs[NUFR_NUM_TASKS];

extern bool systick_active;
extern bool disable_systick;

// NOTES WHILE DEBUGGING IN VC2010
// Sema timers continue to run when stopped on a breakpoint.
// If a sema times out while stopped on a breakpoint while stepping
// through another thread, a context switch will occure when you
// hit F10 and switch you to the thread of the sema which
// timed out. Once the switched in task stops again, the debugger
// will switch you back to the thread you you hit F10, at the place
// you hit F10.

//!
//! @name      nufr_sim_launch_wrapper
//!
//! @brief     Wrapper for task entry point
//!
//! @details   Must use since fcn. ptrs are of different types.
//!
//! @param[in] 'arg_ptr'--pthread_create arg
//!
void *nufr_sim_launch_wrapper(void *arg_ptr)
{
    // fixme, linux: warning: ISO C forbids conversion of object pointer to function pointer type [-Wpedantic]
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic push
#endif
    void (*fcn)(unsigned parm) = arg_ptr;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    // Invoke real entry point
    // Parameter passing TBD
    (*fcn)(0);

    return NULL;
}

//!
//! @name      nufr_sim_context_switch
//!
//! @brief     Wrapper for task entry point
//!
//! @details   NUFR_INVOKE_CONTEXT_SWITCH() handler in pthread mode only
//!
void nufr_sim_context_switch(void)
{
    bool        bg_task_running;
    bool        bg_task_is_switchin;
    int         rv;
    unsigned    task_index;
    nufr_tcb_t *old_running_task;

    if (pthreads_disabled)
        return;

    bg_task_running = nufr_running == (nufr_tcb_t *)nufr_bg_sp;
    bg_task_is_switchin = NULL == nufr_ready_list;

    // Sanity checks
    if (!bg_task_running)
    {
        UT_REQUIRE(NUFR_IS_TCB(nufr_running));
    }

    if (!bg_task_is_switchin)
    {
        UT_REQUIRE(NUFR_IS_TCB(nufr_ready_list));
    }

    // Verify that context switch wasn't invoked spuriously.
    // There could be some corner cases which are unavoidable, however.
    UT_REQUIRE(nufr_running != nufr_ready_list);

    old_running_task = nufr_running;

    // If a task is being switched in, that task needs either needs
    // to be launched or needs to be released.
    // If task is being switched in, will need to update 'nufr_running'
    // If no task is switched in, then BG task runs, and 'nufr_running'
    // needs a special value
    if (!bg_task_is_switchin)
    {
        int rv;

        // If the os tick timer handler is about to wake up a task,
        // and no other tasks are running, then disable the handler
        if (systick_active && bg_task_running)
        {
            disable_systick = true;
        }

        task_index = NUFR_TCB_TO_TID(nufr_ready_list) - 1;

        // Check if task hasn't been launched yet. Launch it.
        // Path must've been from nufr_launch_task()
        if (!pthread_task_objs[task_index].is_launched)
        {
            const nufr_task_desc_t *desc_ptr;
            void *arg;

            pthread_task_objs[task_index].is_launched = true;

            nufr_running = nufr_ready_list;

            desc_ptr = nufrplat_task_get_desc(nufr_ready_list, NUFR_TID_null);
            UT_ENSURE(NULL != desc_ptr);

            arg = desc_ptr->entry_point_fcn_ptr;
            rv = pthread_create(&pthread_task_objs[task_index].thread,
                                NULL,
                                nufr_sim_launch_wrapper,
                                arg);

        }
        // ...task is already launched. Unblock it.
        else
        {
            nufr_running = nufr_ready_list;

            UT_ENSURE(0 == pthread_task_objs[task_index].expected_sema_count);
            pthread_task_objs[task_index].expected_sema_count++;
            rv = sem_post(&pthread_task_objs[task_index].sema);
            UT_ENSURE(rv >= 0);
        }

        // Had to move this up: when 'sem_post' was called in os tick
        // thread, occasionally it was switching to a task thread.
        // Shouldn't have. Don't know why.
        //
        //nufr_running = nufr_ready_list;
    }
    // BG task is being switched in. In pthread simulation, it runs
    // all the time, so no sema needs to be kicked.
    else
    {
        nufr_running = (nufr_tcb_t *)nufr_bg_sp;
    }

    // sanity check: if BG task is not running, then
    //   running task must be at head of ready list
    UT_ENSURE((nufr_running != (nufr_tcb_t *)nufr_bg_sp)?
           (nufr_running == nufr_ready_list) : true);
    UT_ENSURE((nufr_running == (nufr_tcb_t *)nufr_bg_sp)?
           (NULL == nufr_ready_list) : true);
    if (systick_active)
    {
        // sanity check: if called from OS Tick handler,
        //   then we must've switched in a task
        UT_ENSURE(nufr_running != (nufr_tcb_t *)nufr_bg_sp);
    }

    // If calling/switchout task isn't the BG task, block it.
    // Also, if this is called from os tick thread, nufr_running
    // has no indication of this--don't block os tick thread either.
    if (!bg_task_running && !systick_active)
    {
        // If the last ready task is about to be blocked, then
        // enable the systicks automatically.
        // You should keep a breakpoint on the systick thread,
        // otherwise you'll never be able to break execution
        //
        if ((nufr_running == (nufr_tcb_t *)nufr_bg_sp) &&
            (NULL == nufr_ready_list))
        {
            disable_systick = false;
        }

        task_index = NUFR_TCB_TO_TID(old_running_task) - 1;

        rv = sem_wait(&pthread_task_objs[task_index].sema);
        UT_ENSURE(rv >= 0);
        pthread_task_objs[task_index].expected_sema_count--;
        UT_ENSURE(0 == pthread_task_objs[task_index].expected_sema_count);
    }
}

//!
//! @name      nufr_sim_entry
//!
//! @brief     Callin for simulation.
//!
//! @details   Spawns thread for BG, then poll-calls systick handler,
//! @details   simulating hardware tick
//!
void nufr_sim_entry(nufr_sim_generic_fcn_ptr bg_fcn_ptr,
                    nufr_sim_generic_fcn_ptr tick_fcn_ptr)
{
    unsigned            i;
    int                 rv;
    struct timespec     ts;
    void               *arg;

    ts.tv_sec = 0;
    ts.tv_nsec = 100000000;    //100 millisecs
    UNUSED(ts);

    rv = sem_init(&nufr_sim_os_tick_sem, 0, 0);
    UT_REQUIRE(rv >= 0);

    rv = sem_init(&nufr_sim_bg_sem, 0, 0);
    UT_REQUIRE(rv >= 0);

    for (i = 0; i < NUFR_NUM_TASKS; i++)
    {
        rv = sem_init(&pthread_task_objs[i].sema, 0, 0);
        UT_REQUIRE(rv >= 0);
        pthread_task_objs[i].expected_sema_count = 0;
    }

    // Init here, before any threads are spawned
    nufr_init();
    nsvc_init();
    nsvc_mutex_init();
    nsvc_timer_init(nufrplat_systick_get_reference_time, NULL);
    nsvc_pcl_init();

    // Create Background Task
    nufr_running = (nufr_tcb_t *)nufr_bg_sp;
    arg = NULL;

    // fixme, linux: warning: passing argument 3 of ‘pthread_create’ from incompatible pointer type [-Wincompatible-pointer-types]
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#pragma GCC diagnostic push
#endif
    rv = pthread_create(&os_tick_thread, NULL, &tick_fcn_ptr, &arg);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    UT_REQUIRE(rv >= 0);

    //Invoke BG task thread
    (*bg_fcn_ptr)(NULL);

    // Cannot return from here or all threads will killed
}