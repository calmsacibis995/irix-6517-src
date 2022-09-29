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

#include <bstring.h>
#include <sys/types.h>
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

extern char *inet_ntoa(struct in_addr);

static int
trace_setvalue(
	unsigned short *list,
	unsigned short count
	)
{
        int i;
        
	if (count <= 0)
        	return (IP_SECOPT_PASSED);

        for (i = 1; i < count ; i++) {
                if (list[i] <= list[i-1]) {
                	printf("Error: Category|Division Out of Order\n");
                        return (IP_SECOPT_FAILED);
		}
		printf(" %d", list[i-1]);
	}
	printf(" %d\n", list[i]);
        return (IP_SECOPT_PASSED);
}


static int
invalid_setvalue(
	unsigned char  level,
	unsigned short count
	)
{
	if (level != 0 || count > 0 ) {
		printf("Error: Level|Grade and Category|Division Count Set\n");
		return (IP_SECOPT_FAILED);
	}
	return (IP_SECOPT_PASSED);
}


static int
trace_sec_attr_mac(
	mac_label *lp
	)
{
        if (lp == NULL) {
        	printf("No MAC Pointer\n");
                return(IP_SECOPT_FAILED);
	}

        if ((lp->ml_catcount + lp->ml_divcount) > MAC_MAX_SETS) {
        	printf("Category|Division Count Greater Then Maximum\n");
                return(IP_SECOPT_FAILED);
	}

        switch (lp->ml_msen_type) {
		case MSEN_ADMIN_LABEL:
			printf("MSEN_ADMIN_LABEL\n");
	                invalid_setvalue(lp->ml_level, lp->ml_catcount);
			break;

		case MSEN_EQUAL_LABEL:
			printf("MSEN_EQUAL_LABEL\n");
	                invalid_setvalue(lp->ml_level, lp->ml_catcount);
			break;

		case MSEN_HIGH_LABEL:
			printf("MSEN_HIGH_LABEL\n");
	                invalid_setvalue(lp->ml_level, lp->ml_catcount);
			break;

		case MSEN_MLD_HIGH_LABEL:
			printf("MSEN_MLD_HIGH_LABEL\n");
	                invalid_setvalue(lp->ml_level, lp->ml_catcount);
			break;

		case MSEN_LOW_LABEL:
			printf("MSEN_LOW_LABEL\n");
	                invalid_setvalue(lp->ml_level, lp->ml_catcount);
			break;

		case MSEN_MLD_LOW_LABEL:
			printf("MSEN_MLD_LOW_LABEL\n");
	                invalid_setvalue(lp->ml_level, lp->ml_catcount);
			break;

		case MSEN_TCSEC_LABEL:
			printf("MSEN_TCSEC_LABEL\n");
			trace_setvalue(lp->ml_list, lp->ml_catcount);
			break;
			
		case MSEN_MLD_LABEL:
			printf("MSEN_MLD_LABEL\n");
			trace_setvalue(lp->ml_list, lp->ml_catcount);
			break;

		case MSEN_UNKNOWN_LABEL:
			printf("MSEN_UNKNOWN_LABEL\n");
	                invalid_setvalue(lp->ml_level, lp->ml_catcount);
			break;

		default:
			printf("Error: Sensitivity Label Not Set\n");
			return(IP_SECOPT_FAILED);
        }

	switch (lp->ml_mint_type) {
		case MINT_BIBA_LABEL:
			printf("MINT_BIBA_LABEL\n");
			trace_setvalue((lp->ml_list) + lp->ml_catcount,
                            	lp->ml_divcount);
			break;
		case MINT_EQUAL_LABEL:
			printf("MINT_EQUAL_LABEL\n");
	                invalid_setvalue(lp->ml_grade, lp->ml_divcount);
			break;
			
		case MINT_HIGH_LABEL:
			printf("MINT_HIGH_LABEL\n");
	                invalid_setvalue(lp->ml_grade, lp->ml_divcount);
			break;

		case MINT_LOW_LABEL:
			printf("MINT_LOW_LABEL\n");
	                invalid_setvalue(lp->ml_grade, lp->ml_divcount);
			break;

		default:
			printf("Error: Integerity Label Not Set\n");
			return(IP_SECOPT_FAILED);
        }

        return( IP_SECOPT_PASSED );
}


static int
trace_sec_attr_b(
	mac_b_label *b_label
	)
{
        if (b_label == NULL) {
        	printf("No b_label Pointer\n");
                return(IP_SECOPT_FAILED);
	}

        if ((b_label->b_nonhier) > MAC_MAX_SETS) {
        	printf("b_nonhier Count Greater then Maximum\n");
                return(IP_SECOPT_FAILED);
	}

        switch (b_label->b_type) {
		case MSEN_ADMIN_LABEL:
			printf("MSEN_ADMIN_LABEL hier=%c|0x%x\n",
				b_label->b_hier, b_label->b_hier);
	                invalid_setvalue(b_label->b_hier, b_label->b_nonhier);
			break;

		case MSEN_EQUAL_LABEL:
			printf("MSEN_EQUAL_LABEL hier=%c|0x%x\n",
				b_label->b_hier, b_label->b_hier);
	                invalid_setvalue(b_label->b_hier, b_label->b_nonhier);
			break;

		case MSEN_HIGH_LABEL:
			printf("MSEN_HIGH_LABEL hier=%c|0x%x\n",
				b_label->b_hier, b_label->b_hier);
	                invalid_setvalue(b_label->b_hier, b_label->b_nonhier);
			break;

		case MSEN_MLD_HIGH_LABEL:
			printf("MSEN_MLD_HIGH_LABEL hier=%c|0x%x\n",
				b_label->b_hier, b_label->b_hier);
	                invalid_setvalue(b_label->b_hier, b_label->b_nonhier);
			break;

		case MSEN_LOW_LABEL:
			printf("MSEN_LOW_LABEL hier=%c|0x%x\n",
				b_label->b_hier, b_label->b_hier);
	                invalid_setvalue(b_label->b_hier, b_label->b_nonhier);
			break;

		case MSEN_MLD_LOW_LABEL:
			printf("MSEN_MLD_LOW_LABEL hier=%c|0x%x\n",
				b_label->b_hier, b_label->b_hier);
	                invalid_setvalue(b_label->b_hier, b_label->b_nonhier);
			break;

		case MSEN_TCSEC_LABEL:
			printf("MSEN_TCSEC_LABEL hier=%c|0x%x\n",
				b_label->b_hier, b_label->b_hier);
			trace_setvalue(b_label->b_list, b_label->b_nonhier);
			break;
			
		case MSEN_MLD_LABEL:
			printf("MSEN_MLD_LABEL hier=%c|0x%x\n",
				b_label->b_hier);
			trace_setvalue(b_label->b_list, b_label->b_nonhier);
			break;

		case MSEN_UNKNOWN_LABEL:
			printf("MSEN_UNKNOWN_LABEL hier=%c|0x%x\n",
				b_label->b_hier, b_label->b_nonhier);
	                invalid_setvalue(b_label->b_hier, b_label->b_nonhier);
			break;

		case MINT_BIBA_LABEL:
			printf("MINT_BIBA_LABEL hier=%c|0x%x\n",
				b_label->b_hier, b_label->b_nonhier);
			trace_setvalue(b_label->b_list, b_label->b_nonhier);
			break;

		case MINT_EQUAL_LABEL:
			printf("MINT_EQUAL_LABEL hier=%c|0x%x\n",
				b_label->b_hier, b_label->b_nonhier);
	                invalid_setvalue(b_label->b_hier, b_label->b_nonhier);
			break;
			
		case MINT_HIGH_LABEL:
			printf("MINT_HIGH_LABEL hier=%c|0x%x\n",
				b_label->b_hier, b_label->b_nonhier);
	                invalid_setvalue(b_label->b_hier, b_label->b_nonhier);
			break;

		case MINT_LOW_LABEL:
			printf("MINT_LOW_LABEL hier=%c|0x%x\n", 
				b_label->b_hier, b_label->b_nonhier);
	                invalid_setvalue(b_label->b_hier, b_label->b_nonhier);
			break;

		default:
			printf("Error: b_type Not Set\n");
			return(IP_SECOPT_FAILED);
        }
        
        return( IP_SECOPT_PASSED );
}


static int
trace_sec_attr_uid(
        struct ipsec   *sec_attrs      /* security attributes  */
        )
{
        ushort  dac;

        if ( sa_sm_msg.sa_mask & T6M_UID ) {
		dac = (ushort) sa_sm_msg.sa_ids.uid;
        	printf("Per-Message UID (sm_uid)=%d\n", dac);
        }

        if ( sa_sm_dflt.sa_mask & T6M_UID ) {
		dac = (ushort) sa_sm_dflt.sa_ids.uid;
        	printf("Default Socket UID (sa_ids.uid)=%d\n", dac);
        }

        if ( sa_sm_snd.sa_mask & T6M_UID ) {
		dac = (ushort) sa_sm_snd.sa_ids.uid;
        	printf("Send UID (sa_ids.uid)=%d\n", dac);
        }

        if ( sa_sm_rcv.sa_mask & T6M_UID ) {
		dac = (ushort) sa_sm_rcv.sa_ids.uid;
        	printf("Receive UID (sa_ids.uid)=%d\n", dac);
        }

	dac = (ushort) sec_attrs->sm_uid;
	printf("Default UID (sm_uid_)=%d\n", dac);

        return ( IP_SECOPT_PASSED);
}


int
trace_sec_attr(
	struct ip	*ip,
        struct ipsec	*sec_attrs	/* security attributes  */
	)
{
        if ( !sec_attrs ) {
		printf("Error: Security Attributes Control Block Missing\n");
                return (IP_SECOPT_FAILED);
	}

        if ( !ip ) {
		printf("Error: IP mbuf Missing\n");
                return (IP_SECOPT_FAILED);
	}

	printf("src @: %s  dst @: %s\n",
		 inet_ntoa(ip->ip_src), inet_ntoa(ip->ip_dst));

	trace_sec_attr_uid( sec_attrs );
	
	if ( sa_sm_msg.sa_mask & T6M_SL ) {
		if ( ! msen_valid ( sa_sm_msg.sa_msen ) )
			printf("Per-Message MSEN Invalid sa_msen=0x%08x",
				sa_sm_msg.sa_msen);
		else {
			printf("Per-Message MSEN\n");
			trace_sec_attr_b( sa_sm_msg.sa_msen );
		}
	}

	if ( sa_sm_msg.sa_mask & T6M_INTEG_LABEL ) {
		if ( ! mint_valid ( sa_sm_msg.sa_mint ) )
			printf("Per-Message MINT Invalid 0x%08x\n",
				sa_sm_msg.sa_mint);
		else {
			printf("Per-Message MINT\n");
			trace_sec_attr_b( sa_sm_msg.sa_mint );
		}
	}

	if ( sa_sm_dflt.sa_mask & T6M_SL ) {
		if ( ! msen_valid ( sa_sm_dflt.sa_msen ) )
			printf("Default Socket MSEN Invalid 0x%08x\n",
				sa_sm_dflt.sa_msen);
		else {
			printf("Default Socket MSEN\n");
			trace_sec_attr_b( sa_sm_dflt.sa_msen );
		}
	}

	if ( sa_sm_dflt.sa_mask & T6M_INTEG_LABEL ) {
		if ( ! mint_valid ( sa_sm_dflt.sa_mint ) )
			printf("Default Socket MINT Invalid 0x%08x\n",
				sa_sm_dflt.sa_mint);
		else {
			printf("Default Socket MINT\n");
			trace_sec_attr_b( sa_sm_dflt.sa_mint );
		}
	}

	if ( sa_sm_rcv.sa_mask & T6M_SL ) {
		if ( ! msen_valid ( sa_sm_rcv.sa_msen ) )
			printf("Receive MSEN Invalid 0x%08x\n",
				sa_sm_rcv.sa_msen);
		else {
			printf("Receive MSEN\n");
			trace_sec_attr_b( sa_sm_rcv.sa_msen );
		}
	}

	if ( sa_sm_rcv.sa_mask & T6M_INTEG_LABEL ) {
		if ( ! mint_valid ( sa_sm_rcv.sa_mint ) )
			printf("Receive MINT Invalid 0x%08x\n",
				sa_sm_rcv.sa_mint);
		else {
			printf("Receive MINT\n");
			trace_sec_attr_b( sa_sm_rcv.sa_mint );
		}
	}

	if ( sa_sm_snd.sa_mask & T6M_SL ) {
		if ( ! msen_valid ( sa_sm_snd.sa_msen ) )
			printf("Send MSEN Invalid 0x%08x\n",
				sa_sm_snd.sa_msen);
		else {
			printf("Send MSEN\n");
			trace_sec_attr_b( sa_sm_snd.sa_msen );
		}
	}

	if ( sa_sm_snd.sa_mask & T6M_INTEG_LABEL ) {
		if ( ! msen_valid ( sa_sm_snd.sa_mint ) )
			printf("Send MSEN Invalid 0x%08x\n",
				sa_sm_snd.sa_mint);
		else {
			printf("Send MINT\n");
			trace_sec_attr_b( sa_sm_snd.sa_mint );
		}
	}

	if ( sec_attrs->sm_sendlabel ) {
		if ( mac_invalid ( sec_attrs->sm_sendlabel ) )
			printf("Default MAC Transmit Invalid 0x%08x\n",
				sec_attrs->sm_sendlabel);
		else {
			printf("Default MAC Transmit\n");
			trace_sec_attr_mac( sec_attrs->sm_sendlabel );
		}
	}

	if ( sec_attrs->sm_label ) {
		if ( mac_invalid ( sec_attrs->sm_label ) )
			printf("Dominate all Receive Invalid 0x%08x\n",
				sec_attrs->sm_label);
		else {
			printf("Dominate all Receive MAC\n");
			trace_sec_attr_mac( sec_attrs->sm_label );
		}
	}

	printf("\n");

	return (IP_SECOPT_PASSED);
}


static int
trace_enum_map(
        u_char	*ttp,	/* tag type pointer     */
	int	cats
        )
{
        u_short *bp;
        u_short enums[ CIPSOTT2_MAX_ENUM ];

        if ( ttp[ CIPSOTAG_ALIGN ] != 0) {
        	printf("\n\t\tBit Map Align Failed\n");
                return (IP_SECOPT_FAILED);
	}

        printf("\tGrade|Level=%c\n", ttp[ CIPSOTAG_LVL ]);

        cats -= CIPSOTT2_ENUMS;

        if ( cats < 2 ) {
        	printf("\n\t\tNo Categories|Divisions\n");
                return (IP_SECOPT_PASSED);
	}

        bp = (u_short *) &ttp[ CIPSOTT2_ENUMS ];
        bzero ( enums, CIPSOTT2_MAX_ENUM );
        bcopy ( bp, enums, cats );

        cats /= sizeof( u_short );        
        printf("\n\t\tCategories|Divisions:\n");

        for ( bp = enums; --cats; ++bp)
                printf("\t\t\t%d, ", category_xtoi( ntohs(*bp), doi));

	printf("\n");

        return (IP_SECOPT_PASSED);
}

static int
trace_bit_map( 
        u_char	*ttp,	/* tag type pointer     */
	int	bytes
        )
{
        u_char  *bitp;
        int     bits;           /* octet bits */
        int     bitpos;         /* bit position */
        int     divno = 0;
      
        if ( ttp[ CIPSOTAG_ALIGN ] != 0) {
        	printf("\n\t\tEnum Map Align Failed\n");
                return (IP_SECOPT_FAILED);
	}
      
        printf("\tGrade|Level=%c\n", ttp[ CIPSOTAG_LVL ]);
      
        bytes -= CIPSOTT1_BITMAP;

        if ( ! bytes ) {
        	printf("\n\t\tNo Categories|Divisions\n");
                return (IP_SECOPT_PASSED);
	}
      
        bitp  = &ttp[ CIPSOTT1_BITMAP ];
      
        printf("\n\t\tCategories|Divisions:\n");

        for ( ; --bytes; divno += 8 )  {
                bits = (int) *bitp++;
                for ( bitpos = 0; bits; ++bitpos ) {
                        if ( bits & 0x80 )
 				printf("\t\t\t%d, ", 
 					category_xtoi(divno + bitpos, doi));

                        bits <<= 1;
                }
        }

	printf("\n");

        return (IP_SECOPT_PASSED);
}

static int
trace_cipso(
        u_char	*cp
        )
{
        u_char  *ttp;   /* tag type pointer */
        int     cnt;    
        int     optlen; /* length of entire option      */
        u_int   ttval;  /* CIPSO tag type value         */
        int     ttlen;  /* CIPSO tag type length        */
	ushort	dac;

        optlen	= cp[ CIPSO_LGH ];
	if ( optlen < CIPSO_MIN_LEN || optlen > CIPSO_MAX_LEN ) {
		printf("CIPSO Packet Length Error: len=%d\n", optlen);
		return (IP_SECOPT_FAILED);
	}

	printf("CIPSO Packet: len=%d", optlen);

        ttp	= &cp[ CIPSO_TAG ];
	cnt	= (optlen - CIPSO_TAG);
      
        for (   ; 
                cnt > 0; 
                cnt -= ttlen, ttp += ttlen ) {

                ttval   = ttp[ CIPSOTAG_VAL ];	/* tag type option */
                ttlen   = ttp[ CIPSOTAG_LEN ];  /* tag type length */      
		if (    ( ttlen < CIPSOTAG_MIN_LGH ) ||
        	        ( ttlen > CIPSOTAG_MAX_LGH ) ) {
			printf("Tag Type Option Length Error, ttlen=%d\n", ttlen);
			return (IP_SECOPT_FAILED);
		}

                switch ( ttval ) {
                        /* 
                         * CIPSO Bit Map of Categories (MSEN) 
                         */
                        case CIPSO_TAG_TYPE_1:
				printf("\n\tCIPSO_TAG_TYPE_1; len=%d ", ttlen);
				trace_bit_map(ttp, ttlen);
				break;

                        /*
                         * CIPSO Enumerated Categories (MSEN)
                         */
                        case CIPSO_TAG_TYPE_2:
				printf("\n\tCIPSO_TAG_TYPE_2; len=%d ", ttlen);
				trace_enum_map(ttp, ttlen);
                                break;

                        /*
                         * SGIPSO Bit Map of Division (MINT)
                         */
                        case SGIPSO_BITMAP:
				printf("\n\tSGIPSO_TAG_TYPE_1; len=%d ", ttlen);
				trace_bit_map(ttp, ttlen);
                                break;

                        /*
                         * SGIPSO Enumerated Division (MINT)
                         */
                        case SGIPSO_ENUM:
				printf("\n\tSGIPSO_TAG_TYPE_2; len=%d ", ttlen);
				trace_enum_map(ttp, ttlen);
                                break;

                        /*
                         * SGIPSO Uid Tag Type
                         */
                        case SGIPSO_DAC:
				printf("\n\tSGIPSO_DAC; len=%d ", ttlen);
				bcopy( &ttp[ SGIPSO_UID ],
					(ushort *) &dac, sizeof(ushort) );
				printf("dac=%d\n", dac);
				break;

                        /*
                         * SGIPSO Special Tag Type
                         */
                        case SGIPSO_SPECIAL:
				printf("\tSGIPSO_SPECIAL; len=%d ", ttlen);
				printf("\n\t\tSGIPSO_MSEN=%c ", ttp[ SGIPSO_MSEN ]);
				printf("SGIPSO_MINT=%c\n", ttp[ SGIPSO_MINT ]);
                                break;

                        /*
                         * Unknown CIPSO/SGIPSO Tag Type, ignore them.
                         */
                        default:
				printf("UNKNOWN Tag Type: len=%d ", ttlen);
                                break;

                } /*end switch */
        } /*end for */

        return (IP_SECOPT_PASSED);
}

int
trace_ip_options( 
	struct mbuf		*m
	)
{
	u_char		*cp;
	struct ip	*ip;
	int		cnt;
	int		opt;
	int		optlen;

	if ( !m ) {
		printf("Error: No mbuf\n");
		return (IP_SECOPT_FAILED);
	}

	ip	= mtod( m, struct ip *);
	cp	= (u_char *) (ip + 1);
	cnt	= (ip->ip_hl << 2) - sizeof (struct ip);

	printf("src @: %s  dst @: %s\n",
		 inet_ntoa(ip->ip_src), inet_ntoa(ip->ip_dst));

	if ( ! cnt )
		printf("No IP Options\n");

	for (; cnt > 0; cnt -= optlen, cp += optlen) {

		opt = cp[IPOPT_OPTVAL];

		if (opt == IPOPT_EOL)
			break;
				
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if ( optlen <= 0 || optlen > cnt) {
				printf("Bad Option Length\n");
				break;
			}
		}

		switch (opt) {

			case IPOPT_SECURITY:
				printf("RIPSO-BSO\n");
				break;

			case IPOPT_ESO:
				printf("RIPSO-ESO\n");
				break;

			case IPOPT_CIPSO:
				trace_cipso( cp );
				break;

			default:
				printf("\tNO IP Security Options\n");
				break;
		}
	}

	printf("\n");

	return (IP_SECOPT_PASSED);
}
