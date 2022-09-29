/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.33 $"


#include <sys/types.h>

#include <sys/arsess.h>
#include <sys/atomic_ops.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/extacct.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/prctl.h>
#include <sys/uthread.h>

#include <ksys/as.h>
#include <ksys/vpag.h>
#include <ksys/vproc.h>

#include "os/proc/pproc_private.h"	/* distasteful */


/* procscan parameters for arsess_chooseuthread_scan */
struct CUTSargs {
	vpagg_t	  *vpag;
	uthread_t *ut;
};


/* Global variables */
int  eff_spilen;		/* Length of Service Provider Information */
int  eff_sessaf;		/* Session accounting record format */
char dfltspi[MAX_SPI_LEN];	/* Default Service Provider Information */


/* Internal functions */
static uthread_t *arsess_chooseuthread(vpagg_t *);
static int arsess_chooseuthread_scan(proc_t *, void *, int);
static int arsess_findvpag(ash_t *, vpagg_t **);


/*
 * arsess_chooseuthread
 *	select a uthread that is a member of the specified array session
 *	and returns its pointer.
 */
uthread_t *
arsess_chooseuthread(vpagg_t *vpag)
{
	struct CUTSargs scanargs;

	/* Unfortunately, the only way to do this is with procscan */

	scanargs.vpag = vpag;
	procscan(arsess_chooseuthread_scan, &scanargs);

	return scanargs.ut;
}


/*
 * arsess_chooseuthread_scan
 *	selection function used with procscan by arsess_chooseuthread,
 *	used for finding a uthread that belongs to a particular array
 *	session.
 */
static int
arsess_chooseuthread_scan(proc_t *p, void *arg, int mode)
{
	struct CUTSargs *scanargs = (struct CUTSargs *) arg;

	switch (mode) {

	    case 0:
		scanargs->ut = NULL;
		break;

	    case 1:
		if (p->p_vpagg == scanargs->vpag) {
			uscan_hold(&p->p_proxy);
			scanargs->ut = prxy_to_thread(&p->p_proxy);
			return 1;	/* Terminate scan */
		}
		break;

	    case 2:
	    default:
		break;
	}

	return 0;
}


/*
 * arsess_ctl:
 *	perform some sort of global operation involving array sessions
 *	on behalf of the current process.
 */
int
arsess_ctl(int funccode, void *userbuf, int buflen)
{
	int result = 0;

	switch (funccode) {

	    case ARSCTL_NOP:
		/* Ping! */
		break;

	    case ARSCTL_GETSAF:
	    {
		    /* Get the session accounting record format */

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(eff_sessaf)) {
			    return EINVAL;
		    }

		    /* Simply copy the format value to the user's buffer */
		    if (copyout(&eff_sessaf, userbuf, buflen) != 0) {
			    return EFAULT;
		    }
		
		    break;
	    }

	    case ARSCTL_SETSAF:
	    {
		    /* Set the session accounting record format */
		    int newval;

		    /* Make sure the user is allowed to do this */
		    if (!cap_able(CAP_SYSINFO_MGT)) {
			    return EPERM;
		    }

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(newval)) {
			    return EINVAL;
		    }

		    /* Copy the new value from the user's buffer */
		    if (copyin(userbuf, &newval, sizeof(newval))) {
			    return EFAULT;
		    }

		    /* Make sure the new value is reasonable */
		    if (newval < 1  ||  newval > 2) {
			    return EINVAL;
		    }

		    /* Install the new value */
		    eff_sessaf = newval;

		    break;
	    }

	    case ARSCTL_GETMACHID:
	    {
		    /* Get the machine ID */
		    int machid;

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(machid)) {
			    return EINVAL;
		    }

		    /* Go obtain the machine ID */
		    machid = vpag_getmachid();

		    /* Copy the value to the user's buffer */
		    if (copyout(&machid, userbuf, buflen) != 0) {
			    return EFAULT;
		    }
		
		    break;
	    }

	    case ARSCTL_SETMACHID:
	    {
		    /* Set the machine ID */
		    int machid;

		    /* Make sure the user is allowed to do this */
		    if (!cap_able(CAP_SYSINFO_MGT)) {
			    return EPERM;
		    }

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(machid)) {
			    return EINVAL;
		    }

		    /* Copy the new value from the user's buffer */
		    if (copyin(userbuf, &machid, sizeof(machid))) {
			    return EFAULT;
		    }

		    /* Install the new value */
		    result = vpag_setmachid(machid);

		    break;
	    }

	    case ARSCTL_GETARRAYID:
	    {
		    /* Get the array ID */
		    int arrayid;

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(arrayid)) {
			    return EINVAL;
		    }

		    /* Go obtain the array ID */
		    arrayid = vpag_getarrayid();

		    /* Copy the value to the user's buffer */
		    if (copyout(&arrayid, userbuf, buflen) != 0) {
			    return EFAULT;
		    }
		
		    break;
	    }

	    case ARSCTL_SETARRAYID:
	    {
		    /* Set the array ID */
		    int arrayid;

		    /* Make sure the user is allowed to do this */
		    if (!cap_able(CAP_SYSINFO_MGT)) {
			    return EPERM;
		    }

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(arrayid)) {
			    return EINVAL;
		    }

		    /* Copy the new value from the user's buffer */
		    if (copyin(userbuf, &arrayid, sizeof(arrayid))) {
			    return EFAULT;
		    }

		    /* Install the new value */
		    result = vpag_setarrayid(arrayid);

		    break;
	    }

	    case ARSCTL_GETASHCTR:
	    {
		    /* Get the ASH/paggid local counter */
		    paggid_t idctr;

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(idctr)) {
			    return EINVAL;
		    }

		    /* Go obtain the ID counter */
		    idctr = vpag_getidctr();

		    /* Copy the value to the user's buffer */
		    if (copyout(&idctr, userbuf, buflen) != 0) {
			    return EFAULT;
		    }
		
		    break;
	    }

	    case ARSCTL_SETASHCTR:
	    {
		    /* Set the ASH/paggid local counter */
		    paggid_t idctr;

		    /* Make sure the user is allowed to do this */
		    if (!cap_able(CAP_SYSINFO_MGT)) {
			    return EPERM;
		    }

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(idctr)) {
			    return EINVAL;
		    }

		    /* Copy the new value from the user's buffer */
		    if (copyin(userbuf, &idctr, sizeof(idctr))) {
			    return EFAULT;
		    }

		    /* Install the new value */
		    result = vpag_setidctr(idctr);

		    break;
	    }

	    case ARSCTL_GETASHINCR:
	    {
		    /* Get the ASH/paggid local counter increment */
		    paggid_t idincr;

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(idincr)) {
			    return EINVAL;
		    }

		    /* Go obtain the ID counter increment */
		    idincr = vpag_getidincr();

		    /* Copy the value to the user's buffer */
		    if (copyout(&idincr, userbuf, buflen) != 0) {
			    return EFAULT;
		    }
		
		    break;
	    }

	    case ARSCTL_SETASHINCR:
	    {
		    /* Set the ASH/paggid local counter increment */
		    paggid_t idincr;

		    /* Make sure the user is allowed to do this */
		    if (!cap_able(CAP_SYSINFO_MGT)) {
			    return EPERM;
		    }

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(idincr)) {
			    return EINVAL;
		    }

		    /* Copy the new value from the user's buffer */
		    if (copyin(userbuf, &idincr, sizeof(idincr))) {
			    return EFAULT;
		    }

		    /* Install the new value */
		    result = vpag_setidincr(idincr);

		    break;
	    }

	    case ARSCTL_GETDFLTSPI:
	    {
		    /* Get the default Service Provider Info */
		    int copylen;

		    /* Make sure a reasonable buffer length was specified */
		    if (buflen < 1) {
			    return EINVAL;
		    }

		    /* Determine how much to copy */
		    copylen = eff_spilen;
		    if (buflen < copylen) {
			    copylen = buflen;
		    }

		    /* Copy it to the user's buffer */
		    if (copyout(dfltspi, userbuf, copylen) != 0) {
			    return EFAULT;
		    }

		    /* Pad the information if necessary */
		    if (copylen < buflen) {
			    char *ptr = ((char *) userbuf) + copylen;

			    if (uzero(ptr, buflen - copylen) != 0) {
				    return EFAULT;
			    }
		    }
		
		    break;
	    }

	    case ARSCTL_SETDFLTSPI:
	    {
		    /* Set the default Service Provider Information */
		    char *spibuf;

		    /* Make sure user is allowed to do this */
		    if (!cap_able(CAP_SYSINFO_MGT)) {
			    return EPERM;
		    }

		    /* Make sure the buffer length is reasonable */
		    if (buflen < 0  ||  buflen > eff_spilen) {
			    return EINVAL;
		    }

		    /* A buffer length of 0 is a special case */
		    if (buflen == 0) {
			    bzero(dfltspi, eff_spilen);
			    break;
		    }

		    /* Allocate storage for the user data (it's too big */
		    /* to live on the stack on low-end systems)		*/
		    spibuf = kern_malloc(buflen);
		    if (spibuf == NULL) {
			    return ENOMEM;
		    }

		    /* Obtain the new info from the user's buffer */
		    if (copyin(userbuf, spibuf, buflen) != 0) {
			    kern_free(spibuf);
			    return EFAULT;
		    }

		    /* Now that we know it's safe, store the new info, */
		    /* and pad it if necessary.			       */
		    bcopy(spibuf, dfltspi, buflen);
		    if (buflen < eff_spilen) {
			    bzero(dfltspi + buflen, eff_spilen - buflen);
		    }

		    /* Done with the temporary storage */
		    kern_free(spibuf);

		    break;
	    }

	    case ARSCTL_GETDFLTSPILEN:
	    {
		    /* Get the default length of the Service Provider Info */

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(int)) {
			    return EINVAL;
		    }

		    /* Simply copy the SPI length to the user's buffer */
		    if (copyout(&eff_spilen, userbuf, buflen) != 0) {
			    return EFAULT;
		    }
		
		    break;
	    }

	    case ARSCTL_SETDFLTSPILEN:
	    {
		    /* Set the default length of the Service Provider Info */
		    int newval;

		    /* Make sure the user is allowed to do this */
		    if (!cap_able(CAP_SYSINFO_MGT)) {
			    return EPERM;
		    }

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(newval)) {
			    return EINVAL;
		    }

		    /* Copy the new value from the user's buffer */
		    if (copyin(userbuf, &newval, sizeof(newval))) {
			    return EFAULT;
		    }

		    /* Make sure the size is reasonable (for some   */
		    /* fairly arbitrary definition of "reasonable") */
		    if (newval < 0  ||  newval > MAX_SPI_LEN) {
			    return EINVAL;
		    }

		    /* Install the new value */
		    eff_spilen = newval;

		    break;
	    }

	    case ARSCTL_ALLOCASH:
	    {
		    /* Allocate an unused ASH (aka paggid) */
		    paggid_t newid;

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(newid)) {
			    return EINVAL;
		    }

		    /* Go obtain a fresh paggid */
		    newid = vpag_allocid();

		    /* Copy the value to the user's buffer */
		    if (copyout(&newid, userbuf, buflen) != 0) {
			    return EFAULT;
		    }
		
		    break;
	    }

	    default:
	    {
		    /* Invalid function code */
		    return EINVAL;
	    }
	}

	return result;
}


/*
 * arsess_enumashs
 */
int
arsess_enumashs(ash_t *ashlist, int maxashs)
{
	return vpagg_enumpaggids((paggid_t *)ashlist, maxashs);
}


/*
 * arsess_findvpag:
 *	find the vpag associated with the ASH at the given address
 *	in user space, or for the current process if the ASH is < 0.
 *	Returns 0 if successful, or an errno if not.
 *
 *	Note that the vpag will be held if an ASH pointer was specified.
 */
int
arsess_findvpag(ash_t *ashptr, vpagg_t **vpagptr)
{
	ash_t ash;

	/* Get the ASH from the user's buffer */
	if (ashptr == NULL) {
		ash = -1LL;
	}
	else {
		/* Copy in the ASH from the user's buffer */
		if (copyin((caddr_t) ashptr, &ash, sizeof(ash_t))) {
			return EFAULT;
		}
	}

	/* Find the vpag for the specified ASH */
	if (ash < 0LL) {
		/* Use the array session of the current process */
		VPROC_GETVPAGG(curvprocp, vpagptr);
	}
	else {
		/* Use the array session with the specified ASH */
		*vpagptr = VPAG_LOOKUP(ash);
	}

	/* Bail out if the array session does not exist */
	if (*vpagptr == NULL) {
		return ESRCH;
	}

	return 0;
}


/*
 * arsess_getmachid:
 *	return the current array session machine ID, performing any
 *	necessary translations and massaging.
 */
int
arsess_getmachid(void)
{
	return vpag_getmachid();
}


/*
 * arsess_join:
 *	adds a specified process to the array session identified by the
 *	specified ASH if it exists, otherwise starts a new array session
 *	with that ASH. Returns a *negative* errno if unsuccessful, 0 if
 *	a new array session was created, or a positive number if an existing
 *	array sesssion was joined.
 */
int
arsess_join(pid_t pid, ash_t ash)
{
	vpagg_t *newvpag;
	vpagg_t *oldvpag;
	vproc_t *vpr;
	int rele = 0;
	int nice;
	int error;



	/* Make sure the ASH is reasonable */
	if (ash < 0LL) {
		return -EINVAL;
	}

	if (pid == current_pid())
		vpr = curvprocp;
	else {
		if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return -ESRCH;
		rele = 1;
	}

	VPROC_GETVPAGG(vpr, &oldvpag);

	/*
 	 * Make sure that the process did belong to an array session.
	 */
	if (oldvpag && (VPAG_GET_TYPE(oldvpag) != VPAG_ARRAY_SESSION)){
		if (rele)
			VPROC_RELE(vpr);
		return -EINVAL;
	}

	if (newvpag = VPAG_LOOKUP(ash)) {
		VPAG_JOIN(newvpag, pid);
		VPAG_RELE(newvpag); 	/* Release LOOKUP reference */
	} else {

		/* The ASH is not in use - start a new array session */
		VPROC_GET_NICE(curvprocp, &nice, get_current_cred(), error);
		error = vpag_create(ash, pid, nice, &newvpag, VPAG_ARRAY_SESSION);
		if (error) {
			if (rele)
				VPROC_RELE(vpr);
			return -error;
		}
		

		/* Initialize the rest of the new arsess */
		if (oldvpag) {
			prid_t	prid;

			prid = VPAG_GETPRID(oldvpag);
			VPAG_SETPRID(newvpag, prid);

			if (!VPAG_SPINFO_ISDFLT(oldvpag)) {
				char    *spibuf;
				int     oldspilen;

				oldspilen = VPAG_GETSPILEN(oldvpag);

				/* Must use malloc to avoid stack overflows */
				spibuf = kern_malloc(oldspilen);
				ASSERT(spibuf != NULL);

				VPAG_GETSPINFO(oldvpag, spibuf, oldspilen);
				VPAG_SETSPINFO(newvpag, spibuf, oldspilen);

				kern_free(spibuf);
			}
		}

	}
		
	/* Officially change to the new array session */
	if (oldvpag)
		VPAG_LEAVE(oldvpag, pid);
	VPROC_SETVPAGG(vpr, newvpag);


	if (rele)
		VPROC_RELE(vpr);

	return 0;
}


/*
 * arsess_leave:
 *	do processing related to a process leaving an array session
 */
void
arsess_leave(vpagg_t *vpag, pid_t pid)
{

	if (!vpag) return;

	VPAG_LEAVE(vpag, pid);
}


/*
 * arsess_new:
 *	starts a new array session and moves the specified process from
 *	its current array session to the new one as the official "initial
 *	process". Relatives of the process are *not* moved.  Returns the
 *	array session handle of the new array session, or a negative errno
 *	if unsuccessful.
 */
int
arsess_new(pid_t pid)
{
	vpagg_t *oldvpag;
	vpagg_t *newvpag;
	vproc_t *vpr;
	int rele = 0;
	int	error;
	int	nice;

	/* Get the appropriate vproc and vpagg */
	if (pid == current_pid())
		vpr = curvprocp;
	else {
		if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return -ESRCH;
		rele = 1;
	}

	VPROC_GETVPAGG(vpr, &oldvpag);

	/*
 	 * Make sure that the process did belong to an array session.
	 */
	if (oldvpag && (VPAG_GET_TYPE(oldvpag) != VPAG_ARRAY_SESSION)){
		if (rele)
			VPROC_RELE(vpr);
		return -EINVAL;
	}

	/* Check if new array sessions have been restricted */
	if (VPAG_RESTRICT_CTL(oldvpag, VPAG_NEW_IS_RESTRICTED)) {
		if (rele)
		    VPROC_RELE(vpr);
		return -EPERM;
	}

	/* Create a new vpagg */
	VPROC_GET_NICE(curvprocp, &nice, get_current_cred(), error);
	error = vpag_create(INVALID_PAGGID, pid, nice, &newvpag, VPAG_ARRAY_SESSION);
	if (error) {
		if (rele)
			VPROC_RELE(vpr);
		return -error;
	}
	
	/* Initialize the rest of the new vpagg */
	if (oldvpag) {
		prid_t	prid;

		prid = VPAG_GETPRID(oldvpag);
		VPAG_SETPRID(newvpag, prid);

		if (!VPAG_SPINFO_ISDFLT(oldvpag)) {
			char    *spibuf;
			int	oldspilen;

			oldspilen = VPAG_GETSPILEN(oldvpag);
			spibuf = kern_malloc(oldspilen);
			ASSERT(spibuf != NULL);

			VPAG_GETSPINFO(oldvpag, spibuf, oldspilen);
			VPAG_SETSPINFO(newvpag, spibuf, oldspilen);

			kern_free(spibuf);
		}
	} 


	/* Officially change to the new array session */
	if (oldvpag)
		VPAG_LEAVE(oldvpag, pid);
	VPROC_SETVPAGG(vpr, newvpag);
	if (rele)
		VPROC_RELE(vpr);

	return 0;
}


/*
 * arsess_op:
 *	perform some sort of operation on an individual array session
 *	for the current process.
 *
 *	XXX an unprivileged process can infer the existence of another
 *	    array session with these - are additional capability checks
 *	    needed for trix?
 */
int
arsess_op(int funccode, ash_t *ashptr, void *userbuf, int buflen)
{
	int result;
	vpagg_t *vpag;

	/* Caution! Do not do a "return" after calling arsess_findvpag, */
	/* the returned vpag will have had its refcnt incremented if    */
	/* it does not belong to the local process.			*/

	/* Proceed according to the function code */
	switch (funccode) {

	    case ARSOP_NOP:
	    {
		    /* Do nothing other than ensure the array session exists */
		    result = arsess_findvpag(ashptr, &vpag);
		    if (result != 0) {
			    return result;
		    }
		    break;
	    }


	    case ARSOP_GETSPI:
	    {
		    /* Extract the Service Provider Information */
		    char *spibuf;
		    int  copylen;

		    /* Make sure the buffer length is reasonable */
		    if (buflen < 1) {
			    return EINVAL;
		    }

		    /* Get the vpag for the requested array session */
		    result = arsess_findvpag(ashptr, &vpag);
		    if (result != 0) {
			    return result;
		    }

		    /* Determine size of this pagg's SPI */
		    copylen = VPAG_GETSPILEN(vpag);
		    if (buflen < copylen) {
			    copylen = buflen;
		    }

		    /* Allocate storage for the SPI data */
		    spibuf = kern_malloc(copylen);
		    if (spibuf == NULL) {
			    result = ENOMEM;
			    break;
		    }

		    /* Obtain the info from the pagg */
		    VPAG_GETSPINFO(vpag, spibuf, copylen);

		    /* Copy it to the user's buffer */
		    if (copyout(spibuf, userbuf, copylen) != 0) {
			    kern_free(spibuf);
			    result = EFAULT;
			    break;
		    }

		    /* Done with the SPI buffer */
		    kern_free(spibuf);

		    /* Pad the information if necessary */
		    if (copylen < buflen) {
			    char *ptr = ((char *) userbuf) + copylen;

			    if (uzero(ptr, buflen - copylen) != 0) {
				    result = EFAULT;
				    break;
			    }
		    }

		    break;
	    }


	    case ARSOP_SETSPI:
	    {
		    /* Set the Service Provider Information */

		    /* Make sure user is allowed to do this */
		    if (!cap_able(CAP_ACCT_MGT)) {
			    return EPERM;
		    }

		    /* Get the vpag for the requested array session */
		    result = arsess_findvpag(ashptr, &vpag);
		    if (result != 0) {
			    return result;
		    }

		    /* Set up the arguments differently depending on */
		    /* whether or not the default was specified.     */
		    if (userbuf == NULL) {

			    /* Reset to default SPI */
			    VPAG_SETSPINFO(vpag, NULL, 0);
		    }
		    else {
			    char *spibuf;
			    int  ars_spilen;

			    /* Determine the maximum SPI length */
			    ars_spilen = VPAG_GETSPILEN(vpag);

			    /* Make sure the buffer length is reasonable */
			    if (buflen < 0  ||  buflen > ars_spilen) {
				    result = EINVAL;
				    break;
			    }

			    /* Allocate storage for a copy of the SPI data */
			    spibuf = kern_malloc(ars_spilen);
			    if (spibuf == NULL) {
				    result = ENOMEM;
				    break;
			    }

			    /* Obtain the new info from the user's buffer */
			    if (buflen > 0  &&
				copyin(userbuf, spibuf, buflen) != 0)
			    {
				    kern_free(spibuf);
				    result = EFAULT;
				    break;
			    }

			    /* Pad the data if necessary */
			    if (buflen < ars_spilen) {
				    bzero(spibuf + buflen,
					  ars_spilen - buflen);
			    }

			    /* Store the new info into the pagg */
			    VPAG_SETSPINFO(vpag, spibuf, ars_spilen);

			    /* Done with the buffer */
			    kern_free(spibuf);
		    }

		    break;
	    }


	    case ARSOP_GETSPILEN:
	    {
		    /* Get the length of the session's Service Provider Info */
		    int length;

		    /* Make sure the user's buffer has the right size */
		    if (buflen != sizeof(int)) {
			    return EINVAL;
		    }

		    /* Get the vpag for the requested array session */
		    result = arsess_findvpag(ashptr, &vpag);
		    if (result != 0) {
			    return result;
		    }

		    /* Determine size of this pagg's SPI */
		    length = VPAG_GETSPILEN(vpag);

		    /* Copy it to the user's buffer */
		    if (copyout(&length, userbuf, buflen) != 0) {
			    result = EFAULT;
			    break;
		    }

		    break;
	    }


	    case ARSOP_SETSPILEN:
	    {
		    /* Set the length of the Service Provider Information */
		    int  newval;

		    /* Make sure user is allowed to do this */
		    if (!cap_able(CAP_ACCT_MGT)) {
			    return EPERM;
		    }

		    /* Make sure an appropriate buffer size was specified */
		    if (buflen != sizeof(newval)) {
			    return EINVAL;
		    }

		    /* Copy the new value from the user's buffer */
		    if (copyin(userbuf, &newval, sizeof(newval))) {
			    return EFAULT;
		    }

		    /* Make sure the size is reasonable (for some   */
		    /* fairly arbitrary definition of "reasonable") */
		    if (newval < 1  ||  newval > MAX_SPI_LEN) {
			    return EINVAL;
		    }

		    /* Get the vpag for the requested array session */
		    result = arsess_findvpag(ashptr, &vpag);
		    if (result != 0) {
			    return result;
		    }

		    /* Store the new info into the pagg */
		    VPAG_SETSPILEN(vpag, newval);

		    break;
	    }


	    case ARSOP_FLUSHACCT:
	    {

		    uthread_t *ut;

		    /* Make sure the user is allowed to do this */
		    if (!cap_able(CAP_ACCT_MGT)) {
			    return EPERM;
		    }

		    /* Get the vpag for the requested array session */
		    result = arsess_findvpag(ashptr, &vpag);
		    if (result != 0) {
			    return result;
		    }

		    /* SAT needs a uthread from the array session */
		    ut = arsess_chooseuthread(vpag);
		    if (ut == NULL) {
			    result = EINVAL;
			    break;
		    }

		    /* Go flush the accounting data */
		    VPAG_FLUSH_ACCT(vpag, ut);
		    uscan_rele(&UT_TO_PROC(ut)->p_proxy);

		    break;
	    }


	    case ARSOP_GETINFO:
	    {
		    /* Extract accounting information */
		    arsess_t *info;
		    int  copylen;

		    /* Make sure the buffer length is reasonable */
		    if (buflen < 1) {
			    return EINVAL;
		    }

		    /* Get the vpag for the requested array session */
		    result = arsess_findvpag(ashptr, &vpag);
		    if (result != 0) {
			    return result;
		    }

		    /* Allocate storage for the data */
		    info = (arsess_t *) kern_malloc(sizeof(arsess_t));
		    if (info == NULL) {
			    result = ENOMEM;
			    break;
		    }

		    /* Don't copy more than requested */
		    copylen = sizeof(arsess_t);
		    if (buflen < copylen) {
			    copylen = buflen;
		    }

		    /* Obtain the info from the pagg */
		    VPAG_EXTRACT_ARSESS_INFO(vpag, info);

		    /* Copy it to the user's buffer */
		    if (copyout(info, userbuf, copylen) != 0) {
			    kern_free(info);
			    result = EFAULT;
			    break;
		    }

		    /* Done with the data storage */
		    kern_free(info);

		    /* Pad the information if necessary */
		    if (copylen < buflen) {
			    char *ptr = ((char *) userbuf) + copylen;

			    if (uzero(ptr, buflen - copylen) != 0) {
				    result = EFAULT;
				    break;
			    }
		    }

		    break;
	    }


	    case ARSOP_GETCHGD:
	    {
		    /* Extract shadow accounting information */
		    shacct_t *info;
		    int  copylen;

		    /* Make sure the buffer length is reasonable */
		    if (buflen < 1) {
			    return EINVAL;
		    }

		    /* Get the vpag for the requested array session */
		    result = arsess_findvpag(ashptr, &vpag);
		    if (result != 0) {
			    return result;
		    }

		    /* Allocate storage for the data */
		    info = (shacct_t *) kern_malloc(sizeof(shacct_t));
		    if (info == NULL) {
			    result = ENOMEM;
			    break;
		    }

		    /* Don't copy more than requested */
		    copylen = sizeof(shacct_t);
		    if (buflen < copylen) {
			    copylen = buflen;
		    }

		    /* Obtain the info from the pagg */
		    VPAG_EXTRACT_SHACCT_INFO(vpag, info);

		    /* Copy it to the user's buffer */
		    if (copyout(info, userbuf, copylen) != 0) {
			    kern_free(info);
			    result = EFAULT;
			    break;
		    }

		    /* Done with the data storage */
		    kern_free(info);

		    /* Pad the information if necessary */
		    if (copylen < buflen) {
			    char *ptr = ((char *) userbuf) + copylen;

			    if (uzero(ptr, buflen - copylen) != 0) {
				    result = EFAULT;
				    break;
			    }
		    }

		    break;
	    }


	    case ARSOP_RESTRICT_NEW:
	    {
		    /* Restrict new array sessions */

		    /* Get the vpag for the requested array session */
		    result = arsess_findvpag(ashptr, &vpag);
		    if (result != 0) {
			    return result;
		    }

		    /* Go set the appropriate restriction in the pagg */
		    if (VPAG_RESTRICT_CTL(vpag, VPAG_RESTRICT_NEW) < 0) {
			    result = EINVAL;
			    break;
		    }

		    break;
	    }


	    case ARSOP_ALLOW_NEW:
	    {
		    /* Permit new array sessions */
		    
		    /* Make sure the user is allowed to do this */
		    if (!cap_able(CAP_ACCT_MGT)) {
			    return EPERM;
		    }

		    /* Get the vpag for the requested array session */
		    result = arsess_findvpag(ashptr, &vpag);
		    if (result != 0) {
			    return result;
		    }

		    /* Go set the appropriate restriction in the pagg */
		    if (VPAG_RESTRICT_CTL(vpag, VPAG_ALLOW_NEW) < 0) {
			    result = EINVAL;
			    break;
		    }

		    break;
	    }


	    default:
	    {
		    /* Unrecognized function code */
		    return EINVAL;
	    }
	}

	/* If a real ASH was specified, then the corresponding vpag */
	/* has been held, so we must now release it.		    */
	if (ashptr != NULL  &&  *ashptr >= 0LL) {
		VPAG_RELE(vpag);
	}

	return result;
}


/*
 * arsess_setash:
 *	change the array session handle for the specified array session to
 *	a different value. The new array session handle must not currently
 *	be in use by another session.
 */
int
arsess_setash(ash_t oldash, ash_t newash)
{
	vpagg_t *vpag;
	int	error;

	/* Find the arsess to be changed */
	if (oldash < 0) {
		VPROC_GETVPAGG(curvprocp, &vpag);
	} else
		vpag = VPAG_LOOKUP(oldash);

	if (!vpag) {
		return ESRCH;
	}

	error = vpag_set_paggid(vpag, newash);

	if (oldash >= 0)
		VPAG_RELE(vpag);

	return error;

}


int
arsess_setmachid(int newmachid)
{
	return vpag_setmachid(newmachid);
}


/*
 * arsess_setprid:
 *	set the project ID for the specified array session to a 
 *	specified value. 
 */
int
arsess_setprid(ash_t ash, prid_t newprid)
{
	vpagg_t *vpag;

	/* Make sure the new project ID is not negative */
	if (newprid < 0LL)
	    return EINVAL;

	/* Find the arsess to be changed */
	if (ash < 0) {
		VPROC_GETVPAGG(curvprocp, &vpag);
	} else
		vpag = VPAG_LOOKUP(ash);

	if (!vpag)
	    return ESRCH;

	VPAG_SETPRID(vpag, newprid);
	if (ash >= 0)
		VPAG_RELE(vpag);

	return 0;
}
