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
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/include/RCS/oam.h,v 1.1 1992/12/14 13:23:08 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <pfmt.h>

char			*agettxt(
#if (defined(__STDC__) || (__SVR4__STDC))
				long	msg_id, char *	buf, int	buflen
#endif
			);

int			fmtmsg(
#if (defined(__STDC__) || (__SVR4__STDC))
				char * label,
			        int severity,
			        int seqnum, 
			        long int arraynum,
                                int logind,
                                ...
#endif
			);

/*
 * Possible values of "severity":
 */
#define	HALT		MM_HALT
#define	ERROR		MM_ERROR
#define	WARNING		MM_WARNING
#define	INFO		MM_INFO

/*
 **
 ** LP Spooler specific error message handling.
 **/

#define	MSGSIZ		512
#define	LOG             1 
#define	NOLOG           0   

#if	defined(WHO_AM_I)

#include "oam_def.h"

#if	WHO_AM_I == I_AM_CANCEL
static char		*who_am_i = "UX:cancel";

#elif	WHO_AM_I == I_AM_COMB
static char		*who_am_i = "UX:comb           ";
				  /* changed inside pgm */

#elif	WHO_AM_I == I_AM_LPMOVE
static char		*who_am_i = "UX:lpmove";

#elif	WHO_AM_I == I_AM_LPUSERS
static char		*who_am_i = "UX:lpusers";

#elif	WHO_AM_I == I_AM_LPNETWORK
static char		*who_am_i = "UX:lpnetwork";

#elif	WHO_AM_I == I_AM_LP
static char		*who_am_i = "UX:lp";

#elif	WHO_AM_I == I_AM_LPADMIN
static char		*who_am_i = "UX:lpadmin";

#elif	WHO_AM_I == I_AM_LPFILTER
static char		*who_am_i = "UX:lpfilter";

#elif	WHO_AM_I == I_AM_LPFORMS
static char		*who_am_i = "UX:lpforms";

#elif	WHO_AM_I == I_AM_LPPRIVATE
static char		*who_am_i = "UX:lpprivate";

#elif	WHO_AM_I == I_AM_LPSCHED
static char		*who_am_i = "UX:lpsched";

#elif	WHO_AM_I == I_AM_LPSHUT
static char		*who_am_i = "UX:lpshut";

#elif	WHO_AM_I == I_AM_LPSTAT
static char		*who_am_i = "UX:lpstat";

#elif	WHO_AM_I == I_AM_LPSYSTEM
static char		*who_am_i = "UX:lpsystem";

#elif	WHO_AM_I == I_AM_LPDATA
static char		*who_am_i = "UX:lpdata";

#else
static char		*who_am_i = "UX:mysterious";

#endif

/*
 * Simpler interfaces to the "fmtmsg()" and "agettxt()" stuff.
 */

#if	defined(lint)

#define LP_ERRMSG(C,X)               (void)printf("", C, X)
#define LP_ERRMSG1(C,X,A)            (void)printf("", C, X, A)
#define LP_ERRMSG2(C,X,A1,A2)        (void)printf("", C, X, A1, A2)
#define LP_LERRMSG(C,X)              (void)printf("", C, X)
#define LP_LERRMSG1(C,X,A)           (void)printf("", C, X, A)
#define LP_LERRMSG2(C,X,A1,A2)       (void)printf("", C, X, A1, A2)
#define LP_OUTMSG(C,X)               (void)printf("", C, X)
#define LP_OUTMSG1(C,X,A)            (void)printf("", C, X, A)
#define LP_OUTMSG2(C,X,A1,A2)        (void)printf("", C, X, A1, A2)
#define LP_OUTMSG3(C,X,A1,A2,A3)     (void)printf("", C, X, A1, A2, A3)
#define LP_OUTMSG4(C,X,A1,A2,A3,A4)  (void)printf("", C, X, A1, A2, A3,A4)

#else

#define	LP_ERRMSG(C,X) \
	(void)fmtmsg (who_am_i, C, X, NOLOG)
			   
#define	LP_ERRMSG1(C,X,A) \
	(void)fmtmsg (who_am_i, C, X, NOLOG, A)
			 
#define	LP_ERRMSG2(C,X,A1,A2) \
	(void)fmtmsg (who_am_i, C, X, NOLOG, A1, A2)
			 
#define	LP_LERRMSG(C,X) \
	(void)fmtmsg (who_am_i, C, X, LOG)
			   
#define	LP_LERRMSG1(C,X,A) \
	(void)fmtmsg (who_am_i, C, X, LOG, A)
			 
#define	LP_LERRMSG2(C,X,A1,A2) \
	(void)fmtmsg (who_am_i, C, X, LOG, A1, A2)
   
#define	LP_OUTMSG(C,X) \
	(void)outmsg (who_am_i, C, X, NOLOG)
			   
#define	LP_OUTMSG1(C,X,A) \
	(void)outmsg (who_am_i, C, X, NOLOG, A)
			 
#define	LP_OUTMSG2(C,X,A1,A2) \
	(void)outmsg (who_am_i, C, X, NOLOG, A1, A2)
			 
#define	LP_OUTMSG3(C,X,A1,A2,A3) \
	(void)outmsg (who_am_i, C, X, NOLOG, A1, A2, A3)
			 
#define	LP_OUTMSG4(C,X,A1,A2,A3,A4) \
	(void)outmsg (who_am_i, C, X, NOLOG, A1, A2, A3, A4)
			 
#endif	/* lint */

#endif	/* WHO_AM_I */
