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

.extern VEC_trap

/*
 * VEC_addrerr
 * Handles AdrErrRead, AdrErrWrite
 */
VECTOR(VEC_addrerr, M_EXCEPT)
EXL_EXPORT(locore_exl_4)
	j	VEC_trap
EXL_EXPORT(elocore_exl_4)
	END(VEC_addrerr)

