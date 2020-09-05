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
// By Chris Martin

#ifndef _TASK_TESTS_H_
#define _TASK_TESTS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <nufr-global.h>
#include <nufr-platform.h>
#include <nufr-api.h>
#include <nufr-kernel-task.h>
#include <nufr-kernel-timer.h>
#include <nufr-kernel-semaphore.h>

#include <CUnit/CUnit.h>

#define READY_LIST_TEST_SUITE "Task Test Suite"

CU_ErrorCode ut_setup_task_tests(void);

void ut_launch_task(void);
void ut_launch_non_init_task(void);

/*  Ready List Insert Tests */
void ut_insert_before_head(void);
void ut_insert_nominal_after_causing_ready_list_walk(void);
void ut_insert_at_head_of_list_but_as_nominal(void);
void ut_insert_after_before_nominal_with_no_nominal_set(void);
void ut_insert_after_nominal_before_end(void);
void ut_insert_tail_last(void);
void ut_insert_before_nominal(void);
void ut_insert_at_ready_list_tail(void);
void ut_insert_nominal_to_ready_list_with_non_nominal_tail(void);
void ut_insert_two_nominal_tasks_in_ready_list(void);
void ut_insert_nominal_at_ready_list_head(void);
void ut_insert_at_ready_list_head(void);
void ut_insert_before_head(void);

/*  Ready List Delete Tests */
void ut_ready_list_delete_last_task(void);
void ut_ready_list_delete_from_multiple_nominal_tasks(void);
void ut_ready_list_delete_nominal_tail_from_multiple_nominal_tasks(void);
void ut_ready_list_delete_not_found_task(void);
void ut_ready_list_delete_running_task(void);
void ut_ready_list_delete_null_node(void);
void ut_ready_list_delete_at_tail(void);
void ut_ready_list_delete_between_nominal_and_tail(void);
void ut_ready_list_delete_nominal(void);
void ut_ready_list_delete_between_head_and_nominal(void);
void ut_ready_list_delete_at_head(void);
void ut_ready_list_delete_at_head_alternate(void);

/* Remove Head Tests */
void ut_remove_head_from_ready_list(void);
void ut_remove_head_from_single_task_list(void);
void ut_remove_last_nominal_from_task_list(void);

/*  Task Blocking Tests */

void ut_block_task(void);
void ut_block_last_running_nominal_task(void);

/*  Task Priority Tests */

void ut_make_running_task_highest_priority(void);
void ut_restore_single_task_priority(void);
void ut_task_set_priority_lowest(void);
void ut_task_set_priority_blocked_task(void);
void ut_task_set_priority_of_non_blocked_task(void);

/* Misc Tests */

void ut_exit_running_task(void);
void ut_exit_running_task_with_semaphore(void);

#endif
