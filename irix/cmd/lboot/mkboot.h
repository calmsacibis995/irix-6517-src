/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 2.11 $"

#ifndef _MKBOOT_H
#define _MKBOOT_H

# include	"boothdr.h"


# define	MAXDEP		25	/* maximum dependencies per module */
# define	MAXRTN		200	/* maximum routine definitions per module */
# define	MAXSTRING	2000	/* maximum size of all strings/names per module */
#define		MAXALIASES	25	/* maximum aliases per module */
#define 	MAXADMIN	1000	/* maximum driver admin directives */	

/*
 * SARAH: See comments in mkboot.y. We currently parse the alias
 * strings from a master file, if they exist, and place the info
 * in the master structure, but don't use this information yet.
 */

extern struct master		master;
extern struct depend		depend[], *ndepend;
extern struct routine		routine[], *nroutine;
extern struct alias		alias[], *nalias;
extern char			string[], *nstring;

extern char			any_error;
extern jmp_buf *jmpbuf;

/*
 *	These are required to collect the driver administration hint information
 */
extern admin_t			admin[],*nadmin;

extern char		*basename(char *);
extern boolean		check_master(char *, register struct master *);
extern char		*copystring(char *);
extern char		*lcase(char *);
extern void		warn(char *, ...);
extern void		yyerror(char *, ...);
extern void		yyfatal(char *, ...);
extern int		yyparse(void);
extern void		lexinit(void);

#endif /* !_MKBOOT_H */
