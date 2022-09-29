/*
 *  umountcd.c
 *
 *  Description:
 *      program to un-mount an ISO 9660 filesystem and kill the
 *      associated mountcd process
 *
 *  History:
 *      rogerc      01/17/91    Cannibalized umountdos.c
 *      rogerc      01/31/91    Rewrote to use mntent
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <mntent.h>

static char *progname;

static int
nopt( struct mntent *mnt, char *opt );

static char gfstab[] = "/etc/gfstab";

main(int argc, char **argv)
{
	FILE            *fptr;
	int             pid;
	struct mntent   *mntp;
	char            buf[80], *fsname, *dir, *type;
	int             killedIt = 0;

	progname = argv[0];

	if (argc < 4 ) {
	  fprintf(stderr, "usage: %s fsname dir type opts\n", progname);
	  exit(2);
	}

	fsname = argv[1];
	dir = argv[2];
	type = argv[3];

	/*
	 *  If the mount is remote, we don't have to do anything
	 */
	if (strchr( fsname, ':' ))
		exit (0);

	if ((fptr = setmntent(gfstab, "r")) == NULL) {
		sprintf(buf, "Can't open %s", gfstab);
		perror(buf);
		exit(1);
	}

	/* scan /etc/gfstab and find the matching entry */
	while ((mntp = getmntent( fptr )) != NULL) {
 		if (strcmp( mntp->mnt_fsname, fsname ) == 0 &&
		 strcmp( mntp->mnt_dir, dir ) == 0
		 && (strcmp( mntp->mnt_type, type ) == 0
#ifdef TESTISO
		 || strcmp( mntp->mnt_type, "testiso" ) ==0
#endif
		 )) {

			pid = nopt( mntp, "pid" );
			if (!pid) {
				fprintf(stderr, "Can't determine pid\n");
				exit(1);
			}
			
			/*
			 *  Kill mountcd, then repeatedly send signal 0
			 *  until the process isn't there.
			 *  This verifies that the filesystem is really
			 *  dismounted before umountcd returns.
			 */

			if (kill( pid, SIGTERM ) == 0) {
				do {
					sleep(1);
				} while (kill(pid, 0) == 0);
				killedIt = 1;
				break;
			}
		}
	}

	endmntent( fptr );

	if (!mntp) {
		fprintf(stderr, "Can't find %s in %s\n", argv[1], gfstab);
		exit(1);
	} else if (!killedIt) {
		/*
		 * We found an entry that matched, but we couldn't
		 * kill the matching daemon.
		 */
		sprintf(buf, "Can't kill process %d.  Check %s.",
			pid, gfstab);
		perror(buf);
		exit(1);
	}

	exit (0);
}

/*
 * Return the value of a numeric option of the form foo=x, if
 * option is not found or is malformed, return 0.
 */
static int
nopt( struct mntent *mnt, char *opt )
{
	int val = 0;
	char *equal, *index();
	char *str;

	if (str = hasmntopt(mnt, opt)) {
		if (equal = index(str, '=')) {
			val = atoi(&equal[1]);
		} else {
			(void) fprintf(stderr, "%s: bad numeric option '%s'\n",
			 progname, str );
		}
	}
	return (val);
}
