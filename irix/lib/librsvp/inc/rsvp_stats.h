
/*
 * @(#) $Id: rsvp_stats.h,v 1.3 1998/11/25 08:43:36 eddiem Exp $
 */

/***************************** rsvp_stats.h **************************
 *                                                                   *
 *          Define data structure for gathering statistics	     *
 *                                                                   *
 *********************************************************************/

/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Nov. 1993.
		Current Version: Steven Berson & Bob Braden, May 1996.

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


#ifndef __rsvp_stats_h__
#define __rsvp_stats_h__

typedef struct {
    u_int32_t	rsvpstat_msgs_in[RSVP_MAX_MSGTYPE+1];
    u_int32_t	rsvpstat_msgs_out[RSVP_MAX_MSGTYPE+1];
    u_int32_t	rsvpstat_admiss_fails;	/* #Local admission control failures */
    u_int32_t	rsvpstat_policy_fails;	/* #Local policy control failures    */
    u_int32_t	rsvpstat_other_failures;/* #Other local resv failures        */
    u_int32_t	rsvpstat_no_outscope;   /* #No Resv sent because of scope    */
    u_int32_t	rsvpstat_no_inscope;    /* #WF resv received w/o SCOPE	     */
    u_int32_t	rsvpstat_blockade_ev;	/* #Blockade =>ignore resv in refresh*/
    u_int32_t   rsvpstat_resv_timeout;	/* #timeouts of resv state	     */
    u_int32_t   rsvpstat_path_timeout;	/* #timeouts of path state	     */
    u_int32_t   rsvpstat_path_ttl0_in;	/* #path/ptear rcvd with ttl=0	     */
    u_int32_t   rsvpstat_path_ttl0_out;	/* #path/ptear not sent with ttl=0   */
}	RSVPstat;
	

#endif	/* __rsvp_stats_h__ */


