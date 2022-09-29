/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: bomb_core.c,v 65.5 1998/04/01 14:15:45 gwehrman Exp $";
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
 * $Log: bomb_core.c,v $
 * Revision 65.5  1998/04/01 14:15:45  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	The null pointer is used to obtain the offset in the data structure
 * 	for the elements bpe_next and bpe_name.  However, the resultant
 * 	address was cast to an integer.  The complier complains because
 * 	pointers do not always fit in an integer.  However, in this case it
 * 	will always fit, so type cast the pointer to a long and then an int
 * 	to make the compiler happy.
 *
 * Revision 65.4  1998/03/23 16:28:37  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/12/16 17:05:32  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.2  1997/11/06  19:58:48  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:18:18  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.44.1  1996/10/02  17:03:36  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:04:41  damon]
 *
 * Revision 1.1.35.3  1994/07/13  22:19:12  devsrc
 * 	merged with bl-10
 * 	[1994/06/28  17:55:04  devsrc]
 * 
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  15:53:48  mbs]
 * 
 * Revision 1.1.35.2  1994/06/09  13:51:46  annie
 * 	fixed copyright in src/file
 * 	[1994/06/08  21:25:21  annie]
 * 
 * Revision 1.1.35.1  1994/02/04  20:06:02  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:06:32  devsrc]
 * 
 * Revision 1.1.33.1  1993/12/07  17:12:44  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/06  13:33:33  jaffe]
 * 
 * $EndLog$
 */

/*
 * Copyright (C) 1995, 1993 Transarc Corporation - All rights reserved
 */

#include <dcedfs/osi.h>
#include <dcedfs/hash.h>
#include <dcedfs/lock.h>

#include <bomb.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/bomb/RCS/bomb_core.c,v 65.5 1998/04/01 14:15:45 gwehrman Exp $")


/* Undo the convenient defines from bomb.p.h */
#ifdef KERNEL
#undef BOMB_EXP_TYPE_ERRNO
#undef BOMB_EXP_TYPE_EXCEPTION
#undef BOMB_EXP_TYPE_EXIT
#undef BOMB_EXP_TYPE_SIGNAL
#endif


typedef struct bombPointEntry {
    struct lock_data	bpe_lock;
    dfsh_hashEntry_t	bpe_next;

    int			bpe_isActive;
    int			bpe_shouldExplode;
    char		bpe_name[BOMB_MAX_NAME+1];

    bombPoint_t		bpe_bombPoint;
}	bombPointEntry_t;

typedef enum {NO_SLEEP, DROP_READ, DROP_WRITE}	lookupType_t;


#ifndef KERNEL
static void			DummySigHandler(int);
static long			ParseAndSet(char*);
static long			ParseDesc(char*, char*, bombPoint_t*);
#endif
static int			Enter(bombPointEntry_t*);
static void			FreeBombPoint(bombPointEntry_t*);
static unsigned long		Hash(char*);
static void			Init(void);
static bombPointEntry_t*	Lookup(char*, lookupType_t);
#ifdef SGIMIPS
static void			Msg(char *, ...);	/* No prototype */
#else
static void			Msg();	/* No prototype */
#endif
static bombPointEntry_t*	NewBombPoint(char*);
static void			Remove(bombPointEntry_t*);
static long			Set(char*, bombPoint_t*);


/*
 * The following lock protects the `initted' variable.  We assume that
 * declaring it as "static", and thereby setting it to zero(s), will be good
 * enough to initialize it; otherwise, we'd have a serious chicken-and-egg
 * problem.
 */
/* In the SunOS5 kernel all these locks are below the preemption lock so we use
 * the "_r" versions of lock_Obtain/Release and osi_Alloc/Free. */

static struct lock_data		initLock;
static int			initted = 0;

/* The following lock protects `hashTable' */
static struct lock_data		hashTableLock;
static struct dfsh_hashTable	hashTable;

/* The following lock protects `_bomb_active' */
static struct lock_data		bombActiveLock;
extern unsigned long		_bomb_active = 1;


/******************************************************************************
 * Exported routines
 *****************************************************************************/
#ifndef KERNEL
/*
 * bomb_ParseDesc()
 */
extern long
bomb_ParseDesc(char* bpDesc, char* bpName, bombPoint_t* bpP)
{
    long	code = 0;

    Init();

    code = ParseDesc(bpDesc, bpName, bpP);
    return code;
}	/* bomb_ParseDesc() */
#endif	/* KERNEL */


/*
 * bomb_Set()
 */
extern long
bomb_Set(char* bpName, bombPoint_t* bpP)
{
    long code;

    Init();

    code = Set(bpName, bpP);
    return code;
}	/* bomb_Set() */


#ifndef KERNEL
/*
 * bomb_SetDesc()
 */
extern long
bomb_SetDesc(char* bpDesc)
{
    bombPoint_t	bp;
    char	bpName[BOMB_MAX_NAME + 1];
    long	code;

    Init();

    if (code = ParseDesc(bpDesc, bpName, &bp))
	return code;

    code = Set(bpName, &bp);
    return code;
}	/* bomb_Set() */
#endif	/* KERNEL */


/*
 * bomb_Unset()
 */
extern long
bomb_Unset(char* bpName)
{
    bombPointEntry_t*	bpeP;
    long		code = 0;

    Init();

    lock_ObtainWrite_r(&hashTableLock);

    if ((bpeP = Lookup(bpName, DROP_WRITE)) == NULL)
	code = BOMB_E_BOMB_POINT_NOT_SET;
    else {
	code = 0;
	if (bpeP->bpe_isActive) {
	    lock_ObtainWrite_r(&bombActiveLock);

	    afsl_PAssertTruth(_bomb_active > 0,
		       ("Number of active bomb points should be positive: %lu",
			_bomb_active));

	    --_bomb_active;

	    lock_ReleaseWrite_r(&bombActiveLock);
	}

	Remove(bpeP);

	lock_ReleaseWrite_r(&bpeP->bpe_lock);
    }

    lock_ReleaseWrite_r(&hashTableLock);

    if (bpeP) {
	/* free bomp point after dropping all locks */
	FreeBombPoint(bpeP);
    }

    return code;
}	/* bomb_Unset() */

/******************************************************************************
 * Exported (yet internal) routines that are used by the bomb point macros.
 *****************************************************************************/
/*
 * _bomb_Explode()
 */
extern long
_bomb_Explode(char* bpName, unsigned allowedExpTypes)
{
    bombPointEntry_t* volatile	bpeP;
    bombPoint_t*	bpP;
    volatile long	code = 0;
    int			isActive;

    Init();

    lock_ObtainRead_r(&hashTableLock);

    bpeP = Lookup(bpName, NO_SLEEP);

    lock_ReleaseRead_r(&hashTableLock);

    afsl_PAssertTruth(bpeP != NULL, ("Can't find bomb point: %s", bpName));

    bpP = (bombPoint_t *) &bpeP->bpe_bombPoint;

    afsl_PAssertTruth(bpP->bp_explosionType & allowedExpTypes,
		     ("Explosion type isn't valid at this bomb point: %s, %#x",
		      bpName, bpP->bp_explosionType));
    afsl_PAssertTruth(bpeP->bpe_isActive, ("Bomb point should be active: %s",
					   bpName));
    afsl_PAssertTruth(bpeP->bpe_shouldExplode,
		      ("Bomb point should be set to explode: %s", bpName));

    Msg("BOMB: Hit a bomb (%s)--", bpName);

#ifndef KERNEL
    /* We may raise an exception, so we need a TRY-FINALLY-ENDTRY wrapper */
    TRY {
#endif

	switch(bpP->bp_explosionType) {
	  case BOMB_EXP_TYPE_ABORT:
	    Msg("aborting\n");
	    panic((char *) bpeP->bpe_name);
	    break;
	    
	  case BOMB_EXP_TYPE_DELAY:
	    Msg("delaying for %u second%s...",
		bpP->bp_delay, bpP->bp_delay == 1 ? "" : "s");
	    osi_Pause(bpP->bp_delay);
	    Msg("done\n");
	    break;
	    
	  case BOMB_EXP_TYPE_ERROR:
	    Msg("returning code %d\n", (int)bpP->bp_errorCode);
	    code = bpP->bp_errorCode;
	    break;
	    
	  case BOMB_EXP_TYPE_IF:
	    Msg("executing alternate code path\n");
	    code = 1;
	    break;
	    
	  case BOMB_EXP_TYPE_RETURN:
	    Msg("returning %d\n", (int)bpP->bp_returnVal);
	    code = bpP->bp_returnVal;
	    break;
	    
#ifndef KERNEL
	  case BOMB_EXP_TYPE_WAIT:
	    {
		sigset_t	sigmask;
	    
		Msg("waiting; send signal %d to continue...", SIGUSR2);
		(void)sigemptyset(&sigmask);
		(void)sigaddset(&sigmask, SIGUSR2);
		code = sigwait(&sigmask);
		afsl_PAssertTruth(code != -1,
				  ("Failed to wait for signal, errno = %d",
				   errno));
		afsl_PAssertTruth(code == SIGUSR2,
				  ("Wrong signal was intercepted, signo = %d",
				   code));
		Msg("done.\n");
	    }
	    break;

	  case BOMB_EXP_TYPE_ERRNO:
	    Msg("setting errno to %d and returning -1\n", bpP->bp_errno);
	    errno = bpP->bp_errno;
	    code = -1;
	    break;
	    
	  case BOMB_EXP_TYPE_EXCEPTION:
	    {
		EXCEPTION	exception;
	    
		Msg("raising status exception with value %d\n",
		    (int)bpP->bp_excVal);
		EXCEPTION_INIT(exception);
		exc_set_status(&exception, bpP->bp_excVal);
		RAISE(exception);
	    }
	    break;
	    
	  case BOMB_EXP_TYPE_EXIT:
	    Msg("exiting with %d\n", bpP->bp_exitVal);
	    exit(bpP->bp_exitVal);
	    break;
	    
	  case BOMB_EXP_TYPE_SIGNAL:
	    Msg("sending signal %d\n", bpP->bp_signal);
	    code = kill(getpid(), bpP->bp_signal);
	    afsl_PAssertTruth(code != -1,
			      ("Failed to send signal: %s",
			       dfs_dceErrTxt(errno)));
	    break;
#endif	/* KERNEL */
	    
	  default:
	    afsl_PAssertTruth(0, ("Bogus explosion type: %#x",
				  bpP->bp_explosionType));
	    break;
	}

#ifndef KERNEL
    } FINALLY {
#endif
	
	switch (bpP->bp_triggerType) {
	  case BOMB_TRIG_TYPE_COUNTDOWN:
	    if (bpP->bp_refreshCount != 0)
		bpP->bp_count = bpP->bp_refreshCount;
	    else
		bpeP->bpe_isActive = 0;
	    break;
	    
	  case BOMB_TRIG_TYPE_RANDOM:
	    if (bpP->bp_refreshRandom != 0)
		bpP->bp_random = bpP->bp_refreshRandom;
	    else
		bpeP->bpe_isActive = 0;
	    break;
	    
	  case BOMB_TRIG_TYPE_TIMER:
	    if (bpP->bp_refreshTimer.tv_sec != 0
		|| bpP->bp_refreshTimer.tv_usec != 0)
		bpP->bp_timer = bpP->bp_refreshTimer;
	    else
		bpeP->bpe_isActive = 0;
	    break;
	    
	  default:
	    afsl_PAssertTruth(0, ("Bogus trigger type: %d",
				  bpP->bp_triggerType));
	    break;
	}

	if (!bpeP->bpe_isActive) {
	    lock_ObtainWrite_r(&bombActiveLock);

	    afsl_PAssertTruth(_bomb_active > 0,
			      ("Num. active bomb points not positive: %lu",
			       _bomb_active));

	    --_bomb_active;

	    lock_ReleaseWrite_r(&bombActiveLock);
	}

	bpeP->bpe_shouldExplode = 0;
	osi_Wakeup((caddr_t) &bpeP->bpe_shouldExplode);

	lock_ReleaseWrite_r((osi_dlock_t *) &bpeP->bpe_lock);

#ifndef KERNEL
    } ENDTRY
#endif

    return code;
}	/* _bomb_Explode() */


/*
 * _bomb_ShouldExplode()
 */
extern int
_bomb_ShouldExplode(char* bpName, char* file, int line)
{
    bombPointEntry_t*	bpeP;
    bombPoint_t*	bpP;
    int			shouldExplode = 0;

    Init();

    lock_ObtainRead_r(&hashTableLock);

    bpeP = Lookup(bpName, DROP_READ);

    lock_ReleaseRead_r(&hashTableLock);

    if (bpeP == NULL)
	return 0;

    afsl_PAssertTruth(bpeP->bpe_shouldExplode == 0,
		      ("Bomb point should not be set to explode: %s",
		       bpeP->bpe_name));

    bpP = &bpeP->bpe_bombPoint;

    if (bpeP->bpe_isActive) {
	switch (bpP->bp_triggerType) {
	  case BOMB_TRIG_TYPE_COUNTDOWN:
	    --bpP->bp_count;

	    afsl_PAssertTruth(bpP->bp_count >= 0,
			      ("Bomb point count is less than zero: %s, %d",
			       bpeP->bpe_name, bpP->bp_count));

	    if (bpP->bp_count == 0)
		shouldExplode = 1;

	    break;

	  case BOMB_TRIG_TYPE_RANDOM:
	  case BOMB_TRIG_TYPE_TIMER:
	    /* XXX */

	  default:
	    afsl_PAssertTruth(0,
			      ("Bogus trigger type: %d", bpP->bp_triggerType));
	    break;
	}
    }

    if (shouldExplode) {
	/*
	 * Leave bomb point entry locked.  It is _bomb_Explode's responsibility
	 * to unlock it.
	 */
	bpeP->bpe_shouldExplode = 1;

	Msg("BOMB: Explosion imminent in %s at line %d\n",
		      file, line);
    } else {
	lock_ReleaseWrite_r(&bpeP->bpe_lock);
    }

    return shouldExplode;
}	/* _bomb_ShouldExplode() */


/******************************************************************************
 * Internal utilities
 *****************************************************************************/
#ifndef KERNEL
static void
DummySigHandler(int signo)
{
    ;
}	/* DummySigHandler() */
#endif	/* KERNEL */


static void
FreeBombPoint(bombPointEntry_t* bpeP)
{
    osi_Free_r(bpeP, sizeof *bpeP);
}	/* FreeBombPoint() */


static void
Init(void)
{
    long	code;

    lock_ObtainWrite_r(&initLock);

    if (initted) {
	lock_ReleaseWrite_r(&initLock);
	return;
    }

    /* Initialize bomb point hash table */
#ifdef SGIMIPS
    code = dfsh_HashInit(&hashTable, Hash,
			 (int)(long)&((bombPointEntry_t*)0)->bpe_next);
#else
    code = dfsh_HashInit(&hashTable, Hash,
			 (int)&((bombPointEntry_t*)0)->bpe_next);
#endif
    afsl_PAssertTruth(!code, ("Failed to initialize hash table"));

#ifdef SGIMIPS
    code = dfsh_HashInitKeyString(&hashTable,
				  (int)(long)&((bombPointEntry_t*)0)->bpe_name);
#else
    code = dfsh_HashInitKeyString(&hashTable,
				  (int)&((bombPointEntry_t*)0)->bpe_name);
#endif
    afsl_PAssertTruth(!code, ("Failed to initialize hash table key type"));

    /* Initialize lock that covers hash table */
    lock_Init(&hashTableLock);

    /* Initialize lock that covers `_bomb_active' */
    lock_Init(&bombActiveLock);
    _bomb_active = 0;

#ifndef KERNEL
    /*
     * We're in user space--see if any bomb points are being set via
     * the environment.
     */
    {
	char*		bombPoints = getenv(BOMB_ENV_VAR_NAME);
    
	if (bombPoints != NULL) {
	    code = ParseAndSet(bombPoints);
	    afsl_PAssertTruth(code == 0,
			      ("Failed to set bomb points: %s",
			       dfs_dceErrTxt(code)));
	}
    }
#endif	/* KERNEL */

    initted = 1;

    lock_ReleaseWrite_r(&initLock);
}	/* Init() */


static void
Msg(fmt, p1, p2, p3, p4)		/* No prototype */
    char*	fmt;
    long	p1, p2, p3, p4;
{
#ifdef KERNEL
    (void)printf(fmt, p1, p2, p3, p4);
#else
    (void)fprintf(stderr, fmt, p1, p2, p3, p4);
#endif
}	/* Msg() */


static bombPointEntry_t*
NewBombPoint(char* bpName)
{
    bombPointEntry_t*	bpeP;

    bpeP = osi_Alloc_r(sizeof(bombPointEntry_t));
    bzero((char*)bpeP, sizeof *bpeP);
    (void)strcpy(bpeP->bpe_name, bpName);
    lock_Init(&bpeP->bpe_lock);

    return bpeP;
}	/* NewBombPoint() */


#ifndef KERNEL
static long
ParseAndSet(char* bombPoints)
{
    bombPoint_t	bp;
    char	bpName[BOMB_MAX_NAME + 1];
    long	code = 0;
    char*	tempBombPoints;
    char*	x;
    char*	y;

    if (bombPoints == NULL)
	return 0;

    tempBombPoints = osi_Alloc_r(strlen(bombPoints) + 1);
    (void)strcpy(tempBombPoints, bombPoints);
    x = tempBombPoints;

    while (x != NULL && *x != '\0') {
	y = strchr(x, ';');
	if (y != NULL)
	    *y++ = '\0';

	if (code = ParseDesc(x, bpName, &bp))
	    goto done;

	if (code = Set(bpName, &bp))
	    goto done;

	x = y;
    }
    
done:
    osi_Free_r(tempBombPoints, strlen(bombPoints) + 1);
    return code;
}	/* ParseAndSet() */
#endif	/* KERNEL */


#ifndef KERNEL
/*
 *	Parse bomb point descriptions of the form:
 *
 *	BP_DESC ->	    <name>':'EXP_DESC':'TRIG_DESC
 *
 *	EXP_DESC ->	    SIMPLE_EXP_NAME | COMLEX_EXP_NAME=<value>
 *
 *	SIMPLE_EXP_NAME ->  abort | if | wait
 *
 *	COMPLEX_EXP_NAME -> delay | errno | error | exception | exit | return |
 *			    signal
 *
 *	TRIG_DESC ->	    TRIG_NAME=<value>+<value>
 *
 *	TRIG_NAME ->	    count | random | timer
 *
 *	Examples: "bombpoint#1:abort,count=2+2"
 *		  "bombpoint#2:error=88,count=1+0"
 */
static long
ParseDesc(char* bpDesc, char* bpName, bombPoint_t* bpP)
{
    long	code = 0;
    bombPoint_t	tempBP;
    char*	tempDesc;
    char	tempName[BOMB_MAX_NAME + 1];
    long	val1;
    long	val2;
    char*	x;
    char*	y;
    char*	z;
    
    tempDesc = osi_Alloc_r(strlen(bpDesc) + 1);

    (void)strcpy(tempDesc, bpDesc);
    x = tempDesc;

    y = strchr(x, ':');
    if (y == NULL) {
 	code = BOMB_E_MALFORMED_BOMB_POINT;
	goto done;
    }

    *y++ = '\0';
    if (strlen(x) > (size_t)BOMB_MAX_NAME) {
	code = BOMB_E_NAME_TOO_LONG;
	goto done;
    }

    (void)strcpy(tempName, x);

    x = y;

    y = strchr(x, ':');
    if (y == NULL) {
	code = BOMB_E_MALFORMED_BOMB_POINT;
	goto done;
    }

    *y++ = '\0';

    bzero((caddr_t)&tempBP, sizeof tempBP);

    z = strchr(x, '=');
    if (z == NULL) {
	if (strcmp("abort", x) == 0)
	    tempBP.bp_explosionType = BOMB_EXP_TYPE_ABORT;
	else if (strcmp("if", x) == 0)
	    tempBP.bp_explosionType = BOMB_EXP_TYPE_IF;
	else if (strcmp("wait", x) == 0)
	    tempBP.bp_explosionType = BOMB_EXP_TYPE_WAIT;
	else {
	    code = BOMB_E_MALFORMED_BOMB_POINT;
	    goto done;
	}
    } else {
	*z++ = '\0';
	val1 = strtol(z, NULL, 0);

	if (strcmp("delay", x) == 0) {
	    tempBP.bp_explosionType = BOMB_EXP_TYPE_DELAY;
	    tempBP.bp_delay = val1;
	} else if (strcmp("return", x) == 0) {
	    tempBP.bp_explosionType = BOMB_EXP_TYPE_RETURN;
	    tempBP.bp_returnVal = val1;
	}
#ifndef KERNEL
	else if (strcmp("errno", x) == 0) {
	    tempBP.bp_explosionType = BOMB_EXP_TYPE_ERRNO;
	    tempBP.bp_errno = val1;
	} else if (strcmp("error", x) == 0) {
	    tempBP.bp_explosionType = BOMB_EXP_TYPE_ERROR;
	    tempBP.bp_errorCode = val1;
	} else if (strcmp("exception", x) == 0) {
	    tempBP.bp_explosionType = BOMB_EXP_TYPE_EXCEPTION;
	    tempBP.bp_excVal = val1;
	} else if (strcmp("exit", x) == 0) {
	    tempBP.bp_explosionType = BOMB_EXP_TYPE_EXIT;
	    tempBP.bp_exitVal = val1;
	} else if (strcmp("signal", x) == 0) {
	    tempBP.bp_explosionType = BOMB_EXP_TYPE_SIGNAL;
	    tempBP.bp_signal = val1;
	}
#endif	/* KERNEL */
	else {
	    code = BOMB_E_MALFORMED_BOMB_POINT;
	    goto done;
	}
    }

    x = y;
    y = strchr(x, '=');
    if (y == NULL) {
	code = BOMB_E_MALFORMED_BOMB_POINT;
	goto done;
    }

    *y++ = '\0';
    z = strchr(y, '+');
    if (z == NULL) {
	code = BOMB_E_MALFORMED_BOMB_POINT;
	goto done;
    }

    *z++ = '\0';

    val1 = strtol(y, NULL, 0);
    val2 = strtol(z, NULL, 0);

    if (strcmp("count", x) == 0) {
	tempBP.bp_triggerType = BOMB_TRIG_TYPE_COUNTDOWN;
	tempBP.bp_count = val1;
	tempBP.bp_refreshCount = val2;
    } else if (strcmp("random", x) == 0) {
	tempBP.bp_triggerType = BOMB_TRIG_TYPE_RANDOM;
	tempBP.bp_random = val1;
	tempBP.bp_refreshRandom = val2;
    } else {
	code = BOMB_E_MALFORMED_BOMB_POINT;
	goto done;
    }

    (void)strcpy(bpName, tempName);
    *bpP = tempBP;

done:
    osi_Free_r(tempDesc, strlen(bpDesc) + 1);
    return code;
}	/* bomb_ParseDesc() */
#endif	/* KERNEL */


static long
Set(char* name, bombPoint_t* bpP)
{
    long		code = 0;
    bombPointEntry_t*	bpeP;

    if (strlen(name) > (size_t)BOMB_MAX_NAME)
	return BOMB_E_NAME_TOO_LONG;

    lock_ObtainWrite_r(&hashTableLock);

    if ((bpeP = Lookup(name, DROP_WRITE)) == NULL) {
	bombPointEntry_t*	newbpeP;
	int			result;

	newbpeP = NewBombPoint(name);

	result = Enter(newbpeP);
	afsl_PAssertTruth(result != -1,
			  ("Bomb point should not exist: %s", name));

	bpeP = newbpeP;
	lock_ObtainWrite_r(&bpeP->bpe_lock);
    }

    lock_ReleaseWrite_r(&hashTableLock);

    switch (bpP->bp_explosionType) {
      default:
	code = BOMB_E_BAD_EXPLOSION_TYPE;
	break;

      case BOMB_EXP_TYPE_ABORT:
      case BOMB_EXP_TYPE_DELAY:
      case BOMB_EXP_TYPE_ERROR:
      case BOMB_EXP_TYPE_IF:
      case BOMB_EXP_TYPE_RETURN:
#ifndef KERNEL /* XXX */
      case BOMB_EXP_TYPE_WAIT:
#endif
#ifndef KERNEL
      case BOMB_EXP_TYPE_ERRNO:
      case BOMB_EXP_TYPE_EXCEPTION:
      case BOMB_EXP_TYPE_EXIT:
      case BOMB_EXP_TYPE_SIGNAL:
#endif	/* KERNEL */
	break;
    }

    if (!code) {
	int	wasActive;

	bpeP->bpe_bombPoint = *bpP;
	wasActive = bpeP->bpe_isActive;
	bpeP->bpe_isActive = 1;

	switch (bpP->bp_triggerType) {
	  case BOMB_TRIG_TYPE_COUNTDOWN:
	    if (bpP->bp_count == 0)
		bpeP->bpe_isActive = 0;
	    break;
	    
	  case BOMB_TRIG_TYPE_RANDOM:
	    if (bpP->bp_random == 0)
		bpeP->bpe_isActive = 0;
	    break;
	    
	  case BOMB_TRIG_TYPE_TIMER:
	    if (bpP->bp_timer.tv_sec == 0 && bpP->bp_timer.tv_usec == 0)
		bpeP->bpe_isActive = 0;
	    break;
	    
	  default:
	    code = BOMB_E_BAD_TRIGGER_TYPE;
	    bpeP->bpe_isActive = 0;
	    break;
	}
	
	if (wasActive != bpeP->bpe_isActive) {
	    lock_ObtainWrite_r(&bombActiveLock);
	    _bomb_active += (wasActive ? -1 : 1);
	    lock_ReleaseWrite_r(&bombActiveLock);
	}
    }
	
    lock_ReleaseWrite_r(&bpeP->bpe_lock);

    return code;
}	/* Set() */

/******************************************************************************
 * Internal utilities for hash table management
 *****************************************************************************/
static int
Enter(bombPointEntry_t* bpeP)
{
    afsl_PAssertTruth(bpeP->bpe_name[0] != '\0',
		      ("Bomb point name is not set"));
    return dfsh_HashIn(&hashTable, (dfsh_hashEntry_t)bpeP);
}	/* Enter() */


static unsigned long
Hash(dfsh_hashEntry_t hashEntry)
{
   register char*		p;
   register unsigned long	h = 0;
   register unsigned long	g;

   for (p = ((bombPointEntry_t*)hashEntry)->bpe_name; *p != '\0'; ++p) {
       h = (h << 4) + *p;
       if (g = h & 0xf0000000) {
	   h ^= g >> 24;
	   h ^= g;
       }
   }

   return h;
}	/* Hash() */


static bombPointEntry_t*
Lookup(char* bpName, lookupType_t lookupType)
{
    bombPointEntry_t*	bpeP;
    bombPointEntry_t	query;

    (void)strcpy(query.bpe_name, bpName);

    while (1) {
	bpeP = (bombPointEntry_t*)dfsh_HashLookup(&hashTable,
						  (dfsh_hashEntry_t)&query);
	if (!bpeP || lookupType == NO_SLEEP)
	    break;

	lock_ObtainWrite_r(&bpeP->bpe_lock);

	if (!bpeP->bpe_shouldExplode)
	    break;

	afsl_PAssertTruth(lookupType == DROP_READ || lookupType == DROP_WRITE,
			  ("Bogus lookup type: %d", lookupType));

	if (lookupType == DROP_READ)
	    lock_ReleaseRead_r(&hashTableLock);
	else
	    lock_ReleaseWrite_r(&hashTableLock);

	osi_SleepW(&bpeP->bpe_shouldExplode, &bpeP->bpe_lock);

	if (lookupType == DROP_READ)
	    lock_ObtainRead_r(&hashTableLock);
	else
	    lock_ObtainWrite_r(&hashTableLock);
    }

    return bpeP;
}	/* Lookup() */


static void
Remove(bombPointEntry_t* bpeP)
{
    int	result;

    result = dfsh_HashOut(&hashTable, (dfsh_hashEntry_t)bpeP);
    afsl_PAssertTruth(result != -1, ("Bomb point expected in hash table: %s",
				     bpeP->bpe_name));
}	/* Remove() */
