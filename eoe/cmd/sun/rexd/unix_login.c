/* @(#)unix_login.c	1.2 87/08/13 3.2/4.3NFSSRC */
# ifdef lint
static char sccsid[] = "@(#)unix_login.c 1.1 86/09/25 Copyright 1985, 1987 Sun Microsystems, Inc.";
/*
 *
 * NFSSRC 3.2/4.3 for the VAX*
 * Copyright (C) 1987 Sun Microsystems, Inc.
 * 
 * (*)VAX is a trademark of Digital Equipment Corporation
 *
 */
# endif lint

# include <rpc/types.h>
# include <sys/ioctl.h>
# include <sys/signal.h>
# include <sys/file.h>
# include <pwd.h>
# include <errno.h>
# include <stdio.h>
# include <syslog.h>
# include <sys/sysmacros.h>
# include <sys/stat.h>
# include "rexioctl.h"
# include <../utmp.h>	/* get SYSV utmp, not Sun's */
# include <signal.h>
# include <sys/stropts.h>	/* for I_PUSH */

# include "rex.h"

/*
 * unix_login - hairy junk to simulate logins for Unix
 *
 * Copyright (c) 1985, 1987 Sun Microsystems, Inc.
 */

char Utmp[] = "/etc/utmp";	/* the tty slots */
char Wtmp[] = "/usr/adm/wtmp";	/* the log information */

int Master, Slave;		/* sides of the pty */
int InputSocket, OutputSocket;	/* Network sockets */
#define killpg(x,y)  kill( -x, y)
fd_set	HelperMask;
int Helper1, Helper2;		/* pids of the helpers */
char UserName[256];		/* saves the user name for loging */
char HostName[256];		/* saves the host name for loging */
char	*PtyName;
static int TtySlot;		/* slot number in Utmp */

extern char * _getpty(int*, int, mode_t, int);

void LoginUser();
void LogoutUser();
void SendSignal(int);

/*
 * Check for user being able to run on this machine.
 * returns 0 if OK, TRUE if problem, error message in "error"
 * copies name of shell if user is valid.
 */
ValidUser(host, uid, error, shell)
    char *host;
    int uid;
    char *error;
    char *shell;
{
    struct passwd *pw;
    
    if (uid == 0) {
    	errprintf(error,"root execution from %s not allowed\n",host);
	return(1);
    }
    pw = getpwuid(uid);
    if (pw == NULL) {
    	errprintf(error,"access denied: user id %d from %s not valid\n",uid, host);
	return(1);
    }
    strncpy(UserName, pw->pw_name, sizeof(UserName)-1 );
    strncpy(HostName, host, sizeof(HostName)-1 );
    strcpy(shell,pw->pw_shell);
    setproctitle(pw->pw_name, host);
    return(0);
}

/*
 *  eliminate any controlling terminal that we have already
 */
void
NoControl()
{
    int devtty;

    devtty = open("/dev/tty",O_RDWR);
    if (devtty > 0) {
    	    ioctl(devtty, TIOCNOTTY, NULL);
	    close(devtty);
    }
}

/*
 * Allocate a pseudo-terminal
 * sets the global variables Master and Slave.
 * returns 1 on error, 0 if OK
 */
AllocatePty(socket0, socket1)
    int socket0, socket1;
{
    int on = 1;

    signal(SIGHUP,SIG_IGN);
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
	{
	    PtyName = _getpty(&Master, O_RDWR|O_NDELAY, 0, 0);
	    if (PtyName == 0) {
		    syslog(LOG_CRIT, "Out of ptys: %m");
		    return(1);
	    }

	    Slave = open(PtyName, O_RDWR);
	    if (Slave < 0) {
		syslog(LOG_ERR, "open: %s: %m", PtyName);
		close(Master);
		return(1);
	    }
	    LoginUser();
	    setpgrp();
	    InputSocket = socket0;
	    OutputSocket = socket1;
	    ioctl(Master, FIONBIO, &on);
	    FD_SET(InputSocket, &HelperMask);
	    FD_SET(Master, &HelperMask);
	    return(0);
	}
}


  /*
   * Special processing for interactive operation.
   * Given pointers to three standard file descriptors,
   * which get set to point to the pty.
   */
void
DoHelper(pfd0, pfd1, pfd2)
    int *pfd0, *pfd1, *pfd2;
{
    int pgrp;

    pgrp = getpid();
    setpgrp();
    ioctl(Slave, TIOCSPGRP, &pgrp);

    signal( SIGINT, SIG_IGN);
    close(Master);

    *pfd0 = Slave;
    *pfd1 = Slave;
    *pfd2 = Slave;
}


/*
 * destroy the helpers when the executing process dies
 */
void
KillHelper(grp)
    int grp;
{
    char	buf[128];
    int		cc;

    /*
     * Finish reading any last output from the process.
     * If we don't do this here, then there is a good chance
     * we'll miss the last output of the process.  This is
     * because we receive the SIGCHLD from the process exiting
     * before we get a chance to look at the return value
     * of select().  Thus, if we close the pty here the last
     * data output by the process is tossed.
     */
    cc = read(Master, buf, 128);
    while (cc > 0) {
	(void) write(OutputSocket, buf, cc);
    	cc = read(Master, buf, 128);
    }
    close(Master);
    FD_ZERO(&HelperMask);
    close(InputSocket);
    close(OutputSocket);
    LogoutUser();
    if (grp) killpg(grp,SIGKILL);
    close(Slave);
}


/*
 * edit the Unix traditional data files that tell who is logged
 * into "the system"
 */
void
LoginUser()
{
	struct utmp	entry;
	register FILE *fp;

	entry.ut_user[0] = 'r';
	entry.ut_user[1] = '-';
	strncpy(entry.ut_user + 2, UserName, sizeof(entry.ut_user) - 2);
	strncpy(entry.ut_id, &PtyName[(sizeof("/dev/tty") - 1)],
		sizeof(entry.ut_id));
	strncpy(entry.ut_line, PtyName+sizeof("/dev/")-1, sizeof(entry.ut_line));
	entry.ut_pid = getpid();
	entry.ut_type = USER_PROCESS;
	entry.ut_time = time(0);
	setutent();
	(void)pututline(&entry);
	endutent();

/* Now attempt to add to the end of the wtmp file.  Do not create
 * if it does not already exist.  **  Note  ** This is the reason
 * "r+" is used instead of "a+".  "r+" will not create a file, while
 * "a+" will. */
	if ((fp = fopen("/etc/wtmp","r+")) != NULL) {
		fseek(fp,0L,2);
		fwrite((char*)&entry,sizeof(entry),1,fp);
		fclose(fp);
	}
}


/*
 * edit the Unix traditional data files that tell who is logged
 * into "the system".
 */
void
LogoutUser()
{
	register FILE *fp;
	struct utmp	entry;

	strncpy(entry.ut_user, "deadrex", sizeof(entry.ut_user));
	strncpy(entry.ut_id, &PtyName[(sizeof("/dev/tty") - 1)],
		sizeof(entry.ut_id));
	strncpy(entry.ut_line, PtyName+sizeof("/dev/")-1, sizeof(entry.ut_line));
	entry.ut_pid = getpid();
	entry.ut_type = DEAD_PROCESS;
	entry.ut_time = time(0);
	setutent();
	(void)pututline(&entry);
	endutent();
	if ((fp = fopen("/etc/wtmp","r+")) != NULL) {
		entry.ut_user[0] = '\0';
		fseek(fp,0L,2);
		fwrite((char*)&entry,sizeof(entry),1,fp);
		fclose(fp);
	}

	chmod(PtyName, 0666);
	chown(PtyName, 0, 0);
}


/*
 * set the pty modes to the given values
 */
void
SetPtyMode(mode)
    struct rex_ttymode *mode;
{
    struct termio term;

    Term_BsdToSysV(mode, &term);
    ioctl(Slave, TCSETA, &term);
}

/*
 * set the pty window size to the given value
 */
void
SetPtySize(size)
    struct ttysize *size;
{
    int pgrp;

    (void) ioctl(Slave, TIOCSWINSZ, size);
    SendSignal(SIGWINCH);
}


/*
 * send the given signal to the group controlling the terminal
 */
void
SendSignal(sig)
{
    int pgrp;

    if (ioctl(Slave, TIOCGPGRP, &pgrp) >= 0)
    	(void) killpg( pgrp, sig);
}


/*
 * called when the main select loop detects that we might want to
 * read something.
 */
void
HelperRead(fds)
    fd_set fds;
{
    char buf[128];
    int cc;
    extern int errno;

    if ( FD_ISSET(Master, &fds) ) {
    	cc = read(Master, buf, sizeof buf);
	if (cc > 0)
		(void) write(OutputSocket, buf, cc);
	else {
		if (cc < 0 && errno != EINTR && errno != EWOULDBLOCK)
			syslog(LOG_ERR, "pty read: %m");
		shutdown(OutputSocket, 1);
		FD_CLR(Master, &HelperMask);
	}
    }
    if ( FD_ISSET(InputSocket, &fds) ) {
    	cc = read(InputSocket, buf, sizeof buf);
	if (cc > 0)
		(void) write(Master, buf, cc);
	else {
		if (cc < 0 && errno != EINTR && errno != EWOULDBLOCK)
			syslog(LOG_ERR, "socket read: %m");
		FD_CLR(InputSocket, &HelperMask);
	}
    }
}

/* 
 * NFSSRC 3.2/4.3 for the VAX*
 * Copyright (C) 1987 Sun Microsystems, Inc.
 * 
 * (*)VAX is a trademark of Digital Equipment Corporation
 */
