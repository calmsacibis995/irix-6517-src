#ifndef lint
static  char sccsid[] = "@(#)biod.c	1.1 88/03/08 4.0NFSSRC; from 1.6 88/02/07 Copyr 1983 Sun Micro";
#endif

#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/capability.h>

/*
 * This is the NFS asynchronous block I/O daemon
 */

static void
usage(name)
	char	*name;
{
	fprintf(stderr, "usage: %s [<count>]\n", name);
	exit(1);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int pid;
	int count;

	if (argc > 2) {
		usage(argv[0]);
	}

	if (argc == 2) {
		count = atoi(argv[1]);
		if (count < 0) {
			usage(argv[0]);
		}
	} else {
		count = 1;
	}

	{ int d = getdtablesize();
		while (--d >= 0)
			(void) close(d);
	}
	{ int tt = open("/dev/tty", O_RDWR);
		if (tt >= 0) {
			ioctl(tt, TIOCNOTTY, 0);
			close(tt);
		}
	}
	while (count--) {
		pid = fork();
		if (pid == 0) {
			cap_value_t cap_mount_mgt = CAP_MOUNT_MGT;
			cap_t ocap = cap_acquire(1, &cap_mount_mgt);
			async_daemon();		/* Should never return */
			cap_surrender(ocap);
			syslog(LOG_ERR, "%s: %m", argv[0]);
			exit(1);
		}
		if (pid < 0) {
			syslog(LOG_ERR, "%s: cannot fork: %m", argv[0]);
			exit(1);
		}
	}
}
