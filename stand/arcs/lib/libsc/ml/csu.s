#ident "$Revision: 1.18 $"

/*
 * csu.s -- standalone library startup code for programs linked
 *		with libsc.a.
 */

#include <ml.h>
#include <sys/regdef.h>
#include <sys/asm.h>
#include <fault.h>

#include "stack.h"
/*
SCSTKSIZE=0x20000			# 128K stack
	BSS(scstack,SCSTKSIZE)
	BSS(startsc_stk,BPREG)
*/

	.text

STARTSCFRAME=(4*BPREG)+BPREG+BPREG	# room for four args, sp and ra
NESTED(startsc, STARTSCFRAME, zero)
	move	v0,sp
	LA	sp,scstack
	PTR_ADDU sp,SCSTKSIZE		# beginning stack address
	PTR_SUBU sp,4*BPREG		# bump 4 args so it looks real

	PTR_SUBU sp,STARTSCFRAME
	sreg	ra,STARTSCFRAME-BPREG(sp)
	sreg	v0,STARTSCFRAME-(2*BPREG)(sp)
	sreg	a0,STARTSCFRAME(sp)
	sreg	a1,STARTSCFRAME+BPREG(sp)
	sreg	a2,STARTSCFRAME+(2*BPREG)(sp)

	LA	v0,scstack
	PTR_ADDU v0,SCSTKSIZE		# beginning stack address

	sreg	sp,0(v0)	# save sp because debugger might trash
	
	/* copy env and argv to the client's memory
	 * from the caller's memory space, resulting new
	 * values are returned on the stack
	 */
	PTR_ADDU a0,sp,STARTSCFRAME+(2*BPREG)	# address of envp
	jal	initenv

	PTR_ADDU a0,sp,STARTSCFRAME		# address of argc
	PTR_ADDU a1,sp,STARTSCFRAME+BPREG	# address of argv
	jal	initargv

	lreg	a0,STARTSCFRAME(sp)
	lreg	a1,STARTSCFRAME+BPREG(sp)
	lreg	a2,STARTSCFRAME+(2*BPREG)(sp)
	LA	a3,_dbgsc_start		# where debugger starts client
	jal	_check_dbg		# check for debug request
	b	_no_dbgsc		# no debugger loaded

_dbgsc_start:
	LA	sp,scstack
	PTR_ADDU sp,SCSTKSIZE
	lreg	sp,0(sp)	# reload sp
	jal	idbg_init		# set up for debugging
	lw	v0,dbg_loadstop		# find out if we should stop
	beq	zero,v0,_no_dbgsc
	jal	quiet_debug

_no_dbgsc:
	LA	sp,scstack
	PTR_ADDU sp,SCSTKSIZE
	lreg	sp,0(sp)	# reload sp

	lreg	a0,STARTSCFRAME(sp)	# get new values from stack
	lreg	a1,STARTSCFRAME+BPREG(sp)
	lreg	a2,STARTSCFRAME+(2*BPREG)(sp)

	jal	main

	lreg	ra,STARTSCFRAME-BPREG(sp)
	lreg	sp,STARTSCFRAME-(2*BPREG)(sp)	# restore stack address of caller
	j	ra
	END(startsc)
