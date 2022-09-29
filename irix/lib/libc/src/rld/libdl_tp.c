/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: libdl_tp.c,v 1.4 1996/08/21 19:50:32 jwag Exp $"

#pragma weak __tp_dlinsert_pre = ___tp_dlinsert_pre
#pragma weak __tp_dlinsert_version_pre = ___tp_dlinsert_version_pre
#pragma weak __tp_dlremove_pre = ___tp_dlremove_pre
#pragma weak __tp_dlremove_post = ___tp_dlremove_post
#pragma weak __tp_dlinsert_post = ___tp_dlinsert_post

#include "libc_interpose.h"

/*
 * trace point stub for perf package
 */

/* ARGSUSED */
char *
___tp_dlinsert_pre(const char *a, int b)
{
	return (char *)a;
}

/* ARGSUSED */
char *
___tp_dlinsert_version_pre(const char * a, int b, const char *c, int d)
{
	return (char *)a;
}

/* ARGSUSED */
void
___tp_dlremove_pre(void *a)
{
}

/* ARGSUSED */
void
___tp_dlremove_post(void *a, int rv)
{
}

/* ARGSUSED */
void
___tp_dlinsert_post(const char *n, void *a)
{
}
