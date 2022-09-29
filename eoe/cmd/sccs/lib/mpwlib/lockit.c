/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sccs:lib/mpwlib/lockit.c	6.12" */
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sccs/lib/mpwlib/RCS/lockit.c,v 1.7 1995/12/30 02:19:14 ack Exp $"

/*
	Process semaphore.
	Try repeatedly (`count' times) to create `lockfile' mode 444.
	Sleep 10 seconds between tries.
	If `tempfile' is successfully created, write the process ID
	`pid' in `tempfile' (in binary), link `tempfile' to `lockfile',
	and return 0.
	If `lockfile' exists and it hasn't been modified within the last
	minute, and either the file is empty or the process ID contained
	in the file is not the process ID of any existing process,
	`lockfile' is removed and it tries again to make `lockfile'.
	After `count' tries, or if the reason for the create failing
	is something other than EACCES, return xmsg().
 
	Unlockit will return 0 if the named lock exists, contains
	the given pid, and is successfully removed; -1 otherwise.
*/

# include	<unistd.h>
# include	"../../hdr/defines.h"
# include	"errno.h"
# include       "sys/utsname.h"
# define        nodenamelength 9

static int	onelock();
static int	_creat();
static int	_unlink();
static int	_link();
static int	extract_dir();
extern char	*fmalloc();
extern void	ffree();

static int _creat(path, mode)
const char *path;
mode_t mode;
{
        int     ret_val;
        char    oldpwd[PATH_MAX];
        char    newpwd[PATH_MAX];
        char    newfile[PATH_MAX];

        if (strlen(path) > 1000){
                (void) getcwd(oldpwd, PATH_MAX);
                extract_dir(path, newpwd, newfile);
                chdir(newpwd);
                ret_val = creat(newfile, mode);
                (void) chdir(oldpwd);
        }
        else {
                ret_val = creat(path, mode);
        }
        return (ret_val);
}

static int _link(path1, path2)
const char *path1;
const char *path2;
{
        int     ret_val;
        char    oldpwd[PATH_MAX];
        char    newpwd[PATH_MAX];
        char    newfile[PATH_MAX];

        if (strlen(path1) > 1000){
                (void) getcwd(oldpwd, PATH_MAX);
                extract_dir(path1, newpwd, newfile);
                chdir(newpwd);
                ret_val = link(newfile, path2);
                (void) chdir(oldpwd);
        }
        else {
                ret_val = link(path1, path2);
        }
        return (ret_val);

}

static int _unlink(path)
const char *path;
{
        int     ret_val;
        char    oldpwd[PATH_MAX];
        char    newpwd[PATH_MAX];
        char    newfile[PATH_MAX];

        if (strlen(path) > 1000){
                (void) getcwd(oldpwd, PATH_MAX);
                extract_dir(path, newpwd, newfile);
                chdir(newpwd);
                ret_val = unlink(newfile);
                (void) chdir(oldpwd);
        }
        else {
                ret_val = unlink(path);
        }
        return (ret_val);
}


int
lockit(lockfile,count,pid,uuname)
register char *lockfile;
register unsigned count;
pid_t pid;
char *uuname;
{
	register int fd;
	int ret;
	pid_t opid;
	char ouuname[nodenamelength];
	
	long ltime;
	long omtime;
	extern int errno;
	static char tempfile[1024];
	char	dir_name[1024];
	struct stat64 sbuf;
	unsigned short min_mode;
	int	strcmp(), close(), read(), open(), stat64(), unlink(), kill();
	unsigned int	sleep();
	int egid;
	int ngrps;

	copy(lockfile,dir_name);
/*
	sprintf(tempfile,"%s/%u.%ld",dname(dir_name),pid,uuname,time((long *)0));
*/
	sprintf(tempfile,"%s/%u.%s%ld",dname(dir_name),pid,uuname,time((long *)0));

/*
** Determine if we have a chance to create the lockfile.
** (Is directory writeable?)
*/	

	(void) stat64(dir_name,&sbuf);
	min_mode = 0003;
	if(sbuf.st_gid == getegid()) {
		min_mode = 0030;
	} else {
		gid_t *gidset;
		gid_t *gp;
		int agrps = sysconf(_SC_NGROUPS_MAX);
		if (agrps == -1)	/* XXX */
			return(-1);
		gidset = (gid_t *)fmalloc(agrps * sizeof(gid_t));
		ngrps = getgroups(agrps, gidset);
		for (gp = &gidset[0]; ngrps > 0; ngrps--, gp++) {
			if (sbuf.st_gid == *gp) {
				min_mode = 0030;
				ngrps = 0;	/* terminate loop */
			}
		}
		ffree(gidset);
	}
	if(sbuf.st_uid == geteuid())
		min_mode = 0300;
	if(!((sbuf.st_mode & min_mode) == min_mode))
		return(-1);

	for (++count; --count; (void) sleep(10)) {
		if (onelock(pid,uuname,tempfile,lockfile) == 0)
			return(0);
		if (!exists(lockfile))
			continue;
		omtime = Statbuf.st_mtime;
		if ((fd = open(lockfile,0)) < 0)
			continue;
		ret = read(fd,(char *)&opid,sizeof(opid));
		(void) read(fd,ouuname,nodenamelength);
		(void) close(fd);
		if (ret != sizeof(pid) || ret != Statbuf.st_size) {
			(void) unlink(lockfile);
			continue;
		}
		/* check for pid */
		if (strncmp(ouuname, uuname, nodenamelength) == 0)
		  if (kill((int) opid,0) == -1 && errno == ESRCH) {
			if (exists(lockfile) &&
				omtime == Statbuf.st_mtime) {
					(void) unlink(lockfile);
					continue;
			}
		  }
		if ((ltime = time((long *)0) - Statbuf.st_mtime) < 60L) {
			if (ltime >= 0 && ltime < 60)
				(void) sleep((unsigned) (60 - ltime));
			else
				(void) sleep(60);
		}
		continue;
	}
	return(-1);
}

int
unlockit(lockfile,pid,uuname)
register char *lockfile;
pid_t pid;
char *uuname;

{
	register int fd, n;
	pid_t opid;
	char ouuname[nodenamelength];
	int unlink(), open();

	if ((fd = open(lockfile,0)) < 0)
		return(-1);
	n = read(fd,(char *)&opid,sizeof(opid));
	(void) read(fd,ouuname,nodenamelength);
	(void) close(fd);
	if (n == sizeof(opid) && opid == pid &&
	    strncmp(ouuname, uuname, nodenamelength) == 0)
		return(unlink(lockfile));
	else
		return(-1);
}


static int
onelock(pid,uuname,tempfile,lockfile)
pid_t pid;
char *uuname;
char *tempfile;
char *lockfile;
{
	int	fd;
	extern int errno;
	int	xmsg(), write(), unlink(), creat(), link();

	if ((fd = _creat(tempfile,(mode_t)0444)) >= 0) {
		(void) write(fd,(char *)&pid,sizeof(pid));
		(void) write(fd,uuname,nodenamelength);
		(void) close(fd);
		if (_link(tempfile,lockfile) < 0) {
			(void) _unlink(tempfile);
			return(-1);
		}
		(void) _unlink(tempfile);
		return(0);
	}
	if (errno == ENFILE) {
		(void) _unlink(tempfile);
		return(-1);
	}
	if (errno != EACCES)
		return(xmsg(tempfile,"lockit"));
	return(-1);
}


mylock(lockfile,pid,uuname)
register char *lockfile;
pid_t pid;
char *uuname;

{
	register int fd, n;
	pid_t opid;
	char ouuname[nodenamelength];
	int	open();

	if ((fd = open(lockfile,0)) < 0)
		return(0);
	n = read(fd,(char *)&opid,sizeof(opid));
	(void) read(fd,ouuname,nodenamelength);
	(void) close(fd);
	if (n == sizeof(opid) && opid == pid &&
	    strncmp(ouuname, uuname, nodenamelength) == 0)
		return(1);
	else
		return(0);
}

static int extract_dir(abs, dir, file)
char *abs;
char *dir;
char *file;
{
	int i;
	
	for (i = strlen(abs)-1; i >= 0; i--)
		if (abs[i] == '/')
			break;
	if (i == -1){
		strcpy(file, abs);
		strcpy(dir, ".");
	}
	else {
		strcpy(file, &abs[i+1]);
		strncpy(dir, abs, i-1);
	}
	return 1;
}

