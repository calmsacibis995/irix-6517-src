#ident	"$Id: mark_s.s,v 1.1 1994/07/21 01:14:44 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "sys/regdef.h"
#include "sys/asm.h"

/*
 * This routine is used to mark the start of lib code for loading 
 * lib code from prom into scache for cached execution of tests. 
 */
	.text

LEAF(_mark_start)
	j	ra
END(_mark_start)

