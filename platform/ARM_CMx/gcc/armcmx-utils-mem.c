/*
Copyright (c) 2019, Bernie Woodland
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//! @file     armcmx-assembler.c
//! @authors  Bernie Woodland
//! @date     14Apr19
//!
//! @brief    Assembly language routine alternatives
//!
//! @details  Fulfills fcns. in 'raging-utils-mem.h'
//!

#include "raging-global.h"
#include "raging-utils-mem.h"

#ifdef OPTIMIZED_FOR_SPEED_OVER_CODESPACE

//! @details  For OPTIMIZED_FOR_SPEED_OVER_CODESPACE:
//! @details  Together, rutils_memset() and rutils_memcpy()
//! @details  consume around 425 bytes. Optimized for speed
//! @details  at the expense of code space.

//!
//! @name      rutils_memset
//!
//! @brief     ARM-assembler optimized version of rutils_memset()
//!
//! @details   This is 100% compatible from caller's perspective
//! @details   As straight-C 'rutils_memset()'
//!
//! @param[in] 'dest_str'-- address where to start memset op to
//! @param[in] 'set_value'-- value to poke in
//! @param[in] 'length'-- number of times 'value' is to be set
//!
__attribute__((naked)) void rutils_memset(void    *dest_str,
                                          uint8_t  set_value,
                                          unsigned length)
{
    // Caller puts parameters in these caller-save registers:
    // 
    // r0 <-- 'dest_str'
    // r1 <-- 'set_value'
    // r2 <-- 'length'
    //
    // We'll use this/these caller-save registers in addition:
    // r3 <-- temp register
    //
    // For long values of 'length' we'll use callee-saved
    // registers r4-r11.

    // ***** Check corner case: length <= 3

    // r0: 'dest_str'
    // r1: 'set_value'
    // r2: 'length'
    // r3: (unassigned)

    __asm volatile goto(
        "  cmp    r2, #3\n\t"            // 'length' <= 3?
        "  bls    %l[slow_set]\n\t"      // Yes--branch
        : 
        :
        :
        : slow_set);

    // ***** Do unaligned single-byte writes, 0-3 bytes

    // r0-r3: (unchanged)

    __asm volatile goto(
        // Is 'dest_str' unaligned on a 32-bit boundary?
        // If so, copy from 1-3 bytes over, just enough
        // to bump 'dest_str' up to next aligned address
        "  ands   r3, r0, #3\n\t"        // bits 0,1 == 0?
        "  beq    %l[no_unaligneds]"     // yes--branch
        : 
        :
        :
        : no_unaligneds);

    // r0: 'dest_str', an unaligned value
    // r1: 'set_value'
    // r2: 'length'
    // r3:  bits0,1 of 'dest_str'. r3 != 0

    __asm volatile goto(
        // Calculate the num bytes to get up to alignment
        "  eor    r3, r3, #3\n\t"        // 1-complement of bits0,1 only
        "  add    r3, r3, #1\n\t"        // make 2's complement, r3 must be 1-3
        "  lsrs   r3, r3, #1\n\t"        // Rotate bit0 into C flag
        "  bcc    %l[no_unaligned0]\n\t" // C flag==0? branch if yes
        "  strb   r1, [r0], #1\n\t"      // Set 1st unaligned byte
                                         // Inc. r0 after write
        "  sub    r2, r2, #1"            // Dec. 'length'
        : 
        :
        :
        : no_unaligned0);

    // r0: 'dest_str', but aligned up to next 16-bit alignment
    // r1: 'set_value'
    // r2: 'length' remaining, 0 or 1 less than original 'length'
    // r3:  align-up count. will be one of 0 or 2
    //             If 2, then r0 must be aligned up another 16-bits

no_unaligned0:
    __asm volatile goto(
        "  lsrs   r3, r3, #1\n\t"        // Rotate original bit1 to C flag
        "  bcc    %l[no_unaligneds]\n\t" // C flag==0? branch if yes
        "  strb   r1, [r0], #1\n\t"      // Set 2 unaligned bytes
        "  strb   r1, [r0], #1\n\t"      // ...incr'ing r0 each write
        "  sub    r2, r2, #2"            // Then dec. 'length' by 2
        : 
        :
        :
        : no_unaligneds, exit);

    // **** Duplicate 'set_value' out to 32-bits, if necessary

    // r0: 'dest_str', aligned up to next 32-bit alignment
    // r1: 'set_value'
    // r2: 'length' remaining (0-3 less than original)
    // r3:  (unassigned)

no_unaligneds:
    __asm volatile goto(
        // 'length' was updated to reflect alignment writes (if any)
        // If new 'length'==0, take early exit
        "  orrs   r2, r2, r2\n\t"        // no op to test 'length'
        "  beq    %l[exit]\n\t"          // if zero, we're done

        // Most of the time, caller will use 'set_value'==0.
        // However, if 'set_value'!=0, then we have to duplicate it,
        //  since we'll be doing 32-bit writes now.
        // Duplication copies 3 sets of bits 0-7 into bits
        // 8-15, 16-23, 24-31, and stuffs it back into r1
        "  orrs   r1, r1, r1\n\t"        // no op to test 'set_value'
        "  beq    %l[setting_zero]\n\t"  // 'set_value'==0? If yes, branch
        "  lsl    r3, r1, #8\n\t"        // Copy bits 0-7 to bits 8-15
        "  orr    r1, r1, r3\n\t"        //
        "  lsl    r3, r1, #16\n\t"       // Copy bits 0-15 to bits 16-31
        "  orr    r1, r1, r3"            //
        : 
        :
        :
        : setting_zero, exit);

    // **** Do sets of 32-bit writes

    // r0: 'dest_str', aligned up to next 32-bit alignment
    // r1: 'set_value' duplicated to 32-bits
    // r2: 'length' remaining (0-3 less than original)
    // r3:  (unassigned)

setting_zero:
    __asm volatile goto(
        // Now that 'dest_str' is aligned and 'set_value' is expanded
        // to 32-bit equivalent of caller's 8-bit value,
        // we can do 32-bit writes now.
        //
        // Set r3 to number of 32-bit writes.
        // This will be 'length" >> 2
        // First shift is 3 bits, not 2, to put original bit2 into C flag
        "  lsrs   r3, r2, #3\n\t"        // Shift 'length' 3 bits to right
        "  bcc    %l[no_single32]\n\t"   // Was original bit2==0? If yes, br
        "  str    r1, [r0], #4"          // Store a single 32-bit value
        : 
        :
        :
        : no_single32);

    // r0: 'dest_str' offsetted 0-7 bytes
    // r1: 'set_value' duplicated to 32-bits
    // r2: 'length' remaining (0-3 less than original)
    // r3:  num 8-byte sets remaining

no_single32:
    __asm volatile goto(
        // Was original r3 bit1 set? If so, then do 2 32-bit writes
        "  beq    %[no_more_octs]\n\t"   // Finished with all 32-bit writes?
                                         // if yes, branch
        "  lsrs   r3, r3, #1\n\t"        //
        "  bcc    %l[no_double32]\n\t"   //
        "  str    r1, [r0], #4\n\t"      //
        "  str    r1, [r0], #4"          //
        : 
        :
        :
        : no_double32, no_more_octs);

    // r0: 'dest_str' offsetted 0-15 bytes
    // r1: 'set_value' duplicated to 32-bits
    // r2: 'length' remaining (0-3 less than original)
    // r3:  num 16-byte sets remaining

no_double32:
    __asm volatile goto(
        // Was original r3 bit2 set? If so, then do 4 32-bit writes
        "  beq    %[no_more_octs]\n\t"   // Finished with all 32-bit writes?
                                         // if yes, branch
        "  lsrs   r3, r3, #1\n\t"        //
        "  bcc    %l[no_quad32]\n\t"     //
        "  str    r1, [r0], #4\n\t"      //
        "  str    r1, [r0], #4\n\t"      //
        "  str    r1, [r0], #4\n\t"      //
        "  str    r1, [r0], #4"          //
        : 
        :
        :
        : no_quad32, no_more_octs);

    // *** Prepare for oct writes

    // r0: 'dest_str' offsetted 0-31 bytes
    // r1: 'set_value' duplicated to 32-bits
    // r2: 'length' remaining (0-3 less than original)
    // r3:  num octs (32-byte sets) remaining

no_quad32:
    __asm volatile goto(
        // If r3 was originally >= 8, then do groups of 8 (octals)
        // Recall that r3's value is now num. of octals to do
        // We'll kick this into high gear by using stm instr.
        "  orrs   r3, r3, r3\n\t"        // Test r3
        "  beq    %l[no_more_octs]\n\t"  // r3==0? If yes, skip octals
        "  stmfd  sp!, {r4-r11}\n\t"     // Push working registers
        "  mov    r4, r1\n\t"            // Manually assign working registers
        "  mov    r5, r1\n\t"            //
        "  mov    r6, r1\n\t"            //
        "  mov    r7, r1\n\t"            //
        "  mov    r8, r1\n\t"            //
        "  mov    r9, r1\n\t"            //
        "  mov    r10, r1\n\t"           //
        "  mov    r11, r1"               //
        : 
        :
        :
        : no_more_octs);

    // **** Do sets of oct writes

    // r0-r4: (no change)

    __asm volatile goto(
        // Was original r3 bit1 set? If so, then do 1 oct
        "  lsrs   r3, r3, #1\n\t"        //
        "  bcc    %l[no_single_oct]\n\t" //
        "  stmia  r0!, {r4-r11}"         // Set a single octal. Inc 'dest_str'
        : 
        :
        :
        : no_single_oct);

    // r0: 'dest_str' offsetted 0-63 bytes
    // r1: 'set_value' duplicated to 32-bits
    // r2: 'length' remaining (0-3 less than original)
    // r3:  num 2-octs (64-bytes) remaining

no_single_oct:
    __asm volatile goto(
        // Was original r3 bit2 set? If so, then do 2 octs
        "  beq   %l[restore_regs]\n\t"   // Done early with octs?
        "  lsrs   r3, r3, #1\n\t"        //
        "  bcc    %l[no_double_oct]\n\t" //
        "  stmia  r0!, {r4-r11}\n\t"     // Set a single octal. Inc 'dest_str'
        "  stmia  r0!, {r4-r11}"         // Set a single octal. Inc 'dest_str'
        : 
        :
        :
        : no_double_oct, restore_regs);

    // r0: 'dest_str' offsetted 0-127 bytes
    // r1: 'set_value' duplicated to 32-bits
    // r2: 'length' remaining (0-3 less than original)
    // r3:  num quad octs (128-bytes) remaining

no_double_oct:
    __asm volatile goto(
        "  beq    %l[restore_regs]"      // Done early with octs?
        : 
        :
        :
        : restore_regs);

    // r0-r3: (no change)

loop_again:
    __asm volatile goto(
        // Do quad octals
        "  stmia  r0!, {r4-r11}\n\t"     // Set a single octal. Inc 'dest_str'
        "  stmia  r0!, {r4-r11}\n\t"     // Set a single octal. Inc 'dest_str'
        "  stmia  r0!, {r4-r11}\n\t"     // Set a single octal. Inc 'dest_str'
        "  stmia  r0!, {r4-r11}\n\t"     // Set a single octal. Inc 'dest_str'
        "  subs   r3, r3, #1\n\t"        // Decr. quad octal count
        "  bne    %l[loop_again]"        // octal count==0? If no, loop again
        : 
        :
        :
        : loop_again);

restore_regs:
    __asm volatile(
        // Looping finished
        "  ldmfd  sp!, {r4-r11}"         // Restore working registers
        : );

    // *** Do dangling odd bytes at end. 0-3 remaining.

    // r0: 'dest_str' offsetted to 0-3 less than 'length'
    // r1: 'set_value' duplicated to 32-bits
    // r2: 'length' remaining (0-3 less than original)
    // r3:  (unassigned)

no_more_octs:
    __asm volatile goto(
        // There could be 1-3 remaining bytes to be copied, if
        // 'length' was an odd value or if we were unaligned
        // Finish setting these dangling bytes
        //
        // r2 is now 'length' - (num. sets to reach alignment)
        // Num. dangling bytes is value in bits0,1
        "  ands   r3, r2, #3\n\t"        // Keep only bits0,1
        "  lsrs   r3, r3, #1\n\t"        // C flag <== original bit 0
        "  bcc    %l[no_danglings0]\n\t" // Bit 0==0? If yes, branch
        "  strb   r1, [r0], #1"          // Write single byte, then inc. r0
        : 
        :
        :
        : no_danglings0);

    // r0: 'dest_str' offsetted to 0 or 2 less than 'set_value'
    // r1: 'set_value' duplicated to 32-bits
    // r2: 'length' remaining (0-3 less than original)
    // r3: 'length' remaining (0 or 2)

no_danglings0:
    __asm volatile goto(
        // NOTE:
        // 'bxeq' is not a legal thumb mode instr., actual instr. inserted
        // is 'bx'. So 'it' instr. is necessary (and enforced by assembler)
        "  it     eq\n\t"                // Done already?
        "  bxeq   lr\n\t"                //
        "  lsrs   r3, r3, #1\n\t"        // C flag <== original bit 1
        "  bcc    %l[exit]\n\t"          // Orig. bit 1==0? If yes, branch
        "  strb   r1, [r0], #1\n\t"      // Do 2 writes
        "  strb   r1, [r0], #1"          //
        : 
        :
        :
        : exit);

exit:
    __asm volatile(
        // Exit for all paths, except 'length' == 1, 2, or 3
        "  bx     lr"                  // Return from this fcn call
        :);

    // ***** Brute force set 1 byte at a time
    //    Painfully slow, but always works

    // r0: 'dest_str'
    // r1: 'set_value'
    // r2: 'length'. Value from 0-3
    // r3: (unassigned)

slow_set:
    __asm volatile goto(
        // Sanity check for zero length
        "  orrs   r2, r2, r2\n\t"        // test 'length'
        "  beq    %l[exit]\n\t"          // br if == 0
        : 
        :
        :
        : exit);

    // r0: 'dest_str'
    // r1: 'set_value'
    // r2: 'length' Value from 1-3
    // r3: (unassigned)

slow_set_loop:
    __asm volatile goto(
        // Manually write all registers
        "  strb   r1, [r0], #1\n\t"      // Set 1 byte
        "  subs   r2, #1\n\t"            // Decrement 'length'
        "  bne    %l[slow_set_loop]\n\t" // 'length' > 0? yes--loop again
        "  bx     lr"                    //
        : 
        :
        :
        : slow_set_loop);

}

//!
//! @name      rutils_memcpy
//!
//! @brief     ARM-optimized memcpy
//!
//! @details   Using move-multiple-register instructions, 
//! @details   efficiently memcpy 'src_addr' to 'dest_addr'
//! @details   'length' times.
//!
//! @param[in] 'dest_addr'-- address where to start memset op to
//! @param[in] 'src_addr'-- address where to start memset op to
//! @param[in] 'length'-- number of 8-bit copies to do
//!
__attribute__((naked)) void rutils_memcpy(void       *dest_str,
                                          const void *src_str,
                                          unsigned    length)
{
    // Caller puts parameters in these caller-save registers:
    // 
    // r0 <-- 'dest_str'
    // r1 <-- 'src_str'
    // r2 <-- 'length'
    //
    // We'll use this/these caller-save registers in addition:
    // r3 <-- temp register
    //
    // For a moment, we need to borrow r4 early because
    // we're out of registers.
    //
    // For long values of 'length' we'll use callee-saved
    // registers r4-r11.

    // r0: 'dest_str'
    // r1: 'src_str'
    // r2: 'length'
    // r3: (unassigned)

    // ***** Check corner case: length <= 3

    __asm volatile goto(
        "  cmp    r2, #3\n\t"            // 'length' <= 3?
        "  bls    %l[unaligned]\n\t"     // Yes--branch
        : 
        :
        :
        : unaligned);

    // ***** Check for alignment

    // r0-r3: (unchanged)

    __asm volatile goto(
        // Are either bits0,1 of either 'dest_str' or 'src_str' set?
        // If so, then this is an unaligned copy.
        // Will do unaligned copies 1 byte at a time.
        "  orr    r3, r0, r1\n\t"        // 
        "  ands   r3, r3, #3\n\t"        // Is unaligned if bit0 or 1 set
        "  bne    %l[unaligned]\n\t"     // Unaligned? Yes--branch
        "  stmfd  sp!, {r4}"             // Early push of working register
        : 
        :
        :
        : unaligned);

        // *** Do an aligned copy

    // r0-r3: (unchanged)
    // r4: (unassigned)

    __asm volatile goto(
        // Shift 'length' to right 2 bits to get number of 32-bit copies.
        // Shift r3 a 3rd time to load original bit2 of 'length' into C flag
        "  lsrs   r3, r2, #3\n\t"        // Load C flag with bit 0
        "  bcc    %l[no_single32]\n\t"   // Was original bit2==0? If yes, br
        "  ldr    r4, [r1], #4\n\t"      // Single 32-bit copy...
        "  str    r4, [r0], #4"          // ...inc. both r1,r2 after copy
        : 
        :
        :
        : no_single32);

    // r0: 'dest_str' + 0 or 4
    // r1: 'src_str' + 0 or 4
    // r2: 'length'
    // r3: num. 8-byte sets to copy
    // r4: (unassigned)

no_single32:
    __asm volatile goto(
        "  beq    %l[no_oct32]\n\t"      // if yes, do scraps
        "  lsrs   r3, r3, #1\n\t"        // C flag <== original bit 3
        "  bcc    %l[no_double32]\n\t"   // Was original bit3==0? If yes, br
        "  ldr    r4, [r1], #4\n\t"      // 2 (double) Load/Stores
        "  str    r4, [r0], #4\n\t"      //
        "  ldr    r4, [r1], #4\n\t"      //
        "  str    r4, [r0], #4"          //
        : 
        :
        :
        : no_double32, no_oct32);

    // r0: 'dest_str' + 0, 4, 8, or 12
    // r1: 'src_str' + 0, 4, 8, or 12
    // r2: 'length'
    // r3: num. 16-byte sets to copy
    // r4: (unassigned)

no_double32:
    __asm volatile goto(
        "  beq    %l[no_oct32]\n\t"      // if yes, do scraps
        "  lsrs   r3, r3, #1\n\t"        // C flag <== original bit 4
        "  bcc    %l[no_quad32]\n\t"     // Was original bit4==0? If yes, br
        "  ldr    r4, [r1], #4\n\t"      // 4 (quad) Load/Stores
        "  str    r4, [r0], #4\n\t"      //
        "  ldr    r4, [r1], #4\n\t"      //
        "  str    r4, [r0], #4\n\t"      //
        "  ldr    r4, [r1], #4\n\t"      //
        "  str    r4, [r0], #4\n\t"      //
        "  ldr    r4, [r1], #4\n\t"      //
        "  str    r4, [r0], #4"          //
        : 
        :
        :
        : no_quad32, no_oct32);

    // r0: 'dest_str' + 0, 4, 8, ... 28
    // r1: 'src_str' + 0, 4, 8, ... 28
    // r2: 'length'
    // r3: num. 32-byte sets to copy
    // r4: (unassigned)

no_quad32:
    __asm volatile goto(
        "  orrs   r3, r3, r3\n\t"        // Test r3
        "  beq    %l[no_oct32]\n\t"      // Was original bit5==0? If yes, br
        "  stmfd  sp!, {r5-r11}"         // Push remaining working registers
        : 
        :
        :
        : no_oct32);

    // r0: 'dest_str' + 0, 4, 8, ... 28
    // r1: 'src_str'  0-3 bytes from 'length'
    // r2: 'length'
    // r3: (unassigned)
    // r4: (unassigned)

loop_again:
    __asm volatile goto(
        // Do 8 32-bit (octal) copies
        // r3 is octal count
        "  ldmia  r1!, {r4-r11}\n\t"     //
        "  stmia  r0!, {r4-r11}\n\t"     // Set a single octal. Inc 'dest_str'
        "  subs   r3, r3, #1\n\t"        // Decr. octal count
        "  bne    %l[loop_again]\n\t"    // octal count==0? If no, loop again

        // We're done with octals
        "  ldmfd  sp!, {r5-r11}"         // Restore remaining working registers
        : 
        :
        :
        : loop_again);

    // r0: 'dest_str' 0-3 bytes from 'length'
    // r1: 'src_str'  0-3 bytes from 'length'
    // r2: 'length'
    // r3: (unassigned)
    // r4: (unassigned)

no_oct32:
    __asm volatile(
        "  ldmfd  sp!, {r4}\n\t"         // Restore early push working register
        "  and    r2, r2, #3\n\t"        // Change 'length' to be only
                                         // ...remaining values (must be 0-3)
        :);

        // *** Finish the 1-3 odd bytes from an aligned copy,
        // ***  or do a brute-force byte-by-byte copy for an unaligned.

    // r0: 'dest_str' current
    // r1: 'src_str'  current
    // r2: 'length' remaining: 0-3 bytes if started aligned;
    //     original 'length' if not
    // r3: (unassigned)

unaligned:
    __asm volatile goto(
        // Sanity check
        "  orrs   r2, r2, r2\n\t"        // Test r2
        "  beq    %l[exit]\n\t"          // if r2==0, we're done (branch)
        // Do single byte writes
        "  lsrs   r2, r2, #1\n\t"        // 
        "  bcc    %l[no_single8]\n\t"    //
        "  ldrb   r3, [r1], #1\n\t"      // Single byte copy
        "  strb   r3, [r0], #1"          //
        :
        :
        :
        : no_single8, exit);

    // r0: 'dest_str' current
    // r1: 'src_str'  current
    // r2: 'length'/2 remaining
    // r3: (unassigned)

no_single8:
    __asm volatile goto(
        // Do double byte writes
        "  beq    %l[exit]\n\t"          // if r2==0, we're done (branch)
        "  lsrs   r2, r2, #1\n\t"        // 
        "  bcc    %l[no_double8]\n\t"    //
        "  ldrb   r3, [r1], #1\n\t"      // Double byte copy
        "  strb   r3, [r0], #1\n\t"      //
        "  ldrb   r3, [r1], #1\n\t"      //
        "  strb   r3, [r0], #1"          //
        :
        :
        :
        : no_double8, exit);

    // r0: 'dest_str' current
    // r1: 'src_str'  current
    // r2: 'length'/4 remaining
    // r3: (unassigned)

no_double8:
    __asm volatile goto(
        // Finished yet?
        "  beq    %l[exit]\n\t"          // if r2==0, we're done (branch)
        :
        :
        :
        : exit);

    // r0-r3: (no change)

loop_again_brute_force:
    __asm volatile goto(
        // Byte-by-byte copy
        "  ldrb   r3, [r1], #1\n\t"      // Quad byte copy
        "  strb   r3, [r0], #1\n\t"      //
        "  ldrb   r3, [r1], #1\n\t"      //
        "  strb   r3, [r0], #1\n\t"      //
        "  ldrb   r3, [r1], #1\n\t"      //
        "  strb   r3, [r0], #1\n\t"      //
        "  ldrb   r3, [r1], #1\n\t"      //
        "  strb   r3, [r0], #1\n\t"      //
        "  subs   r2, r2, #1\n\t"        //
        "  bne    %l[loop_again_brute_force]" // 
        : 
        :
        :
        : loop_again_brute_force);

exit:
    __asm volatile(
        "  bx     lr"                    //
        :);

}





#else  // Still fast, but using less codespace

// rutils_memset    120 bytes codespace
// rutils_memcpy    136 bytes codespace

//!
//! @name      rutils_memset
//!
//! @brief     ARM-assembler optimized version of rutils_memset()
//!
//! @details   This is 100% compatible from caller's perspective
//! @details   As straight-C 'rutils_memset()'
//!
//! @param[in] 'dest_str'-- address where to start memset op to
//! @param[in] 'set_value'-- value to poke in
//! @param[in] 'length'-- number of times 'value' is to be set
//!
__attribute__((naked)) void rutils_memset(void    *dest_str,
                                          uint8_t  set_value,
                                          unsigned length)
{

    // Caller puts parameters in these caller-save registers:
    // 
    // r0 <-- 'dest_str'
    // r1 <-- 'set_value'
    // r2 <-- 'length'
    //
    // We'll use this/these caller-save registers in addition:
    // r3 <-- temp register
    //
    // For long values of 'length' we'll use callee-saved
    // registers r4-r11.

    __asm volatile goto(
        // Check for word alignment
        "  mov   r3, r0\n\t"             //
        "  ands  r3, #3\n\t"             // Keep only bits0,1
        "  beq   %l[do_words]\n\t"       // Already word aligned? branch
        // Calc. num. bytes to reach alignment
        "  eor   r3, #3\n\t"             // 1-complement of bits0,1 only
        "  add   r3, #1"                 // make 2's complement, r3 must be 1-3
        : 
        :
        :
        : do_words);

do_1to3_bytes:
    __asm volatile goto(
        // Sanity check remaining 'length' > 0
        "  orrs   r2, r2\n\t"            // Test 'length'
        "  beq    %l[exit]"              //
        : 
        :
        :
        : exit);

bytes_loop:
    __asm volatile goto(
        // Do 1 byte at a time for either r3 bytes (which will be1-3 bytes),
        // or until 'length' is depleted, whichever comes first.
        // This does either bytes up to alignment or
        // bytes at end of copy, past last word
        "  strb   r1, [r0], #1\n\t"      // Single byte set
        "  subs   r2, #1\n\t"            // Decrement 'length'
        "  beq    %l[exit]\n\t"          // If 'length'==0, branch
        "  subs   r3, #1\n\t"            // Decrement loop counter
        "  bne    %l[bytes_loop]\n\t"    // r3 >= 0? Yes--branch
        : 
        :
        :
        : exit, bytes_loop);

do_words:
    __asm volatile goto(
        // Duplicate bits 0-7 of r1 to bits 0-31 of r1
        "  lsl    r3, r1, #8\n\t"        // Copy bits 0-7 to bits 8-15
        "  orr    r1, r1, r3\n\t"        //
        "  lsl    r3, r1, #16\n\t"       // Copy bits 0-15 to bits 16-31
        "  orr    r1, r1, r3\n\t"        //

        // Put bits 2-4 of 'length' into r3. This is the
        // number of word sets, not inc. oct sets, to do
        "  mov    r3, r2\n\t"            //
        "  lsrs   r3, #2\n\t"            // r3 is total word count, inc. octs
        "  beq    %l[finish_trailing_bytes]\n\t" // No words+octs? branch 
        "  ands   r3, #7\n\t"            // Exclude num. of octs from r3
        "  beq    %l[do_movems]"         // If word count==0, branch
        : 
        :
        :
        : finish_trailing_bytes, do_movems);

word_loop:
    __asm volatile goto(
        // Loop around for single-byte sets
        "  str    r1, [r0], #4\n\t"      //
        "  subs   r3, #1\n\t"            //
        "  bne    %l[word_loop]"         //
        : 
        :
        :
        : word_loop);

do_movems:
    __asm volatile goto(
        // Do setup for oct sets
        "  mov    r3, r2\n\t"            //
        "  lsrs   r3, #5\n\t"            // Convert 'length' to num. of octs
        "  beq    %l[finish_trailing_bytes]\n\t" // None to do? branch
        "  stmfd  sp!, {r4-r11}\n\t"     // Save off oct working reg's
        "  mov    r4, r1\n\t"            // Init r4-r11 for oct copying
        "  mov    r5, r1\n\t"            //
        "  mov    r6, r1\n\t"            //
        "  mov    r7, r1\n\t"            //
        "  mov    r8, r1\n\t"            //
        "  mov    r9, r1\n\t"            //
        "  mov    r10, r1\n\t"           //
        "  mov    r11, r1"               //
        : 
        :
        :
        : finish_trailing_bytes);

movems_loop:
    __asm volatile goto(
        // Loop to do all oct sets
        "  stmia  r0!, {r4-r11}\n\t"     // Do a single oct set
        "  subs   r3, #1\n\t"            // Decr. oct set count
        "  bne    %l[movems_loop]\n\t"   // r3 >= 0? yes--branch
        "  ldmfd  sp!, {r4-r11}\n\t"     // Restore oct working reg's

        // We're done with all word and octs. Only thing left is
        // trailing bytes (0-3). Adjust 'length' for word+oct sets
        "  and    r2, #3"                // Only want to keep bits0,1
        : 
        :
        :
        : movems_loop);
  
finish_trailing_bytes:
    __asm volatile goto(
        // Setup for finishing 0-3 bytes at end
        "  and    r2, #3\n\t"            //
        "  mov    r3, r2\n\t"            //
        "  b      %l[do_1to3_bytes]"     //
        : 
        :
        :
        : do_1to3_bytes);

exit:
    __asm volatile(
        "  bx     lr\n\t"                //
        : );

    // make compiler happy
    UNUSED(dest_str);
    UNUSED(set_value);
    UNUSED(length);
}

//!
//! @name      rutils_memcpy
//!
//! @brief     ARM-optimized memcpy
//!
//! @details   Using move-multiple-register instructions, 
//! @details   efficiently memcpy 'src_addr' to 'dest_addr'
//! @details   'length' times.
//!
//! @param[in] 'dest_addr'-- address where to start memset op to
//! @param[in] 'src_addr'-- address where to start memset op to
//! @param[in] 'length'-- number of 8-bit copies to do
//!
__attribute__((naked)) void rutils_memcpy(void       *dest_str,
                                          const void *src_str,
                                          unsigned    length)
{
    // Caller puts parameters in these caller-save registers:
    // 
    // r0 <-- 'dest_str'
    // r1 <-- 'src_str'
    // r2 <-- 'length'
    //
    // We'll use this/these caller-save registers in addition:
    // r3 <-- temp register
    //
    // For a moment, we need to borrow r4 early because
    // we're out of registers.
    //
    // For long values of 'length' we'll use callee-saved
    // registers r4-r11.

    __asm volatile goto(
        // If bits0,1 of r0 and r1 are the same, then they're
        // aligned relative to each other, and word+oct copies
        // can be done. If not, no optimizations possible
        // Also, if 'length' < 8, then do byte-by-byte too
        "  eor   r3, r0, r1\n\t"         //
        "  ands  r3, #3\n\t"             //
        "  bne   %l[brute_force]\n\t"    //
        "  cmp   r2, #8\n\t"             // Is r2 < 8?
        "  blo   %l[brute_force]\n\t"    // ...if yes, branch

        "  stmfd sp!, {r4}\n\t"          // We need an extra working reg

        // Calculate num. of bytes to get to next word alignment
        "  mov   r3, r2\n\t"             //
        "  ands  r3, #3\n\t"             //
        "  beq   %l[do_words]\n\t"       // Already word aligned? yes--branch
        "  eor   r3, #3\n\t"             // 1-complement of bits0,1 only
        "  add   r3, #1"                 // make 2's complement, r3 must be 1-3
        : 
        :
        :
        : brute_force, do_words);

do_1to3_bytes:
    __asm volatile goto(
        // Prepare to do 1-3 unaligned bytes at beginning or
        // 1-3 aligned bytes at end
        "  orrs  r2, r2\n\t"             // test 'length'==0?
        "  beq   %l[exit]\n\t"           // ...if yes, branch
        "  sub   r2, r3\n\t"             // maintain 'length'
        : 
        :
        :
        : exit);

do_1to3_loop:
    __asm volatile goto(
        // Do these odd bytes.
        // Num. of bytes to do is in r3, and will be 1-3
        "  ldrb  r4, [r1], #1\n\t"       //
        "  strb  r4, [r0], #1\n\t"       //
        "  subs  r3, #1\n\t"             // More bytes to copy?
        "  bne   %l[do_1to3_loop]"       //
        : 
        :
        :
        : do_1to3_loop);

do_words:
    __asm volatile goto(
        // First optimization: 32-bit copies
        // Both src and dest are aligned on a 32-bit boundary now
        "  mov   r3, r2\n\t"             //
        "  lsrs  r3, #2\n\t"             // calc. num. 32-bit copies left
        "  beq   %l[trailing_bytes_setup]\n\t" // No more? then branch
        "  ands  r3, #7\n\t"             // Exclude num. of oct copies
        "  beq   %l[do_movems]"          // Are there any word copies? yes--br
        : 
        :
        :
        : trailing_bytes_setup, do_movems);

word_loop:
    __asm volatile goto(
        // Do 32-bit copies. Will be from 1-7.
        "  ldr   r4, [r1], #4\n\t"       //
        "  str   r4, [r0], #4\n\t"       //
        "  subs  r3, #1\n\t"             //
        "  bne   %l[word_loop]"          //
        : 
        :
        :
        : word_loop);

do_movems:
    __asm volatile goto(
        // Second optimization: oct (32-byte) copies
        // This uses movem (move multiple registers) ARM instr.
        "  mov   r3, r2\n\t"             //
        "  lsrs  r3, #5\n\t"             // r3= num oct copies to do
        "  beq   %l[trailing_bytes_setup]\n\t" // None to do? then branch
        "  stmfd sp!, {r5-r11}"          // Save oct working reg's
        : 
        :
        :
        : trailing_bytes_setup);

movems_loop:
    __asm volatile goto(
        // Do the oct copies in a loop
        "  ldmia r1!, {r4-r11}\n\t"      //
        "  stmia r0!, {r4-r11}\n\t"      //
        "  subs  r3, #1\n\t"             //
        "  bne   %l[movems_loop]\n\t"    //
        "  ldmfd sp!, {r5-r11}\n\t"      // Restore oct working reg's

        // At this point, we're done with all copies
        // (unaligneds, words, octs). All that's left
        // is 0-3 dangling bytes at end
        "  b     %l[trailing_bytes_setup]" //
        : 
        :
        :
        : movems_loop, trailing_bytes_setup);

exit:
    __asm volatile(
        // Thi is the exit point for all non-brute force paths
        "  ldmfd sp!, {r4}\n\t"          // Restore working reg
        "  bx    lr"                     // Return from fcn.
        : );

        // Alternate code path:
        // Byte-by-byte "brute force" method

brute_force:
    __asm volatile goto(
        // Make sure 'length' > 0
        "  orrs   r2, r2\n\t"            //
        "  beq    %l[brute_force_exit]"  //
        : 
        :
        :
        : brute_force_exit);

brute_force_loop:
    __asm volatile goto(
        // Copy 1 byte at a time in this loop
        "  ldrb   r3, [r1], #1\n\t"      //
        "  strb   r3, [r0], #1\n\t"      //
        "  subs   r2, #1\n\t"            //
        "  bne    %l[brute_force_loop]"  //
        : 
        :
        :
        : brute_force_loop);

brute_force_exit:
    __asm volatile(
        "  bx     lr"                    //
        : ); 

        // Stub to do trailing bytes
        // NOTE: still have r4 pushed here

trailing_bytes_setup:
    __asm volatile goto(
        // Subtract all word+oct copying done from 'length'
        // Set r3 to count num. of bytes to copy
        "  and    r2, #3\n\t"            //
        "  mov    r3, r2\n\t"            //
        "  b      %l[do_1to3_bytes]"     //
        : 
        :
        :
        : do_1to3_bytes);

    // make compiler happy
    UNUSED(dest_str);
    UNUSED(src_str);
    UNUSED(length);
}

#endif