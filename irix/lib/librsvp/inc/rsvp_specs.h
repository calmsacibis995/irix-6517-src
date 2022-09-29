
/*
 * @(#) $Id: rsvp_specs.h,v 1.5 1998/11/25 08:43:36 eddiem Exp $
 */
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Nov. 1993.
		Current Version:  Steven Berson, May 1996

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


#ifndef __rsvp_specs_h__
#define __rsvp_specs_h__

#include "rapi_lib.h"

/*
 *	Results of flowspec comparison
 */
#define SPECS_NEQ	-2
#define SPEC1_LSS	-1
#define SPECS_EQL	0
#define SPEC1_GTR	+1
#define SPECS_USELUB	+2
#define SPECS_INCOMPAT	+3
#define SPEC1_BAD	+4
#define SPEC2_BAD	+5

#endif
