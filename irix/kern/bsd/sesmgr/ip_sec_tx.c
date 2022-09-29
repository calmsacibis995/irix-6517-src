#ident "$Revision: 1.17 $"

#include <bstring.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/sat.h>
#include <sys/mac_label.h>
#include <sys/sesmgr.h>
#include <sys/t6rhdb.h>
#include <sys/tcpipstats.h>
#include <sys/ip_secopts.h>
#include <sys/sm_private.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>	/* needed for ip.h */
#include <netinet/ip.h>
#include <netinet/ip_var.h>	/* needed for MAX_IPOPTLEN & struct ipstat */

extern char *inet_ntoa(struct in_addr);

#define MSEN 0                  /* Flag values for */
#define MINT 1                  /* cipso_add_msen_or_mint(). */
/*
 * This table maps from internal sensitivity level to BSO level.
 * A zero value in the table signifies an invalid BSO level.
 * This table is the inverse mapping of the table ripso_lvl_valid[].
 */
static u_char
level_itor_tab[256] = {
	0xf1, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0xab, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,

	0xcc, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0x66, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,

	0x96, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0x5a, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,

	0x3D, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0x01
};


/*
 * Separate IP header from old message into a new mbuf making, 
 * room for security options.
 *
 * Remove any old IP security options (e.g. RIPSO, CIPSO).
 * Assumes that the entire IP header, including any extant options, 
 * is all in one mbuf.
 */
static struct mbuf *
ip_xmt_secopt_merge( 
	register struct mbuf *m_org,	/* orginal mbuf		*/
	register struct mbuf *m_sec	/* security mbuf	*/
	)
{
	register u_char		*cp;
	register struct ip	*ip;
	register struct mbuf	*m_new;
	caddr_t		to;
	int		cnt;
	int		hlen;
	int		opt;
	int		optlen;
	int		pad_bytes;

	ip = mtod( m_org, struct ip *);
	hlen = ip->ip_hl << 2;

	if ( hlen > m_org->m_len ) {
		if ((m_org = m_pullup( m_org, hlen)) == 0) {
			IPSTAT ( ips_badhlen );
			return (struct mbuf *)0;
		}
		ip = mtod( m_org, struct ip *);
		hlen = ip->ip_hl << 2;
	}

	if ( ! ( m_new = m_getclr( M_DONTWAIT, MT_HEADER)) )
		return (struct mbuf *) 0;

	/*
	 * First seperate the ip structure from the
	 * transport structure (tcp, udp, raw) by
	 * putting it into another mbuf.  Then 
	 * remove any previously inserted ip security 
	 * options.
	 */
	m_new->m_flags |= ( m_org->m_flags & M_COPYFLAGS );
	m_new->m_off = MMAXOFF - ( sizeof(*ip) + MAX_IPOPTLEN );
	to = mtod(m_new, caddr_t);
	bcopy( ip, to, sizeof(*ip) );
	to += sizeof(*ip);

	cp   = (u_char *) (ip + 1);
	cnt  = hlen - sizeof (struct ip);

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];

			/* 
			 * A packet with bad options should never have 
			 * gotten this far.  It should have been caught
			 * in ip_intr() or ip_pcbopts().
			 */
			ASSERT( optlen > 0 && optlen <= cnt);
		}
		
		switch (opt) {

			/* 
			 * don't copy these
			 */
			case IPOPT_NOP:
			case IPOPT_SECURITY:
			case IPOPT_ESO:
			case IPOPT_CIPSO:
				break;

			default:
				bcopy( cp, to, optlen ); 
				to += optlen;
				break;
		}
	}

	/*
	 * Delete the orginal ip by moving the offset and
	 * length of the orginal mbuf.
	 */
	m_org->m_off += hlen;
	m_org->m_len -= hlen;
	
	/*
	 * Link orginal mbuf into new mbuf
	 */
	m_new->m_next = m_org;

	/*
	 * Point to the new mbuf ip header
	 */
        ip = mtod( m_new, struct ip *);
	
	/*
	 * The ip security option(s) in m_sec should
	 * fit into the ip options.  The functions creating
	 * the m_sec mbuf should have verified its size and
	 * should not be here if it will not fit. If it does
	 * not fit, the bug is in one of the functions
	 * that created m_sec.
	 */
	bcopy( mtod( m_sec, caddr_t), to, m_sec->m_len );

	m_new->m_len = (to + m_sec->m_len) - ip;
	ip->ip_len += m_sec->m_len;

	/*
	 * Pad the end of ip options if needed.
	 */
	if ( m_new->m_len & 3 ) {
		pad_bytes = 4 - ( m_new->m_len & 3 );
		m_new->m_len += pad_bytes;
		ip->ip_len += pad_bytes;
		cp = mpassd( m_new, u_char *);
		while ( pad_bytes-- )
			*cp++ = IPOPT_EOL;
	}

	/*
	 * Length of ip header and ip options in 4-byte unites
	 */
	ip->ip_hl  = m_new->m_len >> 2;

	return m_new;
} /* end ip_xmt_secopt_merge */


static struct mbuf *
ip_xmt_bso( 
	register struct ipsec *sec_attrs	/* security attributes	*/
        )
{
	register struct mbuf 		*m_sec;
	register struct ripso_bso	*bsop;
	register msen_t			msenp;

	if ( ! ( m_sec = m_getclr( M_DONTWAIT, MT_SOOPTS) ) )
		return (struct mbuf *) 0;

	bsop = mtod( m_sec, ripso_bso_t *);
	bsop->opttype	= IPOPT_SECURITY;
	bsop->optlength	= BSO_MIN_LEN;
	bsop->flags	= ( u_char ) NULL;

	msenp = ip_sec_get_msen( sec_attrs );
	if ( msenp )
		bsop->lvl	= level_itor(msenp->b_type);
	
	m_sec->m_len = BSO_MIN_LEN;
	
	return( m_sec );
} /* end ip_xmt_bso */


/*
 * Create an mbuf and format it with CIPSO options. 
 */
static struct mbuf *
ip_xmt_cipso_header( 
        register struct t6rhdb_kern_buf *t6p	/* t6rhdb pointer	*/
	)	
{
	register struct mbuf	*m_sec;
	register struct cipso_h	*cipsop;
	u_long			doi = 3; /* should come from the t6rhdb, fix */

	if ( ! ( m_sec = m_getclr(M_DONTWAIT, MT_SOOPTS) ) )
		return (struct mbuf *) 0;

	cipsop = mtod( m_sec, struct cipso_h *);
	cipsop->opttype   = IPOPT_CIPSO;
	cipsop->optlength = CIPSO_TAG;
	cipsop->doih      = htons( doi >> 16 );
	cipsop->doil      = htons( doi & 0xffff );

	m_sec->m_len = CIPSO_TAG;

	return m_sec;
} /* end ip_xmt_cipso_header */


/* 
 * Create Tag Type 1 (bit map) packet.
 */
static int
ip_xmt_tt1(
	u_int	 ttval,
	register struct mac_b_label *b_label,
	register struct cipso_tt1 *tt1
	)
{
	register u_short *lp;	/* list pointer	and end pointer	*/
	u_short ep = 0;
	u_short bitval;
	u_short bitlength = 0;

	tt1->tagtype	= ttval;
	tt1->taglength	= CIPSOTT1_BITMAP;
	
	/*
	 * If we don't have
	 * a b_label, don't send any. 
	 * That was to easy.
	 */
	if ( ! b_label )
		return (IP_SECOPT_PASSED);

	/*
	 * If we don't have
	 * a b_label, don't send any. 
	 * That was to easy.
	 */
	if ( b_label->b_nonhier > CIPSOTT1_MAX_BITS ) {
		/* Set tag type length to invalid length */
		tt1->taglength	= MAX_IPOPTLEN + 8;
		return (IP_SECOPT_FAILED);
	}
	
	tt1->level = b_label->b_hier;
	
	/*
	 * If we don't
	 * have any, don't send any. 
	 * That was easy.
	 */
	if ( b_label->b_hier <= 0 )
		return (IP_SECOPT_PASSED);
	
	lp = (u_short *) &b_label->b_list;

	do {
		bitval = *lp++;
		
		/*
		 * We cannot represent values greater
		 * then CIPSOTT1_MAX_VAL in Tag Type 1
		 * We cannot used Tag Type 1, Error.
		 */
		if ( bitval > CIPSOTT1_MAX_VAL ) {
			/* Set tag type length to invalid length */
			tt1->taglength	= MAX_IPOPTLEN + 8;
			return (IP_SECOPT_FAILED);
		}

		if ( bitval > bitlength )
			bitlength = bitval;

		tt1->bits[ bitval >> 3 ] |= 0x80 >> ( bitval & 7 );
				
	} while ( ++ep <= b_label->b_nonhier );
	
	tt1->taglength	+= ( bitlength + 8 / 8 );

	return (IP_SECOPT_PASSED);
} /* end ip_xmt_tt1 */


/*
 * Create Tag Type 2 (enumerated map) packet.
 */
static int
ip_xmt_tt2(
	u_int	 ttval,
	register struct mac_b_label *b_label,
	register struct cipso_tt2 *tt2
	)
{
	u_int	enumlength;		/* list pointer	and end pointer	*/

	tt2->tagtype	= ttval;
	tt2->taglength	= CIPSOTT2_ENUMS;

	/*
	 * If we don't have
	 * a b_label, don't send any. 
	 * That was to easy.
	 */
	if ( ! b_label )
		return (IP_SECOPT_PASSED);

	if ( b_label->b_nonhier > CIPSOTT2_MAX_ENUM ) {
		/* Set tag type length to invalid length */
		tt2->taglength	= MAX_IPOPTLEN + 8;
		return (IP_SECOPT_FAILED);
	}
	
	tt2->level	= b_label->b_hier;
		
	/*
	 * If we don't have
	 * any, don't send any. 
	 * That was easy.
	 */
	if ( b_label->b_nonhier <= 0 )
		return (IP_SECOPT_PASSED);
	
	enumlength = ( b_label->b_nonhier * sizeof( u_short ) );

	bcopy( &b_label->b_list, &tt2->cats, enumlength);
	tt2->taglength	+= enumlength;
	
	return (IP_SECOPT_PASSED);
} /* end ip_xmt_tt2 */


/*
 * CIPSO Tag Type 1 uses bit map only.  If the values
 * are greater then CIPSOTT1_MAX_VAL, then error. No
 * best fit.
 */
static int
ip_xmt_cipso_bits(
	register struct	ipsec	*sec_attrs,	/* security attributes	*/
	register struct	mbuf	*m_sec	/* mbuf security cache entry	*/
	)
{
	struct cipso_tt1 tt1;
	register struct cipso_h	*cipsop;
	struct mac_b_label *msenp;

	bzero( &tt1, sizeof( struct cipso_tt1 ) );
		
	msenp = ip_sec_get_msen( sec_attrs );

	if ( ip_xmt_tt1( CIPSO_TAG_TYPE_1, msenp, &tt1) )
		return (IP_SECOPT_FAILED);
		
	cipsop = mtod( m_sec, struct cipso_h *);

	if ( ( m_sec->m_len + tt1.taglength ) < MAX_IPOPTLEN ) {
		bcopy( &tt1, mpassd( m_sec, caddr_t), 
			tt1.taglength);
				
		m_sec->m_len += tt1.taglength;
		cipsop->optlength += tt1.taglength;
	}
	else
		return (IP_SECOPT_FAILED);

	return (IP_SECOPT_PASSED);
} /* end ip_xmt_cipso_bits */


/*
 * CIPSO Tag Type 2 uses enumerated maps only.  If there
 * are more then CIPSOTT2_MAX_ENUM, then error. No
 * best fit.
 */
static int
ip_xmt_cipso_enums(
	register struct	ipsec	*sec_attrs,	/* security attributes	*/
	register struct	mbuf	*m_sec	/* mbuf security cache entry	*/
	)
{
	struct cipso_tt2 tt2;
	register struct cipso_h	*cipsop;
	struct mac_b_label *msenp;

	bzero( &tt2, sizeof( struct cipso_tt2 ) );
		
	msenp = ip_sec_get_msen( sec_attrs );
	
	if ( ip_xmt_tt2( CIPSO_TAG_TYPE_2, msenp, &tt2) )
		return (IP_SECOPT_FAILED);

	cipsop = mtod( m_sec, struct cipso_h *);
		
	if ( ( m_sec->m_len + tt2.taglength ) < MAX_IPOPTLEN ) {
		bcopy( &tt2, mpassd( m_sec, caddr_t), 
			tt2.taglength);

		m_sec->m_len += tt2.taglength;
		cipsop->optlength += tt2.taglength;
	}
	else	
		return (IP_SECOPT_FAILED);
	
	return (IP_SECOPT_PASSED);
} /* end ip_xmt_cipso */


/*
 * CIPSO is a best fit, best effort style of ip security
 * packet. That means we do are best to make CIPSO tag
 * type 1 or tag type 2 fit into the m_sec mbuf.  If
 * we cannot, error.
 */
static int
ip_xmt_cipso(
	register struct	ipsec	*sec_attrs,	/* security attributes	*/
	register struct	mbuf	*m_sec	/* mbuf security cache entry	*/
	)
{
	struct cipso_tt1 tt1;
	struct cipso_tt2 tt2;
	register struct cipso_h	*cipsop;
	struct mac_b_label *msenp;

	bzero( &tt1, sizeof( struct cipso_tt1 ) );
	bzero( &tt2, sizeof( struct cipso_tt2 ) );
		
	msenp = ip_sec_get_msen( sec_attrs );
	
	ip_xmt_tt1( CIPSO_TAG_TYPE_1, msenp, &tt1);
	ip_xmt_tt2( CIPSO_TAG_TYPE_2, msenp, &tt2);
		
	if ( ( tt1.taglength > MAX_IPOPTLEN ) &&
	     ( tt2.taglength > MAX_IPOPTLEN ) )
		return ( IP_SECOPT_FAILED);

	cipsop = mtod( m_sec, struct cipso_h *);

	if ( ( tt1.taglength <= tt2.taglength ) &&
	     ( m_sec->m_len + tt1.taglength ) <= MAX_IPOPTLEN ) {
		bcopy(	&tt1, mpassd( m_sec, caddr_t), 
			tt1.taglength);

		m_sec->m_len += tt1.taglength;
		cipsop->optlength += tt1.taglength;
	}
	else if ( ( m_sec->m_len + tt2.taglength ) <= MAX_IPOPTLEN ) {
		bcopy(	&tt2, mpassd( m_sec, caddr_t), 
			tt2.taglength);

		m_sec->m_len += tt2.taglength;
		cipsop->optlength += tt2.taglength;
	}
	else	
		return (IP_SECOPT_FAILED);

	return (IP_SECOPT_PASSED);
} /* end ip_xmt_cipso */


/*
 * SGIPSO is a best fit, best effort style of ip security
 * packet. That means we do are best to make SGIPSO tag
 * type 1 or tag type 2 fit into the m_sec mbuf.  If
 * we cannot, error.
 */
static int
ip_xmt_sgipso(
	register struct	ipsec	*sec_attrs,	/* security attributes	*/
	register struct	mbuf	*m_sec	/* mbuf security cache entry	*/
	)
{
	struct cipso_tt1 tt1;
	struct cipso_tt2 tt2;
	register struct cipso_h	*cipsop;
	struct mac_b_label *mintp;

	if ( ip_xmt_cipso( sec_attrs, m_sec) )
		return (IP_SECOPT_FAILED);

	bzero( &tt1, sizeof( struct cipso_tt1 ) );
	bzero( &tt2, sizeof( struct cipso_tt2 ) );
	
	mintp = ip_sec_get_mint( sec_attrs );
	
	ip_xmt_tt1( SGIPSO_BITMAP, mintp, &tt1);
	ip_xmt_tt2( SGIPSO_ENUM  , mintp, &tt2);
		
	if ( ( tt1.taglength > MAX_IPOPTLEN ) &&
	     ( tt2.taglength > MAX_IPOPTLEN ) ) {
		return (IP_SECOPT_FAILED);
	}

	cipsop = mtod( m_sec, struct cipso_h *);

	if ( ( tt1.taglength <= tt2.taglength ) &&
	     ( m_sec->m_len + tt1.taglength ) <= MAX_IPOPTLEN ) {
		bcopy( &tt1, mpassd( m_sec, caddr_t), 
			tt1.taglength);
				
		m_sec->m_len += tt1.taglength;
		cipsop->optlength += tt1.taglength;
	}
	else if ( ( m_sec->m_len + tt2.taglength ) <= MAX_IPOPTLEN ) {
		bcopy(	&tt2, mpassd( m_sec, caddr_t), 
			tt2.taglength);

		m_sec->m_len += tt2.taglength;
		cipsop->optlength += tt2.taglength;
	}
	else
		return (IP_SECOPT_FAILED);
	
	return (IP_SECOPT_PASSED);
} /* end ip_xmt_sgipso */


/*
 * Create SGIPSO DAC (uid) Tag Type. CIPSO header should
 * already be in the m_sec mbuf.
 */
static int
ip_xmt_sgipso_uid(
	register struct	ipsec	*sec_attrs,	/* security attributes	*/
	register struct	mbuf	*m_sec		/* mbuf security cache entry	*/
	)
{
	register sgipso_dac_t	*dacp;
	register struct cipso_h	*cipsop;

	if ( ( m_sec->m_len + SGIPSO_DAC_LEN ) > MAX_IPOPTLEN )
		return( IP_SECOPT_FAILED );
	
	dacp = mpassd( m_sec, sgipso_dac_t *);

	dacp->tagtype	= SGIPSO_DAC;
	dacp->taglength	= SGIPSO_DAC_LEN;
	dacp->uid 	= (short) ip_sec_get_uid_t( sec_attrs );

	m_sec->m_len   += SGIPSO_DAC_LEN;
	cipsop = mtod( m_sec, struct cipso_h *);
	
	cipsop->optlength += SGIPSO_DAC_LEN;
	
	return (IP_SECOPT_PASSED);
} /* end ip_xmt_sgipso_uid */


static struct mbuf *
ip_xmt_sgipso_special( 
	register struct	ipsec	*sec_attrs,	/* security attributes	*/
	register struct	mbuf	*m_sec		/* mbuf security cache entry	*/
        )
{
	register struct cipso_h	*cipsop;
	register sgipso_special_t	*spclp;
	register msen_t			msenp;
	register mint_t			mintp;

	spclp			= mtod( m_sec, sgipso_special_t *);
	spclp->tagtype		= SGIPSO_SPECIAL;
	spclp->taglength	= SGIPSO_SPEC_LEN;

	msenp = ip_sec_get_msen( sec_attrs );
	spclp->msen = msenp->b_type;

	mintp = ip_sec_get_mint( sec_attrs );
	spclp->mint = mintp->b_type;
	
	m_sec->m_len   += SGIPSO_SPEC_LEN;

	cipsop = mtod( m_sec, struct cipso_h *);
	cipsop->optlength += SGIPSO_SPEC_LEN;
	
	return( m_sec );
} /* end ip_xmt_bso */


/*
 * This function identifies which CIPSO Tag Types to
 * create and cancatenates them one after the other.
 */
static struct mbuf *
ip_xmt_cipso_sgipso(
	register struct ipsec	  *sec_attrs,	/* security attributes	*/
        register struct t6rhdb_kern_buf *t6p	/* t6rhdb pointer	*/
	)	
{
	register struct mbuf	*m_sec;	/* mbuf security cache entry	*/

	if ( ! ( m_sec = ip_xmt_cipso_header( t6p) ) )
		return (struct mbuf *) 0;
	
	switch ( t6p->hp_nlm_type ) {
	
		/* SGIPSO best effort with uid */
		case T6RHDB_NLM_SGIPSO:
			if ( ip_xmt_sgipso( sec_attrs, m_sec) ) {
				m_free ( m_sec );
				return (struct mbuf *) 0;
			}
		
			if ( ip_xmt_sgipso_uid( sec_attrs, m_sec) ) {
				m_free ( m_sec);
				return (struct mbuf *) 0;
			}
			break;

		/* SGIPSO best effort w/o uid */
		case T6RHDB_NLM_SGIPSONOUID:
			if ( ip_xmt_sgipso( sec_attrs, m_sec) ) {
				m_free ( m_sec );
				return (struct mbuf *) 0;
			}
			break;
		
		/* CIPSO best effort */
		case T6RHDB_NLM_CIPSO:
			if ( ip_xmt_cipso( sec_attrs, m_sec) ) {
				m_free ( m_sec );
				return (struct mbuf *) 0;
			}
			break;
			
		/* CIPSO tag type 1 only */
		case T6RHDB_NLM_CIPSO_TT1:
			if ( ip_xmt_cipso_bits( sec_attrs, m_sec) ) {
				m_free ( m_sec );
				return (struct mbuf *) 0;
			}
			break;

		/* CIPSO tag type 2 only */
		case T6RHDB_NLM_CIPSO_TT2:
			if ( ip_xmt_cipso_enums( sec_attrs, m_sec) ) {
				m_free ( m_sec );
				return (struct mbuf *) 0;
			}
			break;

		/* SGIPSO Special */
		case T6RHDB_NLM_SGIPSO_SPCL:
			if ( ip_xmt_sgipso_special( sec_attrs, m_sec) ) {
				m_free ( m_sec );
				return (struct mbuf *) 0;
			}
			break;
		
		default:
			return (struct mbuf *) 0;
	}
	
	return( m_sec );
} /* end ip_xmt_cipso_sgipso */


/* ip_sec_insertopts - translate internal label into external label
 * strategy:
 * 1. separate the ip header from the rest of the datagram, and 
 *    strip out any old security options from the header
 * 2. construct the option to be sent based on the policy of the outgoing
 *	interface.
 * 3. See if the option is too big for the ip header. If so, punt this packet.
 * 4. If not too big, combine into header.
 * If any errors occur, free the input mbuf chain (m), and return null pointer.
 * Otherwise return pointer to (probably modified) message.
 * NOTE: must be called at splnet or eqivalent.
 */
struct mbuf *
ip_sec_insertopts(
	register struct mbuf	  *m,
	register struct ipsec	  *sec_attrs,	/* security attributes	*/
	register struct t6rhdb_kern_buf *t6p,	/* tsix rhdb entry	*/
	int		  *error
	)
{
	register struct mbuf	*m_sec;
	struct mbuf		*m_org = m;

    	if ( m_sec = ip_sec_get_xmt( sec_attrs, t6p) ) {
		if ( ! ( m = ip_xmt_secopt_merge( m, m_sec) ) ) {
				*error = EACCES;
				m_freem( m_org );
				return (struct mbuf *) 0;
		}
		return m;
	}

	switch ( t6p->hp_nlm_type ) {
	
		case T6RHDB_NLM_RIPSO_BSO:
		case T6RHDB_NLM_RIPSO_BSOR:
			m_sec = ip_xmt_bso( sec_attrs );
			break;

		case T6RHDB_NLM_CIPSO:
		case T6RHDB_NLM_CIPSO_TT1:
		case T6RHDB_NLM_CIPSO_TT2:
		case T6RHDB_NLM_SGIPSO:
		case T6RHDB_NLM_SGIPSONOUID:
		case T6RHDB_NLM_SGIPSO_SPCL:
			m_sec = ip_xmt_cipso_sgipso( sec_attrs, t6p);
			break;
					
		default:
			return ( struct mbuf *) 0;
	}

	/*
	 * Verify there is an ip security option mbuf.
	 */
	if ( ! m_sec ) {
		*error = EACCES;
		m_freem( m_org);
		return ( struct mbuf *) 0;
	}
	
	/*
	 * Seperate ip structure into its own mbuf and
	 * add ip security options (as well as any other
	 * options that might have been in there.)
	 */
	if ( ! ( m = ip_xmt_secopt_merge( m, m_sec) ) ) {
		*error = EACCES;
		m_free( m_sec );
		m_freem( m_org);
		return ( struct mbuf *) 0;
	}

	ip_sec_add_cache( mtod( m_sec, u_char *), sec_attrs, t6p);

	m_free( m_sec );
	
	return ( m );
}
