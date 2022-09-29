/*
*
* Copyright 1997, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/

#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/locale/RCS/catmsgfmt.c,v 1.5 1997/05/30 20:28:21 bforney Exp $"

#if defined(__STDC__) && !defined(_LIBU)
        #pragma weak catmsgfmt = _catmsgfmt
#endif

/*
 * IMPORTANT:
 * This section is needed since this file also resides in the compilers'
 * libcsup/msg (v7.2 and higher). Once the compilers drop support for
 * pre-IRIX 6.5 releases this can be removed. Please build a libu before
 * checking in any changes to this file.
 *
 */

#ifndef _LIBU
#include "synonyms.h"
#endif

#include <locale.h>
#include <stdio.h>
#define __NLS_INTERNALS 1
#include <nl_types.h>
#undef __NLS_INTERNALS
#include <string.h>
#include <pfmt.h>

/*
 *	(C) COPYRIGHT CRAY RESEARCH, INC.
 *	UNPUBLISHED PROPRIETARY INFORMATION.
 *	ALL RIGHTS RESERVED.
 */
#include <stdlib.h>
#include <time.h>

#define OCTAL		8
#define HEXADECIMAL	16
#define	MAXTIME		51

/*
 *	catmsgfmt - format an error message
 *
 *	char *
 *	catmsgfmt(
 *		char	*cmdname,
 *		char	*groupcode,
 *		int	msg_num,
 *		char	*severity,
 *		char	*msgtext,
 *		char	*buf,
 *		int	buflen,
 *		char	*position,
 *		char	*debug)
 *
 *	catmsgfmt() formats up to "buflen" characters a message containing
 *	the command name "cmdname", group code "groupcode", message number
 *	"msg_num", severity level "severity", the text of the message
 *	"msgtext", and a debug parameter "debug".  The formatted
 *	message is placed in the user-supplied buffer "buf".
 *
 *	The msg_num parameter is an integer which is converted to an ASCII
 *	string of digits.  The cmdname, groupcode, severity, and msgtext
 *	parameters are all null-terminated character strings.
 *
 *	The MSG_FORMAT environment variable controls how the message
 *	is formatted.  If the MSG_FORMAT environment variable is not
 *	defined, a default formatting value is used.
 */

char *
catmsgfmt(
	  const char *cmdname,
	  const char *groupcode,
	  int msg_num,
	  const char *severity,
	  const char *msgtext,
	  char *buf,
	  int buflen,
	  char *pos,
	  char *dbg
	  )
{
	char	c;		/* Current character */
	char	*fmtp;		/* Pointer to format */
	char	*cmsp;		/* Pointer to current position in message */
	char	*tknp;		/* Pointer to current token */
	char	*tncp;		/* Pointer to terminating number character */
	char	*tfmt;		/* Timestamp format */
	char	num[20];	/* Converted message number */
	char	tms[MAXTIME];	/* Converted timestamp */
	int	cmsl;		/* Bytes remaining in message */
	time_t	cts;		/* Current timestamp */
	struct tm	*ltsp;	/* Local time structure */

	if ((buflen < 1) || (msg_num < 1))
		return( (char *) NULL);

	/* Convert number */
	(void) sprintf(num, "%d", msg_num);

	/* Clear timestamp string */
	*tms	= '\0';

	/* Find format */
	if ((fmtp = getenv(MSG_FORMAT)) == NULL) /* If format not defined */
		fmtp	= D_MSG_FORMAT;	/* Set default format */

	cmsl	= 1;			/* Current length (save 1 byte for \0 */
	cmsp	= buf;			/* Set current position in message */

	while ((*fmtp != '\0') && (cmsl < buflen))
		if ((c	= *fmtp++) != '%') {
			cmsl++;
			if (c != '\\')
				*cmsp++	= c;
			else {
				switch (*fmtp) {	/* Process escapes */

					case 'a' : c	= '\a';
						   break;

					case 'b' : c	= '\b';
						   break;

					case 'f' : c	= '\f';
						   break;

					case 'n' : c	= '\n';
						   break;

					case 'r' : c	= '\r';
						   break;

					case 't' : c	= '\t';
						   break;

					case 'v' : c	= '\v';
						   break;

					case 'x' : /* Do hex character */
						   c	= (char)strtol(fmtp + 1,
							&tncp, HEXADECIMAL); 
						   if (tncp == (fmtp + 1))
							c	= *fmtp;
						   else
							fmtp	= tncp - 1;
						   break;

					case '\\': c	= '\\';
						   break;

					case '?' : c	= '?';
						   break;

					case '\'': c	= '\'';
						   break;

					case '"' : c	= '"';
						   break;

					case '0' :
					case '1' :
					case '2' :
					case '3' :
					case '4' :
					case '5' :
					case '6' :
					case '7' : /* Do octal character */
						   c	= (char) strtol(fmtp,
							&tncp, OCTAL);
						   if (tncp == fmtp)
							c	= *fmtp;
						   else
							fmtp	= tncp - 1;
						   break;

					default  : c	= *fmtp;
				}

				*cmsp++	= c;
				fmtp++;
			}
		}
		else {
			switch (*fmtp) {

				case '%':	/* Percent */
					tknp	= "%";
					break;

				case 'C':	/* Command */
					tknp	= (char *) cmdname;
					break;

				case 'D':	/* Debug */
					tknp	= dbg;
					break;

				case 'G':	/* Group name */
					tknp	= (char *) groupcode;
					break;

				case 'N':	/* Message number */
					tknp	= num;
					break;

				case 'P':	/* Position */
					tknp	= pos;
					break;

				case 'S':	/* Severity */
					tknp	= (char *) severity;
					break;

				case 'T':	/* Timestamp */
					tknp	= tms;

					if (*tms == '\0') {	/* If not set */
						(void) time(&cts);
						ltsp	= localtime(&cts);
						tfmt	= getenv("CFTIME");
						if (tfmt == NULL ||
						    *tfmt == '\0')
							/* date(1) format */
							tfmt = "%a %b %e %H:%M:%S %Z %Y";
						(void) strftime(tms, MAXTIME,
								tfmt, ltsp);
					}
					break;

				case 'M':	/* Message text */
					tknp	= (char *) msgtext;
					break;

				default:	/* Everything else */
					*cmsp++	= c;
					if (cmsl++ < buflen)
						*cmsp++	= *fmtp;
					cmsl++;
					tknp	= "";
			}

			if (tknp != NULL) {
			  while ((*tknp != '\0') && (cmsl++ < buflen))
			    *cmsp++ = *tknp++;
			}
			fmtp++;

		}

	*cmsp	= '\0';		/* Terminate formatted message */

	return(buf);
}


