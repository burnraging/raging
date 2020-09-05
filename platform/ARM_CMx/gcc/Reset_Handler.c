#include <stdint.h>

#include "nufr-platform-export.h"
#include "raging-utils-mem.h"

// Defined in assembly.s
void Set_PSP(uint32_t *psp_register); 
void Switch_To_Process_Stack(void);


void Reset_Handler(void)
{

    /* entry point into application */
    extern int main(void);

    /* initialization method for static constructors in C++ */
    extern int __libc_init_array(void);

    /* start of .data in the linker script */
    extern unsigned __data_start;

    /* end of .data in the linker script */
    extern unsigned __data_end__;

    /* initialization values for .data  */
    extern unsigned const __data_load;

    /* start of .bss in the linker script */
    extern unsigned __bss_start__;

    /* end of .bss in the linker script */
    extern unsigned __bss_end__;

    /* Start of Main Stack (MSP) */
    //extern unsigned _estack;

    unsigned length;
    //unsigned const *src;
    //unsigned *dst;

    /* copy the data segment initializers from flash to RAM... */
    //src = &__data_load;
    //for (dst = &__data_start; dst < &__data_end__; ++dst, ++src)
    //{
    //    *dst = *src;
    //}
    length = (unsigned) ((uint8_t *)&__data_end__ - (uint8_t *)&__data_start);
    rutils_memcpy(&__data_start, & __data_load, length);

    /* zero fill the .bss segment in RAM... */
    //for (dst = &__bss_start__; dst < &__bss_end__; ++dst)
    //{
    //    *dst = 0;
    //}
    length = (unsigned) ((uint8_t *)&__bss_end__ - (uint8_t *)&__bss_start__);
    rutils_memset(&__bss_start__, 0, length);

    /* Set the Process stack ptr to the bottom of the BG Task stack */
    /* NOTE: compiler tools probably won't like this as it goes over
             the end of the array
    */
    //asm("MSR psp, %0\n\t" "r" (&__Background_Stack));
    Set_PSP(&Bg_Stack[BG_STACK_SIZE/BYTES_PER_WORD32]);

    /* Switch over to Process stack */
    /*asm(
        "LDR R0, #1\n\t"
        "MSR R0, CONTROL\n\t"
        );
    */
    Switch_To_Process_Stack();

    /* Set the Main stack back to start of stack */
    //asm("MSR msp, %0\n\t" : : "r" (&_estack));
    //Set_MSP(&_estack);

    /* application's entry point; should never return! */
    (void)main();

    while (1)
    {
    }
}

/**
 * @}
 */
