/*
 *	This file contains intersystem compatibility routines derived from
 *	various public domain sources.  This file is not subject to any of
 *	the licensing or distribution restrictions applicable to the rest
 *	of the bru source code.  It is provided to improved transportability
 *	of bru source code between various flavors of unix.
 *
 *	NOTE:  The remote tape drive access routines are derived from a
 *	public domain version of the remote tape library originally
 *	produced at Georgia Tech.  They are provided for convenience
 *	in using the bru source code on systems where the library has
 *	not been installed.
 *
 *	They have been reformatted to fit in with the style in the
 *	rest of the bru source code but other than that are essentially
 *	unchanged.
 *
 *		Fred Fish  28-Oct-85
 *
 */

#include "autoconfig.h"

#if RMT 

#include "typedefs.h"

#if (unix || xenix)
#  if BSD
#    include <sys/file.h>		/* BSD DEPENDANT!!! */
#  else
#    include <fcntl.h>		/* use this one for S5 with remote stuff */
#  endif
#  include <signal.h>
#  include <errno.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#else
#  include "sys.h"
#endif
  
#ifndef EOPNOTSUPP
#define EOPNOTSUPP EINVAL
#endif

#if BSD
extern int errno;		/* Not in 4.2's <errno.h> */
#define strchr index
typedef int NBYTETYPE;		/* Funny things, system calls... */
typedef char *IOCTL3;
extern int perror (), _exit ();
#else
# ifdef sgi
typedef char *IOCTL3;
# else
typedef int IOCTL3;
# endif
typedef unsigned int NBYTETYPE;
extern VOID perror (), _exit ();
#endif

/*
 *	If ioctl calls are supported on remote device then define
 *	RMTIOCTL.
 */

#if BSD || sgi
#define RMTIOCTL
#ifndef sgi
extern char *sprintf ();
#endif
#endif

#ifdef RMTIOCTL
#include <sys/ioctl.h>
#include <sys/mtio.h>
#endif

/*
 *	Note that local vs remote file descriptors are distinguished
 *	by adding a bias to the remote descriptors.  This is a quick
 *	and dirty trick that may not be portable to some systems.
 */

#define REM_BIAS 128


/*
 *	BUFMAGIC --- Magic buffer size
 *	MAXUNIT --- Maximum number of remote tape file units
 */

#define BUFMAGIC	64
#define MAXUNIT		4

/*
 *	Useful macros.
 *
 *	READ --- Return the number of the read side file descriptor
 *	WRITE --- Return the number of the write side file descriptor
 */

#ifdef sgi
#	define	READ(fd)	fd
#	define	WRITE(fd)	fd
#else
#define READ(fd)	(_rmt_Ctp[fd][0])
#define WRITE(fd)	(_rmt_Ptc[fd][1])

static int _rmt_Ctp[MAXUNIT][2] = {
    -1, -1, -1, -1, -1, -1, -1, -1
};

static int _rmt_Ptc[MAXUNIT][2] = {
    -1, -1, -1, -1, -1, -1, -1, -1
};
#endif

/*
 *	Isrmt. Let a programmer know he has a remote device.
 */

int isrmt (fd)
int fd;
{
    return (fd & REM_BIAS);
}

/*
 *	abort --- close off a remote tape connection
 */

VOID _rmt_abort (fildes)
int fildes;
{
    (VOID) close (READ (fildes));
    (VOID) close (WRITE (fildes));
    READ (fildes) = -1;
    WRITE (fildes) = -1;
}

/*
 *	Test pathname for specified access.  Looks just like access(2)
 *	to caller.
 */

int rmtaccess (path, amode)
char *path;
int amode;
{
    if (_rmt_dev (path)) {
	return (0);		/* Let /etc/rmt find out */
    } else {
	return (access (path, amode));
    }
}

/*
 *	Close a file.  Looks just like close(2) to caller.
 */

int rmtclose (fildes)
int fildes;
{
    if (isrmt (fildes)) {
	return (_rmt_close (fildes & ~REM_BIAS));
    } else {
	return (close (fildes));
    }
}

/*
 *	_rmt_close --- close a remote magtape unit and shut down
 */

static int _rmt_close (fildes)
int fildes;
{
    int rc;

    if (_rmt_command (fildes, "C\n") != -1) {
	rc = _rmt_status (fildes);
	_rmt_abort (fildes);
	return (rc);
    }
    return (-1);
}

/*
 *	_rmt_command --- attempt to perform a remote tape command
 */

int _rmt_command (fildes, buf)
int fildes;
char *buf;
{
    register int blen;
    SIGTYPE pstat;
    int	err;

    /*
     *	save current pipe status and try to make the request
     */

    blen = strlen (buf);
    pstat = signal (SIGPIPE, SIG_IGN);
  writeagain:
    if ((err = write (WRITE (fildes), buf, (NBYTETYPE) blen)) == blen) {
	(VOID) signal (SIGPIPE, pstat);
	return (0);
    }
    if (err == -1 && errno == EINTR)
	goto writeagain;

    /*
     *	something went wrong. close down and go home
     */

    (VOID) signal (SIGPIPE, pstat);
    _rmt_abort (fildes);

    errno = EIO;
    return (-1);
}

/*
 *	Create a file from scratch.  Looks just like creat(2) to the caller.
 */

int rmtcreat (path, mode)
char *path;
int mode;
{
    if (_rmt_dev (path)) {
	return (rmtopen (path, 1 | O_CREAT, mode));
    } else {
	return (creat (path, mode));
    }
}

/*
 *	Test pathname to see if it is local or remote.  A remote device
 *	is any string that contains ":/dev/".  Returns 1 if remote,
 *	0 otherwise.
 */

int _rmt_dev (path)
register char *path;
{
    extern char *strchr ();

    if ((path = strchr (path, ':')) != (char *) 0) {
	if (strncmp (path + 1, "/dev/", 5) == 0) {
	    return (1);
	}
    }
    return (0);
}

/*
 *	Duplicate an open file descriptor.  Looks just like dup(2)
 *	to caller.
 */

int rmtdup (fildes)
int fildes;
{
    if (isrmt (fildes)) {
	errno = EOPNOTSUPP;
	return (-1);		/* For now (fnf) */
    } else {
	return (dup (fildes));
    }
}

/*
 *	Rmtfcntl. Do a remote fcntl operation.
 */

#ifdef DEADCODE

int rmtfcntl (fd, cmd, arg)
int fd;
int cmd;
int arg;
{
    if (isrmt (fd)) {
	errno = EOPNOTSUPP;
	return (-1);
    } else {
	return (fcntl (fd, cmd, arg));
    }
}

#endif  /* DEADCODE */

/*
 *	Get file status.  Looks just like fstat(2) to caller.
 */

#if !HAVE_SEEKDIR

int rmtfstat (fildes, buf)
int fildes;
struct stat64 *buf;
{
    if (isrmt (fildes)) {
	errno = EOPNOTSUPP;
	return (-1);		/* For now (fnf) */
    } else {
	return (fstat64 (fildes, buf));
    }
}

#endif  /* !HAVE_SEEKDIR */

/*
 *	Do ioctl on file.  Looks just like ioctl(2) to caller.
 */

int rmtioctl (fildes, request, arg)
int fildes;
int request;
IOCTL3 arg;
{
    if (isrmt (fildes)) {
#ifdef RMTIOCTL
	return (_rmt_ioctl (fildes, request, arg));
#else
	errno = EOPNOTSUPP;
	return (-1);		/* For now  (fnf) */
#endif
    } else {
	return (ioctl (fildes, request, arg));
    }
}

/*
 *	_rmt_ioctl --- perform raw tape operations remotely
 */

#ifdef RMTIOCTL
static _rmt_ioctl (fildes, op, arg)
int fildes;
int op;
IOCTL3 arg;
{
    char c;
    int rc;
    int cnt;
    char buffer[BUFMAGIC];

    /*
     *	MTIOCOP is the easy one. nothing is transfered in binary
     */

    if (op == MTIOCTOP) {
	(VOID) sprintf (buffer, "I%d\n%d\n", ((struct mtop *) arg) -> mt_op,
		 ((struct mtop  *) arg) -> mt_count);
	if (_rmt_command (fildes, buffer) == -1) {
	    return (-1);
	}
	return (_rmt_status (fildes));
    }

    /*
     *	we can only handle 2 ops, if not the other one, punt
     */

    if (op != MTIOCGET) {
	errno = EINVAL;
	return (-1);
    }

    /*
     *	grab the status and read it directly into the structure
     *	this assumes that the status buffer is (hopefully) not
     *	padded and that 2 shorts fit in a long without any word
     *	alignment problems, ie - the whole struct is contiguous
     *	NOTE - this is probably NOT a good assumption.
     */

    if (_rmt_command (fildes, "S\n") == -1 || (rc = _rmt_status (fildes)) == -1)
	return (-1);

    for (; rc > 0; rc -= cnt, arg += cnt) {
	cnt = read (READ (fildes), arg, rc);
	if (cnt == -1 && errno == EINTR) {
	    cnt = 0;
	    continue;
	}
	if (cnt <= 0) {
	    _rmt_abort (fildes);
	    errno = EIO;
	    return (-1);
	}
    }

    /*
     *	now we check for byte position. mt_type is a small integer field
     *	(normally) so we will check its magnitude. if it is larger than
     *	256, we will assume that the bytes are swapped and go through
     *	and reverse all the bytes
     */

    if (((struct mtget *) arg) -> mt_type < 256) {
	return (0);
    }
    for (cnt = 0; cnt < rc; cnt += 2) {
	c = arg[cnt];
	arg[cnt] = arg[cnt + 1];
	arg[cnt + 1] = c;
    }
    return (0);
}
#endif				/* RMTIOCTL */

/*
 *	Rmtisaatty.  Do the isatty function.
 */

#ifdef DEADCODE
int rmtisatty (fd)
int fd;
{
    if (isrmt (fd)) {
	return (0);
    } else {
	return (isatty (fd));
    }
}

#endif  /* DEADCODE */

/*
 *	_rmt_lseek --- perform an imitation lseek operation remotely
 */

static long _rmt_lseek (fildes, offset, whence)
int fildes;
long offset;
int whence;
{
    char buffer[BUFMAGIC];

    (VOID) sprintf (buffer, "L%d\n%d\n", offset, whence);
    if (_rmt_command (fildes, buffer) == -1) {
	return (-1);
    }
    return (_rmt_status (fildes));
}

/*
 *	Perform lseek on file.  Looks just like lseek(2) to caller.
 */

long rmtlseek (fildes, offset, whence)
int fildes;
long offset;
int whence;
{
    extern long lseek ();

    if (isrmt (fildes)) {
	return (_rmt_lseek (fildes & ~REM_BIAS, offset, whence));
    } else {
	return (lseek (fildes, offset, whence));
    }
}

/*
 *	Get file status, even if symlink.  Looks just like lstat(2) to caller.
 */

#if HAVE_SYMLINKS
/*VARARGS1*/	/* Funny stuff because of "#define stat rmtstat" in rmt.h */
int rmtlstat (path, buf)
char *path;
struct stat64 *buf;
{
    if (_rmt_dev (path)) {
	errno = EOPNOTSUPP;
	return (-1);		/* For now (fnf) */
    } else {
	return (lstat64 (path, buf));
    }
}

#endif

/*
 *	Open a local or remote file.  Looks just like open(2) to
 *	caller.
 */

int rmtopen (path, oflag, mode)
char *path;
int oflag;
int mode;
{
    if (_rmt_dev (path)) {
	return (_rmt_open (path, oflag, mode) | REM_BIAS);
    } else {
	return (open (path, oflag, mode));
    }
}

/*
 *	_rmt_open --- open a magtape device on system specified, as given user
 *
 *	file name has the form system[.user]:/dev/????
 */

#define MAXHOSTLEN	257	/* BSD allows very long host names... */

/*ARGSUSED*/	/* Turn off lint checking for unused args in next func */

static int _rmt_open (path, oflag, mode)
char *path;
int oflag;
int mode;
{
    int i;
    int rc;
    char buffer[BUFMAGIC];
    char system[MAXHOSTLEN];
    char device[BUFMAGIC];
    char login[BUFMAGIC];
    char *sys;
    char *dev;
    char *user;

    sys = system;
    dev = device;
    user = login;

#ifndef sgi
    /*
     *	first, find an open pair of file descriptors
     */

    for (i = 0; i < MAXUNIT; i++) {
	if (READ (i) == -1 && WRITE (i) == -1) {
	    break;
	}
    }
    if (i == MAXUNIT) {
	errno = EMFILE;
	return (-1);
    }

    /*
     *	pull apart system and device, and optional user
     *	don't munge original string
     */

    while (*path != '.' && *path != ':') {
	*sys++ = *path++;
    }
    *sys = '\0';
    path++;

    if (*(path - 1) == '.') {
	while (*path != ':') {
	    *user++ = *path++;
	}
	*user = '\0';
	path++;
    } else {
	*user = '\0';
    }
#else
    {
        char	*s;
	char	*strchr();

	*user = '\0';
	if ((s = strchr(path, ':')) == 0)
		*sys = '\0';
	else {
            char	*t;
	    char	*u;

	    *s = '\0';
	    if ((t = strchr(path, '.')) == 0) {
		if ((u = strchr(path, '@')) == 0) {
		    strcpy(sys, path);
		    *user = '\0';
		}
		else {
		    *u = '\0';
		    strcpy(sys, u+1);
		    strcpy(user, path);
		    *u = '@';
		}
	    }
	    else {
		*t = '\0';
		strcpy(sys, path);
		strcpy(user, t+1);
		*t = '.';
	    }
	    *s = ':';
	    path = s+1;
	}
    }
#endif
    while (*path) {
	*dev++ = *path++;
    }
    *dev = '\0';

#ifdef sgi
    /*
     * Blow off this mess if we've got inetd support locally.
     * The descriptor returned is good for both reading and
     * writing.
     */
    if (login[0] != '\0') {
	if ((i = contact(system, login, "/etc/rmt")) == -1) {
	    errno = EPERM;
	    return(-1);
        }
    }
    else {
	if ((i = contact(system, "guest", "/etc/rmt")) == -1) {
	    errno = EPERM;
	    return(-1);
	}
    }
#else
    /*
     *	setup the pipes for the 'rsh' command and fork
     */

    if (pipe (_rmt_Ptc[i]) == -1 || pipe (_rmt_Ctp[i]) == -1) {
	return (-1);
    }

    if ((rc = fork ()) == -1) {
	return (-1);
    }

    if (rc == 0) {
	(VOID) close (0);
	(VOID) dup (_rmt_Ptc[i][0]);
	(VOID) close (_rmt_Ptc[i][0]);
	(VOID) close (_rmt_Ptc[i][1]);
	(VOID) close (1);
	(VOID) dup (_rmt_Ctp[i][1]);
	(VOID) close (_rmt_Ctp[i][0]);
	(VOID) close (_rmt_Ctp[i][1]);
	if (login) {
	    (VOID) execl ("/usr/ucb/rsh", "rsh", system, "-l", login,
		    "/etc/rmt", (char *) 0);
	    (VOID) execl ("/usr/bsd/rsh", "rsh", system, "-l", login,
		    "/etc/rmt", (char *) 0);
	    (VOID) execl ("/usr/bin/remsh", "remsh", system, "-l", login,
		    "/etc/rmt", (char *) 0);
	    (VOID) execl ("/bin/remsh", "remsh", system, "-l", login,
		    "/etc/rmt", (char *) 0);
	    (VOID) execl ("/bin/rsh", "rsh", system, "-l", login,
		    "/etc/rmt", (char *) 0);
	} else {
	    (VOID) execl ("/usr/ucb/rsh", "rsh", system, "/etc/rmt", 
			  (char *) 0);
	    (VOID) execl ("/usr/bsd/rsh", "rsh", system, "/etc/rmt", 
			  (char *) 0);
	    (VOID) execl ("/usr/bin/remsh", "remsh", system, "/etc/rmt",
			  (char *) 0);
	    (VOID) execl ("/bin/remsh", "remsh", system, "/etc/rmt",
			  (char *) 0);
	    (VOID) execl ("/bin/rsh", "rsh", system, "/etc/rmt",
			  (char *) 0);
	}

	/*
	 *	bad problems if we get here
	 */

	(VOID) perror ("can't find remote shell program");
	(VOID) _exit (1);
    }

    (VOID) close (_rmt_Ptc[i][0]);
    (VOID) close (_rmt_Ctp[i][1]);
#endif

    /*
     *	now attempt to open the tape device
     */

    (VOID) sprintf (buffer, "O%s\n%d\n", device, oflag);
    if (_rmt_command (i, buffer) == -1 || _rmt_status (i) == -1) {
	return (-1);
    }

    return (i);
}

#ifdef sgi
# include	<stdio.h>
# include	<bsd/netdb.h>

contact(node, user, cmd)
   char		*node;
   char		*user;
   char		*cmd;
{
   struct servent	*seb;
   int			fd;
   char			nbuf[128];
   char			*nnbuf;
   void			(*func)();

	func = signal(SIGINT, SIG_DFL);
	if ((seb = getservbyname("exec", "tcp")) == 0) {
		fprintf(stderr, "Unable to contact name server demon\n");
		signal(SIGINT, func);
		return(-1);
	}
	strcpy(nbuf, node);
	nnbuf = nbuf;
	if ((fd = rexec(&nnbuf, seb->s_port, user, "", cmd, 0)) == -1) {
		fprintf(stderr, "Unable to invoke remote demon at %s\n",
			node);
		signal(SIGINT, func);
		return(-1);
	}
	signal(SIGINT, func);
	return(fd);
}
#endif

/*
 *	Read from stream.  Looks just like read(2) to caller.
 */

int rmtread (fildes, buf, nbyte)
int fildes;
char *buf;
unsigned int nbyte;
{
    if (isrmt (fildes)) {
#ifdef sgi
    int	cnt;
    unsigned int	i;

	i = 0;
	while (i < nbyte) {
	    if ((cnt = _rmt_read (fildes & ~REM_BIAS, &buf[i], nbyte - i)) <= 0)
		if (cnt == 0)
		    return (i);
		else if (errno != EINTR)
		    return(-1);
		else
		    continue;
	    i += cnt;
	}
        return (nbyte);
#else
	return (_rmt_read (fildes & ~REM_BIAS, buf, nbyte));
#endif
    } else {
	return (read (fildes, buf, (NBYTETYPE) nbyte));
    }
}

/*
 *	_rmt_read --- read a buffer from a remote tape
 */

static int _rmt_read (fildes, buf, nbyte)
int fildes;
char *buf;
unsigned int nbyte;
{
    int rc;
    int i;
    register int bytesread;
    char buffer[BUFMAGIC];

    (VOID) sprintf (buffer, "R%d\n", nbyte);
    if (_rmt_command (fildes, buffer) == -1 || (rc = _rmt_status (fildes)) == -1)
	return (-1);

    for (i = 0; i < rc; i += bytesread, buf += bytesread) {
	bytesread = read (READ (fildes), buf, (NBYTETYPE) rc);
	if (bytesread == -1 && errno == EINTR) {
	    bytesread = 0;
	    continue;
	}
	if (bytesread <= 0) {
	    _rmt_abort (fildes);
	    errno = EIO;
	    return (-1);
	}
    }

    return (rc);
}

/*
 *	Get file status.  Looks just like stat(2) to caller.
 */

#if !BSD4_2			/* Under BSD4_2 we use rmtlstat instead */

int rmtstat (path, buf)
char *path;
struct stat64 *buf;
{
    extern int stat64 ();

    if (_rmt_dev (path)) {
	errno = EOPNOTSUPP;
	return (-1);		/* For now (fnf) */
    } else {
	return (stat64 (path, buf));
    }
}

#endif  /* Not BSD4_2 */

/*
 *	_rmt_status --- retrieve the status from the pipe
 */

int _rmt_status (fildes)
int fildes;
{
    int i;
    char c;
    int err;
    char *cp;
    char buffer[BUFMAGIC];

    /*
     *	read the reply command line
     */

    for (i = 0, cp = buffer; i < BUFMAGIC; i++, cp++) {
      readagain:
	if ((err = read (READ (fildes), cp, 1)) != 1) {
	    if (err == -1 && errno == EINTR)
		goto readagain;
	    _rmt_abort (fildes);
	    errno = EIO;
	    return (-1);
	}
	if (*cp == '\n') {
	    *cp = 0;
	    break;
	}
    }

    if (i == BUFMAGIC) {
	_rmt_abort (fildes);
	errno = EIO;
	return (-1);
    }

    /*
     *	check the return status
     */

    for (cp = buffer; *cp; cp++) {
	if (*cp != ' ') {
	    break;
	}
    }

    if (*cp == 'E' || *cp == 'F') {
	errno = atoi (cp + 1);
      scanagain:
	while ((err = read (READ (fildes), &c, 1)) == 1) {
	    if (c == '\n') {
		break;
	    }
	}
	if (err == -1)
	    if (errno == EINTR)
		goto scanagain;
	    else {
		_rmt_abort (fildes);
		return(-1);
	    }

	if (*cp == 'F') {
	    _rmt_abort (fildes);
	}

	return (-1);
    }

    /*
     *	check for mis-synced pipes
     */

    if (*cp != 'A') {
	_rmt_abort (fildes);
	errno = EIO;
	return (-1);
    }

    return (atoi (cp + 1));
}

/*
 *	Write to stream.  Looks just like write(2) to caller.
 */

int rmtwrite (fildes, buf, nbyte)
int fildes;
char *buf;
unsigned int nbyte;
{
    if (isrmt (fildes)) {
#ifdef notdef
    int	cnt;
    unsigned int	i;

	i = 0;
	while (i < nbyte) {
	    if ((cnt = _rmt_write (fildes &~REM_BIAS, &buf[i], nbyte - i)) <= 0)
		if (cnt == 0)
		    return(i);
		else if (errno != EINTR)
		    return(-1);
		else
		    continue;
	    i += cnt;
	}
	return (nbyte);
#else
	return (_rmt_write (fildes & ~REM_BIAS, buf, nbyte));
#endif
    } else {
	return (write (fildes, buf, (NBYTETYPE) nbyte));
    }
}

/*
 *	_rmt_write --- write a buffer to the remote tape
 */

static int _rmt_write (fildes, buf, nbyte)
int fildes;
char *buf;
unsigned int nbyte;
{
    char buffer[BUFMAGIC];
    SIGTYPE pstat;
    int err;

    (VOID) sprintf (buffer, "W%d\n", nbyte);
    if (_rmt_command (fildes, buffer) == -1) {
	return (-1);
    }

    pstat = signal (SIGPIPE, SIG_IGN);
    if ((err = write (WRITE (fildes), buf, (NBYTETYPE) nbyte)) == nbyte) {
	(VOID) signal (SIGPIPE, pstat);
	return (_rmt_status (fildes));
    }
    if (err == -1 && errno == EINTR) {
        (VOID) signal (SIGPIPE, pstat);
	errno = EINTR;
        return(-1);
    }

    (VOID) signal (SIGPIPE, pstat);
    _rmt_abort (fildes);
    errno = EIO;
    return (-1);
}

#else

#ifdef amiga
int lattice_hack;		/* Keeps Lattice 3.10 happy... */
#endif

#endif				/* RMT */
