/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: connect.c,v 2.52 1999/04/29 23:22:55 kenmcd Exp $"

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <syslog.h>
#include <netdb.h>
#include <errno.h>
#include <limits.h>
#if defined(HAVE_DLFCN)
#include <dlfcn.h>
#endif

/* default connect timeout is 5 seconds */
static struct timeval	canwait = { 5, 000000 };

static struct itimerval	my_itimer;
static struct itimerval	off_itimer;

/*
 * If additional DSOs must be supported via PM_CONTEXT_LOCAL then add their
 * definitions in here.
 */
#define SAMPLE_DSO 30
#define PROC_DSO 3

static __pmDSO dsotab[] = {
    {	       1, "libirixpmda.so",	"irix_init", },
    {	PROC_DSO, "pmda_proc.so",	"proc_init", },
    { SAMPLE_DSO, "pmda_sample.so",	"sample_init", }
};
static int	numdso = (sizeof(dsotab)/sizeof(dsotab[0]));

static char     *dsoprefix =
#if   _MIPS_SIM == _MIPS_SIM_ABI32
    "mips_o32.";
#elif _MIPS_SIM == _MIPS_SIM_NABI32
    "mips_n32.";
#elif _MIPS_SIM == _MIPS_SIM_ABI64
    "mips_64.";
#elif defined(__i386__) || defined(__i486__) || defined(__i586__)
    "intel_32."
#else
    !!! bozo : dont know which MIPS executable format pmcd should be!!!
#endif

#if !defined(HAVE_HSTRERROR)
static char *
hstrerror(int h_errno)
{
    switch (h_errno) {
	case 0: return "";
	case 1: return "Host not found";
	case 2: return "Try again";
	case 3: return "Non-recoverable error";
	case 4: return "No address";
	default: return "Unknown error";
    }
}
#endif /* HAVE_HSTRERROR */

/*
 * search for a PMDA using /var/pcp/lib-/usr/pcp/lib equivalence and
 * the $PMDA_PATH search path
 */
const char *
__pmFindPMDA(const char *name)
{
    static char	myname[MAXPATHLEN];
    char	*path;
    char	*p;
    char	*q;

    if (access(name, F_OK) == 0) {
	if (*name == '.' || *name == '/')
	    /* dlopen() will not use LD_LIBRARY_PATH et al */
	    return name;

	/* relative path, be paranoid */
	strcpy(myname, "./");
	strcat(myname, name);
	return myname;
    }

    if (*name == '/') {			/* absolute path */
	int	alt = 0;

	strcpy(myname, name);
	if (strncmp(myname, "/usr/pcp/lib", 12) == 0) {
	    myname[1] = 'v';
	    myname[2] = 'a';
	    alt = 1;
	}
	else if (strncmp(myname, "/var/pcp/lib", 12) == 0) {
	    myname[1] = 'u';
	    myname[2] = 's';
	    alt = 1;
	}
	if (alt && access(myname, F_OK) == 0)
	    return myname;

	return NULL;
    }

    /* try PMDA_PATH from environment */
    path = getenv("PMDA_PATH");
    if (path == NULL)
	path = "/var/pcp/lib:/usr/pcp/lib";

    for (p = q = path; ;q++) {
	if (q > p && (*q == ':' || *q == '\0')) {
	    strncpy(myname, p, q - p);
	    myname[q - p] = '\0';
	    p = q + 1;
	    strcat(myname, "/");
	    strcat(myname, name);
	    if (access(myname, F_OK) == 0)
		return myname;
	}
	if (*q == '\0')
	    break;
    }

    return NULL;
}

/*ARGSUSED1*/
static void
onalarm(int dummy)
{
}

int
__pmAuxConnectPMCD(const char *hostname)
{
    int			sts;
    int			rc;
    struct sockaddr_in	myAddr;
    struct hostent*	servInfo;
    int			fd;	/* Fd for socket connection to pmcd */
    int			fdFlags;
    int			nodelay=1;
    struct linger	nolinger = {1, 0};
    struct itimerval	old_itimer;
    extern int		errno;
    extern int		h_errno;
    void		(*old_handler)(int foo);
    char		*env_str;
    double		timeout;
    static int		first_time = 1;
    static int		pmcd_port;

    if ((servInfo = gethostbyname(hostname)) == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "__pmAuxConnectPMCD(%s) : h_errno=%d, ``%s''\n",
		hostname, h_errno, hstrerror(h_errno));
	}
#endif
	return -EHOSTUNREACH;
    }

    /*
     * get optional stuff from environment ...
     * 	PMCD_CONNECT_TIMEOUT
     *	PMCD_PORT
     */
    if (first_time) {
	first_time = 0;

	if ((env_str = getenv("PMCD_CONNECT_TIMEOUT")) != NULL) {
	    char	*end_ptr;

	    timeout = strtod(env_str, &end_ptr);
	    if (*end_ptr != '\0' || timeout < 0.0)
		__pmNotifyErr(LOG_WARNING,
			     "__pmAuxConnectPMCD: ignored bad PMCD_CONNECT_TIMEOUT = '%s'\n",
			     env_str);
	    else {
		canwait.tv_sec = (time_t)timeout;
		canwait.tv_usec = (int)((timeout - (double)canwait.tv_sec) * 1000000);
	    }
	}

	if ((env_str = getenv("PMCD_PORT")) != NULL) {
	    char	*end_ptr;

	    pmcd_port = (int)strtol(env_str, &end_ptr, 0);
	    if (*end_ptr != '\0' || pmcd_port < 0) {
		__pmNotifyErr(LOG_WARNING,
			     "__pmAuxConnectPMCD: ignored bad PMCD_PORT = '%s'\n",
			     env_str);
		pmcd_port = SERVER_PORT;
	    }
	}
	else
	    pmcd_port = SERVER_PORT;

    }

    /* Create socket and attempt to connect to the local PMCD */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
	return -errno;

    /* avoid 200 ms delay */
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &nodelay, sizeof(nodelay)) < 0) {
        __pmNotifyErr(LOG_ERR, "__pmAuxConnectPMCD(%s): setsockopt TCP_NODELAY: %s\n",
	    hostname, strerror(errno));
    }

    /* don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &nolinger, sizeof(nolinger)) < 0) {
        __pmNotifyErr(LOG_ERR, "__pmAuxConnectPMCD(%s): setsockopt SO_LINGER: %s\n",
	    hostname, strerror(errno));
    }

    memset(&myAddr, 0, sizeof(myAddr));	/* Arrgh! &myAddr, not myAddr */
    myAddr.sin_family = AF_INET;
    memcpy(&myAddr.sin_addr, servInfo->h_addr, servInfo->h_length);
    myAddr.sin_port = htons(pmcd_port);

    /*
     * arm interval timer
     */
    my_itimer.it_value = canwait;
    my_itimer.it_interval.tv_sec = 0;
    my_itimer.it_interval.tv_usec = 0;
    old_handler = signal(SIGALRM, onalarm);
    setitimer(ITIMER_REAL, &my_itimer, &old_itimer);
#ifdef DESPERATE
    fprintf(stderr, "old before: interval %d.%06d, value %d.%06d\n",
	old_itimer.it_interval.tv_sec, old_itimer.it_interval.tv_usec,
	old_itimer.it_value.tv_sec, old_itimer.it_value.tv_usec);
    fprintf(stderr, "my before: interval %d.%06d, value %d.%06d\n",
	my_itimer.it_interval.tv_sec, my_itimer.it_interval.tv_usec,
	my_itimer.it_value.tv_sec, my_itimer.it_value.tv_usec);
#endif

    rc = connect(fd, (struct sockaddr*) &myAddr, sizeof(myAddr));
    sts = errno;

    /*
     * disarm the interval timer, re-install the earlier SIGALRM signal
     * handler, and if necessary re-arm interval timer
     */
    setitimer(ITIMER_REAL, &off_itimer, &my_itimer);
    signal(SIGALRM, old_handler);
#ifdef DESPERATE
    fprintf(stderr, "my after: interval %d.%06d, value %d.%06d\n",
	my_itimer.it_interval.tv_sec, my_itimer.it_interval.tv_usec,
	my_itimer.it_value.tv_sec, my_itimer.it_value.tv_usec);
#endif
    if (old_itimer.it_value.tv_sec != 0 && old_itimer.it_value.tv_usec != 0) {
	old_itimer.it_value.tv_usec -= canwait.tv_usec - my_itimer.it_value.tv_usec;
	while (old_itimer.it_value.tv_usec < 0) {
	    old_itimer.it_value.tv_usec += 1000000;
	    old_itimer.it_value.tv_sec--;
	}
	while (old_itimer.it_value.tv_usec > 1000000) {
	    old_itimer.it_value.tv_usec -= 1000000;
	    old_itimer.it_value.tv_sec++;
	}
	old_itimer.it_value.tv_sec -= canwait.tv_sec - my_itimer.it_value.tv_sec;
	if (old_itimer.it_value.tv_sec < 0) {
	    /* missed the user's itimer, pretend there is 1 msec to go! */
	    old_itimer.it_value.tv_sec = 0;
	    old_itimer.it_value.tv_usec = 1000;
	}
	setitimer(ITIMER_REAL, &old_itimer, &my_itimer);
#ifdef DESPERATE
	fprintf(stderr, "old after: interval %d.%06d, value %d.%06d\n",
	    old_itimer.it_interval.tv_sec, old_itimer.it_interval.tv_usec,
	    old_itimer.it_value.tv_sec, old_itimer.it_value.tv_usec);
#endif
    }

    if (rc < 0) {
	if (sts == EINTR)
	    sts = ETIMEDOUT;
	close(fd);	/* safe for __pmNoMoreInput() ... not used yet */
	return -sts;
    }

    /* make sure this file descriptor is closed if exec() is called */
    fdFlags = fcntl(fd, F_GETFD);
    if (fdFlags != -1)
	sts = fcntl(fd, F_SETFD, fdFlags | FD_CLOEXEC);
    else
	sts = -1;
    if (sts == -1) {
	__pmNotifyErr(LOG_WARNING,
		     "__pmAuxConnectPMCD(%s): fcntl() get/set flags failed: %s\n",
		     hostname, strerror(errno));
    }

    if (sts < 0)
	return sts;

    return fd;
}

/*
 * for libpcp and unauthorized clients, __pmConnectHostMethod() leads
 * here and PMCD connection failure ...
 */
/*ARGSUSED*/
static int
pcp_trustme(int fd, int what)
{
    return PM_ERR_PMCDLICENSE;
}

/*
 * Function pointer manipulation here is used to make the authorised
 * pmcd connect functionality accessible via a single call to
 * __pmConnectPMCD and in such a way that all subsequent calls will do
 * the right thing (always) when deciding which to call.
 */
__pmConnectHostType __pmConnectHostMethod = pcp_trustme;

int
__pmConnectPMCD(const char *hostname)
{
    __pmIPC	ipcinfo = { UNKNOWN_VERSION, NULL };
    __pmPDU	*pb;
    int		sts;
    int		ok;
    int		challenge;
    int		fd;	/* Fd for socket connection to pmcd */

    if ((fd = __pmAuxConnectPMCD(hostname)) < 0)
	return fd;

    /* Expect an error PDU back from PMCD: ACK/NACK for connection */
    sts = __pmGetPDU(fd, PDU_BINARY, TIMEOUT_DEFAULT, &pb);
    if (sts == PDU_ERROR) {
	/*
	 * see comments in pmcd ... we actually get an extended PDU
	 * from a 2.0 pmcd, of the form
	 *
	 *  :----------:-----------:
	 *  |  status  | challenge |
	 *  :----------:-----------:
	 *
	 *                            status      challenge
	 *     pmcd  licensed             0          bits
	 *	     unlicensed       -1007          bits
	 *
	 *  -1007 is magic and is PM_ERR_LICENSE for PCP 1.x
	 *
	 * a 1.x pmcd will send us just the regular error PDU with
	 * a "status" value.
	 */
	ok = __pmDecodeXtendError(pb, PDU_BINARY, &sts, &challenge);
	if (ok < 0) {
	    sts = ok;
	    goto failed;
	}

	/*
	 * At this point, ok is PDU_VERSION1 or PDU_VERSION2 and
	 * sts is a PCP 2.0 error code
	 */
	ipcinfo.version = ok;
	ipcinfo.ext = NULL;
	if ((ok = __pmAddIPC(fd, ipcinfo)) < 0) {
	    sts = ok;
	    goto failed;
	}

	if (ipcinfo.version == PDU_VERSION1) {
	    /* 1.x pmcd */
	    ;
	}
	else if (sts < 0 && sts != PM_ERR_LICENSE) {
	    /*
	     * 2.0+ pmcd, but we have a fatal error on the connection ...
	     */
	    ;
	}
	else {
	    /*
	     * 2.0+ pmcd, either pmcd is not licensed or no error so far,
	     * so negotiate connection version and credentials
	     */
	    __pmPDUInfo	*pduinfo;
	    __pmCred	handshake[2];

	    /*
	     * note: __pmDecodeXtendError() has not swabbed challenge
	     * because it does not know it's data type.
	     */
	    pduinfo = (__pmPDUInfo *)&challenge;
	    *pduinfo = __ntohpmPDUInfo(*pduinfo);

	    if (pduinfo->licensed == PM_LIC_COL) {
		/* licensed pmcd, accept all */
		handshake[0].c_type = CVERSION;
		handshake[0].c_vala = PDU_VERSION;
		handshake[0].c_valb = 0;
		handshake[0].c_valc = 0;
		sts = __pmSendCreds(fd, PDU_BINARY, 1, handshake);
	    }
	    else
		/* pmcd not licensed, are you an authorized client? */
		sts = __pmConnectHostMethod(fd, challenge);
	}
    }
    else
	sts = PM_ERR_IPC;

failed:
    if (sts < 0) {
	__pmNoMoreInput(fd);
	close(fd);
	return sts;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "__pmConnectPMCD(%s): pmcd PDU version=%u\n",
					    hostname, ipcinfo.version);
	__pmPrintIPC();
    }
#endif

    return fd;
}

__pmDSO *
__pmLookupDSO(int domain)
{
    int		i;
    for (i = 0; i < numdso; i++) {
	if (dsotab[i].domain == domain && dsotab[i].handle != NULL)
	    return &dsotab[i];
    }
    return NULL;
}

int
__pmConnectLocal(void)
{
    static int		done_init = 0;
    int			i;
    __pmDSO		*dp;
    char		pathbuf[MAXPATHLEN];
    const char		*path;
    unsigned int	challenge;
    void		(*initp)(pmdaInterface *);

    if (done_init)
	return 0;

    for (i = 0; i < numdso; i++) {
	dp = &dsotab[i];
	if (dp->domain == SAMPLE_DSO) {
	    /*
	     * only attach sample pmda dso if env var PCP_LITE_SAMPLE
	     * or PMDA_LOCAL_SAMPLE is set
	     */
	    if (getenv("PCP_LITE_SAMPLE") == NULL &&
		getenv("PMDA_LOCAL_SAMPLE") == NULL) {
		/* no sample pmda */
		dp->domain = -1;
		continue;
	    }
	}
	if (dp->domain == PROC_DSO) {
	    /*
	     * only attach proc pmda dso if env var PMDA_LOCAL_PROC
	     * is set
	     */
	    if (getenv("PMDA_LOCAL_PROC") == NULL) {
		/* no proc pmda */
		dp->domain = -1;
		continue;
	    }
	}

	strcpy(pathbuf, dsoprefix);
	strcat(pathbuf, dp->name);
	path = __pmFindPMDA(pathbuf);

	if (path == NULL) {

	    pmprintf("__pmConnectLocal: Warning: cannot find DSO \"%s\"\n", 
			 pathbuf);
	    pmflush();
	    dp->domain = -1;
	    dp->handle = NULL;
	}
	else {
	    dp->handle = dlopen(path, RTLD_NOW);
	    if (dp->handle == NULL) {
		pmprintf("__pmConnectLocal: Warning: error attaching DSO \"%s\"\n", path);
		pmprintf("%s\n\n", dlerror());
		pmflush();
		dp->domain = -1;
	    }
	}

	if (dp->handle == NULL)
	    continue;

	initp = (void (*)(pmdaInterface *))dlsym(dp->handle, dp->init);
	if (initp == NULL) {
	    pmprintf("__pmConnectLocal: Warning: couldn't find init function "
		     "\"%s\" in DSO \"%s\"\n", dp->init, path);
	    pmflush();
	    dlclose(dp->handle);
	    dp->domain = -1;
	    continue;
	}

	/*
	 * Pass in the expected domain id.
	 * The PMDA initialization routine can (a) ignore it, (b) check it
	 * is the expected value, or (c) self-adapt.
	 */
	dp->dispatch.domain = dp->domain;

	/*
	 * the PMDA interface / PMAPI version discovery as a "challenge" ...
	 * for pmda_interface it is all the bits being set,
	 * for pmapi_version it is the complement of the one you are using now
	 */
	challenge = 0xff;
	dp->dispatch.comm.pmda_interface = challenge;
	dp->dispatch.comm.pmapi_version = ~PMAPI_VERSION;

	dp->dispatch.comm.flags = 0;
	dp->dispatch.status = 0;

	(*initp)(&dp->dispatch);

	if (dp->dispatch.status != 0) {
	    /* initialization failed for some reason */
	    pmprintf("__pmConnectLocal: Warning: initialization routine \"%s\" "
		     "failed in DSO \"%s\": %s\n", dp->init, path,
		     pmErrStr(dp->dispatch.status));
	    pmflush();
	    dlclose(dp->handle);
	    dp->domain = -1;
	}
	else {
	    if (dp->dispatch.comm.pmda_interface == challenge) {
		/*
		 * DSO did not change pmda_interface, assume PMAPI version 1
		 * from PCP 1.x and PMDA_INTERFACE_1
		 */
		dp->dispatch.comm.pmda_interface = PMDA_INTERFACE_1;
		dp->dispatch.comm.pmapi_version = PMAPI_VERSION_1;
	    }
	    else {
		/*
		 * gets a bit tricky ...
		 * interface_version (8-bits) used to be version (4-bits),
		 * so it is possible that only the bottom 4 bits were
		 * changed and in this case the PMAPI version is 1 for
		 * PCP 1.x
		 */
		if ((dp->dispatch.comm.pmda_interface & 0xf0) == (challenge & 0xf0)) {
		    dp->dispatch.comm.pmda_interface &= 0x0f;
		    dp->dispatch.comm.pmapi_version = PMAPI_VERSION_1;
		}
	    }

	    if (dp->dispatch.comm.pmda_interface < PMDA_INTERFACE_1 ||
		dp->dispatch.comm.pmda_interface > PMDA_INTERFACE_LATEST) {
		pmprintf("__pmConnectLocal: Error: Unknown PMDA interface version "
			"%d in \"%s\" DSO\n", dp->dispatch.comm.pmda_interface, path);
		pmflush();
		dlclose(dp->handle);
		dp->domain = -1;
	    }

	    if (dp->dispatch.comm.pmapi_version != PMAPI_VERSION_1 &&
		dp->dispatch.comm.pmapi_version != PMAPI_VERSION_2) {
		pmprintf("__pmConnectLocal: Error: Unknown PMAPI version %d in "
			"\"%s\" DSO\n", dp->dispatch.comm.pmapi_version, path);
		pmflush();
		dlclose(dp->handle);
		dp->domain = -1;
	    }
	}
    }
    done_init = 1;

    return 0;
}
