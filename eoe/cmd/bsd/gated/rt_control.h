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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/rt_control.h,v 1.1 1989/09/18 19:02:14 jleong Exp $
 *
 */

#define MAXINTERFACE	25		/* Maximum number of interfaces */

struct restricthash {
	struct	restrictlist *rt_forw;
	struct	restrictlist *rt_back;
};

struct restrictlist {
	struct	restrictlist *rt_forw;
	struct	restrictlist *rt_back;
	u_long	rhash;
	struct	sockaddr_in rdst;
	int	rproto;
	int	regpmetric;
	int	flags;
	u_long  rintf[MAXINTERFACE];
};

#define RT_ANNOUNCE	0x1		/* announce control restriction */
#define RT_NOLISTEN	0x2		/* listen control restriction */
#define RT_SRCLISTEN	0x4		/* listen from source */
#define RT_NOANNOUNCE	0x8		/* noannounce control restriction */

struct as_entry {
	struct as_entry *next;
	u_short as;
	u_short flags;
};

struct as_list {
	struct as_list *next;
	u_short as;
	u_short flags;
	struct as_entry *as_ptr;
};

#define	AS_SEND		0x1		/* Can send to this AS */
#define AS_DONOTSEND	0x2		/* Can not send to this AS */
#define	AS_RESTRICT	0x4		/* Announcement restrictions apply */

struct as_valid {
	struct as_valid *next;
	struct in_addr dst;
	u_short as;
	u_short metric;
};

#define MARTIAN_NETS	static char *martian_nets[7] = {\
	"127.0.0.0",\
	"128.0.0.0",\
	"191.255.0.0",\
	"192.0.0.0",\
	"223.255.255.0",\
	"224.0.0.0",\
	(char *)0 }		/* This is the end of the table, not default net */
