/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Id: vsession_lp.c,v 1.6 1997/04/12 18:16:36 emb Exp $"

#include <sys/types.h>
#include <ksys/vsession.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include "vsession_private.h"

void
vsession_cell_init()
{
}

/*
 * Look for a vsession structure with the given id in a hash table queue
 * If not found, a vsession will be created if so requested. The vsession
 * returned is referenced.
 */
vsession_t *
vsession_lookup(
	pid_t		sid)
{
	return vsession_lookup_local(sid);
}

vsession_t *
vsession_create(
	pid_t		sid)
{
	return vsession_create_local(sid);
}
