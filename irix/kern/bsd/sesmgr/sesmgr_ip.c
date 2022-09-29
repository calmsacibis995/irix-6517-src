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

/* 
 *  Routines to maintain IP security attributes.
 */

#ident	"$Revision: 1.4 $"

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


int
sesmgr_ip_options (
	struct mbuf  *m,
	struct ipsec **attrs	/* security attributes	*/
	)
{
        struct t6rhdb_kern_buf t6p;	/* t6rhdb pointer	*/
	struct ipsec	  *sec_attrs;	/* security attributes	*/
	struct ip	  *ip;

    	ASSERT (sesmgr_enabled);
		
	ip = mtod( m, struct ip *);
	bzero( &t6p, sizeof(t6p) );
	*attrs = NULL;

	/* 
	 * Allocate security control block for security attributes.
	 * The control block is for network AND transport layer
	 * security attributes.
	 */
	sec_attrs = sesmgr_soattr_alloc (M_DONTWAIT);
	ASSERT( sec_attrs != NULL);
	if (sec_attrs == NULL)
		return (IP_SECOPT_FAILED);

 	/*
	 * Look for the source (remote) system entry in the T6RHDB.
	 */
	if ( t6findhost( &ip->ip_src, 0, &t6p ) ) {

		if ( t6p.hp_flags & T6RHDB_MASK( T6RHDB_FLG_TRC_RCVPKT ) ) {
			printf("Receive Packet\n");
			trace_ip_options( m );
		}

		if ( ip_sec_dooptions(m, sec_attrs, &t6p) ) {
			sesmgr_soattr_free (sec_attrs);
			return (IP_SECOPT_FAILED);
		}

		if ( t6p.hp_flags & T6RHDB_MASK( T6RHDB_FLG_TRC_RCVATT ) ) {
			printf("Receive Attributes\n");
			trace_sec_attr( ip, sec_attrs );
		}
	
		*attrs = sec_attrs;
		return (IP_SECOPT_PASSED);
	}

	/*
	 * If we do not have a T6RHDB entry for the source station,
	 * silently drop packets.
	 *
	 * The spec calls for optionally returning ICMP errors
	 * which is not implemented at this time.
	 *
	 * XXX Audit record.
	 */

	IPSTAT (ips_badoptions);
	sesmgr_soattr_free ( sec_attrs );
	return (IP_SECOPT_FAILED);
} /* end sesmgr_ip_options */

struct mbuf *
sesmgr_ip_output (
	struct mbuf *m, 
	struct ipsec *sec_attrs,	/* security attributes	*/
	struct sockaddr_in *nhost,	/* next hop host	*/
	int *error
	)
{
	struct ip	  *ip;
	struct t6rhdb_kern_buf t6p;

    	ASSERT (sesmgr_enabled);

	if ( ! m )
		return (struct mbuf *) 0;
		
	if ( ! sec_attrs )
		return (struct mbuf *) 0;

	ip = mtod (m, struct ip *);
	ASSERT (ip != NULL);
	bzero ( &t6p, sizeof(t6p) );

	if ( !t6findhost( &ip->ip_dst, 0, &t6p) ) {
		*error = ENETUNREACH;
		return (struct mbuf *) 0;
	}

	if ( t6p.hp_flags & T6RHDB_MASK( T6RHDB_FLG_TRC_XMTATT ) ) {
		printf("Transmit Attributes\n");
		trace_sec_attr( ip, sec_attrs );
	}

	if ( ( t6p.hp_nlm_type == T6RHDB_NLM_UNLABELED ) ||
	     ( t6p.hp_nlm_type == T6RHDB_NLM_RIPSO_BSOR ) ) {
	        if ( t6p.hp_flags & T6RHDB_MASK( T6RHDB_FLG_TRC_XMTPKT ) ) {
			printf("Transmit Packet\n");
			trace_ip_options( m );
		}

		return ( m );
	}

	m = ip_sec_insertopts( m, sec_attrs, &t6p, error);

	if ( t6p.hp_flags & T6RHDB_MASK( T6RHDB_FLG_TRC_XMTPKT ) ) {
		printf("Transmit Packet\n");
		trace_ip_options( m );
	}

	return( m );
}
