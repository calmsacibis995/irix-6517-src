/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/pfmt_print.c	1.3"

/* pfmt_print() - format and print
 */
#include "synonyms.h"
#include "mplib.h"
#include <pfmt.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "pfmt_data.h"
#include <ctype.h>
#include <stdlib.h>		/* for getenv() prototype */

/* Catalogue for local messages */
#define fmt_cat		"uxlibc"
#define def_colon	": "
#define def_colonid	3

/* Table of default severities */
#define MSG_OFFSET	72
#define SVLNMLEN	9
static const char sev_list[][SVLNMLEN] = {
	"SEV = %d",
	"TO FIX",
	"ERROR",
	"HALT",
	"WARNING",
	"INFO"
};

/* flags to determine what to print */
#define PFMT_LABEL 0x1
#define PFMT_SEV 0x2

int
__pfmt_print(FILE *stream, long flag, const char *format,
	 const char **text_ptr, const char **sev_ptr, va_list args)
{
	const char *ptr;
	char catbuf[DB_NAME_LEN];
	int i, status;
	register int length = 0;
	int txtmsgnum = 0;
	int dofmt = (flag & MM_NOSTD) == 0;
	long doact = (flag & MM_ACTION);
	int verbose = PFMT_LABEL|PFMT_SEV;
	LOCKDECLINIT(l, LOCKLOCALE);

	if (format && !(flag & MM_NOGET)){
		char c;
		ptr = format;
		for (i = 0 ; i < DB_NAME_LEN - 1 && (c = *ptr++) && c != ':' ; i++)
			catbuf[i] = c;
		/* Extract the message number */
		if (i != DB_NAME_LEN - 1 && c){
			catbuf[i] = '\0';
			while (isdigit(c = *ptr++)){
				txtmsgnum *= 10;
				txtmsgnum += c - '0';
			}
			if (c != ':')
				txtmsgnum = -1;
		}
		else
			txtmsgnum = -1;
		format = __gtxt(catbuf, txtmsgnum, ptr);

	}

	if (text_ptr)
		*text_ptr = format;
		
	/*
	 * Permit user to shut up ugly SVR4 message formatting
	 */
	if (getenv("NOMSGLABEL") != NULL)
		verbose &= ~PFMT_LABEL;
	if (getenv("NOMSGSEVERITY") != NULL)
		verbose &= ~PFMT_SEV;

	if (dofmt){
		int severity, sev;
		const char *psev, *colon = NULL;

		if ((verbose & PFMT_LABEL) && *__pfmt_label && stream){
			if ((status = fprintf(stream, __pfmt_label)) < 0)
				goto err;
			length += status;
			colon = __gtxt(fmt_cat, def_colonid, def_colon);
			if ((status = fprintf(stream, colon)) < 0)
				goto err;
			length += status;
		}
		
		severity = (int)(flag & 0xff);

		if (!colon)
			colon = __gtxt(fmt_cat, def_colonid, def_colon);
		if (doact)
			sev = 1;
		else if (severity <= MM_INFO)
			sev = severity + 2;
		else {
			register int i;
			for (i = 0 ; i < __pfmt_nsev ; i++){
				if (__pfmt_sev_tab[i].severity == severity){
					psev = __pfmt_sev_tab[i].string;
					sev = -1;
					break;
				}
			}
			if (i == __pfmt_nsev)
				sev = 0;
		}

		if (sev >= 0)
			psev = __gtxt(fmt_cat, sev + MSG_OFFSET, sev_list[sev]);

		if (sev_ptr)
			*sev_ptr = psev;

		if ((verbose & PFMT_SEV) && stream){
			if ((status = fprintf(stream, psev, severity)) < 0)
				goto err;
			length += status;
			if ((status = fprintf(stream, colon)) < 0)
				goto err;
			length += status;
		}
	}
	else if (sev_ptr)
		*sev_ptr = NULL;

	if (format && stream){
		if ((status = vfprintf(stream, format, args)) < 0)
			goto err;
		length += status;
	}

	UNLOCKLOCALE(l);
	return length;
err:
	UNLOCKLOCALE(l);
	return -1;
}
