/* Copyright 2013-2014 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <asm-utils.h>
#include <asm-offsets.h>
#include <processor.h>
#include <stack.h>

#define GET_LR_STACK(stack_reg,pir_reg)				\
	sldi	stack_reg,pir_reg,STACK_SHIFT;			\
	addis	stack_reg,stack_reg,CPU_LR_STACKS_OFFSET@ha;	\
	addi	stack_reg,stack_reg,CPU_LR_STACKS_OFFSET@l;

	.section ".text","ax"
	.balign	0x10

	/* void set_hid0(unsigned long hid0) */
.global set_hid0
set_hid0:
	sync
	mtspr	SPR_HID0,%r3
	mfspr	%r3,SPR_HID0
	mfspr	%r3,SPR_HID0
	mfspr	%r3,SPR_HID0
	mfspr	%r3,SPR_HID0
	mfspr	%r3,SPR_HID0
	mfspr	%r3,SPR_HID0
	isync
	blr

.global __trigger_attn
__trigger_attn:
	sync
	isync
	attn
	blr

#ifdef STACK_CHECK_ENABLED
.global _mcount
_mcount:
	mr	%r3,%r1
	mflr	%r4
	b	__mcount_stack_check
#else


/* entry tracing - call into mcount */

/* exit tracing:
	1. push LRSAVE on the LRSTACK
	2. overwrite LRSAVE in the stack frame of the function that called into
		mcount
	3. on mexit, pop off the LRSTACK and return into the intended function
*/

/* When tracing is enabled we do the following:
  Establish a new stack frame */


/*
 *	when mcount gets called we are:
 *		mcount          (no stack frame)
 *		traced_function (current stack frame)
 *		parent_function
 *
 *	We need to patch LRSAVE in the parent function's stack frame so
 *	it returns to __mexit rather than the parent. inside __mexit we'll
 *	fix things up and resume normal control flow.
 *
 **/

.global _mcount
_mcount:
	/* ok, lets mangle the stack */
	ld	%r4,0(%r1) // grab the backchain pointer
	addi	%r4,%r4,16
	mflr	%r3
	mfspr	%r5,SPR_PIR
	GET_LR_STACK(%r5, %r5)
	b	__mcount_trace

/* landing point - when the traced function returns we end up here, all we need
 * to do here is preserve the return value which we do by leaving it in r3
 * so it's passed in as an argument. The C code is then responsible for keeping
 * the value saved.
 *
 * At this point the traced function has returned so r1 points to
 * parent_function's stack frame. Restoring LRSAVE to it's original
 * value (written there by traced_function) will resume the program normally.
 */
.global __mexit
__mexit:
	addi	%r4,%r1,16
	mfspr	%r5,SPR_PIR
	GET_LR_STACK(%r5, %r5)
	b __mcount_exit

#endif


.global mambo_read
mambo_read:
#define SIM_READ_CONSOLE_CODE 60
	li	%r3,SIM_READ_CONSOLE_CODE
	.long	0x000eaeb0
	extsw	%r3,%r3
	blr

.global mambo_write
mambo_write:
#define SIM_WRITE_CONSOLE_CODE 0
	li	%r6,0
	mr	%r5,%r4
	mr	%r4,%r3
	li	%r3,SIM_WRITE_CONSOLE_CODE
	.long	0x000eaeb0
	blr

.global mambo_sim_exit
mambo_sim_exit:
	li	%r3, 31	/* aka. SimExitCode */
	.long	0x000eaeb0
	b	.

.global mambo_get_time
mambo_get_time:
#define SIM_GET_TIME_CODE 70
	li	%r3,SIM_GET_TIME_CODE
	.long	0x000eaeb0
	blr

