/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991 MIPS Computer Systems, Inc.            |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 52.227-7013.   |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Drive                                |
 * |         Sunnyvale, CA 94086                               |
 * |-----------------------------------------------------------|
 */
#ident "$Id: libdl.c,v 1.9 1995/12/05 07:37:21 ack Exp $"

/*
 * RLD routines to support LIBDL
 */
#ifdef __STDC__
	#pragma weak dlopen = _dlopen
	#pragma weak dlclose = _dlclose
	#pragma weak dlsym = _dlsym
	#pragma weak dlerror = _dlerror
	#pragma weak sgidladd = _sgidladd
	#pragma weak sgidlopen_version = _sgidlopen_version
	#pragma weak sgigetdsoversion = _sgigetdsoversion
#endif

#include "synonyms.h"

#include "libc_interpose.h"
#include <stdio.h> 
#include <ctype.h>
#include <rld_interface.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stddef.h>

void *dlopen(const char * a, int b)
{
	void *rv;
	a = __tp_dlinsert_pre(a, b);
	rv = _rld_new_interface(_RLD_LIBDL_INTERFACE,
				     _LIBDL_RLD_DLOPEN,
				     (unsigned long) a,
				     (unsigned long) b);
	__tp_dlinsert_post(a, rv);
	return rv;
}

void *dlsym(void * a, const char * b)
{
	return(_rld_new_interface(_RLD_LIBDL_INTERFACE,
				   _LIBDL_RLD_DLSYM,
				   (unsigned long) a,
				   (unsigned long) b));
}

int dlclose(void *a)
{
	int rv;

	__tp_dlremove_pre(a);
	rv  = (int)(ptrdiff_t) _rld_new_interface(_RLD_LIBDL_INTERFACE,
					 _LIBDL_RLD_DLCLOSE,
					 (unsigned long) a);
	__tp_dlremove_post(a, rv);
	return rv;
}

char *dlerror(void)
{
	return((char *) _rld_new_interface(_RLD_LIBDL_INTERFACE,
					    _LIBDL_RLD_DLERROR));
}

void *sgidladd(const char * a, int b)
{
	void *rv;
	a = __tp_dlinsert_pre(a, b);
	rv = _rld_new_interface(_RLD_LIBDL_INTERFACE,
				     _LIBDL_SGI_RLD_DLADD,
				     (unsigned long) a,
				     (unsigned long) b);
	__tp_dlinsert_post(a, rv);
	return rv;
}

void *sgidlopen_version(const char * a, int b, const char *c, int d)
{
	void *rv;
	a = __tp_dlinsert_version_pre(a, b, c, d);
	rv = _rld_new_interface(_RLD_LIBDL_INTERFACE,
				     _LIBDL_RLD_DLOPEN_VERSION,
				     (unsigned long) a,
				     (unsigned long) b,
				     (unsigned long) c,
				     (unsigned long) d);
	__tp_dlinsert_post(a, rv);
	return rv;
}

char *sgigetdsoversion(const char *a)
{
	return(_rld_new_interface(_RLD_DSO_VERSION, (unsigned long) a));
}
