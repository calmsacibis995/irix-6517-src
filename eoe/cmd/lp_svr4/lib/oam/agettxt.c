/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/oam/RCS/agettxt.c,v 1.1 1992/12/16 11:01:43 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* LINTLIBRARY */

#include "oam.h"

char			**_oam_msg_base_	= 0;

char *
#if	defined(__STDC__)
agettxt (
	long			msg_id,
	char *			buf,
	int			buflen
)
#else
agettxt (msg_id, buf, buflen)
	long			msg_id;
	char			*buf;
	int			buflen;
#endif
{
	if (_oam_msg_base_)
		strncpy (buf, _oam_msg_base_[msg_id], buflen-1);
	else
		strncpy (buf, "No message defined--get help!", buflen-1);
	buf[buflen-1] = 0;
	return (buf);
}
