/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifdef SN0

#include <sys/types.h>
#include <sys/pfdat.h>
#include <sys/partition.h>
#include <ksys/partition.h>
#include <ksys/xpc.h>

#include <sys/tcp-param.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/immu.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/protosw.h>
#include <sys/kmem.h>
#include <sys/iograph.h>
#include <sys/hwgraph.h>
#include <sys/invent.h>
#include <sys/idbgentry.h>	/* idbg_addfunc */

#include <net/if.h>
#include <net/if_types.h>
#include <net/raw.h>
#include <net/route.h>
#include <net/soioctl.h>
#include <net/netisr.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <sys/SN/SN0/bte.h>
#include <sys/SN/if_cl.h>

/* file: if_cl.c
 * -------------
 * Craylink network driver for cross partititon communication over
 * craylink.  The following functions are called by functions outside
 * this file:
 * 
 * ifcl_init:
 *	called from main
 *
 * ifcl_open:
 *	called from cdrv when the interface is opened
 *	
 * ifcl_ifioctl: 
 * 	called from in_ifinit to init interface's internet address and
 * 	routing table entry
 *
 * ifcl_ifcl_attach_xpttn:
 *	this is the registered partition activate handler
 *
 * ifcl_shutdown:
 *	this is the registered partition deactivate handler
 *
 * ifcl_xpc_async:
 *	called from xpc_set_error, xpc_process_connect or
 *	xpc_receive_xpm
 *
 * ifcl_output_odone:
 *	called from xpc_notify_continue
 *
 * ifcl_output:
 *	called from udp_output, ip_output, etc....
 * 
 * All other functions are only called from inside this file.  This
 * files uses the rings of the xpc layer to transport data and then
 * places this data in the mbuf structures before passing them up to the
 * higher level networking code (network_input).
 */

#if IFCL_DEBUG
int		ifcl_debug = 0;

#define		DCMN_ERR1(_x)	if (ifcl_debug >= 1) cmn_err _x;
#define		DCMN_ERR2(_x)	if (ifcl_debug >= 2) cmn_err _x;

static void
mbuf_dump(struct mbuf *m)
{
    while (m) {
        DCMN_ERR1((CE_NOTE, "mtod 0x%lx: len %d\n", mtod(m, __psunsigned_t), 
		   m->m_len));
        m = m->m_next;
    }
}

#else /* !IFCL_DEBUG */

#define		DCMN_ERR1(_x)
#define		DCMN_ERR2(_x)
#define		mbuf_dump(_m)

#endif /* IFCL_DEBUG */

static ifcl_t		**ifcl_dev;	/* array of one pointer to general
					 * overall ifcl structure. */

/* snarptab
 * --------
 * Arp table for all the arps we have gotten from other partitions through
 * this interface and save the address information for.  This is hashed
 * using the SNARP_HASH macro below (for possibly better lookup). 
 */
static snarptab_t	snarptab[MAX_PARTITIONS];
static mrlock_t		snarp_lock;	/* lock for snarptab */

#define	SNARP_HASH(a)	((u_int) (a) % MAX_PARTITIONS)

int	ifcl_version;
int	ifcl_devflag = D_MP;

extern int	ifcl_num_unit;	/* just 1 for now */

/* IFCL_LOG is for saving state for all sends and receives of data
 * to be dumped at a given point in the code.  The log can be turned
 * in in if_cl.h */
#if IFCL_LOG
ifcl_recv_log_t ifcl_recv_log;
ifcl_send_log_t ifcl_send_log;
#endif /* IFCL_LOG */

#define	IFCL_LOCK(lock, s)	(s) = mutex_spinlock(lock)
#define	IFCL_UNLOCK(lock, s)	mutex_spinunlock(lock, (s))

#define ROUND(x, align)	(((x) & (__psunsigned_t) (~(align - 1))) + \
				((x) & (__psunsigned_t) (align - 1) ? \
				align : 0))

#define ROUND_BTE(x)	ROUND(x, BTE_LEN_MINSIZE)

/* RAW is for snooping raw packets ??? */
#define	RAW	1


     /* FIX: thread count race!!! */
/* IFCL_THREAD_COUNT_INCREMENT/DECREMENT
 * -------------------------------------
 * Increment or decrement the nthread count for the number of threads
 * that have started an xpc_allocate, but not sent the message yet.
 * This information is needed for when another partition goes down.  
 * We need to make sure that we have let all the threads finish up
 * before destroying all of that partition's structures.
 */
#define IFCL_THREAD_COUNT_INCREMENT(_ifcl, _dst, _fail)			\
     {									\
	 int s;								\
									\
	 (_fail) = 0;							\
	 IFCL_LOCK(&((_ifcl)->cl_mutex), s);				\
	 if((_ifcl)->cl_xpttn[(_dst)].icxp_flags & IFCL_XP_DISPEND ||	\
	    !((_ifcl)->cl_xpttn[(_dst)].icxp_flags & IFCL_XP_UP)) {	\
	     (_fail) = 1;						\
	 } else {							\
	     (_ifcl)->cl_xpttn[(_dst)].icxp_nthreads++;			\
	 }								\
	 IFCL_UNLOCK(&((_ifcl)->cl_mutex), s);				\
     }

#define IFCL_THREAD_COUNT_DECREMENT(_ifcl, _dst)			\
     {									\
	 int s;								\
									\
	 IFCL_LOCK(&((_ifcl)->cl_mutex), s);				\
	 (_ifcl)->cl_xpttn[(_dst)].icxp_nthreads--;			\
	 if((_ifcl)->cl_xpttn[(_dst)].icxp_flags & IFCL_XP_DISPEND &&	\
	    (_ifcl)->cl_xpttn[(_dst)].icxp_nthreads == 0) {		\
	     sv_broadcast(&((_ifcl)->cl_xpttn[(_dst)].icxp_thread_sv));	\
	 }								\
	 IFCL_UNLOCK(&(_ifcl)->cl_mutex, s);				\
     }



/* slist_init
 * ----------
 * Send list init
 */
static void
slist_init(ifcl_slist_t *s)
{
    bzero((caddr_t) s, sizeof(ifcl_slist_t));
}

/* slist_enqueue
 * -------------
 * Returns:		< 0 , if can't enqueue
 *			>= 0 index
 * Parameters:	
 *	s:	slist to enqueue onto
 *	m:	mbuf to enqueue onto s
 *	pttn:	pttn that m is associated with
 *
 * Add m and pttn to the end of the send list.
 */

static int
slist_enqueue(ifcl_slist_t *s, struct mbuf *m, partid_t pttn)
{
    int		last = s->last;

    if ((s->first_wrap != s->last_wrap) && (s->first == s->last)) {
        return -1;
    }

    s->slist[s->last] = m;
    s->pttn[s->last] = pttn;
    s->nflag[s->last] &= ~ISL_NOTIFY;
    s->last = (s->last + 1) % CL_MAX_OUTQ;

    if (!s->last) {
        s->last_wrap++;
    }

    return last;
}

/* slist_notify
 * ------------
 * Returns:		1, if top of the list
 *			2, if mid list
 * Parameters:
 *	s:		slist
 *	slist_index:	index of ack
 *	
 * We got an ack for a send on the send list.
 */

static int
slist_notify(ifcl_slist_t *s, int slist_index)
{
    s->nflag[slist_index] |= ISL_NOTIFY;

    return (slist_index == s->first) ? 1 : 2;
}

/* slist_dequeue
 * -------------
 * Returns:		0 if nothing more to dequeue
 *			1 if more to dequeue
 *			< 0 if nothing to dequeue at all
 * Parameters:	
 *	s:	send list
 *	mp:	pointer to return dequeued mbuf
 *	pttn:	pointer to return dequeued partition.
 *
 * Simply return the top of the send list regardless of the
 * notify state and advance the start of the list.
 */

static int
slist_dequeue(ifcl_slist_t *s, struct mbuf **mp, partid_t *pttn)
{
    if ((s->first_wrap == s->last_wrap) && (s->first == s->last)) {
        return -1;
    }

    if (mp) {
        *mp = s->slist[s->first];
    }
    if (pttn) {
        *pttn = s->pttn[s->first];
    }

    s->nflag[s->first] &= ~ISL_NOTIFY;
    s->slist[s->first] = 0;
    s->first = (s->first + 1) % CL_MAX_OUTQ;

    if (!s->first) {
        s->first_wrap++;
    }

    return (s->nflag[s->first] & ISL_NOTIFY) ? 1 : 0;
}


/* slist_notify_pttn
 * -----------------
 * Parameters:
 *	s:	send list
 *	pttn:	partition to notify
 * Returns:
 *	1:	the first element can be dequeued now!
 *	0:	the first element can't be dequeued yet.
 *
 * go through and mark all the mbufs on the send list that are
 * to a particular partition as sent. 
 */
static int
slist_notify_pttn(ifcl_slist_t *s, partid_t pttn)
{
    int		i, first = 0;

    if (s->last > s->first) {
        for (i = s->first; i < s->last; i++) {
            if (s->pttn[i] == pttn) {
                s->nflag[i] |= ISL_NOTIFY;
                if (i == s->first) {
                    first = 1;
		}
            }
	}
    }
    else if(s->last_wrap > s->first_wrap) {
        for (i = s->first; i < CL_MAX_OUTQ; i++) {
            if (s->pttn[i] == pttn) {
                s->nflag[i] |= ISL_NOTIFY;
                if (i == s->first) {
                    first = 1;
		}
            }
	}

        for (i = 0; i < s->last; i++) {
            if (s->pttn[i] == pttn) {
                s->nflag[i] |= ISL_NOTIFY;
	    }
	}
    } else {
	ASSERT(s->first == s->last && s->first_wrap == s->last_wrap);
    }

    return first;
}


/* m_rearrange
 * -----------
 * Parameters:
 *	m:	mbuf to rearrange.
 * 	return:	-1 on error and 0 on success
 *
 * Rearrange the mbuf to try to make the data/linklist more compact.
 *
 * If this function fails, then it frees the mbuf -- callers might not be
 * expecting this!!!!!
 */
static int
m_rearrange(struct mbuf *m)
{
    /*
     * We know from the MTU size, the max # bytes (and pages). Build 
     * mbuf from complete. 
     */
    __psint_t		l, al;		/* total/alloc length */
    __psint_t		omo, nmo;	/* old/new mbuf offset */
    struct mbuf		*om, *nm, *fm;	/* old/new/free mbuf */

    fm = om = m->m_next;
    nm=m;

    omo = 0;

    l = m_length(om);

    while (l) {
	al = l > MCLBYTES ? MCLBYTES : l;
	nm->m_next = m_vget(M_DONTWAIT, al, MT_DATA);
	nm = nm->m_next;
	nmo = 0;

	if (!nm) {			/* Failed to allocate */
	    m_freem(fm);		/* Free old list */
	    m_freem(m);			/* Free new list */
	    return(-1);
	}

	nm->m_flags |= (om->m_flags & M_COPYFLAGS); /* Copy required flags */

	/* Copy data */

	while (al && (nmo < nm->m_len)) {
	    int		tl;
	    tl  = m_datacopy(om, omo, MIN(nm->m_len - nmo, om->m_len - omo), 
			     mtod(nm, caddr_t) + nmo);
	    omo += tl;
	    nmo += tl;
	    
	    if (omo == om->m_len) {
		omo = 0;
		om  = om->m_next;
	    }
	    al -= tl;
	    l  -= tl;
	}
    }
    m_freem(fm);
    return(0);
}


/* mfit_xpm
 * --------
 * Parameters:
 *	m:	mbuf to find out info for
 *	return:	MFIT_* for how the mbuf fits in xpm, -1 on error
 *
 * Figure out if the mbuf can be sent as immediate, as pointers in the
 * mbuf or must be modified so that it can be sent.
 */

#define	MFIT_NORMAL	1	/* immediate/pointer data can be fit together 
				 * in one message */
#define	MFIT_PTR	2	/* can fit into one message, but all 
				 *immedidate data must be sent as pointer */
#define	MFIT_PULLUP	3	/* need to do some work to make the message 
                                 * sendable */

static int
mfit_xpm(struct mbuf *m)
{
    int		mcnt = 0, error = 0;
    int		space = IFCL_XP_MSG_SZ - sizeof(xpm_t);
    alenlist_t	al = NULL;
    alenlist_t	al_tmp;

    while (m) {
        /* cluster mbuf */
        if (M_HASCL(m)) {
            __psunsigned_t	ptr = mtod(m, __psunsigned_t);

            if (IS_KSEGDM(ptr)) {
                mcnt++;
                space -= sizeof(xpmb_t);
            }
            else {
                /* make alenlist for user land cluser mbuf pointer to see how many 
                 * message we need */
                int	al_pairs;

                if (!(al_tmp = kvaddr_to_alenlist(al, (caddr_t) ptr, m->m_len, 
						  AL_NOSLEEP))) {
		    error = -1;
		    break;
		}
		al = al_tmp;

                al_pairs = alenlist_size(al);

                mcnt += al_pairs;
                space -= (al_pairs * sizeof(xpmb_t));
            }
        }
        else {
            mcnt++;
            space -= sizeof(xpmb_t);
            space -= m->m_len;
        }

        m = m->m_next;
    }

    if (al) {
        alenlist_destroy(al);
    }

    if(error == -1) {
	return(error);
    }

    if (space >= 0)
        return MFIT_NORMAL;
    else if (sizeof(xpm_t) + mcnt * sizeof(xpmb_t) <= IFCL_XP_MSG_SZ)
        return MFIT_PTR;
    else
        return MFIT_PULLUP;
}


/* mfit_normal
 * -----------
 * Parameters:
 *	m:	mbuf to place in the xpm message 
 *	xpm:	xpm to fill up with mbuf info
 *	return:	-1 for error and 0 for success
 *
 * Figure out what kind of data (immedate/pointer (and kernel/user)) we
 * have and place each element in the mbuf link list into the xpm.
 * Everything should fit into one xpm before getting to this funciton.
 */
static int
mfit_normal(struct mbuf *m, xpm_t *xpm)
{
    alenlist_t	al = NULL;
    int error = 0;

    while (m) {
	/* pointer data */
	if (M_HASCL(m)) {
	    __psunsigned_t	data = mtod(m, __psunsigned_t);

	    xpm->xpm_tcnt += m->m_len;
	    
	    /* kernel memory so just put a pointer to mbuf data in xpm */
	    if (IS_KSEGDM(data)) {
		XPM_BUF_FLAGS(xpm, xpm->xpm_cnt) = XPMB_F_PTR;
		XPM_BUF_CNT(xpm, xpm->xpm_cnt) = m->m_len;
		XPM_BUF_PTR(xpm, xpm->xpm_cnt) = KDM_TO_PHYS(data);
		xpm->xpm_cnt++;
	    }
	    else {
		alenaddr_t	alp;
		size_t		all;
		alenlist_t	al_tmp;

		if (!(al_tmp = kvaddr_to_alenlist(al, (caddr_t) data, 
					      m->m_len, AL_NOSLEEP))) {
		    error = -1;
		    break;
		}
		al = al_tmp;
		alenlist_cursor_init(al, 0, NULL);
		
		/* place alenlist pairs in xpm */
		while (alenlist_get(al, 0, 0, &alp, &all, 0) ==
		       ALENLIST_SUCCESS) {
		    XPM_BUF_FLAGS(xpm, xpm->xpm_cnt) = XPMB_F_PTR;
		    XPM_BUF_CNT(xpm, xpm->xpm_cnt) = all;
		    XPM_BUF_PTR(xpm, xpm->xpm_cnt) = alp;
		    xpm->xpm_cnt++;
		}
	    }
	}
	/* immediate data */
	else {
	    caddr_t	cp;
	    int		i;
	    __uint32_t	prev = 0;

	    /* count number of bytes before */
	    for (i = 0; i < xpm->xpm_cnt; i++) {
		if (XPM_BUF_FLAGS(xpm, i) & XPMB_F_IMD) {
		    prev += XPM_BUF_CNT(xpm, i);
		}
	    }

	    /* set as immediate data in xpm message */
	    cp = (caddr_t) ((__psunsigned_t) xpm + IFCL_XP_MSG_SZ - prev
			    - m->m_len);
	    bcopy(mtod(m, caddr_t), cp, m->m_len);
	    
	    xpm->xpm_tcnt += m->m_len;
	    XPM_BUF_FLAGS(xpm, xpm->xpm_cnt) = XPMB_F_IMD;
	    XPM_BUF_CNT(xpm, xpm->xpm_cnt) = m->m_len;
	    XPM_BUF_PTR(xpm, xpm->xpm_cnt) = 
		XPM_BUF_PTR_TO_OFF(XPM_BUF(xpm, xpm->xpm_cnt), cp);
	    xpm->xpm_cnt++;
	}
	
	m = m->m_next;
    }
    if (al) {
        alenlist_destroy(al);
    }

    return(error);
}


/* mfit_ptr
 * --------
 * Parameters:
 *	m:	mbuf that we are putting in the xpm
 *	xpm:	xpm that we are putting mbuf into
 *	return:	-1 for error and 0 for success.
 *
 * All the data fits into one message, but even if we have immediate
 * data we must put it as a pointer/len pair in the xpm message.  Like
 * the mfit_normal case, but never send immediate so don't even check
 * for it.
 */
static int
mfit_ptr(struct mbuf *m, xpm_t *xpm) 
{
    alenlist_t	al = NULL;
    int		error = 0;

    while (m) {
	__psunsigned_t	data = mtod(m, __psunsigned_t);
	
	xpm->xpm_tcnt += m->m_len;
	
	/* in kernel memory */
	if (IS_KSEGDM(data)) {
	    XPM_BUF_FLAGS(xpm, xpm->xpm_cnt) = XPMB_F_PTR;
	    XPM_BUF_CNT(xpm, xpm->xpm_cnt) = m->m_len;
	    XPM_BUF_PTR(xpm, xpm->xpm_cnt) = KDM_TO_PHYS(data);
	    xpm->xpm_cnt++;
	}
	else {
	    alenaddr_t		alp;
	    size_t		all;
	    alenlist_t		al_tmp;
	    
	    if (!(al_tmp = kvaddr_to_alenlist(al, (caddr_t) data, m->m_len, 
					      AL_NOSLEEP))) {
		error = -1;
		break;
	    }
	    al = al_tmp;
	    alenlist_cursor_init(al, 0, NULL);
	    
            /* place alenlist pairs in xpm */
	    while (alenlist_get(al, 0, 0, &alp, &all, 0) ==
		   ALENLIST_SUCCESS) {
		XPM_BUF_FLAGS(xpm, xpm->xpm_cnt) = XPMB_F_PTR;
		XPM_BUF_CNT(xpm, xpm->xpm_cnt) = all;
		XPM_BUF_PTR(xpm, xpm->xpm_cnt) = alp;
		xpm->xpm_cnt++;
	    }
	}
	
	m = m->m_next;
    }
    if (al) {
        alenlist_destroy(al);
    }
    return(error);
}


/* mbuf_to_xpm
 * -----------
 * Parameters:
 *	m:	mbuf to put into xpm
 *	xpm:	xpm message to put into
 *	notify:	will the xpm want a notify message?
 * 	return:	-1 for error and 0 for success
 *
 * call mfit_xpm to find out how m fits into the xpm message.  If it
 * needs to be pulled up, pull up and see if it fits.  Returns an error
 * if can't fit into the xpm.
 */
static int
mbuf_to_xpm(struct mbuf *m, xpm_t *xpm, int notify)
{
    int		fit;
    int 	error = 0;

    xpm->xpm_type = XPM_T_DATA;
    xpm->xpm_flags |= (notify ? XPM_F_NOTIFY : 0);
    xpm->xpm_cnt = 0;
    xpm->xpm_tcnt = 0;

    fit = mfit_xpm(m);
    switch(fit) {
        case MFIT_NORMAL: {
	    error = mfit_normal(m, xpm);
            break;
	}
        case MFIT_PTR: {
	    error = mfit_ptr(m, xpm);
	    break;
	}
        case MFIT_PULLUP: {
            if (m_rearrange(m) < 0) {
                error =  -1;
	    } else {
		fit = mfit_xpm(m);
		if (fit == MFIT_NORMAL) {
		    error = mfit_normal(m, xpm);
		}
		else if (fit == MFIT_PTR) {
		    error = mfit_ptr(m, xpm);
		} 
		else {
		    error = -1;
		}
	    }
	    break; 
	}
        default: {
            DCMN_ERR2((CE_ALERT, "mbuf_to_xpm: Unknown rv\n"));
            error =  -1;
	    break;
	}
    }

    return(error);
}


/* mbuf_to_alenlist
 * ----------------
 * Parameters:  
 *      m:      mbuf to put into alenlist
 *      al:     alenlist to fill up
 *      return: -1 on error or total length on all pairs on success
 *
 * This function does not free al on error.  But builds up an alenlist
 * on the given al with the mbuf data.
 */
static int
mbuf_to_alenlist(struct mbuf *m, alenlist_t al)
{
    int		length = 0;

    do {
        alenlist_t	l;
        __psunsigned_t	data;

        data = mtod(m, __psunsigned_t);

        if (IS_KSEGDM(data)) {
            if (alenlist_append(al, (alenaddr_t) TO_PHYS(data), m->m_len, 
				AL_NOSLEEP) != ALENLIST_SUCCESS) {
                return -1;
	    }
        }
        else {
            if (!(l = kvaddr_to_alenlist(NULL, (caddr_t) data, m->m_len, 
					 AL_NOSLEEP))) {
                return -1;
	    }

            alenlist_concat(l, al);
            alenlist_destroy(l);
        }

        length += m->m_len;
        m = m->m_next;
    } while (m);

    alenlist_cursor_init(al, 0, NULL);

    return length;
}

/* m_peek
 * ------
 * Parameters:
 *      m:      mbuf to peek at
 *      offset: offset into the mbuf to peek at.
 * 
 * Return the data ptr skipping 'offset' bytes in the mbuf chain. Return NULL
 * if mbuf isn't long enough
 */
static __inline __psunsigned_t
m_peek(struct mbuf *m, int offset)
{
    while (m) {
        if (offset < m->m_len) {
            return mtod(m, __psunsigned_t) + offset;
	}

	offset -= m->m_len;
	m = m->m_next;
    }

    return NULL;
}


/* ifcl_xmit_arp
 * -------------
 * Parameters:	
 * 	ifcl:	ifcl_t structure
 *	m:	mbuf containing arp control packet to send
 *	dst:	dst partition (if to send specifically).
 *
 * Convert the mbuf m to a xpm and then send it to the specified 
 * partition (if IFCL_ARP_REQ or IFCL_ARP_REP) or send it to all
 * connected partitions (if IFCL_ARP_ALL for the arp message type).
 * This function is called only by ifcl_arp_output for sending 
 * arp packets.
 */
static void
ifcl_xmit_arp(ifcl_t *ifcl, struct mbuf *m, partid_t dst)
{
    partid_t	pttn, my_partid = part_id();
    ifcl_hdr_t	*ifcl_hdr;
    ifcl_ctrl_t	*ifcl_ctrl;
    xpm_t	*xpm;
    xpc_t	xpc;
    int         fail;

    ifcl_hdr = mtod(m, ifcl_hdr_t *);
    ifcl_ctrl = (ifcl_ctrl_t *) (mtod(m, caddr_t) + sizeof(ifcl_hdr_t));

    /*
     * We need the entire control packet to fit into one xpm; We don't wait
     * for notifs, etc. --> Short & Sweet
     */

    ASSERT(m_length(m) <= (IFCL_XP_MSG_SZ - sizeof(xpm_t) - sizeof(xpmb_t)));

    switch (ifcl_ctrl->ict_arp.ica_type) {
        case IFCL_ARP_REQ:
        case IFCL_ARP_REP: {
            if ((dst != my_partid) && (ifcl->cl_xpttn[dst].icxp_flags &
				       IFCL_XP_UP) &&
		(ifcl->cl_flags & IFCL_IF_UP)) {

		IFCL_THREAD_COUNT_INCREMENT(ifcl, dst, fail);
		if(fail) {
		    break;
		}

                sprintf(ifcl_hdr->ich_tpttn, "%d", dst);

                xpc = xpc_allocate(ifcl->cl_xpttn[dst].icxp_xpchan, &xpm);
                if (xpc == xpcSuccess) {
                    mbuf_to_xpm(m, xpm, 0);

                    xpc = xpc_send(ifcl->cl_xpttn[dst].icxp_xpchan, xpm);

		    IFCL_THREAD_COUNT_DECREMENT(ifcl, dst);

                    if ((xpc != xpcSuccess) && (xpc != xpcDone)) {
                        DCMN_ERR2((CE_ALERT, "ifcl_xmit_arp: xpc_send failed "
				   "dst %d xpc %d\n", dst, xpc));
                    }
                }
                else {
		    IFCL_THREAD_COUNT_DECREMENT(ifcl, dst);
                    DCMN_ERR2((CE_ALERT, "ifcl_xmit_arp: Can't allocate xpm: "
			       "dst %d xpc %d\n", dst, xpc));
                }
            }

            break;
	}
        case IFCL_ARP_ALL: {
            for (pttn = 0; pttn < MAX_PARTITIONS; pttn++) {
                if ((pttn != my_partid) && (ifcl->cl_xpttn[pttn].icxp_flags & 
					    IFCL_XP_UP) &&
		    (ifcl->cl_flags & IFCL_IF_UP)) {

		    IFCL_THREAD_COUNT_INCREMENT(ifcl, pttn, fail);
		    if(fail) {
			break;
		    }

                    sprintf(ifcl_hdr->ich_tpttn, "%d", pttn);

                    xpc = xpc_allocate(ifcl->cl_xpttn[pttn].icxp_xpchan, &xpm);
                    if (xpc == xpcSuccess) {
                        mbuf_to_xpm(m, xpm, 0);

                        xpc = xpc_send(ifcl->cl_xpttn[pttn].icxp_xpchan, xpm);

			IFCL_THREAD_COUNT_DECREMENT(ifcl, pttn);

                        if ((xpc != xpcSuccess) && (xpc != xpcDone)) {
                            DCMN_ERR2((CE_ALERT, "ifcl_xmit_arp: xpc_send "
				       "failed pttn %d xpc %d\n", pttn, xpc));
		        }
                    }
                    else {
			IFCL_THREAD_COUNT_DECREMENT(ifcl, pttn);
                        DCMN_ERR2((CE_ALERT, "ifcl_xmit_arp: Can't allocate "
				   "xpm: pttn %d xpc %d\n", pttn, xpc));
                    }
                }
	    }

            break;
	}
        default: {
            break;
	}
    }
}


/* ifcl_arp_output
 * ---------------
 * Parameters:
 * 	ifcl:		ifcl_t structure
 *	arp_type:	type of arp (to all or particular part)
 *	arp_inaddr:	inaddr that we are trying find info about
 *	dst:		dst partition to send packet to
 *
 * Build up a ifcl control packet mbuf and sent it along to ifcl_xmit_arp 
 * to actually send the packet to all other partitions or to a particular
 * partition (dst).
 */
void
ifcl_arp_output(ifcl_t *ifcl,
                u_short arp_type,
                struct in_addr arp_inaddr,
                partid_t dst)
{
    struct mbuf	*m;
    ifcl_ctrl_t	*ict;
    ifcl_hdr_t	*ich;

    DCMN_ERR2((CE_NOTE, "ifcl_arp_output: type %d dst %d arp_inaddr 0x%x\n",
	       arp_type, dst, arp_inaddr.s_addr));

    m = m_get(M_DONTWAIT, MT_DATA);
    if (!m) {
        return;
    }

    /*
     * Align the end of ifcl_hdr with the ctrl message
     */

    m->m_off += IFCLBUF_PAD;

    ich = mtod(m, ifcl_hdr_t *);

    /*
     * Build the ifcl_hdr in the ctrl pkt.
     * ETHERTYPE_SG_RESV is used in the driver, only for sending ctrl pkts
     */

    bcopy(ifcl->cl_addr, ich->ich_fpttn, IFCL_ADDR_SZ);
    ich->ich_type = ETHERTYPE_SG_RESV;

    ict = (ifcl_ctrl_t *) (mtod(m, caddr_t) + sizeof(ifcl_hdr_t));

    /*
     * Build the ctrl pkt itself
     * The first element of a ctrl pkt (after ifcl_hdr) is a u_int always!
     */

    ict->ict_type = IFCL_CTRL_ARP;
    ict->ict_version = ifcl_version;

    ict->ict_arp.ica_type = arp_type;
    bcopy(ifcl->cl_addr, ict->ict_arp.ica_src_addr, IFCL_ADDR_SZ);
    ict->ict_arp.ica_src_inaddr = ifcl->cl_inaddr;
    ict->ict_arp.ica_arp_inaddr = arp_inaddr;

    m->m_len = sizeof(ifcl_hdr_t) + sizeof(ifcl_ctrl_t);

    /*
     * Send it
     */

    ifcl_xmit_arp(ifcl, m, dst);

    m_free(m);
}


/* snarp_update
 * ------------
 * Parameters:
 *	ifcl_arp:	control information sent with arp message
 *
 * Save the arp information sent to us in the snarptab.  This function
 * is called only by ifcl_arp_input when we get an arp control packet.
 */
static void
snarp_update(ifcl_arp_t *ifcl_arp)
{
    int		i, n, update = MAX_PARTITIONS;

    i = SNARP_HASH(ifcl_arp->ica_src_inaddr.s_addr);

    mrupdate(&snarp_lock);

    for (n = i; n < MAX_PARTITIONS; n++) {
        if (snarptab[n].snarp_flags & SNARP_VALID) {
            if (snarptab[n].snarp_inaddr.s_addr == 
		ifcl_arp->ica_src_inaddr.s_addr) {
                update = n;
                goto end;
	    }
        }
        else {
            if (update == MAX_PARTITIONS) {
                update = n;
	    }
        }
    }

    for (n = 0; n < i; n++) {
        if (snarptab[n].snarp_flags & SNARP_VALID) {
            if (snarptab[n].snarp_inaddr.s_addr == 
		ifcl_arp->ica_src_inaddr.s_addr) {
                update = n;
                goto end;
	    }
        }
        else {
            if (update == MAX_PARTITIONS) {
                update = n; 
	    }
        }
    }

end:
    ASSERT(update < MAX_PARTITIONS);
    snarptab[update].snarp_flags |= SNARP_VALID;
    snarptab[update].snarp_inaddr = ifcl_arp->ica_src_inaddr;
    bcopy(ifcl_arp->ica_src_addr, snarptab[update].snarp_addr, IFCL_ADDR_SZ);

    mrunlock(&snarp_lock);
}


/* ifcl_arp_input
 * --------------
 * Parameters;
 *	ifcl:		ifcl_t structure
 *	ifcl_arp:	arp message sent to us
 *
 * Take the arp message sent to us and do the appropriate thing
 * depending on if this is an IFCL_ARP_ALL or IFCL_ARP_REQ (save the
 * data an send a reply) or IFCL_ARP_REP (just save the data).  This
 * function is called only by ifcl_ctrl_input when we get a control
 * packet.
 */
void
ifcl_arp_input(ifcl_t *ifcl, ifcl_arp_t *ifcl_arp)
{
    struct in_addr	null_inaddr;

    null_inaddr.s_addr = 0;

    DCMN_ERR2((CE_NOTE, "ifcl_arp_input: type %d src in_addr 0x%x src addr "
	       "%s\n", ifcl_arp->ica_type, ifcl_arp->ica_src_inaddr.s_addr, 
	       ifcl_arp->ica_src_addr));

    switch (ifcl_arp->ica_type) {
    case IFCL_ARP_ALL: {
        snarp_update(ifcl_arp);
        ifcl_arp_output(ifcl, IFCL_ARP_REP, null_inaddr,
		atoi(ifcl_arp->ica_src_addr));
        break;
    }
    case IFCL_ARP_REQ: {
        snarp_update(ifcl_arp);
        if (ifcl_arp->ica_arp_inaddr.s_addr == ifcl->cl_inaddr.s_addr) {
            ifcl_arp_output(ifcl, IFCL_ARP_REP, null_inaddr,
		atoi(ifcl_arp->ica_src_addr));
	}
        break;
    }
    case IFCL_ARP_REP: {
        snarp_update(ifcl_arp);
        break;
    }
    default: {
        break;
    }
    }
}


/* ifcl_ctrl_input
 * ---------------
 * Parameters:
 *	ifcl:	ifcl_t structure
 *	m:	mbuf of control packet
 *
 * basically get info out of the mbuf and call ifcl_arp_input to do all
 * the real work.  This function is called by ifcl_xpc_async only when
 * we see that there is a control packet coming to us (ie the typ eof
 * the packet is: ETHERTYPE_SG_RESV).
 */
void
ifcl_ctrl_input(ifcl_t *ifcl, struct mbuf *m)
{
    ifcl_ctrl_t	*ifcl_ctrl;

    M_ADJ(m, sizeof(ifclbufhead_t));

    ifcl_ctrl = mtod(m, ifcl_ctrl_t *);

    switch (ifcl_ctrl->ict_type) {
    case IFCL_CTRL_ARP: {
        ifcl_arp_input(ifcl, &ifcl_ctrl->ict_arp);
        break;
    }
    default: {
        DCMN_ERR2((CE_NOTE, "Received unknown type %d ctrl pkt\n", 
		   ifcl_ctrl->ict_type));
        break;
    }
    }

    m_freem(m);
}


/* ifcl_arp_resolve
 * ----------------
 * Parameters:
 * 	ifcl:		pointer to ifcl_t structure 
 * 	destaddr:	destination address for arp
 *	pttnid:		in non-null save the partid of destaddr here
 *	resolve:	boolean, if true, resolve the address if we
 *			don't know it already.
 *	return:		0 if destaddr found & connected and 1 if not.
 *
 * If pttnid is non-null, then if the destaddr was found then pttnid is
 * filled in with the associated partid for the destaddr.  If resolve is
 * set, then we try to call ifcl_arp_output to find the destination
 * partition, but as far as can tell this doesn't actually work right
 * now!!!!
 */
static int
ifcl_arp_resolve(ifcl_t *ifcl, 
                 struct in_addr destaddr, 
                 partid_t *pttnid, 
                 int resolve)
{
    int		i, n;

    DCMN_ERR2((CE_NOTE, "ifcl_arp_resolve: ifcl 0x%lx: arpaddr 0x%x: "
	       "&pttn 0x%lx\n", ifcl, destaddr.s_addr, pttnid));

    mraccess(&snarp_lock);

    i = SNARP_HASH(destaddr.s_addr);

    for (n = i; n < MAX_PARTITIONS; n++) {
        if ((snarptab[n].snarp_flags & SNARP_VALID) && 
		(snarptab[n].snarp_inaddr.s_addr == destaddr.s_addr)) {
            if (pttnid) {
                *pttnid = atoi(snarptab[n].snarp_addr);
	    }
            mrunlock(&snarp_lock);
            return 0;
        }
    }

    for (n = 0; n < i; n++) {
        if ((snarptab[n].snarp_flags & SNARP_VALID) && 
		(snarptab[n].snarp_inaddr.s_addr == destaddr.s_addr)) {
            if (pttnid) {
                *pttnid = atoi(snarptab[n].snarp_addr);
	    }
            mrunlock(&snarp_lock);
            return 0;
        }
    }

    mrunlock(&snarp_lock);

    /* never found the address in our snarptab */
    /* if we pass in IFCL_ARP_REQ then the destination partition must be
     * valid to do anything in ifcl_xmit_arp (called by ifcl_arp_output).
     * here we are passing in 0 as the partition number which will not be
     * the correct destination necessarily (or most likely) */
    if (resolve) {
        ifcl_arp_output(ifcl, IFCL_ARP_REQ, destaddr, 0);
    }

    return 1;
}


/* ifcl_attach_xpttn
 * -----------------
 * Parameters:
 * 	pttn:	partition coming up
 *	
 * This function is registered with the partition code as the partition
 * activation function.  It basically just initializes the
 * ifcl->cl_xpttn structure for the particular partition that is coming
 * up and then calls xpc_connect to connect the xpc communication
 * channel.
 */
void
ifcl_attach_xpttn(partid_t pttn)
{
    int		unit, s;

    DCMN_ERR1((CE_NOTE, "ifcl_attach_xpttn: pttn %d\n", pttn));

    for (unit = 0; unit < ifcl_num_unit; unit++) {
        xpchan_t	xpchan;
        ifcl_t		*ifcl = ifcl_dev[unit];

        if (!ifcl) {
            continue;
	}

        IFCL_LOCK(&ifcl->cl_mutex, s);

        /*
         * Already initialized
         */

        if (ifcl->cl_xpttn[pttn].icxp_flags & IFCL_XP_UP) {
            IFCL_UNLOCK(&ifcl->cl_mutex, s);
            continue;
        }

        ifcl->cl_xpttn[pttn].icxp_sent = 0;
        ifcl->cl_xpttn[pttn].icxp_ackd = 0;

	/* initialized the thread counting for clean up when the partition
	 * goes down */
	ASSERT(!(ifcl->cl_xpttn[pttn].icxp_flags & IFCL_XP_DISPEND));
	ifcl->cl_xpttn[pttn].icxp_nthreads = 0;
	if(!(ifcl->cl_xpttn[pttn].icxp_flags & IFCL_XP_INIT)) {
	    sv_init(&(ifcl->cl_xpttn[pttn].icxp_thread_sv), SV_FIFO,
		    "icxp_thread_sv");
	    ifcl->cl_xpttn[pttn].icxp_flags |= IFCL_XP_INIT;
	}

        IFCL_UNLOCK(&ifcl->cl_mutex, s);

        xpc_connect(pttn, XP_NET_BASE + unit, IFCL_XP_MSG_SZ, 
			  IFCL_XP_MAX_MSG_NUM, (xpc_async_rtn_t)ifcl_xpc_async,
			  XPC_CONNECT_AOPEN, maxnodes, &xpchan);
    }
}

/* prototypes for debug functions */
#if IFCL_LOG
void ifcl_recv_log_dump(int start, int n);
void ifcl_send_log_dump(int start, int n);
#endif
void ifcl_dump(void);
void ifcl_snarp_dump(void);

/* ifcl_init
 * ---------
 * Initialize the ifcl module.
 */
void
ifcl_init(void)
{
    int			unit;
    graph_error_t	e = GRAPH_SUCCESS;
    char		path[32];
    vertex_hdl_t	xplink_vrtx, xpc_net_vertex = 0;

    DCMN_ERR1((CE_NOTE, "ifcl_init\n"));

    ifcl_version = IFCL_VERSION;

    mrinit(&snarp_lock, "SnarpLock");

    ifcl_dev = (ifcl_t **) kmem_zalloc(ifcl_num_unit * sizeof(ifcl_t *), 
	KM_SLEEP);

    /* add needed ifcl links in the hardware graph for the general ifcl */
    sprintf(path, "/hw/%s", EDGE_LBL_XPLINK);
    if ((xplink_vrtx = hwgraph_path_to_vertex(path)) == GRAPH_VERTEX_NONE) {
        cmn_err(CE_WARN, "ifcl_init: /hw/xplink missing\n");
    } 
    else {
        sprintf(path, "%s/unit", EDGE_LBL_XPLINK_NET);
        if ((e = hwgraph_path_add(xplink_vrtx, path, &xpc_net_vertex))
		!= GRAPH_SUCCESS) {
            cmn_err(CE_WARN, "ifcl_init: Unable to add %s to /hw/xplink "
			"err %d\n", EDGE_LBL_XPLINK_NET, e);
            xpc_net_vertex = 0;
        }
    }

    /* allocate memory for each unit (only 1 now) and add to hwgraph */
    for (unit = 0; unit < ifcl_num_unit; unit++) {
        ifcl_t		*ifcl;

        ifcl = (ifcl_t *) kmem_zalloc(sizeof(ifcl_t), KM_SLEEP);

        slist_init(&ifcl->cl_slist);

	init_spinlock(&ifcl->cl_mutex, "ifcl_cl_mutex", unit);

        sprintf(path, "%d", unit);
        if (xpc_net_vertex && (e = hwgraph_char_device_add(xpc_net_vertex,
		path, "ifcl_", &ifcl->cl_dev)) == GRAPH_SUCCESS) {
            device_info_set(ifcl->cl_dev, ifcl);
            device_inventory_add(ifcl->cl_dev, 0, 0, unit, 0, 0);
        } 
	else {
            cmn_err(CE_WARN, "ifcl_init: Unable to add hwgraph dev for unit "
		"%d err %d\n", unit, e);
	}

        ifcl_dev[unit] = ifcl;
    }

#if IFCL_LOG
    /* init IFCL_LOG structures for debuggin purposes */
    ifcl_recv_log.ptr = 0;
    ifcl_recv_log.wraps = 0;
    ifcl_recv_log.num_exc = 0;
    init_spinlock(&ifcl_recv_log.mutex, "ifcl_recv_log_mutex", 0);
    ifcl_recv_log.re = kmem_alloc(LOG_ENTRIES * sizeof(re_t), KM_SLEEP);
    ifcl_send_log.ptr = 0;
    ifcl_send_log.wraps = 0;
    ifcl_send_log.num_exc = 0;
    init_spinlock(&ifcl_send_log.mutex, "ifcl_send_log_mutex", 0);
    ifcl_send_log.se = kmem_alloc(LOG_ENTRIES * sizeof(se_t), KM_SLEEP);
    
    idbg_addfunc("ifcl_send", ifcl_send_log_dump);
    idbg_addfunc("ifcl_recv", ifcl_recv_log_dump);
#endif /* IFCL_LOG */

    idbg_addfunc("ifcl", ifcl_dump);
    idbg_addfunc("ifcl_snarp", ifcl_snarp_dump);

}

/*
 * Function:		ifcl_open -> Open the device
 * Args: 		dev -> device (others unused)
 * Returns:		0
 * Note:		Exists only to be invoked by ioconfig to if_attach
 *			the device
 */
/* ARGSUSED */
int
ifcl_open(dev_t *devp, int flag, int otyp, struct cred *crp)
{
    ifcl_t		*ifcl;
    int			s, unit;

    DCMN_ERR1((CE_NOTE, "ifcl_open\n"));

    /* unit should always be 0 */
    if ((unit = device_controller_num_get(dev_to_vhdl(*devp))) < 0) {
        cmn_err(CE_ALERT, "ifcl_open: Missing vertex ctrlr number\n");
        return EIO;
    }

    ifcl = ifcl_dev[unit];

    DCMN_ERR1((CE_NOTE, "ifcl_open: unit %d flags 0x%x\n", unit, 
	       ifcl->cl_flags));

    /* Already attached */
    if (ifcl->cl_flags & IFCL_INIT) {
        return 0;
    }

    ifcl->cl_flags |= IFCL_INIT;

    s = splimp();

    /*
     * Setup ntwk interface for xp SN0
     */

    ifcl->cl_if.if_unit = unit;
    ifcl->cl_if.if_name = IF_CL_NAME;	/* name (try netstat -i) */
    ifcl->cl_if.if_type = IFT_CLXP;		/* if type */
    ifcl->cl_if.if_baudrate.ifs_value = 200*1000*1000;
    ifcl->cl_if.if_baudrate.ifs_log2 = 5;
    ifcl->cl_if.if_mtu = IFCL_DFLT_MTU;
    ifcl->cl_if.if_ioctl = ifcl_ifioctl;	/* ioctl syscall */
    ifcl->cl_if.if_output = ifcl_output;	/* send a packet */
#if defined(DEBUG)
    ifcl->cl_if.if_flags = (IFF_DRVRLOCK | IFF_DEBUG);
#else /* !DEBUG */
    ifcl->cl_if.if_flags = IFF_DRVRLOCK;
#endif /* DEBUG */
    ifcl->cl_if.if_snd.ifq_maxlen = CL_MAX_OUTQ;	/* max o/p queue */

    /*
     * Register the CL XP interface as a network interface
     */

    if_attach(&ifcl->cl_if);

    /*
     * Set local interface addr
     */

    sprintf(ifcl->cl_addr, "%d", part_id());

#if RAW
        /*
         * Register raw interface
         */
    rawif_attach(&ifcl->cl_rawif, &ifcl->cl_if,
		(caddr_t) ifcl->cl_addr, 
		(caddr_t) &etherbroadcastaddr[0], 
		IFCL_ADDR_SZ, sizeof(ifcl_hdr_t),
                0, IFCL_ADDR_SZ);
#endif /* RAW */

    splx(s);

    part_register_handlers(ifcl_attach_xpttn, ifcl_shutdown);

    return 0;
}

/* Function:		ifcl_drain -> Drain the pttn queue
 * Args:		ifcl -> ifcl interface
 *			pttn -> pttn to drain
 * Returns:		Nothing
 * Note:		Assumes the pttn lock for the interface is held/
 *			released by the calling routine
 *
 * This is called from ifcl_ifinit -- so I assume that the in ifq is
 * already drained.  This should be true since we got an ifcl_ifioctl
 * when are cl_if is not up yet (only reason we call this function).
 * In this case, I imagine the the ifq is empty.  Again, perhaps
 * we shouldn't be freeing these........
 */

void
ifcl_drain(ifcl_t *ifcl)
{
    int			i;

    DCMN_ERR1((CE_NOTE, "ifcl_drain: unit %d\n", ifcl->cl_if.if_unit));

    /* XXX: Need to fix this. Drain that pttn queue only */

    DCMN_ERR1((CE_NOTE, "ifcl_drain: slist first = %d last = %d\n", 
	       ifcl->cl_slist.first, ifcl->cl_slist.last));

    if (ifcl->cl_slist.last <= ifcl->cl_slist.first) {
        for (i = ifcl->cl_slist.first; i < CL_MAX_OUTQ; i++) {
            m_freem(ifcl->cl_slist.slist[i]);
	    ifcl->cl_slist.slist[i] = 0;
	}

        for (i = 0; i < ifcl->cl_slist.last; i++) {
            m_freem(ifcl->cl_slist.slist[i]);
	    ifcl->cl_slist.slist[i] = 0;
	}
    }
    else {
        for (i = ifcl->cl_slist.first; i < ifcl->cl_slist.last; i++) {
            m_freem(ifcl->cl_slist.slist[i]);
	    ifcl->cl_slist.slist[i] = 0;
	}
    }

    ifcl->cl_slist.first = ifcl->cl_slist.last = 
	ifcl->cl_slist.first_wrap = ifcl->cl_slist.last_wrap = 0;
}


/* ifcl_shutdown
 * -------------
 * Parameters:
 *	pttn:	part going down
 *
 * Registered partition deactivate handler registered with the
 * partition code. 
 */
void
ifcl_shutdown(partid_t pttn)
{
    int			unit;

    DCMN_ERR1((CE_NOTE, "ifcl_shutdown: pttn %d\n", pttn));

    for (unit = 0; unit < ifcl_num_unit; unit++) {
        int		s1;
        ifcl_t		*ifcl = ifcl_dev[unit];

        if (!ifcl) {
            continue;
	}

        IFCL_LOCK(&ifcl->cl_mutex, s1);

	/* partition not up ! */
        if (!(ifcl->cl_xpttn[pttn].icxp_flags & IFCL_XP_UP)) {
            IFCL_UNLOCK(&ifcl->cl_mutex, s1);
            continue;
        }

        ifcl->cl_xpttn[pttn].icxp_flags &= ~IFCL_XP_UP;

	/* remove all mbufs off the send list for this partition */
        if (slist_notify_pttn(&ifcl->cl_slist, pttn)) {
            struct mbuf		*m;

            do {
                IF_DEQUEUE(&ifcl->cl_if.if_snd, m);

                if (m) {
		    IFCL_UNLOCK(&ifcl->cl_mutex, s1);
                    m_freem(m);
		    IFCL_LOCK(&ifcl->cl_mutex, s1);
		}
            } while (slist_dequeue(&ifcl->cl_slist, NULL, NULL) == 1);
        }

	/* Make sure that all threads that have done xpc_allocate do
	 * something with that allocation before we mark the partition
	 * as dead Could get in a funny state if that partition comes
	 * back up and we have let the ring in a bad state. */
	if(ifcl->cl_xpttn[pttn].icxp_nthreads != 0) {
	    ifcl->cl_xpttn[pttn].icxp_flags |= IFCL_XP_DISPEND;
	    sv_wait(&(ifcl->cl_xpttn[pttn].icxp_thread_sv),
		    PZERO, &ifcl->cl_mutex, s1);
	    IFCL_LOCK(&ifcl->cl_mutex, s1);
	    ifcl->cl_xpttn[pttn].icxp_flags &= ~IFCL_XP_DISPEND;
	}

        IFCL_UNLOCK(&ifcl->cl_mutex, s1);

        xpc_disconnect(ifcl->cl_xpttn[pttn].icxp_xpchan);
    }
}

/* ifcl_fillin
 * -----------
 * Doesn't do anything! ?????  Hopefully this means that we don't have
 * to do anything when the networking stuff goes down and then comes
 * back up.
 */
void
ifcl_fillin(ifcl_t *ifcl)
{
    DCMN_ERR1((CE_NOTE, "ifcl_fillin: unit %d\n", ifcl->cl_if.if_unit));

    if (!ifcl) {
        return;
    }

    if (!(ifcl->cl_if.if_flags & IFF_UP)) {
        return;
    }
}

/* ifcl_ifinit
 * -----------
 * Parameters: 
 *	ifp:	struct ifnet for this interface.
 *
 * Called only from ifcl_ifioctl during intialization time.  Just called
 * to mark the interface as up and send out an arp message to all other
 * partitions.
 */
static void
ifcl_ifinit(struct ifnet *ifp)
{
    ifcl_t		*ifcl = ifcl_dev[ifp->if_unit];
    struct in_addr	null_inaddr;

    null_inaddr.s_addr = 0;

    DCMN_ERR1((CE_NOTE, "ifcl_ifinit: unit %d\n", ifcl->cl_if.if_unit));

    if (ifcl->cl_if.if_flags & IFF_UP) {
        ifcl->cl_flags |= IFCL_IF_UP;
        ifcl_fillin(ifcl);

        ifcl_arp_output(ifcl, IFCL_ARP_ALL, null_inaddr, 0);
    }
    else {
        ifcl->cl_flags &= ~IFCL_IF_UP;
        ifcl_drain(ifcl);
    }

    /* XXX: If xp is asleep wake it up */
}


/* ifcl_ifioctl
 * ------------
 * Parameters:
 *	ifp:		struct ifnet for interface.
 *	cmd:		ioctl command
 *	request:	to get the address information for interface.
 *	return:		errno values
 *	
 * We are receiving information about our local address here.
 * Initialize other partions we are connected to and save this address.
 */
int
ifcl_ifioctl(struct ifnet *ifp, int cmd, void *request)
{
    ifcl_t	*ifcl = (ifcl_t *) ifp;
    int		error = 0;

    DCMN_ERR1((CE_NOTE, "ifcl_ifioctl: unit %d cmd %d\n", ifcl->cl_if.if_unit,
	       cmd));

    switch (cmd) {

    case SIOCSIFADDR: {
        struct sockaddr	*addr = _IFADDR_SA(request);
#if 0
	/* if(!(ifcl->cl_xpttn[own_part].icxp_flags & IFCL_XP_UP)) { */
	/* if(!(ifcl->cl_flags & IFCL_READY)) { */
	/* what if this happens before ifcl_attach_xpttn.  That means that
	 *	 the ifcl_arp_output (IFCL_ARP_ALL) doesn't get called
	 *	 at the right time.  Will we loose contact with that other
	 *     	 partition at this point?????? */
        /* XXX: Need to do this */
        if (!(ifcl->xp->p_flags & P_F_READY)) {
            error = ENODEV;
            break;
        }
#endif /* 0 */

        /*
         * User is setting link or protocol address.
         */

        switch (addr->sa_family) {

        case AF_INET: {
            /*
             * Set my IP address
             */
            ifcl->cl_inaddr = ((struct sockaddr_in *)addr)->sin_addr;
            ifp->if_flags |= (IFF_RUNNING | IFF_UP);

            /*
             * Get pttn in correct mode
             */
            ifcl_ifinit(ifp);

            break;
	}
#if RAW
        case AF_RAW: {
            /*
             * Set the cl_addr
             */
            bcopy(_IFADDR_SA(request)->sa_data, ifcl->cl_addr,
		IFCL_ADDR_SZ);

            break;
	}
#endif /* RAW */
        default: {
            DCMN_ERR1((CE_ALERT, "ifcl_ifioctl: Unrecognized sa_family %d\n",
		       _IFADDR_SA(request)->sa_family));
            break;
	}
        }

        break;
    }
    case SIOCSIFFLAGS: {
#if 0
        /* XXX: Need to do this */
        if (!(ifcl->xp->p_flags & P_F_READY)) {
            error = ENODEV;
            break;
        }
#endif /* 0 */

        /*
         * Get pttn in correct mode
         */
        ifcl_ifinit(ifp);

        break;
    }
    case SIOCADDSNOOP:
    case SIOCDELSNOOP: {
        DCMN_ERR1((CE_NOTE, "SNOOP ioctl data 0x%lx\n", request));
        break;
    }
    default: {
        error = EINVAL;
        DCMN_ERR1((CE_ALERT, "ifcl_ifioctl: Unrecognized cmd 0x%x\n", cmd));
        break;
    }
    }

    return error;
}


/* ifcl_alloc_huge_mbuf
 * --------------------
 * Parameters:
 *	length:	length of mbuf to get
 *	return:	mbuf allocated
 *
 * Try to fit it into one mbuf, but it not, concat a bunch of mbufs together.
 */
static __inline struct mbuf *
ifcl_alloc_huge_mbuf(size_t length)
{
    ssize_t	alloc;
    struct mbuf	*m, *n;

    if (length <= MCLBYTES) {
        return m_vget(M_DONTWAIT, length, MT_DATA);
    }

    m = NULL;
    alloc = MCLBYTES;

    do {
        if (m) {
            if (!(n = m_vget(M_DONTWAIT, alloc, MT_DATA))) {
                m_freem(m);
                return NULL;
            }
            else {
                m_cat(m, n);
	    }
        }
        else if (!(m = m_vget(M_DONTWAIT, alloc, MT_DATA))) {
            return NULL;
	}

        length -= alloc;
        alloc = (length > MCLBYTES ? MCLBYTES : length);
    } while (length > 0);

    return m;
}


/* ifcl_alloc_mbuf
 * ---------------
 * Parameters:
 * 	len:	length of mbuf
 * 	return:	mbuf allocated
 *
 * Allocate a new mbuf.  Try to fit into one.  If not call
 * ifcl_alloc_huge_mbuf.
 */
static __inline struct mbuf *
ifcl_alloc_mbuf(size_t len)
{
    struct mbuf	*m1;

    if (len <= MLEN) {
        m1 = m_get(M_DONTWAIT, MT_DATA);
        if (m1) {
            m1->m_len = len;
	}
    }
    else if (len <= MCLBYTES) {
        m1 = m_vget(M_DONTWAIT, len, MT_DATA);
    }
    else {
        m1 = ifcl_alloc_huge_mbuf(len);
    }

    return m1;
}

/* end of mbuf */
#define EOMBUF(m, eombuf)       \
        for ((eombuf) = (m); (eombuf)->m_next; (eombuf) = (eombuf)->m_next)

/* ifcl_mbuf_get
 * -------------
 * Parameters:
 *	xpm:	xpm to turn to mbuf.
 *	return:	mbuf allocated, or NULL on error
 *
 * Turn an xpm into an mbuf and return it.
 */
static struct mbuf *
ifcl_mbuf_get(xpm_t *xpm)
{
    int		i;
    struct mbuf	*m;

    m = NULL;

    for (i = 0; i < xpm->xpm_cnt; i++) {
        size_t		alloc;
        struct  mbuf    *m1, *eombuf;

        /*
         * At the start is the ifcl header. Allocate space for ifclbufhead_t
         * which also includes the header
         */

        if (!i) {
            alloc = XPM_BUF(xpm, i)->xpmb_cnt + IFCLBUF_HEAD;
	}
        else {
            alloc = XPM_BUF(xpm, i)->xpmb_cnt;
	}

        m1 = ifcl_alloc_mbuf(alloc);

        if (!m1) {
            if (m) {
                m_freem(m);
	    }
            return NULL;
        }

        if (!m) {
            m = m1;
	}
        else {
            EOMBUF(m, eombuf);
            eombuf->m_next = m1;
        }
    }

    /*
     * If the message couldn't be contained in one xpm, attach another mbuf
     * for the residual. This will cause very poor performance, but we try
     * to contain everything within one xpm
     */

    if ((m_length(m) - IFCLBUF_HEAD) < xpm->xpm_tcnt) {
        struct  mbuf    *m1, *eombuf;

        m1 = ifcl_alloc_mbuf(xpm->xpm_tcnt - (m_length(m) - IFCLBUF_HEAD));

        if (!m1) {
            if (m) {
                m_freem(m);
	    }
            return NULL;
        }

        EOMBUF(m, eombuf);
        eombuf->m_next = m1;
    }

    return m;
}


/* ifcl_output_odone
 * -----------------
 * Parameters:
 *	xpc:    return code for this callback
 *	xpr:    unused (pointer dup-ed in ifcl struct)
 *	pttn:   partition number of callback
 *	chan:   which xpc channel
 *	cookie: index in send queue (void * for callback)
 *
 * This function is passed into xpc_send_notify as a callback function.
 * So when xpc_notify_continue is called when the channel is ready, this
 * function gets called from the xpc code.  This can also be called
 * directly by ifcl_done (which is the function that calls
 * xpc_send_notify as well).  Just goes into the send queue and frees up
 * all the entries at the end of the queue that are freed up by this
 * acknowledgement.
 */
/* ARGSUSED */ 
void ifcl_output_odone(xpc_t xpc,
                  xpchan_t xpr, 
                  partid_t pttn, 
                  int chan, 
                  void *cookie)
{
    struct mbuf	*m;
    int		s, index, unit = chan - XP_NET_BASE;
    ifcl_t	*ifcl = ifcl_dev[unit];

    DCMN_ERR2((CE_NOTE, "ifcl_output_odone: xpc %d pttn %d unit %d cookie "
	       "%d\n", xpc, pttn, unit, cookie));

    /*
     * IF_DEQUEUE might be called and if so, it'll grab a spinlock.
     * Same order of IFCL_LOCK followed by IFQ_LOCK is also
     * followed in ifcl_output, so we're safe!
     */

    IFCL_LOCK(&ifcl->cl_mutex, s);

    ifcl->cl_xpttn[pttn].icxp_ackd++;

    index = (__psunsigned_t) cookie;

    if (slist_notify(&ifcl->cl_slist, index) == 1) {
        do {
            IF_DEQUEUE(&ifcl->cl_if.if_snd, m);
            ASSERT(m);

	    IFCL_UNLOCK(&ifcl->cl_mutex, s);
            m_freem(m);
	    IFCL_LOCK(&ifcl->cl_mutex, s);
            DCMN_ERR2((CE_NOTE, "ifcl_output_odone: dequeue m 0x%lx\n", m));
        } while (slist_dequeue(&ifcl->cl_slist, NULL, NULL) == 1);
    }

    /* XXX: Need to check notify status here ! */

    IFCL_UNLOCK(&ifcl->cl_mutex, s);
}

#if IFCL_DEBUG_HDR
/* ifcl_docksum
 * ------------
 * Parameters:
 *      m:      mbuf to do checksum of
 *      return: checksum 
 * 
 * Implement a checksum just for IFCL_DEBUG_HDR
 */
static u_long
ifcl_docksum(struct mbuf *m)
{
    u_long	cksum = 0;

    while (m) {
        char	*c;
        int	i;

        c = mtod(m, char *);

        for (i = 0; i < m->m_len; i++) {
            cksum += c[i];
	}
    
        m = m->m_next;
    }

    return cksum;
}

#endif /* IFCL_DEBUG_HDR */

/* ifcl_output
 * -----------
 * Parameters:
 *      ifp:    network interface info
 *      m:      mbuf that is to be sent
 *      dst:    address of interface we need to send to
 *      rt:     route entry info.
 *
 * Called by other layers of network code to do output of data
 * (ip_output, udp_output, etc).  Add a ifcl header to the message,
 * allocate a xpc message and send it, and then enqueue is in the send
 * list that we are waiting for.  Make sure to clean up at any stage if
 * there is an error.
 */
int
ifcl_output(struct ifnet *ifp,
             struct mbuf *m,
             struct sockaddr *dst,
             struct rtentry *rt)
{
    int			s, s1, fail, r = 0;
    ifcl_hdr_t		*ifcl_hdr;
    partid_t		to_pttn;
    xpm_t		*xpm;
    xpc_t		xpc;
    __psunsigned_t	cookie;
    struct ip		*ip;
    ifcl_t		*ifcl = ifcl_dev[ifp->if_unit];

    DCMN_ERR2((CE_NOTE, "ifcl_output: unit %d m 0x%lx len %d dst family %d "
	       "addr 0x%x\n", ifp->if_unit, m, m_length(m), dst->sa_family, 
	       ((struct sockaddr_in *) dst)->sin_addr.s_addr));

    if ((ifp->if_flags & (IFF_RUNNING | IFF_UP)) != (IFF_RUNNING | IFF_UP)) {
        m_freem(m);
        return ENETDOWN;
    }

    IFQ_LOCK(&ifcl->cl_if.if_snd, s);

    if (IF_QFULL(&ifcl->cl_if.if_snd)) {
        IF_DROP(&ifcl->cl_if.if_snd);
        IFQ_UNLOCK(&ifcl->cl_if.if_snd, s);
        DCMN_ERR2((CE_NOTE, "ifcl_output: IF_QFULL"));
        m_freem(m);
        return ENOBUFS;
    }

    IFQ_UNLOCK(&ifcl->cl_if.if_snd, s);

    ip = mtod(m, struct ip *);

    /*
     * Squeeze the if_cl header to the mbuf chain
     */
    
#if 0
    if ((m->m_off < MMAXOFF) && !(m->m_off & 3) &&
	(m->m_off >= (__psint_t) (sizeof(ifcl_hdr_t) + MMINOFF))) {
        ASSERT(!M_HASCL(m));

        /*
         * Header can fit in front of first mbuf
         */

        m->m_off -= sizeof(ifcl_hdr_t);
        m->m_len += sizeof(ifcl_hdr_t);
        ifcl_hdr = mtod(m, ifcl_hdr_t *);

        bzero((caddr_t) ifcl_hdr, sizeof(ifcl_hdr_t));
    }
    else if ((m->m_off >= MMINOFF) && (m->m_off < MMAXOFF) &&
	!(m->m_len & 3) && !(m->m_off & 3) &&
	(m->m_len + sizeof(ifcl_hdr_t) <= MLEN)) {
        __uint32_t	*d1, *d2;
        int		i;

        /*
         * Header can fit if we tweak the first mbuf
         */

        d1 = mtod(m, __uint32_t *);
        d2 = d1 + (MMAXOFF - m->m_len - m->m_off) / sizeof(__uint32_t);

	/* !!!! WHERE DO THE BRACKETS GO !!!!!????? */
        for (i = (m->m_len - 4) / 4; i >= 0; i--)
            d2[i] = d1[i];

            m->m_off = MMAXOFF - m->m_len - sizeof(ifcl_hdr_t);
            m->m_len +=  sizeof(ifcl_hdr_t);
            ifcl_hdr = mtod(m, ifcl_hdr_t *);

            bzero((caddr_t) ifcl_hdr, sizeof(ifcl_hdr_t));
    }
    else
#endif /* 0 */
    {
        /* add new mbuf at beginning for ifcl header */
        struct mbuf	*m2;

        m2 = m_get(M_DONTWAIT, MT_DATA);

        if (!m2) {
            m_freem(m);
            return ENOBUFS;
        }

        m2->m_off += (ROUND_BTE(mtod(m2, __psunsigned_t)) - 
		mtod(m2, __psunsigned_t));
        ifcl_hdr = mtod(m2, ifcl_hdr_t *);
        m2->m_len = sizeof(ifcl_hdr_t);
        m2->m_next = m;
        m2->m_flags |= (m->m_flags & M_COPYFLAGS);
        m = m2;
        DCMN_ERR2((CE_NOTE, "ifcl_output: m = 0x%lx: len = %d\n", m, 
		   m_length(m)));
    }

    switch (dst->sa_family) {
    case AF_INET: {
        /*
         * Resolve pttnid
         */
        if (ifcl_arp_resolve(ifcl, ((struct sockaddr_in *) dst)->sin_addr, 
			&to_pttn, 1)) {
            m_freem(m);
            DCMN_ERR2((CE_NOTE, "ifcl_output: no ifcl_arp_resolve\n"));
            return EHOSTUNREACH;
        }

        /*
         * Fill up header
         */
        bcopy(ifcl->cl_addr, (caddr_t) ifcl_hdr->ich_fpttn, IFCL_ADDR_SZ);
        sprintf(ifcl_hdr->ich_tpttn, "%d", to_pttn);
        ifcl_hdr->ich_type = ETHERTYPE_IP;
#if IFCL_DEBUG_HDR
        ifcl_hdr->ich_cksum = 0;
        ifcl_hdr->ich_cookie = 0;
        ifcl_hdr->ich_cksum = ifcl_docksum(m);

        DCMN_ERR2((CE_NOTE, "ifcl_output: ifcl_arp_resolve to_pttn %d cksum "
		   "0x%lx\n" , to_pttn, ifcl_hdr->ich_cksum));
#else /* !IFCL_DEBUG_HDR */
        DCMN_ERR2((CE_NOTE, "ifcl_output: ifcl_arp_resolve to_pttn %d\n", 
		   to_pttn));
#endif /* IFCL_DEBUG_HDR */

        break;
    }
    default: {
        DCMN_ERR1((CE_ALERT, "%s%d: ifcl_output: Unsupported addr. family: "
		   "0x%x.\n", ifp->if_name, ifp->if_unit, dst->sa_family));
        m_freem(m);
        return EAFNOSUPPORT;
    }
    }

    /*
     * Turn on RTF_CKSUM (TCP doesn't do data checksumming) if the final 
     * destination is one of the hosts directly connected through the cl 
     * interface
     */

    if ((!(rt->rt_flags & RTF_CKSUM)) && 
	((((struct sockaddr_in *) dst)->sin_addr.s_addr == ip->ip_dst.s_addr)
	 || !ifcl_arp_resolve(ifcl, ip->ip_dst, NULL, 0))) {
        rt->rt_flags |= RTF_CKSUM;
    }

    DCMN_ERR2((CE_NOTE, "ifcl_output: dst = 0x%x: rt = 0x%x: ip = 0x%x "
	       "rtcsum %d\n", ((struct sockaddr_in *) dst)->sin_addr.s_addr,
	       ((struct sockaddr_in *) rt_key(rt))->sin_addr.s_addr,
	       ip->ip_dst.s_addr, (rt->rt_flags & RTF_CKSUM)));

#if 0
#if RAW
    /* XXX: What is snoop_match ? Is m the right parameter to pass ? */
    if (RAWIF_SNOOPING(&ifcl->cl_rawif) &&
		snoop_match(&ifcl->cl_rawif, mtod(m, caddr_t), m->m_len)) {
        int		len, curlen, lenoff;
        struct mbuf	*ms, *m1, *mt;
        int		s2 = splimp();

        len = m_length(m);
        lenoff = 0;
        curlen = len + sizeof(struct ifheader) + sizeof(struct snoopheader);

        if (curlen > VCL_MAX) {
            curlen = VCL_MAX;
	}

        ms = m_vget(M_DONTWAIT, MAX(curlen, sizeof(struct ifheader) +
		sizeof(struct snoopheader) + IFCL_ADDR_SZ), MT_DATA);

        if (ms) {
            IF_INITHEADER(mtod(ms, caddr_t),
			&ifcl->cl_if, sizeof(struct ifheader) + 
			sizeof(struct snoopheader) + IFCL_ADDR_SZ);

            curlen = m_datacopy(m, lenoff, curlen - sizeof(struct ifheader)
			- sizeof(struct snoopheader), mtod(ms, caddr_t) +
			sizeof(struct ifheader) + sizeof(struct snoopheader));

            mt = ms;

            for (;;) {
                lenoff += curlen;
                len -= curlen;

                if (len <= 0) {
                    break;
		}

                curlen = MIN(len, VCL_MAX);
                m1 =  m_vget(M_DONTWAIT, curlen, MT_DATA);

                if (!m1) {
                    m_freem(ms);
                    ms = 0;
                    break;
                }

                mt->m_next = m1;
                mt = m1;
                curlen = m_datacopy(m, lenoff, curlen, mtod(m1, caddr_t));
            }
        }
        if (!ms) {
            snoop_drop(&ifcl->cl_rawif, SN_PROMISC, mtod(m, caddr_t),
			m->m_len);
        }
        else {
            (void) snoop_input(&ifcl->cl_rawif, SN_PROMISC, mtod(m, caddr_t), ms, (lenoff > IFCL_ADDR_SZ ? lenoff - IFCL_ADDR_SZ : 0));
        }

        splx(s2);
    }
#endif /* RAW */
#endif /* 0 */

    /*
     * Allocate a xpm entry. xpc_allocate could go to sleep!
     */
    IFCL_THREAD_COUNT_INCREMENT(ifcl, to_pttn, fail);

    if(fail) {
        m_freem(m);
	return ENETDOWN;
    }

    if ((xpc = xpc_allocate(ifcl->cl_xpttn[to_pttn].icxp_xpchan, &xpm)) !=
	xpcSuccess) {
        m_freem(m);
        ifcl->cl_if.if_odrops++;
        DCMN_ERR2((CE_NOTE, "ifcl_output: xpc_allocate failed\n"));
	IFCL_THREAD_COUNT_DECREMENT(ifcl, to_pttn);
        return ENOBUFS;
    }

    xpm->xpm_type = XPM_T_DATA;
    xpm->xpm_flags = XPM_F_CLEAR;
    xpm->xpm_cnt = 0;
    xpm->xpm_tcnt = 0;

    /*
     * Both of them are spinlocks! This ordering is importatnt! Follow
     * the same order in output_odone and you're safe!
     */

    IFCL_LOCK(&ifcl->cl_mutex, s1);
    IFQ_LOCK(&ifcl->cl_if.if_snd, s);

    /*
     * Check this again if anyone shutdown the if/xp
     */

    if (((ifp->if_flags & (IFF_RUNNING | IFF_UP)) != (IFF_RUNNING | IFF_UP)) ||
	!(ifcl->cl_xpttn[to_pttn].icxp_flags & IFCL_XP_UP) ||
	(ifcl->cl_xpttn[to_pttn].icxp_flags & IFCL_XP_DISPEND)) {

        DCMN_ERR2((CE_NOTE, "ifcl_output: if/xp down\n"));
        r = ENETDOWN;
        goto fail;
    }

    if (IF_QFULL(&ifcl->cl_if.if_snd)) {
        ifcl->cl_if.if_odrops++;
        IF_DROP(&ifcl->cl_if.if_snd);
        r = ENOBUFS;
        goto fail;
    }

    /*
     * Enqueue the message!
     */

    if ((cookie = (__psunsigned_t) slist_enqueue(&ifcl->cl_slist, m, to_pttn))
		== (__psunsigned_t) (int) -1) {
        ifcl->cl_if.if_odrops++;
        IF_DROP(&ifcl->cl_if.if_snd);
        DCMN_ERR2((CE_NOTE, "ifcl_output: slist_enqueue failed\n"));
        r = ENOBUFS;
        goto fail;
    }

#if IFCL_DEBUG_HDR
    ifcl_hdr->ich_cookie = cookie;
#endif /* IFCL_DEBUG_HDR */

    if (mbuf_to_xpm(m, xpm, 1) < 0) {
        xpm->xpm_cnt = 0;
        xpm->xpm_tcnt = 0;
        ifcl->cl_if.if_odrops++;
        IF_DROP(&ifcl->cl_if.if_snd);
        DCMN_ERR2((CE_NOTE, "ifcl_output: mbuf_to_xpm failed\n"));
        r = ENOBUFS;
        goto fail;
    }

    IF_ENQUEUE_NOLOCK(&ifcl->cl_if.if_snd, m);

    ifp->if_opackets++;
    ifp->if_obytes += m_length(m);

    IFQ_UNLOCK(&ifcl->cl_if.if_snd, s);
    IFCL_UNLOCK(&ifcl->cl_mutex, s1);

    /*
     * Send the packet! If return code is not xpcSuccess
     * dequeue from send queue
     */

    xpc = xpc_send_notify(ifcl->cl_xpttn[to_pttn].icxp_xpchan, xpm,
		ifcl_output_odone, (void *) cookie);

    IFCL_THREAD_COUNT_DECREMENT(ifcl, to_pttn);
    atomicAddInt(&(ifcl->cl_xpttn[to_pttn].icxp_sent), 1);

#if IFCL_LOG
    /* log this sending of the message */
    {
        int     s1, i, ptr;

        s1 = mutex_spinlock(&ifcl_send_log.mutex);
        ptr = ifcl_send_log.ptr;
        ifcl_send_log.ptr++;
        if (ifcl_send_log.ptr == LOG_ENTRIES) {
            ifcl_send_log.ptr = 0;
            ifcl_send_log.wraps++;
        }
        mutex_spinunlock(&ifcl_send_log.mutex, s1);

        bzero((caddr_t) &ifcl_send_log.se[ptr], sizeof(se_t));

        ifcl_send_log.se[ptr].cpu = cpuid();
        ifcl_send_log.se[ptr].kthread = private.p_curkthread;

        for (i = 0; i < MIN(xpm->xpm_cnt, MAX_ADDRS_IN_MESG); i++) {
            ifcl_send_log.se[ptr].cnt[i] = XPM_BUF_CNT(xpm, i);
            ifcl_send_log.se[ptr].addr[i] = XPM_BUF_PTR(xpm, i);
        }

        if (xpm->xpm_cnt >= MAX_ADDRS_IN_MESG) {
            ifcl_send_log.num_exc++;
	}

        ifcl_send_log.se[ptr].outsd = ifcl->cl_xpttn[to_pttn].icxp_sent -
                ifcl->cl_xpttn[to_pttn].icxp_ackd;
    }
#endif /* IFCL_LOG */

    /* our send failed so dequeue this info message from the send list */
    if (xpc != xpcSuccess) {
        ifcl_output_odone(xpc, ifcl->cl_xpttn[to_pttn].icxp_xpchan, to_pttn,
		ifp->if_unit + XP_NET_BASE, (void *) cookie);
    }

    DCMN_ERR2((CE_NOTE, "ifcl_output: xpn_send_al cookie %ld xpc %d\n", cookie,
	       xpc));
    return xpc_errno[xpc];

fail:
    IFQ_UNLOCK(&ifcl->cl_if.if_snd, s);
    IFCL_UNLOCK(&ifcl->cl_mutex, s1);
    m_freem(m);

    /*
     * Turn off notify flag, don't care about this one.
     */
    xpm->xpm_flags &= ~XPM_F_NOTIFY;
    xpc_send(ifcl->cl_xpttn[to_pttn].icxp_xpchan, xpm);

    IFCL_THREAD_COUNT_DECREMENT(ifcl, to_pttn);

    return r;
}

/* ifcl_xpc_async
 * --------------
 * Parameters:
 *	xpc:	xpc_t return code associated with this call
 *	xpr:	xpr_t structure for the chan (for xpcConnect)
 *	pttn:	what partition this message is coming from
 *	chan:	logical channel associated with packet
 *	r:	xpm message when called by xpc_receive_xpm
 *
 * Passed into xpc_connect and this is called by xpc_set_error (when
 * there is an error), xpc_process_connect (when the connect is
 * complete) and xpc_receive_xpm (whenever there is an incoming packet).
 *
 * Most will come in with a xpcAsync for the xpc return code.
 * xpcConnect is pretty simple and the code is at the end of the
 * function.  Basically xpcAsync deals with all packets with info.  The
 * packet can either be an arp packet or a regular message from a
 * another partition.  If we get a regular message, then basically we
 * just massage it into the form that the other network input code wants
 * the message in (mbuf without ifcl header) and we sent it up the chain
 * to the network_input code.
 */
/* ARGSUSED */
void
ifcl_xpc_async(xpc_t xpc, xpchan_t xpr, partid_t pttn, int chan, void *r)
{
    int			s;
    struct mbuf		*m;
    alenlist_t		al;
    alenaddr_t		alp;
    size_t		all;
    xpm_t		*xpm;
    ssize_t		length;
    __uint32_t		xpmb_cnt;
    xpmb_t		*xpmb_left;
    int			unit;
    ifcl_t		*ifcl;
    ifcl_hdr_t		*ifcl_hdr;
    struct in_addr	*ip_src;
#if IFCL_DEBUG_HDR
    ssize_t		dlen;
    u_long		cksum, clccksum;
    __psunsigned_t	cookie;
#endif /* IFCL_DEBUG_HDR */

    unit = chan - XP_NET_BASE;
    ifcl = ifcl_dev[unit];

    DCMN_ERR2((CE_NOTE, "ifcl_xpc_async: xpc %d unit %d pttn %d\n", xpc, unit,
	       pttn));
    
    switch (xpc) {
    case xpcAsync: {
        xpm = (xpm_t *) r;

#if IFCL_DEBUG_HDR
        dlen = xpm->xpm_tcnt;
#endif /* IFCL_DEBUG_HDR */
        length = xpm->xpm_tcnt;
        xpmb_cnt = xpm->xpm_cnt;
        DCMN_ERR2((CE_NOTE, "ifcl_xpc_async: xpcAsync length %d xpmb_cnt "
		   "%d\n", length, xpmb_cnt));

        if (!length || !xpmb_cnt) {
            DCMN_ERR2((CE_NOTE, "ifcl_xpc_async: NULL length\n"));
            xpc_done(ifcl->cl_xpttn[pttn].icxp_xpchan, xpm);
            break;
        }

        /* created mbuf and alenlist from xpm */
        if (!(m = ifcl_mbuf_get(xpm))) {
            DCMN_ERR2((CE_NOTE, "ifcl_xpc_async: m_vget failed\n"));
            xpc_done(ifcl->cl_xpttn[pttn].icxp_xpchan, xpm);
            ifcl->cl_if.if_ipackets++;
            ifcl->cl_if.if_ierrors++;
            break;
        }

        DCMN_ERR1((CE_NOTE, "xpc_async\n"));
        mbuf_dump(m);

        al = alenlist_create(0);

        if (mbuf_to_alenlist(m, al) < 0) {
            DCMN_ERR2((CE_NOTE, "ifcl_xpc_async: mbuf_to_alenlist failed\n"));
            xpc_done(ifcl->cl_xpttn[pttn].icxp_xpchan, xpm);
            alenlist_destroy(al);
            m_freem(m);
            ifcl->cl_if.if_ipackets++;
            ifcl->cl_if.if_ierrors++;
            break;
        }

        /*
         * Move the offset of the first mbuf, so we start DMA'ing from
         * the beginning of ifcl_hdr_t
         */

        alenpair_get(al, &alp, &all);
        alp += IFCLBUF_HEAD;
        all -= IFCLBUF_HEAD;
        alenlist_replace(al, NULL, &alp, &all, 0);
        alenlist_cursor_init(al, 0, NULL);        

#if IFCL_LOG
    {
        int     s1, i, ptr;

        s1 = mutex_spinlock(&ifcl_recv_log.mutex);
        ptr = ifcl_recv_log.ptr;
        ifcl_recv_log.ptr++;
        if (ifcl_recv_log.ptr == LOG_ENTRIES) {
            ifcl_recv_log.ptr = 0;
            ifcl_recv_log.wraps++;
        }
        mutex_spinunlock(&ifcl_recv_log.mutex, s1);

        bzero((caddr_t) &ifcl_recv_log.re[ptr], sizeof(re_t));

        ifcl_recv_log.re[ptr].chan = chan;
        ifcl_recv_log.re[ptr].cpu = cpuid();
        ifcl_recv_log.re[ptr].kthread = private.p_curkthread;

        if (xpm->xpm_cnt >= MAX_ADDRS_IN_MESG) {
            ifcl_recv_log.num_exc++;
	}

        for (i = 0; i < MIN(MAX_ADDRS_IN_MESG, xpm->xpm_cnt); i++) {
            if (XPM_BUF(xpm, i)->xpmb_flags & XPMB_F_IMD) {
                ifcl_recv_log.re[ptr].type[i] = IMD;
                ifcl_recv_log.re[ptr].scnt[i] = XPM_BUF(xpm, i)->xpmb_cnt;
                ifcl_recv_log.re[ptr].saddr[i] = (__psunsigned_t) xpm +
                        (__psunsigned_t) XPM_BUF(xpm, i)->xpmb_ptr;
            }
            else {
                ifcl_recv_log.re[ptr].type[i] = PTR;
                ifcl_recv_log.re[ptr].scnt[i] = XPM_BUF(xpm, i)->xpmb_cnt;
                ifcl_recv_log.re[ptr].saddr[i] = PHYS_TO_K0((__psunsigned_t)
                        XPM_BUF(xpm, i)->xpmb_ptr);
            }
        }

        alenlist_cursor_init(al, 0, NULL);

        for (i = 0; i < MAX_ADDRS_IN_MESG; i++) {
            alenaddr_t  alp;
            size_t      all;

            if (alenlist_get(al, NULL, 0, &alp, &all, 0) ==
                        ALENLIST_SUCCESS) {
                ifcl_recv_log.re[ptr].rcnt[i] = all;
                ifcl_recv_log.re[ptr].raddr[i] = alp;
            }
            else {
                break;
	    }
        }

        alenlist_cursor_init(al, 0, NULL);
    }
#endif /* IFCL_LOG */

        if ((xpc = xpc_receive_xpmb(XPM_BUF(xpm, 0), &xpmb_cnt, &xpmb_left, 
			al, &length)) != xpcSuccess) {
            DCMN_ERR2((CE_NOTE, "ifcl_xpc_async: xpc_recv_al xpc %d\n", xpc));
            xpc_done(ifcl->cl_xpttn[pttn].icxp_xpchan, xpm);
            alenlist_destroy(al);
            m_freem(m);
            ifcl->cl_if.if_ipackets++;
            ifcl->cl_if.if_ierrors++;
            break;
        }

        xpc_done(ifcl->cl_xpttn[pttn].icxp_xpchan, xpm);

        if (length || xpmb_cnt) {
            DCMN_ERR1((CE_NOTE, "ifcl_xpc_async: length %d\n", length));
            alenlist_destroy(al);
            m_freem(m);
            ifcl->cl_if.if_ipackets++;
            ifcl->cl_if.if_ierrors++;
            break;
        }

        alenlist_destroy(al);

        ifcl_hdr = (ifcl_hdr_t *) (mtod(m, caddr_t) + IFCLBUF_HEAD);

        /*
         * Check for snoop pkts
         */

#if RAW
        if (RAWIF_SNOOPING(&ifcl->cl_rawif) && snoop_match(&ifcl->cl_rawif, 
		(caddr_t) ifcl_hdr, m->m_len - IFCLBUF_HEAD)) {
            struct mbuf	*ms, *m1, *eom, *mo;

            mo = m;
            ms = NULL;

            while (mo) {
                m1 = ifcl_alloc_mbuf(mo->m_len);
                if (!m1) {
                    if (ms) {
                        m_freem(ms);
		    }
                    ms = NULL;
                    break;
                }

                if (!ms) {
                    ms = m1;
		}
                else {
                    EOMBUF(ms, eom);
                    eom->m_next = m1;
                }

                m_datacopy(mo, 0, mo->m_len, mtod(m1, caddr_t));

                mo = mo->m_next;
            }

            if (!ms) {
                snoop_drop(&ifcl->cl_rawif, SN_PROMISC, mtod(m, caddr_t),
			m->m_len);
            }
            else {
                IF_INITHEADER(mtod(ms, caddr_t),
			&ifcl->cl_if, sizeof(ifclbufhead_t));

                (void) snoop_input(&ifcl->cl_rawif, SN_PROMISC, (caddr_t) 
			ifcl_hdr, ms, m_length(m) - sizeof(ifclbufhead_t));
            }
        }
#endif /* RAW */

        /* XXX: Do some assertions here for ifcl_hdr */

#if IFCL_DEBUG_HDR
        cksum = ifcl_hdr->ich_cksum;
        cookie = ifcl_hdr->ich_cookie;
        ifcl_hdr->ich_cksum = 0;
        ifcl_hdr->ich_cookie = 0;
        clccksum = ifcl_docksum(m);

	/* every message is actually a mismatch.  There is something wrong
	 * with the checksum algorithm.  I'm not going to fix it now, but
	 * if you want to look at it :). */
        if (clccksum != cksum) {
            DCMN_ERR1((CE_NOTE, "ifcl_xpc_async: pkt cookie %ld cksum 0x%lx "
		       "length %d computed cksum 0x%lx MISMATCH\n", cookie, 
		       cksum, dlen, clccksum));
        }
        else {
            DCMN_ERR2((CE_NOTE, "ifcl_xpc_async: pkt cookie %ld cksum 0x%lx "
		       "MATCH\n", cookie, cksum));
        }
#endif /* IFCL_DEBUG_HDR */

        /*
         * Fake checksum if the src is one of the partitions connected through
         * the cl interface; Just peek into the ip hdr for the ip_src addr and
         * see if we can resolve it
         */

        if (ifcl_hdr->ich_type == ETHERTYPE_IP) {
            struct ip	ip_dummy;
#define	IP_SRCADDR_OFF(ip)	\
	((__psunsigned_t) &ip.ip_src - (__psunsigned_t) &ip)

            if (!(ip_src = (struct in_addr *)
		m_peek(m, sizeof(ifclbufhead_t) + IP_SRCADDR_OFF(ip_dummy)))) {
                DCMN_ERR2((CE_NOTE, "ifcl_xpc_async: short packet m 0x%lx "
			   "len %d\n", m, m_length(m)));
                m_freem(m);
                ifcl->cl_if.if_ipackets++;
                ifcl->cl_if.if_ierrors++;
                break;
            }

            /* find out who this message is from */
            if (!ifcl_arp_resolve(ifcl, *ip_src, NULL, 0)) {
                struct mbuf	*m1;

                m1 = m;
                while (m1) {
                    m1->m_flags |= M_CKSUMMED;
                    m1 = m1->m_next;
                }
            }
        }

        ifcl->cl_if.if_ipackets++;
        ifcl->cl_if.if_ibytes += length;

        /*
         * Init ifqueue header. This will hose the ifcl_hdr
         */

        IF_INITHEADER(mtod(m, caddr_t), &ifcl->cl_if,
		sizeof(ifclbufhead_t));

        /*
         * Check packet type here
         */

        switch (ifcl_hdr->ich_type) {
        case ETHERTYPE_IP: {
            if (network_input(m, AF_INET, 0)) {
                ifcl->cl_if.if_iqdrops++;
                ifcl->cl_if.if_ierrors++;
                DCMN_ERR2((CE_NOTE, "ifcl_xpc_async: network_input failed\n"));
            }
            break;
	}
        case ETHERTYPE_SG_RESV: {
            ifcl_ctrl_input(ifcl, m);
            break;
	}
        default: {
            ifcl->cl_if.if_iqdrops++;
            ifcl->cl_if.if_ierrors++;
            DCMN_ERR2((CE_NOTE, "ifcl_xpc_async: unrecognized ETHERTYPE\n"));
            break;
        }
	}

        DCMN_ERR2((CE_NOTE, "ifcl_xpc_async: end\n"));

        break;
    }
    case xpcConnected: {
        /*
         * Async connect callout
         */
        DCMN_ERR1((CE_NOTE, "ifcl_xpc_async: xpcConnected: pttn %d\n", pttn));

        IFCL_LOCK(&ifcl->cl_mutex, s);

        ifcl->cl_flags |= IFCL_READY;
        ifcl->cl_xpttn[pttn].icxp_xpchan = xpr;
        ifcl->cl_xpttn[pttn].icxp_flags |= IFCL_XP_UP;

        IFCL_UNLOCK(&ifcl->cl_mutex, s);

        break;
    }
    case xpcTimeout:
    case xpcError: {
        DCMN_ERR1((CE_NOTE, "ifcl_xpc_async: Received %d xpc\n", xpc));
        break;
    }
    default: {
        DCMN_ERR1((CE_ALERT, "ifcl_xpc_async: Unknown async message %d "
		   "received!\n", xpc));
        break;
    }
    }
}


/* ifcl_slist
 * ----------
 * Print out slist
 */
void
ifcl_slist(ifcl_slist_t *s)
{
    int		i;

    if (s->first == s->last) {
        return;
    }

    if (s->first < s->last) {
        for (i = s->first; i < s->last; i++) {
            qprintf("\t\tEntry %d: nflag 0x%x mbuf 0x%lx pttn %d\n", i, 
		s->nflag[i], s->slist[i], s->pttn[i]);
	}
    }
    else {
        for (i = s->first; i < CL_MAX_OUTQ; i++) {
            qprintf("\t\tEntry %d: nflag 0x%x mbuf 0x%lx pttn %d\n", i, 
		s->nflag[i], s->slist[i], s->pttn[i]);
	}
        for (i = 0; i < s->last; i++) {
            qprintf("\t\tEntry %d: nflag 0x%x mbuf 0x%lx pttn %d\n", i, 
		s->nflag[i], s->slist[i], s->pttn[i]);
	}
    }
}

/* ifcl_dump
 * ---------
 * general idbg function dump for ifcl module
 */
void
ifcl_dump(void)
{
    int		unit;

    qprintf("IFCL DATA: ifcl_num_unit %d\n", ifcl_num_unit);

    for (unit = 0; unit < ifcl_num_unit; unit++) {
        ifcl_t		*ifcl;
        partid_t	pttn;

        if (!(ifcl = ifcl_dev[unit])) {
            continue;
	}

        qprintf("\tifcl: ifnet 0x%lx rawif 0x%lx\n", &ifcl->cl_if, &ifcl->cl_rawif);
        qprintf("\tifcl: name %s unit %d flags 0x%x type %d\n",
		ifcl->cl_if.if_name, ifcl->cl_if.if_unit,
                ifcl->cl_flags, ifcl->cl_if.if_type);
        qprintf("\tifcl: mtu %d if_flags 0x%x outq %d\n",
		ifcl->cl_if.if_mtu, ifcl->cl_if.if_flags,
                ifcl->cl_if.if_snd.ifq_maxlen);

        qprintf("\tifcl: slist 0x%lx first %d last %d first_wrap %d last_wrap "
		"%d\n", &ifcl->cl_slist, ifcl->cl_slist.first,
		ifcl->cl_slist.last, ifcl->cl_slist.first_wrap,
		ifcl->cl_slist.last_wrap);

        qprintf("\tifcl: Sendlist:\n");
        ifcl_slist(&ifcl->cl_slist);

        qprintf("\n");

        for (pttn = 0; pttn < MAX_PARTITIONS; pttn++) {
            if (ifcl->cl_xpttn[pttn].icxp_flags & IFCL_XP_UP) {
                qprintf("\t\tifcl: pttn %d flags 0x%x xpchan 0x%lx\n", pttn,
		ifcl->cl_xpttn[pttn].icxp_flags,
		ifcl->cl_xpttn[pttn].icxp_xpchan);
	    }
	}
    }
}

/* ifcl_snarp_dump
 * ---------------
 * dump the snarp table
 */
void
ifcl_snarp_dump(void)
{
    int		i;

    qprintf("IFCL SNARP DUMP:\n");

    for (i = 0; i < MAX_PARTITIONS; i++) {
        if (snarptab[i].snarp_flags & SNARP_VALID) {
            qprintf("entry %d: in_addr 0x%x: addr %s\n", i, 
		snarptab[i].snarp_inaddr.s_addr, snarptab[i].snarp_addr);
	}
    }
}

#if IFCL_LOG

/* dump_re
 * -------
 * dump a receive log entry
 */
static void
dump_re(re_t *re)
{
    int         j;

    qprintf("chan %d: cpu %d: kth 0x%lx\n", re->chan, re->cpu,
                re->kthread);

    qprintf("\t\tSEND\n");
    for (j = 0; j < MAX_ADDRS_IN_MESG; j++) {
        qprintf("\t\t\t%d: type %s: cnt %d: addr 0x%lx\n", j,
                (re->type[j] == IMD ? "Imd" : "Ptr"),
                re->scnt[j], re->saddr[j]);
    }

    qprintf("\t\tRECV\n");
    for (j = 0; j < MAX_ADDRS_IN_MESG; j++) {
        qprintf("\t\t\t%d: type %s: cnt %d: addr 0x%lx\n", j,
                (re->type[j] == IMD ? "Imd" : "Ptr"),
                re->rcnt[j], re->raddr[j]);
    }
}

/* ifcl_recv_log_dump
 * ------------------
 * dump the whole receive log
 */
void
ifcl_recv_log_dump(int start, int n)
{
    int         i, cnt = 0;

    qprintf("ifcl_recv_log: ptr %d: wraps %d: num_exc %d\n", ifcl_recv_log.ptr,
                ifcl_recv_log.wraps, ifcl_recv_log.num_exc);

    for (i = start; (i >= 0) && (++cnt < n); i--) {
        qprintf("\n\tENTRY %d ", i);
        dump_re(&ifcl_recv_log.re[i]);
    }
}

/* dump_se
 * -------
 * dump a send entry in the send log
 */
static void
dump_se(se_t *se)
{
    int         j;

    qprintf("cpu %d: outsd %d: kth 0x%lx\n", se->cpu, se->outsd, se->kthread);

    qprintf("\t\tSEND\n");
    for (j = 0; j < MAX_ADDRS_IN_MESG; j++) {
        qprintf("\t\t\t%d: cnt %d: addr 0x%lx\n", j,
                se->cnt[j], se->addr[j]);
    }
}

/* ifcl_send_log_dump
 * ------------------
 * dump whole send log 
 */
void
ifcl_send_log_dump(int start, int n)
{
    int         i, cnt = 0;

    qprintf("ifcl_send_log: ptr %d: wraps %d: num_exc %d\n", ifcl_send_log.ptr,
                ifcl_send_log.wraps, ifcl_send_log.num_exc);

    for (i = start; (i >= 0) && (++cnt < n); i--) {
        qprintf("\n\tENTRY %d ", i);
        dump_se(&ifcl_send_log.se[i]);
    }
}

#endif /* IFCL_LOG */

#endif /* SN0 */
