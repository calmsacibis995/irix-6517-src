/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	based on @(#)uipc_syscalls.c	(Berkeley) tahoe
 *
 * $Revision: 1.40 $
 */


#include <tcp-param.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/domain.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/sema.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/xlate.h>
#include <sys/systm.h>
#include <sys/sat.h>
#include <sys/vsocket.h>
#include <sys/unpcb.h>

extern struct domain unixdomain;
extern struct domain *domains;
extern mrlock_t unp_lock;
#define UNP_INITLOCK()	mrlock_init(&unp_lock, MRLOCK_BARRIER, "unpmrlock", 0)

int
uds_add(void)
{
	extern struct unpcb unpcb_list;

	unixdomain.dom_next = domains;
	domains = &unixdomain;

	unpcb_list.unp_lnext = unpcb_list.unp_lprev = &unpcb_list;

	UNP_INITLOCK();
	return(1);
}

/*
 * System call interface to the socketpair abstraction.
 */
struct socketpaira {
	sysarg_t	domain;
	sysarg_t	type;
	sysarg_t	protocol;
	sysarg_t	rsv;
};


int
socketpair(struct socketpaira *uap)
{
	int error;
	/* REFERENCED */
	int error1;
	struct vfile *fp1, *fp2;
	struct vsocket *vso1, *vso2;
	int sv[2];
	struct protosw *prp;

	if (error = useracc((void *)uap->rsv, 2 * sizeof (int), B_READ, NULL)) {
		_SAT_BSDIPC_CREATE_PAIR(0,(struct socket *)NULL,uap->domain,
			uap->protocol, 0,(struct socket *)NULL, error);
		return error;
	}

	/*
	 * useracc acquired aspacelock (ACCESS), but we release it now,
	 * therefore the copyout below could fail if a shared process
	 * makes the address unavailable.
	 */
	unuseracc((void *)uap->rsv, 2 * sizeof (int), B_READ);

	if (uap->protocol)
	        prp = pffindproto(uap->domain, uap->protocol, uap->type, &error);
	else 
	        prp = pffindtype(uap->domain, uap->type, &error);
	if (prp == NULL) 
	        goto free;

	if (uap->domain != AF_UNIX) {
	        error = EOPNOTSUPP;
	        goto free;
	}

	if (uap->protocol) {
		error = EPROTONOSUPPORT;
		goto free;
	}

	error = vsocreate(uap->domain, &vso1, uap->type, uap->protocol);
	if (error)
		goto free;
	error = vsocreate(uap->domain, &vso2, uap->type, uap->protocol);
	if (error)
		goto free1;

	error = makevsock(&fp1, &sv[0]);
	if (error) {
		goto free2;
	}

	error = makevsock(&fp2, &sv[1]);
	if (error)
		goto free3;

	VSOP_CONNECT2(vso1, vso2, error);
	if (error)
		goto free4;
	if (uap->type == SOCK_DGRAM) {
		/*
		 * Datagram socket connection is asymmetric.
		 */
		 VSOP_CONNECT2(vso2, vso1, error);
		 if (error)
			goto free4;
	}
	vfile_ready(fp1, vso1);
	vfile_ready(fp2, vso2);

	(void) copyout(sv, (void *)uap->rsv, sizeof sv);
	_SAT_BSDIPC_CREATE_PAIR(sv[0],
	    (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso1))),
	    uap->domain,uap->protocol, sv[1],
	    (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(vso2))),
	    0);
#ifdef CKPT
	fp1->vf_cpr.cu_mate = (caddr_t)fp2;
	fp2->vf_cpr.cu_mate = (caddr_t)fp1;
#endif
	return 0;

free4:
	vfile_alloc_undo(sv[1], fp2);
free3:
	vfile_alloc_undo(sv[0], fp1);
free2:
	VSOP_CLOSE(vso2, 1, 0, error1);
free1:
	VSOP_CLOSE(vso1, 1, 0, error1);
free:
	_SAT_BSDIPC_CREATE_PAIR(0,(struct socket *)NULL,uap->domain,
		uap->protocol, 0,(struct socket *)NULL,error);
	return error;
}
