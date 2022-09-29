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

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/message_system/RCS/errmsg.h,v 1.2 1997/04/10 16:22:10 bforney Exp $"

/* 'errmsg' utility msg include file 
 * 
 * message code #define format: 
 *	#define msgcode nnn
 */ 

#ifndef _ERRMSG_H
#define _ERRMSG_H

/* cmdmsg function prototypes */

extern void _cmdmsg (int, int, const char *, const char *, const char *,
		     const char *, ...);

/* #define starts here: */

#define CMD_ACCESS	1001	/* access(); Cannot access '%s'.	*/
#define CMD_FDOPEN	1002	/* fdopen(); Cannot open '%s'.		*/
#define CMD_FOPEN	1003	/* fopen(); Cannot open '%s'.		*/
#define CMD_FSEEK	1004	/* fseek() '%s' failed.			*/
#define CMD_OPEN	1005	/* open(); Cannot open '%s'.		*/
#define CMD_PCLOSE	1006	/* pclose(); Cannot open pipe.		*/
#define CMD_POPEN	1007	/* popen(); Cannot open pipe to '%s'.	*/
#define CMD_STAT	1008	/* stat(); Cannot stat '%s'.		*/
#define CMD_SYSTEM	1009	/* system() of string %s failed.	*/
#define CMD_TEMPNAM	1010	/* tempnam() failed.			*/

#define ERRMSG_CATERRUSAGE	1940
#define ERRMSG_MACROACCESS	1941
#define ERRMSG_USMID		1942
#define ERRMSG_NROFFEMPTY	1943
#define ERRMSG_LIBCPP		1944
#define ERRMSG_SYMNAME		1945
#define ERRMSG_FOPENTEXTF	1946
#define ERRMSG_SEEKLAST		1947
#define ERRMSG_SEEKBLANK	1948
#define ERRMSG_TRUNCNROFF	1949
#define ERRMSG_FOPENPROCF	1950
#define ERRMSG_EXPLAINUSAGE	1951
#define ERRMSG_MSGIDENT		1952
#define ERRMSG_EXPCATOPEN	1953
#define ERRMSG_NOEXPMSG		1954
#define ERRMSG_NL_MALLOC	1955
#define ERRMSG_NL_MAXOPEN	1956
#define ERRMSG_NL_MAP		1957
#define ERRMSG_NL_HEADER	1958
#define ERRMSG_NROFF            1959
#endif /* _ERRMSG_H */








