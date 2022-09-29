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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/snmp.h,v 1.1 1989/09/18 19:02:23 jleong Exp $
 *
 */

#if	defined(AGENT_SNMP) || defined(AGENT_SGMP)

#define CORE_VALUE	0

struct mibtbl {
	char length;
	char object[11];
	int (*function)();
};

#endif	defined(AGENT_SNMP) || defined(AGENT_SGMP)

