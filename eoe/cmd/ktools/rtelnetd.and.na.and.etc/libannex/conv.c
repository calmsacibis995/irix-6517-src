/*****************************************************************************
 *
 *        Copyright 1991, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 *
 *	Port name to port number conversion routine
 *
 * Original Author:  Pete Cameron		Created on: 02/28/88
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/conv.c,v 1.2 1996/10/04 12:07:52 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/conv.c,v $
 *
 * Revision History:
 *
 * $Log: conv.c,v $
 * Revision 1.2  1996/10/04 12:07:52  cwilson
 * latest rev
 *
 * Revision 1.1  1991/03/01  13:15:50  pjc
 * Initial revision
 *
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:07:52 $
#define RCSREV	$Revision: 1.2 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/conv.c,v 1.2 1996/10/04 12:07:52 cwilson Exp $"

#ifndef lint
static char rcsid[] = RCSID;
#endif

#include <sys/types.h>
#include <ctype.h>

#define	M_SLUS		64	/* Max on Annex 3 */

#if NDPTG > 0
#define	M_WINDOW	16	/* DPTG uses 16 windows */
#endif /* NDPTG */

#define	ERROR	-1

/*****************************************************************************
 *
 * NAME:
 *    name_to_unit
 *
 * DESCRIPTION:
 *    translate a device name to a logical unit number
 *
 * ARGUMENTS:
 *    name -	human-entered text
 *
 * RETURN VALUE:
 *    unit number if it could be translated
 *    -1 if it couldn't
 *
 * SIDE EFFECTS:
 *
 * EXCEPTIONS:
 *
 * ASSUMPTIONS:
 */

int
name_to_unit(name)
char	*name;
{
	char	*s = name;
	u_short	u = 0;

	if(*s == '\0')
		return ERROR;

	/*
	 * We gotta start with digits
	 */
	if(!isdigit(*s))
		return ERROR;

	/*
	 * Process leading digits
	 */
	while(*s && isdigit(*s)){
		u = (u * 10) + (*s - '0');
		s++;
	}

	/*
	 * Check to see if the number is in range
	 */
	if( (u < 0) || (u > M_SLUS) ){
		return ERROR;
	}

#if NDPTG > 0
	/*
	 * Now we might have a letter indicating window
	 */
	if(*s) {
		/* Cope with upper case */
		if( (*s >= 'B') && (*s <= 'P') ){
			*s = tolower(*s);
		}
		
		/* Window a can not be used */
		if(*s < 'b' || *s > 'p'){
			   return ERROR;
		}

		u = (u - 1) * (M_WINDOW - 1) + (M_SLUS + 1) + (*s - 'b');
		s++;
	}

	/*
	 * Not all values are allowed (994 == port 62p)
	 */
	if(u > 994){
		return ERROR;
	}
#endif /* NDPTG */

	/*
	 * Now we better be done
	 */
	if(*s != '\0'){
		return ERROR;
	}

	/*
	 * Well, we got a good value, pass it back
	 */
	return u;
}

