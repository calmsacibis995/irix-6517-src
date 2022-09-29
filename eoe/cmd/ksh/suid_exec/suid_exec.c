
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * This is a program to execute 'execute only' and suid/sgid shell scripts.
 * This program must be owned by root and must have the suid bit set.  It must
 * not have the guid bit set.  This program must be installed where the
 * define parameter THISPROG indicates to work correctly on system V machines.
 *
 *  Written by David Korn
 *  AT&T Bell Laboratories
 *  ulysses!dgk
 *  Enhanced by Rob Stampfli
 *  AT&T Bell Laboratories
 */

/* The file name of the script to execute is argv[0]
 * Argv[1] is the  program name
 * The basic idea is to open the script as standard input, set the effective
 *   user and group id correctly, and then exec the shell.
 * The complicated part is getting the effective uid of the caller and 
 *   setting the effective uid/gid.  The program which execs this program
 *   may pass file descriptor FDIN as an open file with mode SPECIAL if
 *   the effective user id is not the real user id.  The effective
 *   user id for authentication purposes will be the owner of this
 *   open file.  On systems without the setreuid() call, e[ug]id is set
 *   by copying this program to a /tmp/file, making it a suid and/or sgid
 *   program, and then execing this program.
 * A forked version of this program waits until it can unlink the /tmp
 *   file and then exits.  Actually, we fork() twice so the parent can
 *   wait for the child to complete.  A pipe is used to guarantee that we
 *   do not remove the /tmp file too soon.
 */

#include	"sh_config.h"
#ifdef		_fcntl_
#   include	<fcntl.h>
#   ifndef F_DUPFD
#	define fcntl(a,b,c)	dup2(a,c)
#   endif /* F_DUPFD */
#else
#   define fcntl(a,b,c)	dup2(a,c)
#endif		/* _fcntl_ */
#ifdef	_unistd_
#   include	<unistd.h>
#endif	/* _unistd_ */
#include	<sys/stat.h>
#include	<signal.h>
#include	<errno.h>
#include	<stdlib.h>

#ifdef sgi
#define	stat	stat64
#define	fstat	fstat64
#endif

#ifndef X_OK
#   define	X_OK	1
#endif /* X_OK */
#define SPECIAL		04100	/* setuid execute only by owner */
#define FDIN		10	/* must be same as /dev/fd below */
#define FDCLSYNC	11	/* used on sys5 to synchronize cleanup */
#define FDVERIFY	12	/* used to validate /tmp process */
#define BLKSIZE		sizeof(char*)*1024
#define THISPROG	"/etc/suid_exec"
#define DEFSHELL	"/bin/sh"

#ifndef sgi
extern char *getenv();
#endif
extern int errno;

static void setids();
static void error();
static int in_dir();
static int endsh();
#ifndef SETREUID
    static int copy();
    static void mktemp();
#endif /* SETREUID */

static char version[]	= "\n@(#)suid_exec 11/16/88e-Beta1\n";
static char tmpname[]	= "/tmp/SUIDXXXXXX";
static char badopen[]	= "cannot open";
static char badexec[]	= "cannot exec";
static char devfd[]	= "/dev/fd/10";		/* must match FDIN above */
static char **arglist;

static char *shell;
static char *command;
static uid_t ruserid;
static uid_t euserid;
static gid_t rgroupid;
static gid_t egroupid;
static struct stat statb;

main(argc,argv)
char *argv[];
{
	register int m,n;
	register char *p;
	struct stat statx;
	int mode;
	uid_t effuid;
	gid_t effgid;
	arglist = argv;
	if((command = argv[1]) == 0)
		error(badexec);
	ruserid = getuid();
	euserid = geteuid();
	rgroupid = getgid();
	egroupid = getegid();
	p = argv[0];
#ifndef SETREUID
	mktemp(tmpname);
	if(strcmp(p,tmpname)==0)
	{
		/* At this point, the presumption is that we are the
		 * version of THISPROG copied into /tmp, with the owner,
		 * group, and setuid/gid bits correctly set.  This copy of
		 * the program is executable by anyone, so we must be careful
		 * not to allow just any invocation of it to succeed, since
		 * it is setuid/gid.  Validate the proper execution by
		 * examining the FDVERIFY file descriptor -- if it is owned
		 * by root and is mode SPECIAL, then this is proof that it was
		 * passed by a program with superuser privileges -- hence we
		 * can presume legitimacy.  Otherwise, bail out, as we suspect
		 * an impostor.
		 */
		if(fstat(FDVERIFY,&statb) < 0 || statb.st_uid != 0 ||
		    (statb.st_mode & ~S_IFMT) != SPECIAL || close(FDVERIFY)<0)
			error(badexec);
		/* This enables the grandchild to clean up /tmp file */
		close(FDCLSYNC);
		/* Make sure that this is a valid invocation of the clone.
		 * Perhaps unnecessary, given FDVERIFY, but what the heck...
		 */
		if(stat(tmpname,&statb) < 0 || statb.st_nlink != 1 ||
		    (statb.st_mode & S_IFMT) != S_IFREG)
			error(badexec);
		if(ruserid != euserid &&
		  ((statb.st_mode & S_ISUID) == 0 || statb.st_uid != euserid))
			error(badexec);
		goto exec;
	}
	/* Make sure that this is the real setuid program, not the clone.
	 * It is possible by clever hacking to get past this point in the
	 * clone, but it doesn't do the hacker any good that I can see.
	 */
	if(euserid)
		error(badexec);
#endif /* SETREUID */
	/* Open the script for reading first and then validate it.  This
	 * prevents someone from pulling a switcheroo while we are validating.
	 */
	n = open(p,0);
	if(n == FDIN)
	{
		n = dup(n);
		close(FDIN);
	}
	if(n < 0)
		error(badopen);
	/* validate execution rights to this script */
	if(fstat(FDIN,&statb) < 0 || (statb.st_mode & ~S_IFMT) != SPECIAL)
		euserid = ruserid;
	else
		euserid = statb.st_uid;
	/* do it the easy way if you can */
	if(euserid == ruserid && egroupid == rgroupid)
	{
		if(access(p,X_OK) < 0)
			error(badexec);
	}
	else
	{
		/* have to check access on each component */
		while(*p++)
		{
			if(*p == '/' || *p == 0)
			{
				m = *p;
				*p = 0;
				if(eaccess(argv[0],X_OK) < 0)
					error(badexec);
				*p = m;
			}
		}
		p = argv[0];
	}
	if(fstat(n, &statb) < 0 ||
	   (statb.st_mode & S_IFMT) != S_IFREG)
		error(badopen);
	if(stat(p, &statx) < 0 ||
	  statb.st_ino != statx.st_ino || statb.st_dev != statx.st_dev)
		error(badexec);
	if(stat(THISPROG, &statx) < 0 ||
	  (statb.st_ino == statx.st_ino && statb.st_dev == statx.st_dev))
		error(badexec);
	close(FDIN);
	if(fcntl(n,F_DUPFD,FDIN) != FDIN)
		error(badexec);
	close(n);

	/* compute the desired new effective user and group id */
	effuid = euserid;
	effgid = egroupid;
	mode = 0;
	if(statb.st_mode & S_ISUID)
		effuid = statb.st_uid;
	if(statb.st_mode & S_ISGID)
		effgid = statb.st_gid;

	/* see if group needs setting */
	if(effgid != egroupid)
		if(effgid != rgroupid || setgid(rgroupid) < 0)
			mode = S_ISGID;
		
	/* now see if the uid needs setting */
	if(mode)
	{
		if(effuid != ruserid)
			mode |= S_ISUID;
	}
	else if(effuid)
	{
		if(effuid != ruserid || setuid(ruserid) < 0)
			mode = S_ISUID;
	}
		
	if(mode)
		setids(mode, effuid, effgid);
exec:
	/* only use SHELL if file is in trusted directory and ends in sh */
	shell = getenv("SHELL");
	if(shell == 0 || !endsh(shell) || (
		!in_dir("/bin",shell) &&
		!in_dir("/usr/bin",shell) &&
		!in_dir("/usr/lbin",shell)))
			shell = DEFSHELL;
	argv[0] = command;
	argv[1] = devfd;
	execv(shell,argv);
	error(badexec);
}

/*
 * return true of shell ends in sh
 */

static int endsh(shell)
register char *shell;
{
	while(*shell)
		shell++;
	if(*--shell != 'h' || *--shell != 's')
		return(0);
	return(1);
}


/*
 * return true of shell is in <dir> directory
 */

static int in_dir(dir,shell)
register char *dir;
register char *shell;
{
	while(*dir)
	{
		if(*dir++ != *shell++)
			return(0);
	}
	/* return true if next character is a '/' */
	return(*shell=='/');
}

static void error(message)
char *message;
{
	printf("%s: %s\n",command,message);
	exit(1);
	/*NOTREACHED*/
}


/*
 * This version of access checks against effective uid and effective gid
 */

eaccess(name, mode)
register char	*name;
register int mode;
{	
	struct stat statb;
	if (stat(name, &statb) == 0)
	{
		if(euserid == 0)
		{
			if((statb.st_mode&S_IFMT) != S_IFREG || mode != 1)
				return(0);
		    	/* root needs execute permission for someone */
			mode = (S_IEXEC|(S_IEXEC>>3)|(S_IEXEC>>6));
		}
		else if(euserid == statb.st_uid)
			mode <<= 6;
		else if(egroupid == statb.st_gid)
			mode <<= 3;
#if defined(NGROUPS) && NGROUPS>0
		/* on some systems you can be in several groups */
		else
		{
			gid_t groups[NGROUPS];
			register int n;
			n = getgroups(NGROUPS,groups);
			while(--n >= 0)
			{
				if(groups[n] == statb.st_gid)
				{
					mode <<= 3;
					break;
				}
			}
		}
#endif /* NGROUPS */
		if(statb.st_mode & mode)
			return(0);
	}
	return(-1);
}

#ifdef SETREUID
static void
setids(mode,owner,group)
{
	if(mode & S_ISGID)
		setregid(rgroupid,group);

	/* set effective uid even if S_ISUID is not set.  This is because
	 * we are *really* executing EUID root at this point.  Even if S_ISUID
	 * is not set, the value for owner that is passsed should be correct.
	 */
	setreuid(ruserid,owner);
}

#else
/*
 * This version of setids creats a /tmp file and copies itself into it.
 * The "clone" file is made executable with appropriate suid/sgid bits.
 * Finally, the clone is exec'ed.  This file is unlinked by a grandchild
 * of this program, who waits around until the text is free.
 */

static void
setids(mode,owner,group)
int mode;
uid_t owner;
gid_t group;
{
	register int n,m;
	int pv[2];

	/*
	 * Create a token to pass to the new program for validation.
	 * This token can only be procured by someone running with an
	 * effective userid of root, and hence gives the clone a way to
	 * certify that it was really invoked by THISPROG.  Someone who
	 * is already root could spoof us, but why would they want to?
	 *
	 * Since we are root here, we must be careful:  What if someone
	 * linked a valuable file to tmpname?
	 */
	unlink(tmpname);	/* should normally fail */
#ifdef O_EXCL
	if((n = open(tmpname, O_WRONLY | O_CREAT | O_EXCL, SPECIAL)) < 0 ||
		unlink(tmpname) < 0)
#else
	if((n = creat(tmpname, SPECIAL)) < 0 || unlink(tmpname) < 0)
#endif
		error(badexec);
	if(n != FDVERIFY)
	{
		close(FDVERIFY);
		if(fcntl(n,F_DUPFD,FDVERIFY) != FDVERIFY)
			error(badexec);
	}
	mode |= S_IEXEC|(S_IEXEC>>3)|(S_IEXEC>>6);
	/* create a pipe for synchronization */
	if(pipe(pv) < 0)
		error(badexec);
	if((n=fork()) == 0)
	{	/* child */
		close(FDVERIFY);
		close(pv[1]);
		if((n=fork()) == 0)
		{	/* grandchild -- cleans up clone file */
			signal(SIGHUP, SIG_IGN);
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			signal(SIGTERM, SIG_IGN);
			read(pv[0],pv,1); /* wait for clone to close pipe */
			while(unlink(tmpname) < 0 && errno == ETXTBSY)
				sleep(1);
			exit(0);
	    	}
		else if(n == -1)
			exit(1);
		else
		{
			/* Create a set[ug]id file that will become the clone. 
			 * To make this atomic, without need for chown(), the
			 * child takes on desired user and group.  The only
			 * downsize of this that I can see is that it may
			 * screw up some per- * user accounting.
			 */
			if((m = open(THISPROG, O_RDONLY)) < 0)
				exit(1);
			if((mode & S_ISGID) && setgid(group) < 0)
				exit(1);
			if((mode & S_ISUID) && owner && setuid(owner) < 0)
				exit(1);
#ifdef O_EXCL
			if((n = open(tmpname, O_WRONLY | O_CREAT | O_EXCL, mode)) < 0)
#else
			unlink(tmpname);
			if((n = creat(tmpname, mode)) < 0)
#endif /* O_EXCL */
				exit(1);
			/* populate the clone */
			exit(copy(m,n));
		}
	}
	else if(n == -1)
		error(badexec);
	else
	{
		arglist[0] = tmpname;
		close(pv[0]);
		/* move write end of pipe into FDCLSYNC */
		if(pv[1] != FDCLSYNC)
		{
			close(FDCLSYNC);
			if(fcntl(pv[1],F_DUPFD,FDCLSYNC) != FDCLSYNC)
				error(badexec);
		}
		/* wait for child to die */
		while((m = wait(0)) != n)
			if(m == -1 && errno != EINTR)
				break;
		/* Kill any setuid status at this point.  That way, if the
		 * clone is not setuid, we won't exec it as root.  Also, don't
		 * neglect to consider that someone could have switched the
		 * clone file on us.
		 */
		if(setuid(ruserid) < 0)
			error(badexec);
		execv(tmpname,arglist);
		error(badexec);
	}
}

/*
 * create a unique name into the <template>
 */

static void mktemp(template)
char *template;
{
	register char *cp = template;
	register pid_t n = getpid();
	/* skip to end of string */
	while(*++cp);
	/* convert process id to string */
	while(n > 0)
	{
		*--cp = (n%10) + '0';
		n /= 10;
	}
	
}

/*
 *  copy THISPROG into the open file number <fdo> and close <fdo>
 */

static int copy(fdi, fdo)
int fdi, fdo;
{
	char buffer[BLKSIZE];
	register int n;

	while((n = read(fdi,buffer,BLKSIZE)) > 0)
		if(write(fdo,buffer,n) != n)
			break;
	close(fdi);
	close(fdo);
	return n;
}

#endif /* SETREUID */


