/*
 * @(#) $Id: rsvp_TCif.h,v 1.6 1998/11/25 08:43:36 eddiem Exp $
 */

/***************************************************************
 *	rsvp_TCif.h: Define RSVP's Traffic Control interface --
 *		calls to procedures in adaptation module.
 *
 ***************************************************************/
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Current Version:  Bob Braden, May 1996.

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


#ifndef __rsvp_TCif_h__
#define __rsvp_TCif_h__
/*
 *	The following default values are used in TC interface to update
 *	general characterization parameters.
 */
#define	TC_DFLT_PATH_BW		(1.0e7/8)	/* Ethernet BW assumed	*/
#define TC_DFLT_MIN_LATENCY	0		/* *REALLY* fast!	*/
#define TC_DFLT_MTU		1500		/* Assume Ethernet	*/



/* TC_AddFlowspec(): Makes a reservation.
 *
 *	Reservation is for MIN(*spec, *stspec); pass this to
 *	admission control, and if it is OK, make reservation.
 *
 *	Returns rhandle or TC_ERROR.
 */
u_long
TC_AddFlowspec(int OIf, FLOWSPEC *spec, SENDER_TSPEC *stspec, int flags,
					FLOWSPEC **fwd_specpp);

/*
 * TC_ModFlowspec(): Modifies a flowspec of a given flow.
 */
int
TC_ModFlowspec(int OIf, u_long rhandle,
			 FLOWSPEC *specp, SENDER_TSPEC *stspecp, int flags,
					FLOWSPEC **fwd_specpp);

/*
 * TC_DelFlowspec(): This routine deletes flow for specified handle;
 *	it also deletes all corresponding filter specs.
 */
int
TC_DelFlowspec(int OIf, u_long rhandle);

/*
 * TC_AddFilter(): Adds a filter for an existing flow.
 *
 *	Returns fhandle or TC_ERROR.
 */
u_long
TC_AddFilter(int OIf, u_long rhandle, Session *dest, FILTER_SPEC *filtp);

/*
 * TC_DelFilter(): Deletes existing filter.
 */
int
TC_DelFilter(int OIf, u_long fhandle);

/*
 * TC_Advertise(): Update OPWA ADSPEC.
 */
ADSPEC *
TC_Advertise(int OIf, ADSPEC * adspecp, int flags);

#define ADVERTF_NonRSVP			0x01

		
/* Initialization call when RSVP starts.  In case of error, it is
 *  suggested that an error message be logged and exit called.
 */
void
TC_init(int kernel_socket);

/*
 *	Define flags in calls
 */
#define TCF_E_POLICE	0x01		/* Entry policing		*/
#define TCF_M_POLICE	0x02		/* Merge-point policing		*/
#define TCF_B_POLICE	0x04		/* Branch-point policing	*/
#define TCF_MTUNNEL     0x08		/* this VIF is a TUNNEL         */

/*	In case of error, these routines set the appropriate error code
 *	and value in the external variable rsvp_errno, and return one of
 *	the following error indications:
 */
#define TC_ERROR	-1
#define TC_OK		0

#endif	/* __rsvp_TCif_h__ */
