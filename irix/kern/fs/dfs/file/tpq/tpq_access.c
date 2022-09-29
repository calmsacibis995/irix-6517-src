/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tpq_access.c,v 65.3 1998/03/23 16:28:08 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1994, 1992 Transarc Corporation - All rights reserved */
#include <dcedfs/param.h>
#include <dcedfs/stds.h>

#include <tpq.h>
#include <tpq_private.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tpq/RCS/tpq_access.c,v 65.3 1998/03/23 16:28:08 gwehrman Exp $")

/*
 * This module provides an interface for queued functions so that they
 * may get and set various components of their operations.  For example, 
 * An operation might want to changes its reschedule interval.
 */
/* EXPORT */
void tpq_SetArgument(opHandle, arg)
  IN void *opHandle;		/* operation to change */
  IN void *arg;			/* new argument */
{
    struct tpq_queueEntry *entryP = (struct tpq_queueEntry *)opHandle;
    entryP->arg = arg;
}

/* EXPORT */
int tpq_GetPriority(opHandle)
  IN void *opHandle;		/* operation to change */
{
    return ((struct tpq_queueEntry *)opHandle)->priority;
}

/* EXPORT */
void tpq_SetPriority(opHandle, priority)
  IN void *opHandle;		/* operation to change */
  IN int priority;		/* new priority */
{
    struct tpq_queueEntry *entryP = (struct tpq_queueEntry *)opHandle;
    entryP->priority = priority;
}

/* EXPORT */
long tpq_GetGracePeriod(opHandle)
  IN void *opHandle;		/* operation to change */
{
    return ((struct tpq_queueEntry *)opHandle)->gracePeriod;
}

/* EXPORT */
void tpq_SetGracePeriod(opHandle, gracePeriod)
  IN void *opHandle;		/* operation to change */
  IN long gracePeriod;		/* new grace period */
{
    struct tpq_queueEntry *entryP = (struct tpq_queueEntry *)opHandle;
    entryP->gracePeriod = gracePeriod;
}

/* EXPORT */
long tpq_GetRescheduleInterval(opHandle)
  IN void *opHandle;		/* operation to change */
{
    return ((struct tpq_queueEntry *)opHandle)->rescheduleInterval;
}

/* EXPORT */
void tpq_SetRescheduleInterval(opHandle, rescheduleInterval)
  IN void *opHandle;		/* operation to change */
  IN long rescheduleInterval;	/* new reschedule interval */
{
    struct tpq_queueEntry *entryP = (struct tpq_queueEntry *)opHandle;
    icl_Trace3(tpq_iclSetp, TPQ_ICL_SETRESCHED, ICL_TYPE_POINTER, entryP,
	       ICL_TYPE_LONG, (entryP->rescheduleInterval),
	       ICL_TYPE_LONG, rescheduleInterval);
    
    entryP->rescheduleInterval = rescheduleInterval;
}

/* EXPORT */
long tpq_GetDropDeadTime(opHandle)
  IN void *opHandle;		/* operation to change */
{
    return ((struct tpq_queueEntry *)opHandle)->dropDeadTime;
}

/* EXPORT */
void tpq_SetDropDeadTime(opHandle, dropDeadTime)
  IN void *opHandle;		/* operation to change */
  IN long dropDeadTime;		/* new drop dead time */
{
    struct tpq_queueEntry *entryP = (struct tpq_queueEntry *)opHandle;
    entryP->dropDeadTime = dropDeadTime;
}

