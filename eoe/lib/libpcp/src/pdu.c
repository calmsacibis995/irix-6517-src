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

#ident "$Id: pdu.c,v 2.39 1998/11/15 08:35:24 kenmcd Exp $"

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#if defined(sgi)
#include <bstring.h>
#endif
#include "pmapi_dev.h"

#ifdef MALLOC_AUDIT
/*
 * PDU buffer allocations here are permanent, and do not cause any
 * memory leaks
 */
#include "no-malloc-audit.h"
#endif

extern int	errno;
extern int	__pmLastUsedFd;

int		pmDebug;		/* the real McCoy */

#define ABUF_SIZE	1024	/* read buffer for reading ascii PDUs */

/*
 * Performance Instrumentation
 *  ... counts binary PDUs received and sent by low 4 bits of PDU type
 */

unsigned int	__pmPDUCntIn[PDU_MAX+1];
unsigned int	__pmPDUCntOut[PDU_MAX+1];

/*
 * unit of space allocation for PDU buffer ... actually defined in pdubuf.c,
 * it should have the same value here.
 */
#define PDU_CHUNK	1024

typedef struct {		/* this is like stdio for reading ascii PDUs */
    int		fd;
    int		timeout;
    int		len;
    int		next;
    char	abuf[ABUF_SIZE];
} ascii_ctl;

static int		mypid = -1;
static ascii_ctl	ac;

typedef struct {
    __pmPDU	*pdubuf;
    int		len;
} more_ctl;

static more_ctl		*more;
static int		maxfd = -1;

static void
moreinput(int fd, __pmPDU *pdubuf, int len)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	__pmPDUHdr	*php = (__pmPDUHdr *)pdubuf;
	int		j;
	__pmPDU		*p;
	char		*q;
	int		jend = (ntohl(php->len)+(int)sizeof(__pmPDU)-1)/(int)sizeof(__pmPDU);

	fprintf(stderr, "[%d]moreinput fd=%d pdubuf=0x%p len=%d\n",
	    mypid, fd, pdubuf, len);
	fprintf(stderr, "Piggy-back PDU: %s addr=0x%p len=%d from=%d",
		__pmPDUTypeStr(ntohl(php->type)), php, ntohl(php->len), ntohl(php->from));
	fprintf(stderr, "%03d: ", 0);
	p = (__pmPDU *)php;

	/* for Purify ... */
	q = (char *)p + (ntohl(php->len));
	while (q < (char *)p + jend*sizeof(__pmPDU))
	    *q++ = '~';	/* buffer end */

	for (j = 0; j < jend; j++) {
	    if ((j % 8) == 0)
		fprintf(stderr, "\n%03d: ", j);
	    fprintf(stderr, "%8x ", p[j]);
	}
	putc('\n', stderr);
    }
#endif

    if (fd > maxfd) {
	int	next = maxfd + 1;
	if ((more = (more_ctl *)realloc(more, (fd+1)*sizeof(more[0]))) == NULL) {
	    __pmNoMem("moreinput: realloc", (fd+1)*sizeof(more[0]), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	maxfd = fd;
	while (next <= maxfd) {
	    more[next].pdubuf = NULL;
	    next++;
	}
    }

    __pmPinPDUBuf(pdubuf);
    more[fd].pdubuf = pdubuf;
    more[fd].len = len;
}

int
__pmMoreInput(int fd)
{
    if (fd < 0 || fd > maxfd)
	return 0;
    
    return more[fd].pdubuf == NULL ? 0 : 1;
}

void
__pmNoMoreInput(int fd)
{
    if (fd < 0 || fd > maxfd)
	return;
    
    if (more[fd].pdubuf != NULL) {
	__pmUnpinPDUBuf(more[fd].pdubuf);
	more[fd].pdubuf = NULL;
    }
}

static int
pduread(int fd, char *buf, int len, int mode, int timeout)
{
    /*
     * handle short reads that may spilt a PDU ...
     */
    int			status = 0;
    int			have = 0;
    fd_set		onefd;
    static struct timeval	def_wait = { 10, 0 };
    static int		done_default = 0;

    if (timeout == TIMEOUT_DEFAULT) {
	if (!done_default) {
	    double	def_timeout;
	    char	*timeout_str;
	    char	*end_ptr;
	    if ((timeout_str = getenv("PMCD_REQUEST_TIMEOUT")) != NULL) {

		def_timeout = strtod(timeout_str, &end_ptr);
		if (*end_ptr != '\0' || def_timeout < 0.0) {
		    __pmNotifyErr(LOG_WARNING,
				 "pduread: ignored bad PMCD_REQUEST_TIMEOUT = '%s'\n",
				 timeout_str);
		}
		else {
		    def_wait.tv_sec = (int)def_timeout;		/* truncate -> secs */
		    if (def_timeout > (double)def_wait.tv_sec)
			def_wait.tv_usec = (long)((def_timeout - (double)def_wait.tv_sec) * 1000000);
		    else
			def_wait.tv_usec = 0;
		}
	    }
	    done_default = 1;
	}
    }

    while (len) {
	if (timeout == GETPDU_ASYNC) {
	    /*
	     * no grabbing more than you need ... read header to get
	     * length and then read body.
	     * This assumes you are either willing to block, or have
	     * already done a select() and are pretty confident that
	     * you will not block.
	     * Also assumes buf is aligned on a __pmPDU boundary.
	     */
	    __pmPDU	*lp;
	    status = (int)read(fd, buf, (int)sizeof(__pmPDU));
	    __pmLastUsedFd = fd;
	    if (status <= 0)
		/* EOF or error */
		return status;
	    else if (status != sizeof(__pmPDU))
		/* short read, bad error! */
		return PM_ERR_IPC;
	    lp = (__pmPDU *)buf;
	    have = ntohl(*lp);
	    status = (int)read(fd, &buf[sizeof(__pmPDU)], have - (int)sizeof(__pmPDU));
	    if (status <= 0)
		/* EOF or error */
		return status;
	    else if (status != have - (int)sizeof(__pmPDU))
		/* short read, bad error! */
		return PM_ERR_IPC;
	    break;
	}
	else {
	    struct timeval	wait;
	    /*
	     * either never timeout (i.e. block forever), or timeout
	     */
	    if (timeout != TIMEOUT_NEVER) {
		if (timeout > 0) {
		    wait.tv_sec = timeout;
		    wait.tv_usec = 0;
		}
		else
		    wait = def_wait;
		FD_ZERO(&onefd);
		FD_SET(fd, &onefd);
		status = select(fd+1, &onefd, NULL, NULL, &wait);
		if (status == 0) {
		    if (__pmGetInternalState() != PM_STATE_APPL) {
			/* special for PMCD and friends */
			__pmNotifyErr(LOG_WARNING, "pduread: timeout (after %d.%03d sec) on fd=%d",
				(int)wait.tv_sec, 1000*(int)wait.tv_usec, fd);
		    }
		    return PM_ERR_TIMEOUT;
		}
		else if (status < 0) {
		    __pmNotifyErr(LOG_ERR, "pduread: select() on fd=%d: %s",
			    fd, strerror(errno));
		    return status;
		}
	    }
	    status = (int)read(fd, buf, len);
	    __pmLastUsedFd = fd;
	    if (status <= 0 || mode == PDU_ASCII)
		/* ASCII, EOF or error */
		return status;
	    if (mode == -1)
		/* special case, see __pmGetPDU */
		return status;
	    have += status;
	    buf += status;
	    len -= status;
	}
    }

    return have;
}

const char *
__pmPDUTypeStr(int type)
{
    /* libpcp PDU names */
    if (type == PDU_ERROR) return "ERROR";
    else if (type == PDU_RESULT) return "RESULT";
    else if (type == PDU_PROFILE) return "PROFILE";
    else if (type == PDU_FETCH) return "FETCH";
    else if (type == PDU_DESC_REQ) return "DESC_REQ";
    else if (type == PDU_DESC) return "DESC";
    else if (type == PDU_INSTANCE_REQ) return "INSTANCE_REQ";
    else if (type == PDU_INSTANCE) return "INSTANCE";
    else if (type == PDU_TEXT_REQ) return "TEXT_REQ";
    else if (type == PDU_TEXT) return "TEXT";
    else if (type == PDU_CONTROL_REQ) return "CONTROL_REQ";
    else if (type == PDU_DATA_X) return "DATA_X";
    else if (type == PDU_CREDS) return "CREDS";
    else if (type == PDU_PMNS_IDS) return "PMNS_IDS";
    else if (type == PDU_PMNS_NAMES) return "PMNS_NAMES";
    else if (type == PDU_PMNS_CHILD) return "PMNS_CHILD";
    else if (type == PDU_PMNS_TRAVERSE) return "PMNS_TRAVERSE";
    /* pmlc <-> pmlogger PDU names from libpcp_dev */
    else if (type == PDU_LOG_CONTROL) return "LOG_CONTROL";
    else if (type == PDU_LOG_STATUS) return "LOG_STATUS";
    else if (type == PDU_LOG_REQUEST) return "LOG_REQUEST";
    /* and anything else ... */
    else {
	static char	buf[20];
	sprintf(buf, "TYPE-%d?", type);
	return buf;
    }
}

/* Because the default handler for SIGPIPE is to exit, we always want a handler
   installed to override that so that the write() just returns an error.  The
   problem is that the user might have installed one prior to the first write()
   or may install one at some later stage.  This doesn't matter.  As long as a
   handler other than SIG_DFL is there, all will be well.  The first time that
   __pmXmitPDU is called, install SIG_IGN as the handler for SIGPIPE.  If the
   user had already changed the handler from SIG_DFL, put back what was there
   before. */

static int sigpipe_done = 0;		/* First time check for installation of
					   non-default SIGPIPE handler */

int
__pmXmitPDU(int fd, __pmPDU *pdubuf)
{
    int		n;
    int		len;
    __pmPDUHdr	*php = (__pmPDUHdr *)pdubuf;

    /* assume PDU_BINARY ... should not be here, otherwise */
    if (!sigpipe_done) {	/* Make sure SIGPIPE is handled */
	SIG_PF	user_onpipe;
	user_onpipe = signal(SIGPIPE, SIG_IGN);
	if (user_onpipe != SIG_DFL)	/* Put user handler back */
	    signal(SIGPIPE, user_onpipe);
	sigpipe_done = 1;
    }

    if (mypid == -1)
	mypid = getpid();
    php->from = mypid;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	int	j;
	char	*p;
	int	jend = (php->len+(int)sizeof(__pmPDU)-1)/(int)sizeof(__pmPDU);

	/* for Purify ... */
	p = (char *)pdubuf + php->len;
	while (p < (char *)pdubuf + jend*sizeof(__pmPDU))
	    *p++ = '~';	/* buffer end */

	fprintf(stderr, "[%d]pmXmitPDU: %s fd=%d len=%d",
		mypid, __pmPDUTypeStr(php->type), fd, php->len);
	for (j = 0; j < jend; j++) {
	    if ((j % 8) == 0)
		fprintf(stderr, "\n%03d: ", j);
	    fprintf(stderr, "%8x ", pdubuf[j]);
	}
	putc('\n', stderr);
    }
#endif
    len = php->len;

    php->len = htonl(php->len);
    php->from = htonl(php->from);
    php->type = htonl(php->type);
    n = (int)write(fd, pdubuf, len);
    php->len = ntohl(php->len);
    php->from = ntohl(php->from);
    php->type = ntohl(php->type);

    if (n != len)
	return -errno;

    __pmLastUsedFd = fd;
    if (php->type >= PDU_START && php->type <= PDU_FINISH)
	__pmPDUCntOut[php->type-PDU_START]++;

    return n;
}

int
__pmXmitAscii(int fd, const char *buf, int nbytes)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	fprintf(stderr, "[%d]pmXmitPDU: fd=%d ASCII (%d bytes): %s",
	    mypid, fd, nbytes, buf);
	if (buf[nbytes-1] != '\n')
	    putc('\n', stderr);
    }
#endif
    if (!sigpipe_done) {	/* Make sure SIGPIPE is handled */
	SIG_PF	user_onpipe;
	user_onpipe = signal(SIGPIPE, SIG_IGN);
	if (user_onpipe != SIG_DFL)	/* Put user handler back */
	    signal(SIGPIPE, user_onpipe);
	sigpipe_done = 1;
    }

    if (write(fd, buf, nbytes) != nbytes)
	return -errno;

    __pmLastUsedFd = fd;
    return 0;
}

int
__pmGetPDU(int fd, int mode, int timeout, __pmPDU **result)
{
    int			need;
    int			len;
    static int		maxsize = PDU_CHUNK;
    char		*handle;
    __pmPDU		*pdubuf;
    __pmPDU		*pdubuf_prev;
    __pmPDUHdr		*php;

    if (mypid == -1)
	mypid = getpid();
    if (mode == PDU_BINARY) {
	/*
	 * Note.
	 *	This stuff is rather tricky.  What we try to do is read()
	 *	an amount of data equal to the largest PDU we have (or are
	 *	likely to have) seen thus far.  In the majority of cases
	 *	this returns exactly one PDU's worth, i.e. read() returns
	 *	a length equal to php->len.
	 *
	 *	For this to work, we have a special "mode" of -1
	 *	to pduread() which means "really do a PDU_BINARY" mode read,
	 *	but return after the first read(), rather than trying to 
	 *      read up to the request length with multiple read()s, which
	 *      would of course "hang" after the first PDU arrived.
	 *
	 *      We need to handle the following tricky cases.
	 *      1. We get _more_ than we need for a single PDU -- happens
	 *         when PROFILE and FETCH PDU's arrive together.  This
	 *         requires "moreinput" to avoid the select() in pmcd's
	 *         ClientInput loop, and to handle leftovers here (it gets
	 *         even uglier if we have part, but not all of the second
	 *         PDU).
	 *      2. We get _less_ than we need for a single PDU -- this
	 *         requires at least another read(), and possibly acquiring
	 *         another pdubuf and doing a memcpy() for the partial PDU
	 *         from the earlier call.
	 */
	if (__pmMoreInput(fd)) {
	    /* some leftover from last time ... handle -> start of PDU */
	    pdubuf = more[fd].pdubuf;
	    len = more[fd].len;
	    __pmNoMoreInput(fd);
	}
	else {
	    if ((pdubuf = __pmFindPDUBuf(maxsize)) == NULL)
		return -errno;
	    len = pduread(fd, (void *)pdubuf, maxsize, -1, timeout);
	}
	php = (__pmPDUHdr *)pdubuf;

	if (len < (int)sizeof(__pmPDUHdr)) {
	    if (len == -1) {
		if (errno == ECONNRESET||
#if defined(__NUTC__)
		    /* for NuTCRACKER, len is -1 and errno is 0 if client closes connection */
		    errno == 0 || 
#endif
		    errno == ETIMEDOUT || errno == ENETDOWN ||
		    errno == ENETUNREACH || errno == EHOSTDOWN ||
		    errno == EHOSTUNREACH || errno == ECONNREFUSED)
		    /*
		     * failed as a result of pmcd exiting and the connection
		     * being reset, or as a result of the kernel ripping
		     * down the connection (most likely becuase the host at
		     * the other end just took a dive)
		     *
		     * treat this like end of file on input
		     *
		     * from irix/kern/fs/nfs/bds.c seems like all of the
		     * following are peers here:
		     *  ECONNRESET (pmcd terminated?)
		     *  ETIMEDOUT ENETDOWN ENETUNREACH EHOSTDOWN EHOSTUNREACH
		     *  ECONNREFUSED
		     * peers for bds but not here:
		     *  ENETRESET ENONET ESHUTDOWN (cache_fs only?)
		     *  ECONNABORTED (accept, user req only?)
		     *  ENOTCONN (udp?)
		     *  EPIPE EAGAIN (nfs, bds & ..., but not ip or tcp?)
		     */
		    len = 0;
		else
		    __pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d BINARY hdr: %s", fd, pmErrStr(-errno));
	    }
	    else if (len > 0)
		__pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d BINARY hdr len=%d, not %d?", fd, len, sizeof(__pmPDUHdr));
	    else if (len == PM_ERR_TIMEOUT)
		return PM_ERR_TIMEOUT;
	    else if (len < 0)
		__pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d BINARY hdr: %s", fd, pmErrStr(len));
	    return len ? PM_ERR_IPC : 0;
	}

	php->len = ntohl(php->len);
	if (php->len < 0) {
	    __pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d BINARY illegal len=%d in hdr", fd, php->len);
	    return PM_ERR_IPC;
	}

	if (len == php->len)
		/* return below */
		;
	else if (len > php->len) {
	    /*
	     * read more than we need for this one, save it up for next time
	     */
	    handle = (char *)pdubuf;
	    moreinput(fd, (__pmPDU *)&handle[php->len], len - php->len);
	}
	else {
	    /*
	     * need to read more ...
	     */
	    int		tmpsize;
	    __pmPinPDUBuf(pdubuf);
	    pdubuf_prev = pdubuf;
	    if (php->len > maxsize)
		tmpsize = PDU_CHUNK * ( 1 + php->len / PDU_CHUNK);
	    else
		tmpsize = maxsize;
	    if ((pdubuf = __pmFindPDUBuf(tmpsize)) == NULL) {
		__pmUnpinPDUBuf(pdubuf_prev);
		return -errno;
	    }
	    if (php->len > maxsize)
		maxsize = tmpsize;
	    memmove((void *)pdubuf, (void *)php, len);
	    __pmUnpinPDUBuf(pdubuf_prev);
	    php = (__pmPDUHdr *)pdubuf;
	    need = php->len - len;
	    handle = (char *)pdubuf;
	    /* block until all of the PDU is received this time */
	    len = pduread(fd, (void *)&handle[len], need, PDU_BINARY, timeout);
	    if (len != need) {
		if (len == PM_ERR_TIMEOUT)
		    return PM_ERR_TIMEOUT;
		else if (len < 0)
		    __pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d BINARY data: %s", fd, pmErrStr(-errno));
		else
		    __pmNotifyErr(LOG_ERR, "__pmGetPDU: fd=%d BINARY data len=%d, not %d?", fd, len, need);

		__pmNotifyErr(LOG_ERR, "__pmGetPDU: hdr: len=0x%x type=0x%x from=0x%x\n", php->len, ntohl(php->type), ntohl(php->from));
		return PM_ERR_IPC;
	    }
	}

	*result = (__pmPDU *)php;
	php->type = ntohl((unsigned int)php->type);
	php->from = ntohl((unsigned int)php->from);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU) {
	    int		j;
	    char	*p;
	    int		jend = (php->len+(int)sizeof(__pmPDU)-1)/(int)sizeof(__pmPDU);

	    /* for Purify ... */
	    p = (char *)*result + php->len;
	    while (p < (char *)*result + jend*sizeof(__pmPDU))
		*p++ = '~';	/* buffer end */

	    fprintf(stderr, "[%d]pmGetPDU: %s fd=%d len=%d from=%d moreinput? ",
		    mypid, __pmPDUTypeStr(php->type), fd, php->len, php->from);
	    if (__pmMoreInput(fd))
		fprintf(stderr, "yes =%d", more[fd].len);
	    else
		fprintf(stderr, "no");
	    for (j = 0; j < jend; j++) {
		if ((j % 8) == 0)
		    fprintf(stderr, "\n%03d: ", j);
		fprintf(stderr, "%8x ", (*result)[j]);
	    }
	    putc('\n', stderr);
	}
#endif
	if (php->type >= PDU_START && php->type <= PDU_FINISH)
	    __pmPDUCntIn[php->type-PDU_START]++;

	return php->type;
    }
    else {
	/* assume PDU_ASCII */
	if (ac.len && ac.next < ac.len) {
	    __pmNotifyErr(LOG_WARNING, "__pmGetPDU: fd=%d extra ASCII PDU text discarded \"%s\"", fd, &ac.abuf[ac.next]);
	}
	ac.fd = fd;
	ac.timeout = timeout;
	ac.len = pduread(fd, ac.abuf, ABUF_SIZE-1, PDU_ASCII, timeout);
	ac.next = 0;

	/* expect at least one line, with magic PDU cookie at the start */
	if (ac.len == PM_ERR_IPC || ac.len == PM_ERR_TIMEOUT)
	    /* errno not always set */
	    return ac.len;
	else if (ac.len < 0)
	    return -errno;
	else if (ac.len == 0)
	    return PM_ERR_EOF;
	ac.abuf[ac.len] = '\0';
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDU) {
	    fprintf(stderr, "[%d]pmGetPDU: fd=%d ASCII (%d bytes): %s",
		mypid, fd, ac.len, ac.abuf);
	    if (ac.abuf[ac.len-1] != '\n')
		putc('\n', stderr);
	}
#endif

	*result = (__pmPDU *)&ac;		/* ugly, but caller sees this as opaque */

	if (strncmp(ac.abuf, "INSTANCE_REQ", 12) == 0)
	    return PDU_INSTANCE_REQ;
	else if (strncmp(ac.abuf, "CONTROL_REQ", 11) == 0)
	    return PDU_CONTROL_REQ;
	else if (strncmp(ac.abuf, "DESC_REQ", 8) == 0)
	    return PDU_DESC_REQ;
	else if (strncmp(ac.abuf, "INSTANCE", 8) == 0)
	    return PDU_INSTANCE;
	else if (strncmp(ac.abuf, "DESC_REQ", 8) == 0)
	    return PDU_DESC_REQ;
	else if (strncmp(ac.abuf, "TEXT_REQ", 8) == 0)
	    return PDU_TEXT_REQ;
	else if (strncmp(ac.abuf, "PROFILE", 7) == 0)
	    return PDU_PROFILE;
	else if (strncmp(ac.abuf, "RESULT", 6) == 0)
	    return PDU_RESULT;
	else if (strncmp(ac.abuf, "ERROR", 5) == 0)
	    return PDU_ERROR;
	else if (strncmp(ac.abuf, "FETCH", 5) == 0)
	    return PDU_FETCH;
	else if (strncmp(ac.abuf, "DESC", 4) == 0)
	    return PDU_DESC;
	else if (strncmp(ac.abuf, "TEXT", 4) == 0)
	    return PDU_TEXT;
	else {
	    char	*p;

	    for (p = ac.abuf; *p && *p != ' ' && *p != '\n'; p++)
		;
	    *p = '\0';
	    __pmNotifyErr(LOG_WARNING, "__pmGetPDU: fd=%d ASCII type=\"%s\"?", fd, ac.abuf);
	    return PM_ERR_IPC;
	}
    }
}

/*
 * deprecated old interface ...
 */
int
__pmRecvPDU(int fd, int mode, __pmPDU **result)
{
    return __pmGetPDU(fd, mode, TIMEOUT_NEVER, result);
}

/*
 * Fetch next line of input from an ASCII format PDU stream.
 *
 * Note: pdubuf is really an opaque pointer to the (ascii_ctl *) value
 * returned from an earlier call to __pmGetPDU.
 */
int
__pmRecvLine(__pmPDU *pdubuf, int maxch, char *lbuf)
{
    ascii_ctl	*ap = (ascii_ctl *)pdubuf;
    char	*p = lbuf;
    char	*lend = &lbuf[maxch];

    for ( ; ; ) {
	if (ap->next >= ap->len) {
	    ap->len = pduread(ap->fd, ap->abuf, ABUF_SIZE, PDU_ASCII, ap->timeout);
	    if (ap->len == PM_ERR_IPC || ap->len == PM_ERR_TIMEOUT)
		/* errno not always set */
		return ap->len;
	    else if (ap->len < 0)
		return -errno;
	    else if (ap->len == 0)
		return PM_ERR_EOF;
	    ap->abuf[ac.len] = '\0';
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PDU) {
		fprintf(stderr, "[%d]pmRecvLine: ASCII (%d bytes): %s", mypid, ap->len, ap->abuf);
		if (ap->abuf[ap->len-1] != '\n')
		    putc('\n', stderr);
	    }
#endif
	    ap->next = 0;
	}
	if (p >= lend)
	    return PM_ERR_TOOBIG;
	*p = ap->abuf[ap->next++];
	if (*p == '\n')
	    break;
	p++;
    }
    *p = '\0';

    return (int)(p - lbuf);
}
