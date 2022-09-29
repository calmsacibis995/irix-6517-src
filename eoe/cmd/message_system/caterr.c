/*
*
* Copyright 1997, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/

#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/message_system/RCS/caterr.c,v 1.3 1997/08/23 17:36:34 bforney Exp $"


/* caterr: IRIX Message Catalog Text Preprocessor
 * 
 * caterr [-c 'catfile'] [-e] [-Y 'x','path'] [-s [-P cpp_opts]] [msgfile]
 * 
 * caterr converts the error message text source in msgfile into 'gencat'
 * format. If caterr is invoked without the -e option, conversion of the 
 * messages text is assumed.
 * 
 * The -c option is used to identify a catalog file, caterr invokes gencat 
 * to update the catalog using the generated output. If no catfile is 
 * provided, caterr sends the gencat source to standard output.
 *
 * The -e option is used to convert the explanation text into 'gencat'
 * format.
 *
 * The -Y option allows the user to specify the version of nroff, gencat, 
 * and nroff message macros that caterr calls. 
 * The -Y option takes two arguments: a key letter which indicates whether 
 * the path name is for nroff, gencat or message macros and a path name.
 * "-Y c," followed by a path, an alternate gencat as specified by 'path' 
 * will be used instead of /usr/bin/gencat.
 *
 * "-Y n," followed by a path, an alternate nroff as specified by 'path' 
 * will be used instead of /usr/bin/nroff.
 *
 * If nroff is not available on the system, explanation catalog will not
 * be made. If there is an existing catalog, then the file is touched.
 * If there is no explanation catalog, then an empty catalog is 
 * created. 
 *
 * "-Y m," followed by a path, an alternate message macros as specified
 * by 'path' will be used instead of /usr/share/lib/tmac/tmac.sg
 *
 * Alternate tool path specified with the -Y option must be absolute path.
 * The -Y option can be specified three times in the same command line if
 * all three alternate nroff, gencat and message macros paths are to be 
 * used.
 * 
 * The -s option is used to call the C preprocessor for the symbolic names
 * used in the message text file. The message text file can use symbolic 
 * names for the msg/exp numbers, then includes the file containing the
 * definitions of the symbolic names. The -s option will then replace
 * the symbolic names used in the message text with the defined msg/exp
 * numbers before further processing.
 *
 * The "-P cpp_opts" option is used to pass options to the C preprocessor.
 * It must be used in combination with the -s option.
 * 
 * If the 'msgfile' is not specified or if a dash (-) is specified, caterr
 * reads from standard input.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <nl_types.h>
#include <sys/param.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "errmsg.h"

/* maximum number of argc allowed */
#define MAX_ARGC        10

/* default locations for c preprocessor and gencat */
#define DEFAULT_CPP_PATH "/lib/cpp"
#define DEFAULT_GENCAT_PATH "/usr/bin/gencat"
#define DEFAULT_NROFF_PATH "/usr/bin/nroff"

/* message catalogs */
#define CMDCAT "msgsys.cat"
#define SYSCAT ""

void usage_err();
void process_sym();
void make_sym();
void process_msg();
void process_exp();

char *cmdname;

main(argc, argv)
int argc;
char *argv[];
{
	extern char *optarg;
	extern int optind;
	int cflg = 0;
	int eflg = 0;
	int yflg1 = 0; /* flag for calling alternate tool or macros */
	int yflg2 = 0; /* flag for calling alternate tool or macros */
	int yflg3 = 0; /* flag for calling alternate tool or macros */
	int cpathflg = 0; /* flag to call alternate gencat */
	int npathflg = 0; /* flag to call alternate nroff */
	int mpathflg = 0; /* flag to call nroff message macros */
	int symflg = 0; /* flag to call C preprocessor for symbolic names */
	int cppflg = 0;	/* flag for options to the C preprocessor */
	char *temppath1;
	char *temppath2;
	char *temppath3;
	char *cpp_opts = NULL;
	char gencatpath[MAXPATHLEN]; /* path for alternate gencat */
	char nroffpath[MAXPATHLEN]; /* path for alternate nroff */
	char macrospath[MAXPATHLEN]; /* path for alternate message macros */
	int errflg =0;
	int c;
	char *catfile; 	/* catalog file */
	char textfile[MAXPATHLEN]; /* message text file */
	FILE *textfd;
	char msgbuf[MAXPATHLEN];
	char gcatcmd[NL_TEXTMAX];
	FILE *tempstream;
	struct stat statbuf; /* file information */
	extern int optind;
	int stdinflg= 0;
	
	cmdname = argv[0];

	while ((c=getopt(argc, argv, "esY:c:P:")) != EOF){
		switch(c){
		case 'c':
			catfile = optarg;
			cflg ++;
			/* gencat will validate the catalog. It's entirely
			legal for the catalog file not to exist until
			gencat creates it. */
			break;

		case 'e':
			eflg++;
			break;

			/* The -Y option can be specified concurrently for
		   	   key letters 'n', 'c' or 'm'. */
		case 'Y':
			if (! yflg1) {
				yflg1 ++;
				temppath1 = optarg;
			}
			else if (! yflg2) {
				yflg2 ++;
				temppath2 = optarg;
			}
			else if (! yflg3) {
				yflg3 ++;
				temppath3 = optarg;
			}
			else errflg ++;
			break;

		case 's':
			symflg++;
			break;

		case 'P':
			cppflg++;
			cpp_opts = optarg;
			break;
		case '?':
			errflg++;
		}

	} /* while */

	if (errflg) usage_err();

	/* check to see if any of the -Y options is set */
	if (yflg1) {

		/* key letter for the first -Y option is 'c' */
		if ((temppath1[0] == 'c') && (temppath1[1] == ',')) {
			cpathflg ++;
			sprintf(gencatpath, "%s", temppath1 + 2);
		}
		/* key letter for the first -Y option is 'n' */
		else if ((temppath1[0] == 'n') && (temppath1[1] == ',')) {
			npathflg ++;
			sprintf(nroffpath, "%s", temppath1 + 2);
		}
		/* key letter for the first -Y option is 'm' */
		else if ((temppath1[0] == 'm') && (temppath1[1] == ',')) {
			mpathflg ++;
			sprintf(macrospath, "%s", temppath1 + 2);
		}
		else usage_err();
	} /* yflg1 */

	if (yflg2) {
		/* key letter for the first -Y option is 'c' */
		if ((temppath2[0] == 'c') && (temppath2[1] == ',')) {
			/* if alternate gencat is not already set */
			if (! cpathflg) {
				cpathflg ++;
				sprintf(gencatpath, "%s", temppath2 + 2);
			}
		}
		/* key letter for the first -Y option is 'n' */
		else if ((temppath2[0] == 'n') && (temppath2[1] == ',')) {
			/* if alternate nroff is not already set */
			if (! npathflg) {
				npathflg ++;
				sprintf(nroffpath, "%s", temppath2 + 2);
			}
		}
		/* key letter for the first -Y option is 'm' */
		else if ((temppath2[0] == 'm') && (temppath2[1] == ',')) {
			/* if alternate macros is not already set */
			if (! mpathflg) {
				mpathflg ++;
				sprintf(macrospath, "%s", temppath2 + 2);
			}
		}
		else 
			usage_err();
	} /* yflg2 */

	if (yflg3) {
		/* key letter for the first -Y option is 'c' */
		if ((temppath3[0] == 'c') && (temppath3[1] == ',')) {
			/* if alternate gencat is not already set */
			if (! cpathflg) {
				cpathflg ++;
				sprintf(gencatpath, "%s", temppath3 + 2);
			}
		}
		/* key letter for the first -Y option is 'n' */
		else if ((temppath3[0] == 'n') && (temppath3[1] == ',')) {
			/* if alternate nroff is not already set */
			if (! npathflg) {
				npathflg ++;
				sprintf(nroffpath, "%s", temppath3 + 2);
			}
		}
		/* key letter for the first -Y option is 'm' */
		else if ((temppath3[0] == 'm') && (temppath3[1] == ',')) {
			/* if alternate macros is not already set */
			if (! mpathflg) {
				mpathflg ++;
				sprintf(macrospath, "%s", temppath3 + 2);
			}
		}
		else 
			usage_err();
	} /* yflg3 */

	/* -P cpp_opts MUST be used with the -s flag */
	if (cppflg && !symflg)
		usage_err();

	/* Check to see if the input is to come from stdin or
	** should get the next argument as the message text file
	*/
	if ((optind == argc) || 
		((*argv[optind] == '-') && (*argv[optind+1]=='\0'))) {
		stdinflg++;
		textfd = stdin;
	}

	if (!stdinflg) {
		strcpy(textfile, argv[optind]);
		if ((textfd = fopen (textfile, "r")) == NULL) {
			_cmdmsg(ERRMSG_FOPENTEXTF, errno, "ERROR", cmdname,
				CMDCAT, SYSCAT, textfile);
			exit(2);
		}
	}

	/* if symbolic labels are used, call either process_sym()
	** or make_sym() to preprocess them. make_sym() makes the 
	** a preprocessed message text file from standard input.
	*/ 
	if (symflg) {
		if (!stdinflg) 
		  process_sym(textfd, textfile, eflg, cpp_opts);
		else 
		  make_sym(&textfd, textfile, eflg, cpp_opts);
	} 
	
	if (! npathflg)
		strcpy(nroffpath, DEFAULT_NROFF_PATH);

	/*	if ((stat(nroffpath, &statbuf) == -1) &&
	    (errno == ENOENT)) {
		if (eflg) {
			_cmdmsg(ERRMSG_NROFF, 0, "ERROR", cmdname, CMDCAT,
				SYSCAT, nroffpath);
			exit(2);
		}
	}

	strcat(nroffpath, " -Tlp ");
	if (mpathflg) {
		if ((stat(macrospath, &statbuf) == -1) ||
			(S_ISDIR(statbuf.st_mode))) { 
			_cmdmsg(ERRMSG_MACROACCESS, 0, "ERROR", cmdname,
				CMDCAT, SYSCAT, macrospath);
			exit(2);
		}
		strcat(nroffpath, macrospath);
		strcat(nroffpath, " ");
	}
	else
		strcat(nroffpath, "-msg ");*/



	/* -c option to make catalog */
	if (cflg) {
		if (cpathflg) {
			strcpy(gcatcmd, gencatpath);
		}
		else 
			strcpy(gcatcmd, DEFAULT_GENCAT_PATH);

                if (access(gcatcmd, X_OK) == -1 ) {
			_cmdmsg(CMD_ACCESS, 0, "ERROR", cmdname, CMDCAT,
				SYSCAT, gcatcmd);

                        exit(2);
                }
		strcat(gcatcmd, " ");
		strcat(gcatcmd, catfile );
		strcat(gcatcmd, "\n");
		/* open stream for calling gencat */
		tempstream = popen(gcatcmd, "w");
		if (tempstream == NULL) {
			_cmdmsg(CMD_POPEN, 0, "ERROR", cmdname, CMDCAT,
				SYSCAT, gcatcmd);
			exit(2);
		}
		if (eflg)
		  if (mpathflg)
		    process_exp(textfd, tempstream, nroffpath, macrospath);
		  else
		    process_exp(textfd, tempstream, nroffpath, NULL);
		else
			process_msg(textfd, tempstream);
		if (pclose(tempstream) == -1)
			_cmdmsg(CMD_PCLOSE, 0, "ERROR", cmdname, CMDCAT,
				SYSCAT);
	} /* if cflg, else do not call gencat */
	else {
		if (eflg)
		  if (mpathflg)
		    process_exp(textfd, stdout, nroffpath, macrospath);
		  else
		    process_exp(textfd, stdout, nroffpath, NULL);

		else 
			process_msg(textfd, stdout);
	} /* else cflg */

	/* close message source text file */
	(void) fclose(textfd);
	
	/* unlink the intermediate file produced by calling C pre-processor */
	if (symflg) (void) unlink(textfile);  

	return(0);

}


void process_msg(tfd, tempfd)
FILE *tfd, *tempfd;
{
	char textbuf[NL_TEXTMAX];
	int cont_flg = 0;
	void readtext();


	/* generate the set directives */
	fprintf(tempfd, "$set %d\n", NL_MSGSET);

	while (fgets(textbuf, NL_TEXTMAX, tfd) != NULL) {
		if ((strncmp(textbuf, "$msg ", 5) == 0) ||
		    (strncmp(textbuf, "$msg\t", 5) == 0 )) {
			readtext(textbuf, tempfd);
			cont_flg = check_bslash(textbuf);
		}
		else if ((strncmp(textbuf, "$ USMID ", 8) == 0) ||
			(strncmp(textbuf, "$ USMID\t ", 8) == 0) ||
			(strncmp(textbuf, "$\tUSMID ", 8) == 0) ||
			(strncmp(textbuf, "$\tUSMID\t", 8) == 0)) {
			fprintf(tempfd, "%s", textbuf);
			/* usmid line should not be within a $msg */
			if (cont_flg) {
			  _cmdmsg(ERRMSG_USMID, 0, "WARNING", cmdname,
				  CMDCAT, SYSCAT);

			  cont_flg= 0;
			}
		}

		/* if the continuation flag is set, then just output the 
		   line */
		else if (cont_flg) {
			fprintf(tempfd, "%s", textbuf);
			cont_flg = check_bslash(textbuf);
		}
		/* other lines are ignored. */
	}
}


void readtext(textline, tfd)
char textline[];
FILE *tfd;
{
	int i;

	for (i=5; i < ((int) strlen(textline)); i++) {
		if (isdigit(textline[i])) {
			fprintf(tfd, "%s", textline+i);
			break;
		}
	}

}


void process_exp(tfd, tempfd, npath, macrospath)
FILE *tfd, *tempfd;
char *npath;
char *macrospath;
{
	char textbuf[NL_TEXTMAX];
	int cont_flg = 0;
	int nexp_flg = 0;
	int nroff_flg = 0;
	char *nscrfile; /* scratch file for nroff output */
	FILE *nscrfd;
	char msgbuf[256];
	void call_nroff();


	/* generate the set directives */
	fprintf(tempfd,	"$set %d\n", NL_EXPSET);

	/* create a scratch file for input to nroff */
	nscrfile = tempnam(NULL, "cat.");
	if ((nscrfd = fopen(nscrfile, "w")) == NULL) {
		_cmdmsg(CMD_FOPEN, errno, "ERROR", cmdname, CMDCAT,
			SYSCAT, nscrfile);
		free(nscrfile);
		exit(2);
	}

	while (fgets(textbuf, NL_TEXTMAX, tfd) != NULL) {
		if ((strncmp(textbuf, "$exp ", 5) == 0) ||
		    (strncmp(textbuf, "$exp\t", 5) == 0)) {
			if (nroff_flg) {
				rewind(nscrfd);
				(void) fclose(nscrfd);
				call_nroff(nscrfile, tempfd, npath,
					   macrospath);
				if ((nscrfd = fopen(nscrfile, "w")) == NULL) {
					_cmdmsg(CMD_FOPEN, errno, "ERROR",
						cmdname, CMDCAT, SYSCAT,
						nscrfile);
					free(nscrfile);
					exit(2);
				}
				nroff_flg = 0;
			}
			readtext(textbuf, tempfd);
			cont_flg = check_bslash(textbuf);
		}

		else if ((strncmp(textbuf, "$ USMID ", 8) == 0) ||
			(strncmp(textbuf, "$ USMID\t ", 8) == 0) ||
			(strncmp(textbuf, "$\tUSMID ", 8) == 0) ||
			(strncmp(textbuf, "$\tUSMID\t", 8) == 0)) {
				fprintf(tempfd, "%s", textbuf);
				if (nexp_flg) nexp_flg = 0;
				if (cont_flg) {
			  	  _cmdmsg(ERRMSG_USMID, 0, "WARNING",
					  cmdname, CMDCAT, SYSCAT);
				  cont_flg = 0;
				}
		}
		/* nroff expands the tab to spaces, they 
		   will have to be removed for gencat, so
		   the tab is not preserved when converting
		   nexp to exp */
		else if ((strncmp(textbuf, "$nexp ", 6) == 0) ||
			(strncmp(textbuf, "$nexp\t", 6) == 0)) {
			if (!(nroff_flg))   nroff_flg = 1;

			nexp_flg = 1;

			fprintf(nscrfd, ".MS $exp %s", textbuf+6);
		}

		/* if the continuation flag is set, then just output 
		   the following exp lines */
		else if (cont_flg) {
			fprintf(tempfd, "%s", textbuf);
			cont_flg = check_bslash(textbuf);
		}

		/* pick up any following nexp lines */
		else if (nexp_flg) {
			if ((strncmp(textbuf,"$msg ", 5) == 0) ||
			    (strncmp(textbuf, "$msg\t", 5) == 0) ||
			    (strncmp(textbuf,"$ ", 2) == 0) ||
			    (strncmp(textbuf, "$\t", 2) == 0) ||
			    (strncmp(textbuf, "$\n", 2) == 0)) 
				nexp_flg = 0;
			else 
				fprintf(nscrfd, "%s", textbuf);
		}
		/* other lines are ignored. */
	}
	/* process any remaining nexp lines */
	if (nroff_flg) {
		(void) fclose(nscrfd);
		call_nroff(nscrfile, tempfd, npath, macrospath);

	}
	/* unlink the file created by tempname  */
	(void) unlink(nscrfile);
}


int check_bslash(tbuf)
char *tbuf;
{
	int cnt_flg = 0;
	char *tempbuf;


	/* find the last occurrence of backslash, if any */
	tempbuf = strrchr(tbuf, '\\');

	/* if the next char following the last back slash is a 
	     new line character '\n' then the back slash is to be 
	     interpreted as continuation. */
	if (tempbuf != NULL) {
		if (tempbuf[1] == '\n')	cnt_flg++;
	}
	return(cnt_flg);
}

void call_nroff(n_infile, tfd, nroffp, macrospath)
char *n_infile;
FILE *tfd;
char *nroffp;
char *macrospath;
{
	char *nout; /* scratch file for nroff output */
	char *nout1;
	char nroffcmd[NL_TEXTMAX];
	char msgbuf[256];
	struct stat statbuf; /* file information */
	void rm_blanks();
	void add_bslash();

	/* make sure nroff exists */
	if ((stat(nroffp, &statbuf) == -1) &&
	    (errno == ENOENT)) {
	  _cmdmsg(ERRMSG_NROFF, 0, "ERROR", cmdname, CMDCAT,
		  SYSCAT, nroffp);
	  exit(2);
	}

	strcpy(nroffcmd, nroffp);
	strcat(nroffcmd, " ");

	/* add the macropath */
	if (macrospath != NULL) {
		if ((stat(macrospath, &statbuf) == -1) ||
			(S_ISDIR(statbuf.st_mode))) { 
			_cmdmsg(ERRMSG_MACROACCESS, 0, "ERROR", cmdname,
				CMDCAT, SYSCAT, macrospath);
			exit(2);
		}
		strcat(nroffcmd, macrospath);
		strcat(nroffcmd, " ");
	}
	else
		strcat(nroffcmd, "-msg ");

	/* create a scratch file for nroff output */
	if ((nout = tempnam(NULL, "cat.")) == NULL) {
		_cmdmsg(CMD_TEMPNAM, errno, "ERROR", cmdname, CMDCAT, SYSCAT);
		exit(2);
	}

        if ((nout1 = tempnam(NULL, "cat.")) == NULL) {
		_cmdmsg(CMD_TEMPNAM, errno, "ERROR", cmdname, CMDCAT, SYSCAT);
                exit(2);
        }

	/* call nroff */
	strcat(nroffcmd, n_infile);
	strcat(nroffcmd, "  > ");
	strcat(nroffcmd, nout);
	if (system(nroffcmd)) {
		_cmdmsg(CMD_SYSTEM, errno, "ERROR", cmdname, CMDCAT,
			SYSCAT, nroffcmd);
		exit(2);
	}

        if (! stat(nout, &statbuf)) { 
		/* check nroff output file size */
		if (statbuf.st_size == 0) {
			_cmdmsg(ERRMSG_NROFFEMPTY, 0, "ERROR", cmdname,
				CMDCAT, SYSCAT, nout);
			(void) unlink(n_infile);
			(void) unlink(nout);
			exit(2);
		}
        }

	errno = 0;
	strcpy(nroffcmd, "/bin/cat ");
	strcat(nroffcmd, nout);
	strcat(nroffcmd, " | /bin/col -b > ");
	strcat(nroffcmd, nout1);
	if (system (nroffcmd)) {
		_cmdmsg(CMD_SYSTEM, errno, "ERROR", cmdname, CMDCAT,
			SYSCAT, nroffcmd);
		exit(2);
	}
	errno = 0;
	strcpy(nroffcmd, "/bin/mv ");
	strcat(nroffcmd, nout1);
	strcat(nroffcmd, " ");
	strcat(nroffcmd, nout);
	if (system (nroffcmd)) {
		_cmdmsg(CMD_SYSTEM, errno, "ERROR", cmdname, CMDCAT,
			SYSCAT, nroffcmd);
		exit(2);
	}

	/* remove the trailing blank lines produced by nroff */
	rm_blanks(nout);

	/* add the back slash new line characters, remove the exp tags,
	   and write it to the output stream */
	add_bslash(nout, tfd);

	/* unlink or remove the nroff scratch file */
	(void) unlink(nout);
}


void rm_blanks(n_file)
char *n_file;
{
	FILE *nstream;
	char msgbuf[256];
	char c;
	int nfd;
	long int nfdtemp;

	/* open the nroff output file in read/write mode */
	if ((nfd = open(n_file, O_RDWR)) == -1) {
		_cmdmsg(CMD_OPEN, errno, "ERROR", cmdname, CMDCAT,
			SYSCAT, n_file);
		exit(2);
	}

	/* get stream associated with the file descriptor for file 
	   manipulation */
	if ((nstream = fdopen(nfd, "r+")) == NULL) {
		_cmdmsg(CMD_FDOPEN, errno, "ERROR", cmdname, CMDCAT,
			SYSCAT, n_file);
		exit(2);
	}

	/* go to the last byte of the file */
	if (fseek(nstream, -1L, SEEK_END)) {
		_cmdmsg(ERRMSG_SEEKLAST, errno, "ERROR", cmdname, CMDCAT,
			SYSCAT, n_file);
		exit(2);
	}
	else {
		/* trace back the file until no new line character is 
		   encountered */
		c= (char) fgetc(nstream);
		while (c == '\n') {
			if (fseek(nstream, -2, SEEK_CUR)) {
				_cmdmsg(ERRMSG_SEEKBLANK, errno, "ERROR",
					cmdname, CMDCAT, SYSCAT, n_file);
				exit(2);
			}

			c= (char) fgetc(nstream);
		}
		/* save the last two new line characters */
		if (fseek(nstream, 2, SEEK_CUR)) {
			_cmdmsg(CMD_FSEEK, errno, "ERROR", cmdname, CMDCAT,
				SYSCAT, n_file);
			exit(2);
		}

		/* find the position to truncate */
		nfdtemp= ftell(nstream);

		if ( ftruncate(nfd, nfdtemp) == -1) {
			_cmdmsg(ERRMSG_TRUNCNROFF, errno, "ERROR", cmdname,
				CMDCAT, SYSCAT, n_file);
			exit(2);
		}
	}
	(void) close(nfd);
}

void add_bslash(n_outfile, outfd)
char *n_outfile;
FILE *outfd;
{
	char tempbuf[NL_TEXTMAX];
	char textbuf[NL_TEXTMAX];
	char printbuf[NL_TEXTMAX];
	char msgbuf[256];
	int len;
	FILE *noutfd;
	int expstart = 0; 

	strcpy(tempbuf, "\0");
	strcpy(printbuf, "\0");

	if ((noutfd = fopen(n_outfile, "r")) == NULL) {
		_cmdmsg(CMD_FOPEN, errno, "ERROR", cmdname, CMDCAT,
			SYSCAT, n_outfile);
		exit(2);
	}

	while (fgets(textbuf, NL_TEXTMAX, noutfd) != NULL) {
		if (printbuf != NULL)
			fprintf(outfd, "%s", printbuf);

		if ((strncmp(textbuf, "$exp ", 5) == 0) ||
		    (strncmp(textbuf, "$exp\t", 5) == 0)) {
			/* set flag to indicate starting text for exp's */
			if (! expstart) expstart ++;

			if (strcmp(tempbuf,"\0") != 0) 
				sprintf(printbuf, "%s", tempbuf);

			sprintf(tempbuf, "%s", textbuf+5);
		}
		else {
			/* nroff output lines contains the new line char */
			if (expstart) {
				if (strcmp(tempbuf, "\0") != 0) {
					len = strlen(tempbuf);
					tempbuf[len-1] = '\\';
					tempbuf[len]= 'n';
					tempbuf[len+1] = '\\';
					tempbuf[len+2]= '\n';
					tempbuf[len+3]= '\0';

					sprintf(printbuf, "%s", tempbuf);
				}

				sprintf(tempbuf, "%s", textbuf);
			} /* expstart */

		} /* else */

	}

	/* print the last contents in the printing buffer */
	if (printbuf != NULL) {
		fprintf(outfd, "%s\n", printbuf);

	}
	(void) fclose(noutfd);
}


/* This routine prints the caterr's usage message then exits the program */

void usage_err()
{
	_cmdmsg(ERRMSG_CATERRUSAGE, 0, "ERROR", cmdname, CMDCAT, SYSCAT);
	exit(2);
}

/* get_cppname() makes up the cpp file name from the argument 'tname'.
*/
void get_cppname(tname, cname, e_flag)
char *tname;
char cname[];
int e_flag;
{
  char *p;


        /* get basename of file */
        p= strrchr(tname, '/');
        if (p == NULL)
                p = tname;
        else
                p++;

        strcpy(cname, p);

        /* replace any '.stuff' at the end of the name with '.i' */
        p= strrchr(cname, '.');
        if (p == NULL)
                strcat(cname, ".i");
        else
                strcpy(p, ".i");
}


/* 
 * check_cpp
 *
 * copies the c preprocessor command line into sysline. The environment
 * variable USECPPPATH is checked first. If USECPPPATH is not defined,
 * a default value of cc -E -Uunix is used
 *
 */
void
check_cpp(char *sysline)
{

  char *usecpppath;
  
  if (((usecpppath= getenv("USECPPPATH")) != NULL) &&
      (!access(usecpppath, X_OK))) {
    strcpy(sysline, usecpppath);
  } else if(!access(DEFAULT_CPP_PATH, X_OK)) {
    /* use default */
    strcpy(sysline, DEFAULT_CPP_PATH);
    strcat(sysline, " -Uunix ");
  } else {
    _cmdmsg(ERRMSG_LIBCPP, 0, "ERROR", cmdname, CMDCAT, SYSCAT,
	    DEFAULT_CPP_PATH);
    exit(1);
  }
}


/* process_sym() takes the input message text file and process it through
** cpp. After cpp preprocessing, the pointer tfd is changed to pointed
** the cpp outputfile and the file name 'tfile' is changed to the name
** of the cpp output file for unlinking after the catalog is mad.
*/
void process_sym(tfd, tfile, exp_flag, cpp_opts)
FILE *tfd;
char *tfile;
int exp_flag;
char *cpp_opts;
{
	char symcmd[NL_TEXTMAX];
	char cppfile[MAXPATHLEN]; 
	char msgbuf[256];


	get_cppname(tfile, cppfile, exp_flag);
	check_cpp(symcmd);

	if (cpp_opts) {
		strcat(symcmd, cpp_opts);
		strcat(symcmd, " ");
	}
	strcat(symcmd, tfile);

	strcat(symcmd, " > ");
	strcat(symcmd, cppfile);

	if (system(symcmd)) {
		_cmdmsg(ERRMSG_SYMNAME, 0, "ERROR", cmdname, CMDCAT, SYSCAT);
		exit(1);
	}
	/* set the message text file to the cppfile for further processing */
	strcpy(tfile, cppfile);


	(void) fclose (tfd);

	if ((tfd= fopen(tfile, "r")) == NULL) {
	  _cmdmsg(ERRMSG_FOPENPROCF, errno, "ERROR", cmdname, CMDCAT,
		  SYSCAT, tfile);
	  exit(2);
	}

}


/* make_sym() copies the input from standard in to a temporary message
** text file for cpp preprocessing. 
** After cpp preprocessing, the pointer tfd is changed to pointed
** the cpp outputfile and the file name 'tfile' is changed to the name
** of the cpp output file for unlinking after the catalog is mad.
*/
void make_sym(tfd, tfile, exp_flag, cpp_opts)
FILE **tfd;
char tfile[];
int exp_flag;
char *cpp_opts;
{
	char symcmd[NL_TEXTMAX];
	char *p;
	FILE *tmpfd;
	char cppfile[MAXPATHLEN]; 
	char line[256];
	char *tmpfile;


	tmpfile = tempnam(NULL, "");
	p= strrchr(tmpfile, '/');
	if (p != NULL) {
	  p++;
	  strcpy(tmpfile, p);
	}

	if ((tmpfd = fopen(tmpfile, "w")) == NULL) {
	  _cmdmsg(CMD_FOPEN, 0, "ERROR", cmdname, CMDCAT, SYSCAT, tmpfile);
	  exit(1);
	}

	while (fgets(line, 255, *tfd) != NULL)
	  fputs(line, tmpfd);
	
	(void) fclose (tmpfd);

	check_cpp(symcmd);
	if (cpp_opts) {
		strcat(symcmd, cpp_opts);
		strcat(symcmd, " ");
	}
	strcat(symcmd, tmpfile);

	if (system(symcmd)) {
		_cmdmsg(ERRMSG_SYMNAME, 0, "ERROR", cmdname, CMDCAT, SYSCAT);
		exit(1);
	}

	/* unlink the file holding the text coming in from stdin */
	(void) unlink(tmpfile); 

	if ((tmpfd = fopen(cppfile, "r")) == NULL) {
	  _cmdmsg(CMD_FOPEN, 0, "ERROR", cmdname, CMDCAT, SYSCAT, cppfile);
          exit(1);
        }
	strcpy(tfile, cppfile);	
	*tfd = tmpfd;
}
