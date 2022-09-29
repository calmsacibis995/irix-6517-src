/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.11 $
 */

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <curses.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/times.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smtd.h>
#include <smtd_fs.h>
#include <smt_snmp_clnt.h>
#include <sm_map.h>
#include <ma_str.h>

#ifdef _IRIX4
extern int printw(const char *, ...);
#endif
static void spr(void);
static int (*pptr)(const char *, ...) = printf;

char	name[30];
static uid_t uid;
static int first = 1;
static int zero = 0;
static int interval = 0;
static int delall = 0;
static int delsome = 0;
static int sflag = 0;
static int devnum = 0;
SMT_FSSTAT fsq, zfsq;

#define USAGE "usage: smtgc [-a] [-z] [-i devnum] [-f type]...\n"
const	char	ctrl_l = 'L' & 037;
const	int	BOS = 23;	/* Index of last line on screen */

static void
quit(int stat)
{
	if (sflag) {
		move(BOS, 0);
		clrtoeol();
		refresh();
		endwin();
	}
	map_exit(stat);
}

static void
fail(char *fmt, ...)
{
	va_list args;

	if (sflag) {
		move(BOS, 0);
		clrtoeol();
		refresh();
		endwin();
	}
	va_start(args, fmt);
	fprintf(stderr, "smtstat: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	map_exit(1);
}

void
clearstat()
{
	if (uid) {
		if (sflag) {
			move(BOS, 0);
			clrtoeol();
			move(BOS, 0);
			pptr("You are not superuser");
			refresh();
			return;
		}
		goto refuse;
	}
	fsq.num = -1;
	map_smt(0,FDDI_SNMP_FSDELETE,(char*)&fsq,sizeof(fsq),devnum);
	return;
refuse:
	fprintf(stderr, "You are not superuser\n");
	quit(1);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int fc;
	register int i;
	register char *cp;
	uid = geteuid();

	fsq.num = 0;

	sm_openlog(SM_LOGON_STDERR, LOG_ERR, 0, 0, 0);

	argc--, argv++;
	while (argc) {
		if (!strcmp(*argv, "-f")) {
			delsome = 1;
		} else if (!strcmp(*argv, "-a")) {
			sflag = 1;
			argc--, argv++;
			continue;
		} else if (!strcmp(*argv, "-i")) {
			argc--, argv++;
			devnum = atoi(*argv);
			argc--, argv++;
			continue;
		} else {
			goto usage;
		}

		argc--, argv++;
		if (argc <= 0)
			goto usage;

		if (isdigit(**argv)) {
			fc = atoi(*argv);
		} else {
			cp = *argv;

			if (!strcasecmp("NIF", cp))
				fc = SMT_FC_NIF;
			else if (!strcasecmp("CSIF", cp))
				fc = SMT_FC_CONF_SIF;
			else if (!strcasecmp("OSIF", cp))
				fc = SMT_FC_OP_SIF;
			else if (!strcasecmp("ECF", cp))
				fc = SMT_FC_ECF;
			else if (!strcasecmp("RAF", cp))
				fc = SMT_FC_RAF;
			else if (!strcasecmp("SRF", cp))
				fc = SMT_FC_SRF;
			else if (!strcasecmp("GET_PMF", cp))
				fc = SMT_FC_GET_PMF;
			else if (!strcasecmp("CHG_PMF", cp))
				fc = SMT_FC_CHG_PMF;
			else if (!strcasecmp("ADD_PMF", cp))
				fc = SMT_FC_ADD_PMF;
			else if (!strcasecmp("RM_PMF", cp))
				fc = SMT_FC_RMV_PMF;
			else if (!strcasecmp("ESF", cp))
				fc = SMT_FC_ESF;
			else if (!strcasecmp("ALL", cp)) {
				delall = 1;
				break;
			} else {
				fprintf(stderr, "%s: invalid frame class\n",cp);
				exit(1);
			}
		}

		fsq.entry[fsq.num].fc = fc;
		fsq.num++;
		argc--, argv++;
	}

	signal(SIGINT, quit);
	signal(SIGTERM, quit);

	if (delall) {
		if (uid) goto refuse;
		fsq.num = 0;
		map_smt(0,FDDI_SNMP_FSDELETE,(char*)&fsq,sizeof(fsq),devnum);
	} else if (delsome) {
		if (uid) goto refuse;
		fsq.num = -3;
		map_smt(0,FDDI_SNMP_FSDELETE,(char*)&fsq,sizeof(fsq),devnum);
	} else if (sflag) {
		spr();
	} else if (fsq.num == 0) {
		map_smt(0,FDDI_SNMP_FSSTAT,(char*)&fsq,sizeof(fsq),devnum);
		for (i = 0; i < SMT_MAXFSENTRY && i < fsq.num; i++) {
			register SMT_FS_INFO *e = &fsq.entry[i];
			printf("%d: %-.5s, port %d, type %s, timo %d, num %d\n",
			       i, ma_fc_str((u_char)e->fc),
			       (int)e->fsi_to.sin_port,
			       e->ft == 0 ? "ALL" : ma_ft_str((u_char)e->ft),
			       e->timo, e->svcnum);
		}
	}

	quit(0);

refuse:
	fprintf(stderr, "You are not superuser\n");
	quit(1);

usage:
	sm_log(LOG_ERR, 0, USAGE);
	quit(1);
}

/*
 * Print a description of the station.
 */
#define P(lab,fmt,val) \
	{ \
	    if (first) { move(y,xlab); pptr("%-.*s", LABEL_LEN, lab); } \
	    move(y,x); \
	    pptr(fmt,(val)); \
	    y++; \
	}

static void
spr_fsq(int cmd, register int y, char *tmb)
{
	register int i, x = 0;

	pptr("   %d: Frame Status -- %s", cmd, tmb);

	y++;
	if (first) {
	    move(y,x);
	    pptr("                INPUT                      OUTPUT          ");
	    y++; move(y,x);
	    pptr("FC    request  response anounce  request  response anounce ");
	    y++;
	} else
	    y += 2;

	map_smt(0,FDDI_SNMP_STAT,(char*)&fsq,sizeof(fsq),devnum);
	for (i = 0; i < SMT_MAXFSENTRY && i < fsq.num; i++) {
		register SMT_FS_INFO *e = &fsq.entry[i];
		register SMT_FS_INFO *ze = &zfsq.entry[i];
		if (first) {
			move(y,x);
			pptr("%-5.5s", ma_fc_str((u_char)e->fss_fc));
		}
		x += 6; move(y,x);
		pptr("%-08d %-08d %-08d %-08d %-08d %-08d",
			e->fss_ireq - ze->fss_ireq,
			e->fss_ires - ze->fss_ires,
			e->fss_ianoun - ze->fss_ianoun,
			e->fss_oreq - ze->fss_oreq,
			e->fss_ores - ze->fss_ores,
			e->fss_oanoun - ze->fss_oanoun);
		y++; x = 0;
	}
}

#define NUMFUNCS 1

static void (*sprfuncs[NUMFUNCS])(int, int, char *) = {
    spr_fsq,
};

const char min_cmd_num = '1';
const char max_cmd_num = '1' + NUMFUNCS - 1;

static void
spr(void)
{
	time_t t;
	char tmb[26];
	struct timeval wait;
	int c, n;
	fd_set rmask;
	struct termio  tb;
	int intrchar;   /* user's interrupt character */
	int suspchar;   /* job control character */
	int scmd = 0;	/* default scmd = MAC */

	if (interval == 0)
		interval = 1;	/* default to 1 sec interval */

#ifdef _IRIX4
	pptr = printw;
#else
	pptr = (int (*)(const char *, ...))printw;
#endif
	initscr();
	raw();
	noecho();
	keypad(stdscr, TRUE);
	leaveok(stdscr, FALSE);
	move(0, 0);

	(void) ioctl(0, TCGETA, &tb);
	intrchar = tb.c_cc[VINTR];
	suspchar = tb.c_cc[VSUSP];

	FD_ZERO(&rmask);


	while (1) {
		if (first) {
			clear();
		}
		t = time(0);
		bcopy(ctime(&t), tmb, 24);
		tmb[24] = 0;

		sprfuncs[scmd](scmd+1, 1, tmb);

		if (first) {
			standout();
			move(BOS, 0);
			pptr("1:Frame");
			standend();
		}
		move(0, 0);
		refresh();
		first = 0;
		zero = 0;

		FD_SET(0, &rmask);
		wait.tv_sec = interval;
		wait.tv_usec = 0;
		n = select(1, &rmask, NULL, NULL, &wait);
		if (n < 0) {
			fail("select: %s", strerror(errno));
			break;
		} else if (n == 1) {
			c = getch();
			if (c == intrchar || c == 'q' || c == 'Q') {
				quit(0);
				break;
			} else if (c == suspchar) {
				reset_shell_mode();
				kill(getpid(), SIGTSTP);
				reset_prog_mode();
			} else if (c == 'z') {
				zero = first = 1;
				bcopy((char*)&fsq, (char*)&zfsq, sizeof(fsq));
			} else if (c == 'r') {
				bzero((char*)&zfsq, sizeof(zfsq));
				first = 1;
			} else if (c == ctrl_l) {
				first = 1;
			} else if ((c >= min_cmd_num) && (c <= max_cmd_num)) {
				scmd = c - '1';
				first = 1;
			}
		}
	}
}
