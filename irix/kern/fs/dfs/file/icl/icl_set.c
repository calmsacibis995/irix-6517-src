/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: icl_set.c,v 65.5 1998/03/24 17:20:19 lmc Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: icl_set.c,v $
 * Revision 65.5  1998/03/24 17:20:19  lmc
 * Changed an osi_mutex_t to a mutex_t and changed osi_mutex_enter() to
 * mutex_lock and osi_mutex_exit to mutex_unlock.
 * Added a name parameter to osi_mutex_init() for debugging purposes.
 *
 * Revision 65.4  1998/03/23  16:25:32  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1998/01/20 16:46:59  lmc
 * Initial merge of icl tracing for kudzu.  This picks up the support for
 * tracing 64 bit kernels.  There are still some compile errors because
 * it uses AFS_hgethi/AFS_hgetlo which isn't done yet.
 *
 * Revision 65.2  1997/11/06  19:58:06  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:41  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.41.1  1996/10/02  17:52:25  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:41:02  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1995, 1993 Transarc Corporation - All rights reserved. */
/*
 * This file contains routines for managing event sets, including
 * their creation and deletion.  Functions dealing with actually
 * logging a particular event are in icl_event.c
 */

#include <icl.h>	/* includes standard stuff */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/icl/RCS/icl_set.c,v 65.5 1998/03/24 17:20:19 lmc Exp $")

struct icl_set *icl_allSets = 0;

int
icl_CreateSet(
  char *name,
  struct icl_log *baseLogp,
  struct icl_log *fatalLogp,
  struct icl_set **outSetpp)
{
    icl_MakePreemptionRight();
    icl_CreateSetWithFlags_r(name, baseLogp, fatalLogp, /*flags*/0, outSetpp);
    icl_UnmakePreemptionRight();
    return 0;
}

int
icl_CreateSetWithFlags(
  char *name,
  struct icl_log *baseLogp,
  struct icl_log *fatalLogp,
#ifdef SGIMIPS
  unsigned32 flags,
#else
  unsigned long  flags,
#endif
  struct icl_set **outSetpp)
{
    icl_MakePreemptionRight();
    icl_CreateSetWithFlags_r(name, baseLogp, fatalLogp, flags, outSetpp);
    icl_UnmakePreemptionRight();
    return 0;
}

static void FreeSet(struct icl_set *setp)
{
    osi_Free_r(setp->name, 1+strlen(setp->name));
    osi_Free_r(setp->eventFlags, ICL_EVENTBYTES(setp->nevents));
    if (setp->tidHT) {
	int size = ((sizeof(setp->perThreadLog[0]) +
		     sizeof(setp->tidHT[0]) +
		     sizeof(setp->tid[0]))
		    << ICL_LOGPERTHREAD);
	osi_Free_r(setp->tidHT, size);
    }
    osi_Free_r(setp, sizeof(struct icl_set));
}

static void UnthreadSet(struct icl_set *setp)
{
    struct icl_set **lpp, *tp;
    int i;

    for (lpp = &icl_allSets, tp = *lpp; tp; lpp = &tp->nextp, tp = *lpp) {
	if (tp == setp) {
	    /* found the dude we want to remove */
	    *lpp = setp->nextp;
	    break;			/* won't find it twice */
	}
    }
    afsl_Assert (tp == setp);		/* we found the set we were seeking */

    for (i = 0; i < ICL_LOGSPERSET; i++) {
	if (setp->logs[i] != NULL)
	    icl_LogReleNL_r(setp->logs[i]);
    }
}

/* create a set, given pointers to base and fatal logs, if any.
 * Logs are unlocked, but referenced, and *outSetpp is returned
 * referenced.  Function bumps reference count on logs, since it
 * addds references from the new icl_set.  When the set is destroyed,
 * those references will be released.
 */
void icl_CreateSetWithFlags_r(
  char *name,
  struct icl_log *baseLogp,
  struct icl_log *fatalLogp,
#ifdef SGIMIPS
  unsigned32  flags,
#else
  unsigned long  flags,
#endif /* SGIMIPS */
  struct icl_set **outSetpp)
{
    struct icl_set *setp;
    struct icl_set *newsetp;
    int i;
#ifdef SGIMIPS
    signed32 states = ICL_DEFAULT_SET_STATES;
#else
    long states = ICL_DEFAULT_SET_STATES;
#endif

    if (!icl_inited) icl_Init();

    newsetp = osi_Alloc_r(sizeof(struct icl_set));
    bzero((caddr_t)newsetp, sizeof(*newsetp));
    newsetp->name = osi_Alloc_r(strlen(name)+1);
    strcpy(newsetp->name, name);
    newsetp->nevents = ICL_DEFAULTEVENTS;
    newsetp->eventFlags = osi_Alloc_r(ICL_DEFAULTEVENTS);
    if (flags & ICL_CRSET_FLAG_PERTHREAD) {
	int size = (sizeof(newsetp->perThreadLog[0]) +
		    sizeof(newsetp->tidHT[0]) +
		    sizeof(newsetp->tid[0]))
	    << ICL_LOGPERTHREAD;
	char *data = osi_Alloc_r(size);
	bzero(data, size);
	newsetp->tidHT = (long *)data;
	newsetp->tid = (long *)(&newsetp->tidHT[1<<ICL_LOGPERTHREAD]);
	newsetp->perThreadLog =
	    (struct icl_log **)(&newsetp->tid[1<<ICL_LOGPERTHREAD]);
	afsl_Assert (((char *)(&newsetp->perThreadLog[1<<ICL_LOGPERTHREAD]) -
		      data) == size);
    }

    osi_mutex_enter(&icl_lock);

    for (setp = icl_allSets; setp; setp = setp->nextp) {
	if (strcmp(setp->name, name) == 0) {
	    setp->refCount++;
	    *outSetpp = setp;
	    if (flags & ICL_CRSET_FLAG_PERSISTENT) {
		osi_mutex_enter(&setp->lock);
		setp->states |= ICL_SETF_PERSISTENT;
		osi_mutex_exit(&setp->lock);
	    }
	    osi_mutex_exit(&icl_lock);
	    FreeSet (newsetp);		/* don't need this after all */
	    return;
	}
    }

    setp = newsetp;

    /* determine initial state */
    if (flags & ICL_CRSET_FLAG_DEFAULT_ON)
	states = ICL_SETF_ACTIVE;
    else if (flags & ICL_CRSET_FLAG_DEFAULT_OFF)
	states = ICL_SETF_FREED;
    if (flags & ICL_CRSET_FLAG_PERSISTENT)
	states |= ICL_SETF_PERSISTENT;
    if (states & ICL_SETF_FREED)
	states &= ~ICL_SETF_ACTIVE;	/* if freed, can't be active */
    setp->states = states;

    /* next lock is obtained in wrong order, hierarchy-wise, but
     * it doesn't matter, since no one can find this lock yet, since
     * the icl_lock is still held, and thus the obtain can't block. */
#if defined(SGIMIPS) && defined(_KERNEL)
    osi_mutex_init(&setp->lock, "icl_set");
#else
    osi_mutex_init(&setp->lock);
#endif
    osi_mutex_enter(&setp->lock);

    setp->refCount = 1;
    for (i = 0; i < ICL_DEFAULTEVENTS; i++)
	setp->eventFlags[i] = 0xff;	/* default to enabled */

    /* update this global info under the icl_lock */
    setp->nextp = icl_allSets;
    icl_allSets = setp;
    osi_mutex_exit(&icl_lock);

    /* set's basic lock is still held, so we can finish init */

    if (baseLogp) {
	setp->logs[0] = baseLogp;
	icl_LogHold(baseLogp);
	if (!(setp->states & ICL_SETF_FREED))
	    icl_LogUse_r(baseLogp);	/* log is actually being used */
    }
    if (fatalLogp) {
	setp->logs[1] = fatalLogp;
	icl_LogHold(fatalLogp);
	if (!(setp->states & ICL_SETF_FREED))
	    icl_LogUse_r(fatalLogp);	/* log is actually being used */
    }
    osi_mutex_exit(&setp->lock);

    *outSetpp = setp;
}

/* function to change event enabling information for a particular set */
int
icl_SetEnable(
  struct icl_set *setp,
#ifdef SGIMIPS
  signed32 eventID,
#else
  long eventID,
#endif /* SGIMIPS */
  int setValue)
{
    char *tp;

    osi_mutex_enter(&setp->lock);
    if (!ICL_EVENTOK(setp, eventID)) {
	osi_mutex_exit(&setp->lock);
	return -1;
    }
    tp = &setp->eventFlags[ICL_EVENTBYTE(eventID)];
    if (setValue)
	*tp |= ICL_EVENTMASK(eventID);
    else
	*tp &= ~(ICL_EVENTMASK(eventID));
    osi_mutex_exit(&setp->lock);
    return 0;
}

/* return indication of whether a particular event ID is enabled
 * for tracing.  If *getValuep is set to 0, the event is disabled,
 * otherwise it is enabled.  All events start out enabled by default.
 */
int
icl_GetEnable(
  struct icl_set *setp,
#ifdef SGIMIPS
  signed32 eventID,
#else
  long eventID,
#endif /* SGIMIPS */
  int *getValuep)
{
    osi_mutex_enter(&setp->lock);
    if (!ICL_EVENTOK(setp, eventID)) {
	osi_mutex_exit(&setp->lock);
	return -1;
    }
    if (setp->eventFlags[ICL_EVENTBYTE(eventID)] & ICL_EVENTMASK(eventID))
	*getValuep = 1;
    else
	*getValuep = 0;
    osi_mutex_exit(&setp->lock);
    return 0;
}

/* hold and release event sets */
void icl_SetHold(struct icl_set *setp)
{
    osi_mutex_enter(&icl_lock);
    setp->refCount++;
    osi_mutex_exit(&icl_lock);
}

/* icl_ZapSet_r -- frees a set after removing it from the allSets list.
 *
 * LOCKS USED -- Called with icl_lock locked */

void icl_ZapSet_r(struct icl_set *setp)
{
    UnthreadSet (setp);
    FreeSet (setp);
}

/* do the release, watching for deleted entries */
void icl_SetRele(struct icl_set *setp)
{
    int free = 0;

    icl_MakePreemptionRight();
    osi_mutex_enter(&icl_lock);
    if (--setp->refCount == 0 && (setp->states & ICL_SETF_DELETED)) {
	UnthreadSet (setp);
	free = 1;
    }
    osi_mutex_exit(&icl_lock);
    if (free)
	FreeSet (setp);
    icl_UnmakePreemptionRight();
}

/* find a set by name, returning it held */
struct icl_set *
icl_FindSet(char *name)
{
    struct icl_set *tp;
    osi_mutex_enter(&icl_lock);
    for (tp = icl_allSets; tp; tp = tp->nextp) {
	if (strcmp(tp->name, name) == 0) {
	    /* this is the dude we want */
	    tp->refCount++;
	    break;
	}
    }
    osi_mutex_exit(&icl_lock);
    return tp;
}

/* zero out all the logs in the set */
int icl_ZeroSet(struct icl_set *setp)
{
    int i;
    int code = 0;
    int tcode;
    struct icl_log *logp;

    osi_mutex_enter(&setp->lock);
    for (i = 0; i < ICL_LOGSPERSET; i++) {
	logp = setp->logs[i];
	if (logp) {
	    icl_LogHold(logp);
	    tcode = icl_ZeroLog(logp);
	    if (tcode != 0) code = tcode; /* save the last bad one */
	    icl_LogRele(logp);
	}
    }
    osi_mutex_exit(&setp->lock);
    return code;
}

/*
 * XXX Since we don't hold the icl_lock during the callback, there
 * is no guarantee that this will really find all the sets.  Also,
 * it would probably make more sense to use an iterator-style interface
 * that returns a different set each time that it is called and
 * maintains state in a cookie.
 */
int
icl_EnumerateSets(
  int (*aproc)(char *, char *, struct icl_set *),
  char *arock)
{
    struct icl_set *tp, *np;
    int code = 0;

    osi_mutex_enter(&icl_lock);
    tp = icl_allSets;
    while (tp != NULL && code == 0) {
	tp->refCount++;			/* hold this guy */
	osi_mutex_exit(&icl_lock);
	code = (*aproc)(tp->name, arock, tp);
	osi_mutex_enter(&icl_lock);
	np = tp->nextp;			/* tp may disappear next, but not np */
	if (--tp->refCount == 0 && (tp->states & ICL_SETF_DELETED))
	    icl_ZapSet_r(tp);
	tp = np;
    }
    osi_mutex_exit(&icl_lock);
    return code;
}

int
icl_AddLogToSet(struct icl_set *setp, struct icl_log *newlogp)
{
    int i;
    int code = -1;

    icl_MakePreemptionRight();
    osi_mutex_enter(&setp->lock);
    for (i = 0; i < ICL_LOGSPERSET; i++) {
	if (!setp->logs[i]) {
	    setp->logs[i] = newlogp;
	    code = i;
	    icl_LogHold(newlogp);
	    if (!(setp->states & ICL_SETF_FREED)) {
		/* bump up the number of sets using the log */
		icl_LogUse_r(newlogp);
	    }
	    break;
	}
    }
    osi_mutex_exit(&setp->lock);
    icl_UnmakePreemptionRight();
    return code;
}

int icl_SetSetStat(
  struct icl_set *setp,
  int op)
{
    int code;
    icl_MakePreemptionRight();
    code = icl_SetSetStat_r(setp, op);
    icl_UnmakePreemptionRight();
    return code;
}

int icl_SetSetStat_r(
  struct icl_set *setp,
  int op)
{
    int i;
#ifdef SGIMIPS
    signed32 code;
#else    
    long code;
#endif /* SGIMIPS */    
    struct icl_log *logp;

    osi_mutex_enter(&setp->lock);
    switch (op) {
    case ICL_OP_SS_ACTIVATE:	/* activate a log */
	/*
	 * If we are not already active, see if we have released
	 * our demand that the log be allocated (FREED set).  If
	 * we have, reassert our desire.
	 */
	if (!(setp->states & ICL_SETF_ACTIVE)) {
	    if (setp->states & ICL_SETF_FREED) {
		/* have to reassert desire for logs */
		for (i = 0; i < ICL_LOGSPERSET; i++) {
		    logp = setp->logs[i];
		    if (logp)
			icl_LogUse_r(logp);
		}
		setp->states &= ~ICL_SETF_FREED;
	    }
	    setp->states |= ICL_SETF_ACTIVE;
	}
	code = 0;
	break;

    case ICL_OP_SS_DEACTIVATE:	/* deactivate a log */
	/* this doesn't require anything beyond clearing the ACTIVE flag */
	setp->states &= ~ICL_SETF_ACTIVE;
	code = 0;
	break;

    case ICL_OP_SS_FREE:	/* deassert design for log */
	/*
	 * if we are already in this state, do nothing; otherwise
	 * deassert desire for log
	 */
	if (setp->states & ICL_SETF_ACTIVE)
	    code = EINVAL;
	else {
	    if (!(setp->states & ICL_SETF_FREED)) {
		for (i = 0; i < ICL_LOGSPERSET; i++) {
		    logp = setp->logs[i];
		    if (logp)
			icl_LogFreeUse_r(logp);
		}
		setp->states |= ICL_SETF_FREED;
	    }
	    code = 0;
	}
	break;

    default:
	code = EINVAL;
    }
    osi_mutex_exit(&setp->lock);
    return code;
}
