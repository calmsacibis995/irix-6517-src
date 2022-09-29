
/*
 * @(#) $Id: rsvp_global.h,v 1.7 1998/11/25 08:43:36 eddiem Exp $
 */
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Nov. 1993.
		Current Version:  Steven Berson & Bob Braden,  May 1996.

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies. and that any
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

#ifndef __rsvp_global_h__
#define __rsvp_global_h__

#ifdef __MAIN__

/*	DEFINE GLOBAL VALUES IN rsvp_main.c
 *
 */

/*    Define sockets and other I/O data structures
 *
 *    -- rsvp_socket:
 *		o Receive intercepted (RSVP_ON) raw multicast and unicast
 *			Path messages; however, superceded by rvsock[].
 *		o Receive intercepted (RSVP_ON) raw unicast Resv messages;
 *			however, superceded by rvsock[].
 *		o Send raw unicast Resv messages.
 *
 *    -- rsvp_Usocket:
 *		o Receive UDP-encap multicast Path messages, to G*, Pu.
 *		o Send UDP-encap unicast Resv messages.
 *
 *    -- rsvp_pUsocket:
 *		o UDP-only: Receive UDP-encap multicast Path messages
 *			to (D), Pu'.
 *
 *    -- isock[if]:
 *		o Send raw unicast Path message ONLY on specific phyint.
 *		o Send UDP-encap unicast Path messages on specific phyint,
 *			to D, Pu'.
 *
 *    -- vsock[vif/if]: (Not UDP-only; Vif if mrouted is running)
 *		o Send raw multicast Path message ONLY on specific vif/if.
 *
 *    -- Uvsock[vif/if]: (Not UDP-only; Vif if mrouted is running)
 *		o Send UDP-encap multicast Path msg on spec vif/if,
 *			 to D, Pu'.
 *
 *    -- rvsock[vif]: (Not UDP-only and mrouted is running)
 *		o Receive intercepted (VIF_ON) raw multicast and unicast
 *			Path message on specific vif.
 *		o Receive intercepted (VIF_ON) raw unicast Resv messages
 *			on specific vif.
 *
 *    -- Uisock[if]:
 *		o Receive UDP-encap unicast Resv messages on
 *			specific phyint.
 *		o Not UDP-only: Receive UDP-encap unicast Path message on 
 *			specific phyint, to port Pu.
 *		o UDP-only: Receive UDP-encap unicast Path message on 
 *			specific phyint, to port Pu'.
 *		o UDP-only: Send UDP-encap multicast Path message on
 *			specific phyint, to G*, Pu.
 *		o UDP-only: Send UDP-encap unicast Path message on
 *			specific phyint, to D/Ra, Pu.
 *
 *		(It looks very complicated, and unfortunately, it is.)
 */

int			 if_num;	/* number of phyints */
if_rec			*if_vec;	/* ptr to table of phyints */
int			 vif_num;	/* number of vifs */
int	 		 vif_toif[RSRR_MAX_VIFS];
					/* Vec to map vif# -> if# */

int			*vsock;		/* Vec of vif-spec raw send socks */
int			*Uvsock;	/* Vec of vif-spec UDP/raw send socks*/
int			*rvsock;	/* Vec of vif-spec raw recv socks */
int			*isock;		/* Vec of phyint-spec send socks */
int			*Uisock;	/* Vec of phyint-spec UDP S/R socks*/
int			 rsvp_socket;	/* Raw send/recv socket		*/
int			 rsvp_Usocket;	/* UDP send/recv socket		*/
int			 rsvp_pUsocket; /* UDP0-only recv socket	*/

int			 num_sessions;
Session			*session_hash[SESS_HASH_SIZE];
					/* Hash table to locate session */

api_rec			 api_table[API_TABLE_SIZE];
					/* Per-session API state */

KEY_ASSOC		 key_assoc_table[KEY_TABLE_SIZE];
int			 Key_Assoc_Max;

struct in_addr		 local_addr;	/* Default IP addr for my node */
int			 rsvp_errno;	/* error code -- used to communicate
					 *	error from kernel interface
					 */
int			 Max_rsvp_msg;	/* Largest RSVP message */
int			 debug;		/* Debug output control bits */
int			 l_debug;	/* Logging severity level */
FILE			*logfp;		/* Log file pointer */
char			*Log_Event_Cause = NULL;
					/* string to identify reason for
					 * initiation of event
					 */
	/* Control flags */
int			NoRawIO;
int			NoMcast;
int			NoMroute;
int			NoUnicast;
int			UDP_Only;


#else			/* an extern shadow of those vars */

extern int		 vif_num;
extern int		*vsock, *Uvsock;
extern int		 vif_toif[32];
extern int		 if_num;
extern int		*isock, *Uisock;
extern if_rec		*if_vec;
#ifndef __STATIC_RSVP_SOCKET__
/* Keep from getting multiple declarations - mwang */
extern int		 rsvp_socket;
#endif
extern int		rsvp_Usocket, rsvp_pUsocket;
extern int		 UDP_Only;
extern struct in_addr	 local_addr;
extern int		 rsvp_errno;
extern Session		*session_hash[SESS_HASH_SIZE];
extern api_rec		 api_table[API_TABLE_SIZE];
extern KEY_ASSOC	 key_assoc_table[KEY_TABLE_SIZE];
extern int		 Key_Assoc_Max;
extern int		 num_sessions;
extern int		 Max_rsvp_msg;
extern int		 debug, l_debug;
extern FILE		*logfp;
extern char		*Log_Event_Cause;
extern int		 NoRawIO;
extern int		 NoMcast;
extern int		 NoMroute;
extern int		 NoUnicast;

#endif	/* __MAIN__ */

extern u_long   time_now;	/* defined in rsvp_timer.c */

#endif	/* __rsvp_global_h__ */
