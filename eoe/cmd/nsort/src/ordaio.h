/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.       *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 *
 * ordaio.h	- header for ordinal technology's asynchronous i/o library
 *
 *	Its aiocb's always have 64-bit offsets
 *
 *	$Ordinal-Id: ordaio.h,v 1.3 1996/10/28 04:25:21 chris Exp $
 *	$Revision: 1.1 $
 */
#ifndef _ORDAIO_H
#define _ORDAIO_H

#include <sys/types.h>
#include <sys/time.h>

/*
 * This stucture is the optional argument to aio_sgi_init. The defaults
 * that are used if NULL is given as the argument are in parentheses at the
 * end of each comment.
 */
typedef struct aioinit {
    int aio_threads;	/* The number of threads started (5) */
    int aio_locks;	/* Initial number of preallocated locks (3) */
    int aio_num;	/* estimated total simultanious aiocb structs (1000) */
    int aio_debug;	/* turn on debugging (0) */
    int aio_numusers;	/* max number of user sprocs making aio_* calls (5) */
    int aio_reserved[3];		
} aioinit_t;

typedef struct aiocb64 {
	int	aio_fildes;	/* file descriptor to perform aio on */
	off64_t	aio_offset;	/* file offset position */
	volatile void *aio_buf;
	size_t	aio_nbytes;	/* #bytes requested */
	int	aio_lio_opcode;	/* opcode for lio_listio() call */
	size_t	aio_nobytes;	/* #bytes returned? */
	off64_t	aio_filesize;	/* mapped reading needs to know where eof is */
	int	aio_errno;	/* return error from this aio op */
	int	aio_ret;	/* returned status */
} aiocb_t;

extern void	ordaio_sgi_init(aioinit_t *);
extern int	ordaio_read(aiocb_t *);
extern int	ordaio_write(aiocb_t *);
extern int	ordaio_nop(aiocb_t *);
extern int	ordaio_fsync(aiocb_t *);
extern int	ordaio_cancel(int, aiocb_t *);
extern int	ordaio_error(const aiocb_t *);
extern ssize_t	ordaio_return(aiocb_t *);
extern int	ordaio_suspend(aiocb_t *const[], int, const timespec_t *);
extern int	ord_read(aiocb_t *);
extern int	ord_write(aiocb_t *);

extern int	ordaio_map_rdonly(aiocb_t *);
extern int	ordaio_map_copyonwrite(aiocb_t *);
extern int	ord_map_copyonwrite(aiocb_t *);
extern int	ordaio_map_rdwrite(aiocb_t *);

extern void	ordaio_getrusage(struct rusage *total);

#define aio_read	ordaio_read
#define aio_write	ordaio_write
#define aio_nop		ordaio_nop
#define aio_fsync	ordaio_fsync
#define aio_suspend	ordaio_suspend
#define aio_return	ordaio_return
#define aio_error	ordaio_error
#define aio_cancel	ordaio_cancel
#define aio_sgi_init	ordaio_sgi_init

/*for aio_cancel() return values */
#define AIO_CANCELED	1	/* cancelled operation */
#define AIO_NOTCANCELED 2	/* some ops not cancelled */
#define AIO_ALLDONE	3	/* all aio has completed */

/* for aiocb.aio_lio_opcode */
#define LIO_READ		1
#define LIO_WRITE		2
#define LIO_NOP			3
#define LIO_FSYNC		4
#define	LIO_MAPRDONLY		5
#define	LIO_FIRSTMAP		LIO_MAPRDONLY
#define LIO_MAPCOPYONWRITE	6
#define LIO_MAPRDWRITE		7

#endif /* _ORDAIO_H_ */
