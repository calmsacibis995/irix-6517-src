/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)pwck:pwck.c	1.7"	*/
#ident	"pwck: $Header: /proj/irix6.5.7m/isms/eoe/cmd/pwck/RCS/pwck.c,v 1.9 1996/10/23 01:16:58 thefred Exp $"

#include	<sys/types.h>

#ifdef NOT_INCLUDED		/* Using UID_MAX from <limits.h> instead 
#include	<sys/param.h> 	 * of MAXUID from <sys/param.h>.
				 */
#endif /* NOT_INCLUDED */

#include	<sys/signal.h>
#include	<sys/sysmacros.h>
#include	<sys/stat.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<limits.h> 

#define	ERROR1	"Too many/few fields"
#define ERROR1a "Line too long"
#define ERROR2	"Bad character(s) in logname"
#define ERROR2a "First char in logname not lower case alpha"
#define ERROR2b "Logname field null"
#define ERROR3	"Logname too long/short"
#define ERROR4	"Invalid UID"
#define ERROR5	"Invalid GID"
#define ERROR6	"Login directory not found"
#define ERROR6a	"Login directory null"
#define ERROR6b	"Login directory illegal, try pathchk(1)"
#define	ERROR7	"Optional shell file not found"
#define	ERROR7a	"Optional shell file not found, try pathchk(1)"

int eflag, pflag=0, code=0;
int badc;
char buf[BUFSIZ];
char tmpbuf[BUFSIZ];
static int isblank(char *);

main(int argc, char **argv)
{
	int delim[BUFSIZ];
	FILE *fptr;
	void error();
	struct	stat obuf;
	uid_t uid;
	gid_t gid;
	int len;
	register int i, j, colons;
	char *pw_file, *logbuf;
	long pathmax;
	int arg, ypflag;
	char c;

	while ((arg = getopt(argc, argv, "p")) != EOF) {
    		switch (arg) {
      		case 'p':
        		pflag++;
        		break;
      		default:
            		fprintf(stderr, "Usage: %s [[-p] file]\n", argv[0]);
        		exit(1);
    		}
  	}

	if(argc == 1 || argc == optind) pw_file="/etc/passwd";
	else pw_file=argv[optind];

	if(stat(pw_file, &obuf)<0){
	  perror("cannot get file stats");
	  exit(1);
	}

	if(!(obuf.st_mode & S_IRGRP ) || !(obuf.st_mode & S_IROTH)){
	  fprintf(stderr, 
		  "Warning: %s should be group and world readable\n", pw_file);
	  exit(2);
	}

	if((fptr=fopen(pw_file,"r"))==NULL) {
		fprintf(stderr,"cannot open %s\n",pw_file);
		exit(1);
	}

	pathmax = pathconf("/",_PC_PATH_MAX);
	if(pathmax == -1) {  
		pflag++;
	} else {
		logbuf = (char *) malloc(pathmax+1);
	}

	while(fgets(buf,BUFSIZ,fptr)!=NULL) {

		colons=0;
		badc=0;
		uid=gid=0;
		eflag=0;
		ypflag = 0;

		i = strlen(buf);
		if ( (i == (BUFSIZ-1)) && (buf[i-1] != '\n') ) {  
			/* line too long */
			buf[i-1] = '\n';	/* add newline for printing */
			error(ERROR1a);
			/* done reading continuation line(s) */
			while((c = getc(fptr)) != '\n');  
		}

		/* allow # to be used as comment character */

		if (isblank(buf))
			continue;


		/* check yellow page entry */

		if (buf[0] == '+') {
			ypflag++;
			if (buf[1] == '@')
				ypflag++;
		}

		/*  Check number of fields */

		for(i=0 ; buf[i]!=NULL; i++) {
			if(buf[i]==':') {
				delim[colons]=i;
				++colons;
			}
		delim[6]=i;
		delim[7]=NULL;
		}
		if(!ypflag && colons != 6) {
			error(ERROR1);
			continue;
		}

		if(buf[0] == ':') {
			error(ERROR2b);
		}
		for(i=0; buf[i]!=':' && buf[i]!='\n' && buf[i] != '\0'; i++) {
			if(islower(buf[i]));
			else if(isupper(buf[i]));
			else if(isdigit(buf[i]));
			else if(ypflag && i == 0);	 /* yellow page entry */
			else if (ypflag == 2 && i == 1); /* yellow page entry */
			else ++badc;
		}
		if(badc > 0) {
			error(ERROR2);
		}

		/*  Check for valid number of characters in logname  */

		if(i <= 0  ||  i > (8 + ypflag)) {
			error(ERROR3);
		}
		if (colons)
			colons--;

		/*  Check that UID is numeric and <= UID_MAX  */

		if (!ypflag) {
			len = (delim[2]-delim[1])-1;
			if ( (len > 10) || (len < 1) ) {
				error(ERROR4);
			}
			else {
			    for (i=(delim[1]+1); i < delim[2]; i++) {
				if(!(isdigit(buf[i]))) {
					error(ERROR4);
					break;
				}
			uid = uid*10 + (uid_t)((buf[i])-'0');
			    }
		    if(uid > UID_MAX  ||  uid < 0) {
				error(ERROR4);
			    }
			}
			colons--;
		}

		/*  Check that GID is numeric and <= UID_MAX  */

		if (!ypflag) {
			len = (delim[3]-delim[2])-1;
			if ( (len > 10) || (len < 1) ) {
				error(ERROR5);
			}
			else {
			    for(i=(delim[2]+1); i < delim[3]; i++) {
				if(!(isdigit(buf[i]))) {
					error(ERROR5);
					break;
				}
			gid = gid*10 + (gid_t)((buf[i])-'0');
			    }
		    if(gid > UID_MAX  ||  gid < 0) {
				error(ERROR5);
			    }
			}
			colons -= 2;
		}

		/*  Stat initial working directory  */

		if (colons && !pflag ) {
			int tmp_eflag=0;
			logbuf[0] = NULL;
			for(j=0, i=(delim[4]+1); i<delim[5]; j++, i++) {
				if(j == pathmax) {
					error(ERROR6b);
					tmp_eflag=1;
					break;
				}
				logbuf[j]=buf[i];
			}
			if(!(tmp_eflag)) {
			   logbuf[j] = NULL;
			   if (!ypflag) {
				if((stat(logbuf,&obuf)) == -1) {
					error(ERROR6);
				}
				/* Currently OS translates "/" for NULL field */
				if(logbuf[0] == NULL) { 
					error(ERROR6a);   
				}
			   } else if(logbuf[0] != NULL 
				&& ((stat(logbuf,&obuf)) == -1)) 
				error(ERROR6);
			}

			colons--;
		}

		/*  Stat of program to use as shell  */

		if (colons && !pflag ) {
			if((buf[(delim[5]+1)]) != '\n') {
				int tmp_eflag=0;
				logbuf[0] = NULL;
				for(j=0, i=(delim[5]+1); i<delim[6]; j++, i++) {
					if(j == pathmax) {
						error(ERROR7a);
						tmp_eflag=1;
						break;
					}
					logbuf[j]=buf[i];
				}
				if(!(tmp_eflag)) {
					logbuf[j] = NULL;
					/* subsystem login */
					if(strcmp(logbuf,"*") == 0)  {  
						continue;
					}
					if((stat(logbuf,&obuf)) == -1) {
						error(ERROR7);
					}
				}
			}
		}
	}
	free(logbuf);	 /* free before exiting?  sure, why not. */ 	
	fclose(fptr);
	exit(code);
}

/*  Error printing routine  */

void error(char *msg)
{
	if(!(eflag)) {
		fprintf(stderr,"\n%s",buf);
		code = 1;
		++eflag;
	}
	if(!(badc)) {
	fprintf(stderr,"\t%s\n",msg);
	return;
	}
	else {
	fprintf(stderr,"\t%d %s\n",badc,msg);
	badc=0;
	return;
	}
}

static int
isblank(char *p)
{
	register char *ptr;

	ptr = p;
	while(isspace(*ptr))
		ptr++;
	if (*ptr == '\n' || *ptr == '\0' || *ptr == '#')
		return(1);
	return(0);
}
