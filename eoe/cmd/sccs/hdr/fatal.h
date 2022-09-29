/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FATAL_H
#define _FATAL_H

/* #ident	"@(#)head:fatal.h	1.4.4.2" */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sccs/hdr/RCS/fatal.h,v 1.11 1995/12/30 02:18:32 ack Exp $"

#include <setjmp.h>

extern	int	Fflags;
extern	char	*Ffile;
extern	int	Fvalue;
extern	int	(*Ffunc)(char *);
extern	jmp_buf	Fjmp;

# define FTLMSG		0100000
# define FTLCLN		 040000
# define FTLFUNC	 020000
# define FTLACT		    077
# define FTLJMP		     02
# define FTLEXIT	     01
# define FTLRET		      0

# define FSAVE(val)	SAVE(Fflags,old_Fflags); Fflags = val;
# define FRSTR()	RSTR(Fflags,old_Fflags);

#endif /* _FATAL_H */
