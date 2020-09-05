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

//! @file    ut-nsvc-timer.c
//! @authors Bernie Woodland
//! @date    20Nov17

// Tests SL Timer functionality (nsvc-timer.c)

#include "nufr-global.h"
#include "nufr-platform.h"
#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-kernel-message-blocks.h"
#include "nsvc.h"
#include "nsvc-app.h"
#include "nsvc-api.h"

#include "nufr-kernel-task.h"

#include "raging-contract.h"
#include "raging-utils.h"

// ************ Start extern from nsvc-timer.c **********
extern nsvc_timer_t    *nsvc_timer_queue_head;
extern nsvc_timer_t    *nsvc_timer_queue_tail;
extern unsigned         nsvc_timer_queue_length;
extern nsvc_timer_t    *nsvc_timer_expired_list_head;
extern nsvc_timer_t    *nsvc_timer_expired_list_tail;
extern uint32_t         nsvc_timer_latest_time;

bool sl_timer_active_insert(nsvc_timer_t *tm,
                            nsvc_timer_t *before_this_tm);
bool sl_timer_active_dequeue(nsvc_timer_t *tm);
void sl_timer_push_expired(nsvc_timer_t *tm);
nsvc_timer_t *sl_timer_pop_expired(void);
nsvc_timer_t *sl_timer_find_sorted_insert(uint32_t duration,
                                          bool    *is_new_head_ptr);
void sl_timer_check_and_expire(uint32_t previous_check_time);
void sl_timer_send_message_for_expired_timer(nsvc_timer_t *tm);
bool sl_timer_process_expired_timers(void);
// ************ End extern from nsvc-timer.c   **********

uint32_t ut_hw_time;

uint32_t get_hw_time(void)
{
    return ut_hw_time;
}

void copy_timer_defaults(nsvc_timer_t *tm)
{
    tm->duration = 0;
    tm->msg_fields =  NUFR_SET_MSG_FIELDS(
                                 0,
                                 1,
                                 NUFR_TID_null,
                                 NUFR_MSG_PRI_MID);
    tm->mode = NSVC_TMODE_SIMPLE;
    tm->msg_parameter = 0;
    tm->dest_task_id = NUFR_TID_02;
}

// Exercise 'sl_timer_active_insert()' and 'sl_timer_active_dequeue()'
void ut_timer_active_list(void)
{
    nsvc_timer_t *tm1, *tm2, *tm3, *tm4, *tm5, *tm6, *tm7, *tm8, *tm9;
    bool rc;

    nsvc_init();
    nsvc_timer_init(get_hw_time, NULL);

    tm1 = nsvc_timer_alloc();
    tm2 = nsvc_timer_alloc();
    tm3 = nsvc_timer_alloc();
    tm4 = nsvc_timer_alloc();
    tm5 = nsvc_timer_alloc();
    tm6 = nsvc_timer_alloc();
    tm7 = nsvc_timer_alloc();
    tm8 = nsvc_timer_alloc();
    tm9 = nsvc_timer_alloc();

    // Simple add/delete
    rc = sl_timer_active_insert(tm1, NULL);
    UT_ENSURE(rc);
    UT_ENSURE(tm1 == nsvc_timer_queue_head);
    rc = sl_timer_active_dequeue(tm1);
    UT_ENSURE(rc);
    UT_ENSURE(NULL == nsvc_timer_queue_head);

    // Simple 3 timer add/delete
    rc = sl_timer_active_insert(tm3, NULL);
    UT_ENSURE(rc);
    rc = sl_timer_active_insert(tm2, tm3);
    UT_ENSURE(rc);
    rc = sl_timer_active_insert(tm1, tm2);
    UT_ENSURE(rc);
    UT_ENSURE(tm1 == nsvc_timer_queue_head);
    rc = sl_timer_active_dequeue(tm1);
    UT_ENSURE(rc);
    rc = sl_timer_active_dequeue(tm2);
    UT_ENSURE(rc);
    rc = sl_timer_active_dequeue(tm3);
    UT_ENSURE(rc);
    UT_ENSURE(NULL == nsvc_timer_queue_head);

    // Mix of appends and inserts
    rc = sl_timer_active_insert(tm1, NULL);
    UT_ENSURE(rc);
    rc = sl_timer_active_insert(tm2, NULL);
    UT_ENSURE(!rc);
    rc = sl_timer_active_insert(tm3, NULL);
    UT_ENSURE(!rc);
    rc = sl_timer_active_insert(tm4, tm2);
    UT_ENSURE(!rc);
    rc = sl_timer_active_insert(tm5, NULL);
    UT_ENSURE(!rc);
    rc = sl_timer_active_insert(tm6, tm1);
    UT_ENSURE(rc);
    rc = sl_timer_active_dequeue(tm3);
    UT_ENSURE(!rc);
    rc = sl_timer_active_dequeue(tm1);
    UT_ENSURE(!rc);
    rc = sl_timer_active_dequeue(tm6);
    UT_ENSURE(rc);
    rc = sl_timer_active_dequeue(tm2);
    UT_ENSURE(!rc);
    rc = sl_timer_active_dequeue(tm5);
    UT_ENSURE(!rc);
    rc = sl_timer_active_dequeue(tm4);
    UT_ENSURE(rc);
    UT_ENSURE(NULL == nsvc_timer_queue_head);

    // free timers for next test
    nsvc_timer_free(tm1);
    nsvc_timer_free(tm2);
    nsvc_timer_free(tm3);
    nsvc_timer_free(tm4);
    nsvc_timer_free(tm5);
    nsvc_timer_free(tm6);
    nsvc_timer_free(tm7);
    nsvc_timer_free(tm8);
    nsvc_timer_free(tm9);
}

void ut_timer_expired_list(void)
{
    nsvc_timer_t *tm1, *tm2, *tm3, *tm4, *tm5, *tm6, *tm7, *tm8, *tm9;
    nsvc_timer_t *popped_tm;

    nsvc_init();
    nsvc_timer_init(get_hw_time, NULL);

    tm1 = nsvc_timer_alloc();
    tm2 = nsvc_timer_alloc();
    tm3 = nsvc_timer_alloc();
    tm4 = nsvc_timer_alloc();
    tm5 = nsvc_timer_alloc();
    tm6 = nsvc_timer_alloc();
    tm7 = nsvc_timer_alloc();
    tm8 = nsvc_timer_alloc();
    tm9 = nsvc_timer_alloc();

    sl_timer_push_expired(tm1);
    sl_timer_push_expired(tm2);
    sl_timer_push_expired(tm3);
    sl_timer_push_expired(tm4);
    popped_tm = sl_timer_pop_expired();
    UT_ENSURE(popped_tm == tm1);
    popped_tm = sl_timer_pop_expired();
    UT_ENSURE(popped_tm == tm2);
    popped_tm = sl_timer_pop_expired();
    UT_ENSURE(popped_tm == tm3);
    popped_tm = sl_timer_pop_expired();
    UT_ENSURE(popped_tm == tm4);
    UT_ENSURE(NULL == nsvc_timer_expired_list_head);

    // Skip timer free: next test should clean this up
}

void ut_timer_sorted_insert(void)
{
    nsvc_timer_t *tm1, *tm2, *tm3, *tm4, *tm5, *tm6, *tm7, *tm8, *tm9;
    uint32_t offset;

    nsvc_init();
    nsvc_timer_init(get_hw_time, NULL);

    nufr_running = NUFR_TID_TO_TCB(NUFR_TID_01);

    tm1 = nsvc_timer_alloc();
    UT_ENSURE(NULL != tm1);
    tm2 = nsvc_timer_alloc();
    UT_ENSURE(NULL != tm2);
    tm3 = nsvc_timer_alloc();
    UT_ENSURE(NULL != tm3);
    tm4 = nsvc_timer_alloc();
    UT_ENSURE(NULL != tm4);
    tm5 = nsvc_timer_alloc();
    UT_ENSURE(NULL != tm5);
    tm6 = nsvc_timer_alloc();
    UT_ENSURE(NULL != tm6);
    tm7 = nsvc_timer_alloc();
    UT_ENSURE(NULL != tm7);
    tm8 = nsvc_timer_alloc();
    UT_ENSURE(NULL != tm8);
    tm9 = nsvc_timer_alloc();
    UT_ENSURE(NULL != tm9);

    copy_timer_defaults(tm1);
    copy_timer_defaults(tm2);
    copy_timer_defaults(tm3);
    copy_timer_defaults(tm4);
    copy_timer_defaults(tm5);
    copy_timer_defaults(tm6);
    copy_timer_defaults(tm7);
    copy_timer_defaults(tm8);
    copy_timer_defaults(tm9);

    //**** [1] Sequential insert
    //****

    // Set time reference to a nominal value
    ut_hw_time = 100;

    tm1->duration = 10;
    nsvc_timer_start(tm1);
    UT_ENSURE(tm1->expiration_time == ut_hw_time + 10);
    
    tm2->duration = 20;
    nsvc_timer_start(tm2);
    UT_ENSURE(tm2->expiration_time == ut_hw_time + 20);

    tm3->duration = 30;
    nsvc_timer_start(tm3);
    UT_ENSURE(tm3->expiration_time == ut_hw_time + 30);

    tm4->duration = 40;
    nsvc_timer_start(tm4);
    UT_ENSURE(tm4->expiration_time == ut_hw_time + 40);

    tm5->duration = 50;
    nsvc_timer_start(tm5);
    UT_ENSURE(tm5->expiration_time == ut_hw_time + 50);

    tm6->duration = 60;
    nsvc_timer_start(tm6);
    UT_ENSURE(tm6->expiration_time == ut_hw_time + 60);

    tm7->duration = 70;
    nsvc_timer_start(tm7);
    UT_ENSURE(tm7->expiration_time == ut_hw_time + 70);

    tm8->duration = 80;
    nsvc_timer_start(tm8);
    UT_ENSURE(tm8->expiration_time == ut_hw_time + 80);

    tm9->duration = 90;
    nsvc_timer_start(tm9);
    UT_ENSURE(tm9->expiration_time == ut_hw_time + 90);

    UT_ENSURE(nsvc_timer_queue_head == tm1);
    UT_ENSURE(nsvc_timer_queue_tail == tm9);
    UT_ENSURE(tm1->flink == tm2);
    UT_ENSURE(tm2->blink == tm1);
    UT_ENSURE(tm2->flink == tm3);
    UT_ENSURE(tm3->blink == tm2);
    UT_ENSURE(tm3->flink == tm4);
    UT_ENSURE(tm4->blink == tm3);
    UT_ENSURE(tm4->flink == tm5);
    UT_ENSURE(tm5->blink == tm4);
    UT_ENSURE(tm5->flink == tm6);
    UT_ENSURE(tm6->blink == tm5);
    UT_ENSURE(tm6->flink == tm7);
    UT_ENSURE(tm7->blink == tm6);
    UT_ENSURE(tm7->flink == tm8);
    UT_ENSURE(tm8->blink == tm7);
    UT_ENSURE(tm8->flink == tm9);
    UT_ENSURE(tm9->blink == tm8);

    nsvc_timer_kill(tm1);
    nsvc_timer_kill(tm2);
    nsvc_timer_kill(tm3);
    nsvc_timer_kill(tm4);
    nsvc_timer_kill(tm5);
    nsvc_timer_kill(tm6);
    nsvc_timer_kill(tm7);
    nsvc_timer_kill(tm8);
    nsvc_timer_kill(tm9);
    UT_ENSURE((NULL == tm1->flink) && (NULL == tm1->blink));
    UT_ENSURE((NULL == tm2->flink) && (NULL == tm2->blink));
    UT_ENSURE((NULL == tm3->flink) && (NULL == tm3->blink));
    UT_ENSURE((NULL == tm4->flink) && (NULL == tm4->blink));
    UT_ENSURE((NULL == tm5->flink) && (NULL == tm5->blink));
    UT_ENSURE((NULL == tm6->flink) && (NULL == tm6->blink));
    UT_ENSURE((NULL == tm7->flink) && (NULL == tm7->blink));
    UT_ENSURE((NULL == tm8->flink) && (NULL == tm8->blink));
    UT_ENSURE((NULL == tm9->flink) && (NULL == tm9->blink));
    UT_ENSURE(NULL == nsvc_timer_queue_head);
    UT_ENSURE(NULL == nsvc_timer_queue_tail);

    //**** [2] Reverse order insert
    //****

    ut_hw_time = 100;

    tm9->duration = 90;
    nsvc_timer_start(tm9);
    UT_ENSURE(tm9->expiration_time == ut_hw_time + 90);

    tm8->duration = 80;
    nsvc_timer_start(tm8);
    UT_ENSURE(tm8->expiration_time == ut_hw_time + 80);

    tm7->duration = 70;
    nsvc_timer_start(tm7);
    UT_ENSURE(tm7->expiration_time == ut_hw_time + 70);

    tm6->duration = 60;
    nsvc_timer_start(tm6);
    UT_ENSURE(tm6->expiration_time == ut_hw_time + 60);

    tm5->duration = 50;
    nsvc_timer_start(tm5);
    UT_ENSURE(tm5->expiration_time == ut_hw_time + 50);

    tm4->duration = 40;
    nsvc_timer_start(tm4);
    UT_ENSURE(tm4->expiration_time == ut_hw_time + 40);

    tm3->duration = 30;
    nsvc_timer_start(tm3);
    UT_ENSURE(tm3->expiration_time == ut_hw_time + 30);

    tm2->duration = 20;
    nsvc_timer_start(tm2);
    UT_ENSURE(tm2->expiration_time == ut_hw_time + 20);

    tm1->duration = 10;
    nsvc_timer_start(tm1);
    UT_ENSURE(tm1->expiration_time == ut_hw_time + 10);
    
    UT_ENSURE(nsvc_timer_queue_head == tm1);
    UT_ENSURE(nsvc_timer_queue_tail == tm9);
    UT_ENSURE(tm1->flink == tm2);
    UT_ENSURE(tm2->blink == tm1);
    UT_ENSURE(tm2->flink == tm3);
    UT_ENSURE(tm3->blink == tm2);
    UT_ENSURE(tm3->flink == tm4);
    UT_ENSURE(tm4->blink == tm3);
    UT_ENSURE(tm4->flink == tm5);
    UT_ENSURE(tm5->blink == tm4);
    UT_ENSURE(tm5->flink == tm6);
    UT_ENSURE(tm6->blink == tm5);
    UT_ENSURE(tm6->flink == tm7);
    UT_ENSURE(tm7->blink == tm6);
    UT_ENSURE(tm7->flink == tm8);
    UT_ENSURE(tm8->blink == tm7);
    UT_ENSURE(tm8->flink == tm9);
    UT_ENSURE(tm9->blink == tm8);

    nsvc_timer_kill(tm1);
    nsvc_timer_kill(tm2);
    nsvc_timer_kill(tm3);
    nsvc_timer_kill(tm4);
    nsvc_timer_kill(tm5);
    nsvc_timer_kill(tm6);
    nsvc_timer_kill(tm7);
    nsvc_timer_kill(tm8);
    nsvc_timer_kill(tm9);
    UT_ENSURE((NULL == tm1->flink) && (NULL == tm1->blink));
    UT_ENSURE((NULL == tm2->flink) && (NULL == tm2->blink));
    UT_ENSURE((NULL == tm3->flink) && (NULL == tm3->blink));
    UT_ENSURE((NULL == tm4->flink) && (NULL == tm4->blink));
    UT_ENSURE((NULL == tm5->flink) && (NULL == tm5->blink));
    UT_ENSURE((NULL == tm6->flink) && (NULL == tm6->blink));
    UT_ENSURE((NULL == tm7->flink) && (NULL == tm7->blink));
    UT_ENSURE((NULL == tm8->flink) && (NULL == tm8->blink));
    UT_ENSURE((NULL == tm9->flink) && (NULL == tm9->blink));
    UT_ENSURE(NULL == nsvc_timer_queue_head);
    UT_ENSURE(NULL == nsvc_timer_queue_tail);

    //**** [3] Non-Sequential insert
    //****

    ut_hw_time = 100;

    tm4->duration = 40;
    nsvc_timer_start(tm4);
    UT_ENSURE(tm4->expiration_time == ut_hw_time + 40);

    tm1->duration = 10;
    nsvc_timer_start(tm1);
    UT_ENSURE(tm1->expiration_time == ut_hw_time + 10);
    
    tm3->duration = 30;
    nsvc_timer_start(tm3);
    UT_ENSURE(tm3->expiration_time == ut_hw_time + 30);

    tm2->duration = 20;
    nsvc_timer_start(tm2);
    UT_ENSURE(tm2->expiration_time == ut_hw_time + 20);

    tm5->duration = 50;
    nsvc_timer_start(tm5);
    UT_ENSURE(tm5->expiration_time == ut_hw_time + 50);

    tm6->duration = 60;
    nsvc_timer_start(tm6);
    UT_ENSURE(tm6->expiration_time == ut_hw_time + 60);

    tm9->duration = 90;
    nsvc_timer_start(tm9);
    UT_ENSURE(tm9->expiration_time == ut_hw_time + 90);

    tm7->duration = 70;
    nsvc_timer_start(tm7);
    UT_ENSURE(tm7->expiration_time == ut_hw_time + 70);

    tm8->duration = 80;
    nsvc_timer_start(tm8);
    UT_ENSURE(tm8->expiration_time == ut_hw_time + 80);

    UT_ENSURE(nsvc_timer_queue_head == tm1);
    UT_ENSURE(nsvc_timer_queue_tail == tm9);
    UT_ENSURE(tm1->flink == tm2);
    UT_ENSURE(tm2->blink == tm1);
    UT_ENSURE(tm2->flink == tm3);
    UT_ENSURE(tm3->blink == tm2);
    UT_ENSURE(tm3->flink == tm4);
    UT_ENSURE(tm4->blink == tm3);
    UT_ENSURE(tm4->flink == tm5);
    UT_ENSURE(tm5->blink == tm4);
    UT_ENSURE(tm5->flink == tm6);
    UT_ENSURE(tm6->blink == tm5);
    UT_ENSURE(tm6->flink == tm7);
    UT_ENSURE(tm7->blink == tm6);
    UT_ENSURE(tm7->flink == tm8);
    UT_ENSURE(tm8->blink == tm7);
    UT_ENSURE(tm8->flink == tm9);
    UT_ENSURE(tm9->blink == tm8);

    nsvc_timer_kill(tm1);
    nsvc_timer_kill(tm2);
    nsvc_timer_kill(tm3);
    nsvc_timer_kill(tm4);
    nsvc_timer_kill(tm5);
    nsvc_timer_kill(tm6);
    nsvc_timer_kill(tm7);
    nsvc_timer_kill(tm8);
    nsvc_timer_kill(tm9);
    UT_ENSURE((NULL == tm1->flink) && (NULL == tm1->blink));
    UT_ENSURE((NULL == tm2->flink) && (NULL == tm2->blink));
    UT_ENSURE((NULL == tm3->flink) && (NULL == tm3->blink));
    UT_ENSURE((NULL == tm4->flink) && (NULL == tm4->blink));
    UT_ENSURE((NULL == tm5->flink) && (NULL == tm5->blink));
    UT_ENSURE((NULL == tm6->flink) && (NULL == tm6->blink));
    UT_ENSURE((NULL == tm7->flink) && (NULL == tm7->blink));
    UT_ENSURE((NULL == tm8->flink) && (NULL == tm8->blink));
    UT_ENSURE((NULL == tm9->flink) && (NULL == tm9->blink));
    UT_ENSURE(NULL == nsvc_timer_queue_head);
    UT_ENSURE(NULL == nsvc_timer_queue_tail);

    //**** [4] Wrapped insert
    //****

    ut_hw_time = 0xFFFFFFFFu - 100;
    offset = 100;

    tm9->duration = 190 + offset;
    nsvc_timer_start(tm9);
    UT_ENSURE(tm9->expiration_time == ut_hw_time + offset + 190);

    tm1->duration = 10;
    nsvc_timer_start(tm1);
    UT_ENSURE(tm1->expiration_time == ut_hw_time + 10);
    
    tm2->duration = 20 + offset;
    nsvc_timer_start(tm2);
    UT_ENSURE(tm2->expiration_time == ut_hw_time + offset + 20);

    tm3->duration = 30 + offset;
    nsvc_timer_start(tm3);
    UT_ENSURE(tm3->expiration_time == ut_hw_time + offset + 30);

    tm5->duration = 150 + offset;
    nsvc_timer_start(tm5);
    UT_ENSURE(tm5->expiration_time == ut_hw_time + offset + 150);

    tm6->duration = 160 + offset;
    nsvc_timer_start(tm6);
    UT_ENSURE(tm6->expiration_time == ut_hw_time + offset + 160);

    tm4->duration = 40 + offset;
    nsvc_timer_start(tm4);
    UT_ENSURE(tm4->expiration_time == ut_hw_time + offset + 40);

    tm7->duration = 170 + offset;
    nsvc_timer_start(tm7);
    UT_ENSURE(tm7->expiration_time == ut_hw_time + offset + 170);

    tm8->duration = 180 + offset;
    nsvc_timer_start(tm8);
    UT_ENSURE(tm8->expiration_time == ut_hw_time + offset + 180);

    UT_ENSURE(nsvc_timer_queue_head == tm1);
    UT_ENSURE(nsvc_timer_queue_tail == tm9);
    UT_ENSURE(tm1->flink == tm2);
    UT_ENSURE(tm2->blink == tm1);
    UT_ENSURE(tm2->flink == tm3);
    UT_ENSURE(tm3->blink == tm2);
    UT_ENSURE(tm3->flink == tm4);
    UT_ENSURE(tm4->blink == tm3);
    UT_ENSURE(tm4->flink == tm5);
    UT_ENSURE(tm5->blink == tm4);
    UT_ENSURE(tm5->flink == tm6);
    UT_ENSURE(tm6->blink == tm5);
    UT_ENSURE(tm6->flink == tm7);
    UT_ENSURE(tm7->blink == tm6);
    UT_ENSURE(tm7->flink == tm8);
    UT_ENSURE(tm8->blink == tm7);
    UT_ENSURE(tm8->flink == tm9);
    UT_ENSURE(tm9->blink == tm8);

    nsvc_timer_kill(tm1);
    nsvc_timer_kill(tm2);
    nsvc_timer_kill(tm3);
    nsvc_timer_kill(tm4);
    nsvc_timer_kill(tm5);
    nsvc_timer_kill(tm6);
    nsvc_timer_kill(tm7);
    nsvc_timer_kill(tm8);
    nsvc_timer_kill(tm9);
    UT_ENSURE((NULL == tm1->flink) && (NULL == tm1->blink));
    UT_ENSURE((NULL == tm2->flink) && (NULL == tm2->blink));
    UT_ENSURE((NULL == tm3->flink) && (NULL == tm3->blink));
    UT_ENSURE((NULL == tm4->flink) && (NULL == tm4->blink));
    UT_ENSURE((NULL == tm5->flink) && (NULL == tm5->blink));
    UT_ENSURE((NULL == tm6->flink) && (NULL == tm6->blink));
    UT_ENSURE((NULL == tm7->flink) && (NULL == tm7->blink));
    UT_ENSURE((NULL == tm8->flink) && (NULL == tm8->blink));
    UT_ENSURE((NULL == tm9->flink) && (NULL == tm9->blink));
    UT_ENSURE(NULL == nsvc_timer_queue_head);
    UT_ENSURE(NULL == nsvc_timer_queue_tail);

    // Skip timer free: next test should clean this up
}

void ut_timer_exercise_timeouts(void)
{
    nsvc_timer_t *tm1, *tm2, *tm3, *tm4, *tm5, *tm6, *tm7, *tm8, *tm9;
    nsvc_timer_callin_return_t rv = NSVC_TCRTN_DISABLE_QUANTUM_TIMER;
    uint32_t quantum_delay;
    unsigned reconfig_count = 0;
    unsigned i;

    nsvc_init();
    nsvc_timer_init(get_hw_time, NULL);

    nufr_running = NUFR_TID_TO_TCB(NUFR_TID_01);

    tm1 = nsvc_timer_alloc();
    tm2 = nsvc_timer_alloc();
    tm3 = nsvc_timer_alloc();
    tm4 = nsvc_timer_alloc();
    tm5 = nsvc_timer_alloc();
    tm6 = nsvc_timer_alloc();
    tm7 = nsvc_timer_alloc();
    tm8 = nsvc_timer_alloc();
    tm9 = nsvc_timer_alloc();

    copy_timer_defaults(tm1);
    copy_timer_defaults(tm2);
    copy_timer_defaults(tm3);
    copy_timer_defaults(tm4);
    copy_timer_defaults(tm5);
    copy_timer_defaults(tm6);
    copy_timer_defaults(tm7);
    copy_timer_defaults(tm8);
    copy_timer_defaults(tm9);

    //**** [1] Simple timeouts
    //****

    // Set time reference to a nominal value
    ut_hw_time = 100;

    tm1->duration = 10;
    nsvc_timer_start(tm1);
    
    tm2->duration = 20;
    nsvc_timer_start(tm2);

    tm3->duration = 30;
    nsvc_timer_start(tm3);

    tm4->duration = 40;
    nsvc_timer_start(tm4);

    tm5->duration = 50;
    nsvc_timer_start(tm5);

    tm6->duration = 60;
    nsvc_timer_start(tm6);

    tm7->duration = 70;
    nsvc_timer_start(tm7);

    tm8->duration = 80;
    nsvc_timer_start(tm8);

    tm9->duration = 90;
    nsvc_timer_start(tm9);

    // init to tm1 timeout
    quantum_delay = 10;

    for (i = 0; i < 100; i++)
    {
        nsvc_timer_callin_return_t rv;
        uint32_t reconfigured_time;

        ut_hw_time++;
        quantum_delay--;

        // Simulate quantum timer: expire after this long
        if (0 == quantum_delay)
        {
            rv = (nsvc_timer_callin_return_t)
                  nsvc_timer_expire_timer_callin(get_hw_time(),
                                       &reconfigured_time);

            // No task consuming message blocks. Purge them.
            nufr_msg_drain(NUFR_TID_02, NUFR_MSG_PRI_MID);

            if (NSVC_TCRTN_RECONFIGURE_QUANTUM_TIMER == rv)
            {
                quantum_delay = reconfigured_time;

                reconfig_count++;
            }
            else
            {
                break;
            }
        }
    }

    UT_ENSURE(NSVC_TCRTN_DISABLE_QUANTUM_TIMER == rv);
    UT_ENSURE(8 == reconfig_count);
    UT_ENSURE(NULL == nsvc_timer_queue_head);
    UT_ENSURE(NULL == nsvc_timer_queue_tail);

    //**** [1] Wrapped timeouts
    //****

    // Set time reference to a nominal value
    ut_hw_time = 0xFFFFFFFF;

    tm1->duration = 10;
    nsvc_timer_start(tm1);
    
    tm2->duration = 20;
    nsvc_timer_start(tm2);

    tm3->duration = 30;
    nsvc_timer_start(tm3);

    tm4->duration = 40;
    nsvc_timer_start(tm4);

    tm5->duration = 50;
    nsvc_timer_start(tm5);

    tm6->duration = 60;
    nsvc_timer_start(tm6);

    tm7->duration = 70;
    nsvc_timer_start(tm7);

    tm8->duration = 80;
    nsvc_timer_start(tm8);

    tm9->duration = 90;
    nsvc_timer_start(tm9);

    // init to tm1 timeout
    quantum_delay = 10;

    reconfig_count = 0;

    for (i = 0; i < 100; i++)
    {
        nsvc_timer_callin_return_t rv;
        uint32_t reconfigured_time;

        ut_hw_time++;
        quantum_delay--;

        // Simulate quantum timer: expire after this long
        if (0 == quantum_delay)
        {
            rv = (nsvc_timer_callin_return_t)
                  nsvc_timer_expire_timer_callin(get_hw_time(),
                                       &reconfigured_time);

            // No task consuming message blocks. Purge them.
            nufr_msg_drain(NUFR_TID_02, NUFR_MSG_PRI_MID);

            if (NSVC_TCRTN_RECONFIGURE_QUANTUM_TIMER == rv)
            {
                quantum_delay = reconfigured_time;

                reconfig_count++;
            }
            else
            {
                break;
            }
        }
    }

    UT_ENSURE(NSVC_TCRTN_DISABLE_QUANTUM_TIMER == rv);
    UT_ENSURE(8 == reconfig_count);
    UT_ENSURE(NULL == nsvc_timer_queue_head);
    UT_ENSURE(NULL == nsvc_timer_queue_tail);

}


void ut_nsvc_timers(void)
{
    ut_timer_active_list();
    ut_timer_expired_list();
    ut_timer_sorted_insert();
    ut_timer_exercise_timeouts();
}