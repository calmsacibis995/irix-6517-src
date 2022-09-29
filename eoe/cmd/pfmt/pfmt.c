/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pfmt:pfmt.c	1.2"
/*
 * pfmt and lfmt
 *
 * Usage:
 * pfmt [-l label] [-s severity] [-g catalog:msgid] format [args]
 * lfmt [-G type] [-c] [-f flags][-l label] [-s severity] [-g catalog:msgid]
 *                                                                format [args]
 */
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <pfmt.h>
#include <errno.h>
#include <string.h>
#include <stropts.h>
#include <getopt.h>
#include <sys/capability.h>

#define NEWSEV	5	/* User defined severity */

#ifdef LFMT
#define FMT	lfmt
#define USAGE	":381:Usage:\n    lfmt [-c] [-f flags] [-l label] [-s sev] [-g cat:msgid] [-G type] format [args]\n"
#define LABEL	"UX:lfmt"
#define OPTIONS	"cf:l:s:g:G:"
char *logopts[] = {
#define SOFT	0
	"soft",
#define HARD	1
	"hard",
#define FIRM	2
	"firm",
#define UTIL	3
	"util",
#define APPL	4
	"appl",
#define OPSYS	5
	"opsys",
	NULL
};
#else
#define FMT	pfmt
#define USAGE	":382:Usage:\n\tpfmt [-l label] [-s severity] [-g cat:msgid] format [args]\n"
#define LABEL	"UX:pfmt"
#define OPTIONS	"l:s:g:"
#endif

struct sevs {
	int s_int;
	const char *s_string;
} case_dep_sev[] = {
	MM_WARNING,	"warn",
	MM_WARNING,	"WARNING",
	MM_ACTION,	"action",
	MM_ACTION,	"TO FIX"
}, case_indep_sev[] = {
	MM_HALT,	"halt",
	MM_ERROR,	"error",
	MM_INFO,	"info"
};

#define NCASE_DEP_SEV	sizeof case_dep_sev / sizeof case_dep_sev[0]
#define NCASE_INDEP_SEV	sizeof case_indep_sev / sizeof case_indep_sev[0]

void
usage(complain)
int complain;
{
	if (complain)
		pfmt(stderr, MM_ERROR, ":8:Incorrect usage\n");
	pfmt(stderr, MM_ACTION, USAGE);
	exit(3);
}

main(argc, argv)
int argc;
char *argv[];
{
	int opt, ret;
	register int i;
	char *loc_label = NULL;
	char *loc_id = NULL;
	const char *format;
	char *fmt;
	long severity = -1L;
	int do_gui = 0;
	char epilogue[5];
#ifdef LFMT
	int do_console = 0;
	long log_opt = 0;
	char *subopt, *logval;
#endif

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel(LABEL);

	while ((opt = getopt(argc, argv, OPTIONS)) != EOF){
		switch(opt){
		case 'G':
			do_gui = 1;
			/* info = 0,
			 * progress = 1
			 * question = 2
			 * warning = 3
			 * action = 4
			 * lower right hand corner = 9
			 */
			if (isdigit(*optarg))
				epilogue[2] = *optarg;
			else
				epilogue[2] = '2';
			break;
#ifdef LFMT
		case 'c':
			do_console = 1;
			break;
		case 'f':
			subopt = optarg;
			while (*subopt != 0){
				switch(getsubopt(&subopt, logopts, &logval)){
				case SOFT:
					log_opt |= MM_SOFT;
					break;
				case HARD:
					log_opt |= MM_HARD;
					break;
				case FIRM:
					log_opt |= MM_FIRM;
					break;
				case UTIL:
					log_opt |= MM_UTIL;
					break;
				case APPL:
					log_opt |= MM_APPL;
					break;
				case OPSYS:
					log_opt |= MM_OPSYS;
					break;
				default:
					pfmt(stderr, MM_ERROR, ":383:Invalid flag\n");
					pfmt(stderr, MM_ACTION,
						":384:Valid flags are: soft, hard, firm, util, appl and opsys\n");
					exit(3);
				}
			}
			break;
#endif
		case 'l':
			if (loc_label){
				(void)pfmt(stderr, MM_ERROR,
					":385:Can only specify one label\n");
				exit(3);
			}
			if ((int)strlen(optarg) > MAXLABEL){
				(void)pfmt(stderr, MM_ERROR,
					":386:Label too long. Maximum is %d characters\n",
					MAXLABEL);
				exit(3);
			}
			loc_label = optarg;
			break;
		case 's':
			if (severity != -1){
				(void)pfmt(stderr, MM_ERROR,
					":387:Can only specify one severity\n");
				exit(3);
			}
				
			for (i = 0 ; i < NCASE_DEP_SEV ; i++){
				if (strcmp(optarg, case_dep_sev[i].s_string) == 0){
					severity = case_dep_sev[i].s_int;
					break;
				}
			}
			if (severity == -1 && (int)strlen(optarg) <= 7){
				char buf[8];
				for (i = 0 ; optarg[i] ; i++)
					buf[i] = tolower(optarg[i]);
				buf[i] = '\0';
				for (i = 0 ; i < NCASE_INDEP_SEV ; i++){
					if (strcmp(buf, case_indep_sev[i].s_string) == 0){
						severity = case_indep_sev[i].s_int;
						break;
					}
				}
			}
			if (severity == -1){
				if (addsev(NEWSEV, optarg) == -1){
					(void)pfmt(stderr, MM_ERROR, ":388:Cannot add user-defined severity\n");
					exit(1);
				}
				severity = NEWSEV;
			}
			break;
		case 'g':
			if (loc_id){
				(void)pfmt(stderr, MM_ERROR,
					":389:Can only specify one message identifier\n");
				exit(3);
			}
			loc_id = optarg;
			break;
		default:
			usage(0);
		}
	}

	if (optind == argc)
		usage(1);

	if (severity == -1)
		severity = MM_ERROR;

#ifdef LFMT
	severity |= log_opt;
	if (do_console)
		severity |= MM_CONSOLE;
#endif

	fmt = malloc(strlen(argv[optind]) + 1);
	if (fmt == 0) {
		pfmt(stderr, MM_ERROR, ":312:Out of memory: %s\n",
			strerror(errno));
		exit(1);
	}
	
	strccpy(fmt, argv[optind]);
	
	if (loc_id){
		(void)setcat("uxcore.abi");	/* Do not accept default catalogue */
		format = gettxt(loc_id, fmt);
		(void)setcat("uxcore.abi");
	}
	else
		format = fmt;

	if (do_gui) {
		struct strioctl st;
		cap_t ocap;
		cap_value_t cap_streams_mgt = CAP_STREAMS_MGT;

/* Gross hack that goes along with hack in gfx/textport/tport.c */
#define TPIOCDOGUI (('t' << 24) | ('p' << 16) | 1)

		bzero(&st, sizeof st);
		st.ic_cmd = TPIOCDOGUI;
		st.ic_timout = -1;

		ocap = cap_acquire(1, &cap_streams_mgt);
		if (!ioctl(2, I_STR, &st)) {
			epilogue[0] = '\033';
			epilogue[1] = '[';
			epilogue[3] = 'Z';
			epilogue[4] = '\0';
			/*
			 * Don't bother with the severity/label
			 * if we're displaying in a notifier box.  The
			 * box has a severity icon already.
			 */
			putenv("NOMSGSEVERITY=1");
			putenv("NOMSGLABEL=1");
			fputs("\033[W", stderr);
		} else
			do_gui = 0;
		cap_surrender(ocap);
	}

	setlabel(loc_label);

	ret = FMT(stderr, severity | MM_NOGET, format, argv[optind + 1],
			argv[optind + 2], argv[optind + 3], argv[optind + 4],
			argv[optind + 5], argv[optind + 6], argv[optind + 7],
			argv[optind + 8], argv[optind + 9], argv[optind + 10],
			argv[optind + 11], argv[optind + 12], argv[optind + 13],
			argv[optind + 14], argv[optind + 15], argv[optind + 16],
			argv[optind + 17], argv[optind + 18], argv[optind +19]);

	if (do_gui)
		fputs(epilogue, stderr);

	if (ret < 0)
		exit(-ret);

	exit(0);
}
