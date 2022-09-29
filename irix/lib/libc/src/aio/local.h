/*************************************************************************
*                                                                        *
*               Copyright (C) 1992, 1993, 1994 Silicon Graphics, Inc.  	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.21 $ $Author: kostadis $"
#include <assert.h>
#include <bstring.h>
#include <ulocks.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#if 1
#include <malloc.h> 
#else
#include <dbmalloc.h> /*XXX */
#endif


#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif
/* 
 * The struct aiocb has reserved space for internal use. Here we map
 * fields from the reserved space to names that have meaning in 
 * our library version.
 */
#define aio_nobytes aio_reserved[0]	/* return bytes */
#define aio_errno aio_reserved[1]	/* return error from this aio op */
#define aio_ret aio_reserved[2]		/* returned status */
#define aio_op aio_reserved[3]		/* What operation is this doing? */
#define aio_waithelp aio_reserved[4]	/* semaphore struct used 
					 * by aio_fsync & lio_listio */
#define aio_64bit aio_reserved[5]	/* used to decide where the offset lives */
#define aio_kaio aio_reserved[6]	/* used by kernel async i/o */

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
#define	AIO_IS64(a)	(((a)->aio_offset == (off_t)(-1)) && \
			 ((a)->aio_64bit == 0xdeadbeef))
#define	AIO_OFFSET(a)	(AIO_IS64(a) ? \
	((aiocb64_t *)(a))->aio_offset : (off64_t)((a)->aio_offset))
#else
#define	AIO_OFFSET(a)	((a)->aio_offset)
#endif

#define AIO_SPROC	PR_SALL|PR_NOLIBC

typedef struct aiolink {
    struct aiolink	*al_forw;
    struct aiolink	*al_back;
    aiocb_t		*al_req;
    int			al_busy;        /* slave pid handling request */
    pid_t		al_rpid;        /* requesting pid */
    int			al_sysprio;	/* combine proc's pri and aio_reqprio */
} aiolink_t;

struct liocb {
    struct aiocb **list;
    int	cnt;
    sigevent_t *sig;
    int	ppid;
};

/*
 * This is used to build aio_fsync and lio_listio. It has an initial count that
 * is decremented in _aiofreelink. When the count gets to 0, we do a 'V'
 * on the semaphore to let the waiter know that all the operations that it is 
 * waiting on are done. This could have been done with a normal semaphore,
 * except that we can not set a ussema* routine to an initially negative value.
 */
typedef struct aiowaitsema {
    int count;	
    int targcount;
    int freecount;
    union {
	    usema_t *us;
	    sem_t *posix;
    } sem;
    struct aiowaitsema *next;	/* Pointer to next for use in free list */
    int type;	/* This might be able to be combined with the next field */
} aiowaitsema_t;
#define sema sem.us
#define psema sem.posix
/*
 * This is a container structure. An aiocb may have multiple aiosemawait
 * structures associated with is (for example is someone has done a aio_fsync
 * on an fd that is being used for a lio_listio operation.
 * We use aiowait can to link them up in the aiocb.
 */
typedef struct aiowaitcan {
    struct aiowaitcan *next;
    aiowaitsema_t *aws;
    int id;
} aiowaitcan_t;

/*
 * Hold list, used in ahold.c. Head of list is _aioinfo->_aiocbholdlist
 */
typedef struct holdlist {
    struct holdlist *next;
    sigevent_t event;
} holdlist_t;

/*
 * gather all aio required variables into a single struct so we
 * can dynamically allocate it.
 * This saves space in the dso for folks that aren't
 * really using this stuff
 */

struct aioinfo {
    sem_t alock;		/* protects list of outstanding I/O */
    struct aiolink ahead;	/* outstanding r/w aio requests list */
    struct aiolink achead;	/* outstanding aio ctr requests list */
    usema_t *wsema;		/* I/O thread wait */
    int numwsema;		/* number of threads sleeping on wsema*/
    usema_t *wcsema;		/* ctr thread wait */
    int numwcsema;		/* number of threads sleeping on wcsema */
    aiolink_t *aiofree;		/* head of free list  */
    int alistfreecount;		/* count of available aio structs */
    sigset_t sigblockset;	/* set of 'in-use' for aio signals */
    ulock_t acblock;		/* protects list of outstanding callbacks */
    struct aiolink **activefds;	/* in progress I/O for O_APPEND writes */
    aiowaitsema_t *waitfree; 	/* Free list of ready aiowaitsema's */
    aiowaitsema_t *waitpollfree;/* Free list of ready aiowaitsema's (poll) */
    aiocb_t	*lioaiocbfree;	/* Free list used by lio_listio of aiocb */
    aiowaitcan_t *waitcanfree;	/* Free list of aiowaitcan structs */
    int maxfd;			/* maximum open file descriptors */
    usptr_t *ahandle;		/* Handle to arena for semas */
    int aiocbbeingheld;		/* Hold count */
    holdlist_t *aiocbholdlist;	/* list of callbacks blocked by aio_hold */
    aiolink_t alist[_AIO_SGI_MAX]; /* array of all aio structs */
    int debug;
    int all_init_done;		/* kaio vs. sproc initialization done */
    struct sh_aio_kaio *sh_aio_kaio;	/* shared data between kaio and aio */
};

/*
 * Kernel async i/o (in libdba.so)
 */
typedef struct kaio_ret {
     long long	rv;		/* value to return to user */
     int	back2user;	/* return to user(1), or continue libc(0) */
} kaio_ret_t;
struct sh_aio_kaio {
     struct aioinit ainit;
     struct kaio_functions {
	  /* Supported or partly-supported */
#define ARGS_TO_LIBDBA_INIT	struct aioinit *, struct aioinfo *, char *, int, struct sh_aio_kaio **, int (*)(int, int), kaio_ret_t *, int
	  void (*kaio_init_fn)(ARGS_TO_LIBDBA_INIT);
	  void (*kaio_read_fn)(const aiocb_t *, kaio_ret_t *);
	  void (*kaio_write_fn)(const aiocb_t *, kaio_ret_t *);
	  void (*kaio_submit_fn)(aiocb_t *, int, kaio_ret_t *);
	  void (*kaio_suspend_fn)(const aiocb_t * const [], int, struct timeval *, int, kaio_ret_t *);
	  void (*kaio_error_fn)(const aiocb_t *, kaio_ret_t *);
	  void (*kaio_return_fn)(aiocb_t *, kaio_ret_t *);
	  void (*kaio_lio_fn)(int, aiocb_t * const [], int, sigevent_t *, aiocb_t ***, int *, kaio_ret_t *);
	  /* Unsupported */
	  void (*kaio_cancel_fn)(int, aiocb_t *, kaio_ret_t *);
	  void (*kaio_fsync_fn)(int, aiocb_t *, kaio_ret_t *);
	  void (*kaio_hold_fn)(int, kaio_ret_t *);
	  /* Auxilliary functions */
	  void (*kaio_closechk_fn)(int);
	  void (*kaio_reset_fn)(void);
	  void (*kaio_realloc_fds_fn)(int, int, int, kaio_ret_t *);
	  void (*kaio_lio_free_fn)(aiocb_t *[]);
	  void (*kaio_user_active_fn)(int);
     } kaio_fns;
};

#if defined(_SGI_COMPILING_LIBC)
#define KFN(f)	(*_aioinfo->sh_aio_kaio->kaio_fns.f)
#define libdba_kaio_init	KFN(kaio_init_fn)
#define libdba_kaio_read	KFN(kaio_read_fn)
#define libdba_kaio_write	KFN(kaio_write_fn)
#define libdba_kaio_submit	KFN(kaio_submit_fn)
#define libdba_kaio_suspend	KFN(kaio_suspend_fn)
#define libdba_kaio_error	KFN(kaio_error_fn)
#define libdba_kaio_return	KFN(kaio_return_fn)
#define libdba_kaio_lio		KFN(kaio_lio_fn)
#define libdba_kaio_cancel	KFN(kaio_cancel_fn)
#define libdba_kaio_fsync	KFN(kaio_fsync_fn)
#define libdba_kaio_hold	KFN(kaio_hold_fn)
#define libdba_kaio_closechk	KFN(kaio_closechk_fn)
#define libdba_kaio_reset	KFN(kaio_reset_fn)
#define libdba_kaio_realloc_fds	KFN(kaio_realloc_fds_fn)
#define libdba_kaio_lio_free	KFN(kaio_lio_free_fn)
#define libdba_kaio_user_active	KFN(kaio_user_active_fn)

#define USR_AIO_IS_ACTIVE  _aioinfo->all_init_done
#endif

#define KAIO_IS_ACTIVE     (_aioinfo && (_aioinfo->sh_aio_kaio != 0))
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
#define DO_SPROC_INIT   -1LL
#else
#define DO_SPROC_INIT   -1L
#endif
#define INIT_DEFAULT		0
#define INIT_KAIO		1
#define INIT_FORCE_SPROCS	2
#define INIT_NORMAL		3

/* 
 * Function prototypes
 */
extern struct aioinfo *_aioinfo;
extern void __aio_init(int, int, int);
extern void __aio_init1(int, int, int, int);
extern void _aio_handle_notify(sigevent_t *, int );
extern void  _aio_call_callback(sigevent_t *);
extern int _aqueue(struct aiocb *, int, aiowaitsema_t *);
extern void _aiofreelink(struct aiolink *al);
extern void _aioworkcans(aiowaitcan_t *);
extern void _aio_grab_alock(sigset_t *); 
extern void _aio_release_alock(sigset_t *); 
extern int _aio_wait_help(aiocb_t *);
extern int _aiochkcnt(int);
extern aiowaitsema_t *_aio_get_waitsema(int);
extern void _aio_free_waitsema(aiowaitsema_t *);
extern aiocb_t *_aio_new_lioaiocb(void);
extern aiocb_t *_aio_get_lioaiocb(void);
extern void _aio_free_lioaiocb(aiocb_t *);
extern aiowaitcan_t *_aio_new_waitcan(void);
extern aiowaitcan_t *_aio_get_waitcan(int);
extern void _aio_free_waitcan(aiowaitcan_t *);
extern int _aio_realloc_fds(int, int);

/*
 * The default number of threads to start
 */
#define _SGI_AIO_THREADS 5

/*
 * The default number of wait help and lioaiocb structures to allocate.
 */
#define _SGI_AIO_INIT_LOCKS 0

/*
 * Default for the max user threads that can make aio_* calls.
 * This is needed to set the arena.
 */
#define _SGI_AIO_USER_THREADS 5

/*
 * These are private LIO_* values used as op arguments to _aqueue
 * If new values are added to aio.h then these need to be sure not
 * to to conflict
 */
#define LIO_SYNC	1256		/* for aio_fsync */
#define LIO_DSYNC	1257		/* for aio_fsync */

/*
 * These are used by the get/free waitsema routines
 */
#define SEMA_STD  1
#define SEMA_POLL 2
#define SEMA_NONE 3
					 

