/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sa:timex.c	1.12" */
#ident	"$Revision: 1.13 $"
/*	timex.c 1.12 of 8/7/85 	*/
#include <sys/types.h>
#include <sys/times.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <wait.h>

void printt(char *s, time_t a);
void hmstime(char stime[]);
void diag(char *s);

char	fname[20];


main(argc, argv)
int argc;
char **argv;
{
	struct	tms buffer, obuffer;
	int	status;
	int	p;
	int	c;
	time_t	before, after;
	char	stime[9], etime[9];
	char	cmd[80];
	extern	char	*optarg;
	extern	int	optind;
	int	pflg = 0, sflg = 0, oflg = 0;
	char	aopt[25];
	FILE	*pipin;
	char	ttyid[12], line[150];
	char	eol;
	char	fld[20][12];
	int	iline = 0, i, nfld;
	int	ichar, iblok;
	long	chars = 0, bloks = 0;

	aopt[0] = NULL;			/* initialize */

	/* check options; */
	while((c = getopt(argc, argv, "sopfhkmrt")) != EOF)
		switch(c)  {
		case 's':  sflg++;  break;
		case 'o':  oflg++;  break;
		case 'p':  pflg++;  break;

		case 'f':  strcat(aopt, "-f ");  break;
		case 'h':  strcat(aopt, "-h ");  break;
		case 'k':  strcat(aopt, "-k ");  break;
		case 'm':  strcat(aopt, "-m ");  break;
		case 'r':  strcat(aopt, "-r ");  break;
		case 't':  strcat(aopt, "-t ");  break;

		case '?':  diag("Usage: timex [-s][-o][-p[-fhkmrt]] cmd");
				break;
		}
	if(optind >= argc)	diag("Missing command");

	/*
	 * check to see if accounting is installed and print a somewhat
	 * meaningful message if not
	 */
	if (((oflg+pflg) != 0) && (access("/bin/acctcom", 01) == -1) ) {
		oflg = 0;
		pflg = 0;
		fprintf(stderr, "Information from -p and -o options not available\n");
		fprintf(stderr, " because process accounting is not operational.\n");
	}

	if (sflg) {
		sprintf(fname,"/tmp/tmx%d",getpid());
		sprintf(cmd,"/usr/lib/sa/sadc 1 1 %s",fname);
		system(cmd);
	}
	if (pflg + oflg) hmstime(stime);
	before = times(&obuffer);
	if ((p = fork()) == -1) diag("Try again.\n");
	if(p == 0) {
		setgid(getgid());
		execvp(*(argv+optind),(argv+optind));
		fprintf(stderr, "%s: %s\n", *(argv+optind), strerror(errno));
		exit(1);
	}
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	while(wait(&status) != p);
	if((status&0377) != 0)
		fprintf(stderr,"Command terminated abnormally.\n");
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	after = times(&buffer);
	if (pflg + oflg) hmstime(etime);
	if (sflg) system(cmd);

	fprintf(stderr,"\n");
	printt("real", (after-before));
	printt("user", buffer.tms_cutime - obuffer.tms_cutime);
	printt("sys ", buffer.tms_cstime - obuffer.tms_cstime);
	fprintf(stderr,"\n");

	if (oflg+pflg) {
		char *tname;
		if(isatty(0) && (tname=ttyname(0)))
			sprintf(ttyid, "-l %s", tname+5);
		else
			*ttyid = '\0';
		sprintf(cmd, "acctcom -S %s -E %s -u %d %s -i %s",
			stime, etime, getuid(), ttyid, aopt);
		pipin = popen(cmd, "r");
		while(fscanf(pipin, "%[^\n]%1c", line, &eol) > 1) {
			if(pflg)
				fprintf(stderr, "%s\n", line);
			if(oflg)  {
				nfld=sscanf(line,
				"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
				fld[0], fld[1], fld[2], fld[3], fld[4],
				fld[5], fld[6], fld[7], fld[8], fld[9],
				fld[10], fld[11], fld[12], fld[13], fld[14],
				fld[15], fld[16], fld[17], fld[18], fld[19]);
				if(++iline == 3)
					for(i=0; i<nfld; i++)  {
						if(strcmp(fld[i], "CHARS") == 0)
							ichar = i+2;
						if(strcmp(fld[i],"BLOCKS") == 0)
							iblok = i+2;
					}
				if (iline > 4)  {
					chars += atol(fld[ichar]);
					bloks += atol(fld[iblok]);
				}
			}
		}
		pclose(pipin);

		if(oflg)
			if(iline > 4)
				fprintf(stderr,
				"\nCHARS TRNSFD = %ld\nBLOCKS READ  = %ld\n",
				chars, bloks);
			else
				fprintf(stderr, "\nNo process records found!\n");
	}

	if (sflg)  {
		sprintf(cmd,"/usr/bin/sar -A -f %s 1>&2",fname);
		system(cmd);
		unlink(fname);
	}
	exit(status>>8);
	/*NOTREACHED*/
}

char quant[] = { HZ/10, 10, 10, 6, 10, 6, 10, 10, 10 };
char *pad  = "000      ";
char *sep  = "\0\0.\0:\0:\0\0";
char *nsep = "\0\0.\0 \0 \0\0";

void
printt(char *s, time_t a)
{
	char	digit[9];
	int	i;
	char	c;
	int	nonzero;

	for(i=0; i<9; i++) {
		digit[i] = (char)(a % quant[i]);
		a /= quant[i];
	}
	fprintf(stderr,s);
	nonzero = 0;
	while(--i>0) {
		c = digit[i]!=0 ? digit[i]+'0':
		    nonzero ? '0':
		    pad[i];
		if (c != '\0') putc(c,stderr);
		nonzero |= digit[i];
		c = nonzero?sep[i]:nsep[i];
		if (c != '\0') putc(c,stderr);
	}
	fprintf(stderr,"%c",digit[0]*100/HZ+'0');
	fprintf(stderr,"\n");
}

/*
** hmstime() sets current time in hh:mm:ss string format in stime;
*/

void
hmstime(char stime[])
{
	char	*ltime;
	time_t tme;

	tme = time((time_t *)0);
	ltime = ctime(&tme);
	strncpy(stime, ltime+11, 8);
	stime[8] = '\0';
}
 
void
diag(char *s)
{
	fprintf(stderr,"%s\n",s);
	unlink(fname);
	exit(1);
}
