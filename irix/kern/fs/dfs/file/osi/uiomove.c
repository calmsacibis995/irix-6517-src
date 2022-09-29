/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: uiomove.c,v 65.5 1998/05/29 20:11:41 bdr Exp $";
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
 * $Log: uiomove.c,v $
 * Revision 65.5  1998/05/29 20:11:41  bdr
 * changed osi_uio_skip length arg to be an afs_hyper_t to support
 * 64 bit files.
 *
 * Revision 65.4  1998/03/23  16:26:26  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1998/01/07 15:27:05  lmc
 * Added casting for the Update macro.  Also added casting where iov_base
 * is updated and changed the type of one parameter from int to size_t.
 *
 * Revision 65.2  1997/11/06  19:58:19  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:46  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.628.1  1996/10/02  18:12:37  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:23  damon]
 *
 * Revision 1.1.622.2  1994/06/09  14:17:30  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:39  annie]
 * 
 * Revision 1.1.622.1  1994/02/04  20:27:05  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:39  devsrc]
 * 
 * Revision 1.1.620.1  1993/12/07  17:31:39  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:15:44  jaffe]
 * 
 * Revision 1.1.5.4  1993/01/21  14:53:41  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:57:37  cjd]
 * 
 * Revision 1.1.5.3  1992/11/24  18:24:27  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:16:36  bolinger]
 * 
 * Revision 1.1.5.2  1992/08/31  20:50:01  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    Declare osi_uio_copy, osi_uio_trim, and osi_uio_skip to return an int.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:43:58  jaffe]
 * 
 * Revision 1.1.3.6  1992/05/22  20:39:56  garyf
 * 	cleanup OSF/1 conditionals &
 * 	make OSF/1 version same as other platforms
 * 	[1992/05/22  03:13:18  garyf]
 * 
 * Revision 1.1.3.5  1992/01/26  01:01:14  delgado
 * 	Merge in OSF changes to dce1.0.1
 * 		- add OSF ifdefs to handle different uio interface for OSF
 * 	[1992/01/26  01:00:51  delgado]
 * 
 * Revision 1.1.3.4  1992/01/25  20:48:47  zeliff
 * 	dfs6.3 merge, part2
 * 	[1992/01/25  20:07:22  zeliff]
 * 
 * $EndLog$
 */
/* Copyright (C) 1990 Transarc Corporation - All rights reserved */
/* uiomove.c
 *
 * This is an implementation of the uiomove subroutine used throughout the
 * kernel.  Originally, at least, this routine will only work in the user ring,
 * but the changes needed to conditionally call copy{in/out} when in kernel
 * mode should be fairly easy.
 *
 * This description is based on the specification written by Shu-Tsui Tu.
 *  -ota 900727. */

/* uiomove - transfers data between a buffer and collection of I/O vectors.
 * This behavior is highly dependent on typical and historical usage patterns,
 * and as such it is more tightly intertwined with the semantics of the read,
 * write, readv and writev system calls.
 *
 * The buffer described by a pointer and length.  The I/O vectors are described
 * by a (struct uio).  This structure is designed to preserve the state of a
 * single logical transfer actually made up of several calls to uiomove.  In
 * part because of this statefullness, other fields helpful to the above
 * mentioned system calls are also included in the uio structure even though
 * thay are mostly unrelated to the operation of uiomove itself.  These extra
 * fields include:
 *   osi_uio_offset: this is the offset in the file at which the I/O starts,
 *     this typically comes from the file descriptor.  The uiomove subroutine
 *     just increments this by the number of bytes moved.
 *   osi_uio_resid: this is initialized to the number of bytes described by all
 *     the I/O vectors.  The field is decremented by uiomove.  This field also
 *     provides one of the termination conditions for uiomove; it will never be
 *     decremented beyond zero.
 *
 * Also, as part of the uiomove's participation in the multi-part data
 * transfer, its specification requires it to update the fields describing the
 * I/O vectors.  These are described below.
 *
 * There are both three and four argument versions of uiomove.  BSD 4.4 uses a
 * three argument version that encodes the transfer directection in a uio
 * structure field (uio_flag).  In other systems this field is not present and
 * the third (of four) argument provides this information.  We will use the
 * four argument form here.
 *
 * #include <sys/types.h>
 * #include <sys/errno.h>
 * #include <sys/uio.h>
 *
 * EXPORT int osi_user_uiomove _TAKES((
 *   IN    caddr_t cp,
 *   IN    int n,
 *   IN    enum uio_rw rw,
 *   INOUT struct uio *uio));
 *
 *
 * cp:  Pointer to the buffer containing data to be copied from/to.
 *
 * n:   Specifies the number of bytes of data to copy.
 *
 * rw:  Mode indicating the direction of copy;
 *      if UIO_READ, uiomove() will copy data from cp to area specified by uio,
 *      if UIO_WRITE, uiomove() will copy data from the area specified by uio
 *      to the area pointed by cp.
 *
 * uio: A pointer to the data structure that maintains the "state" of transfer
 *      operation. It is updated by every call to uiomove.
 *
 * The uio data structure, defined in <sys/uio.h> consists of the following
 * attributes;
 *
 * struct uio {
 *         struct  iovec *osi_uio_iov;
 *         int     osi_uio_iovcnt;
 *         int     osi_uio_offset;
 *         int     osi_uio_seg;
 *         int     osi_uio_resid;
 * };
 *
 * struct iovec {
 *         caddr_t iov_base;
 *         int     iov_len;
 * };
 *
 * osi_uio_iov: points to an array of I/O vector elements. It supports the
 *              notion of scatter/gather I/Os.  Each element has a base pointer
 *              (iov_base) and a length (iov_len).  The uiomove routine will
 *              update both iov_base and iov_len by the amount of data copied.
 *
 * osi_uio_iovcnt: size of the iovec array. Both uio_iovcnt and uio_iov are
 *                 initialized by the caller.  As the transfer of each I/O
 *                 vector element is completed it must be "discarded" by
 *                 uiomove by advancing uio_iov and decrementing uio_iovcnt.
 *                 However, the last element must not be discarded, so that on
 *                 the return from the last call the iov pointer points to the
 *                 last element whose length will have been decremented to
 *                 zero.  Thus, another termination condition for uiomove is
 *                 when the uio_iovcnt field reaches one and the last element's
 *                 length is zero.
 *
 * osi_uio_offset: As described above this is incremented on every call to
 *                 uiomove by the number of bytes copied.
 *
 * osi_uio_segflg: Segment flag indicates whether copy the data described by
 *                 the uio structure is in the kernel space the user space.
 *                 Every element of the I/O vector must be in the same space.
 *
 * osi_uio_resid: indicates the data remaining to be copied. Initially,
 *                uio_resid is set to be the total number of bytes described by
 *                the entire I/O vector.  Each call to uiomove routine will
 *                decrement uio_resid by the amount of data moved.  The
 *                transfer stops when this field reaches zero.
 *
 * Ideally, the osi_uio_resid field, the iov_len field of the last I/O vector
 * element and the length parameter ("n") of last call to uiomove should reach
 * zero simultaneously.  If this does not happen it is not reported as an error
 * but further calls to uiomove will have no effect.  The only errors returned
 * by uiomove are those returned by the actual copy operation.
 *
 */

#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_uio.h>
#include <sys/types.h>
#include <dcedfs/stds.h>


/* Define two macros to do the random uiomove book keeping. */
/* The first, FIND_NEXT_CHUNK is a macro that must be used in a block as it
 * expands into a sequence of statements.  The n and uio parameters are
 * expressions giving the total requested transfer size and the address of the
 * uio structure, repectively.  The last two parameters must be LValue
 * expressions which are set to the base and length, repectively, of the next
 * largest contiguous area of memory to be used for a transfer. */

#define FIND_NEXT_CHUNK(n,uio,base,len) \
    while (base = (uio)->osi_uio_iov->iov_base, \
	   len = (uio)->osi_uio_iov->iov_len, \
	   ((len <= 0) && ((uio)->osi_uio_iovcnt > 1))) \
        (uio)->osi_uio_iov++, (uio)->osi_uio_iovcnt--; \
    if (len > (n)) len = (n); \
    if (len > (uio)->osi_uio_resid) len = (uio)->osi_uio_resid;

#ifdef SGIMIPS
#define Update(uio,delta) \
    ((uio)->osi_uio_iov->iov_base = ((char *)(uio)->osi_uio_iov->iov_base) + (delta), \
     (uio)->osi_uio_iov->iov_len -= (delta), \
     (uio)->osi_uio_offset += (delta), \
     (uio)->osi_uio_resid -= (delta))
#else
#define Update(uio,delta) \
    ((uio)->osi_uio_iov->iov_base += (delta), \
     (uio)->osi_uio_iov->iov_len -= (delta), \
     (uio)->osi_uio_offset += (delta), \
     (uio)->osi_uio_resid -= (delta))
#endif

#if !defined(KERNEL)

int
osi_user_uiomove(
  IN    caddr_t cp,
  IN    u_long n,
  IN	enum uio_rw rw,
  INOUT struct uio *uio)
{
    if ((uio->osi_uio_iovcnt <= 0) || (uio->osi_uio_resid <= 0)) return 0;
    while ((n > 0) && (uio->osi_uio_resid > 0)) {
	int code;
	caddr_t base;
	u_long num;

	FIND_NEXT_CHUNK (n, uio, base, num);
	if (num <= 0) return 0;

	/* do the transfer */
	code = 0;
	if (rw == UIO_READ) {
	    if (uio->osi_uio_seg == OSI_UIOSYS) {
		bcopy (cp, base, num);
	    } else if (uio->osi_uio_seg == OSI_UIOUSER) {
		code = osi_copyout (cp, base, num);
	    } else panic ("uiomove: bad osi_uio_seg");
	} else if (rw == UIO_WRITE) {
	    if (uio->osi_uio_seg == OSI_UIOSYS) {
		bcopy (base, cp, num);
	    } else if (uio->osi_uio_seg == OSI_UIOUSER) {
		code = osi_copyin (base, cp, num);
	    } else panic ("uiomove: bad osi_uio_seg");
	} else panic ("uiomove: bad rw");
	if (code) return code;

	/* update data uio state */
	Update (uio, num);
	n -= num;
    }
    return 0;
}
#endif /* !defined(KERNEL) */

void
osi_uiomove_unit (
  IN    u_long prevNum,
  IN    u_long n,
  INOUT struct uio *uio,
  OUT   caddr_t *baseP,
  OUT   u_long *lenP)
{
    if (prevNum > 0) Update (uio, prevNum);

    FIND_NEXT_CHUNK (n, uio, *baseP, *lenP);
}
/*
 *
 * Routines that deal with copying/trimming multi vector uio structs.
 *
 */
#define	AFS_MAXIOVCNT	    16

/*
 * routine to make copy of uio structure in ainuio, using aoutvec for space
 */
int
osi_uio_copy(
    struct uio *inuiop,
    struct uio *outuiop,
    struct iovec *outvecp)
{
    register int i;
    register struct iovec *iovecp;

    if (inuiop->osi_uio_iovcnt > AFS_MAXIOVCNT)
	return EINVAL;
    *outuiop = *inuiop;
    iovecp = inuiop->osi_uio_iov;
    outuiop->osi_uio_iov = outvecp;
    for (i = 0; i < inuiop->osi_uio_iovcnt; i++){
	*outvecp = *iovecp;
	iovecp++;
	outvecp++;
    }
    return 0;
}

/*
 * trim the uio structure to the specified size
 */
int
osi_uio_trim(struct uio *uiop, long size)
{
    register int i;
    register struct iovec *iovecp;

    uiop->osi_uio_resid = size;
    iovecp = uiop->osi_uio_iov;
    /*
     * It isn't clear that multiple iovecs work ok (hasn't been tested!)
     */
    for (i = 0;; i++, iovecp++) {
	if (i >= uiop->osi_uio_iovcnt || size <= 0) {
	    uiop->osi_uio_iovcnt = i;			/* we're done */
	    break;
	}
	if (iovecp->iov_len <= size)
	    /*
	     * entire iovec is included
	     */
	    size -= iovecp->iov_len;		/* this many fewer bytes */
	else {
	    /*
	     * this is the last one
	     */
	    iovecp->iov_len = size;
	    uiop->osi_uio_iovcnt = i+1;
	    break;
	}
    }
    return 0;
}


/*
 * skip size bytes in the current uio structure
 */
#ifdef SGIMIPS
int
osi_uio_skip(struct uio *uiop, afs_hyper_t size)
#else  /* SGIMIPS */
int
osi_uio_skip(struct uio *uiop, long size)
#endif /* SGIMIPS */
{
    register struct iovec *iovecp;	/* pointer to current iovec */
#ifdef SGIMIPS
    register size_t cnt;
#else
    register int cnt;
#endif

    /*
     * It isn't guaranteed that multiple iovecs work ok (hasn't been tested!)
     */
    while (size > 0 && uiop->osi_uio_resid) {
	iovecp = uiop->osi_uio_iov;
	cnt = iovecp->iov_len;
	if (cnt == 0) {
	    uiop->osi_uio_iov++;
	    uiop->osi_uio_iovcnt--;
	    continue;
	}
	if (cnt > size)
	    cnt = size;
#ifdef SGIMIPS
	iovecp->iov_base = (char *)iovecp->iov_base + cnt;
#else
	iovecp->iov_base += cnt;
#endif
	iovecp->iov_len -= cnt;
	uiop->uio_resid -= cnt;
	uiop->uio_offset += cnt;
	size -= cnt;
    }
    return 0;
}
