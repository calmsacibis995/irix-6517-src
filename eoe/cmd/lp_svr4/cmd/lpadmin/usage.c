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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpadmin/RCS/usage.c,v 1.1 1992/12/14 11:29:58 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "lp.h"
#include "printers.h"
#define WHO_AM_I           I_AM_LPADMIN
#include "oam.h"

/**
 ** usage() - PRINT COMMAND USAGE
 **/

void			usage ()
{
   LP_OUTMSG(INFO, E_ADM_USAGE);
   LP_OUTMSG(INFO|MM_NOSTD, E_ADM_USAGE1);
   LP_OUTMSG(INFO|MM_NOSTD, E_ADM_USAGE2);
#if	defined(CAN_DO_MODULES)
   LP_OUTMSG(INFO|MM_NOSTD, E_ADM_USAGE3);
#endif
   LP_OUTMSG(INFO|MM_NOSTD, E_ADM_USAGE4);
   LP_OUTMSG(INFO|MM_NOSTD, E_ADM_USAGE5);
   LP_OUTMSG(INFO|MM_NOSTD, E_ADM_USAGE6);

   return;
}
