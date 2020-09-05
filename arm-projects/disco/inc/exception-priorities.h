#ifndef EXCEPTION_PRIORITIES_H_
#define EXCEPTION_PRIORITIES_H_

#include <bsp.h>

#include "raging-global.h"
//
// With 4 bits (__NVIC_PRIO_BITS == 4) configurable:
//    Configure 3 bits for group priority and 1 bit for subpriority
// With 3 bits (__NVIC_PRIO_BITS == 3) configurable:
//    Configure 3 bits for group priority and 0 bits for subpriority
// In this way, we can have a common setup for M0 - M4
//
// See this link for AIRCR priority group bits:
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0553a/Cihehdge.html
#define AIRCR_PRIORITY_GROUP    4

// Configurable interrupt priority levels and subpriority levels
// based on a 2 bit priority + 1 bit subpriority scheme

// Configurable interrupt priority group/preempt level.
// Based on a 3-bit preempt field.
//
// 'EXC'-- Fault Exception
// 'IRQ'-- Interrupt
// 'SWE'-- Software Exception/Other Exception

#if __NVIC_PRIO_BITS == 2
// Cortex M0 default, typical
typedef enum
{
    INT_PRL_EXC_LOW = 0,

    // BASEPRI mask-off level (0x40). This is applied by int_lock()
    //  in nufr-platform-import.h, which is applied to
    //  NUFR_LOCK_INTERRUPTS()
    // All levels below will be blocked by int_lock()
    // FIXME!! REQUIRES CHANGE IN int_lock() macro
    INT_PRL_IRQ_HIGHEST = 1,
    INT_PRL_IRQ_MID = 2,
    INT_PRL_SWE_LOW = 3,
} bsp_prl_t;       // group (preempt) priority level

#elif __NVIC_PRIO_BITS >= 3
// Cortex M3 and M4 defaults, typical
//   M3 == 3; M4 == 4
typedef enum
{
    INT_PRL_EXC_HIGH = 0,
    INT_PRL_EXC_LOW = 1,
    INT_PRL_IRQ_HIGHEST = 2,

    // BASEPRI mask-off level (0x60). This is applied by int_lock()
    //  in nufr-platform-import.h, which is applied to
    //  NUFR_LOCK_INTERRUPTS()
    // All levels below will be blocked by int_lock()
    INT_PRL_IRQ_HIGHER = 3,
    INT_PRL_IRQ_MID = 4,
    INT_PRL_IRQ_LOW = 5,
    INT_PRL_SWE_MID = 6,
    INT_PRL_SWE_LOW = 7
} bsp_prl_t;       // group (preempt) priority level

#else
    #error "__NVIC_PRIO_BITS undefined or value is 0 or 1, which is not supported"
#endif

// Configurable interrupt sub-priority level.
// Based on a 1-bit sub-priority config.
typedef enum
{
    INT_SUBL_MID = 0,
    INT_SUBL_LOW = 1
} bsp_subl_t;

typedef struct
{
    IRQn_Type   irq;
    bsp_prl_t   preempt_priority;
    bsp_subl_t  sub_priority;
} bsp_irq_settings_t;

// APIs
const bsp_irq_settings_t *ep_get_irq_priority_table(unsigned *table_length_ptr);

#endif  // EXCEPTION_PRIORITIES_H_