/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Id: vsession_misc.c,v 1.7 1997/05/07 18:38:03 emb Exp $"

#include <sys/types.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <ksys/vsession.h>
#include <sys/errno.h>
#include "vsession_private.h"
#include "pproc.h"
#include "pproc_private.h"


/*
 * Return controlling tty dev if proc p has one.
 *
 * Otherwise, return NODEV.
 */
dev_t
cttydev(register proc_t *p)
{
	vsession_t *vsp;
	dev_t devnum;

	if (p->p_flag & SNOCTTY)
		return NODEV;

	mraccess(&p->p_who);
	if (p->p_sid < 0) {
		mraccunlock(&p->p_who);
		return NODEV;
	}

	vsp = VSESSION_LOOKUP(p->p_sid);
	ASSERT(vsp);
	mraccunlock(&p->p_who);
	VSESSION_CTTY_DEVNUM(vsp, &devnum);
	VSESSION_RELE(vsp);
	return devnum;
}
