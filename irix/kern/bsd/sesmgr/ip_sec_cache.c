/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "sys/types.h"
#include "sys/systm.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "sys/mac_label.h"

#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/tcp-param.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/tcpipstats.h>

#include <sys/sat.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <sys/sesmgr.h>
#include <sys/capability.h>
#include <sys/t6rhdb.h>
#include <sys/ip_secopts.h>
#include <sys/t6satmp.h>
#include <sys/sm_private.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>

/* 
 * Cache of mappings between internal mac pointer (mac_t), 
 * internal uid (uid_t) and ip security option (CIPSO, or SGIPSO)
 * representations of the mac and uid.
 * This is the structure of label and uid cache entries.
 * ip_sec_cache entries are kept in mbufs.
 */
typedef struct ip_sec_cache {
	msen_t	ips_msenp;		/* internal b_label MSEN pointer*/
	mint_t	ips_mintp;		/* internal b_label MINT pointer*/
	int	ips_nlm;		/* network linkable module type */
	uid_t	ips_uid;		/* internal form of uid		*/
} ip_sec_cache_t;

static lock_t	ip_sec_cache_lock;
static struct mbuf *ip_sec_cache_head = 0;

/*
 * Function used to get the mac pointer from the ipsec structure.
 */
mint_t
ip_sec_get_mint(
	register struct ipsec	  *sec_attrs	/* security attributes	*/
	)
{	
 	if ( ! sec_attrs )
		return ((mint_t) 0);

	if ( sa_sm_msg.sa_mask & T6M_INTEG_LABEL )
		return ( (mint_t) sa_sm_msg.sa_mint);

	if ( sa_sm_dflt.sa_mask & T6M_INTEG_LABEL )
		return ( (mint_t) sa_sm_dflt.sa_mint);

	if ( sec_attrs->sm_sendlabel )
		return ( (mint_t) mint_from_mac(sec_attrs->sm_sendlabel) );
	
	return ((mint_t) 0);
} /* end ip_sec_get_mint */


/*
 * Function used to get the msen pointer from the ipsec structure.
 */
msen_t
ip_sec_get_msen(
	register struct ipsec	  *sec_attrs	/* security attributes	*/
	)
{	
 	if ( ! sec_attrs )
		return ( (msen_t) 0 );

	if ( sa_sm_msg.sa_mask & T6M_SL ) 
		return ( (msen_t) sa_sm_msg.sa_msen );

	if ( sa_sm_dflt.sa_mask & T6M_SL )
		return ( (msen_t) sa_sm_dflt.sa_msen );

	if ( sec_attrs->sm_sendlabel )
		return ( (msen_t) msen_from_mac(sec_attrs->sm_sendlabel) );
	
	return ( (msen_t) 0 );
} /* end ip_sec_get_msen */


/*
 * Function used to get the uid from the ipsec structure.
 */
uid_t
ip_sec_get_uid_t(
	register struct ipsec	*sec_attrs	/* security attributes	*/
	)
{
	if ( sa_sm_msg.sa_mask & T6M_UID )
		return ( sa_sm_msg.sa_ids.uid);
		
	if ( sa_sm_dflt.sa_mask & T6M_UID )
		return ( sa_sm_dflt.sa_ids.uid);

	return ( sec_attrs->sm_uid );
} /* end ip_sec_get_uid_t */


/*
 * add a entry to the cache for an interface.
 */
int
ip_sec_add_cache( 
	register u_char	*cp,		/* address of ip security opts */
	register struct ipsec *sec_attrs,
        register struct t6rhdb_kern_buf *t6p
	)
{
	register struct mbuf	*m;
	register ip_sec_cache_t	*entry;
	int	optlen = cp[ IPOPT_OLEN ];
	msen_t	msenp;
	mint_t	mintp;
	int	cache_lock;
	
	if ( ! (msenp = ip_sec_get_msen( sec_attrs )) )
		return (IP_SECOPT_FAILED);

	if ( ! (mintp = ip_sec_get_mint( sec_attrs )) )
		return (IP_SECOPT_FAILED);

	if ( ( m = m_getclr(M_DONTWAIT, MT_SOOPTS) ) == 0)
		return (IP_SECOPT_FAILED);
	
	entry = mtod(m, ip_sec_cache_t *) + 3;
	entry->ips_msenp = msenp;
	entry->ips_mintp = mintp;
	entry->ips_nlm = t6p->hp_nlm_type;
	entry->ips_uid = ip_sec_get_uid_t( sec_attrs );
	bcopy (cp, entry - 3, m->m_len = optlen);

	/* put new entry on chain */
	cache_lock = mutex_spinlock (&ip_sec_cache_lock);
	m->m_next = ip_sec_cache_head;
	ip_sec_cache_head = m;
	mutex_spinunlock (&ip_sec_cache_lock, cache_lock);
	
	return (IP_SECOPT_PASSED);
}


/* 
 * search interface cache for matching mac pointer.
 */
struct mbuf *
ip_sec_get_xmt( 
	register struct ipsec *sec_attrs,
        register struct t6rhdb_kern_buf *t6p
	)
{
	register struct mbuf *m = 0;
	int	cache_lock;
	ip_sec_cache_t xmt_key, *entry;
	
	bzero(&xmt_key, sizeof(xmt_key));

	if ( ! (xmt_key.ips_msenp = ip_sec_get_msen( sec_attrs )) )
		return m;
		
	if ( ! (xmt_key.ips_mintp = ip_sec_get_mint( sec_attrs )) )
		return m;

	xmt_key.ips_nlm	= t6p->hp_nlm_type;
	xmt_key.ips_uid	= ip_sec_get_uid_t( sec_attrs );

        cache_lock = mutex_spinlock (&ip_sec_cache_lock);

	for ( m = ip_sec_cache_head; m; m = m->m_next ) {
		entry = mtod(m, ip_sec_cache_t *) + 3;
		if ( !bcmp( &xmt_key, entry, sizeof(xmt_key))) {
			/* found cache entry */
			break;
		}
	}
        mutex_spinunlock (&ip_sec_cache_lock, cache_lock);

	return (m);
}


/*
 * search interface cache for match on external representation.
 */
int
ip_sec_get_rcv( 
	register u_char *cp,
	register struct ipsec *sec_attrs
	)
{
	register struct mbuf *   m;
	register ip_sec_cache_t	*entry;
	int	optlen = cp[IPOPT_OLEN];
        int     cache_lock;

        cache_lock = mutex_spinlock (&ip_sec_cache_lock);

	for ( m = ip_sec_cache_head; m; m = m->m_next ) {
		if ( optlen == m->m_len
		&& ( !bcmp( cp, mtod(m, u_char *), optlen))) {
			/* found matching RIPSO/CIPSO label in cache */
			break;
		}
	}

        mutex_spinunlock (&ip_sec_cache_lock, cache_lock);

	if ( m ) {
		entry = mtod(m, ip_sec_cache_t *) + 3;
		sa_sm_rcv.sa_msen = entry->ips_msenp;
		sa_sm_rcv.sa_mint = entry->ips_mintp;
		sa_sm_rcv.sa_ids.uid = entry->ips_uid;
		sa_sm_rcv.sa_mask |= ( T6M_SL | T6M_INTEG_LABEL | T6M_UID);

		return (IP_SECOPT_PASSED);
	}

	return (IP_SECOPT_FAILED);
}

/*
 * Initialize IP Security
 */
void
ip_sec_init(void)
{
	spinlock_init( &ip_sec_cache_lock, "ip_sec_cache_lock" );
}
