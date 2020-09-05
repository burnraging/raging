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

//! @file    nufr-kernel-task.c
//! @authors Bernie Woodland
//! @date    25Jul17


#define NUFR_TASK_GLOBAL_DEFS

#include "nufr-global.h"

#include "nufr-platform.h"
#include "nufr-platform-import.h"
#include "nufr-platform-app.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-timer.h"
#include "nufr-api.h"
#include "nufr-kernel-semaphore.h"

#include "raging-contract.h"
#include "raging-utils-mem.h"

nufr_tcb_t nufr_tcb_block[NUFR_NUM_TASKS];

// Current running task.
// Only updated by PendSV handler
// If BG task is running, set to ptr to 'nufr_bg_sp'
nufr_tcb_t *nufr_running;

// Ready list head. NULL if list is empty/the BG task is running.
// This is also the current running task
nufr_tcb_t *nufr_ready_list;

//ready list tail of NUFR_TPR_NOMINAL tasks. NULL if no NUFR_TPR_NOMINAL
//   tasks on list.
nufr_tcb_t *nufr_ready_list_tail_nominal;

//ready list tail. NULL if list is empty
nufr_tcb_t *nufr_ready_list_tail;

// Background task's stack pointer (necessary since BG task has no TCB).
// Must be big enough to hold an SP offset in it.
// NOTE: by using 'unsigned *' type, sizeof(unsigned *) == sizeof(void *);
//       sizeof(unsigned *) on 16-bit CPU will be 2; will be 4 on 32-bit CPU.
unsigned *nufr_bg_sp[NUFR_SP_INDEX_IN_TCB + 1];

uint16_t nufr_bop_key;

// .h's placed down here to pick up global variable definitions above
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    #include "nufr-kernel-task-inlines.h"
    #include "nufr-kernel-semaphore-inlines.h"
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
#ifdef USING_SECONDARY_CONTEXT_SWITCH_FILE
    #include "secondary-context-switch.h"
#endif


// If using inlines, forbid using these
#if NUFR_CS_OPTIMIZATION_INLINES == 0

//! @name      nufrkernel_add_task_to_ready_list
//
//! @brief     Inserts a task into the ready list.
//
//! @details   Ready list is sorted by priority, highest priority on head.
//! @details   Updates tcb->block_flags, globals nufr_ready_list, nufr_ready_list_tail_nominal,
//! @details   nufr_ready_list_tail.
//! @details   Interrupts blocked by caller, not in here.
//
//! @param[in] tcb-- task to be inserted
//
//! @return    'true' if the add necessitates a context switch
bool nufrkernel_add_task_to_ready_list(nufr_tcb_t *tcb)
{
    bool      do_switch = false;
    unsigned  priority;

    KERNEL_ENSURE_IL(NULL != tcb);
    KERNEL_ENSURE_IL(NULL == tcb->flink);

    priority = tcb->priority;

    // The "Fast-Inserts" are ones which use one, two, or three of
    // the head & tail pointers as a means to avoid a Ready List
    // walk. A Fast-Insert is an O**1 operation, vs. a Ready List walk,
    // which is an 0**N operation. Ready List walks don't scale when
    // NUFR is configured for many tasks, meaning that there will be
    // times when the Ready List will have times when many task are on it.
    //
    // The Fast-Inserts constitute a top-level if/else-if/else, and
    // are ordered in highest to lowest typical probability.

    //**** Fast-Insert #1:
    //  Empty ready list
    if (NULL == nufr_ready_list)
    {
        if (NUFR_TPR_NOMINAL == priority)
        {
            nufr_ready_list_tail_nominal = tcb;
        }

        nufr_ready_list = tcb;
        nufr_ready_list_tail = tcb;

        do_switch = true;

        KERNEL_ENSURE_IL(NULL == tcb->flink);
        KERNEL_ENSURE_IL(nufr_ready_list == nufr_ready_list_tail);
    }
    //**** Fast-Insert #2:
    //  Nominal task inserted behind pre-existing nominal tasks
    else if ((NUFR_TPR_NOMINAL == priority) &&
             (NULL != nufr_ready_list_tail_nominal))
    {
        nufr_tcb_t *flink = nufr_ready_list_tail_nominal->flink;
        nufr_ready_list_tail_nominal->flink = tcb;
        tcb->flink = flink;

        nufr_ready_list_tail_nominal = tcb;

        if (NULL == flink)
        {
            nufr_ready_list_tail = tcb;
        }

        KERNEL_ENSURE_IL(NULL != nufr_ready_list);
        KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);
    }
    //*** Fast-Insert #3:
    // Insert at head of Ready List;
    //   this task will be highest priority task.
    else if (priority < nufr_ready_list->priority)
    {
        if (NUFR_TPR_NOMINAL == priority)
        {
            nufr_ready_list_tail_nominal = tcb;
        }

        tcb->flink = nufr_ready_list;
        nufr_ready_list = tcb;

        do_switch = true;

        KERNEL_ENSURE_IL(NULL != nufr_ready_list);
        KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);
    }
    //*** Fast-Insert #4:
    // Insert at end of Ready List;
    //   this task will be lowest priority task.
    else if (priority >= nufr_ready_list_tail->priority)
    {
        if (NUFR_TPR_NOMINAL == priority)
        {
            nufr_ready_list_tail_nominal = tcb;
        }

        nufr_ready_list_tail->flink = tcb;
        nufr_ready_list_tail = tcb;

        KERNEL_ENSURE_IL(NULL != nufr_ready_list);
        KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);
    }
    //*** List Walk
    // Start walk either from head or from nominal tail.
    // The Fast-Insert checks above guarantee that this
    // insert will be between two tasks, not one task
    // and a null.
    else
    {
        // The insert will occur between 'prev_tcb' and 'next_tcb'
        nufr_tcb_t *prev_tcb;
        nufr_tcb_t *next_tcb;
        bool        null_nominal_tail = (NULL == nufr_ready_list_tail_nominal);

        KERNEL_ENSURE_IL(NULL != nufr_ready_list);
        KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);
        KERNEL_ENSURE_IL(nufr_ready_list != nufr_ready_list_tail);

        // If task priority is higher than nominal, or if
        // there are no nominal tasks on Ready List, then
        // start walk from the beginning, between the first
        // and second tasks.
        if ((priority < NUFR_TPR_NOMINAL) || null_nominal_tail)
        {
            if (null_nominal_tail && (NUFR_TPR_NOMINAL == priority))
            {
                nufr_ready_list_tail_nominal = tcb;
            }

            prev_tcb = nufr_ready_list;
        }
        // Otherwise, we're inserting a task which is
        // nominal priority or lower onto a list which
        // has 1 or more nominal tasks. In this case we
        // can speed up walk by starting deeper in list.
        else
        {
            prev_tcb = nufr_ready_list_tail_nominal;
        }

        // 'next_tcb' guaranteed here to be non-null
        next_tcb = prev_tcb->flink;

        KERNEL_ENSURE_IL(NULL != prev_tcb);
        KERNEL_ENSURE_IL(NULL != next_tcb);

        // Since we're guaranteed to insert between two
        // tasks, null flinks cannot occur. Therefore
        // can skip checks for null in loop:
        //while (true)
        // For safety's sake, keep null check
        while (NULL != next_tcb)
        {
            // Is next task lower priority? Then we've
            //  found the place to insert.
            if (priority < next_tcb->priority)
            {
                KERNEL_ENSURE_IL(NULL != prev_tcb);
                KERNEL_ENSURE_IL(NULL != next_tcb);

                tcb->flink = next_tcb;
                prev_tcb->flink = tcb;

                break;
            }

            prev_tcb = next_tcb;
            next_tcb = next_tcb->flink;
        }

        KERNEL_ENSURE_IL(NULL != next_tcb);
    }

    // If there is a head, there must be a tail
    // If there is not head, there cannot be a tail
    KERNEL_ENSURE_IL((nufr_ready_list == NULL) == (nufr_ready_list_tail == NULL));
    // If there is no tail, there cannot be a nominal tail either
    KERNEL_ENSURE_IL(nufr_ready_list == NULL?
               nufr_ready_list_tail_nominal == NULL : true);
    // Last task on list must have an flink == NULL
    KERNEL_ENSURE_IL(nufr_ready_list_tail != NULL?
               nufr_ready_list_tail->flink == NULL : true);
    // If there's > 1 task on list, then head's flink cannot be NULL
    KERNEL_ENSURE_IL((nufr_ready_list != NULL) && (nufr_ready_list_tail != NULL)
           && (nufr_ready_list != nufr_ready_list_tail)?
               nufr_ready_list->flink != NULL : true);

    return do_switch;
}

//! @name      nufrkernel_block_running_task
//
//! @brief     Ready list head is popped, next task becomes current running
//! @brief     task
//
//! @details   Current running task is removed from ready list, leaving next
//! @details   task in list to become current running task.
//! @details   Updates TCB flags, globals nufr_ready_list,
//! @details   nufr_ready_list_tail_nominal, nufr_ready_list_tail.
//! @details   Updates tcb->block_flags, tcb->notifications,
//! @details   Interrupts blocked by caller, not in here.
//
//! @param[in] block_flag-- single bit set into tcb->block_flags
//! @param[in]                 indicating blocking condition
void nufrkernel_block_running_task(unsigned block_flag)
{
    nufr_tcb_t *next_tcb;

    // One and only one of the blocking bits is set
    KERNEL_REQUIRE_IL(ANY_BITS_SET(block_flag, NUFR_TASK_NOT_LAUNCHED   |
                                     NUFR_TASK_BLOCKED_ASLEEP |
                                     NUFR_TASK_BLOCKED_BOP    |
                                     NUFR_TASK_BLOCKED_MSG    |
                                     NUFR_TASK_BLOCKED_SEMA));
    KERNEL_REQUIRE_IL(ANY_BITS_SET(block_flag, NUFR_TASK_NOT_LAUNCHED)?
            ARE_BITS_CLR(block_flag, NUFR_TASK_BLOCKED_ASLEEP |
                                     NUFR_TASK_BLOCKED_BOP    |
                                     NUFR_TASK_BLOCKED_MSG    |
                                     NUFR_TASK_BLOCKED_SEMA)
            : true);
    KERNEL_REQUIRE_IL(ANY_BITS_SET(block_flag, NUFR_TASK_BLOCKED_ASLEEP)?
            ARE_BITS_CLR(block_flag, NUFR_TASK_BLOCKED_BOP    |
                                     NUFR_TASK_BLOCKED_MSG    |
                                     NUFR_TASK_BLOCKED_SEMA)
            : true);
    KERNEL_REQUIRE_IL(ANY_BITS_SET(block_flag, NUFR_TASK_BLOCKED_BOP)?
            ARE_BITS_CLR(block_flag, NUFR_TASK_BLOCKED_MSG    |
                                     NUFR_TASK_BLOCKED_SEMA)
            : true);
    KERNEL_REQUIRE_IL(ANY_BITS_SET(block_flag, NUFR_TASK_BLOCKED_BOP)?
            ARE_BITS_CLR(block_flag, NUFR_TASK_BLOCKED_SEMA)
            : true);

    // There must be a task to block
    KERNEL_REQUIRE_IL(NULL != nufr_ready_list);
    KERNEL_REQUIRE_IL(NULL != nufr_ready_list_tail);

    nufr_ready_list->block_flags = block_flag;

    next_tcb = nufr_ready_list->flink;
    nufr_ready_list->flink = NULL;

    // Is there only 1 task (the task being blocked) on the ready list?
    if (NULL == next_tcb)
    {
        KERNEL_ENSURE_IL(nufr_ready_list == nufr_ready_list_tail);

        nufr_ready_list = NULL;
        nufr_ready_list_tail = NULL;
        nufr_ready_list_tail_nominal = NULL;
    }
    else
    {
        // We're the last nominal priority task on list?
        if (nufr_ready_list_tail_nominal == nufr_ready_list)
        {
            nufr_ready_list_tail_nominal = NULL;
        }

        nufr_ready_list = next_tcb;
    }

    // If there is a head, there must be a tail
    // If there is not head, there cannot be a tail
    KERNEL_ENSURE_IL((nufr_ready_list == NULL) == (nufr_ready_list_tail == NULL));
    // If there is no tail, there cannot be a nominal tail either
    KERNEL_ENSURE_IL(nufr_ready_list == NULL?
               nufr_ready_list_tail_nominal == NULL : true);
    // Last task on list must have an flink == NULL
    KERNEL_ENSURE_IL(nufr_ready_list_tail != NULL?
               nufr_ready_list_tail->flink == NULL : true);
    // If there's > 1 task on list, then head's flink cannot be NULL
    KERNEL_ENSURE_IL((nufr_ready_list != NULL) && (nufr_ready_list_tail != NULL)
           && (nufr_ready_list != nufr_ready_list_tail)?
               nufr_ready_list->flink != NULL : true);
}

//! @name      nufrkernel_remove_head_task_from_ready_list
//
//! @brief     Ready list head is popped, next task becomes current running
//! @brief     task. No TCB bits are changed.
//
//! @details   Interrupts blocked by caller, not in here.
void nufrkernel_remove_head_task_from_ready_list(void)
{
    nufr_tcb_t *next_tcb;

    KERNEL_ENSURE_IL(NULL != nufr_ready_list);
    KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);

    next_tcb = nufr_ready_list->flink;
    nufr_ready_list->flink = NULL;

    // Is there only 1 task (the task being blocked) on the ready list?
    if (NULL == next_tcb)
    {
        KERNEL_ENSURE_IL(nufr_ready_list == nufr_ready_list_tail);

        nufr_ready_list = NULL;
        nufr_ready_list_tail = NULL;
        nufr_ready_list_tail_nominal = NULL;
    }
    else
    {
        // We're the last nominal priority task on list?
        if (nufr_ready_list_tail_nominal == nufr_ready_list)
        {
            nufr_ready_list_tail_nominal = NULL;
        }

        nufr_ready_list = next_tcb;
    }

    // If there is a head, there must be a tail
    // If there is not head, there cannot be a tail
    KERNEL_ENSURE_IL((nufr_ready_list == NULL) == (nufr_ready_list_tail == NULL));
    // If there is no tail, there cannot be a nominal tail either
    KERNEL_ENSURE_IL(nufr_ready_list == NULL?
               nufr_ready_list_tail_nominal == NULL : true);
    // Last task on list must have an flink == NULL
    KERNEL_ENSURE_IL(nufr_ready_list_tail != NULL?
               nufr_ready_list_tail->flink == NULL : true);
    // If there's > 1 task on list, then head's flink cannot be NULL
    KERNEL_ENSURE_IL((nufr_ready_list != NULL) && (nufr_ready_list_tail != NULL)
           && (nufr_ready_list != nufr_ready_list_tail)?
               nufr_ready_list->flink != NULL : true);
}

//! @name      nufr_delete_task_from_ready_list
//
//! @brief     Deletes a task from the ready list.
//
//! @details   Ready list is walked, looking for a match to 'tcb'.
//! @details   If/when 'tcb' is found, it's removed from the list,
//! @details   list is stitched back together again.
//! @details   nufr_ready_list_tail.
//! @details   Updates tcb->block_flags, globals nufr_ready_list,
//! @details   nufr_ready_list_tail_nominal.
//! @details   Interrupts blocked by caller, not in here.
//
//! @param[in] tcb-- task to be inserted
void nufrkernel_delete_task_from_ready_list(nufr_tcb_t *tcb)
{
    nufr_tcb_t *prev_tcb;
    nufr_tcb_t *this_tcb;
    nufr_tcb_t *next_tcb;
    bool        found_it;

    // Cannot be BG task or be a NULL ptr
    KERNEL_REQUIRE_IL(NUFR_IS_TCB(tcb));

    // Empty list?
    if (NULL == nufr_ready_list)
    {
        return;
    }
    // Sanity check: can't remove ourselves
    else if (tcb == nufr_running)
    {
        return;
    }

    KERNEL_ENSURE_IL(NULL != nufr_ready_list);
    KERNEL_ENSURE_IL(NULL != nufr_ready_list_tail);
    // Sanity check: only the BG task can remove the last task
    KERNEL_ENSURE_IL(nufr_ready_list == nufr_ready_list_tail ? 
              nufr_running == (nufr_tcb_t *)nufr_bg_sp
              : true);

    // Initialize tcb's for list walk
    if (tcb == nufr_ready_list)
    {
        prev_tcb = NULL;
        this_tcb = nufr_ready_list;
    }
    else
    {
        prev_tcb = nufr_ready_list;
        this_tcb = prev_tcb->flink;
    }

    // Do non-optimized list walk to find 'tcb'
    found_it = false;

    // 'this_tcb' will only be NULL if 'this_tcb' isn't ready
    while (NULL != this_tcb)
    {
        if (tcb == this_tcb)
        {
            found_it = true;

            break;
        }

        prev_tcb = this_tcb;
        this_tcb = this_tcb->flink;
    }

    // Wasn't found?
    if (!found_it || (NULL == this_tcb))
    {
        return;
    }

    next_tcb = this_tcb->flink;

    // Adjust head, tail, nominal tail if necessary

    // Removal task at head?
    if (tcb == nufr_ready_list)
    {
        // Next task is new head.
        // If next task is NULL, then removal task was also last on list
        nufr_ready_list = next_tcb;
    }

    // Removal task at nominal tail?
    if (tcb == nufr_ready_list_tail_nominal)
    {
        // A nominal task before removal task?
        if ((NULL != prev_tcb) && (NUFR_TPR_NOMINAL == prev_tcb->priority))
        {
            nufr_ready_list_tail_nominal = prev_tcb;
        }
        // No more nominal tasks...
        else
        {
            nufr_ready_list_tail_nominal = NULL;
        }
    }

    // Removal task at tail?
    if (tcb == nufr_ready_list_tail)
    {
        // Tail becomes previous task.
        // If previous was NULL, then removal task was only one on list
        nufr_ready_list_tail = prev_tcb;
    }

    // Stitch flinks
    if (NULL != prev_tcb)
    {
        prev_tcb->flink = tcb->flink;
    }

    tcb->flink = NULL;

    // If there is a head, there must be a tail
    // If there is not head, there cannot be a tail
    KERNEL_ENSURE_IL((nufr_ready_list == NULL) == (nufr_ready_list_tail == NULL));
    // If there is no tail, there cannot be a nominal tail either
    KERNEL_ENSURE_IL(nufr_ready_list == NULL?
               nufr_ready_list_tail_nominal == NULL : true);
    // Last task on list must have an flink == NULL
    KERNEL_ENSURE_IL(nufr_ready_list_tail != NULL?
               nufr_ready_list_tail->flink == NULL : true);
    // If there's > 1 task on list, then head's flink cannot be NULL
    KERNEL_ENSURE_IL((nufr_ready_list != NULL) && (nufr_ready_list_tail != NULL)
           && (nufr_ready_list != nufr_ready_list_tail)?
               nufr_ready_list->flink != NULL : true);
}

#endif  // NUFR_CS_OPTIMIZATION_INLINES == 0

//! @name      nufr_launch_task
// !
//! @brief     Goes into Task Descriptor block, finds stack and entry point
//! @brief     and puts task on ready list.
//!
//! @details   Task may either self-terminate by calling 'nufr_exit_running_task()',
//! @details   or simply return from the entry point.
//! @details
//! @details   Before launching, must place function pointer to
//! @details   'nufrplat_task_exit_point()' onto bottom of stack of task
//! @details   being launched. When task returns from entry point, it'll
//! @details   pop 'nufrplat_task_exit_point' into LR, and thereby
//! @details   jump to it.
//!
//! @param[in] 'task_id'-- task to be launched
//! @param[in] 'parameter'-- parameter to be passed to entry point of task
//!
void nufr_launch_task(nufr_tid_t task_id, unsigned parameter)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_tcb_t             *target_tcb;
    nufr_sr_reg_t           saved_psr;
    _IMPORT_stack_specifier_t stack_spec;
    const nufr_task_desc_t *desc;
    bool                    invoke;

    target_tcb = NUFR_TID_TO_TCB(task_id);

    //#####   Sanity checks
    KERNEL_REQUIRE_API(NUFR_IS_TCB(target_tcb) && (target_tcb != nufr_running));

    desc = nufrplat_task_get_desc(target_tcb, (nufr_tid_t)0);
    KERNEL_REQUIRE_API(NULL != desc);

    // Require that NUFR_TASK_NOT_LAUNCHED be set during init.
    if (NUFR_IS_STATUS_SET(target_tcb, NUFR_TASK_NOT_LAUNCHED))
    {
        return;
    }

    //####    Initialize TCB
    // Keep NUFR_TASK_NOT_LAUNCHED set to prevent
    //   another task from accidentally accessing target before it's ready.
    saved_psr = NUFR_LOCK_INTERRUPTS();

    rutils_memset(target_tcb, 0, sizeof(nufr_tcb_t));
    target_tcb->statuses = NUFR_TASK_NOT_LAUNCHED;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);


    //#####    Set other fields in TCB
    target_tcb->priority = desc->start_priority;

    //####     Prepare stack for usage, according to demands of each CPU
    KERNEL_REQUIRE(IS_ALIGNED32(desc->stack_base_ptr) ||
            (IS_ALIGNED16(desc->stack_base_ptr) &&  (sizeof(unsigned) == 2)));
    KERNEL_REQUIRE(IS_ALIGNED32(desc->stack_size));
    rutils_memset(desc->stack_base_ptr, 0, desc->stack_size);

    stack_spec.stack_base_ptr = desc->stack_base_ptr;
    stack_spec.stack_ptr_ptr = (_IMPORT_REGISTER_TYPE**)&(target_tcb->stack_ptr);
    stack_spec.stack_length_in_bytes = desc->stack_size;
    stack_spec.entry_point_fcn_ptr = desc->entry_point_fcn_ptr;
    stack_spec.exit_point_fcn_ptr = &nufrkernel_exit_running_task;
    stack_spec.entry_parameter = parameter;
    _IMPORT_PREPARE_STACK(&stack_spec);

    //#####    Launch time
    saved_psr = NUFR_LOCK_INTERRUPTS();

    target_tcb->statuses = 0;    // clears NUFR_TASK_NOT_LAUNCHED

#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST(target_tcb);
    invoke = macro_do_switch;
#else
    invoke = nufrkernel_add_task_to_ready_list(target_tcb);
#endif
    if (invoke)
    {
        NUFR_INVOKE_CONTEXT_SWITCH();
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();
        
}

//! @name      nufrkernel_exit_running_task
//
//! @brief     Under-the-covers exit routine. Gets called automatically.
//! @brief     Tasks need only return from their entry point call.
//!
void nufrkernel_exit_running_task(void)
{
    nufr_sr_reg_t           saved_psr;
#if NUFR_CS_MESSAGING == 1
    nufr_tid_t              target_tid;
#endif
#if NUFR_CS_SEMAPHORE == 1
    nufr_sema_block_t      *sema_block;
    bool                    blocked_on_sema;
    bool                    give_back_sema = false;
#endif

    // Can't call from BG
    KERNEL_REQUIRE(NUFR_IS_TCB(nufr_running));

#if NUFR_CS_SEMAPHORE == 1
    //###### Waiting on Sema?

    saved_psr = NUFR_LOCK_INTERRUPTS();

    sema_block = nufr_running->sema_block;
    if (sema_block != NULL)
    {
        blocked_on_sema = NUFR_IS_BLOCK_SET(nufr_running,
                                            NUFR_TASK_BLOCKED_SEMA);

        // If 'nufr_running->sema_block' has a value, and task
        //   is blocked on a sema, then we must be on a sema wait
        //   list. Unlink from it.
        // Else, we own the sema and should return it.
        // fixme: Verify that it's ok to delete the 'blocked_on_sema' code
        //        as it's not possible to be exiting a task while blocked
        //        on a sema.
        KERNEL_ENSURE_IL(!blocked_on_sema);
        UNUSED_BY_ASSERT(blocked_on_sema);
        //if (blocked_on_sema)
        //{
        //    nufrkernel_sema_unlink_task(sema_block, nufr_running);
        //
        //    nufr_running->sema_block = NULL;
        //}
        //else
        //{
            // Assume sema configured with priority inversion uses
            //   binary counts, and therefore only 1 task takes it
            //   at a time.
            give_back_sema = 
                 ANY_BITS_SET(sema_block->flags, NUFR_SEMA_PREVENT_PRI_INV);
        //}
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    //#### Need to return sema?

    if (give_back_sema)
    {
        nufr_sema_release(NUFR_SEMA_BLOCK_TO_ID(sema_block));
    }
#endif  //NUFR_CS_SEMAPHORE

#if NUFR_CS_MESSAGING == 1
    //#### Drain message queue
    target_tid = NUFR_TCB_TO_TID(nufr_running);
    nufr_msg_drain(target_tid, (nufr_msg_pri_t)0);
#endif  //NUFR_CS_MESSAGING     

    //#### Final: pull plug on self
    saved_psr = NUFR_LOCK_INTERRUPTS();

#if NUFR_CS_MESSAGING == 1
    // Post-drain messages: addresses corner-case where another task
    //     can send a msg before task is terminated
    nufr_msg_drain(target_tid, (nufr_msg_pri_t)0);
#endif  //NUFR_CS_MESSAGING     

#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_BLOCK_RUNNING_TASK(NUFR_TASK_NOT_LAUNCHED);
#else
    nufrkernel_block_running_task(NUFR_TASK_NOT_LAUNCHED);
#endif

    NUFR_INVOKE_CONTEXT_SWITCH();

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();
}

//! @name      nufr_kill_task
//
//! @brief     The means by which one task/the BG task kills
//! @brief     another task (target task).
//!
//! @details   Notes:
//! @details     -- App developer may want to add features
//! @details        to any target task. Features will
//! @details        have the task do some cleanup before it
//! @details        gets killed. This will work by having the
//! @details        target task receive a highest priority
//! @details        message, which will cause the cleanup logic
//! @details        to execute.
//! @details     -- Nufr API extensions have been added to allow
//! @details        a bop wait or a wait on sema to abort when
//! @details        a high priority message is received. This
//! @details        allows target task to abort wait so it
//! @details        can handle the cleanup message.
//! @details     -- Some of the things a task would have to self-clean
//! @details        before allowing itself to be killed:
//! @details        o Release semas
//! @details        o Purge any messages in inbox.
//! @details        o Return any message buffer to the pool
//! @details        o TBD: Cleanup at app layer (other memory pools,
//! @details          releasing control of drivers, etc)
//!
//! @details   Following must take place:
//! @details     -- If target task was not blocked,
//! @details        target task must be removed from the ready list.
//! @details     -- Target task's message queue must be
//! @details        drained.
//! @details     -- If target task is blocked on a sema,
//! @details        target task must be removed from the sema's task list.
//! @details        tcb->sema_blocked_on_ptr points to sema.
//! @details     -- If target task is blocked on an OS timer,
//! @details        target task must be purged from OS timer list.
//! @details     -- If target task was blocked, blocking bit must be cleared.
//! @details     -- Set target task's tcb->blocked : NUFR_TASK_NOT_LAUNCHED
//! @details     -- Invoke a context switch
//! @details     -- Next task or BG task switches in
//! @details
#if NUFR_CS_TASK_KILL == 1
void nufr_kill_task(nufr_tid_t task_id)
{
    nufr_tcb_t             *target_tcb;
    nufr_sr_reg_t           saved_psr;
#if NUFR_CS_SEMAPHORE == 1
    nufr_sema_block_t      *sema_block;
    bool                    give_back_sema = false;
#endif

    target_tcb = NUFR_TID_TO_TCB(task_id);

    saved_psr = NUFR_LOCK_INTERRUPTS();

    // Can't kill BG task
    KERNEL_REQUIRE_API(NUFR_IS_TCB(target_tcb));

    //#######    Kill API timeout/OS timer
    //####
    if (NUFR_IS_STATUS_SET(target_tcb, NUFR_TASK_TIMER_RUNNING))
    {
        nufrkernel_purge_from_timer_list(target_tcb);

        target_tcb->statuses &= BITWISE_NOT8(NUFR_TASK_TIMER_RUNNING);
    }

    if (NUFR_IS_TASK_BLOCKED(target_tcb))
    {
    #if NUFR_CS_SEMAPHORE == 1
    //#######    Remove target from a sema task wait list
    //####
        sema_block = target_tcb->sema_block;

        // If task is blocked on a sema, take it off that sema's wait list
        if (sema_block != 0)
        {
            if (NUFR_IS_BLOCK_SET(target_tcb, NUFR_TASK_BLOCKED_SEMA))
            {
            #if NUFR_CS_OPTIMIZATION_INLINES == 1
                NUFRKERNEL_SEMA_UNLINK_TASK(sema_block, target_tcb);
            #else
                nufrkernel_sema_unlink_task(sema_block, target_tcb);
            #endif

                target_tcb->sema_block = NULL;
                sema_block = NULL;
            }
            else
            {
                // Only do for binary semas; assume priority inversion goes
                //  with binary sema.
                give_back_sema =
                   ANY_BITS_SET(sema_block->flags, NUFR_SEMA_PREVENT_PRI_INV);
            }
        }

    #endif  //NUFR_CS_SEMAPHORE
    }
    else
    {
    //#######    Remove target from the ready list
    //####
    #if NUFR_CS_OPTIMIZATION_INLINES == 1
        NUFRKERNEL_DELETE_TASK_FROM_READY_LIST(target_tcb);
    #else
        nufrkernel_delete_task_from_ready_list(target_tcb);
    #endif
    }

    target_tcb->block_flags = NUFR_TASK_NOT_LAUNCHED;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    //#######    Task is now stopped

#if NUFR_CS_MESSAGING == 1
    //#######    Post cleanup: Drain Messages
    //####
    //  Can do this outside main locking block
    //    since owning msg blocks doesn't jeopardize system, short-term.
    nufr_msg_drain(task_id, (nufr_msg_pri_t)0);
#endif  //NUFR_CS_MESSAGING     

#if NUFR_CS_SEMAPHORE == 1
    //#######    Post cleanup: If target owned a sema, give it back
    //######
    if (give_back_sema)
    {
        nufr_sema_release(NUFR_SEMA_BLOCK_TO_ID(sema_block));
    }
#endif  //NUFR_CS_SEMAPHORE
}
#endif  //NUFR_CS_TASK_KILL

//! @name      nufr_self_tid
//
//! @brief     Returns task ID of currently running task
nufr_tid_t nufr_self_tid(void)
{
    // Can't call from BG task
    KERNEL_REQUIRE_API(NUFR_IS_TCB(nufr_running));

    return NUFR_TCB_TO_TID(nufr_running);
}

//! @name      nufr_task_running_state
//
//! @brief     Ascertain running or blocked state of a given task
//
//! @param[in] 'task_id'
//
//! @return    State enumerator
nufr_bkd_t nufr_task_running_state(nufr_tid_t task_id)
{
    nufr_tcb_t             *target_tcb;
    nufr_sr_reg_t           saved_psr;
    unsigned                block_flags;
    unsigned                statuses;
    nufr_bkd_t              rv;
    bool                    not_launched;
    bool                    asleep;
    bool                    bop_blocked;
    bool                    msg_blocked;
    bool                    sema_blocked;
    bool                    timeout;

    target_tcb = NUFR_TID_TO_TCB(task_id);

    // Can't status BG task
    KERNEL_REQUIRE_API(NUFR_IS_TCB(target_tcb));

    saved_psr = NUFR_LOCK_INTERRUPTS();

    block_flags = target_tcb->block_flags;
    statuses = target_tcb->statuses;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    not_launched = ANY_BITS_SET(block_flags, NUFR_TASK_NOT_LAUNCHED);
    asleep = ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_ASLEEP);
    bop_blocked = ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_BOP);
    msg_blocked = ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_MSG);
    sema_blocked = ANY_BITS_SET(block_flags, NUFR_TASK_BLOCKED_SEMA);
    timeout = ANY_BITS_SET(statuses, NUFR_TASK_TIMER_RUNNING);

    if (not_launched)                 rv = NUFR_BKD_NOT_LAUNCHED;
    else if (asleep)                  rv = NUFR_BKD_ASLEEP;
    else if (bop_blocked && timeout)  rv = NUFR_BKD_BOP_TOUT;
    else if (bop_blocked)             rv = NUFR_BKD_BOP;
    else if (msg_blocked && timeout)  rv = NUFR_BKD_MSG_TOUT;
    else if (msg_blocked)             rv = NUFR_BKD_MSG;
    else if (sema_blocked && timeout) rv = NUFR_BKD_SEMA_TOUT;
    else if (sema_blocked)            rv = NUFR_BKD_SEMA;
    else                              rv = NUFR_BKD_READY;

    return rv;
}

//! @name      nufr_sleep
//!
//! @brief     Cause currently running task to sleep for so many OS clock ticks
//!
//! @details   Cannot be called from an ISR or from BG task
//! @details
//!
//! @param[in] sleep_delay_in_ticks-- sleep interval, in OS clock ticks
//! @param[in]              recommend wrapping 'sleepDelayInTicks' parm with
//! @param[in]              NUFR_MILLISECS_TO_TICKS or NUFR_SECS_TO_TICKS
//! @param[in] abort_priority_of_rx_msg-- priority where, lower than that,
//! @param[in]        message rx will abort sleep.
//!
//! @return    'true' if aborted by message send
bool nufr_sleep(unsigned       sleep_delay_in_ticks,
                nufr_msg_pri_t abort_priority_of_rx_msg)
{
    nufr_sr_reg_t    saved_psr;
#if NUFR_CS_TASK_KILL == 1
    unsigned           notifications;
#endif  //NUFR_CS_TASK_KILL

    // Can't be called from BG task
    KERNEL_REQUIRE_API(nufr_running != (nufr_tcb_t *)nufr_bg_sp);

    if (0 == sleep_delay_in_ticks)
    {
        return false;
    }

    //#####    First: sleep
    saved_psr = NUFR_LOCK_INTERRUPTS();

    // Clear notifications, as they'll be bitwise set elsewhere
    nufr_running->notifications = 0;
#if NUFR_CS_TASK_KILL == 1
    nufr_running->abort_message_priority = abort_priority_of_rx_msg;
#else
    UNUSED(abort_priority_of_rx_msg);
#endif  //NUFR_CS_TASK_KILL

    nufrkernel_add_to_timer_list(nufr_running, sleep_delay_in_ticks);

#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_BLOCK_RUNNING_TASK(NUFR_TASK_BLOCKED_ASLEEP);
#else
    nufrkernel_block_running_task(NUFR_TASK_BLOCKED_ASLEEP);
#endif

    NUFR_INVOKE_CONTEXT_SWITCH();

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    //#####    Second: After awakening, determine wakeup reason
    //###              Kill zombie timer.
    //   For a sleep API, zombie timer can only happen due to
    //      a message abort
#if NUFR_CS_TASK_KILL == 1
    saved_psr = NUFR_LOCK_INTERRUPTS();

    notifications = nufr_running->notifications;

    // Zombie timer?
    if (NUFR_IS_STATUS_SET(nufr_running, NUFR_TASK_TIMER_RUNNING))
    {
        // Require that zombie timer caused by msg send abort
        KERNEL_REQUIRE_IL(ANY_BITS_SET(notifications, NUFR_TASK_UNBLOCKED_BY_MSG_SEND));

        nufrkernel_purge_from_timer_list(nufr_running);
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);
#endif  //NUFR_CS_TASK_KILL

    KERNEL_ENSURE(nufr_running == nufr_ready_list);

#if NUFR_CS_TASK_KILL == 1
    return ANY_BITS_SET(notifications, NUFR_TASK_UNBLOCKED_BY_MSG_SEND);
#else
    return false;
#endif  //NUFR_CS_TASK_KILL
}

//! @name      nufr_yield
//
//! @brief     If other tasks of the same task priority are ready, current
//! @brief     running task lets them run; otherwise, no change
//
//! @details   Cannot be called from an ISR or from BG task
//! @details
//!
//! @return    'true' if another task was waiting/context switch happened
bool nufr_yield(void)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_sr_reg_t       saved_psr;
    nufr_tcb_t         *old_head_tcb;
    bool                invoke = false;

    // Can't be called from BG task
    KERNEL_REQUIRE_API(nufr_running != (nufr_tcb_t *)nufr_bg_sp);

    saved_psr = NUFR_LOCK_INTERRUPTS();

    // Any task on ready list after this one?
    if (NULL != nufr_running->flink)
    {
        old_head_tcb = nufr_ready_list;

        // Is the next task at the same priority as ours?
        if (nufr_running->flink->priority == nufr_running->priority)
        { 
        #if NUFR_CS_OPTIMIZATION_INLINES == 1
            NUFRKERNEL_REMOVE_HEAD_TASK_FROM_READY_LIST();

            NUFRKERNEL_ADD_TASK_TO_READY_LIST(nufr_running);
            UNUSED(macro_do_switch);                      // suppress warning
        #else
            nufrkernel_remove_head_task_from_ready_list();

            (void)nufrkernel_add_task_to_ready_list(nufr_running);
        #endif

            invoke = (nufr_ready_list != old_head_tcb);
            if (invoke)
            {
                NUFR_INVOKE_CONTEXT_SWITCH();
            }
        }
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    KERNEL_ENSURE(nufr_running == nufr_ready_list);

    return invoke;
}

//! @name      nufr_prioritize
//
//! @brief     Sets current running task to a priority (NUFR_TPR_guaranteed_highest)
//! @brief     higher than any other task's. Saves off old priority, for restoration
//! @brief     during nufr_unprioritize()
//
//! @details   Cannot be called from an ISR or from BG task
//! @details
//!
void nufr_prioritize(void)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_sr_reg_t saved_psr;

    // Can't be called from BG task
    KERNEL_REQUIRE_API(nufr_running != (nufr_tcb_t *)nufr_bg_sp);

    saved_psr = NUFR_LOCK_INTERRUPTS();

    // We must do a ready list remove and insert, instead
    // of just poking in new priority value. Although the
    // order of the ready list might not change,
    //  'nufr_ready_list_tail_nominal' might change.
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_REMOVE_HEAD_TASK_FROM_READY_LIST();
#else
    nufrkernel_remove_head_task_from_ready_list();
#endif

    nufr_running->priority_restore_prioritized = nufr_running->priority;
    nufr_running->priority = NUFR_TPR_guaranteed_highest;

#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST(nufr_running);
    UNUSED(macro_do_switch);                      // suppress warning
#else
    (void)nufrkernel_add_task_to_ready_list(nufr_running);
#endif

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    KERNEL_ENSURE(nufr_running == nufr_ready_list);
}

//! @name      nufr_unprioritize
//
//! @brief     Restores current running task back to its old priority, the
//! @brief     one it had before it was changed by nufr_prioritize()
//! @brief     during nufr_unprioritize()
//
//! @details   Cannot be called from an ISR or from BG task
//! @details
void nufr_unprioritize(void)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_sr_reg_t       saved_psr;
    nufr_tcb_t         *old_head_tcb;
    nufr_tcb_t         *next_tcb;
    uint8_t             restore_priority;

    // Can't be called from BG task
    // Assume nufr_prioritize was called first
    //KERNEL_REQUIRE(nufr_running != (nufr_tcb_t *)nufr_bg_sp);

    saved_psr = NUFR_LOCK_INTERRUPTS();

    restore_priority = nufr_running->priority_restore_prioritized;

    next_tcb = nufr_ready_list->flink;
    // Is there another behind this one, waiting for the CPU?
    if (NULL != next_tcb)
    {
        // Is the next ready task a higher priority than what prioritized
        // task will be restored to? Then we need to move prioritized
        // task back somewhere in ready list when it's restored.
        if (restore_priority > next_tcb->priority)
        {
            old_head_tcb = nufr_ready_list;

        #if NUFR_CS_OPTIMIZATION_INLINES == 1
            NUFRKERNEL_REMOVE_HEAD_TASK_FROM_READY_LIST();
        #else
            nufrkernel_remove_head_task_from_ready_list();
        #endif

            nufr_running->priority = restore_priority;

        #if NUFR_CS_OPTIMIZATION_INLINES == 1
            NUFRKERNEL_ADD_TASK_TO_READY_LIST(nufr_running);
            UNUSED(macro_do_switch);                      // suppress warning
        #else
            (void)nufrkernel_add_task_to_ready_list(nufr_running);
        #endif

            if (nufr_ready_list != old_head_tcb)
            {
                NUFR_INVOKE_CONTEXT_SWITCH();
            }
        }
        // No, other waiting tasks won't preempt prioritized task
        // when it's restored. Safe to just restore priority.
        else
        {
            nufr_running->priority = restore_priority;
        }
    }
    // No other ready tasks. Just restore priority.
    else
    {
        nufr_running->priority = restore_priority;
    }
        
    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    KERNEL_ENSURE(nufr_running == nufr_ready_list);
}

//! @name      nufr_change_task_priority
//!
//! @brief     Changes task priority of task specified by 'tid' to new
//! @brief     priority 'new_priority'
//!
//! @param[in] 'tid'--
//! @param[in] 'new_priority'--
void nufr_change_task_priority(nufr_tid_t tid, unsigned new_priority)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_tcb_t         *tcb;
    nufr_tcb_t         *old_head_tcb;
    nufr_sr_reg_t       saved_psr;
    bool                is_task_launched;
    bool                doing_another_task;

    tcb = NUFR_TID_TO_TCB(tid);

    // Sanity check tid
    if ( !NUFR_IS_TCB(tcb) )
    {
        KERNEL_REQUIRE_API(false);
        return;
    }
    // Sanity check 'new_priority':
    //   #1 Do not allow change to 'NUFR_TPR_guaranteed_highest' or higher
    //      --that's reserved + illegal
    //   #2 Don't allow priority beyong uint8_t size
    else if ( (new_priority <= NUFR_TPR_guaranteed_highest)
                              ||
              (new_priority > (unsigned)BIT_MASK8))
    {
        KERNEL_REQUIRE_API(false);
        return;
    }

    saved_psr = NUFR_LOCK_INTERRUPTS();

    // Sanity check task state
    is_task_launched = NUFR_IS_TASK_LAUNCHED(tcb);
    KERNEL_REQUIRE_IL(is_task_launched);
    if (is_task_launched)
    {
        old_head_tcb = nufr_ready_list;

        doing_another_task = tcb != nufr_running;

        // Changing priority of task which is blocked?
        // No ready list shuffling needed, as task isn't on ready list.
        if (NUFR_IS_TASK_BLOCKED(tcb))
        {
            tcb->priority = new_priority;
        }
        // Else, we're changing our own/current running task's priority,
        //   or we're changing another task's which isn't blocked.
        // In both cases, we're changing the priority of a task on the
        //   ready list.
        else
        {
            // We must do a ready list remove and insert, instead
            // of just poking in new priority value. Although the
            // order of the ready list might not change,
            //  'nufr_ready_list_tail_nominal' might change.

            if (doing_another_task)
            {
            #if NUFR_CS_OPTIMIZATION_INLINES == 1
                NUFRKERNEL_DELETE_TASK_FROM_READY_LIST(tcb);
            #else
                nufrkernel_delete_task_from_ready_list(tcb);
            #endif
            }
            else
            {
            #if NUFR_CS_OPTIMIZATION_INLINES == 1
                NUFRKERNEL_REMOVE_HEAD_TASK_FROM_READY_LIST();
            #else
                nufrkernel_remove_head_task_from_ready_list();
            #endif
            }

            tcb->priority = (uint8_t)new_priority;

            #if NUFR_CS_OPTIMIZATION_INLINES == 1
                NUFRKERNEL_ADD_TASK_TO_READY_LIST(tcb);
                UNUSED(macro_do_switch);                // suppress warning
            #else
                (void)nufrkernel_add_task_to_ready_list(tcb);
            #endif
        }

        // Did any of above necessitate a context switch?
        if (nufr_ready_list != old_head_tcb)
        {
            NUFR_INVOKE_CONTEXT_SWITCH();
        }
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();
}

//! @name      nufr_bop_get_key
//!
//! @brief     Generate a key that gets saved in the calling task's TCB,
//! @brief     and calling task will send to another task. The other task
//! @brief     must use this key when sending a bop, or else the first
//! @brief     task won't take the bop.
//!
//! @details   Increment 'nufr_bop_key', return value.
//! @details   When 'nufr_bop_key' wraps, increment to 1, so 0 isn't used
//!
//! @return    new key
uint16_t nufr_bop_get_key(void)
{
    nufr_sr_reg_t   saved_psr;
    uint16_t        key;

    // BG task just fetches last key
    if (nufr_running == (nufr_tcb_t *)nufr_bg_sp)
    {
        return nufr_bop_key;
    }

    saved_psr = NUFR_LOCK_INTERRUPTS();

    key = nufr_bop_key + 1;
    if (key == 0)
    {
        key++;
    }

    nufr_bop_key = key;

    nufr_running->bop_key = key;

    // Clear pre-arrived, in case leftover from last key
//    nufr_running->statuses &= BITWISE_NOT8(NUFR_TASK_BOP_PRE_ARRIVED);

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    return key;
}

//!
//! @name      nufr_bop_waitW
//!
//! @brief     Current task blocks waiting to receive a bop from another task.
//!
//! @details   Acts like a non-counting, bound-to-a-single-task semaphore.
//! @details   The waiting task waits for a bop send from another task.
//!
//! @details   The other task must provide a key with the bop send, unless
//! @details   it does a bop send with a key override.
//!
//! @details   Cannot be called from an ISR or from BG task
//! @details
//!
//! @param[in] 'abort_priority_of_rx_msg'--  If a message that's of a priority
//! @param[in]    greater than 'abort_priority_of_rx_msg' is sent to the
//! @param[in]    waiting task's message queue, the bop wait is aborted
//! @param[in]    (implies that 'abort_priority_of_rx_msg'==0 is equivalent
//! @param[in]    to ignore parameter).
//! @param[in]    NOTE: assumes compile-time switches enabled:
//! @param[in]       NUFR_CS_MESSAGING, NUFR_CS_TASK_KILL, 
//
//! @return    reason code for ending wait on bop (timeout, naturally,
//! @return      does not apply)
nufr_bop_wait_rtn_t nufr_bop_waitW(nufr_msg_pri_t abort_priority_of_rx_msg)
{
    nufr_sr_reg_t       saved_psr;
//    bool                pre_arrived;
#if NUFR_CS_TASK_KILL == 1
    bool                aborted;
#endif  //NUFR_CS_TASK_KILL
    nufr_bop_wait_rtn_t return_value;

    // Can't be called from BG task
    KERNEL_REQUIRE_API(nufr_running != (nufr_tcb_t *)nufr_bg_sp);
    KERNEL_REQUIRE_API(abort_priority_of_rx_msg < NUFR_CS_MSG_PRIORITIES);

    saved_psr = NUFR_LOCK_INTERRUPTS();

//    pre_arrived = NUFR_IS_STATUS_SET(nufr_running, NUFR_TASK_BOP_PRE_ARRIVED);
//    nufr_running->statuses &= BITWISE_NOT8(NUFR_TASK_BOP_PRE_ARRIVED);

//    if (!pre_arrived)
    {
        //#####    First: block on bop

        // Clear notifications, as they'll be bitwise set elsewhere
        nufr_running->notifications = 0;

    #if NUFR_CS_TASK_KILL == 1
        nufr_running->abort_message_priority = abort_priority_of_rx_msg;
    #else
        UNUSED(abort_priority_of_rx_msg);
    #endif  //NUFR_CS_TASK_KILL

    #if NUFR_CS_OPTIMIZATION_INLINES == 1
        NUFRKERNEL_BLOCK_RUNNING_TASK(NUFR_TASK_BLOCKED_BOP);
    #else
        nufrkernel_block_running_task(NUFR_TASK_BLOCKED_BOP);
    #endif

        NUFR_INVOKE_CONTEXT_SWITCH();
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    //###### Second: task is now unblock from bop,
    //######         ascertain return value.
#if NUFR_CS_TASK_KILL == 1
    aborted = abort_priority_of_rx_msg > 0;
    if (aborted)
    {
        saved_psr = NUFR_LOCK_INTERRUPTS();

        aborted = NUFR_IS_NOTIF_SET(nufr_running, NUFR_TASK_UNBLOCKED_BY_MSG_SEND);

        NUFR_UNLOCK_INTERRUPTS(saved_psr);
    }

    if (aborted)
    {
        return_value = NUFR_BOP_WAIT_ABORTED_BY_MESSAGE;
    }
    else
    {
        return_value = NUFR_BOP_WAIT_OK;
    }
#else
    return_value = NUFR_BOP_WAIT_OK;
#endif  //NUFR_CS_TASK_KILL
        
    KERNEL_ENSURE(nufr_running == nufr_ready_list);

    return return_value;
}

//! @name      nufr_bop_waitT
//!
//! @brief     Same as nufr_bop_waitW, but with a timeout
//!
//! @details   Cannot be called from an ISR or from BG task
//! @details
//! @details   A zero timeout can only be used with a bop pre-arrival.
//! @details   Otherwised, zero timeout will return NUFR_BOP_WAIT_INVALID.
//!
//! @param[in] 'abort_priority_of_rx_msg'--  If a message that's of a priority
//! @param[in]     greater than 'abort_priority_of_rx_msg' is sent to the
//! @param[in]     waiting task's message queue, the bop wait is aborted
//! @param[in]     NOTE: assumes compile-time switch support
//!
//! @param[in] 'timeoutTicks'-- timeout if no bop received. Timeout causes
//! @param[in]   task to unblock
//!
//! @return    reason code for ending wait on bop
nufr_bop_wait_rtn_t nufr_bop_waitT(nufr_msg_pri_t abort_priority_of_rx_msg,
                                   unsigned       timeout_ticks)
{
    nufr_sr_reg_t       saved_psr;
//    bool                pre_arrived;
#if NUFR_CS_TASK_KILL == 1
    bool                aborted;
#endif  //NUFR_CS_TASK_KILL
    bool                zero_timeout;
    bool                timeout;
    unsigned            notifications;
    nufr_bop_wait_rtn_t return_value;

    // Can't be called from BG task
    KERNEL_REQUIRE_API(nufr_running != (nufr_tcb_t *)nufr_bg_sp);
    KERNEL_REQUIRE_API(abort_priority_of_rx_msg < NUFR_CS_MSG_PRIORITIES);

    zero_timeout = timeout_ticks == 0;

    saved_psr = NUFR_LOCK_INTERRUPTS();

//    pre_arrived = NUFR_IS_STATUS_SET(nufr_running, NUFR_TASK_BOP_PRE_ARRIVED);
//    nufr_running->statuses &= BITWISE_NOT8(NUFR_TASK_BOP_PRE_ARRIVED);

    if (!(/*pre_arrived ||*/ zero_timeout))
    {
    //#####    First: block on bop
        // Clear notifications, as they'll be bitwise set before task awakens
        nufr_running->notifications = 0;

    #if NUFR_CS_TASK_KILL == 1
        nufr_running->abort_message_priority = abort_priority_of_rx_msg;
    #else
        UNUSED(abort_priority_of_rx_msg);
    #endif  //NUFR_CS_TASK_KILL

        nufrkernel_add_to_timer_list(nufr_running, timeout_ticks);

    #if NUFR_CS_OPTIMIZATION_INLINES == 1
        NUFRKERNEL_BLOCK_RUNNING_TASK(NUFR_TASK_BLOCKED_BOP);
    #else
        nufrkernel_block_running_task(NUFR_TASK_BLOCKED_BOP);
    #endif

        NUFR_INVOKE_CONTEXT_SWITCH();
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    //###### Second: verify that timeout was valid
    //######        Only valid zero timeout is a pre-arrival condition.
    if (/*!pre_arrived &&*/ zero_timeout)
    {
        return NUFR_BOP_WAIT_INVALID;
    }

    //###### Third: task is now unblocked from bop,
    //######        ascertain return value.
    saved_psr = NUFR_LOCK_INTERRUPTS();

    notifications = nufr_running->notifications;
    timeout = ANY_BITS_SET(notifications, NUFR_TASK_TIMEOUT);

#if NUFR_CS_TASK_KILL == 1
    aborted = abort_priority_of_rx_msg > 0;
    if (aborted)
    {
        aborted = ANY_BITS_SET(notifications, NUFR_TASK_UNBLOCKED_BY_MSG_SEND);
    }
#endif  //NUFR_CS_TASK_KILL

    //###### Fourth: Check for zombie timer and clean up.
    //######         Zombie timer occurs when bop sent from ISR.

    if (NUFR_IS_STATUS_SET(nufr_running, NUFR_TASK_TIMER_RUNNING))
    {
        (void)nufrkernel_purge_from_timer_list(nufr_running);
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    if (timeout)
    {
        return_value = NUFR_BOP_WAIT_TIMEOUT;
    }
#if NUFR_CS_TASK_KILL == 1
    else if (aborted)
    {
        return_value = NUFR_BOP_WAIT_ABORTED_BY_MESSAGE;
    }
#endif  //NUFR_CS_TASK_KILL
    else
    {
        return_value = NUFR_BOP_WAIT_OK;
    }
        
    KERNEL_ENSURE(nufr_running == nufr_ready_list);

    return return_value;
}

//! @name      nufr_bop_send
//!
//! @brief     Sends a bop to a task waiting on a bop.
//! @brief     Kernel verifies that sender's key matches receiver's key.
//!
//! @param[in] 'task_id'-- which task receives bop
//! @param[in] 'key'-- 16-bit key that must match one stored in receiver's TCB
//
//! @return    return codes:
//! @return      NUFR_BOP_RTN_TASK_NOT_WAITING
//! @return      NUFR_BOP_RTN_KEY_MISMATCH
//! @return      NUFR_BOP_RTN_TAKEN
nufr_bop_rtn_t nufr_bop_send(nufr_tid_t task_id, uint16_t key)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_sr_reg_t       saved_psr;
    nufr_tcb_t         *target_tcb;
    bool                key_match;
    bool                is_blocked = false;
    //bool                is_timer_running;
    bool                invoke;
    nufr_bop_rtn_t      return_value;

    target_tcb = NUFR_TID_TO_TCB(task_id);

    // Sanity check TCB
    KERNEL_REQUIRE_API(NUFR_IS_TCB(target_tcb) && (target_tcb != nufr_running));

    saved_psr = NUFR_LOCK_INTERRUPTS();

    key_match = target_tcb->bop_key == key;

    if (key_match)
    {
        is_blocked = NUFR_IS_BLOCK_SET(target_tcb, NUFR_TASK_BLOCKED_BOP);

        if (is_blocked)
        {
            target_tcb->block_flags = 0;

            // If there was a timer, it's now a zombie timer
            //  it'll get removed when nufr_bop_waitT exits

        #if NUFR_CS_OPTIMIZATION_INLINES == 1
            NUFRKERNEL_ADD_TASK_TO_READY_LIST(target_tcb);
            invoke = macro_do_switch;
        #else
            invoke = nufrkernel_add_task_to_ready_list(target_tcb);
        #endif
            
            if (invoke)
            {
                NUFR_INVOKE_CONTEXT_SWITCH();
            }
        }
        // corner case: target got key but got interrupted before waiting
        //    on bop
        // nufr 1.0+ : not needed, use nsvc_msg_send_and_bop_waitW() to prevent
        //             this corner case. Keeping commented out just in case.
//        else
//        {
//            KERNEL_REQUIRE_IL(NUFR_IS_STATUS_CLR(target_tcb, NUFR_TASK_BOP_PRE_ARRIVED));
//
//            target_tcb->statuses |= NUFR_TASK_BOP_PRE_ARRIVED;
//        }
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    if (!key_match)
    {
        return_value = NUFR_BOP_RTN_KEY_MISMATCH;
    }
    else if (!is_blocked)
    {
        return_value = NUFR_BOP_RTN_TASK_NOT_WAITING;
    }
    else
    {
        return_value = NUFR_BOP_RTN_TAKEN;
    }

    KERNEL_ENSURE(nufr_running == nufr_ready_list);

    return return_value;
}

//! @name      nufr_bop_send_with_key_override
//
//! @brief     Sends a bop to a task waiting on a bop.
//! @brief     Skips key verification
//
//! @param[in] 'task_id'-- which task receives bop
//
//! @return    return codes:
//! @return      NUFR_BOP_RTN_TASK_NOT_WAITING
//! @return      NUFR_BOP_RTN_TAKEN
nufr_bop_rtn_t nufr_bop_send_with_key_override(nufr_tid_t task_id)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_sr_reg_t       saved_psr;
    nufr_tcb_t         *target_tcb;
    bool                is_blocked;
    //bool                is_timer_running;
    bool                invoke;
    nufr_bop_rtn_t      return_value;

    target_tcb = NUFR_TID_TO_TCB(task_id);

    // Sanity check TCB
    KERNEL_REQUIRE_API(NUFR_IS_TCB(target_tcb) && (target_tcb != nufr_running));

    saved_psr = NUFR_LOCK_INTERRUPTS();

    is_blocked = NUFR_IS_BLOCK_SET(target_tcb, NUFR_TASK_BLOCKED_BOP);

    if (is_blocked)
    {
        target_tcb->block_flags = 0;

        // Zombie timer, see above

    #if NUFR_CS_OPTIMIZATION_INLINES == 1
        NUFRKERNEL_ADD_TASK_TO_READY_LIST(target_tcb);
        invoke = macro_do_switch;
    #else
        invoke = nufrkernel_add_task_to_ready_list(target_tcb);
    #endif
            
        if (invoke)
        {
            NUFR_INVOKE_CONTEXT_SWITCH();
        }
    }

    // Keyless call does not set NUFR_TASK_BOP_PRE_ARRIVED;
    //   target must be waiting on bop

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();

    if (!is_blocked)
    {
        return_value = NUFR_BOP_RTN_TASK_NOT_WAITING;
    }
    else
    {
        return_value = NUFR_BOP_RTN_TAKEN;
    }

    KERNEL_ENSURE(nufr_running == nufr_ready_list);

    return return_value;
}

//! @name      nufr_bop_lock_waiter
//!
//! @brief     If another task is blocked on a bop, lock that task
//! @brief     so a timeout or high priority message rx won't unblock it.
//!
//! @details
//!
//! @param[in] 'task_id'-- which task receives bop
//! @param[in] 'key'-- 16-bit key that must match one stored in receiver's TCB
//
//! @return    return code
#if NUFR_CS_LOCAL_STRUCT == 1
nufr_bop_rtn_t nufr_bop_lock_waiter(nufr_tid_t task_id, uint16_t key)
{
    nufr_sr_reg_t       saved_psr;
    nufr_tcb_t         *target_tcb;
    bool                is_bop_blocked;
    bool                key_match;
    nufr_bop_rtn_t      return_value;

    target_tcb = NUFR_TID_TO_TCB(task_id);

    // Sanity check TCB
    KERNEL_REQUIRE_API(NUFR_IS_TCB(target_tcb) && (target_tcb != nufr_running));

    saved_psr = NUFR_LOCK_INTERRUPTS();

    is_bop_blocked = NUFR_IS_BLOCK_SET(target_tcb, NUFR_TASK_BLOCKED_BOP);
    key_match = target_tcb->bop_key == key;

    if (is_bop_blocked && key_match)
    {
        target_tcb->statuses |= NUFR_TASK_BOP_LOCKED;
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    if (!is_bop_blocked)
    {
        return_value = NUFR_BOP_RTN_TASK_NOT_WAITING;
    }
    else if (!key_match)
    {
        return_value = NUFR_BOP_RTN_KEY_MISMATCH;
    }
    else
    {
        return_value = NUFR_BOP_RTN_TAKEN;
    }

    return return_value;
}
#endif  //NUFR_CS_LOCAL_STRUCT

//! @name      nufr_bop_unlock_waiter
//!
//! @brief     Removes lock set by nufr_bop_lock_waiter
//!
//! @details
//!
//! @param[in] 'task_id'-- which task receives bop
//!
#if NUFR_CS_LOCAL_STRUCT == 1
void nufr_bop_unlock_waiter(nufr_tid_t task_id)
{
#if NUFR_CS_OPTIMIZATION_INLINES == 1
    NUFRKERNEL_ADD_TASK_TO_READY_LIST_DECLARATIONS;
#endif  // NUFR_CS_OPTIMIZATION_INLINES == 1
    nufr_sr_reg_t       saved_psr;
    nufr_tcb_t         *target_tcb;
    bool                is_bop_locked;
    bool                invoke;

    target_tcb = NUFR_TID_TO_TCB(task_id);

    // Sanity check TCB
    KERNEL_REQUIRE_API(NUFR_IS_TCB(target_tcb) && (target_tcb != nufr_running));

    saved_psr = NUFR_LOCK_INTERRUPTS();

    is_bop_locked = NUFR_IS_STATUS_SET(target_tcb, NUFR_TASK_BOP_LOCKED);
    if (is_bop_locked)
    {
        target_tcb->statuses &= BITWISE_NOT8(NUFR_TASK_BOP_LOCKED);

        // Task was blocked on a bop when it was locked. If abort
        // message send unblocked it, then status will have changed
        // to unblocked, awaiting this API to put it on ready list.
        if (NUFR_IS_BLOCK_CLR(target_tcb, NUFR_TASK_BLOCKED_BOP))
        {
        #if NUFR_CS_OPTIMIZATION_INLINES == 1
            NUFRKERNEL_ADD_TASK_TO_READY_LIST(target_tcb);
            invoke = macro_do_switch;
        #else
            invoke = nufrkernel_add_task_to_ready_list(target_tcb);
        #endif
            if (invoke)
            {
                NUFR_INVOKE_CONTEXT_SWITCH();
            }
        }
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    NUFR_SECONDARY_CONTEXT_SWITCH();
}
#endif  //NUFR_CS_LOCAL_STRUCT

//! @name      nufr_local_struct_set
//
//! @brief     Copies a pointer into the running task's TCB.
//! @brief     Pointer points to variable or struct on running task's stack.
//
//! @details   NOTE: must be compiled in (NUFR_CS_LOCAL_STRUCT)
//
//! @param[in] 'local_struct_ptr'-- pointer to variable or struct
#if NUFR_CS_LOCAL_STRUCT == 1
void nufr_local_struct_set(void *local_struct_ptr)
{
    nufr_sr_reg_t     saved_psr;

    // Can't be called from BG task
    KERNEL_REQUIRE_API(nufr_running != (nufr_tcb_t *)nufr_bg_sp);

    saved_psr = NUFR_LOCK_INTERRUPTS();

    nufr_running->local_struct_ptr = local_struct_ptr;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);
}
#endif  //NUFR_CS_LOCAL_STRUCT

//! @name      nufr_local_struct_get
//
//! @brief     Retrieves the pointer stored in a task's TCB.
//! @brief     Pointer was set by a call to 'nufr_local_struct_set'
//
//! @detail    The contract is that the task which establishes a local
//! @detail    struct is blocked while another task uses it.
//! @details   Intention is to use this with keyed bops
//
//! @details   NOTE: must be compiled in (NUFR_CS_LOCAL_STRUCT)
//
//! @param[in] 'task_id'--
//
//! @return    returns value
#if NUFR_CS_LOCAL_STRUCT == 1
void *nufr_local_struct_get(nufr_tid_t task_id)
{
    nufr_sr_reg_t       saved_psr;
    nufr_tcb_t         *target_tcb;
    void               *local_struct_ptr;

    target_tcb = NUFR_TID_TO_TCB(task_id);

    // Sanity check TCB
    KERNEL_REQUIRE_API(NUFR_IS_TCB(target_tcb) && (target_tcb != nufr_running));

    saved_psr = NUFR_LOCK_INTERRUPTS();

    local_struct_ptr = target_tcb->local_struct_ptr;

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    return local_struct_ptr;
}
#endif  //NUFR_CS_LOCAL_STRUCT
