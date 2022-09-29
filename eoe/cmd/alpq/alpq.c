/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)alpq:alpq.c	1.2"			*/

#ident 	"$Revision: 1.1 $"

/*
 * alpq.c	This program queries the "alp" module.  The query is
 *		accomplished by pushing the module on top of the
 *		stdin stream, then sending "query" ioctls in
 *		sequence until the list is exhausted.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <errno.h>
#include <locale.h>
#include <pfmt.h>
#include <string.h>
#include <sys/alp.h>

main(argc, argv)
	int argc;
	char **argv;
{
	struct strioctl sb;
	struct alp_q a;
	register int i;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxmesg");
	(void)setlabel("UX:alpq");

	if (ioctl(0, I_PUSH, "alp") != 0) {
		printf("errno=%d.\n", errno);
		pfmt(stderr, MM_ERROR, ":101:Cannot push `alp': %s\n",
			strerror(errno));
		exit(1);
	}
	sb.ic_cmd = ALP_QUERY;
	sb.ic_timout = 30;
	sb.ic_dp = (char *) &a;
	sb.ic_len = sizeof(struct alp_q);
	i = a.a_seq = 0;
	errno = 0;
	/*
	 * Query until non-zero, incrementing sequence number.
	 */
	while (ioctl(0, I_STR, &sb) == 0) {
		printf("%2d%c %-16s (%s)\n",
			i + 1,
			(a.a_flag ? '*' : ' '),
			a.a_name,
			a.a_expl);
		a.a_seq = ++i;
		a.a_name[0] = '\0';
		a.a_expl[0] = '\0';
	}
	if (ioctl(0, I_POP, 0) != 0) {
		pfmt(stderr, MM_ERROR, ":102:Cannot pop `alp': %s\n",
			strerror(errno));
		exit(1);
	}
	exit(0);
}
