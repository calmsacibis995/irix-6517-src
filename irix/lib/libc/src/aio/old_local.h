/*************************************************************************
*                                                                        *
*               Copyright (C) 1992, Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.1 $ $Author: jeffreyh $"
#include <assert.h>
#include <bstring.h>
#include <ulocks.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <signal.h>

#define OLD_AIO_SPROC	PR_SALL|PR_NOLIBC
typedef struct old_aiolink {
        struct old_aiolink  *al_forw;
        struct old_aiolink  *al_back;
        old_aiocb_t *al_req;
        pid_t   al_spid;        /* slave pid handling request */
        pid_t   al_rpid;        /* requesting pid */
        int     al_op;          /* read or write */
	int     al_fd;          /* fd to do I/O on */
	int	al_sysprio;	/* combine proc's pri and aio_reqprio */
} old_aiolink_t;
/* errno.h change */
#define OLD_ECANCELED	1000
struct old_liocb {
	struct old_aiocb **list;
	int	cnt;
	old_sigevent_t *sig;
	int	ppid;
};


extern ulock_t _old_alock;		/* protects list of outstanding I/O */
extern struct old_aiolink _old_ahead;	/* outstanding aio requests list */
extern struct old_aiolink *_old_aiofree;/* head of free list  */
extern int _old_needwake;		/* someone is waiting on suspsema */
extern usema_t *_old_suspsema;
extern int _old_suspfd;			/* file descriptor for suspsema */
extern fd_set _old_suspset;			/* select fd set */
extern ulock_t _old_susplock;		/* protect needwake */


extern void __old_aio_init(int);
extern int _old_aqueue(struct old_aiocb *, int);
