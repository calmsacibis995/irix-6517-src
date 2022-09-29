/*************************************************************************
*                                                                        *
*               Copyright (C) 1998 Silicon Graphics, Inc.        	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.1 $ $Author: wombat $"

/*
 * gather all kaio required variables into a single struct so we
 * can dynamically allocate it.
 */

struct kaioinfo {
    char *aio_raw_fds;		/* raw devs open (1 byte/fd) */
    char *akcomplete;		/* track completed for aio_suspend */
    char *akwaiting;		/* track submitted for aio_suspend */
    struct aioinfo *aioinfo;	/* ptr to libc _aioinfo -- USE ONLY IN PATCHES */
    int (*aio_realloc_fds_fn)(int, int); /* internal libc fn to resize fd array */
    int akfd;			/* fd of /dev/kaio, used for aio_suspend */
    int maxfd;			/* maximum open file descriptors libdba knows */
    int aio_maxfd;		/* maximum open file descriptors libc knows */
    int maxaio;			/* max number of concurrent kaio (from kernel) */
    int kaio_from_init;		/* aio_usedba was set */
    int kaio_from_env;		/* __SGI_USE_DBA was set in environment */
    int user_active;		/* are there sprocs out there? */
    struct aioinit ainit;	/* save any user-supplied params */
#if 0
    sigset_t sigblkset;		/* signals to block while picking wait/comp ix */
#endif
    struct sh_aio_kaio sh_aio_kaio; /* shared data between libc and libdba */
    char *pad_safety[16];	/* in case someone increases sh_aio_kaio on us */

    /* For STATS */
    char *slot1;
    char *slot2;
    /* For generic debug */
    int debug;
    FILE *dfile;
    pid_t pid;
    /* For par-related DEBUGging */
    int aparfd;			/* fd of a descriptor aio_error will write on */
#define APARBSZ 30
    char aparbuf[APARBSZ];	/* buf aio_error will write into */
    int adoprnt;
#if 0
#define RETNUM 200
    int retinx;
    long long returnees[RETNUM][3];
#endif
};
#define aio_do_prints kaio_info->adoprnt

#if 0
#define LOCK_SECTION(s)		sigprocmask(SIG_SETMASK, &kaio_info->sigblkset, &s)
#define UNLOCK_SECTION(s)	sigprocmask(SIG_SETMASK, &s, NULL)
#endif

#define _AIO_SGI_KAIO_UNKNOWN	0
#define _AIO_SGI_KAIO_RAW	1
#define _AIO_SGI_KAIO_COOKED	2

#define KAIO_DFLT_NFDS		2500	/* a magic number in oracle */
#define _AIO_SGI_REALLOC_NOERR	0
#define _AIO_SGI_REALLOC_SIGERR	1

#define USR_AIO_IS_ACTIVE	(kaio_info->user_active)
#define _aio_realloc_fds	(*kaio_info->aio_realloc_fds_fn)

#define _AIO_SGI_ISKAIORAW(fd)	\
	(kaio_info->aio_raw_fds \
		 && (((fd) < kaio_info->maxfd) \
			? 1 : _aio_realloc_fds((fd), _AIO_SGI_REALLOC_NOERR)) \
		 && (kaio_info->aio_raw_fds[(fd)] == _AIO_SGI_KAIO_RAW))

#define MAYBE_REINIT_TO_FORCE_SPROCS { \
					   if (!USR_AIO_IS_ACTIVE) { \
						aio_sgi_init((struct aioinit *)DO_SPROC_INIT); \
					   } \
				      }

extern struct kaioinfo *kaio_info;
extern struct sh_aio_kaio *sh_aio_kaio;

/* 
 * Function prototypes
 */
void kaio_init(ARGS_TO_LIBDBA_INIT);
void kaio_read(const aiocb_t *, kaio_ret_t *);
void kaio_write(const aiocb_t *, kaio_ret_t *);
void kaio_submit(aiocb_t *, int, kaio_ret_t *);
void kaio_suspend(const aiocb_t * const [], int, struct timeval *, int, kaio_ret_t *);
void kaio_error(const aiocb_t *, kaio_ret_t *);
void kaio_return(aiocb_t *, kaio_ret_t *);
void kaio_lio(int, aiocb_t * const [], int, sigevent_t *, aiocb_t ***, int *, kaio_ret_t *);
void kaio_cancel(int, aiocb_t *, kaio_ret_t *);
void kaio_fsync(int, aiocb_t *, kaio_ret_t *);
void kaio_hold(int, kaio_ret_t *);
void _kaio_closechk(int);
void _kaio_reset(void);
void _kaio_realloc_fds(int, int, int, kaio_ret_t *);
void _kaio_lio_free(aiocb_t *[]);
void _kaio_user_active(int);
int _kaio_eligible(aiocb_t *);

/*
 * These are private LIO_* values used as op arguments to _aqueue
 * If new values are added to aio.h then these need to be sure not
 * to to conflict
 */
#define LIO_SYNC	1256		/* for aio_fsync */
#define LIO_DSYNC	1257		/* for aio_fsync */
					 

#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>=700)
#define SYNCHRONIZE()	__synchronize()
#define INLINE		__inline
#else
#define SYNCHRONIZE()
#define INLINE
#endif
