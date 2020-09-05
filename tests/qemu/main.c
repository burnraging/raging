#include "nufr-global.h"
#include "nufr-api.h"
#include "nufr-platform-app.h"
#include "nufr-platform.h"
#include "nsvc-app.h"
#include "nsvc-api.h"
#include "nsvc.h"
#include "nufr-simulation.h"

#include "nufr-kernel-task.h"
#include "nufr-platform-import.h"
#include "nufr-platform-export.h"

#include "raging-global.h"
#include "raging-utils.h"
#include "raging-contract.h"

// for test code, see below
bool ut_armcmx_tests(void);

typedef struct 
{

  volatile unsigned int CTRL;

  volatile unsigned int RELOAD;   // accepts values of 1 - 0x00FFFFFF

  volatile unsigned int VAL;

  volatile unsigned int CALIB;   // read-only

} SysTickTemplate_t;

#define SysTick ( (volatile SysTickTemplate_t*) 0xE000E010 ) // as in the datasheet

// CTRL register bit definitions
#define CTRL_ENABLE   0x00000001  // bit  0:  Enables SysTick counting
#define CTRL_TICKINT  0x00000002  // bit  1:  Enables SysTick exception
#define CTRL_CLKSRC   0x00000004  // bit  2:  0 = external clock; 1 = core clock
#define CTRL_CNTFLAG  0x00010000  // bit 16:  Set to "1" when tickout occurs, cleared on read


// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dai0179b/ar01s02s08.html
void Initialize_SystemTick(void)
{
    SysTick->CTRL = CTRL_CLKSRC;    //use core clock

//    SysTick->LOAD = 8000000; /* Frequency of 1 Hz */
    // 64 bit math prevents intermediate 32 bit overflow by multiply op
    SysTick->RELOAD = (uint32_t) ((uint64_t)_IMPORT_CPU_CLOCK_SPEED * NUFR_TICK_PERIOD
                         / MILLISECS_PER_SEC) - 1;

    SysTick->CTRL |= (CTRL_ENABLE | CTRL_TICKINT); // enable counting, interrupts
}

void InitPendSVSysTickPriorities(unsigned sysTick_priority, unsigned pendSV_priority)
{
// SHPR3 register
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0395b/CIHJHFJD.html
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0552a/CIAGECDD.html
    volatile uint32_t *SHPR3_ptr = (volatile uint32_t *)(0xE000ED20);
    uint32_t x;

    x = *SHPR3_ptr;

    // Keep other bits the same
    x &= 0x0000FFFF;
    x |= ((sysTick_priority & _IMPORT_PRIORITY_MASK) << 24);
    x |= ((pendSV_priority & _IMPORT_PRIORITY_MASK) << 16);

    *SHPR3_ptr = x;
}

void* background_task(void* ptr)
{
    unsigned int counter = 0;

    UNUSED(ptr);

    while(1)
    {
        counter++;

        if(counter >= 0x5000)
        {
            counter = 0;
        }

        
    }
}

int main() 
{

    // Test code, keep compiled out
#if 0
    ut_armcmx_tests();
#endif

    // Always call nufr_init before enabling PendSV or SysTick
    nufr_init();

    // Called after nufr_init(). Can be later, doesn't have to be here.
    nsvc_init();

    // Call after nsvc_init()
    nsvc_timer_init(nufrplat_systick_get_reference_time, NULL);

    // SysTick's priority should be the same or greater than PendSV's in
    //  order to guarantee tail chaining on context switches triggered
    //  by SysTick.
    //
    // Here SysTick is set to same priority as PendSV, but at a lower sub-priority.
    // This saves priority levels on a 3-bit scheme. Side-effect of adding
    // a bit more latency occasionally to SysTick in certain corner-cases.
    InitPendSVSysTickPriorities(_IMPORT_INT_PRI_3, _IMPORT_INT_PRI_3_SUB);

    // Always start SysTick before launching any tasks
    Initialize_SystemTick();

    nufr_launch_task(NUFR_TID_01, 0xFF);


    background_task(NULL);

    return 0;    
}
