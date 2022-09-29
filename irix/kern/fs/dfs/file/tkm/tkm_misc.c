/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_misc.c,v 65.5 1998/04/01 14:16:30 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 *
 *	tkm_init.c -- Initialization of TKM
 *
 *	Copyright (C) 1992, 1993, 1994, 1995 Transarc Corporation
 *	All rights reserved.
 */


#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/tpq.h>
#include <dcedfs/krpc_pool.h>
#include <dcedfs/icl.h>

#include "tkm_internal.h"
#include "tkm_tokens.h"
#include "tkm_ctable.h"
#include "tkm_recycle.h"
#include "tkm_volume.h"
#include "tkm_file.h"
#include "tkm_misc.h"
#ifdef SGIMIPS
#include "tkm_asyncgrant.h"
#include "tkm_revokes.h"
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_misc.c,v 65.5 1998/04/01 14:16:30 gwehrman Exp $")

static int tkm_initialized = 0;

osi_dlock_t tkm_init_lock; /*
			    * since this is extern it is inited full of
			    * 0s so we don't need to do a lock_Init on
			    */

osi_dlock_t tkm_expirationLock;

#ifdef KERNEL
#define TKM_EXPIRATION_SECS_DEFAULT  (2 * 60 * 60 ) /* 2 hours */
#define TKM_EXPIRATION_SECS_HARD_LOW_LIMIT	(20 * 60 ) /* 20 minutes */
#else /*KERNEL*/
#define TKM_EXPIRATION_SECS_DEFAULT  (15 )
#define TKM_EXPIRATION_SECS_HARD_LOW_LIMIT	(2)
#endif /*KERNEL*/

ulong tkm_expiration_secs = TKM_EXPIRATION_SECS_DEFAULT;
ulong tkm_expiration_default = TKM_EXPIRATION_SECS_DEFAULT;


#ifdef KERNEL

struct icl_set *tkm_iclSet = 0;
struct icl_set *tkm_grantIclSet = 0;
struct icl_set *tkm_conflictQIclSet = 0;

#define TKM_TPQ_MINTHREADS	(2)
#define TKM_TPQ_MEDMAXTHREADS	(2)
#define TKM_TPQ_HIGHMAXTHREADS	TKM_MAXPARALLELRPC
#define TKM_TPQ_THREADENNUI	(360)
#else /*KERNEL*/
#define TKM_TPQ_MINTHREADS	(2)
#define TKM_TPQ_MEDMAXTHREADS	(2)
#define TKM_TPQ_HIGHMAXTHREADS	TKM_MAXPARALLELRPC
#define TKM_TPQ_THREADENNUI	(30)
#endif /*KERNEL*/

void * tkm_threadPoolHandle;

/*
 * Sets a new value for the token expiration interval; returns the old value.
 * If newInterval is negative, this routine just returns the current value.
 */

#ifdef SGIMIPS
u_long tkm_SetTokenExpirationInterval(u_long newInterval)
#else
u_long tkm_SetTokenExpirationInterval(int newInterval)
#endif
{
  u_long	oldInterval;
  /* init the token manager, if needed */
  if (tkm_initialized == 0) {
    tkm_Init();
  }

  lock_ObtainWrite(&tkm_expirationLock);

  oldInterval = tkm_expiration_secs;

#ifdef SGIMIPS
/* Turn off "pointless comparison of unsigned integer with zero" error. */
#pragma set woff 1183
#endif
  if (newInterval >= 0) {
    /* if we're really going to do any modification */
    if (newInterval < TKM_EXPIRATION_SECS_HARD_LOW_LIMIT) {
      newInterval = TKM_EXPIRATION_SECS_HARD_LOW_LIMIT;
    }
    if (newInterval > TKM_EXPIRATION_SECS_DEFAULT) {
      newInterval = TKM_EXPIRATION_SECS_DEFAULT;
    }

    tkm_expiration_secs = newInterval;
  }
#ifdef SGIMIPS
#pragma reset woff 1183
#endif

  lock_ReleaseWrite(&tkm_expirationLock);
  return oldInterval;
}

/*
 * generates an expiration time - used only inside the token manager.
 */

u_long tkm_FreshExpTime(tkm_internalToken_t *tokenP)
{
  u_long		rtnVal;

  /*
   * The token manager must have been initialized before this call since it is
   * internal to the token manager.
   */

  lock_ObtainWrite(&tkm_expirationLock);
  rtnVal = ( ((tokenP->flags) & TKM_TOKEN_FOREVER) ? TKM_TOKEN_NOEXPTIME
				   : ( osi_Time() + tkm_expiration_secs) );

  lock_ReleaseWrite(&tkm_expirationLock);
  return rtnVal;
}

#ifdef KERNEL

/*
 * the afs_syscall interface to token manager control
 */
int afscall_tkm_control(long	controlSwitch,
			void *	parm2,
			void *	parm3,
			void *	parm4,
			int *	retValP)
{
  long	rtnVal = 0;
  long	new;
  u_long old;

  if (osi_suser(osi_getucred())) {	/* only root can run this code */
      if (retValP)
	  *retValP = 0;

      switch (controlSwitch) {
	case TKMOP_SET_EXP:
	  rtnVal = osi_copyin(parm2, (caddr_t)&new, sizeof (long));
	  if (rtnVal == 0) {
	      old = tkm_SetTokenExpirationInterval(new);
	      rtnVal = osi_copyout((caddr_t)&old, parm3, sizeof (long));
	  }
	  break;
	case TKMOP_SET_MAX_TOKENS:
	  rtnVal = osi_copyin(parm2, (caddr_t)&new, sizeof (long));
	  if (rtnVal == 0) {
#ifdef SGIMIPS
	      old = tkm_SetMaxTokens((int)new);
#else
	      old = tkm_SetMaxTokens(new);
#endif
	      rtnVal = osi_copyout((caddr_t)&old, parm3, sizeof (long));
	  }
	  break;
	case TKMOP_SET_MIN_TOKENS:
	  rtnVal = osi_copyin(parm2, (caddr_t)&new, sizeof (long));
	  if (rtnVal == 0) {
#ifdef SGIMIPS
	      old = tkm_SetMinTokens((int)new);
#else
	      old = tkm_SetMinTokens(new);
#endif
	      rtnVal = osi_copyout((caddr_t)&old, parm3, sizeof (long));
	  }
	  break;
	case TKMOP_SET_MAX_FILES:
	  rtnVal = osi_copyin(parm2, (caddr_t)&new, sizeof (long));
	  if (rtnVal == 0) {
#ifdef SGIMIPS
	      old = tkm_SetFileMax((int)new);
#else
	      old = tkm_SetFileMax(new);
#endif
	      rtnVal = osi_copyout((caddr_t)&old, parm3, sizeof (long));
	  }
	  break;
	case TKMOP_SET_MAX_VOLS:
	  rtnVal = osi_copyin(parm2, (caddr_t)&new, sizeof (long));
	  if (rtnVal == 0) {
#ifdef SGIMIPS
	      old = tkm_SetVolMax((int)new);
#else
	      old = tkm_SetVolMax(new);
#endif
	      rtnVal = osi_copyout((caddr_t)&old, parm3, sizeof (long));
	  }
	  break;
	default:
	  rtnVal = EINVAL;
      }
  } else {
    rtnVal = EPERM;
  }
#ifdef SGIMIPS
  return (int)rtnVal;
#else
  return rtnVal;
#endif
}
#endif /* defined(KERNEL) */

/*
 *
 * Routine that calls all TKM initialization routines.
 *
 */

#ifdef SGIMIPS
long tkm_Init(void)
#else
long tkm_Init()
#endif
{
  long			rtnVal = TKM_SUCCESS;
  struct icl_log *	logp;
  long			code;
  static char		routineName[] = "tkm_Init";


  lock_ObtainWrite(&tkm_init_lock);
  if (!tkm_initialized) {
      tkm_initialized = 1;
      tkm_InitConflictTable();
      tkm_InitRevokes();
      tkm_InitFile();
      tkm_InitVol();
      /* these create threads, so put the last */
      tkm_InitAsyncGrant();
      tkm_InitRecycle();
#ifdef KERNEL
      code = icl_CreateLog("cmfx", 0, &logp);
      if (code == 0) {
	  icl_CreateSet("tkm", logp, (struct icl_log *) 0, &tkm_iclSet);
	  icl_CreateSetWithFlags("tkm/grants", logp, (struct icl_log *) 0,
				 ICL_CRSET_FLAG_DEFAULT_OFF,
				 &tkm_grantIclSet);
	  icl_CreateSetWithFlags("tkm/conflictQ", logp, (struct icl_log *) 0,
				 ICL_CRSET_FLAG_DEFAULT_OFF,
				 &tkm_conflictQIclSet);
      }
      krpc_AddConcurrentCalls(/* client */ (TKM_MAXPARALLELRPC+1),
			      /* server */ 0);
#endif /*KERNEL*/
    tkm_threadPoolHandle = tpq_Init(TKM_TPQ_MINTHREADS,
				    TKM_TPQ_MEDMAXTHREADS,
				    TKM_TPQ_HIGHMAXTHREADS,
				    TKM_TPQ_THREADENNUI,
				    "tkm");
    osi_assert (tkm_threadPoolHandle != (void *)NULL);
  }
  lock_ReleaseWrite(&tkm_init_lock);
  return rtnVal;
}

#if TKM_DPRINT
#if KERNEL
/* stuff needed for tkm_dprintf */


int tkm_kernel_print = 0;
char tkm_debugBuffer1[10000];
char tkm_debugBuffer[10000];
int tkm_debugBufLen = 0;
osi_dlock_t tkm_debugBufferLock;

tkm_kdprintf(char *s, ...)
{
    va_list ap;
    char tmp[200];

    va_start(ap);
    vsprintf(tmp,s,ap);
    va_end(ap);
    lock_ObtainWrite(&tkm_debugBufferLock);
    tkm_debugBufLen += strlen(tmp);
    if (tkm_debugBufLen > 10000)
    {
	bcopy(tkm_debugBuffer, tkm_debugBuffer1, 10000);
	bzero(tkm_debugBuffer, 10000);
	tkm_debugBufLen=0;
    }
    strcat(tkm_debugBuffer, tmp);
    lock_ReleaseWrite(&tkm_debugBufferLock);
}
#else /*KERNEL*/
long tkm_debug=1;
#endif /* KERNEL */
#endif /* TKM_DPRINT */
