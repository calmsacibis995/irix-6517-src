/* un-mount a MS-DOS filesystem and kill the associated mountdos process */

#include <errno.h>
#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <stdarg.h>
#include <syslog.h>



static char gfstab[] = "/etc/gfstab";
static char *progname;

static int nopt(struct mntent *, char *);
static void err_printf(char *format, ...);



main(int argc, char **argv)
{
	char *dir;
	FILE *fptr;
	char *fsname;
	struct mntent *mntp;
	int pid;
	char *type;

	progname = argv[0];

	/* invocation via umount */
	if (argc != 5) {
		err_printf("usage: %s fsname dir type opts\n", progname);
		exit(2);
	}

	fsname = argv[1];
	dir = argv[2];
	type = argv[3];

	if ((fptr = setmntent(gfstab, "r")) == NULL) {
		err_printf("%s: can't open %s: %s\n",
			progname, gfstab, strerror(errno));
		exit(1);
	}

	/* scan /etc/gfstab and find the matching entry */
	while ((mntp = getmntent(fptr)) != NULL)
		if (strcmp(mntp->mnt_fsname, fsname) == 0
		    && strcmp(mntp->mnt_dir, dir) == 0
		    && strcmp(mntp->mnt_type, type) == 0) {

			pid = nopt(mntp, "pid");
			if (!pid) {
				err_printf("%s: can't determine pid\n",
					progname);
				exit(1);
			}
			
			/*
			 *  Kill mounthfs, then repeatedly send signal 0
			 *  until the process isn't there.
			 *  This verifies that the filesystem is really
			 *  dismounted before umounthfs returns.
			 */

			if (kill(pid, SIGTERM) == 0) {
			    do {
			        sleep(1);
			    } while (kill(pid, 0) == 0);
			}
			if (errno != ESRCH) {
				err_printf("%s: can't kill process %d: %s\n",
					   progname, pid, strerror(errno));
				exit(1);
			}
			break;
		}

	endmntent(fptr);

	if (!mntp) {
		err_printf("%s: can't find %s in %s\n",
			progname,  argv[1], gfstab);
		exit(1);
	}

	exit (0);
}



/*
 * Return the value of a numeric option of the form foo=x, if
 * option is not found or is malformed, return 0.
 */
static int
nopt(struct mntent *mnt, char *opt)
{
	char *equal;
	char *index();
	char *str;
	int val = 0;

	if (str = hasmntopt(mnt, opt)) {
		if (equal = index(str, '='))
			val = atoi(&equal[1]);
		else
			err_printf("%s: bad numeric option '%s'\n",
				progname, str);
	}
	return (val);
}



static void
err_printf(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	if (isatty(2))
		vfprintf(stderr, format, ap);
	else
		vsyslog(LOG_ERR, format, ap);
	va_end(ap);
}
