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
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpstat/RCS/lpstat.h,v 1.1 1992/12/14 11:35:59 suresh Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "sys/types.h"

#define	DESTMAX	14	/* max length of destination name */
#define	SEQLEN	8	/* max length of sequence number */
#define	IDSIZE	DESTMAX+SEQLEN+1	/* maximum length of request id */
#define	LOGMAX	15	/* maximum length of logname */
#define	OSIZE	7

#define INQ_UNKNOWN	-1
#define INQ_ACCEPT	0
#define INQ_PRINTER	1
#define INQ_STORE	2
#define INQ_USER	3

#define V_LONG		0x0001
#define V_BITS		0x0002
#define V_RANK		0x0004
#define V_MODULES	0x0008

#if	defined(__STDC__)
#define BITPRINT(S,B) \
	if ((S)&(B)) { (void)printf("%s%s",sep,#B); sep = "|"; } else
#else
#define BITPRINT(S,B) \
	if ((S)&(B)) { (void)printf("%s%s",sep,"B"); sep = "|"; } else
#endif

typedef struct mounted {
	char			*name,
				**printers;
	struct mounted		*forward;
}			MOUNTED;

#if	defined(__STDC__)

void		add_mounted ( char * , char * , char * );
void		def ( void );
void		do_accept ( char ** );
void		do_charset ( char ** );
void		do_class ( char ** );
void		do_device ( char ** );
void		do_form ( char ** );
void		do_printer ( char ** );
void		do_request ( char ** );
void		do_user ( char ** );
void		done ( int );
void		parse ( int , char ** );
void		putoline ( char * , char * , long , char * , int , char * , char * , char * , int );
void		putpline ( char * , int , char * , char * , char * , char * , char * );
void		putqline ( char * , int , char * , char * );
void		running ( void );
void		send_message ( int , ... );
void		startup ( void );

int		output ( int );

#if	defined(_LP_PRINTERS_H)
char **		get_charsets ( PRINTER * , int );
#endif

#else

void		add_mounted();
void		def();
void		do_accept();
void		do_charset();
void		do_class();
void		do_device();
void		do_form();
void		do_printer();
void		do_request();
void		do_user();
void		done();
void		parse();
void		putoline();
void		putpline();
void		putqline();
void		running();
void		send_message();
void		startup();

int		output();

char **		get_charsets();

#endif

extern int		exit_rc;
extern int		inquire_type;
extern int		D;
extern int		scheduler_active;

extern char		*alllist[];

extern unsigned int	verbosity;

extern MOUNTED		*mounted_forms;
extern MOUNTED		*mounted_pwheels;
