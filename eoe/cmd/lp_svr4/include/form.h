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
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/include/RCS/form.h,v 1.1 1992/12/14 13:22:20 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#if	!defined(_LP_FORM_H)
#define	_LP_FORM_H

/**
 ** The disk copy of the form files:
 **/

/*
 * There are 9 fields in the form configuration file.
 */
#define FO_MAX	17
# define FO_PLEN	0
# define FO_PWID	1
# define FO_NP		2
# define FO_LPI		3
# define FO_CPI		4
# define FO_CHSET	5
# define FO_RCOLOR	6
# define FO_CMT	 	7	
# define FO_ALIGN	8	

/**
 ** The internal copy of a form as seen by the rest of the world:
 **/

typedef struct FORM {
	SCALED			plen;
	SCALED			pwid;
	SCALED			lpi;
	SCALED			cpi;
	int			np;
	char *			chset;
	short			mandatory;
	char *			rcolor;
	char *			comment;
	char *			conttype;
	char *			name;
}			FORM;

/*
 * Default configuration values:
 */
#define DPLEN		66
#define DPWIDTH		80
#define DNP		1
#define	DLPITCH		6
#define DCPITCH		10
#define DCHSET		NAME_ANY
#define	DRCOLOR		NAME_ANY
#define DCONTYP		NAME_SIMPLE
#define ENDENV		"#ENDOF_ENV\n"
#define MANSTR		"mandatory"

/*
 * These are the filenames that may be used for storing a form
 */
#define DESCRIBE	"describe"
#define COMMENT		"comment"
#define ALIGN_PTRN	"align_ptrn"
#define ALERTSH		"alert.sh"
#define ALERTVARS	"alert.vars"

#if (defined(__STDC__) || (__SVR4__STDC))

#define err_hndlr	int (*)( int , int , int )

int		delform ( char * );
int		getform ( char * , FORM * , FALERT * , FILE ** );
int		putform ( char * , FORM * , FALERT * , FILE ** );
int		rdform ( char * , FORM * , FILE * , err_hndlr , int * );
int		wrform ( char * , FORM * , FILE * , err_hndlr , int * );

void		freeform ( FORM * );

#undef	err_hndlr

#else

int		delform();
int		getform();
int		putform();
int		rdform();
int		wrform();

void		freeform();

#endif

#endif
