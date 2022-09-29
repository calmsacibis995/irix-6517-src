/*
 *  @(#) $Id: rsvp_bord.c,v 1.5 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_bord.c  *******************************
 *                                                                   *
 *    RSVP daemon: Byte-order routines                               *
 *                                                                   *
 *********************************************************************/

/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

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

/* Routines in this file convert RSVP objects between network and
 * local host formats. 
 *
 * Currently IP addresses and IP transport level ports are stored
 * internally in network format.  This includes those found inside
 * filterspecs, sender templates, and scope objects.
 
 * All other quantities are stored and processed in the local host format
 */

#include "rsvp_daemon.h"
#include "rsvp_specs.h"
#include "rapi_lib.h"

/* External declarations */
void hton_flowspec(FLOWSPEC *);
void ntoh_flowspec(FLOWSPEC *);
void hton_tspec(SENDER_TSPEC *);
void ntoh_tspec(SENDER_TSPEC *);
void hton_adspec(ADSPEC *);
void ntoh_adspec(ADSPEC *);

/* Forward declarations */
void hton_object(Object_header *);
void ntoh_object(Object_header *);

/*
 *	Convert given packet buffer to network byte order.
 */
void
hton_packet(struct packet *pkt)
	{
#if BYTE_ORDER == LITTLE_ENDIAN
	common_header	*hdrp = pkt->pkt_data;
	char		*lastp = (char *) pkt->pkt_data + pkt->pkt_len;
	Object_header	*objp, *nextp;

	if (pkt->pkt_order == BO_NET)
		return;

	objp = (Object_header *) (hdrp+1);
	while ( (char *)objp < lastp) {
		nextp = Next_Object(objp);
		hton_object(objp);
		objp = nextp;
	}

	/* Common fixed header fields */
	/* NB: checksum is not swapped */
	HTON16(hdrp->rsvp_length);
#endif
	pkt->pkt_order = BO_NET;
}
		

void
hton_object(Object_header *objp)
	{
	switch (objp->obj_class) {
	    case class_NULL:
		break;

	    case class_SESSION:
		/* addresses and ports stored internally in network order */
		break;

	    case class_SESSION_GROUP:
		/* Not defined. */
		break;

	    case class_RSVP_HOP:
		/* addresses are stored internally in network order */
		HTON32(((RSVP_HOP *)objp)->hop4_LIH);
		break;

	    case class_INTEGRITY:
		HTON32(((INTEGRITY *)objp)->intgr4_keyid);
		HTON32(((INTEGRITY *)objp)->intgr4_seqno);
		/* addresses stored internally in network order */
		/* message digest stored internally in network order */
		break;
		
	    case class_TIME_VALUES:
		HTON32(((TIME_VALUES *)objp)->timev_R);
		break;

	    case class_ERROR_SPEC:
		/* addresses stored internally in network order */
		HTON16(((ERROR_SPEC *)objp)->errspec4_value);
		break;

	    case class_SCOPE: {
		/* addresses stored internally in network order */
		break;
	    }

	    case class_STYLE:
		HTON32(((STYLE *)objp)->style_word);
		break;

	    case class_FLOWSPEC:
		hton_flowspec((FLOWSPEC *)objp);
		break;

	    case class_SENDER_TEMPLATE:
	    case class_FILTER_SPEC:
		/* addresses and ports stored internally in network order */
		break;

	    case class_SENDER_TSPEC:
		hton_tspec((SENDER_TSPEC *)objp);
		break;

	    case class_ADSPEC:
		hton_adspec((ADSPEC *)objp);
		break;

	    case class_POLICY_DATA:
		/* TBD */
		break;

	    case class_CONFIRM:
		/* addresses stored internally in network order */
		break;

	    default:
		/* If we don't understand this class, we can't convert its
		 * byte order; but that doesn't matter, because it must
		 * have originated elsewhere and must still be in network
		 * byte order.
		 */
		break;
	}
	HTON16(objp->obj_length);
}


void
ntoh_packet(struct packet *pkt)
	{
#if BYTE_ORDER == LITTLE_ENDIAN
	common_header	*hdrp = pkt->pkt_data;
	char		*lastp = (char *) pkt->pkt_data + pkt->pkt_len;
	Object_header	*objp;

	if (pkt->pkt_order == BO_HOST)
		return;

	objp = (Object_header *) (hdrp+1);
	while ((char *)objp < lastp) {
		ntoh_object(objp);
		objp = Next_Object(objp);
	}
	/* Common fixed header fields */
	/* NB: checksum is not swapped */
	NTOH16(hdrp->rsvp_length);
#endif
	pkt->pkt_order = BO_HOST;
}


void
ntoh_object(Object_header *objp)
	{
	NTOH16(objp->obj_length);
	switch (objp->obj_class) {
	    case class_NULL:
		break;

	    case class_SESSION:
		break;

	    case class_SESSION_GROUP:
		break;

	    case class_RSVP_HOP:
		NTOH32(((RSVP_HOP *)objp)->hop4_LIH);
		break;

	    case class_INTEGRITY:
		NTOH32(((INTEGRITY *)objp)->intgr4_keyid);
		NTOH32(((INTEGRITY *)objp)->intgr4_seqno);
		break;

	    case class_TIME_VALUES:
		NTOH32(((TIME_VALUES *)objp)->timev_R);
		break;

	    case class_ERROR_SPEC:
		NTOH16(((ERROR_SPEC *)objp)->errspec4_value);
		break;

	    case class_SCOPE:
		break;

	    case class_STYLE:
		NTOH32(((STYLE *)objp)->style_word);
		break;

	    case class_FLOWSPEC:
		ntoh_flowspec((FLOWSPEC *)objp);
		break;

	    case class_SENDER_TEMPLATE:
	    case class_FILTER_SPEC:
		break;

	    case class_SENDER_TSPEC:
		ntoh_tspec((SENDER_TSPEC *)objp);
		break;

	    case class_ADSPEC:
		ntoh_adspec((ADSPEC *)objp);
		break;

	    case class_POLICY_DATA:
		break;

	    case class_CONFIRM:
		break;

	    default:
		/*	If we don't understand class, leave in network
		 *	byte order.
		 */
		break;
	}
}

