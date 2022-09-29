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

#ifndef __OLD_AIO_H__
#define __OLD_AIO_H__
/*
 * XXX This local version of the file is just to keep old binaries working
 * XXX it should go away as soon as possible 
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/timers.h>
#include <sys/fcntl.h>
/* This is from the 5.2 sys/signal.h */
#undef sigev_signo /* We need this version */
typedef struct old_sigevent {
	int		sigev_signo;
	union sigval	sigev_value;
} old_sigevent_t;
/*
 * aio - POSIX 1003.4 Draft 12
 */

typedef struct old_aiocb {
	/* defined by Posix.4 D12 */
	int	aio_fildes;	/* file descriptor to perform aio on */
	off_t	aio_offset;	/* file offset position */
	volatile void *aio_buf;
	size_t	aio_nbytes;
	int	aio_reqprio;	/* aio priority, larger values lowers pri */
	old_sigevent_t aio_sigevent;/* signal to be generated on completion */
	int	aio_lio_opcode;	/* opcode for lio_listio() call */
	/* SGI defined */
	size_t	aio_nobytes;	/* return bytes */
	int	aio_whence;	/* for seeking */
	int	aio_errno;	/* return error from this aio op */
	int	aio_ret;	/* returned status */
} old_aiocb_t;

/* for aio_cancel() return values */
#define OLD_AIO_CANCELED	1	/* cancelled operation */
#define OLD_AIO_NOTCANCELED 2	/* some ops not cancelled */
#define OLD_AIO_ALLDONE	3	/* all aio has completed */

/* for aiocb.aio_lio_opcode */
#define OLD_LIO_READ	1
#define OLD_LIO_WRITE	2
#define OLD_LIO_NOP		3

/* for lio_listio mode flag */
#define OLD_LIO_WAIT	1
#define OLD_LIO_NOWAIT	2

#define _OLD_AIO_LISTIO_MAX		255
#define _OLD_AIO_MAX		4
#define _OLD_AIO_PRIO_DELTA_MAX	100

extern void old_aio_init(void);
extern int old_aio_read(struct old_aiocb *);
extern int old_aio_write(struct old_aiocb *);
extern int old_lio_listio(int, struct old_aiocb *[], int, old_sigevent_t *);
extern int old_aio_cancel(int, struct old_aiocb *);
extern int old_aio_error(struct old_aiocb *);
extern ssize_t old_aio_return(struct old_aiocb *);
extern int old_aio_suspend(const struct old_aiocb *[], int, timespec_t *);
extern int old_aio_fsync(int, struct old_aiocb *);

#ifdef __cplusplus
}
#endif

#endif /* __OLD_AIO_H__ */

