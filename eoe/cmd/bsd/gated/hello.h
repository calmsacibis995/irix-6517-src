/*
 *   CENTER FOR THEORY AND SIMULATION IN SCIENCE AND ENGINEERING
 *			CORNELL UNIVERSITY
 *
 *      Portions of this software may fall under the following
 *      copyrights: 
 *
 *	Copyright (c) 1983 Regents of the University of California.
 *	All rights reserved.  The Berkeley software License Agreement
 *	specifies the terms and conditions for redistribution.
 *
 *  GATED - based on Kirton's EGP, UC Berkeley's routing daemon (routed),
 *	    and DCN's HELLO routing Protocol.
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/hello.h,v 1.2 1994/10/17 18:48:05 sfc Exp $
 *
 */

#ifndef IPPROTO_HELLO
#define		IPPROTO_HELLO	63
#endif
#define		DELAY_INFINITY	30000		/* in ms */
#define		HELLO_TIMERRATE	15		/* in seconds */
#define		HELLO_HYST(s)	(int)(s*.25)	/* 25% of old route, in ms */

#define		HELLO_DEFAULT	0		/* net 0 as default */

#define		METRIC_DIFF(x,y)	(x > y ? x - y : y - x)

/*		Define the DCN HELLO protocol packet			*/

struct hellohdr {
		u_short	h_cksum;
		u_short h_date;
		__uint32_t	h_time;
		u_short	h_tstp;
		} ;

struct m_hdr {
		u_char	m_count ;
		u_char	m_type ;
		} ;
		
struct type0pair {
		u_short d0_delay;
		u_short	d0_offset;
		} ;
			

struct type1pair {
		struct in_addr d1_dst;
		u_short	d1_delay ;
		short	d1_offset ;
		} ;


