/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _IO_UIO_H	/* wrapper symbol for kernel use */
#define _IO_UIO_H	/* subject to change without notice */
#ident	"@(#)uts-3b2:io/uio.h	1.2"
#ident	"$Revision: 1.22 $"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * I/O parameter information.  A uio structure describes the I/O which
 * is to be performed by an operation.  Typically the data movement will
 * be performed by a routine such as uiomove(), which updates the uio
 * structure to reflect what was done.
 */

#include <sys/types.h>	/* REQUIRED */

#if !defined(_IOVEC_T)
#define _IOVEC_T
typedef struct iovec {
        void *iov_base;
        size_t  iov_len;
} iovec_t;
#endif /* _IOVEC_T */

#if !defined(_KERNEL)
extern ssize_t readv(int, const struct iovec *, int);
extern ssize_t writev(int, const struct iovec *, int);
#endif

#if defined(_KERNEL) || defined(_KMEMUSER)

#define	_MAX_IOVEC	(NBPP/sizeof(iovec_t))

struct pm;
struct vfile;

typedef struct uio {
	iovec_t		*uio_iov;	/* pointer to array of iovecs */
	int		uio_iovcnt;	/* number of iovecs */
	int		uio_fmode;	/* file mode flags */
	off_t		uio_offset;	/* file offset */
	short		uio_segflg;	/* address space (kernel or user) */
	short		uio_pio;	/* parallel IO */
	short		uio_sigpipe;	/* send sigpipe before syscall retn */
	uchar_t		uio_readiolog;	/* preferred read size (log 2) */
	uchar_t		uio_writeiolog;	/* preferred write size (log 2) */
	ssize_t		uio_resid;	/* residual count */
	off_t		uio_limit;	/* u-limit (maximum byte offset) */
	struct pm 	*uio_pmp;	/* Policy Module pointer of NUMA */
	struct buf	*uio_pbuf;	/* buffer allocated by physio */
	struct vfile	*uio_fp;	/* file pointer */
} uio_t;

/*
 * I/O direction.
 */
typedef enum uio_rw { UIO_READ, UIO_WRITE } uio_rw_t;

/*
 * Segment flag values.
 */
typedef enum uio_seg {
	UIO_NOSPACE = -1,	/* no data movement (used for pagein) */
	UIO_USERSPACE,		/* uio_iov describes user space */
	UIO_SYSSPACE,		/* uio_iov describes system space */
	UIO_USERISPACE		/* uio_iov describes instruction space */
} uio_seg_t;

int	uiomove(void *, size_t, uio_rw_t, uio_t *);
int	ureadc(int, uio_t *);
int	uwritec(uio_t *);
int	uiomvuio(uio_t *, uio_t *);
void	uioskip(uio_t *, size_t);
void	uioupdate(struct uio *, size_t);

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif
#endif	/* _IO_UIO_H */
