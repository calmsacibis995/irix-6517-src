/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. 	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   	*/
/*	  All Rights Reserved                                   	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1990, 1991, 1992 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1985, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)listen:nlsdata.c	1.3.3.1"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/lib/libnls/RCS/nlsdata.c,v 1.1 1992/11/06 18:33:52 coolidge Exp $"

/*
 * data used by network listener service library routines
 */

#include "sys/tiuser.h"

int _nlslog;	/* non-zero allows use of stderr for messages	*/
struct t_call *_nlscall; /* call struct allocated by routines	*/
