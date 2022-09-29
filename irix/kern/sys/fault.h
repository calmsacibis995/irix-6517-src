/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1992, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_FAULT_H__
#define __SYS_FAULT_H__
#ident	"$Revision: 1.3 $"

#include "sys/types.h"
/*
 * Fault tracing for use with /proc
 */

/* Values for fault tracing - start at 1 */
#define	FLTILL		1	/* illegal instruction */
#define FLTPRIV		2	/* priviledged instruction */
#define FLTBPT		3	/* breakpoint trap */
#define	FLTTRACE	4	/* trace trap */
#define FLTACCESS	5	/* memory access violation */
#define FLTBOUNDS	6	/* memory bounds violation */
#define FLTIOVF		7	/* integer overflow */
#define FLTIZDIV	8	/* integer zero-divide */
#define FLTFPE		9	/* floating point exception */
#define FLTSTACK	10	/* unrecoverable stack fault */
#define FLTPAGE		11	/* recoverable page fault */
#define FLTPCINVAL	12	/* invalid PC exception */
#define FLTWATCH	13	/* user watchpoint */
#define FLTKWATCH	14	/* kernel watchpoint */
#define FLTSCWATCH	15	/* hit a store conditional on a watched page */

typedef struct {
	__uint32_t word[4];
} fltset_t;
#endif /* __SYS_FAULT_H__ */
