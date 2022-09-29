/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/var.h>
#include <ksys/pid.h>
#include "pid_private.h"

void
pid_init(void)
{
	pidtabsz = v.v_proc;
	pid_base = 0;
	pid_wrap = (MAXPID / pidtabsz) * pidtabsz;
	pidtab_init();
}

int
pid_getlist(
        pidord_t *next,
        size_t *count,
        pid_t *pidbuf,
        pidord_t *ordbuf)
{
        if (*next >= MAXPID) {
	       *count = 0;
	       return 1;
	}
        else if (pid_getlist_local(*next, count, pidbuf, ordbuf))
		*next = MAXPID;
	else if (*count != 0) 
                *next = *(ordbuf + (*count - 1));
	return (*count == 0);
}
        
