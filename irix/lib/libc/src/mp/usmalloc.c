/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.6 $ $Author: ism $"

#ifdef __STDC__
	#pragma weak usmalloc = _usmalloc
	#pragma weak usfree = _usfree
	#pragma weak usrealloc = _usrealloc
	#pragma weak uscalloc = _uscalloc
	#pragma weak usmallopt = _usmallopt
	#pragma weak usmallinfo = _usmallinfo
	#pragma weak usrecalloc = _usrecalloc
	#pragma weak usmallocblksize = _usmallocblksize
	#pragma weak usmemalign = _usmemalign
#endif

#include "synonyms.h"
#include <sys/types.h>
#include "ulocks.h"
#include "us.h"
#include "malloc.h"

/*
 * wrappers for usmalloc
 */

void *
usmalloc(size_t n, usptr_t *a)
{ return(amalloc(n, ((ushdr_t *)a)->u_arena)); }

void
usfree(void *c, usptr_t *a)
{ afree(c, ((ushdr_t *)a)->u_arena); }

void *
usrealloc(void *c, size_t n, usptr_t *a)
{ return(arealloc(c, n, ((ushdr_t *)a)->u_arena)); }

void *
uscalloc(size_t n, size_t e, usptr_t *a)
{ return(acalloc(n, e, ((ushdr_t *)a)->u_arena)); }

struct mallinfo
usmallinfo(usptr_t *a)
{ return(amallinfo(((ushdr_t *)a)->u_arena)); }

int
usmallopt(int i, int j, usptr_t *a)
{ return(amallopt(i, j, ((ushdr_t *)a)->u_arena)); }

void *
usrecalloc(void *c, size_t num, size_t size, usptr_t *a)
{ return(arecalloc(c, num, size, ((ushdr_t *)a)->u_arena)); }

size_t
usmallocblksize(void *c, usptr_t *a)
{ return(amallocblksize(c, ((ushdr_t *)a)->u_arena)); }

void *
usmemalign(size_t al, size_t n, usptr_t *a)
{ return(amemalign(al, n, ((ushdr_t *)a)->u_arena)); }
