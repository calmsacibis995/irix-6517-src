/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.24 $"
#include <sys/types.h> 
#include <sys/mbuf.h>
#include <sys/tcp-param.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/mac_label.h>
#include <sys/protosw.h>
#include <sys/sat.h>
#include <sys/so_dac.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <net/ksoioctl.h>
#include <sys/xlate.h>
#include <sys/capability.h>
#include <sys/var.h>
#include <sys/kmem.h>
#include <sys/sesmgr.h>
#include <sys/vsocket.h>
#include <ksys/behavior.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <sys/domain.h>

#include <sys/t6rhdb.h>
#include "sm_private.h"
#include "sesmgr_samp.h"
#include "t6satmp.h"

extern void sbdroprecord(struct sockbuf *);
extern void sesmgr_soattr_init(void);
static int sesmgr_istsix(struct socket *, struct mbuf *, struct in_addr *,
			 int *);

/*
 * Simply print a message to the effect that network session management is
 * enabled.
 */
void
sesmgr_confignote(void)
{
	cmn_err(CE_CONT, "Network Session Manager Enabled.\n");
}

void
sesmgr_init(void)
{
	sesmgr_enabled = 1;

#ifdef DEBUG
	printf("Session Manager is enabled\n");
#endif
	sesmgr_samp_init();
	sesmgr_satmp_init();
	sesmgr_soattr_init();
	t6rhdb_init();
	ip_sec_init();
}

__inline static int
sesmgr_dosamp(struct socket *so)
{
	struct protosw *pr = so->so_proto;
	struct domain *dom = pr->pr_domain;

	if (dom->dom_family != AF_INET && dom->dom_family != AF_UNIX)
		return 0;
	if (dom->dom_family == AF_INET &&
	    pr->pr_protocol != IPPROTO_UDP &&
	    pr->pr_protocol != IPPROTO_TCP)
			return 0;
	return 1;
}

#if defined(sgi) && defined(DEBUG)
extern void sbccverify(struct sockbuf *);
#endif
extern int lstatistics[];

/*
 * Implement receive operations on a socket.
 * We depend on the way that records are added to the sockbuf
 * by sbappend*.  In particular, each record (mbufs linked through m_next)
 * must begin with an address if the protocol so specifies,
 * followed by an optional mbuf containing access rights if supported
 * by the protocol, and then zero or more mbufs of data.
 * In order to avoid blocking network interrupts for the entire time here,
 * we splx() while doing the actual copy to user space.
 * Although the sockbuf is locked, new data may still be appended,
 * and thus we must maintain consistency of the sockbuf during that time.
 *
 * When pageflipping, we may do lazy syncing of the tlbs to avoid inter-CPU
 * overhead. Since other CPUs may have the old pre-flip page in their TLBs,
 * we cannot free the old pages until the TLBs are synced. This we do before
 * we exit, or before we block waiting for more data if we're holding several
 * unfreed pages.
 */

int
sesmgr_soreceive(bhv_desc_t *bdp, struct mbuf **aname, struct uio *uio,
		 int *flagsp, struct mbuf **rightsp)
{
	register struct mbuf *m;
	register int len, error = 0, offset;
	register struct socket *so = bhvtos(bdp);
	struct protosw *pr = so->so_proto;
	struct mbuf *nextrecord;
	struct mbuf *unfreed_chain = NULL;
	int unfreed_count = 0;
	int mflipped = 0;
	ssize_t orig_resid = uio->uio_resid;
	int moff;
	int flags;
	int intr = 1;		/* if 1, allow sblock to be interrupted */

	struct in_addr src;
	struct ipsec *soattrs;			/* Socket security attrs */
	t6samp_header_t samp_hdr;		/* Incoming samp header */
	size_t samp_hdr_offset = 0;		/* Byte offset into header */
	size_t samp_count = 0;			/* Number of bytes under
						   current attributes */
	int samp_changed;			/* have attributes changed? */
	int ist6;

	LSTATISTICS(VS_RECEIVE);

	if (!sesmgr_dosamp(so))
		return(soreceive(bdp, aname, uio, flagsp, rightsp));

	NETPAR(NETSCHED, NETEVENTKN, NETPID_NULL,
		 NETEVENT_SOUP, NETCNT_NULL, NETRES_SYSCALL);

	/* get security attributes */
	soattrs = so->so_sesmgr_data;
	ASSERT(soattrs != NULL);
	ASSERT(soattrs->sm_samp_cnt < (64 * 1024));

	if (rightsp)
		*rightsp = 0;
	if (aname)
		*aname = 0;
	if (flagsp) {
		flags = *flagsp;
	} else {
		flags = 0;
	}
	if (flags & MSG_DONTWAIT)
		flags &= ~MSG_WAITALL;	/* conflicting options */
	if (flags & _MSG_NOINTR)
		intr = 2;
	if (flags & MSG_OOB) {
		m = m_get(M_WAIT, MT_DATA);
		SOCKET_LOCK(so);
		error = (*pr->pr_usrreq)(so, PRU_RCVOOB,
		    m, (struct mbuf *)(NULL+(flags & MSG_PEEK)),
					 (struct mbuf *)0);
		SOCKET_UNLOCK(so);
		if (error)
			goto bad;

                do {
                        len = uio->uio_resid;
                        if (len > m->m_len)
                                len = m->m_len;
			error =
			    uiomove(mtod(m, caddr_t), (int)len, UIO_READ, uio);
			m = m_free(m);
		} while (uio->uio_resid && error == 0 && m);
bad:
		if (m)
			m_freem(m);
		NETPAR(error ? NETFLOW : 0,
		       NETDROPTKN, NETPID_NULL, NETEVENT_SOUP,
		       NETCNT_NULL, NETRES_ERROR);
		return (error);
	}

	if ((error = sblock(&so->so_rcv, NETEVENT_SOUP, so, intr)) != 0)
		return (error);
restart:
	samp_changed = 0;
#ifdef DEBUG
	sbccverify(&so->so_rcv);
#endif
	ASSERT((signed)so->so_rcv.sb_cc >= 0);
	if (so->so_rcv.sb_cc == 0) {
		if (so->so_error) {
			/* don't give error now if any data has been moved */
			if (orig_resid == uio->uio_resid) {
				error = so->so_error;
				so->so_error = 0;
			}
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE)
			goto release;
		if ((so->so_state & SS_ISCONNECTED) == 0 &&
		    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
			error = ENOTCONN;
			goto release;
		}
		if (uio->uio_resid == 0)
			goto release;
		if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT)) {
			error = EWOULDBLOCK;
			goto release;
		}
		/* don't hold a lot of memory if we're blocking */
		if (unfreed_count >= 16) {	/* ethan: arbitrary value */
			m_sync(unfreed_chain);
			unfreed_chain = NULL;
			unfreed_count = 0;
		}
		NETPAR(NETSCHED, NETSLEEPTKN, (char)&so->so_rcv,
			 NETEVENT_SOUP, NETCNT_NULL, NETRES_SBEMPTY);
		error = sbunlock_wait(&so->so_rcv, so, intr);
		if (error)
			goto out_error;
		NETPAR(NETSCHED, NETWAKEUPTKN, (char)&so->so_rcv,
			 NETEVENT_SOUP, NETCNT_NULL, NETRES_SBEMPTY);
		error = sblock(&so->so_rcv, NETEVENT_SOUP, so, intr);
		if (error)
			goto out_error;
		goto restart;
	}

	if (pr->pr_flags & PR_ADDR) {
		ASSERT(so->so_rcv.sb_mb != NULL);
		ASSERT(so->so_rcv.sb_mb->m_type == MT_SONAME);
		ist6 = sesmgr_istsix(so, so->so_rcv.sb_mb, &src, &error);
	} else
		ist6 = sesmgr_istsix(so, (struct mbuf *) NULL, &src, &error);
	if (!ist6) {
		if (error == 0 || error == EINVAL)
			error = -1;
		goto release;
	}

	/*
	 * For streams interface, this routine MAY be called from a 	
	 * service routine, which means NO u area, OR the wrong one.
	 */
	if (!(so->so_state & SS_WANT_CALL)) 
		KTOP_UPDATE_CURRENT_MSGRCV(1);
	m = so->so_rcv.sb_mb;
	if (m == 0)
		panic("sesmgr_soreceive 1");
	GOODMT(m->m_type);
	nextrecord = m->m_act;
	if (pr->pr_flags & PR_ADDR) {
		if (m->m_type != MT_SONAME)
			panic("sesmgr_soreceive 1a");
		if (flags & MSG_PEEK) {
			if (aname)
				*aname = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			ASSERT((signed)so->so_rcv.sb_cc >= 0);
			if (aname) {
				*aname = m;
				m = m->m_next;
				(*aname)->m_next = 0;
				so->so_rcv.sb_mb = m;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
			if (m)
				m->m_act = nextrecord;
		}
	}
	if (m && m->m_type == MT_RIGHTS) {
		if ((pr->pr_flags & PR_RIGHTS) == 0)
			panic("sesmgr_soreceive 2");
		if (flags & MSG_PEEK) {
			if (rightsp)
				*rightsp = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			ASSERT((signed)so->so_rcv.sb_cc >= 0);
			if (rightsp) {
				*rightsp = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
			if (m)
				m->m_act = nextrecord;
		}
	}
	moff = 0;
	offset = 0;
	samp_count = soattrs->sm_samp_cnt;
	samp_hdr_offset = 0;
	while (m && uio->uio_resid > 0 && error == 0) {
		int do_oob;

		/*
		 * If the samp_count has expired, we must read the next
		 * samp header before transferring any more user data.
		 */
		do_oob = ((so->so_state & SS_RCVATMARK) &&
			  (so->so_options & SO_OOBINLINE));
		if (!do_oob && samp_count == 0) {
			ASSERT(samp_hdr_offset < 69);

			/*
			 * First we copy the minimum heiader size.  Once
			 * we have copied the minimum header, we know the
			 * actual header size and can copy the remainder,
			 */
			if (samp_hdr_offset < T6SAMP_MIN_HEADER)
				len = T6SAMP_MIN_HEADER - samp_hdr_offset;
			else
				len = (T6SAMP_HEADER_LEN + samp_hdr.attr_length) - samp_hdr_offset;

			/* don't run off end of mbuf */
			len = MIN(len, m->m_len - moff);
			/* transfer a piece */	
			bcopy(mtod(m,caddr_t) + moff,
			      (caddr_t) &samp_hdr + samp_hdr_offset, len);
			samp_hdr_offset += len;

			/* Have we got the complete header? */
			if (samp_hdr_offset >= T6SAMP_MIN_HEADER) {
				int samp_len;

				if (sesmgr_samp_check(&samp_hdr) == 0) {
					soattrs->sm_samp_cnt = 0;
					if (pr->pr_flags & PR_ATOMIC) {
						sbdroprecord(&so->so_rcv);
						goto restart;
					} else {
						sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
						SOCKET_LOCK(so);
						if (soabort(so))
							SOCKET_UNLOCK(so);
						return EPERM;
					}
				}

				samp_len = T6SAMP_HEADER_LEN +
					samp_hdr.attr_length;
				if (samp_hdr_offset == samp_len) {
					samp_count += (samp_hdr.samp_length -
						       samp_len);
					samp_hdr_offset = 0;

					/*
					 * samp_get_attrs can go to sleep,
					 * so we must unlock the socket here.
					 */
					sbunlock(&so->so_rcv, NETEVENT_SOUP,
						 so);
					if (error = samp_get_attrs(so, src, &samp_hdr)) {

						if (pr->pr_flags & PR_ATOMIC) {
							if (sblock(&so->so_rcv, NETEVENT_SOUP, so, intr))
								return error;
							soattrs->sm_samp_cnt = 0;
							sbdroprecord(&so->so_rcv);
							goto restart;
						} else {
							SOCKET_LOCK(so);
							soattrs->sm_samp_cnt = 0;
							if (soabort(so))
								SOCKET_UNLOCK(so);
							return error;
						}
					}
					error = sblock(&so->so_rcv,
						       NETEVENT_SOUP, so, intr);
					if (error)
						return (error);
				}
			}
		} else {
			int m_flip_dosync, maxread;
			struct iovec *iov = uio->uio_iov;
			len = MIN(uio->uio_resid, do_oob ? 1 : samp_count);

			ASSERT(uio->uio_iovcnt >= 1);
			ASSERT(m->m_type == MT_DATA || m->m_type == MT_HEADER);

			so->so_state &= ~SS_RCVATMARK;
			if (so->so_oobmark && len > so->so_oobmark - offset)
				len = so->so_oobmark - offset;
			/*
			 * the holdcount keeps a socket from being closed
			 * by a protocol while executing the loop below
			 */
			so->so_holds++;
			SOCKET_UNLOCK(so);

			if (len > m->m_len - moff)
				len = m->m_len - moff;
			NETPAR(NETFLOW, NETFLOWTKN, NETPID_NULL,
			       NETEVENT_SOUP, (int)len, NETRES_NULL);

			/* page-flip if we can - but only if data in KUSEG. */
			/* m_flip does tlb sync only if we can flip at least
			 * three pages on this call */

			maxread = uio->uio_resid;
			if ((flags & MSG_WAITALL) == 0 && so->so_rcv.sb_cc < maxread)
				maxread = so->so_rcv.sb_cc;
			m_flip_dosync = (maxread < NBPP*3 && unfreed_chain == NULL);
			if (IS_KUSEG(iov->iov_base)
			    && !sesmgr_enabled
#if CELL_IRIX
			    && (curuthread)
#endif
			    && len == NBPP
			    && iov->iov_len >= NBPP
			    && 0 == ((__psint_t)iov->iov_base & (NBPP-1))
			    && moff == 0
			    && !(flags & MSG_PEEK)
			    && m_flip(m, iov->iov_base, m_flip_dosync)) {
				if (!m_flip_dosync)
					mflipped = 1;
				uio->uio_resid -= len;
				uio->uio_offset += len;
				iov->iov_base = (char *)iov->iov_base + len;
				if (0 == (iov->iov_len -= len)) {
					uio->uio_iov = iov+1;
					if (--uio->uio_iovcnt == 0) {
						ASSERT(uio->uio_resid == 0);
					}
				}
			} else {
#ifdef DEBUG
			    if (len == 0 ) {
				printf("Zero length mbuf in soreceive()\n");
			    } else
#endif

				error = uiomove(mtod(m, caddr_t) + moff,
						len, UIO_READ, uio);
			}
			if (samp_count)
				samp_count -= len;
			if (samp_count == 0)
				samp_changed = 1;

			/* m_len must not shrink */
			ASSERT(len <= m->m_len - moff);

			SOCKET_LOCK(so);
			so->so_holds--;
		}

		if (len == m->m_len - moff) {
			if (flags & MSG_PEEK) {
				m = m->m_next;
				moff = 0;
			} else {
				nextrecord = m->m_act;
				sbfree(&so->so_rcv, m);
				ASSERT((signed)so->so_rcv.sb_cc >= 0);
				if (mflipped) {
					so->so_rcv.sb_mb = m->m_next;
					m->m_next = unfreed_chain;
					unfreed_chain = m;
					unfreed_count++;
					mflipped = 0;
				} else {
					MFREE(m, so->so_rcv.sb_mb);
				}
				m = so->so_rcv.sb_mb;
				if (m)
					m->m_act = nextrecord;
			}
		} else {
			if (flags & MSG_PEEK)
				moff += len;
			else {
				m->m_off += len;
				m->m_len -= len;
				so->so_rcv.sb_cc -= len;
				ASSERT((signed)so->so_rcv.sb_cc >= 0);
			}
		}
		ASSERT(mflipped == 0);
		if (so->so_oobmark) {
			if ((flags & MSG_PEEK) == 0) {
				so->so_oobmark -= len;
				if (so->so_oobmark == 0) {
					so->so_state |= SS_RCVATMARK;
					break;
				}
			} else {
				offset += len;
			}
		}

		/*
		 * If we've got part of a header, and have run out of mbufs,
		 * we must wait for the rest of the header to arrive
		 */
		if (samp_count == 0 && samp_hdr_offset > 0 && m == 0) {
			error = sbunlock_wait(&so->so_rcv, so, intr);
			if (error)
				return (error);
			error = sblock(&so->so_rcv, NETEVENT_SOUP, so, intr);
			if (error)
				return (error);
			m = so->so_rcv.sb_mb;
		}

		if (samp_changed)
			break;
	}
	if (m && (pr->pr_flags & PR_ATOMIC) && flagsp) {
		*flagsp = MSG_TRUNC;
	}
	if ((flags & MSG_PEEK) == 0) {
		int zwindow;

		soattrs->sm_samp_cnt = samp_count;
		if (m == 0)
			so->so_rcv.sb_mb = nextrecord;
		else if (pr->pr_flags & PR_ATOMIC)
			(void) sbdroprecord(&so->so_rcv);
		if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
			(*pr->pr_usrreq)(so, PRU_RCVD, (struct mbuf *)0,
			    (struct mbuf *)0, (struct mbuf *)0);
		zwindow = (!(pr->pr_flags & PR_ATOMIC) && error == 0 && uio->uio_resid > 0 && orig_resid == uio->uio_resid);
		if (error == 0 && rightsp && *rightsp &&
		    pr->pr_domain->dom_externalize) {
			error = (*pr->pr_domain->dom_externalize)(*rightsp);
			orig_resid = uio->uio_resid;	/* keep this error */
		}

		/* this only happens if an incoming samp header
		   has a zero-length data window. */
		if (zwindow)
			goto restart;
	}

	if ((flags & MSG_WAITALL) && uio->uio_resid > 0 && error == 0)
		goto restart;
release:
#ifdef DEBUG
	sbccverify(&so->so_rcv);
	ASSERT((signed)so->so_rcv.sb_cc >= 0);
	ASSERT(soattrs->sm_samp_cnt < (64*1024));
#endif
	ASSERT(SOCKET_ISLOCKED(so));
	sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
	NETPAR(error ? NETFLOW : 0,
	       NETDROPTKN, NETPID_NULL,
	       NETEVENT_SOUP, NETCNT_NULL, NETRES_ERROR);
out_error:
	if (unfreed_chain)
		m_sync(unfreed_chain);

	if (error == -1)
		return (soreceive(bdp, aname, uio, flagsp, rightsp));

	/* if data has been moved, return it, give the error next time */
	if (orig_resid > uio->uio_resid)
		return 0;
	else
		return (error);
}

/*
 *  Old socket attribute routines.
 */

struct mac_label *
sesmgr_get_label(struct socket * so) 
{
	struct ipsec *sd = so->so_sesmgr_data;
	ASSERT(sd != NULL);
	return sd->sm_ip_flags ? sd->sm_ip_lbl : sd->sm_label;
}

void
sesmgr_set_label(struct socket * so, struct mac_label * label) 
{
	struct ipsec *sd = so->so_sesmgr_data;
	ASSERT(sd != NULL);
	sd->sm_label = label;
}

struct mac_label *
sesmgr_get_sendlabel(struct socket * so) 
{
	struct ipsec *sd = so->so_sesmgr_data;
	ASSERT(sd != NULL);
	return sd->sm_ip_flags ? sd->sm_ip_lbl : sd->sm_sendlabel;
}

void
sesmgr_set_sendlabel(struct socket * so, struct mac_label * label) 
{
	struct ipsec *sd = so->so_sesmgr_data;
	ASSERT(sd != NULL);
	sd->sm_sendlabel = label;
}

struct soacl *
sesmgr_get_soacl(struct socket * so)
{
	struct ipsec *sd = so->so_sesmgr_data;
	ASSERT(sd != NULL);
	return sd->sm_soacl;
}

void
sesmgr_set_soacl(struct socket * so, struct soacl * soacl)
{
	struct ipsec *sd = so->so_sesmgr_data;
	ASSERT(sd != NULL);
	sd->sm_soacl = soacl;
}

uid_t
sesmgr_get_uid(struct socket * so)
{
	struct ipsec *sd = so->so_sesmgr_data;
	ASSERT(sd != NULL);
	return sd->sm_uid;
}

uid_t
sesmgr_set_uid(struct socket * so, uid_t uid)
{
	struct ipsec *sd = so->so_sesmgr_data;
	ASSERT(sd != NULL);
	return sd->sm_uid = uid;
}

uid_t
sesmgr_get_rcvuid(struct socket * so)
{
	struct ipsec *sd = so->so_sesmgr_data;
	ASSERT(sd != NULL);
	return sd->sm_ip_flags ? sd->sm_ip_uid : sd->sm_rcvuid;
}

uid_t
sesmgr_set_rcvuid(struct socket * so, uid_t uid)
{
	struct ipsec *sd = so->so_sesmgr_data;
	ASSERT(sd != NULL);
	return sd->sm_rcvuid = uid;
}

/*
 *  Compatibility functions.
 */

int
sesmgr_getsock(int fd, struct socket **sop, struct vsocket **vso)
{
	int error;
	struct vsocket *vsp = NULL;

	if (vso == NULL)
		vso = &vsp;
	error = getvsock(fd, vso);
	if (!error)
		*sop = (struct socket *)(BHV_PDATA(VSOCK_TO_FIRST_BHV(*vso)));
	return error;
}

/*ARGSUSED*/
int
sesmgr_add_options(struct ifnet *ifp, struct mbuf *m, struct ipsec *ipsec,
		   struct sockaddr_in *addr, int *error)
{
	return 0;
}

static int
sesmgr_istcp(struct socket *so)
{
	int family = so->so_proto->pr_domain->dom_family;

	if (family != AF_INET && family != AF_UNIX)
		return (0);

	if (family == AF_INET && so->so_proto->pr_protocol != IPPROTO_TCP)
		return (0);

	return (1);
}

/*
 * Look in the control mbuf provide and return true if
 * it contains a multicast destination address
 */
static int
sesmgr_ismulticast(struct mbuf *mctl)
{
	struct cmsghdr *cp;
	struct in_addr dst;

	if (mctl == NULL || mctl->m_type != MT_RIGHTS)
		return (0);

	cp = (struct cmsghdr *) mtod(mctl, struct cmsghdr *);
	if (cp->cmsg_level == IPPROTO_IP && cp->cmsg_type == IP_RECVDSTADDR) {
		bcopy(CMSG_DATA(cp), (caddr_t) &dst, sizeof(struct in_addr));
		return (IN_MULTICAST(ntohl(dst.s_addr)));
	}
	return (0);
}

static int
sesmgr_istsix(struct socket *so, struct mbuf *nam, struct in_addr *src,
	      int *error)
{
	struct domain *dom = so->so_proto->pr_domain;
	struct in_addr inaddr;
	t6rhdb_kern_buf_t hsec;

	*error = 0;

	if (nam != NULL) {
		struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);

		inaddr.s_addr = sin->sin_addr.s_addr;
	} else {
		struct inpcb *inp = sotoinpcb(so);

		/* Determine source host */ 
		if (inp == NULL) {
			*error = ENOTCONN;
			return (0);
		}
		inaddr.s_addr = inp->inp_faddr.s_addr;
	}

	/* Don't do tsix attributes on multicast traffic */
	if (nam && sesmgr_ismulticast(nam->m_next))
		return (0);

	/* select address according to domain */
	if (dom->dom_family == AF_INET)
		src->s_addr = inaddr.s_addr;
	else
		src->s_addr = INADDR_LOOPBACK;

	/* Valid Address */
	if (src->s_addr == INADDR_ANY) {
		*error = EINVAL;
		return (0);
	}

	/* If the src address of the incoming packet is not a samp
	 * host, let the normal soreceive process it.
	 */
	if (!t6findhost(src, 0, &hsec)) {
		*error = ENETUNREACH;
		return (0);
	}

	if (hsec.hp_smm_type != T6RHDB_SMM_TSIX_1_1)
		return (0);

	return (1);
}

int
sesmgr_samp_kudp(struct socket *so, struct mbuf **m, struct mbuf *nam)
{
	t6samp_header_t *hdr;
	int samp_len, error;
	struct in_addr src;

	if (!sesmgr_istsix(so, nam, &src, &error)) {
		return(0);
	}

	if ((*m)->m_len < T6SAMP_MIN_HEADER) {
		*m = m_pullup(*m, T6SAMP_MIN_HEADER);
		if (*m == NULL)
			return(1);
	}

	hdr = mtod(*m, t6samp_header_t *);
	if (sesmgr_samp_check(hdr) == 0) {
		m_freem(*m);
		return(1);
	}

	samp_len = T6SAMP_HEADER_LEN + hdr->attr_length;
	if ((*m)->m_len < samp_len) {
		*m = m_pullup(*m, samp_len);
		if (*m == NULL)
			return(1);
	}

	hdr = mtod(*m, t6samp_header_t *);
	if (samp_get_attrs(so, src, hdr)) {
		m_freem(*m);
		return(1);
	}

	(*m)->m_off += samp_len;
	(*m)->m_len -= samp_len;
	if ((*m)->m_len == 0)
		*m = m_free(*m);

	return(0);
}

/*
 * This routine is called at the top of connect to save the initial set
 * of attributes for later transmission when the connection is finally
 * established. This function must be called with the socket locked.
 */
int
sesmgr_samp_init_data(struct socket *so, struct mbuf *nam)
{
	int error = 0;
	struct ipsec *sc = so->so_sesmgr_data;
	soattr_t *sa;
	/*struct domain *dom = so->so_proto->pr_domain; */ /* XXX */
	struct mbuf *ma;
	struct in_addr src;

	/* The socket must be locked */
	ASSERT(SOCKET_ISLOCKED(so));

	/* TCP only; TSIX hosts only */
	if (!sesmgr_istcp(so) || !sesmgr_istsix(so, nam, &src, &error))
		goto out;

	/* Get security attributes */
	ASSERT(sc != NULL);
	ASSERT(sc->sm_samp_cnt == 0);
	sa = &sc->sm_snd;

	/* Create initial set of attributes */
	sa->sa_mask = T6M_NO_ATTRS;
	ma = samp_create_header(so, src, 0, 0);
	if (ma == NULL) {
		error = ECONNREFUSED;
		goto out;
	}
	if (sc->sm_conn_data != NULL)
		m_freem(sc->sm_conn_data);
	sc->sm_conn_data = ma;

out:
	return error;
}

/*
 * This routine is called at the bottom of soisconnected(), to
 * send the initial set of attributes now that the connection
 * is established. This routine must be called with the socket
 * locked.
 */
void
sesmgr_samp_send_data(struct socket *so)
{
	struct ipsec *sc = so->so_sesmgr_data;

	/* The socket must be locked */
	ASSERT(SOCKET_ISLOCKED(so));

	if (so->so_proto->pr_domain->dom_family == AF_UNIX)
		return;

	if (sc != NULL && sc->sm_conn_data != NULL) {
#ifdef DEBUG
		int error;
		error =
#else
		(void)
#endif
			(*so->so_proto->pr_usrreq)(so, PRU_SEND,
						   sc->sm_conn_data,
						   (struct mbuf *) NULL,
						   (struct mbuf *) NULL);
#ifdef DEBUG
		if (error)
			cmn_err(CE_DEBUG, "socket %0x: error %d\n", so, error);
#endif
		sc->sm_conn_data = NULL;
	}

	if (sc != NULL && sc->sm_conn_data != NULL) {
		m_freem(sc->sm_conn_data);
		sc->sm_conn_data = NULL;
	}
}

void
sesmgr_samp_sbcopy(struct socket *so, struct socket *so2)
{
	struct ipsec *sc = so->so_sesmgr_data;
	if (sc == NULL || sc->sm_conn_data == NULL)
		return;
	sbappend(&so2->so_rcv, sc->sm_conn_data);
	sc->sm_conn_data = NULL;
	sorwakeup(so2, NETEVENT_SOUP);
}

/*
 * This routine is invoked before returning from an accept
 * system call to exchange the initial set of attributes
 * for the connection.  We assume that the initial set of
 * attributes fits in the first mbuf.
 */
int
sesmgr_samp_accept(struct socket *so)
{
	int error = 0;
	struct mbuf *m, *nextrecord;
	t6samp_header_t samp_hdr;		/* Incoming samp header */
	struct ipsec *soattrs;		/* Socket security attrs */
	soattr_t *sa;
	struct in_addr src;
	int hdr_len;
	int ulen;
	int wait_len;

	/* TCP only; TSIX hosts only */
	if (!sesmgr_istcp(so) ||
	    !sesmgr_istsix(so, (struct mbuf *) NULL, &src, &error))
		goto out;

	/* Get security attributes */
	soattrs = so->so_sesmgr_data;
	ASSERT(soattrs != NULL);
	ASSERT(soattrs->sm_samp_cnt == 0);
	sa = &soattrs->sm_rcv;

	/* Wait for initial attributes */
        if ((error = sblock(&so->so_rcv, NETEVENT_SOUP, so, 1)) != 0)
                goto out;
	wait_len = T6SAMP_MIN_HEADER;
restart:
	ASSERT((signed)so->so_rcv.sb_cc >= 0);
	if (so->so_rcv.sb_cc < wait_len) {
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE)
			goto release;
		if ((so->so_state & SS_ISCONNECTED) == 0 &&
		    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
			error = ENOTCONN;
			goto release;
		}
		error = sbunlock_wait(&so->so_rcv, so, 1);
		if (error)
			return (error);
		error = sblock(&so->so_rcv, NETEVENT_SOUP, so, 1);
		if (error)
			return (error);
		goto restart;
	}

	m = so->so_rcv.sb_mb;
	if (m == 0)
		panic("sesmgr_samp_accept 1");
	GOODMT(m->m_type);
	nextrecord = m->m_act;

	if (m->m_len < T6SAMP_MIN_HEADER)  {
		m = so->so_rcv.sb_mb = m_pullup(m, T6SAMP_MIN_HEADER);
		if (m == 0)
			panic("sesmgr_samp_accept 2");
	}
	bcopy(mtod(m, caddr_t), (caddr_t) &samp_hdr, sizeof (samp_hdr));

	hdr_len = T6SAMP_HEADER_LEN + samp_hdr.attr_length;
	ulen = samp_hdr.samp_length - hdr_len;

	if (hdr_len > so->so_rcv.sb_cc) {
		wait_len = hdr_len;
		goto restart;
	}
	if (hdr_len > m->m_len) {
		m = so->so_rcv.sb_mb = m_pullup(m, hdr_len);
		if (m == 0)
			panic("sesmgr_samp_accept 3");
		bcopy(mtod(m, caddr_t), (caddr_t) &samp_hdr,
		      sizeof (samp_hdr));
	}

	sa->sa_mask = T6M_NO_ATTRS;
        sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
	if (error = samp_get_attrs(so, src, &samp_hdr)) {
		return (error);
	}
	error = sblock(&so->so_rcv, NETEVENT_SOUP, so, 1);
	if (error) {
		return (error);
	}

	/* Update samp count */
	if (ulen > 0)
		soattrs->sm_samp_cnt += ulen;

	/* Update mbuf length */
	if (hdr_len == m->m_len) {
		nextrecord = m->m_act;
		sbfree(&so->so_rcv, m);
		ASSERT((signed)so->so_rcv.sb_cc >= 0);
		MFREE(m, so->so_rcv.sb_mb);
		m = so->so_rcv.sb_mb;
		if (m)
			m->m_act = nextrecord;
	} else {
		m->m_off += hdr_len;
		m->m_len -= hdr_len;
		so->so_rcv.sb_cc -= hdr_len;
		ASSERT((signed)so->so_rcv.sb_cc >= 0);
	}

release:
        ASSERT(SOCKET_ISLOCKED(so));
        sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
out:
	return (error);
}

/*
 * This routine is invoked before returning from an accept
 * system call to exchange the initial set of attributes
 * for the connection.  We assume that the intial set of
 * attributes fits in the first mbuf.
 */
int
sesmgr_samp_nread(struct socket *so, int *error, int peek)
{
	register struct mbuf *m;
	struct mbuf *nextrecord;
	t6samp_header_t samp_hdr;		/* Incoming samp header */
	struct ipsec *soattrs;			/* Socket security attrs */
	struct in_addr src;
	int hdr_len;
	int ulen;
	int wait_len;
	int count;

	*error = 0;
	count = so->so_rcv.sb_cc;

	/* TCP only */
	if (!sesmgr_istcp(so))
		return count;

	/* TSIX hosts only */
	if (!sesmgr_istsix(so, (struct mbuf *) NULL, &src, error))
		return (*error != 0 ? 0 : count);

	/* Get security attributes */
	soattrs = so->so_sesmgr_data;
	ASSERT(soattrs != NULL);

	/* Just return the current user data */
	if (soattrs->sm_samp_cnt > 0)
		return soattrs->sm_samp_cnt;

	/* No user data, and we don't have a complete samp header */
	wait_len = T6SAMP_MIN_HEADER;
	if (!peek && so->so_rcv.sb_cc < wait_len)
		return 0;

	/*
	 * From this point on, we have to release the lock on
	 * exit.
	 */
	count = 0;
        if ((*error = sblock(&so->so_rcv, NETEVENT_SOUP, so, 1)) != 0)
                goto out;
restart:
	ASSERT((signed)so->so_rcv.sb_cc >= 0);
	if (so->so_rcv.sb_cc < wait_len) {
		if (so->so_error) {
			*error = so->so_error;
			so->so_error = 0;
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE)
			goto release;
		if ((so->so_state & SS_ISCONNECTED) == 0 &&
		    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
			*error = ENOTCONN;
			goto release;
		}
		if (so->so_state & SS_NBIO) {
			*error = EWOULDBLOCK;
			goto release;
		}
		*error = sbunlock_wait(&so->so_rcv, so, 1);
		if (*error)
			return (0);
		*error = sblock(&so->so_rcv, NETEVENT_SOUP, so, 1);
		if (*error)
			return (0);
		goto restart;
	}

	m = so->so_rcv.sb_mb;
	if (m == 0)
		panic("sesmgr_samp_nread 1");
	GOODMT(m->m_type);
	nextrecord = m->m_act;

	if (m->m_len < T6SAMP_MIN_HEADER)  {
		m = so->so_rcv.sb_mb = m_pullup(m, T6SAMP_MIN_HEADER);
		if (m == 0)
			panic("sesmgr_samp_nread 2");
	}
	bcopy(mtod(m, caddr_t), (caddr_t) &samp_hdr, sizeof (samp_hdr));

	hdr_len = T6SAMP_HEADER_LEN + samp_hdr.attr_length;
	ulen = samp_hdr.samp_length - hdr_len;

	if (hdr_len > so->so_rcv.sb_cc) {
		wait_len = hdr_len;
		goto restart;
	}
	if (hdr_len > m->m_len) {
		m = so->so_rcv.sb_mb = m_pullup(m, hdr_len);
		if (m == 0)
			panic("sesmgr_samp_nread 3");
		bcopy(mtod(m, caddr_t), (caddr_t) &samp_hdr,
		      sizeof (samp_hdr));
	}

	/* update samp attributes */
        sbunlock(&so->so_rcv, NETEVENT_SOUP, so);
	if (*error = samp_get_attrs(so, src, &samp_hdr))
		return (0);
	*error = sblock(&so->so_rcv, NETEVENT_SOUP, so, 1);
	if (*error)
		return (0);

	/* Update samp count */
	if (ulen > 0)
		soattrs->sm_samp_cnt += ulen;

	/* Update mbuf length */
	if (hdr_len == m->m_len) {
		nextrecord = m->m_act;
		sbfree(&so->so_rcv, m);
		ASSERT((signed)so->so_rcv.sb_cc >= 0);
		MFREE(m, so->so_rcv.sb_mb);
		m = so->so_rcv.sb_mb;
		if (m)
			m->m_act = nextrecord;
	} else {
		m->m_off += hdr_len;
		m->m_len -= hdr_len;
		so->so_rcv.sb_cc -= hdr_len;
		ASSERT((signed)so->so_rcv.sb_cc >= 0);
	}

	count = soattrs->sm_samp_cnt;

release:
        ASSERT(SOCKET_ISLOCKED(so));
        sbunlock(&so->so_rcv, NETEVENT_SOUP, so);

out:
	return (count);
}

int
sesmgr_nfs_vsetlabel(struct vnode *vp, struct ipsec *ip)
{
	extern mac_t mac_high_low_lp;
	return (_MAC_VSETLABEL(vp, ip ? ip->sm_ip_lbl : mac_high_low_lp));
}

/*
 * Prepend the ip_security attributes to the front of the
 * mbuf chain for NFS.  We don't use m_act because that field
 * gets cleared by enqueue macro used by NFS.
 */
struct mbuf *
sesmgr_nfs_set_ipsec(struct ipsec *ipsec, struct mbuf **m)
{
	struct mbuf *tmp;

	tmp = m_vget(M_DONTWAIT, sizeof(struct ipsec), MT_SESMGR);
	if (tmp == NULL) {
		sesmgr_soattr_free(ipsec);
		return (NULL);
	}
	bcopy((void *) ipsec, mtod(tmp, void *), sizeof(*ipsec));
	sesmgr_soattr_free(ipsec);
	tmp->m_len += sizeof(*ipsec);
	tmp->m_next = *m;
	*m = tmp;
	return (*m);
}
