/**************************************************************************/
/*                                                                        */
/*               Copyright (C) 1989, Silicon Graphics, Inc.               */
/*                                                                        */
/*  These coded instructions, statements, and computer programs  contain  */
/*  unpublished  proprietary  information of Silicon Graphics, Inc., and  */
/*  are protected by Federal copyright law.  They  may  not be disclosed  */
/*  to  third  parties  or copied or duplicated in any form, in whole or  */
/*  in part, without the prior written consent of Silicon Graphics, Inc.  */
/*                                                                        */
/**************************************************************************/

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

/* 
	__setlinkcontext - function

	Value for uc_link was stored in $s0 by makecontext.  So the 
	argument for setcontext needs to be moved from $s0 into $a0.  
	$a0 will then contain the value of first argument for setcontext.
  
	We need the correct $gp (caller-saved register) before we "jump
	and link" to _setcontext.  The correct $gp value can be found
	$s1 (makecontext routine stored the $gp when altering the context)
	and therefore needs to be moved from the $s1 register to the $gp
	register before "jal". 

	__getgp - function

	Get the current $gp value and return it to the calling function.
*/

	PICOPT

LEAF(__setlinkcontext)
	move	a0,s0
	move	gp,s1
	jal	_setcontext

	END(__setlinkcontext)

LEAF(__getgp)
	move	v0,gp
	j	ra

	END(__getgp)
