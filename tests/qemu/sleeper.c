//!
//!  @brief       Sleeper project
//!

#include "nufr-platform-app.h"
#include "nufr-api.h"
#include "nufr-platform-export.h"

#if QEMU_PROJECT == 1

void entry_01(unsigned parm)
{

    unsigned i;
    uint32_t counter = 0;

    nufr_launch_task(NUFR_TID_02, 0xFF);
    nufr_launch_task(NUFR_TID_03, 0xFF);
    nufr_launch_task(NUFR_TID_04, 0xFF);
    nufr_launch_task(NUFR_TID_05, 0xFF);

    while (1)
    {
        counter++;
        if (counter >= parm)
        {
            counter = 0;
        }
        //nufr_yield();
        nufr_sleep(NUFR_MILLISECS_TO_TICKS(10), NUFR_MSG_MAX_PRIORITY);
    }
}

void entry_02(unsigned parm)
{
    uint32_t index = 0;
    uint32_t counter = 0;

    while (1)
    {
        counter++;

        if (counter >= parm)
        {
            counter = 0;
        }
        //nufr_yield();
        nufr_sleep(NUFR_MILLISECS_TO_TICKS(20), NUFR_MSG_MAX_PRIORITY);
    }
}
void entry_03(unsigned parm)
{
    uint32_t index = 0;
    uint32_t counter = 0;

    while (1)
    {
        counter++;

        if (counter >= parm)
        {
            counter = 0;
        }
        //nufr_yield();
        nufr_sleep(NUFR_MILLISECS_TO_TICKS(30), NUFR_MSG_MAX_PRIORITY);
    }
}
void entry_04(unsigned parm)
{
    uint32_t index = 0;
    uint32_t counter = 0;

    while (1)
    {
        counter++;

        if (counter >= parm)
        {
            counter = 0;
        }
        //nufr_yield();
        nufr_sleep(NUFR_MILLISECS_TO_TICKS(40), NUFR_MSG_MAX_PRIORITY);
    }
}
void entry_05(unsigned parm)
{
    uint32_t counter = 0;
    while (1)
    {
        counter++;

        //nufr_yield();
        nufr_sleep(NUFR_MILLISECS_TO_TICKS(100), NUFR_MSG_MAX_PRIORITY);
    }    
}

#endif