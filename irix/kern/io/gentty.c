/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:io/gentty/gentty.c	1.17"*/
#ident	"$Revision: 3.42 $"

/*
 * Indirect driver for controlling tty.
 */

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vproc.h>
#include <ksys/vfile.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <ksys/vsession.h>
#include <sys/strsubr.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#ifdef	JTK_DEBUG
/* #include <sys/systm.h> */
#endif	/* JTK_DEBUG */

int sydevflag = D_MP;

/*
 * Check that the process has a controlling terminal.
 */
static int
sybegin(vsession_t **vsesspp, dev_t *ttydevp, vnode_t **ttyvpp)
{
	int error;
	vnode_t *vnp;
	vproc_t *vpr;
	vp_get_attr_t attr;
	vsession_t *vsp;

	vpr = VPROC_LOOKUP_STATE(current_pid(), ZYES);

	ASSERT(vpr);

	VPROC_GET_ATTR(vpr, VGATTR_SID|VGATTR_HAS_CTTY, &attr);

	VPROC_RELE(vpr);

	if (!attr.va_has_ctty)
		return ENXIO;

	vsp = VSESSION_LOOKUP(attr.va_sid);
	ASSERT(vsp);
	VSESSION_CTTY_HOLD(vsp, &vnp, error);
	if (error) {
		VSESSION_RELE(vsp);
		return ENXIO;	/* XXXbe svr4 returns EIO for null vp */
	}

	ASSERT(vnp->v_type == VCHR && vnp->v_stream);
	*vsesspp = vsp;
	*ttydevp = vnp->v_rdev;
	*ttyvpp = vnp;
	return 0;			/* all is well */
}

static void
syend(vsession_t *vsp, vnode_t *vp)
{
	VSESSION_CTTY_RELE(vsp, vp);
	VSESSION_RELE(vsp);
}

/* ARGSUSED */
int
syopen(devp, flag, otyp, cr)
	dev_t		*devp;
	int		flag;
	int		otyp;
	struct cred	*cr;
{
	     int		error;
	     dev_t		ttyd;
	auto vnode_t		*ttyvp;
	     vsession_t		*vsp;
	     /* REFERENCED */
	     struct cdevsw	*my_cdevsw;


	if (error = sybegin(&vsp, &ttyd, &ttyvp))
		return error;

#if	(defined(CELL_IRIX) || defined(SPECFS_TEST))

	/*
	 * The controlling tty vnode "may" simply be a client vnode,
	 * needing some underlying distribution mechanism.
	 * Use the VOP_ interface in order to allow this to happen.
	 */
	{
		vnode_t	*vpp;


		vpp = ttyvp;

		VOP_OPEN(ttyvp, &vpp, flag, cr, error);
	}

#else	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	my_cdevsw = get_cdevsw(ttyd);
	ASSERT(my_cdevsw != NULL);

	if (error = cdhold(my_cdevsw)) {
		syend(vsp, ttyvp);
		return error;
	}

	if (my_cdevsw->d_str) {
		VN_HOLD(ttyvp);

		error =  stropen(ttyvp, &ttyd, NULL, flag, cr);

		VN_RELE(ttyvp);
	} else {
		error = cdopen(my_cdevsw, &ttyd, flag, otyp, cr);
	}

	cdrele(my_cdevsw);

#endif	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	syend(vsp, ttyvp);

	return error;
}

/* ARGSUSED */
int
syclose(dev, flag, otyp, cr)
	dev_t		dev;
	int		flag;
	int		otyp;
	struct cred	*cr;
{
#if	(defined(CELL_IRIX) || defined(SPECFS_TEST))
	     int		error;
	     dev_t		ttyd;
	auto vnode_t		*ttyvp;
	     vsession_t		*vsp;


	if (error = sybegin(&vsp, &ttyd, &ttyvp))
		return error;


	/*
	 * The controlling tty vnode "may" simply be a client vnode,
	 * needing some underlying distribution mechanism.
	 * Use the VOP_ interface in order to allow this to happen.
	 */
	VOP_CLOSE(ttyvp, flag, L_TRUE, cr, error);

	syend(vsp, ttyvp);

	return error;

#else	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	return 0;

#endif	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */
}

/* ARGSUSED */
int
syread(dev, uiop, cr)
	dev_t		dev;
	struct uio	*uiop;
	struct cred	*cr;
{
	     int		error;
	auto dev_t		ttyd;
	auto vnode_t		*ttyvp;
	     vsession_t		*vsp;
	     /* REFERENCED */
	     struct cdevsw	*my_cdevsw;

	if (error = sybegin(&vsp, &ttyd, &ttyvp))
		return error;

#if	(defined(CELL_IRIX) || defined(SPECFS_TEST))

	/*
	 * The controlling tty vnode "may" simply be a client vnode,
	 * needing some underlying distribution mechanism.
	 * Use the VOP_ interface in order to allow this to happen.
	 */
	VOP_READ(ttyvp, uiop, 0, cr, NULL, error);

#else	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	my_cdevsw = get_cdevsw(ttyd);
	ASSERT(my_cdevsw != NULL);
	if (my_cdevsw->d_str)
		error = strread(ttyvp, uiop, cr);
	else
		error = cdread(my_cdevsw, ttyd, uiop, cr);

#endif	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	syend(vsp, ttyvp);

	return error;
}

/* ARGSUSED */
int
sywrite(dev, uiop, cr)
	dev_t		dev;
	struct uio	*uiop;
	struct cred	*cr;
{
	     int		error;
	auto dev_t		ttyd;
	auto vnode_t		*ttyvp;
	     vsession_t		*vsp;
	     /* REFERENCED */
	     struct cdevsw	*my_cdevsw;

	if (error = sybegin(&vsp, &ttyd, &ttyvp))
		return error;

#if	(defined(CELL_IRIX) || defined(SPECFS_TEST))

	/*
	 * The controlling tty vnode "may" simply be a client vnode,
	 * needing some underlying distribution mechanism.
	 * Use the VOP_ interface in order to allow this to happen.
	 */
	VOP_WRITE(ttyvp, uiop, 0, cr, NULL, error);

#else	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	my_cdevsw = get_cdevsw(ttyd);
	ASSERT(my_cdevsw != NULL);
	if (my_cdevsw->d_str)
		error = strwrite(ttyvp, uiop, cr);
	else
		error = cdwrite(my_cdevsw, ttyd, uiop, cr);

#endif	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	syend(vsp, ttyvp);

	return error;
}

/* ARGSUSED */
int
syioctl(dev, cmd, arg, mode, cr, rvalp)
	dev_t		dev;
	int		cmd;
	void		*arg;
	int		mode;
	struct cred	*cr;
	int		*rvalp;
{
	     int		error;
	auto dev_t		ttyd;
	auto vnode_t		*ttyvp;
	     vsession_t		*vsp;
	     /* REFERENCED */
	     struct cdevsw	*my_cdevsw;

	if (error = sybegin(&vsp, &ttyd, &ttyvp))
		return error;

#if	(defined(CELL_IRIX) || defined(SPECFS_TEST))

	/*
	 * The controlling tty vnode "may" simply be a client vnode,
	 * needing some underlying distribution mechanism.
	 * Use the VOP_ interface in order to allow this to happen.
	 */
	
	VOP_IOCTL(ttyvp, cmd, arg, mode, cr, rvalp, NULL, error);

#else	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	my_cdevsw = get_cdevsw(ttyd);
	ASSERT(my_cdevsw != NULL);
	if (my_cdevsw->d_str) {
		error = strioctl(ttyvp, cmd, arg, mode, U_TO_K,
				 cr, rvalp);
	} else {
		error = cdioctl(my_cdevsw, ttyd, cmd, arg, mode, cr,
				rvalp);
	}

#endif	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	syend(vsp, ttyvp);

	return error;
}


/* ARGSUSED */
int
sypoll( dev_t		dev,
	short		events,
	int		anyyet,
	short		*reventsp,
       struct pollhead	**phpp,
       unsigned int	*genp)
{
	     int		error;
	auto dev_t		ttyd;
	auto vnode_t		*ttyvp;
	     vsession_t		*vsp;
	     /* REFERENCED */
	     struct cdevsw	*my_cdevsw;

	if (error = sybegin(&vsp, &ttyd, &ttyvp))
		return error;

#if	(defined(CELL_IRIX) || defined(SPECFS_TEST))

	/*
	 * The controlling tty vnode "may" simply be a client vnode,
	 * needing some underlying distribution mechanism.
	 * Use the VOP_ interface in order to allow this to happen.
	 */
	VOP_POLL(ttyvp, events, anyyet, reventsp, phpp, genp, error);

#else	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	my_cdevsw = get_cdevsw(ttyd);
	ASSERT(my_cdevsw != NULL);
	if (my_cdevsw->d_str)
		error = strpoll(ttyvp->v_stream, events, anyyet, reventsp,
				phpp, genp);
	else
		error = cdpoll(my_cdevsw, dev, events, anyyet, reventsp,
				phpp, genp);


#endif	/* ! (defined(CELL_IRIX) || defined(SPECFS_TEST)) */

	syend(vsp, ttyvp);

	return error;
}
