/*
Copyright (c) 2018, Bernie Woodland
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

#include <raging-global.h>
#include <raging-contract.h>


/**
 *	@brief This function is used by the design by contract macros: REQUIRE, ENSURE, INVARIANT.
 *
 *	This function is called by the CONTRACT_ASSERT that is the base macro utilized by the rest
 *	of the design by contract macros.  This function will notify the developer of a failed
 *	precondition( REQUIRE ), postcondition( ENSURE ) or invariance( INVARIANT ).  This method
 *	is customizable, but does provide the filename and line number of the failed contract.
 *
 *	@param[in] 	file	The filename of the file where the failed contract occurred.
 *	@param[in]	line	The line number of the failed contract.
 */
void onContractFailure(uint8_t* file, uint32_t line)
{
    static unsigned place_to_put_breakpoint;
    UNUSED(file);
    UNUSED(line);

    place_to_put_breakpoint++;
}