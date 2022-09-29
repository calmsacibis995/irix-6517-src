
/*
 * @(#) $Id: rsrr_var.h,v 1.5 1998/11/25 08:43:36 eddiem Exp $
 */


/************************* rsrr_var.h ****************************
 *                                                               *
 *  	        RSRR-specific definitions                        *
 *                                                               *
 *****************************************************************/

/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		RSRR written by: Daniel Zappala, April 1995

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

/* RSRR things that are only needed by RSVP. */

#define RSRR_S_NOTIFICATION_BIT   0
#define RSRR_S_PINNING_BIT        1

#define RSRR_R_FIRST_HOP_BIT      0
#define RSRR_R_SECOND_HOP_BIT     1

#define RSRR_QT_SIZE 32
#define RSRR_QT_HASH(x) (x % RSRR_QT_SIZE)
#define RSRR_QT_MAX 0xffffffff

/* Table linking query ids to related RSVP structures. */
struct rsrr_query_table {
    u_long query_id;			/* Query ID of Route Query sent. */
    Session *dp;			/* Session/destination associated
					   with query */
    PSB *sp;				/* Sender associated with query */
    struct rsrr_query_table *next;	/* Next table entry */
};
