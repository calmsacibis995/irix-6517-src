
/*
 * @(#) $Id: rsvp_daemon.h,v 1.5 1998/11/25 08:43:36 eddiem Exp $
 */
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Nov. 1993.
		Current Version:  Steven Berson & Bob Braden, May 1996.

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies, and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

#ifndef __rsvp_daemon_h__
#define __rsvp_daemon_h__

#include "config.h"
#include "rsvp_types.h"		/* it's important for these to come first */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef STANDARD_C_LIBRARY
#  include <stdlib.h>
#  include <stddef.h>		/* for offsetof */
#else
#  include <malloc.h>
#  include <memory.h>
#endif

#ifndef _KERNEL
#  include <signal.h>
#endif

#include <time.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#ifndef NET_IF_H
#define NET_IF_H
#include <net/if.h>
#endif

#include <netinet/in.h>

#ifndef NETINET_IN_SYSTM_H
#define NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif

#include <netinet/udp.h>

#ifndef NETINET_IP_H
#define NETINET_IP_H
#include <netinet/ip.h>
#endif

/* Definitions from <netinet/in.h> for 3.5 multicast setsockopt()'s and
 * RSVP Protocol
 *
 *	If you are running a 3.5 multicast kernel, all of these constants
 *	should be defined to their correct values.  If you are not running
 *	a 3.5 kernel, we define values that should generate an error.
 *
 *	NOTE:  Make sure that our values WILL generate an error if your
 *		kernel does not support 3.5 multicast.
 */
#ifndef IPPROTO_RSVP
#define IPPROTO_RSVP	46
#endif

#ifndef IP_MULTICAST_VIF
#define IP_MULTICAST_VIF -1
#endif /* IP_MULTICAST_VIF */

#ifndef IP_RSVP_ON
#define IP_RSVP_ON -1
#endif /* IP_RSVP_ON */

#ifndef IP_RSVP_OFF
#define IP_RSVP_OFF -1
#endif /* IP_RSVP_OFF */

#ifndef IP_RSVP_VIF_ON
#define IP_RSVP_VIF_ON -1
#endif /* IP_RSVP_VIF_ON */

#ifndef IP_RSVP_VIF_OFF
#define IP_RSVP_VIF_OFF -1
#endif /* IP_RSVP_VIF_OFF */

#ifdef SOLARIS
#include <sys/stream.h>
#include <sys/param.h>
#include <inet/common.h>
#undef IPOPT_EOL
#undef IPOPT_NOP
#undef IPOPT_LSRR
#undef IPOPT_RR
#undef IPOPT_SSRR
#include <inet/ip.h>
#else /* not SOLARIS */
#include <sys/mbuf.h>
#endif /* SOLARIS */

#include <arpa/inet.h>
#include <unistd.h>
#include <sys/un.h>
#include <assert.h>

#include "rsvp.h"

#ifdef STATS
#include "rsvp_stats.h"
#endif

#include "rsrr.h"
#include "rsvp_var.h"
#include "rsvp_global.h"
#include "rsvp_mac.h"
#include "rsvp_TCif.h"
#include "rsvp_specs.h"
#include "rsrr_var.h"

#ifdef INCLUDE_SYSTEM_H
#include "system.h"
#endif

#ifndef SYS_ERRLIST_DECLARED
extern char    *sys_errlist[];
extern int      sys_nerr;
#endif

#include "rsvp_proto.h"

#endif	/* __rsvp_daemon_h__ */
