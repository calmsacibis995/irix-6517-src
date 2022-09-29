/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* XXX hack for now - since we want to run with 'old' rld's for a bit
 * we need to maintain __uscasdata data structure for sproc to pass.
 * Once we get a new rld, we can trash this (and pass NULL in sproc)
 */
#include <ulocks.h>
typedef struct {
        void *arena; /* ptr to arena */
        int fd;         /* file desc for arena */
        int hfd;        /* file desc for hlock device */
        int *haddr;     /* hlocks attach address */
        ulock_t caslck; /* ptr to hardware lock */
        int systype;    /* system type */
} casdata_t;
casdata_t __uscasdata = {
        NULL, -1, -1, (int *)~0L, NULL, 0
};

/*
 * special init entry point for rld
 * No longer needed ... when we had file arenas for cas, we needed to
 * make sure that rld was initialized specially since it shares a pid
 * with the main executable.
 */
/* ARGSUSED */
void
__uscas_rldinit(void *p)
{
}
