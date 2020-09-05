#include <stdint.h>

#include <nufr-platform.h>

/*	Stack Pointer from linker script */
extern uint32_t _estack;

extern void nufr_context_switch(void);

void Default_Handler(void);  /* Default empty handler */

void Reset_Handler(void);    /* Reset Handler */


/* Cortex-M Processor fault exceptions... */
void NMI_Handler           (void) __attribute__ ((weak, naked));
void HardFault_Handler     (void) __attribute__ ((weak, naked));
void MemManage_Handler     (void) __attribute__ ((weak, naked));
void BusFault_Handler      (void) __attribute__ ((weak, naked));
void UsageFault_Handler    (void) __attribute__ ((weak, naked));

/* Cortex-M Processor non-fault exceptions... */
void SVC_Handler           (void) __attribute__ ((weak, naked));
void DebugMon_Handler      (void) __attribute__ ((weak, naked));
void PendSV_Handler        (void) __attribute__ ((weak, naked));
void SysTick_Handler       (void) __attribute__ ((weak, naked));

/*	Inform the compiler that we want to use this as the isr_vector				*/
__attribute__((section(".isr_vector")))
uint32_t const _vector_table[] =
    {
        (uint32_t)&_estack,					/*	Top of the Stack				*/
        (uint32_t)&Reset_Handler,			/*	Reset Handler					*/
        (uint32_t)&NMI_Handler,				/*	NMI Handler						*/
        (uint32_t)&HardFault_Handler,		/*	Hard Fault Handler				*/
        (uint32_t)&MemManage_Handler,		/*	MPU Fault Handler				*/
        (uint32_t)&BusFault_Handler,		/*	Bus Fault Handler				*/
        (uint32_t)&UsageFault_Handler,		/*	Usage Fault Handler				*/
        0,									/*	Reserved						*/
        0,									/*	Reserved						*/
        0,									/*	Reserved						*/
        0,									/*	Reserved						*/
        (uint32_t)&SVC_Handler,				/*	SVCall Handler					*/
        (uint32_t)&DebugMon_Handler,		/*	Debug Monitor Handler			*/
        0,									/*	Reserved						*/
        nufr_context_switch,              /*	PendSV Handler                  */
        (uint32_t)&SysTick_Handler			/*	SysTick Handler					*/

       
    };



