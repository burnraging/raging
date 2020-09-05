#include <stdint.h>

#include "nufr-platform-export.h"

static volatile uint32_t tick_counter = 0;
/**
 *
 */
void SysTick_Handler(void)
{
    tick_counter++;
    nufrplat_systick_handler();
}


