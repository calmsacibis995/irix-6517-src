/*************************************************************************
*                                                                        *
*               Copyright (C) 1992-1997 Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.20 $ $Author: jwag $"

#ifndef __AIO_H__
#define __AIO_H__

#ifdef __cplusplus
extern "C" {
#endif
    
#include <standards.h>
#include <sys/types.h>
#include <time.h>
#include <sys/timespec.h> /* for timespec_t in ANSI/XPG mode */
#include <sys/sigevent.h> /* for sigevent_t in ANSI/XPG mode */
#include <sys/signal.h>	
#include <fcntl.h>	

/*
 * aio - POSIX 1003.1b-1993
 * NOTE: watch for name space pollution.
 */

typedef struct aiocb {
    int	aio_fildes;		/* file descriptor to perform aio on */
    volatile void *aio_buf; 	/* Data buffer */
    size_t	aio_nbytes;	/* number of bytes of data */
    off_t	aio_offset;	/* file offset position */
    int		aio_reqprio;	/* aio priority, (Currently must be 0) */
    sigevent_t	aio_sigevent;	/* notification information */
    int		aio_lio_opcode;	/* opcode for lio_listio() call */
    unsigned long aio_reserved[7];/* reserved for internal use */
    unsigned long aio_pad[6];
} aiocb_t;

#if _LFAPI
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
typedef struct aiocb64 {
    int	aio_fildes;		/* file descriptor to perform aio on */
    volatile void *aio_buf; 	/* Data buffer */
    size_t	aio_nbytes;	/* number of bytes of data */

    off_t	aio_oldoff;	/* old: file offset position */
    int		aio_reqprio;	/* aio priority, (Currently must be 0) */
    sigevent_t	aio_sigevent;	/* notification information */
    int		aio_lio_opcode;	/* opcode for lio_listio() call */
    unsigned long aio_reserved[7];/* reserved for internal use */
    unsigned long aio_pad[6];
    off64_t	aio_offset;	/* file offset position */	
} aiocb64_t;
#else /*  (_MIPS_SIM == _MIPS_SIM_ABI32) */
typedef aiocb_t aiocb64_t;
#endif /*  (_MIPS_SIM == _MIPS_SIM_ABI32) */
#endif	/* _LFAPI */

/* for aio_cancel() return values */
#define AIO_CANCELED	1	/* cancelled operation */
#define AIO_NOTCANCELED 2	/* some ops not cancelled */
#define AIO_ALLDONE	3	/* all aio has completed */

/* for aiocb.aio_lio_opcode */
#define LIO_NOP		0	/* listio request with no data */
#define LIO_READ	1	/* listio read request */
#define LIO_WRITE	2	/* listio write request */

/* for lio_listio mode flag */
#define LIO_WAIT	4	/* blocks until lio_listio complete */
#define LIO_NOWAIT	3	/* asynchronous lio_listio call, doesn't block */

/* for lio_hold routine */
#define AIO_HOLD_CALLBACK	1
#define AIO_RELEASE_CALLBACK	2
#define AIO_ISHELD_CALLBACK	3


#if _SGIAPI || _ABIAPI
/* These three defines are not for use by applications. */
#define _AIO_SGI_LISTIO_MAX	2048
#define _AIO_SGI_MAX		2048
#define _AIO_SGI_PRIO_DELTA_MAX	0

/*
 * This stucture is the optional argument to aio_sgi_init. The defaults
 * that are used if NULL is given as the argument are in parentheses at the
 * end of each comment.
 */
typedef struct aioinit {
    int aio_threads;	/* The number of aio threads to start (5) */
    int aio_locks;	/* Initial number of preallocated locks (3) */
    int aio_num;	/* estimated total simultanious aiobc structs (1000) */
    int aio_usedba;	/* Try to use DBA for raw I/O in lio_listio (0) */
    int aio_debug;	/* turn on debugging (0) */
    int aio_numusers;	/* max number of user sprocs making aio_* calls (5) */
    int aio_reserved[3];		
} aioinit_t;
extern void aio_sgi_init(aioinit_t *);
#if _LFAPI
extern void aio_sgi_init64(aioinit_t *);
#endif
#endif	/* _SGIAPI || _ABIAPI */

extern int aio_read(aiocb_t *);
extern int aio_write(aiocb_t *);
extern int lio_listio(int, aiocb_t * const [], int, sigevent_t *);
extern int aio_cancel(int, aiocb_t *);
extern int aio_error(const aiocb_t *);
extern ssize_t aio_return(aiocb_t *);
extern int aio_suspend(const aiocb_t * const [], int, const timespec_t *);
extern int aio_fsync(int, aiocb_t *);
extern int aio_hold(int);

#if _LFAPI
extern int aio_read64(aiocb64_t *);
extern int aio_write64(aiocb64_t *);
extern int lio_listio64(int, aiocb64_t * const [], int, sigevent_t *);
extern int aio_cancel64(int, aiocb64_t *);
extern int aio_error64(const aiocb64_t *);
extern ssize_t aio_return64(aiocb64_t *);
extern int aio_suspend64(const aiocb64_t * const [], int, const timespec_t *);
extern int aio_fsync64(int, aiocb64_t *);
extern int aio_hold64(int);
#endif	/* _LFAPI */

#ifdef __cplusplus
}
#endif

#endif /* __AIO_H__ */
