/* mpconf.c
 *
 *      For MIPS ABI provide control and information for multi-processor 
 *	services.
 *
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
#ident "$Revision: 1.3 $"

/*
 * mpconf(3C) - For MIPS ABI provide control and information for 
 *		multi-processor services.
 */

#pragma weak mpconf = _mpconf

#define _MIPSABI_SOURCE 3
#include "synonyms.h"
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/systeminfo.h>
#include <sys/syssgi.h>
#include <sys/sysmp.h>
#include <sys/mpconf.h>
#include <sys/pda.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>		/* For ptrdiff_t */
#include <stdlib.h>
#include <unistd.h>

/*
 * The following errors from mpconf are stored in the variable 'errno' and 
 * a minus one (-1) is returned:
 *
 *	EPERM	The command is _MIPS_MP_PROCESSOR_EXBIND or 
 *		_MIPS_MP_PROCESSOR_EXUNBIND and the effective user ID is 
 *		not superuser, or _MIPS_MP_PROCESSOR_BIND is used to bind,
 *		or _MIPS_MP_PROCESSOR_UNBIND is used to unbind, an 
 *		exclusively bound process.
 *
 *	ESRCH	No process corresponding to that specified by a 
 *		_MIPS_MP_PROCESSOR_BIND, _MIPS_MP_PROCESSOR_UNBIND, 
 *		_MIPS_MP_PROCESSOR_EXBIND, _MIPS_MP_PROCESSOR_EXUNBIND, 
 *		or _MIPS_MP_PROCESSOR_PID could be found.
 *
 *	EINVAL	The processor named by a _MIPS_MP_ISPROCESSOR_AVAIL,
 *		_MIPS_MP_PROCESSOR_ACCT, _MIPS_MP_PROCESSOR_TOTACCT,
 *		_MIPS_MP_PROCESSOR_PID, _MIPS_MP_PROCESSOR_BIND, or 
 *		_MIPS_MP_PROCESSOR_EXBIND command does not exist, or 
 *		the cmd argument is invalid.
 *
 *	EBUSY	An attempt was made to bind to an unavailable processor.
 *		A processor may be unavailable because the system does not
 *		allow it to be exclusively bound.  If the command is a
 *		non-exclusive binding, a processor with a previous exclusive
 *		binding is unavailable.  If the command is an exclusive 
 *		binding, a processor with a previous non-exclusive binding
 *		for a different process is unavailable.
 *
 *	EFAULT	An invalid buffer address has been supplied by the 
 *		calling process.
 *
 *	ENOMEM	A routine was unable to dynamically allocate enough space.
 */

int
mpconf(int cmd, ...)
{
	struct	abi_sysinfo *si = NULL;
	struct	pda_stat *pdap = NULL;
	struct	pda_stat *pdastarea = NULL;
	pid_t	*pid	= NULL;
	pid_t	apid	= 0;
	int	*rval	= NULL;
	ptrdiff_t size	= 0;
	ptrdiff_t pnum	= 0;
	int	naprocs = 0;
	int	saved_errno;
	int	i;
	ptrdiff_t arg1, arg2, arg3;

	va_list ap;

	va_start(ap, cmd);
	arg1 = va_arg(ap, ptrdiff_t);
	arg2 = va_arg(ap, ptrdiff_t);
	arg3 = va_arg(ap, ptrdiff_t);
	va_end(ap);

	switch(cmd) {
	case _MIPS_MP_NPROCESSORS:
		/*
		 * usage:
		 *	mpconf(_MIPS_MP_NPROCESSORS);
		 *
		 * Returns the number of physical processors in the system.
		 */
		return((int)sysmp(MP_NPROCS));
		/* NOTREACHED */
	case _MIPS_MP_NAPROCESSORS:
		/*
		 * usage:
		 *	mpconf(_MIPS_MP_NAPROCESSORS);
		 *
		 * Returns the number of processors which are 
		 * currently active and available for general use.
		 * This number excludes those processors that are exclusively
		 * bound by a process.
		 */
		return((int)sysmp(MP_NAPROCS));
		/* NOTREACHED */
	case _MIPS_MP_PROCESSOR_ACCT:
		/*
		 * usage:
		 *	mpconf(_MIPS_MP_PROCESSOR_ACCT, int pnum, 
		 *		struct abi_sysinfo *si, int size);
		 *
		 * The abi_sysinfo structure pointed to by si is filled 
		 * in with accounting information for the processor 
		 * specified by pnum. The abi_sysinfo structure contains 
		 * statistics for the following:
		 *
		 *	- Ticks that cpu was idle not waiting on I/O.
		 *	- Ticks that cpu was in user mode.
		 *	- Ticks that cpu was in kernel mode.
		 *	- Ticks that cpu was idle, waiting on I/O.
		 *
		 * size specifies the maximum number of bytes to transfer 
		 * (usually the size of an abi_sysinfo structure).
		 *
		 * On success, the command returns the number of bytes 
		 * transferred.
		 */
		pnum = arg1;
		si = (struct abi_sysinfo *)arg2;
		size = arg3;
		if (!size) {
			setoserror(EINVAL);
			return(-1);
		}
		if (!si) {
			setoserror(EFAULT);
			return(-1);
		}
		if (size > (int)sizeof(struct abi_sysinfo))
			size = (int)sizeof(struct abi_sysinfo);

		if (sysmp(MP_SAGET1, MPSA_SINFO, si, size, pnum) == -1) {
			return(-1);
		}
		return((int)size);
		/* NOTREACHED */
	case _MIPS_MP_PROCESSOR_TOTACCT:
		/*
		 * usage:
		 *	mpconf(_MIPS_MP_PROCESSOR_TOTACCT, 
		 *		struct abi_sysinfo *si, size);
		 *
		 * The abi_sysinfo structure pointed to by si is filled in 
		 * with accounting information summed across all processors.
		 * size specifies the maximum number of bytes to transfer 
		 * (usually the size of an abi_sysinfo structure).
		 *
		 * On success, the command returns the number of bytes 
		 * transferred.
		 */
		si = (struct abi_sysinfo *)arg1;
		size = arg2;
		if (!size) {
			setoserror(EINVAL);
			return(-1);
		}
		if (!si) {
			setoserror(EFAULT);
			return(-1);
		}
		if (size > (int)sizeof(struct abi_sysinfo))
			size = (int)sizeof(struct abi_sysinfo);

		if (sysmp(MP_SAGET, MPSA_SINFO, si, size) == -1) {
			return(-1);
		}
		return((int)size);
		/* NOTREACHED */
	case _MIPS_MP_ISPROCESSOR_AVAIL:
		/*
		 * usage:
		 *	mpconf(_MIPS_MP_ISPROCESSOR_AVAIL, int pnum);
		 *
		 * Returns 1 if the processor specified by pnum 
		 * is available and non exclusively bound by any process.
		 * If the processor is not available, zero (0) will be 
		 * returned.
		 */
		pnum = arg1;
		if ((naprocs = (int)sysmp(MP_NPROCS)) == -1)
			return(-1);
		pdap = pdastarea = (struct pda_stat *)calloc((size_t)naprocs,
				(size_t)sizeof(struct pda_stat));
		if (!pdastarea) {
			setoserror(ENOMEM); /* XXX - ENOMEM not in BB2.0 */
			return(-1);
		}
		if (sysmp(MP_STAT, pdastarea) == -1) {
			free(pdastarea);
			return(-1);
		}
		for (i=0; i<naprocs; i++) {
			if (pdap->p_cpuid == pnum) {
				if (pdap->p_flags & PDAF_ENABLED) {
					free(pdastarea);
					return(0); /* pnum is not available */
				} else {
					free(pdastarea);
					return(1); /* pnum is available */
				}
			}
			pdap++;
		}
		free(pdastarea);
		return(0);		/* pnum is not available */
		/* NOTREACHED */
	case _MIPS_MP_PROCESSOR_BIND:
		/*
		 * usage:
		 *	mpconf(_MIPS_MP_PROCESSOR_BIND, int pnum, 
		 *		pid_t *pid);
		 *
		 * Binds a process pointed to by pid to the processor 
		 * specified by pnum.  The process is restricted to run only
		 * on this processor.  The processor may continue to run
		 * other processes.  Note that the process may still run on
		 * other processors, briefly, to perform I/O or other
		 * hardware-specific actions.
		 *
		 * If a previous binding was in effect for pid, and the 
		 * binding was not exclusive, then new binding replaces the
		 * previous one.  If the previous binding was exclusive, the
		 * command fails.
		 *
		 * If a previous exclusive binding is in effect for this 
		 * processor, it is unavailable to be reserved, and the
		 * command fails.
		 *
		 * The command returns zero (0) on success.
		 */
		pnum = arg1;
		pid = (pid_t *)arg2;
		if (!pid || !*pid) {
			setoserror(ESRCH);
			return(-1);
		}
		/*
		 * Check to see if the process in pid
		 * is currently assigned to a specific processor
		 */
		if (sysmp(MP_GETMUSTRUN_PID, *pid) == -1)
			return(-1);
		/*
		 * Now check to see if there any processes that are 
		 * exclusively bound to the specified processor pnum.
		 */
		if ((naprocs = (int)sysmp(MP_NPROCS)) == -1)
			return(-1);
		pdap = pdastarea = (struct pda_stat *)calloc((size_t)naprocs,
				(size_t)sizeof(struct pda_stat));
		if (!pdastarea) {
			setoserror(ENOMEM);
			return(-1);
		}
		if (sysmp(MP_STAT, pdastarea) == -1) {
			free(pdastarea);
			return(-1);
		}
		for (i=0; i<naprocs; i++) {
			if (pdap->p_cpuid == pnum) {
				if (pdap->p_flags & PDAF_ENABLED) {
					free(pdastarea);
					setoserror(EBUSY);
					return(-1); /* pnum not available */
				} else {
					free(pdastarea);
					/* pnum is available */
					goto unrestrict;
				}
			}
			pdap++;
		}
		free(pdastarea);
		setoserror(EBUSY);
		return(-1);
	unrestrict:
		pid = (pid_t *)arg2;
		if(sysmp(MP_MUSTRUN_PID, pnum, *pid) == -1)
			return(-1);
		return(0);
		/* NOTREACHED */
	case _MIPS_MP_PROCESSOR_UNBIND:
		/*
		 * usage:
		 *	mpconf(_MIPS_MP_PROCESSOR_UNBIND, pid_t *pid);
		 *
		 * Unbinds a process pointed to by pid, making it free 
		 * to run on any processor.
		 *
		 * _MIPS_MP_PROCESSOR_UNBIND cannot unbind exclusively
		 * bound processes.  _MIPS_MP_PROCESSOR_EXUNBIND should be
		 * used for this purpose.
		 */
		pid = (pid_t *)arg1;
		if (!pid || !*pid) {
			setoserror(ESRCH);
			return(-1);
		}
		if(sysmp(MP_RUNANYWHERE_PID, *pid) == -1)
			return(-1);
		return(0);
		/* NOTREACHED */
	case _MIPS_MP_PROCESSOR_EXBIND:
		/*
		 * usage:
		 *	mpconf(_MIPS_MP_PROCESSOR_EXBIND, int pnum, 
		 *		pid_t *pid);
		 *
		 * Exclusively binds a process pointed to by pid to the 
		 * processor specified by pnum.  The process is restricted 
		 * to run only on this processor.  The processor may still 
		 * run on other processors, briefly, to perform I/O or other
		 * hardware-specific actions.  The specified processor is
		 * restricted to run only those processes that are either
		 * exclusively bound to it, or for which it must provide
		 * service due to hardware necessity, for as long as the
		 * exclusive binding is in effect.  This command requires
		 * superuser authority.
		 *
		 * If a previous binding was in effect for pid, the new 
		 * binding replaces the previous one.
		 *
		 * If a previous non-exclusive binding for a process other
		 * than pid is in effect for this processor, it is 
		 * unavailable to be reserved, and the command fails.
		 *
		 * The command returns zero (0) on success.
		 */
		pnum = arg1;
		pid = (pid_t *)arg2;
		if (!pid || !*pid) {
			setoserror(ESRCH);
			return(-1);
		}
		if (sysmp(MP_RESTRICT, pnum) == -1)
			return(-1);
		if (sysmp(MP_MUSTRUN_PID, pnum, *pid) == -1) {
			saved_errno = errno;
			/*
			 * There is a failure on binding of
			 * the process in *pid.
			 */
			/*
			 * Unrestrict the previously restricted
			 * processor pnum
			 */
			(void)sysmp(MP_EMPOWER, pnum);
			/*
			 * Make sure errno is non-zero when returning
			 * a minus one (-1)
			 */
			if (!saved_errno)
				setoserror(EBUSY);
			else
				setoserror(saved_errno);
			return(-1);
		}
		return(0);
		/* NOTREACHED */
	case _MIPS_MP_PROCESSOR_EXUNBIND:
		/*
		 * usage:
		 *	mpconf(_MIPS_MP_PROCESSOR_EXUNBIND, pid_t *pid);
		 *
		 * Unbinds an exclusively bound process pointed to by 
		 * *pid, making it free to run on any processor. If there
		 * are no other processes exclusively bound to pnum, it is
		 * freed for general use.  This command requires superuser 
		 * authority.
		 *
		 * The command returns zero (0) on success.
		 *
		 * Note: Unbinding occurs when a process exits.
		 */
		pid = (pid_t *)arg1;
		if (!pid || !*pid) {
			setoserror(ESRCH);
			return(-1);
		}
			if((pnum=sysmp(MP_GETMUSTRUN_PID, *pid)) == -1) {
				/*
				 * means that this process has not been
				 * assigned to a specific processor
				 */
				if (sysmp(MP_RUNANYWHERE_PID, *pid) == -1)
					return(-1);
			} else {
				if(sysmp(MP_RUNANYWHERE_PID, *pid) == -1)
					return(-1);
				if(sysmp(MP_EMPOWER, pnum) == -1)
					return(-1);
			}
		setoserror(0);
		return(0);
		/* NOTREACHED */
	case _MIPS_MP_PROCESSOR_PID:
		/*
		 * usage:
		 *	mpconf(_MIPS_MP_PROCESSOR_PID, pid_t pid, int *rval);
		 *
		 * If the process specified in pid is bound to a processor, 
		 * one (1) is returned and rval will be filled in with the 
		 * processor number.  There is no indication whether the
		 * binding in effect is exclusive or non-exclusive.
		 *
		 * Returns 0 if the process is not bound. If pid is not a 
		 * valid process, returns -1 and sets errno to reflect the 
		 * error.
		 */
		apid = (pid_t)arg1;
		rval = (int *)arg2;
		if (!apid) {
			setoserror(ESRCH);
			return(-1);
		}
		if (!rval) {
			setoserror(EFAULT);
			return(-1);
		}
		if((pnum = sysmp(MP_GETMUSTRUN_PID, apid)) == -1) {
			if (errno == EINVAL) {
				/*
				 * XXX - the logic in the kernel system call
				 *	for sysmp case MP_GETMUSTRUN_PID
				 *	returns EINVAL when a process has
				 *	a PDA_RUNANYWHERE state, thus the
				 *	check for EINVAL here.  Is there a
				 *	better way???
				 */
				setoserror(0);
				return(0);
			}
			return(-1);
		}
		*rval = (int)pnum;
		return(0);
		/* NOTREACHED */
	default:
		setoserror(EINVAL);
		return(-1);
	}
}
