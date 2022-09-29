#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/stress/Istress/RCS/mstress.c,v 1.2 1989/03/17 17:56:37 lee Exp $"

/*
 * mstress - stress mount point handling
 *
 * Basically have one process stat'ing a file in a mounted file system
 * and another mounting and unmounting
 *
 * Usage: mstress special mount-on fstyp file-to-stat [nreps]
 *
 * If one gives a directory then all files in that dir are stat'ed
 * If the first character of the fstyp is R then the mount is done
 * read-only
 *
 * Good file-to-stat to try (if one is mounting /usr) are:
 *	/usr/lib
 *	/usr/lib/some-file
 *	/usr/lib/../../
 *	/usr/lib/../../usr/lib
 */

#include "stdio.h"
#include "fcntl.h"
#include "errno.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/fsid.h"
#include "sys/fstyp.h"
#include "sys/mount.h"
#include "dirent.h"
#include "signal.h"

#define MAXDIRS 1000
char *fsarray[MAXDIRS];
int mflags = MS_FSS;

main(argc, argv)
 int argc;
 char **argv;
{
	extern void enddostat();
	char *spec;
	char *mnton;
	char *ftostat;
	char *ftyp;
	int fsno;
	struct stat sb;
	int nreps = 100;
	int nfiles;
	auto int statloc;
	int pid, spid;

	if (argc < 5) {
		Usage();
	}
	if (getuid() != 0) {
		fprintf(stderr, "Must be root to run this!\n");
		Usage();
	}
	spec = argv[1];
	mnton = argv[2];
	ftyp = argv[3];
	if (*ftyp == 'R') {
		mflags |= MS_RDONLY;
		ftyp++;
	}
	ftostat = argv[4];
	/* determine fstyp index for mount */
	if ((fsno = sysfs(GETFSIND, ftyp)) < 0) {
		fprintf(stderr, "File system %s not configured\n", ftyp);
		Usage();
	}

	if (argc > 5) {
		nreps = atoi(argv[5]);
	}
	
	/* make sure file to stat is stat'able */
	if (stat(ftostat, &sb) < 0) {
		fprintf(stderr, "Cannot stat %s\n", ftostat);
		Usage();
	}
	/* make sure can mount and unmount in queiscent system */
	/* don't assume current state - try unmounting first */
	if (umount(spec) < 0) {
		/* couldn't unmount: if EINVAL than wasn't; mounted else?? */
		if (errno != EINVAL) {
			fprintf(stderr, "Cannot unmount ");
			perror(spec);
			Usage();
		}
	}
	/* now try to mount */
	if (mount(spec, mnton, mflags, fsno) < 0) {
		fprintf(stderr, "Cannot mount %s on %s ", spec, mnton);
		perror("");
		Usage();
	}

	/* build set of files to stat */
	nfiles = buildftable(ftostat, fsarray);

	/* ready to go */
	printf("doing %d iterations mounting %s on %s type:%s\n",
		nreps, spec, mnton, ftyp);
	printf("stat'ing %s nfiles:%d\n", ftostat, nfiles);


	/* fork off 2 processes - a mounter and a stater */
	if ((spid = fork()) < 0) {
		perror("Cannot fork?");
		exit(1);
	} else if (spid == 0) {
		/* child 1 */
		signal(SIGUSR1, enddostat);
		dostat(fsarray, nfiles);
		exit(0);
	}
	/* parent */
	if ((pid = fork()) < 0) {
		perror("Cannot fork?");
		kill(spid, SIGUSR1);
		exit(1);
	} else if (pid == 0) {
		/* child 2 */
		domount(spec, mnton, fsno, nreps);
		exit(0);
	}
	/* parent */
		
	/* wait for mounter to finish */
	while (wait(&statloc) != pid)
		;
	/* stop stater */
	kill(spid, SIGUSR1);
}

Usage()
{
	fprintf(stderr, "Usage: mstress special mount-on fstyp file/dir-to-stat [nreps]\n");
	exit(1);
}

/*
 * build list of files to stat
 * - passed file is not a dir then just one, else all files in that dir
 * Returns: # of files to stat
 *       farray is a char**
 */
 int
buildftable(fname, farray)
 char *fname;
 char **farray;
{
	extern char *malloc();
	DIR *dhandle;
	struct dirent *dp;
	int n = 0;

	/* if we get a directory, then rotate stating each file
	 * else just stat file name
	 */
	if ((dhandle = opendir(fname)) == NULL) {
		farray[0] = fname;
		return(1);
	}
	while ((dp = readdir(dhandle)) != NULL) {
		farray[n] = malloc(strlen(dp->d_name)+1);
		strcpy(farray[n], dp->d_name);
		n++;
		if (n >= MAXDIRS)
			break;
	}
	closedir(dhandle);
	return(n);
}

int ngood = 0;		/* keep track of good vs bad stats */
int nbad = 0;

/* do stats until killed */
dostat(fname, nfiles)
 char *fname[];
 int nfiles;
{
	struct stat sb;
	register int i;
	int err;
		
loop:
	for (i = 0; i < nfiles; i++) {
		err = stat(fname[i], &sb);
		if (err < 0) {
			nbad++;
			if (errno != ENOENT) {
				perror("from stat");
			}
		} else {
			ngood++;
		}
	}
	goto loop;
}

void
enddostat()
{
	/* time to quit */
	printf("dostat:ngood:%d nbad:%d\n", ngood, nbad);
	exit(0);
}

/*
 * domount - do mount/unmount pairs
 * nreps are SUCCESSFUL umount/mount iterations
 */

int ngoodmounts = 0;
int nbadmounts = 0;
int ngoodumounts = 0;
int nbadumounts = 0;

domount(spec, dir, type, reps)
 char *spec, *dir;
 int type;
 int reps;
{
	/* starts off mounted */
	while (reps--) {
		while (umount(spec) < 0) {
			if (errno != EBUSY) {
				perror("unmount failed strangly");
				ngoodumounts--;
				break;
			}
			nbadumounts++;
		}
		ngoodumounts++;
		/* now try to mount */
		while (mount(spec, dir, mflags, type) < 0) {
			if (errno != EBUSY) {
				perror("mount failed strangly");
				ngoodmounts--;
				break;
			}
			nbadmounts++;
		}
		ngoodmounts++;
	}
	printf("badumount:%d badmounts:%d goodumounts:%d goodmounts:%d\n",
		nbadumounts, nbadmounts, ngoodumounts, ngoodmounts);
	return(0);
}
