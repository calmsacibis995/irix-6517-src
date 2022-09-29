/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-3b2:io/ddi.c	1.13"*/
#ident	"$Revision: 1.59 $"

/*            UNIX Device Driver Interface functions          

 * This file contains functions that are to be added to the kernel 
 * to put the interface presented to drivers in conformance with
 * the DDI standard. Of the functions added to the kernel, 17 are
 * function equivalents of existing macros in sysmacros.h, map.h,
 * stream.h, and param.h
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/sysinfo.h>
#include <sys/map.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <ksys/vproc.h>
#include <ksys/vsession.h>
#include <sys/stream.h>
#include <sys/uio.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/poll.h>
#include <sys/ddi.h>
#include <sys/mkdev.h>
#include <sys/pfdat.h>
#include <sys/immu.h>
#include <sys/debug.h>

extern short MAJOR[NMAJORENTRY]; /* because we can't include sysmacros.h here */

int splbase(void);
int spltimeout(void);
int spldisk(void);
int splstr(void);

pl_t plbase	= splbase;
pl_t pltimeout	= spltimeout;
pl_t pldisk	= spldisk;
pl_t plstr	= splstr;
pl_t plhi	= splhi;

static __userabi_t userabi32 = {
	sizeof(app32_int_t),
	sizeof(app32_long_t),
	sizeof(app32_ptr_t),
	sizeof(long long),
};

#if _MIPS_SIM == _ABI64

static __userabi_t userabi64 = {
	sizeof(app64_int_t),
	sizeof(app64_long_t),
	sizeof(app64_ptr_t),
	sizeof(long long),
};

#endif /* _ABI64 */

/* function: getmajor()     
 * macro in: sysmacros.h 
 * purpose:  return internal major number corresponding to device
 *           number (new format) argument
 */

major_t
getmajor(dev_t dev)
{
	return (major_t) MAJOR[(dev>>NBITSMINOR) & MAXMAJ];
}




/* function: getemajor()     
 * macro in: sysmacros.h 
 * purpose:  return external major number corresponding to device
 *           number (new format) argument
 */

major_t
getemajor(dev_t dev)
{
	if ((dev >> NBITSMINOR) > MAXMAJ)
		return NODEV;
	return (major_t) ( (dev>>NBITSMINOR) & MAXMAJ );
}



/* function: getminor()     
 * macro in: sysmacros.h 
 * purpose:  return internal minor number corresponding to device
 *           number (new format) argument
 * NOTE (daveh): IRIX is not using an internal/external minor distinction.
 */

minor_t
getminor(dev_t dev)
{
	return (minor_t) (dev & MAXMIN);
}



/* function: geteminor()     
 * macro in: sysmacros.h 
 * purpose:  return external minor number corresponding to device
 *           number (new format) argument
 */

minor_t
geteminor(dev_t dev)
{
	return (minor_t) (dev & MAXMIN);
}


/* function: etoimajor()     
 * purpose:  return internal major number corresponding to external
 *           major number argument or -1 if the external major number
 *	     does not represent a valid loaded device
 */

major_t
etoimajor(register major_t emajnum)
{
	if (emajnum > MAXMAJ || (!bdvalid(&bdevsw[MAJOR[emajnum]]) && !cdvalid(&cdevsw[MAJOR[emajnum]])))
		return (-1); /* invalid external major */

	return ( (major_t) MAJOR[emajnum]);
}



/* function: itoemajor()     
 * purpose:  return external major number corresponding to internal
 *           major number argument or -1 if no external major number
 *	     can be found after lastemaj that maps to the internal
 *	     major number. Pass a lastemaj val of -1 to start
 *	     the search initially. (Typical use of this function is
 *	     of the form:
 *
 *	     lastemaj=-1;
 *	     while ( (lastemaj = itoemajor(imag,lastemaj)) != -1)
 *	        { process major number }
 *
 * The ddi defines this interface in an unfortunate way, since major_t
 * is currently an unsigned value.  Fortunately, it seems that no one
 * actually uses this function.
 */

int
itoemajor(register major_t imajnum, major_t lastemaj)
{
	if (!bdvalid(get_bdevsw(imajnum)) && !cdvalid(get_cdevsw(imajnum)))
		return (-1);

	/* if lastemaj == -1 then start from beginning of MAJOR table */
	if (lastemaj == (major_t)-1)
		lastemaj = 0;
	else
		lastemaj++;

	/* increment lastemaj and search for MAJOR table entry that
	 * equals imajnum. return -1 if none can be found.
	 */

	for (;lastemaj < NMAJORENTRY; lastemaj++)
	   if (imajnum == MAJOR[lastemaj]) 
		return ((int)lastemaj);

	return (-1);
}



/* function: makedevice()
 * macro in: sysmacros.h 
 * purpose:  encode external major and minor number arguments into a
 *           new format device number
 */

dev_t
makedevice(register major_t maj, register minor_t minor)
{
	return (dev_t) ( (maj<<NBITSMINOR) | (minor&MAXMIN) ); 
}

/* function: datamsg()
 * macro in: stream.h 
 * purpose:  return true (1) if the message type input is a data
 *           message type, 0 otherwise
 */

int
datamsg(unsigned char db_type)
{
	return(db_type == M_DATA || db_type == M_PROTO || db_type == M_PCPROTO || db_type == M_DELAY);
}


/* function: OTHERQ()
 * macro in: stream.h 
 * purpose:  return a pointer to the other queue in the queue pair
 *           of qp
 */

queue_t *
OTHERQ(register queue_t *q)
{
	return((q)->q_flag&QREADR? (q)+1: (q)-1);
}




/* function: putnxt()
 * macro in: stream.h 
 * purpose:  call the put routine of the queue linked to qp
 */

int
putnext(register queue_t *q, register mblk_t *mp)
{
	return ((*q->q_next->q_qinfo->qi_putp)(q->q_next, mp));
}




/* function: RD()
 * macro in: stream.h 
 * purpose:  return a pointer to the read queue in the queue pair
 *           of qp; assumed that qp points to a write queue
 */

queue_t *
RD(register queue_t *q)
{
	return(q-1);
}





/* function: splstr()
 * macro in: stream.h 
 * purpose:  set spl to protect critical regions of streams code
 *           spltty() for 3b2, spl5 for others
 */

int
splstr(void)
{
#if defined(u3b2) || defined(sgi)
	return(spltty());
#else
	return(spl5());
#endif
}

int
splbase(void)
{
	return(spl0());
}

int
spltimeout(void)
{
	return(spl1());
}

int
spldisk(void)
{
	return(splvme());
}

/* function: WR()
 * macro in: stream.h 
 * purpose:  return a pointer to the write queue in the queue pair
 *           of qp; assumed that qp points to a read queue
 */

queue_t *
WR(register queue_t *q)
{
	return(q+1);
}


/* function: drv_getparm()
 * purpose:  store value of kernel parameter associated with parm in
 *           valuep and return 0 if parm is PPGRP,
 *           LBOLT, PPID, PSID, TIME; return -1 otherwise
 */

int
drv_getparm(unsigned long parm, unsigned long *valuep)
{
	if (parm == PPGRP || parm == PSID) {
		vproc_t		*vp;
		vp_get_attr_t	attr;

		if (current_pid() == 0)
			return -1;
		
		vp = VPROC_LOOKUP(current_pid());
		if (vp == VPROC_NULL)
			return -1;

		VPROC_GET_ATTR(vp, VGATTR_PGID | VGATTR_SID, &attr);
		VPROC_RELE(vp);
		*valuep = parm == PPGRP ? (ulong_t)attr.va_pgid :
					  (ulong_t)attr.va_sid;

		return 0;
	}

	switch (parm) {
	case UCRED:
		*valuep = (ulong_t) get_current_cred();
		break;
	case LBOLT:
		*valuep= (unsigned long) lbolt;
		break;
	case TIME:
		{
		timespec_t tv;
		nanotime(&tv);
		*valuep= (unsigned long) tv.tv_sec;
		break;
		}
	case PPID:
		*valuep= (unsigned long) current_pid();
		break;
	default:
		return(-1);
	}

	return (0);
}



/* function: drv_setparm()
 * purpose:  set value of kernel parameter associated with parm to
 *           value and return 0 if parm is SYSRINT, SYSXINT,
 *           SYSMINT, SYSRAWC, SYSCANC, SYSOUTC; return -1 otherwise;
 *	     we splhi()/splx() to make this operation atomic
 */


int 
drv_setparm(register unsigned long parm, register unsigned long value)
{
	int oldpri;

	oldpri = splhi();

	switch (parm) {
	case SYSRINT:
		SYSINFO.rcvint+=value;
		break;
	case SYSXINT:
		SYSINFO.xmtint+=value;
		break;
	case SYSMINT:
		SYSINFO.mdmint+=value;
		break;
	case SYSRAWC:
		SYSINFO.rawch+=value;
		break;
	case SYSCANC:
		SYSINFO.canch+=value;
		break;
	case SYSOUTC:
		SYSINFO.outch+=value;
		break;
	default:
		splx(oldpri);
		return(-1);
	}

	splx(oldpri);
	return(0);
}


/* function: physiock()
 * purpose:  perform raw device I/O on block devices
 *
 * The arguments are
 *	- the strategy routine for the device
 *	- a buffer, which is usually NULL, or else a special buffer
 *	  header owned exclusively by the device for this purpose
 *	- the device number
 *	- read/write flag
 *	- size of the device (in blocks)
 *	- uio structure containing the I/O parameters
 *
 * Returns 0 on success, or a non-zero errno on failure.
 */
int
physiock(int (*strat)(struct buf *),
	struct buf *bp,
	dev_t dev,
	uint64_t flags,
	daddr_t limit,
	struct uio *uiop)
{
	register struct iovec *iov;
	size_t iovlen;
	int cnt;
	register daddr_t blkno, upper;

	if (uiop->uio_offset % NBPSCTR)
		return(EINVAL);

	/* determine if offset is at or past end of device 
	 * if past end of device return ENXIO. Also, if at end 
	 * of device and writing, return ENXIO. If at end of device
	 * and reading, nothing more to read -- return 0
	 * XXX sgi returne ENOSPC instead - contrary to the DDI..
	 */
	blkno = uiop->uio_offset >> SCTRSHFT;
	if (blkno >= limit) {
		if (blkno > limit || !(flags & B_READ))
			return ENOSPC;
		return(0);
	}

	/* Adjust count of request so that it does not take I/O past
	 * end of device.
	 */
	iovlen = 0;
	for (cnt = 0,iov = uiop->uio_iov; cnt < uiop->uio_iovcnt; cnt++,iov++)
		iovlen += iov->iov_len;

	upper = blkno + (iovlen >> SCTRSHFT);

	if (upper > limit) {
		limit -= blkno;
		limit <<= SCTRSHFT;
		for (cnt = 0, iov = uiop->uio_iov; cnt < uiop->uio_iovcnt;
		     cnt++, iov++) {
			if (iov->iov_len <= limit)
				limit -= iov->iov_len;
			else {
				iov->iov_len = limit;
				limit = 0;
			}
		}
	}

	/* request in uio has been adjusted to fit device size. Now
	 * perform a biophysio() and return the error number it returns.
	 */
	return(biophysio(strat, bp, dev, flags, blkno, uiop));
}


/* function: btop
 * purpose:  convert byte count input to logical page units
 *           (byte counts that are not a page-size multiple
 *           are rounded down)
 */

unsigned long
btop (register unsigned long numbytes)
{
	return ( numbytes >> BPCSHIFT );
}



/* function: btopr
 * purpose:  convert byte count input to logical page units
 *           (byte counts that are not a page-size multiple
 *           are rounded up)
 */

unsigned long
btopr (register unsigned long numbytes)
{
	return ( (numbytes+(NBPC-1)) >> BPCSHIFT );
}



/* function: ptob
 * purpose:  convert size in pages to to bytes.
 */

unsigned long
ptob (register unsigned long numpages)
{
	return ( numpages << BPCSHIFT );
}


#define MAXCLOCK_T 0x7FFFFFFF

/* function: drv_hztousec
 * purpose:  convert from system time units (given by parameter HZ)
 *           to microseconds. This code makes no assumptions about the
 *           relative values of HZ and ticks and is intended to be
 *           portable. 
 *
 *           A zero or lower input returns 0, otherwise we use the formula
 *           microseconds = (hz/HZ) * 1,000,000. To minimize overflow
 *           we divide first and then multiply. Note that we want
 *           upward rounding, so if there is any fractional part,
 *           we increment the return value by one. If an overflow is
 *           detected (i.e.  resulting value exceeds the
 *           maximum possible clock_t, then truncate
 *           the return value to MAXCLOCK_T.
 *
 *           No error value is returned.
 *
 *           This function's intended use is to remove driver object
 *           file dependencies on the kernel parameter HZ.
 *           many drivers may include special diagnostics for
 *           measuring device performance, etc., in their ioctl()
 *           interface or in real-time operation. This function
 *           can express time deltas (i.e. lbolt - savelbolt)
 *           in microsecond units.
 *
 */

clock_t
drv_hztousec(register clock_t ticks)
{
	clock_t quo, rem;
	register clock_t remusec, quousec;

	if (ticks <= 0) return (0);

	quo = ticks / HZ; /* number of seconds */
	rem = ticks % HZ; /* fraction of a second */
	quousec = 1000000 * quo; /* quo in microseconds */
	remusec = 1000000 * rem; /* remainder in millionths of HZ units */

	/* check for overflow */
	if (quo != quousec / 1000000) return (MAXCLOCK_T);
	if (rem != remusec / 1000000) remusec = MAXCLOCK_T;
	
	/* adjust remusec since it was in millionths of HZ units */
	remusec = (remusec % HZ) ? remusec/HZ + 1 : remusec/HZ;

	/* check for overflow again. If sum of quousec and remusec
	 * would exceed MAXCLOCK_T then return MAXCLOCK_T 
	 */

	if ( (MAXCLOCK_T - quousec) < remusec) return (MAXCLOCK_T);

	return (quousec + remusec);
}

/* function: drv_usectohz
 * purpose:  convert from microsecond time units to system time units
 *           (given by parameter HZ) This code also makes no assumptions
 *           about the relative values of HZ and ticks and is intended to
 *           be portable. 
 *
 *           A zero or lower input returns 0, otherwise we use the formula
 *           hz = (microsec/1,000,000) * HZ. Note that we want
 *           upward rounding, so if there is any fractional part,
 *           we increment by one. If an overflow is detected, then
 *           the maximum clock_t value is returned. No error value
 *           is returned.
 *
 *           The purpose of this function is to allow driver objects to
 *           become independent of system parameters such as HZ, which
 *           may change in a future release or vary from one machine
 *           family member to another.
 */

clock_t
drv_usectohz(register clock_t microsecs)
{
	struct timeval tv;
	long usec;

	tv.tv_sec  = microsecs / USEC_PER_SEC;
	usec = microsecs % USEC_PER_SEC;
	/* upward rounding */
	tv.tv_usec = (usec % USEC_PER_TICK) ? usec + USEC_PER_TICK: usec;
	return(hzto(&tv));
}

/* 
 * function: drv_usecwait
 */

void
drv_usecwait(clock_t count)
{
	us_delay(count);
}

/* function: drv_priv()
 * purpose:  determine if the supplied credentials identify a privileged
 *           process.  To be used only when file access modes and
 *           special minor device numbers are insufficient to provide
 *           protection for the requested driver function.  Returns 0
 *           if the privilege is granted, otherwise EPERM.
 */ 

int
drv_priv(cred_t *cr)
{
	return (cr->cr_uid ? EPERM : 0);
}

/*
 * Allocate a pollhead structure.
 */
struct pollhead *
phalloc(int flag)
{
	struct pollhead *ph;

	ph = (struct pollhead *) kmem_zalloc(sizeof(struct pollhead), flag);
	if (!ph)
		return(NULL);
	initpollhead(ph);
	return(ph);
}

/*
 * Free a previously allocated pollhead structure.
 */
void
phfree(struct pollhead *ph)
{
	destroypollhead(ph);
	kmem_free(ph, sizeof(struct pollhead));
}


/* function: userabi(__userabi_t *)
 * purpose:  determine the size int bytes of various C types in the ABI under
 *	     which the current user process is running.  0 indicates success,
 *	     nonzero indicates failure.  This function must only be called
 *	     from a user's context, and the values copied into __userabi_t
 *	     are only valid for the process executing when userabi is called.
 */
int
userabi(__userabi_t *currentabi)
{
	int abi;

	if (curvprocp == NULL)
		return ESRCH;

	abi = get_current_abi();

#if _MIPS_SIM == _ABI64
	if (ABI_IS_IRIX5_64(abi)) {
		*currentabi = userabi64;
		return 0;
	}

#endif /* _ABI64 */
	
	ASSERT(ABI_IS_IRIX5(abi) || ABI_IS_IRIX5_N32(abi));
	*currentabi = userabi32;
	return 0;
}
