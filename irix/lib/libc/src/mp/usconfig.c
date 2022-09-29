/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.31 $ $Author: joecd $"

#ifdef __STDC__
	#pragma weak usconfig = _usconfig
	#pragma weak usputinfo = _usputinfo
	#pragma weak usgetinfo = _usgetinfo
	#pragma weak uscasinfo = _uscasinfo
#endif

#include "synonyms.h"
#include "sys/types.h" 
#include "shlib.h"
#include "unistd.h"
#include "sys/stat.h"
#include "ulocks.h"
#include "us.h"
#include "errno.h"
#include "stdio.h"
#include "stdarg.h"
#include "mplib.h"

size_t _us_mapsize = _USMMAPSIZE;
int _us_maxusers = _DEFUSUSERS;
int _us_locktype = US_NODEBUG;
int _us_arenatype = US_GENERAL;
void *_us_attachaddr = (void *)~0L;
unsigned _us_autogrow = 1;
unsigned _us_autoresv = 0;

/*
 * usconfig - configuration function for user locks and semaphores
 * Returns:
 *	0 Success
 *	-1 error (errno set)
 */
ptrdiff_t
usconfig(int cmd, ...)
{
	va_list ap;
	__psint_t rval = 0;
	unsigned arg;
	histptr_t *hist;
	ushdr_t *header;
	hist_t *hp, *nhp;
	mode_t acc;

	va_start(ap, cmd);
	switch(cmd) {
	case CONF_LOCKTYPE:
		arg = va_arg(ap, unsigned);
		if (arg > US_DEBUGPLUS) {
			setoserror(EINVAL);
			return(-1);
		}
		rval = (int) _us_locktype;
		_us_locktype = (int) arg;			
		break;

	case CONF_INITUSERS:
		arg = va_arg(ap, unsigned);
		if (arg == 0 || arg > _MAXUSUSERS) {
			setoserror(EINVAL);
			return(-1);
		}
		rval = _us_maxusers;
		_us_maxusers = (int) arg;			
		break;

	case CONF_INITSIZE:
		/* some somewhat arbitrary minimum */
		arg = va_arg(ap, unsigned);
		if (arg < 4096) {
			setoserror(EINVAL);
			return(-1);
		}
		rval = (ptrdiff_t)(_us_mapsize - sizeof(ushdr_t));
		_us_mapsize = arg + sizeof(ushdr_t);
		break;

	case CONF_GETUSERS:
		header = va_arg(ap, ushdr_t *);
		rval = header->u_maxtidusers;
		break;

	case CONF_GETSIZE:
		header = va_arg(ap, ushdr_t *);
		rval = (ptrdiff_t)(header->u_mmapsize - sizeof(ushdr_t));
		break;

	case CONF_HISTON:
		header = va_arg(ap, ushdr_t *);
		header->u_histon |= _USSEMAHIST;
		break;

	case CONF_HISTOFF:
		header = va_arg(ap, ushdr_t *);
		header->u_histon &= ~_USSEMAHIST;
		break;

	case CONF_HISTSIZE:
		header = va_arg(ap, ushdr_t *);
		arg = va_arg(ap, unsigned);
		rval = (int) header->u_histsize;
		header->u_histsize = arg;
		break;

	case CONF_HISTFETCH:
		header = va_arg(ap, ushdr_t *);
		if (header->u_histon & _USSEMAHIST) {
			hist = va_arg(ap, histptr_t *);
			hist->hp_current = header->u_histlast;
			hist->hp_entries = header->u_histcount;
			hist->hp_errors = header->u_histerrors;
		} else {
			setoserror(EINVAL);
			rval = -1;
		}
		break;

	case CONF_HISTRESET:
		header = va_arg(ap, ushdr_t *);
		for (hp = header->u_histfirst; hp; hp = nhp) {
			nhp = hp->h_next;
			usfree(hp, header);
		}
		header->u_histfirst = NULL;
		header->u_histlast = NULL;
		header->u_histcount = 0;
		header->u_histerrors = 0;
		break;

	case CONF_STHREADIOON:
		rval = __us_sthread_stdio;
		__us_sthread_stdio = 1;
		if (__multi_thread)
			__us_rsthread_stdio = 1;
		break;
	case CONF_STHREADIOOFF:
		rval = __us_sthread_stdio;
		__us_sthread_stdio = 0;
		if (__multi_thread)
			__us_rsthread_stdio = 0;
		break;
	case CONF_STHREADMISCON:
		rval = __us_sthread_misc;
		__us_sthread_misc = 1;
		if (__multi_thread)
			__us_rsthread_misc = 1;
		break;
	case CONF_STHREADMISCOFF:
		rval = __us_sthread_misc;
		__us_sthread_misc = 0;
		if (__multi_thread)
			__us_rsthread_misc = 0;
		break;
	case CONF_STHREADMALLOCON:
		rval = __us_sthread_malloc;
		__us_sthread_malloc= 1;
		if (__multi_thread)
			__us_rsthread_malloc = 1;
		break;
	case CONF_STHREADMALLOCOFF:
		rval = __us_sthread_malloc;
		__us_sthread_malloc = 0;
		if (__multi_thread)
			__us_rsthread_malloc = 0;
		break;
	case CONF_ARENATYPE:
		arg = va_arg(ap, unsigned);
		if (arg != US_SHAREDONLY && arg != US_GENERAL) {
			setoserror(EINVAL);
			return(-1);
		}
		rval = _us_arenatype;			
		_us_arenatype = (int)arg;			
		break;
	case CONF_CHMOD:
		/* change mode -
		 * arena mapped file
		 * hardlock device
		 */
		header = va_arg(ap, ushdr_t *);
		acc = va_arg(ap, mode_t);
		if (chmod(header->u_mfile, acc) != 0)
			return(-1);
		break;
	case CONF_ATTACHADDR:
		rval = (__psint_t)_us_attachaddr;
		_us_attachaddr = va_arg(ap, void *);
		break;
	case CONF_AUTOGROW:
		rval = _us_autogrow;
		_us_autogrow = va_arg(ap, unsigned);
		break;
	case CONF_AUTORESV:
		rval = _us_autoresv;
		_us_autoresv = va_arg(ap, unsigned);
		break;
	default:
		setoserror(EINVAL);
		rval = -1;
		break;
	}
	return(rval);
}

/*
 * usputinfo - store a single word for easy retrieval by usgetinfo
 */
void
usputinfo(usptr_t *header, void *info)
{
	((ushdr_t *)header)->u_info = info;
}

/*
 * usgetinfo - retrieve a single word 
 */
void *
usgetinfo(usptr_t *header)
{
	return(((ushdr_t *)header)->u_info);
}

int
uscasinfo(usptr_t *header, void *oldi, void *newi)
{
	return(uscas(&((ushdr_t *)header)->u_info, (ptrdiff_t)oldi,
					(ptrdiff_t)newi, header));
}
