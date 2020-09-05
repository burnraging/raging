#ifndef _TEST_HELPER_H_
#define _TEST_HELPER_H_

#include "raging-global.h"

#include <stdio.h>
#include <stdlib.h>
// These 3 are included in "raging-global.h"
//#include <stddef.h>
//#include <stdint.h>
//#include <stdbool.h>

#include <nufr-global.h>
#include <nufr-platform.h>
#include <nufr-api.h>
#include <nufr-kernel-task.h>
#include <nufr-kernel-timer.h>

void onTestRequireFailure(uint8_t* file, uint32_t line);
void onTestEnsureFailure(uint8_t* file, uint32_t line);

/**
 *	@brief This macro allows marking of a concept as a required precondition.
 *
 *	This macro provides a way to purposely mark an concept in code as a necessary
 *	precondition in the system.  If the expression passed to this macro is false, there will
 *	be a clear indication to the developer that the precondition has failed.
 *	The filename and line number in the file is provided via the onContractFailure
 *	callback method.
 *
 *	@param	x	The expression that is a required precondition.
 *
 *	@code{.c}
 *	void a_method(uint8_t* ptrToRequiredObject)
 *	{
 *		TEST_REQUIRE(NULL != ptrToRequiredObject);
 *		...
 *	}
 *
 *	@endcode
 *
 */
#define TEST_REQUIRE(x) ( (x) ? (void)0 : onTestRequireFailure((uint8_t*)__FILE__,__LINE__))

/**
 *	@brief This macro allows marking of a concept as a required postcondition.
 *
 *	This macro provides a way to purposely mark an concept in code as necessary
 *	postcondition in the system.  If the expression passed to this macro is false, there will
 *	be a clear indication to the developer that the postcondition has failed.
 *	The filename and line number in the file is provided via the onContractFailure
 *	callback method.
 *
 *	@param	x	The expression that is a required postcondition.
 *
 *	@code{.c}
 *	void a_method(uint8_t* ptrToRequiredObject)
 *	{
 *		...
 *		TEST_ENSURE(ptrToRequireObject->status_updated);
 *	}
 *
 *	@endcode
 *
 */
#define TEST_ENSURE(x) ( (x) ? (void)0 : onTestEnsureFailure((uint8_t*)__FILE__,__LINE__))

void ut_clean_list(void);

#endif
