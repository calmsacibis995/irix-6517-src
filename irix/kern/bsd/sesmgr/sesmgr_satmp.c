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

/*
 *	SGI SAMP<->SATMPD interface
 */

#ident	"$Revision: 1.7 $"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socketvar.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>

#include <sys/sat.h>
#include <sys/acl.h>
#include <sys/mac.h>
#include <sys/mac_label.h>

#include <sys/t6attrs.h>
#include <sys/t6satmp.h>
#include <sys/sesmgr.h>
#include <sys/t6api_private.h>
#include <sys/atomic_ops.h>
#include "sesmgr_samp.h"
#include "sm_private.h"

#include "net/if.h"
#include "net/route.h"

#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

static lock_t satmpd_solock;
static struct socket *satmpd_so;

struct list {
	lock_t lock;
	init_request_queue *head;
	init_request_queue *tail;
};

static struct list irq_list;
static zone_t *irq_list_zone;

static u_short samp_to_satmp[] = {
	SATMP_ATTR_SEN_LABEL,
	SATMP_ATTR_NATIONAL_CAVEATS,
	SATMP_ATTR_INTEGRITY_LABEL,
	~0,				/* T6SAMP_SID */
	SATMP_ATTR_CLEARANCE,
	~0,				/* T6SAMP_UNUSED */
	SATMP_ATTR_INFO_LABEL,
	SATMP_ATTR_PRIVILEGES,
	SATMP_ATTR_AUDIT_ID,		/* T6SAMP_LUID */
	~0,				/* T6SAMP_PID */
	SATMP_ATTR_IDS,
	SATMP_ATTR_AUDIT_INFO,
	~0,				/* T6SAMP_PROC_ATTR */
};

static u_short satmp_to_samp[] = {
	T6SAMP_SL,
	T6SAMP_NAT_CAVEAT,
	T6SAMP_INTEG_LABEL,
	T6SAMP_IL,
	T6SAMP_PRIVS,
	T6SAMP_LUID,			/* SATMP_ATTR_AUDIT_ID */
	T6SAMP_IDS,
	T6SAMP_CLEARANCE,
	T6SAMP_AUDIT_INFO,
	~0,				/* SATMP_ATTR_UNASSIGNED_9 */
	~0,				/* SATMP_ATTR_ACL */
	~0,				/* SATMP_ATTR_FILE_PRIVILEGES */
};

/* linked list routines */
static void
list_init (struct list *l)
{
	spinlock_init (&l->lock, "list lock");
	l->head = NULL;
	l->tail = NULL;
}

static void *
list_lookup (struct list *l, int (*m) (init_request_queue *, const void *),
	     const void *key)
{
	int s;
	init_request_queue *q, *rtn = NULL;

	ASSERT (l != NULL);
	ASSERT (m != NULL);
	ASSERT (key != NULL);

	s = mutex_spinlock (&l->lock);
	for (q = l->head; q != NULL; q = q->irq_next)
	{
		if ((*m) (q, key))
		{
			rtn = q;
			break;
		}
	}
	mutex_spinunlock (&l->lock, s);
	return (rtn);
}

static void
list_add (struct list *l, init_request_queue *entry)
{
	int s;

	ASSERT (l != NULL);
	ASSERT (entry != NULL);

	s = mutex_spinlock (&l->lock);
	if (l->tail == NULL)
		l->head = entry;
	else
		l->tail->irq_next = entry;
	l->tail = entry;
	mutex_spinunlock (&l->lock, s);
}

static void *
list_refcnt (struct list *l, int (*r) (void *), init_request_queue *entry)
{
	int s;
	init_request_queue *q, *p = NULL, *rtn = NULL;

	ASSERT (l != NULL);
	ASSERT (r != NULL);
	ASSERT (entry != NULL);

	s = mutex_spinlock (&l->lock);
	for (q = l->head; q != NULL; p = q, q = q->irq_next)
	{
		if (q == entry) {
			if ((*r) (q)) {
				rtn = q;

				/* unlink entry */
				if (p == NULL) {
					l->head = l->head->irq_next;
					if (l->head == NULL)
						l->tail = NULL;
				} else {
					p->irq_next = q->irq_next;
					if (l->tail == q)
						l->tail = p;
				}
			}
			break;
		}
	}
	mutex_spinunlock (&l->lock, s);
	return (rtn);
}

int
sesmgr_satmpd_started (void)
{
	if (satmpd_so == NULL)
		return 0;
	return 1;
}

static size_t
header_size (void)
{
	satmp_header *hdr;

	return (2 + sizeof (hdr->message_length) + sizeof (hdr->message_esi));
}

void
sesmgr_satmp_soclear (struct socket *so)
{
	int spin;

	spin = mutex_spinlock (&satmpd_solock);
	if (so == satmpd_so)
	{
		satmpd_so = NULL;
		(void) samp_update_host (INADDR_LOOPBACK, 0);
	}
	mutex_spinunlock (&satmpd_solock, spin);
}

/* ARGSUSED */
static int
satmp_init_cmd (int fd, u_int generation, rval_t *rvp)
{
	int spin, error = 0;
	struct socket *so;

	/* check for appropriate privilege */
	if (!_CAP_ABLE (CAP_NETWORK_MGT))
	{
		error = EPERM;
		goto out;
	}

	/* get the socket associated with this file descriptor */
	if (error = sesmgr_getsock (fd, &so, NULL))
		goto out;

	spin = mutex_spinlock (&satmpd_solock);
	if (satmpd_so != NULL)
	{
		mutex_spinunlock (&satmpd_solock, spin);
		error = EADDRINUSE;
		goto out;
	}
	satmpd_so = so;
	mutex_spinunlock (&satmpd_solock, spin);

	error = samp_update_host (INADDR_LOOPBACK, generation);
out:
	/* create audit record here */
	return (error);
}

/* ARGSUSED */
static int
satmp_done_cmd (rval_t *rvp)
{
	int spin, error = 0;

	/* check for appropriate privilege */
	if (!_CAP_ABLE (CAP_NETWORK_MGT))
	{
		error = EPERM;
		goto out;
	}

	/* de-register daemon */
	spin = mutex_spinlock (&satmpd_solock);
	satmpd_so = NULL;
	mutex_spinunlock (&satmpd_solock, spin);

	/* destroy cache here */
out:
	/* create audit record here */
	return (error);
}

static void
init_get_attr_reply (get_attr_reply *rep)
{
	rep->attr_spec = NULL;
	rep->attr_spec_count = 0;
}

static void
destroy_get_attr_reply (get_attr_reply *rep)
{
	u_short i;

	if (rep->attr_spec == NULL)
		return;

	for (i = 0; i < rep->attr_spec_count; i++)
		if (rep->attr_spec[i].nr != NULL)
			kmem_free (rep->attr_spec[i].nr,
				   rep->attr_spec[i].nr_length);
	kmem_free (rep->attr_spec,
		   rep->attr_spec_count * sizeof (*rep->attr_spec));
}

static int
t6ids_invalid(t6ids_t *ids)
{
	__uint32_t i;
	t6groups_t *grp;

	/* check primary uid & gid */
	if (ids->uid < 0 || ids->uid > MAXUID)
		return (EINVAL);
	if (ids->gid < 0 || ids->gid > MAXUID)
		return (EINVAL);

	/* check group list */
	grp = &ids->groups;
	if (grp->ngroups > ngroups_max)
		return(EINVAL);
	for (i = 0; i < grp->ngroups; i++)
		if (grp->groups[i] < 0 || grp->groups[i] > MAXUID)
			return(EINVAL);

	return(0);
}

static int
lrep_verify (void *lrep, u_short attribute)
{
	if (lrep == NULL)
		return (0);
	switch (attribute)
	{
		case T6SAMP_SL:
		case T6SAMP_CLEARANCE:
			return (msen_valid ((msen_t) lrep) == 0);
		case T6SAMP_INTEG_LABEL:
			return (mint_valid ((mint_t) lrep) == 0);
		case T6SAMP_LUID:
		{
			uid_t *uidp = (uid_t *) lrep;
			return (*uidp < 0 || *uidp > MAXUID);
		}
		case T6SAMP_IDS:
			return (t6ids_invalid ((t6ids_t *) lrep));
		case T6SAMP_NAT_CAVEAT:
		case T6SAMP_PRIVS:
		case T6SAMP_AUDIT_INFO:
			break;
		case T6SAMP_IL:
		default:
			return (EINVAL);
	}
	return (0);
}

/* WARNING: rep must be initialized before calling this function */
static int
get_attr_reply_buffer (const char *buf, size_t buflen, get_attr_reply *rep)
{
	const char *bufptr = buf;
	u_short t16;
	u_int t32;

	/* adjust bufptr by header size */
	bufptr += header_size ();

	/* hostid */
	bcopy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);
	rep->hostid = ntohl (t32);

	/* generation */
	bcopy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);
	rep->generation = ntohl (t32);

	/* copy reply specs */
	while (bufptr - buf < buflen)
	{
		get_attr_reply_attr_spec *garas;

		rep->attr_spec = kmem_realloc (rep->attr_spec, ++rep->attr_spec_count * sizeof (*rep->attr_spec), KM_SLEEP);
		if (rep->attr_spec == NULL)
			return (ENOMEM);

		garas = &rep->attr_spec[rep->attr_spec_count - 1];

		/* attribute type */
		bcopy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		garas->attribute = satmp_to_samp[ntohs (t16)];

		/* length of local representation */
		bcopy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		garas->nr_length = ntohs (t16);

		/* token */
		bcopy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
		garas->token = ntohs (t32);

		/* status */
		garas->status = *bufptr++;

		/* local representation itself */
		if (garas->nr_length != 0)
		{
			garas->nr = kmem_alloc (garas->nr_length, KM_SLEEP);
			if (garas->nr == NULL)
				return (ENOMEM);
			bcopy (bufptr, garas->nr, garas->nr_length);
		}
		else
			garas->nr = NULL;
		bufptr += garas->nr_length;

		/* verify local representation */
		if (lrep_verify (garas->nr, garas->attribute))
			return (EINVAL);
	}
	return (0);
}

/* ARGSUSED */
static int
sesmgr_get_attr_reply (int fd, void *buf, size_t size, rval_t *rvp)
{
	int error = 0;
	struct socket *so;
	char *kbuf = NULL;
	get_attr_reply rep;

	/* initialize stuff */
	init_get_attr_reply (&rep);

	/* get the socket associated with this file descriptor */
	if (error = sesmgr_getsock (fd, &so, NULL))
		goto out;

	/* only satmpd can call us */
	if (so != satmpd_so)
	{
		error = EPERM;
		goto out;
	}

	/* copy user buffer to kernel space */
	kbuf = kmem_alloc (size, KM_SLEEP);
	if (copyin (buf, kbuf, size))
	{
		error = EFAULT;
		goto out;
	}

	/* parse reply buffer */
	if (error = get_attr_reply_buffer (kbuf, size, &rep))
		goto out;

	/* update the token entry with the attr value for this token */
	samp_update_attr (&rep);
out:
	if (kbuf != NULL)
		kmem_free (kbuf, size);
	destroy_get_attr_reply (&rep);

	/* create audit record here */
	return (error);
}

static void
init_get_lrtok_reply (get_lrtok_reply *rep)
{
	rep->token_spec = NULL;
	rep->token_spec_count = 0;
}

static void
destroy_get_lrtok_reply (get_lrtok_reply *rep)
{
	if (rep->token_spec != NULL)
		kmem_free (rep->token_spec,
			   rep->token_spec_count * sizeof (*rep->token_spec));
}

/* WARNING: rep must be initialized before calling this function */
static int
get_lrtok_reply_buffer (const char *buf, size_t buflen, get_lrtok_reply *rep)
{
	const char *bufptr = buf;
	u_short t16;
	u_int t32;

	/* adjust bufptr by header size */
	bufptr += header_size ();

	/* hostid */
	bcopy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);
	rep->hostid = ntohl (t32);

	/* generation */
	bcopy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);
	rep->generation = ntohl (t32);

	while (bufptr - buf < buflen)
	{
		get_lrtok_reply_token_spec *glrts;

		rep->token_spec = kmem_realloc (rep->token_spec, ++rep->token_spec_count * sizeof (*rep->token_spec), KM_SLEEP);
		if (rep->token_spec == NULL)
			return (ENOMEM);

		glrts = &rep->token_spec[rep->token_spec_count - 1];

		/* attribute type */
		bcopy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		glrts->attribute = satmp_to_samp[ntohs (t16)];

		/* mapping identifier */
		bcopy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
		glrts->mid = ntohl (t32);

		/* token */
		bcopy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
		glrts->token = ntohl (t32);

		/* status */
		glrts->status = *bufptr++;
	}
	return (0);
}

/* ARGSUSED */
static int
sesmgr_get_lrtok_reply (int fd, void *buf, size_t size, rval_t *rvp)
{
	int error = 0;
	struct socket *so;
	char *kbuf = NULL;
	get_lrtok_reply rep;

	/* initialize stuff */
	init_get_lrtok_reply (&rep);

	/* get the socket associated with this file descriptor */
	if (error = sesmgr_getsock (fd, &so, NULL))
		goto out;

	/* only satmpd can call us */
	if (so != satmpd_so)
	{
		error = EPERM;
		goto out;
	}

	/* copy user buffer to kernel space */
	kbuf = kmem_alloc (size, KM_SLEEP);
	if (copyin (buf, kbuf, size))
	{
		error = EFAULT;
		goto out;
	}

	/* parse reply buffer */
	if (error = get_lrtok_reply_buffer (kbuf, size, &rep))
		goto out;

	/* update the token table with the assigned token */
	samp_update_token (&rep);
out:
	if (kbuf != NULL)
		kmem_free (kbuf, size);
	destroy_get_lrtok_reply (&rep);

	/* create audit record here */
	return (error);
}

static void
sesmgr_mcat_data (struct mbuf *m, char *buf, int buflen)
{
	if (m == NULL)
		return;

	GOODMP(m);
	GOODMT(m->m_type);

	/* go to end of chain */
	while (m->m_next != 0)
		m = m->m_next;

	/*
	 * The number of bytes left in an mbuf is the maximum possible
	 * offset (MMAXOFF) minus the mbuf offset (m_off) minus the mbuf
	 * length (m_len).
	 */
	while (buflen > 0)
	{
		int minlen;
		char *mdata;

		minlen = min (buflen, MMAXOFF - m->m_off - m->m_len);
		mdata = mtod (m, char *);
		bcopy ((caddr_t) buf, (caddr_t) (mdata + m->m_len), minlen);
		buf += minlen;
		m->m_len += minlen;
		buflen -= minlen;
		if (buflen > 0) {
			m->m_next = m_get (M_WAIT, MT_DATA);
			m = m->m_next;
			if (m == NULL)
				panic ("SESMGR mbuf alloc failed");
		}
	}
}

static int
sesmgr_send_request (u_int hostid, char *buf, int buflen)
{
	struct mbuf *m;
	struct sockaddr_in src;
	int spin;

	/* Create a samp header */
	if ((m = samp_create_priv_hdr (buflen)) == NULL)
		return (ENOMEM);

	/* Append data */
	sesmgr_mcat_data (m, buf, buflen);
	bzero ((caddr_t) &src, sizeof (src));
	src.sin_family = PF_INET;
	src.sin_addr.s_addr = hostid;
	src.sin_port = 0;

	spin = mutex_spinlock (&satmpd_solock);
	if (satmpd_so == NULL)
	{
		mutex_spinunlock (&satmpd_solock, spin);
		m_freem (m);
		return (EADDRNOTAVAIL);
	}
	SOCKET_LOCK (satmpd_so);
	if (sbappendaddr (&satmpd_so->so_rcv, (struct sockaddr *) &src, m,
			  (struct mbuf *) NULL, (struct ifnet *) NULL) != 0)
	{
		sorwakeup (satmpd_so, NETEVENT_UDPUP);
	}
	SOCKET_UNLOCK (satmpd_so);
	mutex_spinunlock (&satmpd_solock, spin);

	return (0);
}

static void
make_header (u_char request, char *buf, u_short buflen, satmp_esi_t *seqno)
{
	static satmp_esi_t sesmgr_seqno = 0x8000000000000000;
	satmp_esi_t esi;
	u_short t16;

	/* message number */
	*buf++ = request;

	/* message status */
	*buf++ = 0;

	/* message length */
	t16 = htons (buflen);
	bcopy (&t16, buf, sizeof (t16));
	buf += sizeof (t16);

	/* extended session identifier */
	esi = atomicAddUint64(&sesmgr_seqno, 1);
	if (seqno != NULL)
		*seqno = esi;
	bcopy(&esi, buf, sizeof (esi));
	buf += sizeof (esi);
}

static void
sesmgr_token_list_buffer (satmp_token_list *tl, char *buf, int *buflen)
{
	char *bufptr = buf;
	u_short t16;
	u_int   t32;
	int i, j;

	/* allocate space for header */
	bufptr += header_size ();

	/* copy hostid */
	t32 = htonl (tl->hostid);
	bcopy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);

	/* copy port */
	t16 = htons (90);
	bcopy (&t16, bufptr, sizeof (t16));
	bufptr += sizeof (t16);

	/* copy kernel */
	*bufptr++ = 1;

	for (i = 0, j = 0; i < SATMP_ATTR_LAST + 1; i++)
	{
		if (!(tl->mask & (1 << i)))
			continue;

		/* copy attribute type */
		t16 = htons (samp_to_satmp[i]);
		bcopy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);

		/* copy token */
		t32 = htonl (tl->lrtok_list[j].token);
		bcopy (&t32, bufptr, sizeof (t32));
		bufptr += sizeof (t32);

		j++;
	}

	make_header (SATMP_SEND_GET_ATTR_REQUEST, buf,
		     *buflen = bufptr - buf, (satmp_esi_t *) NULL);
}

int
sesmgr_get_attr_request (satmp_token_list *tl)
{
	int error = ENXIO;

	if (satmpd_so != NULL)
	{
		char buf[256];
		int buflen;

		sesmgr_token_list_buffer (tl, buf, &buflen);
		error = sesmgr_send_request (tl->hostid, buf, buflen);
	}
	return (error);
}

static void
sesmgr_lrep_list_buffer (satmp_lrep_list *ll, char *buf, int *buflen)
{
	char *bufptr = buf;
	u_short t16;
	u_int t32;
	int i, j = 0;

	/* allocate space for header */
	bufptr += header_size ();

	/* copy hostid */
	t32 = htonl (ll->hostid);
	bcopy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);

	/* copy generation */
	t32 = htonl (ll->generation);
	bcopy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);

	for (i = 0; i < SATMP_ATTR_LAST + 1; i++)
	{
		if (!(ll->mask & (1 << i)))
			continue;

		/* copy attribute type */
		t16 = htons (samp_to_satmp[i]);
		bcopy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);

		/* copy length */
		t16 = htons (ll->lr_list[j].lr_len);
		bcopy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);

		/* copy mapping identifier */
		t32 = htonl (ll->lr_list[j].mid);
		bcopy (&t32, bufptr, sizeof (t32));
		bufptr += sizeof (t32);

		/* copy lrep */
		bcopy (ll->lr_list[j].lr, bufptr, ll->lr_list[j].lr_len);
		bufptr += ll->lr_list[j++].lr_len;
	}

	make_header (SATMP_GET_LRTOK_REQUEST, buf, *buflen = bufptr - buf,
		     (satmp_esi_t *) NULL);
}

int
sesmgr_get_lrtok_request (satmp_lrep_list *ll)
{
	int error = ENXIO;

	if (satmpd_so != NULL)
	{
		char buf[1024];
		int buflen;

		/* fill mbuf here */
		sesmgr_lrep_list_buffer (ll, buf, &buflen);
		error = sesmgr_send_request (ll->hostid, buf, buflen);
	}
	return (error);
}

static void
sesmgr_hostid_buffer (u_int hostid, char *buf, int *buflen,
		      satmp_esi_t *seqno)
{
	char *bufptr = buf;
	u_int t32;
	u_short t16;

	/* allocate space for header */
	bufptr += header_size ();

	/* copy hostid */
	t32 = htonl (hostid);
	bcopy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);

	/* copy port */
	t16 = htons (90);
	bcopy (&t16, bufptr, sizeof (t16));
	bufptr += sizeof (t16);

	/* copy kernel */
	*bufptr++ = 1;

	make_header (SATMP_SEND_INIT_REQUEST, buf, *buflen = bufptr - buf,
		     seqno);
}

static int
irq_match_hostid (init_request_queue *q, const void *key)
{
	const u_int *hostid = (u_int *) key;

	ASSERT (q != NULL);
	ASSERT (hostid != NULL);

	if (q->irq_hostid == *hostid)
	{
		(void) atomicAddUint (&q->irq_refcnt, 1);
		return (1);
	}
	return (0);
}

static int
irq_match_seqno (init_request_queue *q, const void *key)
{
	const satmp_esi_t *seqno = (satmp_esi_t *) key;

	ASSERT (q != NULL);
	ASSERT (seqno != NULL);

	if (q->irq_seqno == *seqno)
	{
		(void) atomicAddUint (&q->irq_refcnt, 1);
		return (1);
	}
	return (0);
}

static int
irq_refcnt (void *e)
{
	init_request_queue *irq = (init_request_queue *) e;

	ASSERT (irq != NULL);

	return (atomicAddUint (&irq->irq_refcnt, -1) == 0);
}

static void
irq_rele (void *e)
{
	init_request_queue *irq = (init_request_queue *) e;

	if (irq == NULL)
		return;

	sv_destroy (&irq->irq_wait);
	spinlock_destroy (&irq->irq_lock);
	kmem_zone_free (irq_list_zone, irq);
}

int
sesmgr_init_request (u_int hostid, u_int *generation)
{
	int error = ENXIO;

	ASSERT (generation != NULL);

	if (satmpd_so != NULL)
	{
		int spin;
		init_request_queue *irq;

		/* check if this entry is already there */
		irq = list_lookup (&irq_list, irq_match_hostid, &hostid);

		/* allocate new queue entry if necessary */
		if (irq == NULL)
		{
			char buf[128];
			int buflen;

			irq = kmem_zone_alloc (irq_list_zone, KM_SLEEP);
			if (irq == NULL)
				return (ENOMEM);

			/* initialize irq */
			sv_init (&irq->irq_wait, SV_DEFAULT, "irq wait");
			spinlock_init (&irq->irq_lock, "irq lock");
			irq->irq_hostid = hostid;
			irq->irq_generation = 0;
			irq->irq_refcnt = 1;
			irq->irq_flag = IRQ_FLAG_PENDING;
			irq->irq_next = NULL;

			/* create message buffer */
			sesmgr_hostid_buffer (hostid, buf, &buflen,
					      &irq->irq_seqno);

			/* acquire irq lock */
			spin = mutex_spinlock (&irq->irq_lock);

			/* add irq to the list */
			list_add (&irq_list, irq);

			/* send request to daemon */
			error = sesmgr_send_request (hostid, buf, buflen);
			if (error)
				irq->irq_flag = IRQ_FLAG_FAILED;
		} else
			spin = mutex_spinlock (&irq->irq_lock);

		/* wait for response, then reacquire lock */
		while (irq->irq_flag == IRQ_FLAG_PENDING) {
			if (sv_wait_sig (&irq->irq_wait, PZERO,
					 &irq->irq_lock, spin))
			{
				error = EINTR;
				goto release;
			}
			spin = mutex_spinlock (&irq->irq_lock);
		}

		/* check status flag */
		ASSERT (irq->irq_flag == IRQ_FLAG_OK ||
			irq->irq_flag == IRQ_FLAG_FAILED);
		error = (irq->irq_flag == IRQ_FLAG_FAILED ? EPERM : 0);
		*generation = irq->irq_generation;

		/* unlock entry and release */
		mutex_spinunlock (&irq->irq_lock, spin);
release:
		irq_rele (list_refcnt (&irq_list, irq_refcnt, irq));
	}
	return (error);
}

/* ARGSUSED */
static int
init_reply_cmd (int fd, u_int esi1, u_int esi2, int flag, u_int generation,
		rval_t *rvp)
{
	struct socket *so;
	int error;
	init_request_queue *q;
	satmp_esi_t esi = ((satmp_esi_t) esi1 << 32) |
			  ((satmp_esi_t) esi2 & 0xffffffff);

	/* get the socket associated with this file descriptor */
	if (error = sesmgr_getsock (fd, &so, NULL))
		return (error);

	/* only satmpd can call us */
	if (so != satmpd_so)
		return (EPERM);

	/* check value of flag */
	if (flag != IRQ_FLAG_OK && flag != IRQ_FLAG_FAILED)
		return (EINVAL);

	/* find entry in list */
	q = list_lookup (&irq_list, irq_match_seqno, &esi);
	if (q != NULL)
	{
		int spin = mutex_spinlock (&q->irq_lock);
		q->irq_flag = flag;
		q->irq_generation = generation;
		sv_broadcast (&q->irq_wait);
		mutex_spinunlock (&q->irq_lock, spin);
		irq_rele (list_refcnt (&irq_list, irq_refcnt, q));
		return (0);
	}
	return (EINVAL);
}

void
sesmgr_satmp_init (void)
{
	spinlock_init (&satmpd_solock, "satmpd_solock");
	list_init (&irq_list);
	irq_list_zone = kmem_zone_init (sizeof (init_request_queue),
					"irq list table");
	ASSERT (irq_list_zone != NULL);
}

int
sesmgr_satmp_syscall (struct syssesmgra *uap, rval_t *rvp)
{
	/* record command for sat */
	/* _SAT_SET_SUBSYSNUM(curprocp, uap->cmd); */ /* SAT_XXX */

	/* Dispatch to subcommand */
	switch (uap->cmd & 0xffff) {
		case T6SATMP_INIT_CMD:
			return (satmp_init_cmd ((int) uap->arg1,
						(u_int) uap->arg2,
						rvp));
		case T6SATMP_DONE_CMD:
			return (satmp_done_cmd (rvp));
		case T6SATMP_GET_ATTR_REPLY_CMD:
			return (sesmgr_get_attr_reply ((int) uap->arg1,
						       (void *) uap->arg2,
						       (size_t) uap->arg3,
						       rvp));
		case T6SATMP_GET_LRTOK_REPLY_CMD:
			return (sesmgr_get_lrtok_reply ((int) uap->arg1,
							(void *) uap->arg2,
							(size_t) uap->arg3,
							rvp));
		case T6SATMP_INIT_REPLY_CMD:
			return (init_reply_cmd ((int) uap->arg1,
						(u_int) uap->arg2,
						(u_int) uap->arg3,
						(int) uap->arg4,
						(u_int) uap->arg5,
						rvp));
		default:
			return (EINVAL);
	}
}
