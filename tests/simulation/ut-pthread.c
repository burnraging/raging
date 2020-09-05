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

//! @file    ut-pthread.c
//! @authors Bernie Woodland
//! @date    19Oct17

#include "nufr-global.h"
#include "nufr-api.h"
#include "nufr-platform-app.h"
#include "nufr-platform.h"
#include "nsvc-app.h"
#include "nsvc-api.h"
#include "nufr-kernel-task.h"
#include "nufr-kernel-message-blocks.h"

#include "raging-contract.h"

#include <semaphore.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#ifdef VISUAL_C_COMPILER
//    #include "stdafx.h"
    #include <windows.h>
    #include <time.h>
#else
    #include <time.h>
    #include <string.h>
    #include <semaphore.h>
    #include <errno.h>
#endif

typedef enum
{
    TEST_SLEEP_AND_SEND,
    TEST_MSG_CIRCLE,
    TEST_BOPS,
    TEST_BOP_FEATURES,
    TEST_PRIORITY_INVERSION,
    TEST_ZOMBIE_TIMERS,
    TEST_API_TIMEOUTS,
    TEST_MISC
} test_t;

//test_t           current_test = TEST_SLEEP_AND_SEND;
test_t           current_test = TEST_MSG_CIRCLE;
//test_t           current_test = TEST_BOPS;
//test_t           current_test = TEST_BOP_FEATURES;
//test_t           current_test = TEST_PRIORITY_INVERSION;
//test_t           current_test = TEST_ZOMBIE_TIMERS;
//test_t           current_test = TEST_API_TIMEOUTS;
//test_t           current_test = TEST_MISC;


typedef enum
{
    MSG_ID_1 = 1,
    MSG_ID_2,
    MSG_ID_CIRCLE,
} msg_id_t;

extern sem_t     nufr_sim_os_tick_sem;
extern sem_t     nufr_sim_bg_sem;

uint16_t         bops_key1;
uint16_t         bops_key2;
uint16_t         bops_key3;

// Temp flag. Prevents thread collisions. See notes about VC2010 debugging
// with sema timeouts.
bool             disable_systick;
bool             systick_active;

unsigned         msg_rx_count;

//                        Tasks
//                        =====
//
//    NUFR_TID_01     entry_01     NUFR_TPR_HIGHEST
//    NUFR_TID_02     entry_02     NUFR_TPR_HIGHEST
//    NUFR_TID_03     entry_03     NUFR_TPR_HIGHER

//***
//***** TEST_SLEEP_AND_SEND
//***
//
// Exercises:
//  -- 2 tasks on timer list at same time
//  -- nufr_sleep() for 2 tasks at same time
//  -- timeout of nufr_sleep()
//  -- nsvc_msg_send_structW() sending of message
//  -- the nsvc message bpool message pooling

void test_sleep_and_send_entry_01(void)
{
    nufr_launch_task(NUFR_TID_02, 0);
    nufr_launch_task(NUFR_TID_03, 0);

    while (1)
    {
        nufr_sleep(2, 0);
    }
}

void test_sleep_and_send_entry_02(void)
{
    nsvc_msg_send_return_t  send_status;
    nsvc_msg_fields_unary_t msg_fields;

    while (1)
    {
        nufr_sleep(2, 0);

        msg_fields.destination_task = NUFR_TID_03;
        msg_fields.priority = NUFR_MSG_PRI_MID;
        msg_fields.prefix = NSVC_MSG_PREFIX_local;
        msg_fields.id = MSG_ID_1;
        msg_fields.optional_parameter = 5;

        send_status = nsvc_msg_send_structW(&msg_fields);
        UNUSED(send_status);
    }
}

// fixme for this test
void test_sleep_and_send_entry_03(void)
{
    nufr_msg_t     *msg;
    msg_id_t        id;

    while (1)
    {
        msg = nufr_msg_get_block();
        UT_REQUIRE(NULL != msg);

        id = (msg_id_t)NUFR_GET_MSG_ID(msg->fields);

        nufr_msg_free_block(msg);

        switch (id)
        {
        case MSG_ID_1:
            msg_rx_count++;
            break;
        default:
            break;
        }
    }
}

//***
//***** TEST_MSG_CIRCLE
//***

// fixme...description not quite right
// - Task 3 builds 4 messages, then sends them all at once to task 2
// - Task 2 receives the messages in message priority order, which
//   is not the order in which they were sent
// - Task 2 sends 4 messages to Task 1
// - Each time Task 2 sends a message, Task 1 awakens, being at
//   a higher priority, and processes the message.
//
// Exercises:
// - the different combinations of SL message sends and gets
// - messages are sent in one order, received by message priority,
//    not order sent in
// - message optional parameter verified
// - message priority intact after send
// - message sending task verified
// - do-nothing nufr_yield() 
// - nufr_yield() which yields to another task
// - context switching from task sending message to a task
//    running at a higher priority
// - no context switch when task sends message to task at same
//    priority
// - message send return values ok
// 
void test_msg_circle_entry_01(void)
{
    nsvc_msg_fields_unary_t msg_fields;
    static unsigned         msg_count = 0;

    nufr_launch_task(NUFR_TID_02, 0);
    nufr_launch_task(NUFR_TID_03, 0);

    while (1)
    {
        // Will receive messages in message priority order--not
        // in the order which they were sent. This is because
        // Task 2 (sender) runs at same priority as Task 1 (receiver),
        // so Task 2 isn't pre-empted when it send the messages.
        nsvc_msg_get_structW(&msg_fields);

        UT_ENSURE(7 == msg_fields.optional_parameter);
        UT_ENSURE(NUFR_MSG_PRI_HIGH == msg_fields.priority);

        nsvc_msg_get_structW(&msg_fields);
        UT_ENSURE(6 == msg_fields.optional_parameter);
        UT_ENSURE(NUFR_MSG_PRI_MID == msg_fields.priority);

        nsvc_msg_get_structW(&msg_fields);
        UT_ENSURE(5 == msg_fields.optional_parameter);
        UT_ENSURE(NUFR_MSG_PRI_LOW == msg_fields.priority);

        // This 4th message is received in order, because
        // Task 2 yielded before it was sent. This yield
        // caused Task 1 to consume the first 3 messages.
        nsvc_msg_get_structW(&msg_fields);
        UT_ENSURE(8 == msg_fields.optional_parameter);
        UT_ENSURE(NUFR_MSG_PRI_CONTROL == msg_fields.priority);

        msg_count++;
        if ((msg_count % 1000) == 0)
            msg_count += 1;
    }
}

void test_msg_circle_entry_02(void)
{
    nsvc_msg_fields_unary_t msg_fields1, msg_fields2;
    nsvc_msg_fields_unary_t msg_fields3, msg_fields4;
    nufr_msg_send_rtn_t     nufr_send_status;
    nsvc_msg_send_return_t  send_status;
    nufr_msg_t             *alloc_msg;

    while (1)
    {
        // First 2 msgs received in reverse order of sent order,
        //  due to prioritize call
        nsvc_msg_get_structW(&msg_fields2);
        UT_ENSURE(NSVC_MSG_PREFIX_local == msg_fields2.prefix);
        UT_ENSURE(MSG_ID_CIRCLE == msg_fields2.id);
        UT_ENSURE(NUFR_MSG_PRI_MID == msg_fields2.priority);
        UT_ENSURE(NUFR_TID_03 == msg_fields2.sending_task);
        UT_ENSURE(2 == msg_fields2.optional_parameter);

        nsvc_msg_get_structW(&msg_fields1);
        UT_ENSURE(NSVC_MSG_PREFIX_local == msg_fields1.prefix);
        UT_ENSURE(MSG_ID_CIRCLE == msg_fields1.id);
        UT_ENSURE(NUFR_MSG_PRI_LOW == msg_fields1.priority);
        UT_ENSURE(NUFR_TID_03 == msg_fields1.sending_task);
        UT_ENSURE(1 == msg_fields1.optional_parameter);

        // Next 2 msgs received in order that they're sent,
        //   since this task is a higher priority, each msg
        //   send causes it to preempt task 3.
        nsvc_msg_get_structW(&msg_fields3);
        UT_ENSURE(NSVC_MSG_PREFIX_local == msg_fields3.prefix);
        UT_ENSURE(MSG_ID_CIRCLE == msg_fields3.id);
        UT_ENSURE(NUFR_MSG_PRI_HIGH == msg_fields3.priority);
        UT_ENSURE(NUFR_TID_03 == msg_fields3.sending_task);
        UT_ENSURE(3 == msg_fields3.optional_parameter);

        nsvc_msg_get_structW(&msg_fields4);
        UT_ENSURE(NSVC_MSG_PREFIX_local == msg_fields4.prefix);
        UT_ENSURE(MSG_ID_CIRCLE == msg_fields4.id);
        UT_ENSURE(NUFR_MSG_PRI_CONTROL == msg_fields4.priority);
        UT_ENSURE(NUFR_TID_03 == msg_fields4.sending_task);
        UT_ENSURE(4 == msg_fields4.optional_parameter);

        // Send them to task 1, in original order
        msg_fields1.optional_parameter = 5;
        msg_fields1.destination_task = NUFR_TID_01;
        send_status = nsvc_msg_send_argsW(msg_fields1.prefix,
                                          msg_fields1.id,
                                          msg_fields1.priority,
                                          msg_fields1.destination_task,
                                          msg_fields1.optional_parameter);
        UT_ENSURE(NSVC_MSRT_OK == send_status);

        msg_fields2.optional_parameter = 6;
        msg_fields2.destination_task = NUFR_TID_01;
        send_status = nsvc_msg_send_structW(&msg_fields2);
        UT_ENSURE(NSVC_MSRT_OK == send_status);

        msg_fields3.optional_parameter = 7;
        msg_fields3.destination_task = NUFR_TID_01;
        alloc_msg = nufr_msg_get_block();
        alloc_msg->fields = nsvc_msg_args_to_fields(
                                          msg_fields3.prefix,
                                          msg_fields3.id,
                                          msg_fields3.priority,
                                          msg_fields3.sending_task);
        alloc_msg->parameter = msg_fields3.optional_parameter;
        nufr_send_status = nufr_msg_send_by_block(alloc_msg, msg_fields3.destination_task);
        UT_ENSURE(NUFR_MSG_SEND_OK == nufr_send_status);

        // Task 2 will yield; Task 1 will run; Task 1 will consume
        // first 3 messages, then wait for this fourth
        nufr_yield();

        msg_fields4.optional_parameter = 8;
        msg_fields4.destination_task = NUFR_TID_01;
        send_status = nsvc_msg_send_structW(&msg_fields4);
        UT_ENSURE(NSVC_MSRT_OK == send_status);
    }
}

void test_msg_circle_entry_03(void)
{

    nufr_msg_t             *msg1, *msg2, *msg3, *msg4;
    nufr_msg_send_rtn_t     nufr_send_status;
    nsvc_msg_send_return_t  send_status;
    nufr_tid_t              self_tid;

    UNUSED(send_status);

    while (1)
    {
        msg1 = nufr_msg_get_block();
        msg2 = nufr_msg_get_block();
        msg3 = nufr_msg_get_block();
        msg4 = nufr_msg_get_block();

        self_tid = nufr_self_tid();
        msg1->fields = NUFR_SET_MSG_FIELDS(NSVC_MSG_PREFIX_local,
                                           MSG_ID_CIRCLE,
                                           self_tid,
                                           NUFR_MSG_PRI_LOW);
        msg1->parameter = 1;
        msg2->fields = NUFR_SET_MSG_FIELDS(NSVC_MSG_PREFIX_local,
                                           MSG_ID_CIRCLE,
                                           self_tid,
                                           NUFR_MSG_PRI_MID);
        msg2->parameter = 2;
        msg3->fields = NUFR_SET_MSG_FIELDS(NSVC_MSG_PREFIX_local,
                                           MSG_ID_CIRCLE,
                                           self_tid,
                                           NUFR_MSG_PRI_HIGH);
        msg3->parameter = 3;
        msg4->fields = NUFR_SET_MSG_FIELDS(NSVC_MSG_PREFIX_local,
                                           MSG_ID_CIRCLE,
                                           self_tid,
                                           NUFR_MSG_PRI_CONTROL);
        msg4->parameter = 4;

        nufr_prioritize();

        nufr_send_status = nufr_msg_send_by_block(msg1, NUFR_TID_02);
        UT_ENSURE(NUFR_MSG_SEND_OK == nufr_send_status);

        nufr_send_status = nufr_msg_send_by_block(msg2, NUFR_TID_02);
        UT_ENSURE(NUFR_MSG_SEND_OK == nufr_send_status);

        nufr_unprioritize();

        // This send will awaken Task 2, causing Task 3 to block,
        // until Task 2 blocks again
        nufr_send_status = nufr_msg_send_by_block(msg3, NUFR_TID_02);
        UT_ENSURE(NUFR_MSG_SEND_AWOKE_RECEIVER == nufr_send_status);

        // Same for this send
        nufr_send_status = nufr_msg_send_by_block(msg4, NUFR_TID_02);
        UT_ENSURE(NUFR_MSG_SEND_AWOKE_RECEIVER == nufr_send_status);

        // When we got to here
        // Does nothing: there's only 1 task at our priority, of the 3
        nufr_yield();
    }
}

//***
//***** TEST_BOPS
//***
// Task 1 and Task 2 are at same priority

// Exercises:
// - bop key obtained
// - task waiting on bop not released if key is incorrect
// - task waiting on bop released if key is correct
// - bop key override
// - bop wait abort due to message send
// - messages received first by order they're sent,
//    second, in order that they're received
//

void test_bops_entry_01(void)
{
    nufr_launch_task(NUFR_TID_02, 0);

    while (1)
    {
        bops_key1 = nufr_bop_get_key();

        nufr_bop_waitW(NUFR_MSG_PRI_HIGH);    //[A]

        nufr_bop_send_with_key_override(NUFR_TID_02);
        // Task 2 was unblocked. Let it run.
        nufr_yield();                          //[D]

        // [F]
        // The last message send will cause Task 2's
        // bop wait to abort due to message send
        nsvc_msg_send_argsW(NSVC_MSG_PREFIX_B,
                            10,
                            NUFR_MSG_PRI_HIGH,
                            NUFR_TID_02,
                            20);
        nsvc_msg_send_argsW(NSVC_MSG_PREFIX_B,
                            11,
                            NUFR_MSG_PRI_HIGH,
                            NUFR_TID_02,
                            21);
        nsvc_msg_send_argsW(NSVC_MSG_PREFIX_B,
                            12,
                            NUFR_MSG_PRI_CONTROL,
                            NUFR_TID_02,
                            22);

        nufr_yield();  //[G]
    }
}

void test_bops_entry_02(void)
{
    nsvc_msg_fields_unary_t msg_fields1;
    nsvc_msg_fields_unary_t msg_fields2;
    nsvc_msg_fields_unary_t msg_fields3;

    while (1)
    {
        nufr_bop_send(NUFR_TID_01, 1000);       //wrong key value, arbitrary
        nufr_bop_send(NUFR_TID_01, bops_key1);   //[B]

        bops_key2 = nufr_bop_get_key();

        nufr_bop_waitW(NUFR_MSG_PRI_HIGH);       //[C]

        // [E]
        nsvc_msg_get_structW(&msg_fields3);   //3rd message sent
        nsvc_msg_get_structW(&msg_fields1);   //1st
        nsvc_msg_get_structW(&msg_fields2);   //2nd

        // [H]
        UT_ENSURE(msg_fields1.prefix == NSVC_MSG_PREFIX_B);
        UT_ENSURE(msg_fields1.id == 10);
        UT_ENSURE(msg_fields1.priority == NUFR_MSG_PRI_HIGH);
        UT_ENSURE(msg_fields1.sending_task == NUFR_TID_01);
        UT_ENSURE(msg_fields1.optional_parameter == 20);

        UT_ENSURE(msg_fields2.prefix == NSVC_MSG_PREFIX_B);
        UT_ENSURE(msg_fields2.id == 11);
        UT_ENSURE(msg_fields2.priority == NUFR_MSG_PRI_HIGH);
        UT_ENSURE(msg_fields2.sending_task == NUFR_TID_01);
        UT_ENSURE(msg_fields2.optional_parameter == 21);

        UT_ENSURE(msg_fields3.prefix == NSVC_MSG_PREFIX_B);
        UT_ENSURE(msg_fields3.id == 12);
        UT_ENSURE(msg_fields3.priority == NUFR_MSG_PRI_CONTROL);
        UT_ENSURE(msg_fields3.sending_task == NUFR_TID_01);
        UT_ENSURE(msg_fields3.optional_parameter == 22);

        nufr_yield();   //[I]
    }

}

//***
//***** TEST_BOP_FEATURES
//***
//
// [A] Tasks 3 is launched from Task 1. Task 1 stands by waiting for a msg.
// [B] Task 3 gets to run. It sets its local struct.
//     It gets its own bop key, then builds a msg.
// [C] Task 3 sends the message and blocks on the send.
// [D] Since Task 1 is at a higher priority than 3, it preempts 3 the moment
//     Task 3 sends the message, and continues with message.
// [E] Task 1 sends a bop to Task 3. Bop is sent before Task 3 is waiting on
//     the bop.
// [F] Task 1 waits on a message, allowing Task 3 to unblock.
// [G] Task 3 proceeds until it gets its bop. The bop is waiting. It walks by.
// [H] Task 3 sends a message to Task 1, which is waiting on a bop, not
//     a message. Task 1 does not wake up because it's bop locked
// [I] The bop lock is released. Task 3 blocks, as Task 3 is scheduled.
// [J] Task 1 wakes up from bop. It's informed that it awoke early due
//     to an abort msg send. The lock does not affect the abort msg send.
// [K] Task 1 does a timed wait on a bop. Will not timeout.
// [L] Task 3 runs now, since Task 1 is blocked. Task 3 sends an abort msg.
// [M] Task 1 resumes due to abort. Task 1 exists.
// [N] Since Task 1 has exited, Task 3 is unlocked. It exits also.
//
typedef struct
{
    unsigned value1;
    uint8_t  value2;
} test_bop_local_struct_t; 

void test_bop_features_entry_01(void)
{
    nsvc_msg_fields_unary_t  msg;
    test_bop_local_struct_t *struct_ptr;
    nufr_bop_rtn_t           bop_rv;
    nufr_bop_wait_rtn_t      bop_wait_rv;

    nufr_launch_task(NUFR_TID_03, 0);

    // [A]
    nsvc_msg_get_structW(&msg);

    // [D]
    UT_ENSURE(msg.sending_task == NUFR_TID_03);
    UT_ENSURE(msg.destination_task == NUFR_TID_null);
    UT_ENSURE(msg.optional_parameter == bops_key3);
    UT_ENSURE(msg.id == 0);

    // Locking isn't needed, but since it's standard procedure when using
    //  local ptrs, just checking code path
    bop_rv = nufr_bop_lock_waiter(NUFR_TID_03,
                                  (uint16_t)msg.optional_parameter);
    UT_ENSURE(bop_rv == NUFR_BOP_RTN_TASK_NOT_WAITING);
     
    // Make sure contents
    struct_ptr = nufr_local_struct_get(NUFR_TID_03);
    UT_ENSURE(struct_ptr != NULL);
    UT_ENSURE(struct_ptr->value1 == 3000);
    UT_ENSURE(struct_ptr->value2 == 30);

    nufr_bop_unlock_waiter(NUFR_TID_03);

    // [E]
    bop_rv = nufr_bop_send(NUFR_TID_03, (uint16_t)msg.optional_parameter);
    UT_ENSURE(bop_rv == NUFR_BOP_RTN_TASK_NOT_WAITING);

    // Wait on another msg send. This one will be priority aborted...
    // but not until lock is released.

    bops_key1 = nufr_bop_get_key();

    // [F]
    bop_wait_rv = nufr_bop_waitW(NUFR_MSG_PRI_LOW);

    // [J]
    // We got bop locked then unlocked, but still get the abort rv
    UT_ENSURE(bop_wait_rv == NUFR_BOP_WAIT_ABORTED_BY_MESSAGE);

    // There is a message waiting for us. It was the abort message
    nsvc_msg_get_structT(&msg, 0);

    UT_ENSURE(msg.sending_task == NUFR_TID_03);
    UT_ENSURE(msg.destination_task == NUFR_TID_null);
    UT_ENSURE(msg.optional_parameter == 0);
    UT_ENSURE(msg.id == 1);
    UT_ENSURE(msg.priority == NUFR_MSG_PRI_MID);

    bops_key1 = nufr_bop_get_key();

    // [K]
    bop_wait_rv = nufr_bop_waitT(NUFR_MSG_PRI_LOW, 10);

    // We got bop locked then unlocked, but still get the abort rv
    UT_ENSURE(bop_wait_rv == NUFR_BOP_WAIT_ABORTED_BY_MESSAGE);

    // There is a message waiting for us. It was the abort message
    nsvc_msg_get_structT(&msg, 0);

    // [M]
    // Timed wait aborted due to msg send.
    UT_ENSURE(msg.sending_task == NUFR_TID_03);
    UT_ENSURE(msg.destination_task == NUFR_TID_null);
    UT_ENSURE(msg.optional_parameter == 0);
    UT_ENSURE(msg.id == 2);
    UT_ENSURE(msg.priority == NUFR_MSG_PRI_MID);
}

void test_bop_features_entry_03(void)
{
    nsvc_msg_fields_unary_t msg;
    nsvc_msg_send_return_t   msg_send_rv;
    nufr_bop_wait_rtn_t     bop_wait_rv;
    nufr_bop_rtn_t          bop_lock_rv;
    uint16_t                local_key;
    test_bop_local_struct_t local_struct;

    memset(&local_struct, 0, sizeof(local_struct));

    // [B]
    local_key = nufr_bop_get_key();
    bops_key3 = local_key;

    nufr_local_struct_set(&local_struct);

    // Write to local struct values after setting struct,
    // just as another exercise
    local_struct.value1 = 3000;
    local_struct.value2 = 30;

    msg.destination_task = NUFR_TID_01;
    msg.prefix = NSVC_MSG_PREFIX_local;
    msg.id = 0;
    msg.priority = NUFR_MSG_PRI_MID;
    msg.optional_parameter = local_key;
    //msg.sending_task = NUFR_TID_null;     //auto filled, not needed

    // [C]
    msg_send_rv = nsvc_msg_send_structW(&msg);
    UT_ENSURE(NSVC_MSRT_AWOKE_RECEIVER == msg_send_rv);
    // Change local struct value, to ensure that receiver has taken
    // values already.
    local_struct.value1++;
    local_struct.value2++;

    // [G]
    // The bop will have pre-arrived and this bop wait will blow by
    // Set abort priority to the lowest value, just to exercise it more:
    // There will be no msg abort.
    bop_wait_rv = nufr_bop_waitT(NUFR_MSG_PRI_LOW, 0);
    UT_ENSURE(NUFR_BOP_WAIT_OK == bop_wait_rv);

    //
    bop_lock_rv = nufr_bop_lock_waiter(NUFR_TID_01, bops_key1);
    UT_ENSURE(NUFR_BOP_RTN_TAKEN == bop_lock_rv);

    msg.destination_task = NUFR_TID_01;
    msg.prefix = NSVC_MSG_PREFIX_local;
    msg.id = 1;
    msg.priority = NUFR_MSG_PRI_MID;
    msg.optional_parameter = 0;

    // [H]
    // Send an abortable message. Task 1 is locked on bop, so
    // bop won't abort. Return code indicates that it did, however.
    msg_send_rv = nsvc_msg_send_structW(&msg);
    UT_ENSURE(NSVC_MSRT_ABORTED == msg_send_rv);

    // [I]
    // This will cause Task 1 come alive due to a message abort
    nufr_bop_unlock_waiter(NUFR_TID_01);

    // [L]
    // Send a message to Task 1 to cause it to abort again.
    msg.destination_task = NUFR_TID_01;
    msg.prefix = NSVC_MSG_PREFIX_local;
    msg.id = 2;
    msg.priority = NUFR_MSG_PRI_MID;
    msg.optional_parameter = 0;

    // This will cause Task 1 come alive due to a message abort
    msg_send_rv = nsvc_msg_send_structW(&msg);

    // fixme: we never get here

    // [N]
    UT_ENSURE(NSVC_MSRT_ABORTED == msg_send_rv);

}

//***
//***** TEST_PRIORITY_INVERSION
//***
//
// [A] Task 1 launches Task 3, then waits.
// [B] Task 3 runs, gets mutex, then unblocks Task 1
// [C] Task 1 resumes, launches Task 2, then yields to Task 2.
// [D] Task 2 runs, tries to get mutex, blocks
//     This is a priority inversion. Task 3 made ready.
// [E] Task 1 runs again, Task 3 is on ready list.
//     Task 1 yields a 2nd time, this time to let Task 3 run.
// [F] Task 3 runs, with its priority raised. It returns
//     the mutex, and in so doing, is restored to its original priority.
// [G] Task 2 now gets the mutex it was waiting on.
//     It returns it, then waits on a bop.
// [H] Task 1, waiting on the ready list, gets scheduled.
//     It attempts to takes the mutex, but is blocked on it.
// [I] Task 1 gives a bop to Task 2, making it ready.
//     Task 1 yields, allowing Task 2 to run.
// [J] Task 2 attempts to get mutex. Mutex is still owned by Task 1,
//     so Task 2 blocks on mutex.
// [K] Task 2 blocking allows Task 1 to proceed.
//     Task 1 sends a msg to Task 2. The message will cause an abort.
// [L] Task 2 resumes due to message abort.
// [M] Task 2 tries to get mutex again, which is still owned by
//     Task 1. It blocks again.
// [N] Task 1 resumes from yield. It sends another abort msg to
//     Task 2.
// [O] Task 1 terminates. This lets Task 2 run.
// [P] Task 2 consumes abort msg. Task 2 terminates.
// [Q] Task 3 terminates.
//
void test_priority_inversion_entry_01(void)
{
    nufr_sema_get_rtn_t      mutex_rv;
    nufr_bop_wait_rtn_t      bop_wait_rv;
    nufr_bop_rtn_t           bop_rv;
    nsvc_msg_send_return_t   msg_send_rv;
    bool                     did_yield;

    nufr_launch_task(NUFR_TID_03, 0);

    // [A]
    bops_key1 = nufr_bop_get_key();
    bop_wait_rv = nufr_bop_waitT(NUFR_NO_ABORT, 5);
    UT_ENSURE(NUFR_BOP_WAIT_OK == bop_wait_rv);

    nufr_launch_task(NUFR_TID_02, 0);

    // [C]
    did_yield = nufr_yield();
    UT_ENSURE(did_yield);

    // [E]
    did_yield = nufr_yield();
    UT_ENSURE(did_yield);

    // [H]
    // The mutex is not available.
    mutex_rv = nsvc_mutex_getT(NSVC_MUTEX_1, NUFR_NO_ABORT, 3);
    UT_ENSURE(mutex_rv == NUFR_SEMA_GET_OK_BLOCK);

    bop_rv = nufr_bop_send(NUFR_TID_02, bops_key2);
    UNUSED(bop_rv);   //fixme: delete this line, uncomment next, after verifying
    //UT_ENSURE(NUFR_BOP_RTN_TAKEN == bop_rv);

    // [I]
    did_yield = nufr_yield();
    UT_ENSURE(did_yield);

    // [K]
    // This will cause a message abort
    msg_send_rv = nsvc_msg_send_argsW(
                                 NSVC_MSG_PREFIX_B,
                                 10,
                                 NUFR_MSG_PRI_MID,
                                 NUFR_TID_02,
                                 1);
    UT_ENSURE(msg_send_rv == NSVC_MSRT_ABORTED);

    // [L]
    // Let Task 2 resume from message abort
    did_yield = nufr_yield();
    UT_ENSURE(did_yield);

    // [N]
    msg_send_rv = nsvc_msg_send_argsW(
                                 NSVC_MSG_PREFIX_B,
                                 11,
                                 NUFR_MSG_PRI_MID,
                                 NUFR_TID_02,
                                 2);
    UT_ENSURE(msg_send_rv == NSVC_MSRT_ABORTED);

    did_yield = nufr_yield();
    UT_ENSURE(did_yield);

    // [O]
    // exit
}

void test_priority_inversion_entry_02(void)
{
    nufr_sema_get_rtn_t      mutex_rv;
    bool                     mutex_boolean;
    bool                     msg_get_boolean;
    nufr_msg_t              *msg_block_ptr;
    nsvc_msg_fields_unary_t  msg;

    // [D]
    // Mutex already taken by Task 3.
    // This is a priority inversion. Call will raise Task 3's priority.
    mutex_rv = nsvc_mutex_getW(NSVC_MUTEX_1, NUFR_NO_ABORT);
    UT_ENSURE(mutex_rv == NUFR_SEMA_GET_OK_BLOCK);

    // [G]
    mutex_boolean = nsvc_mutex_release(NSVC_MUTEX_1);
    UT_ENSURE(mutex_boolean == true);

    bops_key2 = nufr_bop_get_key();
    nufr_bop_waitT(NUFR_NO_ABORT, 5);

    // [J]
    // Mutex already owned by Task 1. This call will be msg aborted.
    mutex_rv = nsvc_mutex_getW(NSVC_MUTEX_1, NUFR_MSG_PRI_LOW);
    UT_ENSURE(mutex_rv == NUFR_SEMA_GET_MSG_ABORT);

    // A message will be here already, so use timeout of 0 to enforce it.
    msg_get_boolean = nsvc_msg_get_structT(&msg, 0);
    UT_ENSURE(msg_get_boolean == true);
    UT_ENSURE(msg.id == 10);

    // [M]
    // Mutex already owned by Task 1. This call will be msg aborted.
    mutex_rv = nsvc_mutex_getT(NSVC_MUTEX_1, NUFR_MSG_PRI_LOW, 5);
    UT_ENSURE(mutex_rv == NUFR_SEMA_GET_MSG_ABORT);

    // [P]
    // A message will be here already, so use timeout of 0 to enforce it.
    msg_block_ptr = nufr_msg_peek();
    UT_ENSURE(msg_block_ptr != NULL);
    nufr_msg_drain(nufr_self_tid(), NUFR_MSG_PRI_MID);
    msg_block_ptr = nufr_msg_peek();
    UT_ENSURE(msg_block_ptr == NULL);

    msg_get_boolean = nsvc_msg_get_structT(&msg, 0);
    UT_ENSURE(msg_get_boolean == false);

    // exit
}

void test_priority_inversion_entry_03(void)
{
    nufr_sema_get_rtn_t      mutex_rv;
    nufr_bop_rtn_t           bop_rv;
    bool                     mutex_boolean;

    mutex_rv = nsvc_mutex_getW(NSVC_MUTEX_1, NUFR_NO_ABORT);
    UT_ENSURE(mutex_rv == NUFR_SEMA_GET_OK_NO_BLOCK);

    // [B]
    bop_rv = nufr_bop_send(NUFR_TID_01, bops_key1);
    UT_ENSURE(NUFR_BOP_RTN_TAKEN == bop_rv);

    // [F]
    // Priority raised here for priority inversion protection.
    // When mutex is released, priority will revert back.
    mutex_boolean = nsvc_mutex_release(NSVC_MUTEX_1);
    UT_ENSURE(mutex_boolean == true);

    // [Q]
    // exit
}

//***
//***** TEST_ZOMBIE_TIMERS
//***
//
void test_zombie_timers_entry_01(void)
{
    bool                    yield_rv;
    bool                    mutex_release_rv;
    nufr_sema_get_rtn_t     mutex_get_rv;
    nsvc_msg_send_return_t  msg_send_rv;
    nufr_bop_rtn_t          bop_send_rv;

    nufr_launch_task(NUFR_TID_02, 0);

    // [A]
    // Let Task 2 run, and block on message get
    yield_rv = nufr_yield();
    UT_ENSURE(yield_rv);

    // [C]
    // Resume after Task 2 blocked on message get.
    // Send it a message.
    msg_send_rv = nsvc_msg_send_argsW(
                                 NSVC_MSG_PREFIX_B,
                                 1,
                                 NUFR_MSG_PRI_MID,
                                 NUFR_TID_02,
                                 0);
    UT_ENSURE(NSVC_MSRT_OK == msg_send_rv);

    // Hold the mutex, forcing Task 2 to block when it
    // triest to get it.
    mutex_get_rv = nsvc_mutex_getW(NSVC_MUTEX_1, NUFR_NO_ABORT);
    UT_ENSURE(NUFR_SEMA_GET_OK_NO_BLOCK == mutex_get_rv);

    // [D]
    // Let Task 2 try to get the mutex
    yield_rv = nufr_yield();
    UT_ENSURE(yield_rv);

    // [F]
    // Let go of the mutex.
    mutex_release_rv = nsvc_mutex_release(NSVC_MUTEX_1);
    UT_ENSURE(true == mutex_release_rv);

    // Task 2 will try to get the mutex now.
    yield_rv = nufr_yield();
    UT_ENSURE(yield_rv);

    // [H]
    // Resuming from Task 2 waiting on a bop.
    // Give them the bop they want
    bop_send_rv = nufr_bop_send(NUFR_TID_02, bops_key2);
    UT_ENSURE(NUFR_BOP_RTN_TAKEN == bop_send_rv);

    // Allow Task 2 to take bop
    yield_rv = nufr_yield();
    UT_ENSURE(yield_rv);

    // exit
}

void test_zombie_timers_entry_02(void)
{
    nsvc_msg_fields_unary_t msg;
    bool                    msg_get_rv;
    bool                    mutex_release_rv;
    nufr_sema_get_rtn_t     mutex_get_rv;
    nufr_bop_wait_rtn_t     bop_wait_rv;

    memset(&msg, 0, sizeof(msg));

    // [B]
    // Will block on message
    msg_get_rv = nsvc_msg_get_structT(&msg, 5);

    // [E]
    // Message received. Timeout aborted.
    UT_ENSURE(true == msg_get_rv);
    UT_ENSURE(1 == msg.id);

    // Task 1 has mutex. Will wait on it.
    mutex_get_rv = nsvc_mutex_getT(NSVC_MUTEX_1, NUFR_NO_ABORT, 5);
    UT_ENSURE(NUFR_SEMA_GET_OK_BLOCK == mutex_get_rv);

    // We got the mutex. Flush it.
    mutex_release_rv = nsvc_mutex_release(NSVC_MUTEX_1);
    UT_ENSURE(false == mutex_release_rv);

    // [G]
    // Go immediately to a bop wait.
    bops_key2 = nufr_bop_get_key();
    bop_wait_rv = nufr_bop_waitT(NUFR_NO_ABORT, 5);
    UT_ENSURE(NUFR_BOP_WAIT_OK == bop_wait_rv);

    //exit
}

//***
//***** TEST_API_TIMEOUTS
//***
//
void test_api_timeouts_entry_01(void)
{
    nsvc_msg_fields_unary_t msg;
    bool                    msg_get_rv;
    nufr_sema_get_rtn_t     mutex_get_rv;
    nufr_bop_wait_rtn_t     bop_wait_rv;

    // Launch other task, so it'll camp on mutex
    nufr_launch_task(NUFR_TID_02, 0);
    nufr_yield();

    memset(&msg, 0, sizeof(msg));

    // Timeout on getting message
    msg_get_rv = nsvc_msg_get_structT(&msg, 3);

    UT_ENSURE(false == msg_get_rv);
    UT_ENSURE(0 == msg.id);
    UT_ENSURE(0 == msg.optional_parameter);

    // Timeout on mutex get
    mutex_get_rv = nsvc_mutex_getT(NSVC_MUTEX_1, NUFR_NO_ABORT, 3);
    UT_ENSURE(NUFR_SEMA_GET_TIMEOUT == mutex_get_rv);

    // Timeout on bop get
    bop_wait_rv = nufr_bop_waitT(NUFR_NO_ABORT, 3);
    UT_ENSURE(NUFR_BOP_WAIT_TIMEOUT == bop_wait_rv);

    //exit
}

void test_api_timeouts_entry_02(void)
{
    nufr_sema_get_rtn_t     mutex_get_rv;

    mutex_get_rv = nsvc_mutex_getW(NSVC_MUTEX_1, NUFR_NO_ABORT);
    UT_ENSURE(NUFR_SEMA_GET_OK_NO_BLOCK == mutex_get_rv);

    nufr_sleep(10000, NUFR_NO_ABORT);
}

//***
//***** TEST_MISC
//***
//
// note-- misc tests for scaled msg drains, nufr_change_task_priority()

void test_misc_entry_01(void)
{
    nsvc_msg_send_return_t msg_send_rv;

    nufr_launch_task(NUFR_TID_02, 0);

    //*** Send 8 messages to Task 2's inbox, then delete them.
    msg_send_rv = nsvc_msg_send_argsW(NSVC_MSG_PREFIX_B,
                    1, NUFR_MSG_PRI_CONTROL, NUFR_TID_02, 1);
    msg_send_rv = nsvc_msg_send_argsW(NSVC_MSG_PREFIX_B,
                    2, NUFR_MSG_PRI_CONTROL, NUFR_TID_02, 2);
    msg_send_rv = nsvc_msg_send_argsW(NSVC_MSG_PREFIX_B,
                    3, NUFR_MSG_PRI_HIGH, NUFR_TID_02, 3);
    msg_send_rv = nsvc_msg_send_argsW(NSVC_MSG_PREFIX_B,
                    4, NUFR_MSG_PRI_HIGH, NUFR_TID_02, 4);
    msg_send_rv = nsvc_msg_send_argsW(NSVC_MSG_PREFIX_B,
                    5, NUFR_MSG_PRI_MID, NUFR_TID_02, 5);
    msg_send_rv = nsvc_msg_send_argsW(NSVC_MSG_PREFIX_B,
                    6, NUFR_MSG_PRI_MID, NUFR_TID_02, 6);
    msg_send_rv = nsvc_msg_send_argsW(NSVC_MSG_PREFIX_B,
                    7, NUFR_MSG_PRI_LOW, NUFR_TID_02, 7);
    msg_send_rv = nsvc_msg_send_argsW(NSVC_MSG_PREFIX_B,
                    8, NUFR_MSG_PRI_LOW, NUFR_TID_02, 8);

    UNUSED(msg_send_rv);

    nufr_msg_drain(NUFR_TID_02, NUFR_MSG_PRI_CONTROL);


    //*** Adjust task priorities

    // Raise Task's 2 priority. Doing this will cause context switch to Task 2.
    nufr_change_task_priority(NUFR_TID_02, 6);

    //exit
}

void test_misc_entry_02(void)
{
    // Task 1 changed us to a higher priority than themselves.
    // We're running now.
    // Put back to original priority. We'll be put back at same
    // priority as Task 1, but behind Task 1 in ready list.
    // Context switch occurs.
    nufr_change_task_priority(nufr_self_tid(), 7);

    //exit
}

//***
//***** Entry points
//***

void entry_01(unsigned parm)
{
    UNUSED(parm);

    if (TEST_SLEEP_AND_SEND == current_test)
        test_sleep_and_send_entry_01();
    else if (TEST_MSG_CIRCLE == current_test)
        test_msg_circle_entry_01();
    else if (TEST_BOPS == current_test)
        test_bops_entry_01();
    else if (TEST_BOP_FEATURES == current_test)
        test_bop_features_entry_01();
    else if (TEST_PRIORITY_INVERSION == current_test)
        test_priority_inversion_entry_01();
    else if (TEST_ZOMBIE_TIMERS == current_test)
        test_zombie_timers_entry_01();
    else if (TEST_API_TIMEOUTS == current_test)
        test_api_timeouts_entry_01();
    else if (TEST_MISC == current_test)
        test_misc_entry_01();

}

void entry_02(unsigned parm)
{
    UNUSED(parm);

    if (TEST_SLEEP_AND_SEND == current_test)
        test_sleep_and_send_entry_02();
    else if (TEST_MSG_CIRCLE == current_test)
        test_msg_circle_entry_02();
    else if (TEST_BOPS == current_test)
        test_bops_entry_02();
//    else if (TEST_BOP_FEATURES == current_test)
//        test_bop_features_entry_02();
    else if (TEST_PRIORITY_INVERSION == current_test)
        test_priority_inversion_entry_02();
    else if (TEST_ZOMBIE_TIMERS == current_test)
        test_zombie_timers_entry_02();
    else if (TEST_API_TIMEOUTS == current_test)
        test_api_timeouts_entry_02();
    else if (TEST_MISC == current_test)
        test_misc_entry_02();
}

void entry_03(unsigned parm)
{
    UNUSED(parm);

    if (TEST_SLEEP_AND_SEND == current_test)
        test_sleep_and_send_entry_03();
    else if (TEST_MSG_CIRCLE == current_test)
        test_msg_circle_entry_03();
    //else if (TEST_BOPS == current_test)
    //    ;     // not task 3 used
    else if (TEST_BOP_FEATURES == current_test)
        test_bop_features_entry_03();
    else if (TEST_PRIORITY_INVERSION == current_test)
        test_priority_inversion_entry_03();
    //else if (TEST_ZOMBIE_TIMERS == current_test)
    //    test_zombie_timers_entry_03();
    //else if (TEST_API_TIMEOUTS == current_test)
    //    test_api_timeouts_entry_03();
    //else if (TEST_MISC == current_test)
    //    test_misc_entry_03();
}

//***
//***** Infrastructure
//***

//!
//! @name      nufr_sim_background_task
//!
//! @brief     Simulates BG
//!
//! @details   NUFR_INVOKE_CONTEXT_SWITCH() handler in pthread mode only
//!
//! @param[in] 'void_ptr'-- satisfies pthread api
//!
#if 0
     time_t t = time(NULL);

      tm s;

      gmtime_s(&s, &t);
#endif
void *sim_background_task(void *void_ptr)
{
    int rv;
    struct timespec     ts;

    UNUSED(void_ptr);

    nufr_launch_task(NUFR_TID_01, 0);

    // Spin rather than hog CPU. On real target, we would have to hog.
    while (1)
    {
        const uint32_t DELAY_SECONDS = 3;
        const uint32_t DELAY_MILLISECONDS = 0;
        const int32_t MILLISECS_TO_NANOSECS = 1000000;
        const int32_t NANOSECS_PER_SEC = 1000000000;
    #ifdef VISUAL_C_COMPILER
        SYSTEMTIME st;
        time_t local_time;
        //time_t final_utc_time;
        //struct tm  *utc_time;
    #else
//        clock_gettime(CLOCK_REALTIME, &ts);
    #endif

    #ifdef VISUAL_C_COMPILER
        //https://stackoverflow.com/questions/20370920/convert-current-time-from-windows-to-unix-timestamp-in-c-or-c
        GetSystemTime(&st);
        time(&local_time);
        //utc_time = gmtime(&local_time);
        //gmtime_s(&local_time, &utc_time);   ???
        //final_utc_time =  mktime(utc_time);
        //ts.tv_sec = final_utc_time;
        ts.tv_sec = local_time;
        ts.tv_nsec = st.wMilliseconds * MILLISECS_TO_NANOSECS;         //fixme: can't get < 1 sec resolution
    #else
        clock_gettime(CLOCK_REALTIME, &ts);
        UNUSED(MILLISECS_TO_NANOSECS);
    #endif

        ts.tv_sec += DELAY_SECONDS;
        ts.tv_nsec += DELAY_MILLISECONDS;
        if (ts.tv_nsec > NANOSECS_PER_SEC)
        {
            ts.tv_sec++;
            ts.tv_nsec -= NANOSECS_PER_SEC;
        }

        rv = sem_timedwait(&nufr_sim_bg_sem, &ts);

        if ( ! ((0 == rv) || (ETIMEDOUT == errno)) )
        {
            // 'errno' not working...must comment out code
            //break;
        }

        // TBD
    }

    return NULL;
}


void *sim_tick(void *void_ptr)
{
    int                 rv;
    struct timespec     ts;

    UNUSED(void_ptr);

    disable_systick = true;

    // Stroke systick periodically
    while (1)
    {
        const uint32_t DELAY_SECONDS = 0;
        const uint32_t DELAY_MILLISECONDS = 500;
        const int32_t MILLISECS_TO_NANOSECS = 1000000;
        const int32_t NANOSECS_PER_SEC = 1000000000;
    #ifdef VISUAL_C_COMPILER
        SYSTEMTIME st;
        time_t local_time;
        //time_t final_utc_time;
        //struct tm  *utc_time;
    #else
//        clock_gettime(CLOCK_REALTIME, &ts);
    #endif

    #ifdef VISUAL_C_COMPILER
        //https://stackoverflow.com/questions/20370920/convert-current-time-from-windows-to-unix-timestamp-in-c-or-c
        GetSystemTime(&st);
        time(&local_time);
        //utc_time = gmtime(&local_time);
        //gmtime_s(&local_time, &utc_time);   ???
        //final_utc_time =  mktime(utc_time);
        //ts.tv_sec = final_utc_time;
        ts.tv_sec = local_time;
        ts.tv_nsec = st.wMilliseconds * MILLISECS_TO_NANOSECS;         //fixme: can't get < 1 sec resolution
    #else
        clock_gettime(CLOCK_REALTIME, &ts);
        UNUSED(MILLISECS_TO_NANOSECS);
    #endif

        ts.tv_sec += DELAY_SECONDS;
        ts.tv_nsec += DELAY_MILLISECONDS;
        if (ts.tv_nsec > NANOSECS_PER_SEC)
        {
            ts.tv_sec++;
            ts.tv_nsec -= NANOSECS_PER_SEC;
        }

        rv = sem_timedwait(&nufr_sim_os_tick_sem, &ts);
        
        if ( ! ((0 == rv) || (ETIMEDOUT == errno)) )
        {
            //break;
        }

        if (!disable_systick)
        {
            systick_active = true;
            nufrplat_systick_handler();
            systick_active = false;
        }
    }

    return NULL;
}