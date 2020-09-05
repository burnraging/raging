#include "nufr-platform-import.h"
#include "raging-global.h"

#include "raging-utils-mem.h"

// ARM Cortex M CPU register typedef
// fixme: best to put this in another file
typedef uint32_t arm_reg_t;

// context saved by the hardware
typedef struct
{
    arm_reg_t R0;
    arm_reg_t R1;
    arm_reg_t R2;
    arm_reg_t R3;
    arm_reg_t R12;
    arm_reg_t LR;
    arm_reg_t PC;
    arm_reg_t PSR;
} reg_auto_save_t;

// context saved by PendSV
typedef struct
{
    arm_reg_t R4;
    arm_reg_t R5;
    arm_reg_t R6;
    arm_reg_t R7;
    arm_reg_t R8;
    arm_reg_t R9;
    arm_reg_t R10;
    arm_reg_t R11;
} reg_manual_save_t; 


void Prepare_Stack(_IMPORT_stack_specifier_t *ptr)
{
    reg_auto_save_t   *auto_save_regs_ptr;
    reg_manual_save_t *manual_save_regs_ptr;
    unsigned           offset_to_manual_save;
    unsigned           offset_to_auto_save;
    uint32_t          *stack_base_ptr = ptr->stack_base_ptr;

    // Initial stack ptr to point to a full set of registers up
    // from the bottom of the stack
    *(ptr->stack_ptr_ptr) = stack_base_ptr +  (
                                ptr->stack_length_in_bytes -
                                sizeof(reg_manual_save_t) -
                                sizeof(reg_auto_save_t)
                                                    ) / BYTES_PER_WORD32;


    // Auto save registers are at bottom (highest address value) in stack.
    // On top of auto saves are manual saves.
    offset_to_manual_save = ptr->stack_length_in_bytes
                                 - sizeof(reg_manual_save_t)
                                 - sizeof(reg_auto_save_t);
    offset_to_auto_save = ptr->stack_length_in_bytes
                                 - sizeof(reg_auto_save_t); 

    manual_save_regs_ptr = (reg_manual_save_t *)(
           &stack_base_ptr[offset_to_manual_save / BYTES_PER_WORD32]
                                                 );

    auto_save_regs_ptr = (reg_auto_save_t *)(
           &stack_base_ptr[offset_to_auto_save / BYTES_PER_WORD32]
                                                 );

    // Clear from top of stack to where registers will be placed
    rutils_memset(stack_base_ptr,
                  0,
                  offset_to_manual_save);

    manual_save_regs_ptr->R4 = 0x44444444;
    manual_save_regs_ptr->R5 = 0x55555555;
    manual_save_regs_ptr->R6 = 0x66666666;
    manual_save_regs_ptr->R7 = 0x77777777;
    manual_save_regs_ptr->R8 = 0x88888888;
    manual_save_regs_ptr->R9 = 0x99999999;
    manual_save_regs_ptr->R10 = 0xAAAAAAAA;
    manual_save_regs_ptr->R11 = 0xBBBBBBBB;
    auto_save_regs_ptr->R0 = (arm_reg_t)ptr->entry_parameter;
    auto_save_regs_ptr->R1 = 0x11111111;
    auto_save_regs_ptr->R2 = 0x22222222;
    auto_save_regs_ptr->R3 = 0x33333333;
    auto_save_regs_ptr->R12= 0xCCCCCCCC;
//    auto_save_regs_ptr->LR = (arm_reg_t)ptr->exit_point_fcn_ptr;
//    auto_save_regs_ptr->PC = (arm_reg_t)ptr->entry_point_fcn_ptr;
    auto_save_regs_ptr->LR = 0xDEADBEEF;   // not used!
    auto_save_regs_ptr->PC = 0xDEADBEEF;
    auto_save_regs_ptr->PSR = 1 << 24;     // MUST set thumb bit==1 or will fault!
}