/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.2 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

/*
 * VEC_nofault -- handles nofault exceptions early on in system initialization
 * before VEC_trap is usable.
 */
VECTOR(VEC_nofault, M_EXCEPT)
EXL_EXPORT(locore_exl_5)
	move	a2,s0
	move	s1,k1
	jal	trap_nofault		# trap_nofault(ef_ptr, code, sr, cause)
	move	k1,s1
	j	exception_exit
EXL_EXPORT(elocore_exl_5)
	END(VEC_nofault)
