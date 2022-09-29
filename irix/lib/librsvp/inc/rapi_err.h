/*
 * @(#) $Id: rapi_err.h,v 1.9 1998/11/25 08:43:36 eddiem Exp $
 */

/************************* rapi_err.h ****************************
 *                                                               *
 *             Text for RSVP and RAPI error codes                *
 *                                                               *
 *****************************************************************/
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


#ifndef __rapierr_h__
#define __rapierr_h__

/* allow C++ apps to use these files also - mwang */
#ifdef __cplusplus
extern "C" {
#endif

/* rsvp error messages */

char  *rsvp_errlist[] = {
	/*  0 */ "RESV: Confirmation",
	/*  1 */ "RESV: Admission control failure",
	/*  2 */ "RESV: Policy control failure",
	/*  3 */ "RESV: No path information",
	/*  4 */ "RESV: No sender information",
	/*  5 */ "RESV: Conflicting style",
	/*  6 */ "RESV: Unknown style",
	/*  7 */ "Conflicting dest port in Session",
	/*  8 */ "PATH: Conflicting src port",
	/*  9 */ "RESV: Ambiguous filter spec",
	/* 10 */ "(reserved)",
	/* 11 */ "(reserved)",
	/* 12 */ "Service preempted",
	/* 13 */ "Unknown object class",
	/* 14 */ "Unknown object C-type",
	/* 15 */ "(reserved)",
	/* 16 */ "(reserved)",
	/* 17 */ "(reserved)",
	/* 18 */ "(reserved)",
	/* 19 */ "(reserved)",
	/* 20 */ "API Client error",
	/* 21 */ "Traffic control error",
	/* 22 */ "Traffic control system error",
	/* 23 */ "RSVP system error",
""};

/* RSVP API ('RAPI') error messages */

char  *rapi_errlist[] = {"",
	 /* 1 */ "Invalid parameter",
	 /* 2 */ "Too many sessions",
	 /* 3 */ "Illegal session id",
	 /* 4 */ "Bad Filter_no or Flow_no for style",
	 /* 5 */ "Bad reservation style",
	 /* 6 */ "Syscall error -- see errno",
	 /* 7 */ "Parm list overflow",
	 /* 8 */ "Not enough memory",
	 /* 9 */ "RSVP daemon doesn't respond",
	 /* 10 */ "Bad object type",
	 /* 11 */ "Bad object length",
	 /* 12 */ "No sender tspec",
	 /* 13 */ "(Unused)",
	 /* 14 */ "Sender addr not my interface",
	 /* 15 */ "Recvr addr not my interface",
	 /* 16 */ "Bad src port: !=0 when dst port ==0",
	 /* 17 */ "Parms wrong for GPI_SESSION flag"
};

#ifdef __cplusplus
}
#endif

#endif	/* __rapierr_h__ */
