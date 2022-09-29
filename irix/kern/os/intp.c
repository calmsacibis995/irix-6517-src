/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.32 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/vnode.h"
#include "sys/uio.h"
#include "sys/pathname.h"
#include "sys/exec.h"
#include "sys/kmem.h"
#include "sys/systm.h"
#include "sys/capability.h"
#include "sys/cred.h"
#include "sys/kabi.h"
#include "sys/imon.h"
#include <string.h>
#include "os/proc/pproc_private.h"
#include <sys/atomic_ops.h>

extern int	nosuidshells;

/*
 * Crack open a '#!' line.
 */
static int
getintphead(struct intpdata *idatap, vnode_t *vp, off_t vnsize)
{
	int error;
	char *cp, *linep;
	int rdsz;

	/*
	 * Read the entire line and confirm that it starts with '#!'.
	 */
	rdsz = INTPSZ;
	if (rdsz > vnsize)
		rdsz = vnsize;
	if ((error = exrdhead(vp, (off_t) 0, rdsz,
			&idatap->line1p)) != 0)
		return error;
	idatap->line1p_sz = rdsz;
	linep = idatap->line1p;
	if (linep[0] != '#' || linep[1] != '!')
		return ENOEXEC;
	/*
	 * Blank all white space and find the newline.
	 */
	cp = &linep[2];
	linep += rdsz;
	for (; cp < linep && *cp != '\n'; cp++)
		if (*cp == '\t')
			*cp = ' ';
	if (cp >= linep)
		return E2BIG;

	ASSERT(*cp == '\n');
	*cp = '\0';

	/*
	 * Locate the beginning and end of the interpreter name.
	 * In addition to the name, one additional argument may
	 * optionally be included here, to be prepended to the
	 * arguments provided on the command line.  Thus, for
	 * example, you can say
	 *
	 * 	#! /usr/bin/awk -f
	 */
	for (cp = &idatap->line1p[2]; *cp == ' '; cp++)
		;
	if (*cp == '\0')
		return ENOEXEC;
	idatap->intp_name = cp;
	while (*cp && *cp != ' ') {
		cp++;
	}
	if (*cp == '\0')
		idatap->intp_arg = NULL;
	else {
		*cp++ = '\0';
		while (*cp == ' ')
			cp++;
		if (*cp == '\0')
			idatap->intp_arg = NULL;
		else
			idatap->intp_arg = cp;
	}
	return 0;
}

int
intpexec(vnode_t *vp, vattr_t *vattrp, struct uarg *args, int level)
{
	int error = 0;
	struct pathname intppn;
	struct vnode *nvp;
	char **pap;
	int incr;
	__psint_t ap;
	ckpt_handle_t *ckptp = NULL;
#ifdef CKPT
	ckpt_handle_t ckpt = NULL;

	if (ckpt_enabled)
		ckptp = &ckpt;
#endif
	if (level) 	/* Can't recurse */
		return ENOEXEC;

	if ((args->ua_setid) && nosuidshells && !_CAP_ABLE(CAP_DAC_OVERRIDE))
		return EPERM;

	if ((error = getintphead(&args->ua_idata, vp, vattrp->va_size)) != 0)
		goto bad;

	/*
	 * Look the new vnode up.
	 */
	if (error = pn_get(args->ua_idata.intp_name, UIO_SYSSPACE, &intppn))
		goto bad;
	_SAT_PN_SAVE(&intppn, curuthread);

	_SAT_PN_BOOK(SAT_EXEC, curuthread);
	if (error = lookuppn(&intppn, FOLLOW, NULLVPP, &nvp, ckptp)) {
		pn_free(&intppn);
		goto bad;
	}
	pn_free(&intppn);

	/*
	 * Set up prefix arguments for handling by fuexarg later on.
	 * There are either 2 or 3 prefix args as follows:
	 *	interpreter name
	 *	optional interpreter arg 
	 *	original file name (replaces argv0)
	 */
	pap = args->ua_prefix_argv;
	*pap++ = args->ua_idata.intp_name;
	args->ua_prefixc++;

	if (args->ua_idata.intp_arg) {
		*pap++ = args->ua_idata.intp_arg;
		args->ua_prefixc++;
	}

	/*
	 * For setid scripts, the filename is replaced with
	 * a filename of the form "/dev/fd/n", which when opened
	 * will yield a descriptor to the script file.  This closes
	 * a security hole where a setid script is exec'd, and then
	 * replaced with a totally different file, thus causing an
	 * interpreter which is running setid to interpret a different
	 * script.
	 * We do not open the file to be interpreted here, but simply
	 * record the vnode of the file.  When the process is known
	 * to be singly threaded, it calls intpopen() to open the file,
	 * obtain a file descriptor to the open file, and construct
	 * a filename of the form "/dev/fd/n".
	 * This must be done when the process is known to be singly
	 * threaded to prevent a race in a process which consists of
	 * multiple LWPs: another LWP in the process could close
	 * the file descriptor and open/dup/dup2 a new file which
	 * corresponds to the file descriptor (hence causing the
	 * interpreter to interpret a different script, just what
	 * we were trying to prevent in the first place!).
	 */
	if (args->ua_setid) {
		#pragma mips_frequency_hint NEVER
		/*
		 * Check to see if we can access the file as effective
		 * user. This validates the call to execopen in remove_proc()
		 * which uses sys_cred.
		 */
		struct cred *cr = crdup(get_current_cred());

		if (!cr) {
			error = ENOMEM;
			goto bad;
		}
		
		cr->cr_uid = args->ua_uid;
		cr->cr_gid = args->ua_gid;
		cr->cr_cap = args->ua_cap;
		VOP_ACCESS(vp, VREAD, cr, error);
		crfree(cr);
		if (error)
			goto bad;

		/*
		 * Point at the vnode that we will need to open
		 * once we are non-shared.
		 */
		args->ua_intpvp = vp;
		
		/*
		 * Rather than playing storage allocation games, we
		 * assign stack space here knowing that it will be
		 * one of our callers that will use this area.
		 */
		args->ua_fname = args->ua_intpfname;
		sprintf(args->ua_fname, "/dev/fd/XXXXXXX");
	}

	*pap++ = args->ua_fname;
	args->ua_prefixc++;
	*pap++ = NULL;
	args->ua_prefixp = args->ua_prefix_argv;
	
	/*
	 * increment the argp pointer since argv0 gets replaced
	 * by the originally exec'ed file name.  Do nothing if
	 * there aren't any arguments.
	 */
	if (args->ua_argp) {
#if _MIPS_SIM == _ABI64
		if (ABI_IS_IRIX5_64(get_current_abi())) {
			incr = sizeof(__int64_t);
			ap = (__psint_t)fulong(args->ua_argp);
		}
		else
#endif
		{
			incr = sizeof(__int32_t);
			ap = (__psint_t)fuword(args->ua_argp);
		}
		if (ap != NULL)
			args->ua_argp = (char **)((__psint_t)args->ua_argp + incr);
	}
#ifdef CKPT
	args->ua_intp_ckpt = args->ua_ckpt;
	args->ua_ckpt = (ckptp && *ckptp)? ckpt_lookup_add(nvp, *ckptp) : -1;
#endif
	args->ua_intp_vp = nvp;

	error = gexec(&nvp, args, 1);

bad:
	if (error && args->ua_exec_cleanup) {
#ifdef CKPT
		args->ua_ckpt = args->ua_intp_ckpt;
#endif
		/* free buffer allocated for interpreter pathname */
		exhead_free(args->ua_idata.line1p);
	}
	return error;
}
