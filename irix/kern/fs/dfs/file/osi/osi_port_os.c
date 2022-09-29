/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_port_os.c,v 65.6 1998/06/22 20:00:11 lmc Exp $";
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
 * $Log: osi_port_os.c,v $
 * Revision 65.6  1998/06/22 20:00:11  lmc
 * Remove include file for systm.h if building in user space.
 *
 * Revision 65.5  1998/06/18  19:06:58  lmc
 * This adds kernel and user space code for osi_afsGetTime.  The afsTimeval
 * structure is now set by fields in the structure instead of assuming
 * that the timeval and afsTimeval structures are identical.
 *
 * Revision 65.4  1998/04/01  14:16:04  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added a type cast to a char pointer for the first argument to
 * 	printf() in vprintf() to avoid conflicts caused by the const
 * 	char pointer type.
 *
 * Revision 65.3  1998/03/23 16:26:28  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:20  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:51  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:40  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:46:51  dce
 * *** empty log message ***
 *
 * Revision 1.6  1996/09/10  17:52:27  vrk
 * Replace osi_copyout with bcopy in osi_GetMachineName().
 *
 * Revision 1.5  1996/04/10  18:09:41  bhim
 * Fixed vprintf definition to match prototype.
 *
 * Revision 1.4  1996/04/10  17:12:56  bhim
 * Added vprintf define.
 *
 * Revision 1.3  1996/04/09  16:59:39  bhim
 * Fixed osi_GetMachineName() code.
 *
 * Revision 1.2  1996/04/06  00:17:50  bhim
 * No Message Supplied
 *
 * Revision 1.1.820.3  1994/07/13  22:05:50  devsrc
 * 	merged with bl-10 
 * 	[1994/06/28  17:34:31  devsrc]
 *
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  16:00:22  mbs]
 *
 * Revision 1.1.820.2  1994/06/09  14:15:16  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:46  annie]
 * 
 * Revision 1.1.820.1  1994/02/04  20:24:19  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:32  devsrc]
 * 
 * Revision 1.1.818.1  1993/12/07  17:29:39  jaffe
 * 	1.0.3a update from Transarc
 * 
 * 	$TALog: osi_port_os.c,v $
 * 	Revision 1.4  1993/09/08  15:35:41  berman
 * 	Changes corresponding to HP's second drop, with Episode.
 * 	Milestone:  Episode builds cleanly, test_vnodeops works on all_tests.
 * 
 * 	Need osi_GetNGroups outside the kernel as well as inside.
 * 	[from r1.3 by delta bwl-o-db3961-port-103-to-HP, r1.16]
 * 
 * Revision 1.3  1993/08/13  20:19:47  bwl
 * 	More HP port changes.  Milestone: we can cd to /: and do ls.
 * 
 * 	Must include osi_intr.h to get osi_splclock;
 * 	Define vprintf since HP kernel doesn't have one.
 * 	[from r1.2 by delta bwl-o-db3961-port-103-to-HP, r1.9]
 * 
 * Revision 1.2  1993/08/09  14:26:13  berman
 * 	Once more we close to reopen an orthogonal
 * 	delta for export.
 * 
 * 	Include utime.h
 * 	Create local utimes() function to fix HP
 * 	port changs which call utime() with wrong
 * 	arguments.
 * 	[from r1.1 by delta bwl-o-db3961-port-103-to-HP, r1.8]
 * 
 * Revision 1.1  1993/07/30  18:17:26  bwl
 * 	Port DFS 1.0.3 to HP/UX, adapting HP's changes (which were made to the
 * 	1.0.2a code base) to our own code base.
 * 
 * 	Moved to HPUX from HP800 subdirectory, while adapting changes from HP
 * 	(and renaming, hpux => os).
 * 	[added by delta bwl-o-db3961-port-103-to-HP, r1.1]
 * 
 * Revision 1.4  1993/01/28  00:24:57  blake
 * 	This is the first part of the port of the OSI layer to SunOS 5.x,
 * 	consisting of everything that except locking and synchronization
 * 	primitives.  Aside from adding various SunOS-specific definitions,
 * 	I have reorganized the OSI code somewhat.  I have removed many of
 * 	the allegedly (but not really) generic definitions from the top-level
 * 	directory and pruned back the thicket of #ifdefs in the "generic" code.
 * 
 * 	A few external interface changes were necessary.  These result in
 * 	small changes to a large number of files.
 * 
 * 	1) "struct ucred" has been replaced by "osi_cred_t" since the SVR4
 * 	credential structure has a different name.
 * 
 * 	2) To do a setattr in most systems, one fills in as much of the
 * 	vattr structure as appropriate and leaves the remaining members
 * 	set to -1; in SVR4, you indicate which members you want to set
 * 	by means of a bit mask contained in the structure.  "osi_vattr_null(vap)"
 * 	has therefore been replaced by "osi_vattr_init(vap, mask)"; there
 * 	are also osi_vattr_add and osi_vattr_sub functions for manipulating
 * 	the mask.
 * 
 * 	3) The SVR4 timeout/untimeout mechanism is also quite different,
 * 	but I was able to confine the interface changes to osi_net.c.
 * 
 * 	This is just a checkpoint before I begin major surgery on the
 * 	various OSI sleep/wakeup, locking, preemption, and spl routines.
 * 	I have linted the OSI code on a Sun and built it on a RIOS, but
 * 	that is all.  No warranties express or implied.
 * 
 * 	Reorganization of OSI code for SunOS 5.x port.
 * 	[from r1.3 by delta blake-db3108-port-osi-layer-to-SunOS5.x, r1.1]
 * 
 * Revision 1.3  1992/11/25  21:07:43  jaffe
 * 	Fix occasions of afs/ that were not yet at the OSF
 * 	[from revision 1.1 by delta jaffe-sync-with-dcedfs-changes, r1.1]
 * 
 * Revision 1.1  1992/06/06  23:38:45  mason
 * 	Really a bit misnamed - this delta adds code necessary for HP-UX; it isn't
 * 	sufficient for a complete port.
 * 
 * 	Porting routines for HP-UX
 * 	[added by delta mason-add-hp800-osi-routines, revision 1.1]
 * 
 * $EndLog$
 */
/* Copyright (C) 1992, 1993 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>
#include <dcedfs/common_data.h>
#include <utime.h>
#include <osi.h>
#include <osi_uio.h>
#include <osi_intr.h>

#ifdef	_KERNEL

#include <sys/utsname.h>
#include <sys/systm.h>

/*
 * Set the real time
 */
long osi_SetTime(atv)
    struct timeval *atv;
{
	settime(atv->tv_sec, atv->tv_usec);
	return 0;
}

int
osi_GetMachineName(buf, len)
	char *buf;
	int len;
{
	size_t llen;

	if ((llen = len) > hostnamelen)
		llen = hostnamelen+1;

	bcopy(hostname, buf, llen);

	return 0;
}

/*
 * For kernels without their own vprintf
 */
int vprintf(const char *fmt, va_list vargs)
{
    long *arg = (long *)vargs;
    printf((char *)fmt, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]);
    return 0;
}

void osi_SetCMASK(int v)
{
	struct umaska {
		usysarg_t mask;
	} umaska;
	rval_t	rval;

	extern int umask(struct umaska *, rval_t *);

	umaska.mask = v;
	umask(&umaska, &rval);
}

void
osi_GetTime(struct timeval *tvp)
{
   timespec_t   ts;

   nanotime(&ts);
   TIMESPEC_TO_TIMEVAL(&ts, tvp);
}


/*  The afsTimeval structure is always 2 32 bit quantities
    whereas the timeval structure is padded on a 64 bit
    machine for the seconds.  That means there should be
    no casting from an afsTimeval to a timeval and vice
    versa.  This routine allows us to change places that call
    osi_GetTime() with an afsTimeval structure cast to a timeval
    structure to be changed to use osi_afsGetTime() instead
    of changing data types in the calling routine.  */
void
osi_afsGetTime(struct afsTimeval *tvp)
{
   timespec_t   ts;

   nanotime(&ts);
   tvp->sec = ts.tv_sec;
   tvp->usec = ts.tv_nsec/1000;
}
#else	/* _KERNEL */

void osi_afsGetTime(struct afsTimeval *tvp)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	tvp->sec = tv.tv_sec;
	tvp->usec = tv.tv_usec;
}
#endif /* !_KERNEL */
