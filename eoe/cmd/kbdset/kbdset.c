/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)kbdset:kbdset.c	1.2" 			*/

#ident  "$Revision: 1.1 $"

/*
 * kbdset.c	This program attaches kbd maps.  This is main().
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stropts.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <pfmt.h>
#include <string.h>
#include <unistd.h>
/* #include <sys/kbd.h> */
#include <sys/kbdm.h>
extern int optind, opterr;
extern char *optarg;
char *prog;
int uid, euid;
int outside;

main(argc, argv)

	int argc;
	char **argv;
{
	register int c, fd, pub;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxmesg");
	(void)setlabel("UX:kbdset");

	opterr = outside = 0;
	prog = *argv;
	while ((c = getopt(argc, argv, "oqa:d:v:k:m:t:")) != EOF) {
		switch (c) {
		case 'o':
			outside = 1;
			break;
		case 'q':
			doquery();
			exit(0);
			break;
		case 'a':
			tach(optarg);
			break;
		case 'd':
			detach(optarg);
			break;
		case 'v':
			verbose(optarg);
			break;
		case 'k':
		case 'm':
			hotkey(c, optarg);
			break;
		case 't':
			timer(optarg);
			break;
		case '?':
		default:
usage:
			pfmt(stderr, MM_ACTION,
				":142:Usage: %s [-oq] [-{a|d} table] [-v str] [-k hot] [-m n] [-t n]\n",
				prog);
			exit(1);
		}
	}
	if (optind < argc){
		pfmt(stderr, MM_ERROR, ":26:Incorrect usage\n");
		goto usage;
	}
	exit(0);
}

doquery()

{
	register int rval, i, first, seq, type;
	struct strioctl sb;
	struct kbd_ctl cx;
	struct kbd_query q;

	/*
	 * First, get timeout values (cx.c_type == IN, cx.c_arg = OUT)
	 */
	sb.ic_cmd = KBD_TGET;
	sb.ic_len = sizeof(struct kbd_ctl);
	sb.ic_timout = 15;
	sb.ic_dp = (char *) &cx;
	if (ioctl(0, I_STR, &sb) != 0)
		return;
	first = 1;
	type = KBD_QF_PUB;
	seq = 0;
	/*
	 * loop requesting the "next" on the list.  Begin with type
	 * "pub", then do "pri".  Keep sending ioctls until one fails.
	 * If errno is EAGAIN, we're finished.  Otherwise, it's a hard
	 * error of some time.
	 */
	for ( ; ; ) {	/* loop until "rval" gets error */
		sb.ic_cmd = KBD_LIST;	/* do these 4 always, just in case */
		sb.ic_len = sizeof(struct kbd_query);
		sb.ic_timout = 15;
		sb.ic_dp = (char *) &q;

		q.q_flag = type;
		q.q_seq = seq++;
		rval = ioctl(0, I_STR, &sb);
		if (rval) {
			if (errno != EAGAIN) {
				pfmt(stderr, MM_ERROR, ":143:Query failed: %s\n",
					strerror(errno));
				exit(1);
			}
			else {	/* EAGAIN */
				if (type) {
					type = 0;
					seq = 0;
					continue;
				}
				else
					return;	/* finished pub & pri */
			}
		}
		if (first) {
			if (q.q_hkin) {
				pfmt(stdout, MM_NOSTD, ":144:In Hot Key = ");
				pkey(q.q_hkin);
				printf("\n");
			}
			if (q.q_hkout) {
				pfmt(stdout, MM_NOSTD, ":145:Out Hot Key = ");
				pkey(q.q_hkout);
				printf("\n");
			}
			pfmt(stdout, MM_NOSTD, ":146:Timers: In = %d ; Out = %d\n",
				(int) cx.c_type, (int) cx.c_arg);
/*
 * FIX ME: should have a way to retrieve timer information.
 */
			pfmt(stdout, MM_NOSTD, ":147:ID        Name             Size I/O Ref Cmp Type\n");
			first = 0;
		}
		fflush(stdout);
		q.q_name[KBDNL-1] = '\0';
		pfmt(stdout, MM_NOSTD, ":148:%8x  %-16s %4d %s %s %3d ", q.q_id,
			q.q_name, q.q_asize,
			(q.q_tach & Z_UP) ? gettxt(":149", "i") : "-",
			(q.q_tach & Z_DOWN) ? gettxt(":150", "o") : "-",
			q.q_ref);
		if (q.q_flag & KBD_QF_COT)
			pfmt(stdout, MM_NOSTD, ":151: %2d ", q.q_nchild);
		else
			pfmt(stdout, MM_NOSTD, ":152:  - ");

		if (q.q_flag & KBD_QF_EXT)  /* external always public */
			pfmt(stdout, MM_NOSTD, ":153: ext");
		else
			pfmt(stdout, MM_NOSTD, ":154:%c%s", (q.q_flag & KBD_QF_TIM) ? '*' : ' ',
				(q.q_flag & KBD_QF_PUB) ?
					gettxt(":155", "pub") :
					gettxt(":156", "pri"));
		if (q.q_flag & KBD_QF_COT) {
			for (i = 0; i < q.q_nchild; i++) {
				if ((i & 3) == 0)
					pfmt(stdout, MM_NOSTD, ":157:\n            ");
				pfmt(stdout, MM_NOSTD, ":158:[%8x]%s ", q.q_child[i],
					q.q_chtim[i] ? "*" : " ");
			}
		}
		printf("\n");
		fflush(stdout);
	}
}

tach(name)	/* attach a table by name */
	char *name;
{
	struct strioctl sb;
	struct kbd_tach t;
	struct kbd_ctl c;
	register int rval;

	sb.ic_cmd = KBD_ATTACH;
	sb.ic_timout = 15;
	sb.ic_len = sizeof(struct kbd_tach);
	sb.ic_dp = (char *) &t;

	strcpy((char *)t.t_table, name);
	if (! outside)
		t.t_type = Z_UP;
	else
		t.t_type = Z_DOWN;

	rval = ioctl(0, I_STR, &sb);
	if (rval) {
		pfmt(stderr, MM_ERROR, ":159:Attach failed: %s\n",
			strerror(errno));
		exit(1);
	}

	sb.ic_cmd = KBD_ON;
	sb.ic_timout = 15;
	sb.ic_len = sizeof(struct kbd_ctl);
	sb.ic_dp = (char *) &c;
	c.c_type = (outside ? Z_DOWN : Z_UP);

	rval = ioctl(0, I_STR, &sb);
	if (rval) {
		pfmt(stderr, MM_ERROR, ":141:Cannot turn on mapping: %s\n",
			strerror(errno));
		exit(1);
	}
}

hotkey(type, s) 		/* set the hot key and/or mode */
	int type;	/* 'k' or 'm' */
	unsigned char *s;
{
	struct strioctl sb;
	struct kbd_ctl cx;
	register int rval;

	/*
	 * First, get current hot key
	 */
	sb.ic_cmd = KBD_HOTKEY;
	sb.ic_timout = 15;
	sb.ic_len = sizeof(struct kbd_ctl);
	sb.ic_dp = (char *) &cx;
	if (outside)
		cx.c_type = Z_GET | Z_DOWN;
	else
		cx.c_type = Z_GET | Z_UP;
	rval = ioctl(0, I_STR, &sb);
	if (rval) {
		pfmt(stderr, MM_ERROR, ":160:Cannot retrieve hotkey: %s\n",
			strerror(errno));
		exit(1);
	}
	/*
	 * Figure out whether to set key (k) or mode (m)
	 */
	if (type == 'k')
		cx.c_arg = (cx.c_arg & 0xFF00) | *s;
	else
		cx.c_arg = (cx.c_arg & 0x00FF) | (atoi(s) << 8);

	sb.ic_cmd = KBD_HOTKEY;
	sb.ic_timout = 15;
	sb.ic_len = sizeof(struct kbd_ctl);
	sb.ic_dp = (char *) &cx;
	if (outside)
		cx.c_type = Z_SET | Z_DOWN;
	else
		cx.c_type = Z_SET | Z_UP;
	rval = ioctl(0, I_STR, &sb);
	if (rval) {
		pfmt(stderr, MM_ERROR, ":161:Cannot set hotkey: %s\n",
			strerror(errno));
		exit(1);
	}
}

/*
 * Reset timer value for timed-mode tables.  Reset independent for
 * "in" and "out" sides.
 */

timer(s)
	unsigned char *s;
{
	struct strioctl sb;
	struct kbd_ctl cx;
	register int rval;

	sb.ic_cmd = KBD_TSET;
	sb.ic_timout = 15;
	sb.ic_len = sizeof(struct kbd_ctl);
	sb.ic_dp = (char *) &cx;
	if (outside)
		cx.c_type = Z_DOWN;
	else
		cx.c_type = Z_UP;
	cx.c_arg = atoi(s);
	rval = ioctl(0, I_STR, &sb);
	if (rval) {
		pfmt(stderr, MM_ERROR, ":162:Cannot reset timer: %s\n",
			strerror(errno));
		exit(1);
	}
}

/*
 * Turn on verbose mode.
 */

verbose(s)
	unsigned char *s;
{
	struct strioctl sb;
	struct kbd_ctl cx;
	register int rval;

	sb.ic_cmd = KBD_VERB;
	sb.ic_timout = 15;
	sb.ic_len = strlen((char *)s) + 1;
	sb.ic_dp = (char *) s;

	rval = ioctl(0, I_STR, &sb);
	if (rval) {
		pfmt(stderr, MM_ERROR, ":163:Cannot set verbose mode: %s\n",
			strerror(errno));
		exit(1);
	}
}

/*
 * Print a representation of a key
 */

pkey(c)
	unsigned char c;
{
	if (c & 0x80) {
		printf("\\%03o", (int) (c & 0xFF));
		return;
	}
	if (c == '\177') {
		pfmt(stdout, MM_NOSTD, ":164:DEL");
		return;
	}
	if (c < ' ' || c > '~') {
		putchar('^');
		c += '@';
	}
	if (c == ' ')
		pfmt(stdout, MM_NOSTD, ":165:SPACE");
	else
		putchar(c);
}

detach(name)
	char *name;
{
	struct strioctl sb;
	struct kbd_tach t;
	struct kbd_ctl c;
	register int rval;

	sb.ic_cmd = KBD_DETACH;
	sb.ic_timout = 15;
	sb.ic_len = sizeof(struct kbd_tach);
	sb.ic_dp = (char *) &t;

	strcpy((char *)t.t_table, name);
	if (! outside)
		t.t_type = Z_UP;
	else
		t.t_type = Z_DOWN;

	rval = ioctl(0, I_STR, &sb);
	if (rval) {
		pfmt(stderr, MM_ERROR, ":166:Cannot detach: %s\n",
			strerror(errno));
		exit(1);
	}
}
