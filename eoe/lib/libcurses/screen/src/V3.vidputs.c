/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/V3.vidputs.c	1.3"

#include	"curses_inc.h"
extern	int	_outchar();

#ifdef	_VR3_COMPAT_CODE
#undef	vidputs
int
#ifdef __STDC__
vidputs(_ochtype a, int (*o)(int))
#else
vidputs(a, o)
_ochtype	a;
int		(*o)();
#endif
{
    vidupdate(_FROM_OCHTYPE(a), cur_term->sgr_mode, o);
    return (OK);
}
#endif	/* _VR3_COMPAT_CODE */
