/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Lexical Analyzer for Network Configuration Files
 *
 *	$Revision: 1.4 $
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

#include <stdio.h>
#include <strings.h>
#include "cf.h"

#define TOKENSIZE	256

static int
getword(FILE *fp, char *bpos)
{
	int c;

	for (c = getc(fp); isspace(c); c = getc(fp))
		if (c == '\n') {
			*bpos++ = c;
			*bpos = '\0';
			return 1;
		}

	if (c == -1)
		return 0;

	*bpos++ = c;
	if (!isalnum(c)) {
		*bpos = '\0';
		return 1;
	}

	for ( ; ; *bpos++ = c) {
		c = getc(fp);
		if (!isalnum(c)) {
			switch (c) {
			    case ':':
			    case '.':
			    case '-':
			    case '_':
				continue;

			    default:
				ungetc(c, fp);
				*bpos = '\0';
				return 1;
			}
		}
	}
}

void
cf_lex(FILE *fp, int *linenumber, enum cftoken *token, char **value)
{
	static char buf[TOKENSIZE];
	int c;
	char *s = buf;

top:
	if (!getword(fp, s)) {
		*token = TOKEN_EOF;
		*value = 0;
		return;
	}

	switch (*s) {
	    case 'd':
	    case 'D':
		if (strcasecmp(s, "dnaddr") == 0)
			*token = TOKEN_DNADDR;
		else if (strcasecmp(s, "dnarea") == 0)
			*token = TOKEN_DNAREA;
		else
			*token = TOKEN_ID;
		break;
	    case 'h':
	    case 'H':
		if (strcasecmp(s, "host") == 0)
			*token = TOKEN_HOST;
		else
			*token = TOKEN_ID;
		break;
	    case 'i':
	    case 'I':
		switch (s[1]) {
		    case 'n':
		    case 'N':
			if (strcasecmp(s, "interface") == 0)
				*token = TOKEN_INTERFACE;
			else
				*token = TOKEN_ID;
			break;
		    case 'p':
		    case 'P':
			if (strcasecmp(s, "ipaddr") == 0)
				*token = TOKEN_IPADDR;
			else if (strcasecmp(s, "ipnet") == 0)
				*token = TOKEN_IPNET;
			else if (strcasecmp(s, "ipmask") == 0)
				*token = TOKEN_IPMASK;
			else
				*token = TOKEN_ID;
			break;
		    default:
			*token = TOKEN_ID;
		}
		break;
	    case 'n':
	    case 'N':
		switch (s[1]) {
		    case 'e':
		    case 'E':
			if (strcasecmp(s, "network") == 0)
				*token = TOKEN_NETWORK;
			else
				*token = TOKEN_ID;
			break;
		    case 'i':
		    case 'I':
			if (strcasecmp(s, "NISserver") == 0)
				*token = TOKEN_NISSERVER;
			else if (strcasecmp(s, "NISmaster") == 0)
				*token = TOKEN_NISMASTER;
			else
				*token = TOKEN_ID;
			break;
		    case 'o':
		    case 'O':
			if (strcasecmp(s, "node") == 0)
				*token = TOKEN_HOST;
			else
				*token = TOKEN_ID;
			break;
		    default:
			*token = TOKEN_ID;
		}
		break;
	    case 'p':
	    case 'P':
		if (strcasecmp(s, "physaddr") == 0)
			*token = TOKEN_PHYSADDR;
		else
			*token = TOKEN_ID;
		break;
	    case 's':
	    case 'S':
		if (strcasecmp(s, "segment") == 0)
			*token = TOKEN_SEGMENT;
		else
			*token = TOKEN_ID;
		break;
	    case 't':
	    case 'T':
		if (strcasecmp(s, "type") == 0)
			*token = TOKEN_TYPE;
		else
			*token = TOKEN_ID;
		break;
	    case '{':
		*token = TOKEN_LBRACE;
		break;
	    case '}':
		*token = TOKEN_RBRACE;
		break;
	    case '\n':
		(*linenumber)++;
		goto top;
	    case '/':
		c = getc(fp);
		if (c != '/') {
			ungetc(c, fp);
			*token = TOKEN_ERROR;
		}
		/* Fall through */
	    case '#':
		do {
			c = getc(fp);
			if (c == '\n') {
				ungetc(c, fp);
				goto top;
			}
		} while (c != EOF);
		goto top;
	    default:
		if (isalnum(*s))
			*token = TOKEN_ID;
		else
			*token = TOKEN_ERROR;
	}

	*value = s;
}
