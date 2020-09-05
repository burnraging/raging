#include <system.h>
#include <bsp.h>

#define USING_EXCEPTION_CRASH_HANDLERS_C   // in exception-crash-handlers.c

#ifndef USING_EXCEPTION_CRASH_HANDLERS_C
unsigned hard_fault_count;
unsigned memmanage_fault_count;
unsigned bus_fault_count;
unsigned usage_fault_count;
#endif

void bsp_timer_decrement(void);

//void Reset_Handler(void)
//{
//    /* entry point into application */
//    extern int main(void);
//
//    /* initialization method for static constructors in C++ */
//    extern int __libc_init_array(void);
//
//    /* start of .data in the linker script */
//    extern unsigned __data_start;
//
//    /* end of .data in the linker script */
//    extern unsigned __data_end__;
//
//    /* initialization values for .data  */
//    extern unsigned const __data_load;
//
//    /* start of .bss in the linker script */
//    extern unsigned __bss_start__;
//
//    /* end of .bss in the linker script */
//    extern unsigned __bss_end__;
//
//    unsigned const *src;
//    unsigned *dst;
//
//    /* copy the data segment initializers from flash to RAM... */
//    src = &__data_load;
//    for (dst = &__data_start; dst < &__data_end__; ++dst, ++src)
//    {
//        *dst = *src;
//    }
//
//    /* zero fill the .bss segment in RAM... */
//    for (dst = &__bss_start__; dst < &__bss_end__; ++dst)
//    {
//        *dst = 0;
//    }
//
//    /* CMSIS system initialization */
//    SystemInit();
//
//    /* call all static constructors in C++ (harmless in C programs) */
//    __libc_init_array();
//
//    
//
//    /* application's entry point; should never return! */
//    (void)main();
// 
//  	while(true) 
//  	{
//
//  	}
//   
//}

void NMI_Handler(void)
{
}


#ifndef USING_EXCEPTION_CRASH_HANDLERS_C   // in exception-crash-handlers.c
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
      hard_fault_count++;
    bsp_led_toggle(Blue_Led);
  }
}


void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
      memmanage_fault_count++;
    bsp_led_toggle(Blue_Led);
  }
}


void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
      bus_fault_count++;
    bsp_led_toggle(Blue_Led);
  }
}


void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
      usage_fault_count++;
    bsp_led_toggle(Blue_Led);
  }
}
#endif

void SVC_Handler(void)
{
}


void DebugMon_Handler(void)
{
}


void PendSV_Handler(void)
{
}


// Using SysTick_Handler from ./platform/ARM_CMx/SysTick_Handler.c
void SysTick_Handler_stub(void)
{
    bsp_timer_decrement();
}



void handle_rx_byte(uint8_t byte)
{
    (void)byte;  

  
}
