/*
 * chkutent.c
 *			written by Gianni Mariani 16-Apr-1993
 *
 * /etc/utmp checker
 *
 *	    This checks the utmp file for USER_PROCESS entries that
 *  should really be DEAD_PROCESS.  It bascially makes sure that the
 *  process id in the utmp entry is actually a process.  In some
 *  rare cases, the process may exist and it is not the login process.
 *  Bad utmp entries are normally created by an "abnormal" xterm or
 *  xwsh termination.  This happens when the xserver clobbers these
 *  processes on logout.  If this is run in the xdm login then it will
 *  be a very rare case of leaving bad utmp entries around.
 *  
 *  	1/28/94, 'bowen' (Jerre Bowen):  enhanced to scrub the extended
 *  format files of 5.x releases (utmpx and wtmpx)
 *  The additional and more precise information
 *  contained in the extended files is discarded whenever the entries
 *  don't match; acct(1M) and last(1) use the wtmp and wtmpx files as
 *  their database, and orphaned USER entries confuse them.  do_others()
 *  SVR4 version updates wtmp and wtmpx: the 5.0 pututline fixes utmpx.
 *  
 *  	7/17/98:  Made changes get rid of calls to the library routines
 *  since they can't do exactly what we really want to do.  Also make
 *  sure that we lock the utmpx file during the entire process to keep
 *  other processes from trying to update it while we are "fixing" it.
 */

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utmpx.h>

/* #define DEBUG */
struct utmpx	*cut_ptr;
struct utmpx	*uxp;
static void	do_others(char *, struct utmpx *);
static int dupchk(int, char *);
static int utentchk(int, char *, char *);
static void setent(struct utmpx *);
static void lock_utmp(int, char *);
static void unlock_utmp(int);
extern int synchutmp(const char *, const char *);
char *Cmd;

int
main(int argc, char **argv)
{
    int	c;
    int	ufd;
    char *utxfile = UTMPX_FILE;
    char *wtxfile = WTMPX_FILE;
    int gotf = 0, gotw = 0;

    Cmd = argv[0];

    while ((c = getopt(argc, argv, "f:w:")) != EOF)
	switch (c) {
	case 'f':
		utxfile = optarg;
		if (utmpxname(utxfile) == 0) {
			fprintf(stderr, "chkutent:invalid utmpx file name\n");
			exit(1);
		}
		gotf = 1;
		break;
	case 'w':
		wtxfile = optarg;
		gotw = 1;
		break;
	}

    if (gotf && !gotw)
	wtxfile = NULL;

    if ((ufd = open(utxfile, O_RDWR)) < 0) {
	fprintf(stderr, "%s:cannot open %s for write:%s\n",
		Cmd, utxfile, strerror(errno));
	return -1;
    }

    (void)lock_utmp(ufd, utxfile);

    /* find dead processes, and mark them as DEAD */
    if (utentchk(ufd, utxfile, wtxfile) != 0) {
	(void)unlock_utmp(ufd);
	(void)close(ufd);
	exit(1);
    }

    /* check for, and fix, duplicate entries */
    if (dupchk(ufd, utxfile) != 0) {
	(void)unlock_utmp(ufd);
	(void)close(ufd);
	exit(1);
    }

    /* try to force a syncronization of system files */
    if (!gotf)
    	(void)synchutmp(UTMP_FILE, UTMPX_FILE);

    (void)unlock_utmp(ufd);
    (void)close(ufd);

    return 0;
}


/*
 * lock_utmp()
 *
 * Locks the utmpx file
 *
 */

void
lock_utmp(int ufd, char *xfile)
{
	struct flock lock;
	int rv;

	lock.l_type = F_WRLCK;
	lock.l_whence = 0;
	lock.l_start = 0;
	lock.l_len = 0;

	/* Try to acquire lock. Retry if an interrupt occurs. */
	while ((rv = fcntl(ufd, F_SETLKW, &lock) < 0) && (errno == EINTR));
	if (rv == -1)
		fprintf(stderr, "%s: WARNING - cannot lock file %s for write: %s\n",
			Cmd, xfile, strerror(errno));

	return;
}


/*
 * unlock_utmp()
 *
 * Unlock the utmpx file
 *
 */

void
unlock_utmp(int ufd)
{
	struct flock lock;

	lock.l_type = F_UNLCK;
	lock.l_whence = 0;
	lock.l_start = 0;
	lock.l_len = 0;

	(void)fcntl(ufd, F_SETLK, &lock);

	return;
}


/* do_others: if the entry was wrong in utmpx, it's wrong in utmp,
 * wtmp, and wtmpx also.  When the *tmp and *tmpx files become
 * out of sync, 'last' and Co. ignore the extended info (i.e. the
 * remote host and user) until they can again find a synchronized entry.
 * 
 * - SVR4 version:
 *   - pututxline() automatically updates utmp file with the line it
 *     writes to the utmpx file, so we only worry about wtmp and wtmpx.
 *   - The wtmp and wtmpx files each require new entries; create and
 *     append to end.
 */

static void
do_others(char *wtxfile, struct utmpx *utmp_p)
{
    struct utmpx utx;
    /* 
     * updwtmpx creates new wtmp & wtmpx entries using utx's values
     * and appends them to end of wtmp & wtmpx files.
     * 'last' expects nulled-out name (ut_user field).
     */
    utx = *utmp_p;
    utx.ut_user[0] = '\0';
    if (wtxfile)
	    updwtmpx(wtxfile, &utx);
}

/*
 * check for dups
 *
 * We only check utmpx - if we write that, the next getutxline should
 * force the appropriate syncronization.
 */

static int
dupchk(int fdx, char *xfile)
{
	int found;
	struct utmpx utx;
	struct utmpx empty;
	struct ent {
		char ut_id[4];
		short ut_type;
		off_t ut_off;
	} *lines = NULL;
	struct ent *lp;
	int nent;
	ssize_t rv;
	off_t offset;

	/* check utmpx file */
	lseek(fdx, 0, SEEK_SET);
	offset = 0;
	lines = malloc(sizeof(*lines));
	nent = 1;
	lines[nent-1].ut_type = -1;

	while (read(fdx, &utx, sizeof(utx)) == sizeof(utx)) {
		if (utx.ut_type != USER_PROCESS &&
				utx.ut_type != LOGIN_PROCESS &&
				utx.ut_type != DEAD_PROCESS &&
				utx.ut_type != INIT_PROCESS) {
			if (utx.ut_type > UTMAXTYPE) {
				lseek(fdx, -(off_t)sizeof(utx), SEEK_CUR);
				utx.ut_type = EMPTY;
				rv = write(fdx, &utx, sizeof(utx));
				if (rv != sizeof(utx)) {
					fprintf(stderr,
					  "%s:ERROR:write failed on:%s :%s\n",
					  Cmd, xfile, strerror(errno));
					return -1;
				}
				fprintf(stdout,
				    "utmpx bad type %d - see chkutent(1M)\n",
				    utx.ut_type);
			}
			offset += sizeof(utx);
			continue;
		}

		/* check for dup */
		found = 0;
		for (lp = lines; lp->ut_type >= 0; lp++) {
			if (strncmp(lp->ut_id, utx.ut_id,
					sizeof(utx.ut_id)) == 0) {
				/* a dup! */
				fprintf(
				    stdout,
				    "utmpx dup - %.32s %s %.4s %d - see chkutent(1M)\n",
				    utx.ut_user,
				    utx.ut_line,
				    utx.ut_id,
				    utx.ut_pid
				);
				found = 1;
				break;
			}
		}

		if (!found) {	/* no dup - add to list */
			nent++;
			lines = realloc(lines, (nent * sizeof(*lines)));
			lines[nent-2].ut_type = utx.ut_type;
			strncpy(lines[nent-2].ut_id, utx.ut_id, 4);
			lines[nent-2].ut_off = offset;
			lines[nent-1].ut_type = -1;
			offset += sizeof(utx);
		} else {	/* this is a dup */
			/*
			 * to fix it we need to 'EMPTY' out one of
			 * the entries, of course we want to
			 * to EMPTY out a DEAD entry if possible.
			 */
			if (utx.ut_type == DEAD_PROCESS) {
				/* easy! */
				lseek(fdx, -(off_t)sizeof(utx), SEEK_CUR);
				utx.ut_type = EMPTY;
				rv = write(fdx, &utx, sizeof(utx));
				if (rv != sizeof(utx)) {
					fprintf(stderr,
					  "%s:ERROR:write failed on:%s :%s\n",
					  Cmd, xfile, strerror(errno));
					return -1;
				}
				offset += sizeof(utx);
			} else if (lp->ut_type == DEAD_PROCESS) {
				/* its before us ... go back and change it */
				lseek(fdx, lp->ut_off, SEEK_SET);
				if (read(fdx, &empty, sizeof(empty)) != sizeof(empty)) {
					/* This should never happen, but just
					 * in case it does ...
					 * lets at least make sure that the
					 * 'empty' structure isn't full of
					 * garbage. (it shouldn't really matter)
					 */
					empty = utx;
					setent(&empty);
				}
				empty.ut_type = EMPTY;
				lseek(fdx, lp->ut_off, SEEK_SET);
				rv = write(fdx, &empty, sizeof(utx));
				if (rv != sizeof(utx)) {
					fprintf(stderr,
					  "%s:ERROR:write failed on:%s :%s\n",
					  Cmd, xfile, strerror(errno));
					return -1;
				}
				/*
				 * In lp, throw away the info for the EMPTY
				 * process, and replace it with the info for
				 * the valid USER_PROCESS.
				 */
				lp->ut_type = utx.ut_type;
				strncpy(lp->ut_id, utx.ut_id, 4);
				lp->ut_off = offset;
				offset += sizeof(utx);

				/* go back to where we were */
				lseek(fdx, offset, SEEK_SET);
			} else {
				/* hard - punt */
				fprintf(stderr,
				  "%s:utmpx file %s CORRUPT with dup entry and neither entry 'DEAD'\n",
					Cmd, xfile);
				return -1;
			}
		}
	}

	free(lines);

	return 0;
}


/*
 * Check for broken USER_PROCESS entries.
 */

static int
utentchk(int fdx, char *xfile, char *wtxfile)
{
	struct utmpx utx;
	ssize_t rv;

	while (read(fdx, &utx, sizeof(utx)) == sizeof(utx)) {
		if (utx.ut_type != USER_PROCESS)
			continue;

		if (utx.ut_type == USER_PROCESS) {
			if ((kill(utx.ut_pid, 0) == -1) &&
					(oserror() == ESRCH)) {
				setent(&utx); /* set to DEAD */
				lseek(fdx, -(off_t)sizeof(utx), SEEK_CUR);
				rv = write(fdx, &utx, sizeof(utx));
				if (rv != sizeof(utx)) {
					fprintf(stderr,
					  "%s:ERROR:write failed on:%s :%s\n",
					  Cmd, xfile, strerror(errno));
					return -1;
				}
				do_others(wtxfile, &utx);
			}
		}
	}

	return 0;
}


static void
setent(struct utmpx *ut)
{
	fprintf(stdout,
	    "utmpx fix - %.32s %s %.4s %d - see chkutent(1M)\n",
	    ut->ut_user,
	    ut->ut_line,
	    ut->ut_id,
	    ut->ut_pid
	);

	ut->ut_type = DEAD_PROCESS;
	ut->ut_exit.e_termination = -1;
	ut->ut_exit.e_exit = -1;
	gettimeofday(&ut->ut_tv);
}

