#include "nufr-platform-import.h"
#include "raging-global.h"
#include "raging-utils-mem.h"

#include <msp430.h>
#include "msp430-assembler.h"

// See http://www.ti.com/lit/an/slaa140/slaa140.pdf


//*** Compile switches passed in by the compiler

// MSP430X CPU compiled for 20-bit usage
//   It's mandatory that compiler only generate functions which
//   use CALLA/RETA
//#define CS_MSP430X_20BIT

// MSP430X-capable CPU compiled for 16-bit usage
//   It's mandatory that compiler only generate functions which
//   use CALL/RET
//#define CS_MSP430X_16BIT

// Pre-MSP430X/Legacy CPUs
//   If neither CS_MSP430X_20BIT or CS_MSP430X_16BIT is set, then
//   assembler uses only legancy/non-extended instructions.

//*** MSP430 Stack Frame conventions

// CALL instruction:
//
//      SP = SP + 2
//  0
//  2         PC bits 15:0

// CALLA instruction:
//
//       SP = SP + 4
//  0
//  2         PC bits 15:0
//  4         [12 bits unused] [PC bits 19:16]

//  Interrupt (5-Series/MSP430X-capable CPUs)
//
//       SP = SP + 4
//  0
//  2         [PC bits 19:16] [SR bits 11:0]
//  4         PC bits 15:0

//  Interrupt (2-Series/non-extended instruction CPUs)
//
//       SP = SP + 4
//  0
//  2         SR bits 16:0
//  4         PC bits 15:0


// NUFR Stack frame. Used for context switches.
// Frame is defined differently for 20-bit vs. 16-bit register sizes 
// Cannot use the EABI format for a stack frame, since this is
// incompatible with the order in which the MSP430X/extended inst
// PUSHM, POPM instructions work.
//
// NOTE:
// Not in accordance with new EABI specification,
// using registers according
// to "the GCC compiler for MSP" instead of "MSPGCC".
// (R11 is a save-on-call register)
// See http://www.ti.com/lit/an/slaa664/slaa664.pdf
//
// Points to EABI:
//  -- arguments passed starting with R12 and moving up to R15
//  -- for 32-bit arguments, R12+R13 and R14+R15 are used
//  -- R11 is a save on call register (like R12-R15)
//  -- R4-R10 are save on entry registers
//
typedef struct
{
    msp430_reg_t  R4;
    msp430_reg_t  R5;
    msp430_reg_t  R6;
    msp430_reg_t  R7;
    msp430_reg_t  R8;
    msp430_reg_t  R9;
    msp430_reg_t  R10;
    msp430_reg_t  R11;
    msp430_reg_t  R12;
    msp430_reg_t  R13;
    msp430_reg_t  R14;
    msp430_reg_t  R15;
    msp430_sr_reg_t SR;
    msp430_reg_t  PC;

    // This is beyond the frame: the exit point.
    msp430_reg_t  exit_fcn_ptr;
} msp430_frame_plus_exit_t;

// Populate values on a task stack prerequisite to launching it.
void Prepare_Stack(_IMPORT_stack_specifier_t *ptr)
{
    msp430_frame_plus_exit_t  *save_regs_ptr;
    unsigned                   offset_to_start_of_regs;
    uint8_t                   *start_of_regs_ptr;
    uint8_t                   *stack_base_ptr = (uint8_t *)ptr->stack_base_ptr;

    // offset in bytes to where frame will start
    offset_to_start_of_regs = ptr->stack_length_in_bytes -
                                    sizeof(msp430_frame_plus_exit_t);

    // Initial stack ptr to point to a full set of registers up
    // from the bottom of the stack
    start_of_regs_ptr = stack_base_ptr + offset_to_start_of_regs;

    // Set TCB's stack ptr to address of where R4 will be pushed
    *(ptr->stack_ptr_ptr) = (_IMPORT_REGISTER_TYPE *)(start_of_regs_ptr);

    // Clear from top of stack to where registers will be placed
    rutils_memset(stack_base_ptr,
                  0,
                  offset_to_start_of_regs);

    save_regs_ptr = (msp430_frame_plus_exit_t *)start_of_regs_ptr;

    save_regs_ptr->R4 = (msp430_reg_t)0x44444444;
    save_regs_ptr->R5 = (msp430_reg_t)0x55555555;
    save_regs_ptr->R6 = (msp430_reg_t)0x66666666;
    save_regs_ptr->R7 = (msp430_reg_t)0x77777777;
    save_regs_ptr->R8 = (msp430_reg_t)0x88888888;
    save_regs_ptr->R9 = (msp430_reg_t)0x99999999;
    save_regs_ptr->R10 = (msp430_reg_t)0xAAAAAAAA;
    save_regs_ptr->R11 = (msp430_reg_t)0xBBBBBBBB;
    save_regs_ptr->R12 = (msp430_reg_t)ptr->entry_parameter;
    save_regs_ptr->R13 = (msp430_reg_t)0xDDDDDDDD;
    save_regs_ptr->R14 = (msp430_reg_t)0xEEEEEEEE;
    save_regs_ptr->R15 = (msp430_reg_t)0xFFFFFFFF;
    save_regs_ptr->SR = msp430asm_get_sr();
    save_regs_ptr->PC = (msp430_reg_t)ptr->entry_point_fcn_ptr;
    save_regs_ptr->exit_fcn_ptr = (msp430_reg_t)ptr->exit_point_fcn_ptr;
}