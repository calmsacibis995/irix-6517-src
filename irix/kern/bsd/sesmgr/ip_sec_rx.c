#ident "$Revision: 1.17 $"

#include <bstring.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/mac_label.h>
#include <sys/sesmgr.h>
#include <sys/t6rhdb.h>
#include <sys/tcpipstats.h>
#include <sys/ip_secopts.h>
#include <sys/sat.h>
#include <sys/sm_private.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>		/* needed for ip.h" */
#include <netinet/ip.h>
#include <netinet/ip_var.h>		/* needed for MAX_IPOPTLEN */

extern mac_label *mac_equal_equal_lp;
extern mac_label *mac_low_high_lp;

/* values used in the following table:
 *	0	illegal
 *	1	Reserved 1	(see RFC 1108)
 *	2	Unclassified	
 *	3	Reserved 2
 *	4	Reserved 3
 *	5	Confidential
 *	6	Secret
 *	7	Top Secret
 *	8	Reserved 4
 */
static u_char 
ripso_lvl_valid[256] = {
	0, 8, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 7, 0, 0,

	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 6, 0,   0, 0, 0, 0,
	0, 0, 0, 0,   0, 0, 4, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,

	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0, 0, 0, 0,   0, 0, 5, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 2,   0, 0, 0, 0,
	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,

	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   3, 0, 0, 0,
	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
	0, 1, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0
};

/* This table is the one that will be replaced with customer-selected
 * values.  It has 8 values, corresponding to the 8 legal levels (1-8)
 * defined in the previous table.  These are the default values that
 * the customer gets unless he choses to replace them.
 */
static u_char 
level_rtoi_tab[8] = {
	0, 1<<5, 2<<5, 3<<5, 4<<5, 5<<5, 6<<5, 255	/* default values */
};


/* 
 * This function is used to determine if the incomming ip security mac
 * label is within the minimum and maximum t6rhdb range.
 */
static int
ip_rcv_mac_inrange(
	struct ipsec	  *sec_attrs,	/* security attributes	*/
	struct t6rhdb_kern_buf *t6p	/* tsix rhdb entry */
	)
{
	msen_t	msen_val;
	mint_t	mint_val;
	mac_t	mac_hgh;   /* mac high label range*/
	mac_t	mac_low;   /* mac low label range */
	mac_t	macp;      /* mac label for ip security */
	int	retval;    /* return value */
	
	if ( msen_valid( t6p->hp_max_sl ) )
		msen_val = t6p->hp_max_sl;
	else
		msen_val = msen_from_mac(mac_equal_equal_lp);

	if ( mint_valid( t6p->hp_max_integ ) )
		mint_val = t6p->hp_max_integ;
	else
		mint_val = mint_from_mac(mac_equal_equal_lp);
		
	mac_hgh = mac_from_msen_mint( msen_val, mint_val);


	if ( msen_valid( t6p->hp_min_sl ) )
		msen_val = t6p->hp_min_sl;
	else
		msen_val = msen_from_mac(mac_equal_equal_lp);
	
	if ( mint_valid( t6p->hp_min_integ ) )
		mint_val = t6p->hp_min_integ;
	else
		mint_val = mint_from_mac(mac_equal_equal_lp);
	
	mac_low = mac_from_msen_mint( msen_val, mint_val);
	
	macp = mac_from_msen_mint( 
		sa_sm_rcv.sa_msen, sa_sm_rcv.sa_mint );

	retval = mac_inrange( mac_low, macp, mac_hgh);
	
	return (retval ? IP_SECOPT_PASSED : IP_SECOPT_FAILED);
} /* end ip_rcv_mac_inrange */


/* The BSO and ESO options are *not* mutually exclusive.  The label produced
 * by processing a BSO option may be modified by an ESO option, and vice-versa.
 * The BSO code below will need to be modified when ESO code is added.
 * Returns 1 if caller should place label into cache, zero otherwise.
 * NOTE: must be called at splnet or eqivalent.
 */
static int
ip_rcv_bso( 
	register u_char	  *cp,
	struct ipsec	  *sec_attrs,	/* security attributes	*/
	struct t6rhdb_kern_buf *t6p	/* t6rhdb pointer	*/
	)
{
	int	optlen;		/* length of entire option	*/
	int	level;		/* internal sensitivity level 	*/
        msen_label      b_msen;
      
	if ( ip_sec_get_rcv( cp, sec_attrs ) )
		return (IP_SECOPT_PASSED);

	optlen = cp[IPOPT_OLEN];
	if ( optlen != BSO_MIN_LEN )
		return (IP_SECOPT_FAILED);	/* error */

	level = ripso_lvl_valid[ cp[ RIPSOPT_LVL ] ];

        if ( msen_valid( t6p->hp_def_sl ) )
		sa_sm_rcv.sa_msen = t6p->hp_def_sl;
	else {
        	bzero( &b_msen, sizeof( msen_label ) );
                b_msen.b_type = MSEN_LOW_LABEL;
		b_msen.b_hier = level_rtoi( level - 1 );
		sa_sm_rcv.sa_msen = msen_add_label( (msen_t) &b_msen );
	}

        if ( mint_valid( t6p->hp_def_integ ) )
		sa_sm_rcv.sa_mint = t6p->hp_def_integ;
	else
                sa_sm_rcv.sa_mint = mint_from_mac(mac_low_high_lp);

	sa_sm_rcv.sa_ids.uid = t6p->hp_def_uid;
		
	sa_sm_rcv.sa_mask |= ( T6M_UID | T6M_SL | T6M_INTEG_LABEL );
	
	/*
	 * Verify mac label is between maximum and
	 * minimum label range.  According to James
	 * this compare is not needed, but I am keeping 
	 * it for know.
	 */
	if ( ip_rcv_mac_inrange( sec_attrs, t6p) )
		return (IP_SECOPT_FAILED);

	ip_sec_add_cache( cp, sec_attrs, t6p);

	return (IP_SECOPT_PASSED);
	
} /* end ip_rcv_bso */


static int
ip_rcv_eso( void )
{
	return (IP_SECOPT_FAILED);
} /* end ip_rcv_eso */


static struct mac_b_label *
ip_rcv_tt1( 
        register u_char	*ttp,		/* tag type pointer	*/
        register struct t6rhdb_kern_buf *t6p	/* t6rhdb pointer	*/
        )
{
	u_char	*bitp;
	u_short *lp;		/* cat/div list pointer */
	int	bits;		/* octet bits */
	int	bitpos;		/* bit position */
	int	bitno = 0;
	struct	mac_b_label b_label;
	int	bytes = ttp[ CIPSOTAG_LEN ] - CIPSOTT1_BITMAP;
        u_int	ttval = ttp[ CIPSOTAG_VAL ];	/* tag type option */;

	/* 
	 * alignment octet must be zero. If not it could
	 * be Pre-CIPSO Ver 2.3.  We do not support it.
	 * BTW, CIPSO did not become an RFC.
	 */
	if ( ttp[ CIPSOTAG_ALIGN ] != 0)
		return (struct mac_b_label *) 0;

	bzero( &b_label, sizeof( struct mac_b_label ) );
	
	switch ( ttval ) {
	
		case CIPSO_TAG_TYPE_1:
			if ( msen_valid( t6p->hp_def_sl ) )
				return( t6p->hp_def_sl );

			b_label.b_type = MSEN_TCSEC_LABEL;
			break;

		case SGIPSO_BITMAP:
			if ( mint_valid( t6p->hp_def_integ ) )
				return( t6p->hp_def_integ );

			b_label.b_type = MINT_BIBA_LABEL;
			break;
		default:
			return (struct mac_b_label *) 0;
	}

	b_label.b_hier = ttp[ CIPSOTAG_LVL ];			

	/*
	 * according to the RFC we can have
	 * no categories. But we still need
	 * msen and level.
	 */

	if ( ! bytes ) {
		return ( ( ttval == CIPSO_TAG_TYPE_1 ) ?
			 (struct mac_b_label *) msen_add_label( (msen_t) &b_label) :
			 (struct mac_b_label *) mint_add_label( (mint_t) &b_label) );
	}

	if ( bytes > MAC_MAX_SETS )
		bytes = MAC_MAX_SETS;
	/*
	 * Location of bit maps
	 */
	bitp  = &ttp[ CIPSOTT1_BITMAP ];
				
	for (	lp = (u_short *) &b_label.b_list;
		--bytes;
		bitno += 8 ) {
		
 		bits = (int) *bitp++;
		for ( bitpos = 0; bits; ++bitpos ) {
			if ( bits & 0x80 )
				*lp++ = bitno + bitpos;
				
			bits <<= 1;
		}
 	}
 
 	b_label.b_nonhier = (lp - (u_short *) &b_label.b_list) / sizeof(u_short);

	return ( ( ttval == CIPSO_TAG_TYPE_1 ) ?
		 (struct mac_b_label *) msen_add_label( (msen_t) &b_label) :
		 (struct mac_b_label *) mint_add_label( (mint_t) &b_label) );

} /* end ip_rcv_tt1 */


static struct mac_b_label *
ip_rcv_tt2( 
        register u_char	*ttp,		/* tag type pointer	*/
        register struct t6rhdb_kern_buf *t6p	/* t6rhdb pointer	*/
        )
{
	u_short	*bp;
	ushort	*lp;		/* cat/div list pointer */
	u_short	enums[ CIPSOTT2_MAX_ENUM ];
	struct	mac_b_label b_label;
	int	bytes = ttp[ CIPSOTAG_LEN ] - CIPSOTT2_ENUMS;
        u_int	ttval = ttp[ CIPSOTAG_VAL ];	/* tag type option */;

	/* 
	 * alignment octet must be zero. If not it could
	 * be Pre-CIPSO Ver 2.3.  We do not support it.
	 * BTW, CIPSO did not become an RFC.
	 */
	if ( ttp[ CIPSOTAG_ALIGN ] != 0)
		return (struct mac_b_label *) 0;

	bzero( &b_label, sizeof( struct mac_b_label ) );
	
	switch ( ttval ) {
	
		case CIPSO_TAG_TYPE_2:
			if ( msen_valid( t6p->hp_def_sl ) )
				return( t6p->hp_def_sl );
			
			b_label.b_type = MSEN_TCSEC_LABEL;
			break;
		case SGIPSO_ENUM:
			if ( mint_valid( t6p->hp_def_integ ) )
				return( t6p->hp_def_integ );

			b_label.b_type = MINT_BIBA_LABEL;
			break;
		default:
			return (struct mac_b_label *) 0;
	}

	/*
	 * according to the RFC we can have
	 * no categories. But we still need
	 * msen and level.
	 */

	b_label.b_hier = ttp[ CIPSOTAG_LVL ];

	if ( bytes < 2 ) {
		return ( ( ttval == CIPSO_TAG_TYPE_2 ) ?
			 (struct mac_b_label *) msen_add_label( (msen_t) &b_label) :
			 (struct mac_b_label *) mint_add_label( (mint_t) &b_label) );
	}
		
	if ( bytes > MAC_MAX_SETS )
		bytes = MAC_MAX_SETS;

	bp = (u_short *) &ttp[ CIPSOTT2_ENUMS ];
	bzero ( enums, CIPSOTT2_MAX_ENUM);
	bcopy ( bp, enums, bytes );
	
	/* 
	 * number of enumerated categories in the CIPSO packet 
	 */
	bytes /= sizeof( u_short );
	b_label.b_nonhier = bytes;

	/*
	 * Start of category list
	 */
	lp = (u_short *) &b_label.b_list;	
	
	for ( bp = enums; --bytes; ++bp)
		*lp++ = ntohs(*bp);

	return ( ( ttval == CIPSO_TAG_TYPE_2 ) ?
		 (struct mac_b_label *) msen_add_label( (msen_t) &b_label) :
		 (struct mac_b_label *) mint_add_label( (mint_t) &b_label) );
	
} /* end ip_rcv_tt2 */


/*
 * Find SGIPSO DAC tag and fill in socket attributes.
 */
static uid_t
ip_rcv_sgipso_dac(
        u_char		  *ttp		/* tag type pointer	*/
        )
{
	ushort	dac_uid = 0;

	/* 
	 * Assumptions: (1) We are big endian.
	 *
	 * I assume the above comment means ntohs() 
	 * function is not needed for uid.
	 */
	bcopy( &ttp[ SGIPSO_UID ], (ushort *) &dac_uid, sizeof(ushort) );

	return( (uid_t) dac_uid );
	
} /* end ip_recv_sgipso_dac */

/*
 * SGIPSO Special MSEN/MINT extended tag type
 */
static int
ip_rcv_sgipso_special( 
        u_char		  *ttp,		/* tag type pointer	*/
        struct ipsec      *sec_attrs,   /* security attributes	*/
        register struct t6rhdb_kern_buf *t6p    /* t6rhdb pointer       */
        )
{
	msen_label	b_msen;
	mint_label	b_mint;
	
	bzero( &b_msen, sizeof( msen_label ) );
	bzero( &b_mint, sizeof( mint_label ) );

	if ( ! ( b_msen.b_type = ttp[ SGIPSO_MSEN ] ) )
		b_msen.b_type = MSEN_TCSEC_LABEL;

	if ( ! ( b_mint.b_type = ttp[ SGIPSO_MINT ] ) )
		b_mint.b_type = MINT_BIBA_LABEL;

	sa_sm_rcv.sa_msen = msen_add_label( (msen_t) &b_msen);
	sa_sm_rcv.sa_mint = mint_add_label( (mint_t) &b_mint);
	sa_sm_rcv.sa_mask |= ( T6M_SL | T6M_INTEG_LABEL );

        /*
         * Verify mac label is between maximum and
         * minimum label range.  According to James
         * this compare is not needed, but I am keeping 
         * it for know.
         */
        if ( ip_rcv_mac_inrange( sec_attrs, t6p) )
                return (IP_SECOPT_FAILED);

	return (IP_SECOPT_PASSED);
	
} /* end ip_rcv_sgipso_special */


/*
 * Fill in missing fields and validate 
 * socket attributes
 */
static int
ip_rcv_cipso_valid ( 
        struct ipsec      *sec_attrs,	/* security attributes	*/
        struct t6rhdb_kern_buf *t6p	/* t6rhdb pointer       */
        )
{

        if ( ! (sa_sm_rcv.sa_mask & T6M_SL) ) {
		if ( msen_valid( t6p->hp_def_sl ) )
			sa_sm_rcv.sa_msen = t6p->hp_def_sl;
		else
			sa_sm_rcv.sa_msen = 
				msen_from_mac(mac_low_high_lp);
			
		sa_sm_rcv.sa_mask |= T6M_SL;
	}
	else if ( ! sa_sm_rcv.sa_msen )
		return (IP_SECOPT_FAILED);

        if ( ! (sa_sm_rcv.sa_mask & T6M_INTEG_LABEL) ) {
		if ( mint_valid( t6p->hp_def_integ ) )
			sa_sm_rcv.sa_mint = t6p->hp_def_integ;
		else
			sa_sm_rcv.sa_mint = 
				mint_from_mac(mac_low_high_lp);
			
		sa_sm_rcv.sa_mask |= T6M_INTEG_LABEL;
	}
	else if ( ! sa_sm_rcv.sa_mint )
		return (IP_SECOPT_FAILED);

	/*
	 * Verify mac label is between maximum and
	 * minimum mac labels.
	 */
	if ( ip_rcv_mac_inrange( sec_attrs, t6p ) )
		return (IP_SECOPT_FAILED);

        if ( ! (sa_sm_rcv.sa_mask & T6M_UID) )
		sa_sm_rcv.sa_ids.uid = t6p->hp_def_uid;

	return (IP_SECOPT_PASSED);
} /* end ip_rcv_cipso_valid */


/*
 * Parse the security options in a received CIPSO datagram.
 */
static int
ip_rcv_cipso( 
        u_char		  *cp,		/* beginning of CIPSO packet*/
        struct ipsec      *sec_attrs,	/* security attributes	*/
        struct t6rhdb_kern_buf *t6p	/* t6rhdb pointer       */
        )
{
        u_char	*ttp;	/* tag type pointer */
        int	cnt;	
        int	optlen;	/* length of entire option	*/
        u_int	ttval;	/* CIPSO tag type value		*/
        int	ttlen;	/* CIPSO tag type length	*/

	if ( ip_sec_get_rcv( cp, sec_attrs ) == IP_SECOPT_PASSED)
		return (IP_SECOPT_PASSED);

        /* 
         * total length of CIPSO header and tag type options
         */
        optlen = cp[ CIPSO_LGH ];
        
        /* 
         * verify the length of the CIPSO tag type option 
         */
        if ( optlen < CIPSO_MIN_LEN || optlen > CIPSO_MAX_LEN )
                return (IP_SECOPT_FAILED);

        /* 
         * Address of first CIPSO/SGIPSO Tag Type 
         */
        ttp = &cp[ CIPSO_TAG ];
	cnt = (optlen - CIPSO_TAG);
        
        for (	; 
        	cnt > 0; 
        	cnt -= ttlen, ttp += ttlen ) {

		/*
		 * Check tag type length.
		 */
                ttlen	= ttp[ CIPSOTAG_LEN ];	/* tag type length */
		if (	( ttlen < CIPSOTAG_MIN_LGH ) || 
			( ttlen > CIPSOTAG_MAX_LGH ) )		
			continue;
        	
                ttval	= ttp[CIPSOTAG_VAL];	/* tag type option */
                     
		switch ( ttval ) {
			/* 
			 * CIPSO Bit Map of Categories (MSEN) 
			 */
			case CIPSO_TAG_TYPE_1:

				if ( sa_sm_rcv.sa_mask & T6M_SL )
					break;

				if ( ! (sa_sm_rcv.sa_msen = ip_rcv_tt1( ttp, t6p ) ) ) {
					if ( msen_valid( t6p->hp_def_sl ) )
						sa_sm_rcv.sa_msen = t6p->hp_def_sl;
					else
						return (IP_SECOPT_FAILED);
				}

                		sa_sm_rcv.sa_mask |= T6M_SL;

	                        break;

			/* 
			 * CIPSO Enumerated Categories (MSEN) 
			 */
			case CIPSO_TAG_TYPE_2:

				if ( sa_sm_rcv.sa_mask & T6M_SL )
					break;

				if ( ! (sa_sm_rcv.sa_msen = ip_rcv_tt2( ttp, t6p ) ) ) {
					if ( msen_valid( t6p->hp_def_sl ) )
						sa_sm_rcv.sa_msen = t6p->hp_def_sl;
					else
						return (IP_SECOPT_FAILED);
				}

                		sa_sm_rcv.sa_mask |= T6M_SL;

	                        break;

			/* 
			 * SGIPSO Bit Map of Division (MINT) 
			 */
	                case SGIPSO_BITMAP:

				if ( sa_sm_rcv.sa_mask & T6M_INTEG_LABEL )
					break;

				if ( ! (sa_sm_rcv.sa_mint = ip_rcv_tt1( ttp, t6p ) ) ) {
					if ( mint_valid( t6p->hp_def_integ ) )
						sa_sm_rcv.sa_mint = t6p->hp_def_integ;
					else
						return (IP_SECOPT_FAILED);
				}

                		sa_sm_rcv.sa_mask |= T6M_INTEG_LABEL;

	                        break;
			/* 
			 * SGIPSO Enumerated Division (MINT)
			 */
	                case SGIPSO_ENUM:

				if ( sa_sm_rcv.sa_mask & T6M_INTEG_LABEL )
					break;

				if ( ! (sa_sm_rcv.sa_mint = ip_rcv_tt2( ttp, t6p ) ) ) {
					if ( mint_valid( t6p->hp_def_integ ) )
						sa_sm_rcv.sa_mint = t6p->hp_def_integ;
					else
						return (IP_SECOPT_FAILED);
				}

                		sa_sm_rcv.sa_mask |= T6M_INTEG_LABEL;

	                        break;
	                
			/* 
			 * SGIPSO Uid Tag Type
			 */
			case SGIPSO_DAC:

				if ( sa_sm_rcv.sa_mask & T6M_UID )
					break;

				sa_sm_rcv.sa_ids.uid = ip_rcv_sgipso_dac( ttp );
                		sa_sm_rcv.sa_mask |= T6M_UID;

	                        break;
	                
			/* 
			 * SGIPSO Special Tag Type
			 */
	                case SGIPSO_SPECIAL:

				if ( sa_sm_rcv.sa_mask & 
					( T6M_SL | T6M_INTEG_LABEL ) )
					break;

                		if ( ip_rcv_sgipso_special( ttp, sec_attrs, t6p) )
                			return (IP_SECOPT_FAILED);

	                        break;
	                
	                /*
	                 * Unknown CIPSO/SGIPSO Tag Type, ignore them.
                         */
	                default:
				break;
	                
		} /*end switch */
	} /*end for */
	
	/*
	 * Fill in missing fields and validate 
	 * attributes
	 */
	if ( ip_rcv_cipso_valid( sec_attrs, t6p ) )
		return (IP_SECOPT_FAILED);

	ip_sec_add_cache( cp, sec_attrs, t6p );
	
	return (IP_SECOPT_PASSED);
} /* end ip_rcv_cipso */


static int
ip_rcv_unlabeled(
        struct ipsec      *sec_attrs,   /* security attributes  */
	struct t6rhdb_kern_buf *t6p	/* t6rhdb pointer       */
        )
{

        if ( msen_valid( t6p->hp_def_sl ) )
		sa_sm_rcv.sa_msen = t6p->hp_def_sl;
	else
                sa_sm_rcv.sa_msen = msen_from_mac(mac_low_high_lp);


        if ( mint_valid( t6p->hp_def_integ ) )
		sa_sm_rcv.sa_mint = t6p->hp_def_integ;
	else
                sa_sm_rcv.sa_mint = mint_from_mac(mac_low_high_lp);

	sa_sm_rcv.sa_ids.uid = t6p->hp_def_uid;

	sa_sm_rcv.sa_mask |= ( T6M_UID | T6M_SL | T6M_INTEG_LABEL );

        return (IP_SECOPT_PASSED);

} /* end ip_rcv_unlabeled */


/*
 * Look for security options in IP header.
 * Return pointer to corresponding internal mac_label, NULL otherwise.
 * Make third arg point to uid.
 */
static u_char *
ip_rcv_secopts(
	register struct mbuf	  *m
        )
{
	register u_char		*cp;
	register struct ip	*ip;
	int		cnt;
	int		opt;
	int		optlen;

	ip	= mtod(m, struct ip *);
	cp	= (u_char *) (ip + 1);
	cnt	= (ip->ip_hl << 2) - sizeof( struct ip);

	for (; cnt > 0; cnt -= optlen, cp += optlen) {

		opt = cp[IPOPT_OPTVAL];

		if (opt == IPOPT_EOL)
			return (u_char *) 0;

		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];

			if (optlen <= 0 || optlen > cnt) {
				IPSTAT (ips_badoptions);
				return (u_char *) 0;
			}
		}

		switch (opt)	{

			case IPOPT_SECURITY:	/* RIPSO_BSO */
				return( cp );

			case IPOPT_ESO:		/* RIPSO-ESO */
				return( cp );

			case IPOPT_CIPSO:	/* CIPSO     */
				return( cp );

			default:
				break;
		}
	}
	return (u_char *) 0;
}

int
ip_sec_dooptions(
	register struct mbuf	  *m,
	register struct ipsec	  *sec_attrs,	/* security attributes	*/
        register struct t6rhdb_kern_buf *t6p	/* t6rhdb pointer	*/
        )
{
	register u_char		*cp;

    	ASSERT( sesmgr_enabled );
    	ASSERT( m != NULL );
    	ASSERT( sec_attrs != NULL );
    	ASSERT( t6p != NULL );

	if ( ( t6p->hp_nlm_type == T6RHDB_NLM_UNLABELED )   &&
	     ( t6p->hp_smm_type != T6RHDB_SMM_SINGLE_LEVEL ) ) {
			return (IP_SECOPT_PASSED);
	}

	if ( ! (cp = ip_rcv_secopts( m ) ) ) {
		if ( ip_rcv_unlabeled( sec_attrs, t6p ) )
				return (IP_SECOPT_FAILED);
		return (IP_SECOPT_PASSED);
	}

	switch (t6p->hp_nlm_type) {

		case T6RHDB_NLM_RIPSO_BSO:
		case T6RHDB_NLM_RIPSO_BSOR:
			if ( ip_rcv_bso( cp, sec_attrs, t6p) ) {
				IPSTAT (ips_badoptions);
				return (IP_SECOPT_FAILED);
			}

			break;

		case T6RHDB_NLM_RIPSO_ESO:
			if ( ip_rcv_eso( ) ) {
				IPSTAT (ips_badoptions);
                       	        return(IP_SECOPT_FAILED);
			}

			break;

		case T6RHDB_NLM_CIPSO:		/* CIPSO best effort */
		case T6RHDB_NLM_CIPSO_TT1:	/* CIPSO tag type 1 only */
		case T6RHDB_NLM_CIPSO_TT2:	/* CIPSO tag type 2 only */
		case T6RHDB_NLM_SGIPSO:		/* SGIPSO best effort with uid */
		case T6RHDB_NLM_SGIPSONOUID:	/* SGIPSO best effort w/o uid */
		case T6RHDB_NLM_SGIPSO_SPCL:	/* SGIPSO best effort w/o uid */
			if ( ip_rcv_cipso( cp, sec_attrs, t6p) ) {
				IPSTAT (ips_badoptions);
				return (IP_SECOPT_FAILED);
			}

			break;

		case T6RHDB_NLM_RIPSO_BSOT:
			if ( ip_rcv_unlabeled( sec_attrs, t6p ) )
				return (IP_SECOPT_FAILED);

			break;

		default:
			break;
	}

	/*
	 * Did it find ip security options.
	 */
	if ( ! sa_sm_rcv.sa_mask ) {
		IPSTAT (ips_badoptions);
		return (IP_SECOPT_FAILED);
	}

	return (IP_SECOPT_PASSED);
}
