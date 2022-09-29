/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		copyright	"%c%" 	*/

#ident	"@(#)acct:common/cmd/acct/fwtmp.c	1.11.3.4"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/fwtmp.c,v 1.3 1994/09/15 00:05:19 ananth Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "acctdef.h"
#include <utmp.h>
#include <locale.h>

struct	utmp	Ut;
static char time_buf[50];

main ( c, v )
char	**v;
int	c;
{

	int	iflg,cflg;

	(void)setlocale(LC_ALL, "");

	iflg = cflg = 0;

	while ( --c > 0 ){
		if(**++v == '-') while(*++*v) switch(**v){
		case 'c':
			cflg++;
			continue;
		case 'i':
			iflg++;
			continue;
		}
	}

	for(;;){
		if(iflg){
			if(inp(stdin,&Ut) == EOF)
				break;
		} else {
			if(fread(&Ut,sizeof Ut, 1, stdin) != 1)
				break;
		}
		if(cflg)
			fwrite(&Ut,sizeof Ut, 1, stdout);
		else {
		  cftime(time_buf, DATE_FMT, &Ut.ut_time);
		  printf("%-8.8s %-4.4s %-12.12s %9ld %2hd %4.4ho %4.4ho %lu %s",
				Ut.ut_name,
				Ut.ut_id,
				Ut.ut_line,
				(long) Ut.ut_pid,
				Ut.ut_type,
				Ut.ut_exit.e_termination,
				Ut.ut_exit.e_exit,
				Ut.ut_time,
				time_buf);
		}
	}
	exit ( 0 );
}
inp(file, u)
FILE *file;
register struct utmp *u;
{

	char	buf[BUFSIZ];
	register char *p;
	register i;

	if(fgets((p = buf), BUFSIZ, file)==NULL)
		return EOF;

	for(i=0; i<NSZ; i++)	/* Allow a space in name field */
		u->ut_name[i] = *p++;
	for(i=NSZ-1; i >= 0; i--) {
		if(u->ut_name[i] == ' ')
			u->ut_name[i] = '\0';
		else
			break;
	}
	p++;

	for(i=0; i<4; i++)
		if((u->ut_id[i] = *p++)==' ')
			u->ut_id[i] = '\0';
	p++;

	for(i=0; i<LSZ; i++)	/* Allow a space in line field */
		u->ut_line[i] = *p++;

	u->ut_line[LSZ-1] = '\0'; /* just in case no blank at the end */

	for(i=LSZ-1; i >= 0; i--) {
		if(u->ut_line[i] == ' ')
			u->ut_line[i] = '\0';
		else
			break;
	}

	sscanf(p, "%hd %hd %ho %ho %ld",
		&u->ut_pid,
		&u->ut_type,
		&u->ut_exit.e_termination,
		&u->ut_exit.e_exit,
		&u->ut_time);

	return((unsigned)u);
}
