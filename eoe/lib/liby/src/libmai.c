/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)liby:libmai.c	1.4"	*/
#ident	"$Id: libmai.c,v 1.5 1994/10/10 05:48:42 jwag Exp $"

extern int yyparse(void);

int
main(void){
	return yyparse();
	}
