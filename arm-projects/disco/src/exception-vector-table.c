#include <stdint.h>

#include <system.h>
#include <bsp.h>

#include "raging-global.h"

/*  Stack Pointer from linker script                                                                        */
extern uint32_t _estack;

/*  NUFR's PendSV Handler (nufr_context_switch) */
void nufr_context_switch(void);

/*  Inform the compiler that we want to use this as the isr_vector          */
__attribute__((section(".isr_vector")))
uint32_t const _vector_table[] =
{
    (uint32_t)&_estack,                  /* Top of the Stack                        */
    (uint32_t)&Reset_Handler,            /* Reset Handler                           */
    (uint32_t)&NMI_Handler,              /* NMI Handler                             */
    (uint32_t)&HardFault_Handler,        /* Hard Fault Handler                      */
    (uint32_t)&MemManage_Handler,        /*  MPU Fault Handler                      */
    (uint32_t)&BusFault_Handler,         /*  Bus Fault Handler                      */
    (uint32_t)&UsageFault_Handler,       /*  Usage Fault Handler                    */
    0,                                   /*  Reserved                               */
    0,                                   /*  Reserved                               */
    0,                                   /*  Reserved                               */
    0,                                   /*  Reserved                               */
    (uint32_t)&SVC_Handler,              /*  SVCall Handler                         */
    (uint32_t)&DebugMon_Handler,         /*  Debug Monitor Handler                  */
    0,                                   /*  Reserved                               */
    (uint32_t)nufr_context_switch,       /*    PendSV Handler                       */
    (uint32_t)&SysTick_Handler,          /*  SysTick Handler                        */

    /*                      STM32 Specific Interrupts                                           */
    0 /* 0 */,                              
    0 /* 1 */,                              
    0 /* 2 */,                              
    0 /* 3 */,                              
    0 /* 4 */,                              
    0 /* 5 */,                              
    0 /* 6 */,                              
    0 /* 7 */,                              
    0 /* 8 */,                              
    0 /* 9 */,                              
    0 /* 10 */,                             
    0 /* 11 */,                             
    0 /* 12 */,                             
    0 /* 13 */,                             
    0 /* 14 */,                             
    0 /* 15 */,                             
    0 /* 16 */,                             
    0 /* 17 */,                             
    0 /* 18 */,                             

            /*          STM32F42xxx/STM32F43xxx Specific Interrupts                 */
    0 /* 19 */,                                 
    0 /* 20 */,                                 
    0 /* 21 */,                                 
    0 /* 22 */,                                 
    0 /* 23 */,                                 
    0 /* 24 */,                                 
    0 /* 25 */,                                 
    0 /* 26 */,                                 
    0 /* 27 */,                                 
    0 /* 28 */,                                 
    0 /* 29 */,                                 
    0 /* 30 */,                                 
    0 /* 31 */,                                 
    0 /* 32 */,                                 
    0 /* 33 */,                                 
    0 /* 34 */,                                 
    0 /* 35 */,                                 
    0 /* 36 */,                                 
    0 /* 37 */,                                 
      /* 38 */(uint32_t)&bsp_uart_interrupt,
    0 /* 39 */,
    0 /* 40 */,                                 
    0 /* 41 */,                                 
    0 /* 42 */,                                 
    0 /* 43 */,                                 
    0 /* 44 */,                                 
    0 /* 45 */,                                 
    0 /* 46 */,                                 
    0 /* 47 */,                                 
    0 /* 48 */,                                 
    0 /* 49 */,                                 
    0 /* 50 */,                                 
    0 /* 51 */,                                 
    0 /* 52 */,                                 
    0 /* 53 */,                                 
    0 /* 54 */,                                 
    0 /* 55 */,                                 
    0 /* 56 */,                                 
    0 /* 57 */,                                 
    0 /* 58 */,                                 
    0 /* 59 */,                                 
    0 /* 60 */,                                 
    0 /* 61 */,                                 
    0 /* 62 */,                                 
    0 /* 63 */,                                 
    0 /* 64 */,                                 
    0 /* 65 */,                                 
    0 /* 66 */,                                 
    0 /* 67 */,                                 
    0 /* 68 */,                                 
    0 /* 69 */,                                 
    0 /* 70 */,                                 
    0 /* 71 */,                                 
    0 /* 72 */,                                 
    0 /* 73 */,                                 
    0 /* 74 */,                                 
    0 /* 75 */,                                 
    0 /* 76 */,                                 
    0 /* 77 */,                                 
    0 /* 78 */,                                 
    0 /* 79 */,                                 
    0 /* 80 */,                                 
    0 /* 81 */
};