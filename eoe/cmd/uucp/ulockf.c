/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uucp:ulockf.c	2.7"

#include "uucp.h"

#ifdef	V7
#define O_RDONLY	0
#endif

#ifdef sgi
void	stlock(char*);
char	lock_pid[SIZEOFPID+2];		/* +2 for '\n' and NULL */
#else
static void	stlock();
#endif
static int onelock(char*, char*, char*);

/*
 * create a lock file (file).
 * If one already exists, send a signal 0 to the process--if
 * it fails, then unlink it and make a new one.
 *
 * input:
 *	file - name of the lock file
 *	atime - is unused, but we keep it for lint compatibility with non-ATTSVKILL
 *
 * return:
 *	0	-> success
 *	FAIL	-> failure
 */
/* ARGSUSED */
ulockf(char *file, time_t atime)
{
	static char tempfile[MAXNAMESIZE];

	/* SLIP needs to change its PID.
	 * It might be ok to generate the lock string every time,
	 * but I do not know if uucico might not play games with
	 * children locking on behalf of parents.
	 */
	if (lock_pid[0] == '\0') {
		(void) sprintf(lock_pid, LOCKPID_PAT, getpid());
		(void) sprintf(tempfile, "%s/LTMP.%d", X_LOCKDIR, getpid());
	}
	if (onelock(lock_pid, tempfile, file) == -1) {
		(void) unlink(tempfile);
		if (checkLock(file))
			return(FAIL);
		else {
		    if (onelock(lock_pid, tempfile, file)) {
			(void) unlink(tempfile);
			DEBUG0(4,"ulockf failed in onelock()\n");
			return(FAIL);
		    }
		}
	}

	stlock(file);
	return(0);
}

/*
 * check to see if the lock file exists and is still active
 * - use kill(pid,0) - (this only works on ATTSV and some hacked
 * BSD systems at this time)
 * return:
 *	0	-> success (lock file removed - no longer active
 *	FAIL	-> lock file still active
 */
checkLock(char *file)
{
	register int ret;
#ifdef sgi
	char *p;
#endif
	int lpid = -1;
	char alpid[SIZEOFPID+2];	/* +2 for '\n' and NULL */
	int fd;
	extern int errno;

	DEBUG(4, "ulockf file %s\n", file);
	fd = open(file, O_RDONLY);
	if (fd == -1) {
	    if (errno == ENOENT)  /* file does not exist -- OK */
		return(0);
	    DEBUG(4,"Lock File--can't read (errno %d) --remove it!\n", errno);
	    goto unlk;
	}
#ifdef sgi
	bzero(alpid,sizeof(alpid));
	ret = read(fd, (char *) alpid, SIZEOFPID+1);	/* +1 for '\n' */
	(void) close(fd);
	if (ret < 2) {
		DEBUG0(4, "Lock File--bad format--remove it!\n");
		goto unlk;
	}
	lpid = (int)strtoul(alpid, &p, 10);
	if (lpid == 0 || *p != '\n') {
		DEBUG(4, "Lock File--invalid contents \'%s\'--remove it.\n",
		      alpid);
		goto unlk;
	}
#else
	ret = read(fd, (char *) alpid, SIZEOFPID+1); /* +1 for '\n' */
	(void) close(fd);
	if (ret != (SIZEOFPID+1)) {

	    DEBUG(4, "Lock File--bad format--remove it!\n", "");
	    goto unlk;
	}
	lpid = atoi(alpid);
#endif /* sgi */
	if ((ret=kill(lpid, 0)) == 0 || errno == EPERM) {
#ifdef sgi
	    DEBUG(4, "Lock File--process %d still active--not removed\n",lpid);
#else
	    DEBUG(4, "Lock File--process still active--not removed\n","");
#endif
	    return(FAIL);
	}
	else { /* process no longer active */
	    DEBUG(4, "kill pid (%d), ", lpid);
	    DEBUG(4, "returned %d", ret);
	    DEBUG(4, "--ok to remove lock file (%s)\n", file);
	}
unlk:

	if (unlink(file) != 0) {
		DEBUG0(4,"ulockf failed in unlink()\n");
		return(FAIL);
	}
	return(0);
}


#define MAXLOCKS 10	/* maximum number of lock files */
char *Lockfile[MAXLOCKS];
int Nlocks = 0;

/*
 * put name in list of lock files
 * return:
 *	none
 */
void
stlock(char *name)
{
	register int i;
	char *p;

	for (i = 0; i < Nlocks; i++) {
		if (Lockfile[i] == NULL)
			break;
	}
	ASSERT(i < MAXLOCKS, "TOO MANY LOCKS", "", i);
	if (i >= Nlocks)
		i = Nlocks++;
	p = calloc((unsigned) strlen(name) + 1, sizeof (char));
	ASSERT(p != NULL, "CAN NOT ALLOCATE FOR", name, 0);
	(void) strcpy(p, name);
	Lockfile[i] = p;
	return;
}

/*
 * remove all lock files in list
 * return:
 *	none
 */
void
rmlock(register char *name)
{
	register int i;
#ifdef V8
	char *cp;

	cp = rindex(name, '/');
	if (cp++ != CNULL)
	    if (strlen(cp) > DIRSIZ)
		*(cp+DIRSIZ) = NULLCHAR;
#endif /* V8 */


	for (i = 0; i < Nlocks; i++) {
		if (Lockfile[i] == NULL)
			continue;
		if (name == NULL || EQUALS(name, Lockfile[i])) {
			(void) unlink(Lockfile[i]);
			(void) free(Lockfile[i]);
			Lockfile[i] = NULL;
		}
	}
	return;
}



/*
 * remove a lock file
 * return:
 *	0	-> success
 *	FAIL	-> failure
 */
void
delock(char *s)
{
	char ln[MAXNAMESIZE];

#ifdef sgi
	if (!lockname(s,ln,sizeof(ln)))
		return;
#else
	(void) sprintf(ln, "%s.%s", LOCKPRE, s);
	BASENAME(ln, '/')[MAXBASENAME] = '\0';
#endif
	rmlock(ln);
}


#ifdef sgi
int
lockname(char	*name,
	 char	*tgt,
	 int	tgtlen)
{
	static struct {
		char	*nm1;
		char	*nm2;
	} spcl[] = {
		{"isdn/modem_", "isdn_"},
		{"vsc/wsty",	"vsc_"},
		{"isc/wsty",	"isc_"},
		{"gsc/wsty",	"gsc_"},
		{"psc/wsty",	"psc_"},
		{"pri/pri",	"pri_"}
	};
#	define SPCL_LEN sizeof(spcl)/sizeof(spcl[0])
	int i;

	if (!strncmp(name,"/dev/",5))
		name += 5;

	tgtlen -= sizeof(LOCKPRE ".");
	if (tgtlen <= 0) {
		tgt[0] = '\0';
		return 0;
	}
	(void)strcpy(tgt, LOCKPRE ".");

	/* Allow "LCK..isdn/modem_foo" and "/dev/[vig]sc" by converting to
	 * isdn-foo. That allows programs such as uustat to work.
	 */
	if (strchr(name, '/')) {
		i = 0;
		for (;;) {
			if (i >= SPCL_LEN) {
				tgt[0] = '\0';
				return 0;
			}
			if (!strncmp(name,spcl[i].nm1,strlen(spcl[i].nm1))) {
				tgtlen -= strlen(spcl[i].nm2);
				if (tgtlen <= 0) {
					tgt[0] = '\0';
					return 0;
				}
				name += strlen(spcl[i].nm1);
				(void)strcat(tgt, spcl[i].nm2);
				break;
			}
			i++;
		}
	}

	if (tgtlen <= strlen(name)) {
		tgt[0] = '\0';
		return 0;
	}
	(void)strcat(tgt, name);
	return 1;
}


#endif /* sgi */
/*
 * create system or device lock
 * return:
 *	0	-> success
 *	FAIL	-> failure
 */
mmlock(char *name)
{
	char lname[MAXNAMESIZE];

	/*
	 * if name has a '/' in it, then it's a device name and it's
	 * not in /dev (i.e., it's a remotely-mounted device or it's
	 * in a subdirectory of /dev).  in either case, creating our normal
	 * lockfile (/usr/spool/locks/LCK..<dev>) is going to bomb if
	 * <dev> is "/remote/dev/tty14" or "/dev/net/foo/clone", so never
	 * mind.  since we're using advisory filelocks on the devices
	 * themselves, it'll be safe.
	 *
	 * of course, programs and people who are used to looking at the
	 * lockfiles to find out what's going on are going to be a trifle
	 * misled.  we really need to re-consider the lockfile naming structure
	 * to accomodate devices in directories other than /dev ... maybe in
	 * the next release.
	 */
#ifdef sgi
	if (!lockname(name,lname,sizeof(lname)))
		return(0);
#else
	if ( strchr(name, '/') != NULL )
		return(0);
	(void) sprintf(lname, "%s.%s", LOCKPRE, name);
#endif
	BASENAME(lname, '/')[MAXBASENAME] = '\0';
	return(ulockf(lname, SLCKTIME) < 0 ? FAIL : 0);
}


/*
 * update access and modify times for lock files
 * return:
 *	none
 */
void
ultouch(void)
{
	register int i;
#ifdef sgi
	struct utimbuf ut;
#else
	time_t time();

	struct ut {
		time_t actime;
		time_t modtime;
	} ut;
#endif /* sgi */

	ut.actime = time(&ut.modtime);
	for (i = 0; i < Nlocks; i++) {
		if (Lockfile[i] == NULL)
			continue;
		utime(Lockfile[i], &ut);
	}
	return;
}

/*
 * makes a lock on behalf of pid.
 * input:
 *	pid - process id
 *	tempfile - name of a temporary in the same file system
 *	name - lock file name (full path name)
 * return:
 *	-1 - failed
 *	0  - lock made successfully
 */
static
onelock(char *pid,char *tempfile,char *name)
{
	register int fd;
	char	cb[100];

	fd=creat(tempfile, 0444);
	if(fd < 0){
		(void) sprintf(cb, "%s %s %d",tempfile, name, errno);
		logent("ULOCKC", cb);
		if((errno == EMFILE) || (errno == ENFILE))
			(void) unlink(tempfile);
		return(-1);
	}
	(void) write(fd, pid, SIZEOFPID+1);	/* +1 for '\n' */
	(void) chmod(tempfile,0444);
	(void) chown(tempfile, UUCPUID, UUCPGID);
	(void) close(fd);
	if(link(tempfile,name)<0){
		DEBUG(4, "%s: ", sys_errlist[errno]);
		DEBUG(4, "link(%s, ", tempfile);
		DEBUG(4, "%s)\n", name);
		if(unlink(tempfile)< 0){
			(void) sprintf(cb, "ULK err %s %d", tempfile,  errno);
			logent("ULOCKLNK", cb);
		}
		return(-1);
	}
	if(unlink(tempfile)<0){
		(void) sprintf(cb, "%s %d",tempfile,errno);
		logent("ULOCKF", cb);
	}
	return(0);
}

#ifdef ATTSV

/*
 * filelock(fd)	sets advisory file lock on file descriptor fd
 *
 * returns SUCCESS if all's well; else returns value returned by fcntl()
 *
 * needed to supplement /usr/spool/locks/LCK..<device> because can now
 * have remotely-mounted devices using RFS.  without filelock(), uucico's
 * or cu's on 2 different machines could access the same remotely-mounted
 * device, since /usr/spool/locks is purely local.
 *
 */

int
filelock(int fd)
{
	register int lockrtn, locktry = 0;
	struct flock lck;

	lck.l_type = F_WRLCK;	/* setting a write lock */
	lck.l_whence = 0;	/* offset l_start from beginning of file */
	lck.l_start = 0L;
	lck.l_len = 0L;		/* until the end of the file address space */

	/*	place advisory file locks	*/
	while ( (lockrtn = fcntl(fd, F_SETLK, &lck)) < 0 ) {

		DEBUG(7, "filelock: F_SETLK returns %d\n", lockrtn);

		switch (lockrtn) {
#ifdef EACCESS
		case EACCESS:	/* already locked */
				/* no break */
#endif /* EACCESS */
		case EAGAIN:	/* already locked */
			if ( locktry++ < MAX_LOCKTRY ) {
				sleep(2);
				continue;
			}
			logent("filelock","F_SETLK failed - lock exists");
			return(lockrtn);
		case ENOLCK:	/* no more locks available */
			logent("filelock","F_SETLK failed -- ENOLCK");
			return(lockrtn);
		case EFAULT:	/* &lck is outside program address space */
			logent("filelock","F_SETLK failed -- EFAULT");
			return(lockrtn);
		default:
			logent("filelock","F_SETLK failed");
			return(lockrtn);
		}
	}
	DEBUG0(7, "filelock: ok\n");
	return(SUCCESS);
}
#endif /* ATTSV */
