/************************************************************************
 *									*
 *			Copyright (c) 1988, Fred Fish			*
 *			    All Rights Reserved				*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 *	(NOTE: See attribution to original author below)		*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	dblib.c    support for double buffering under unix, using System V IPC
 *
 *  SCCS
 *
 *	@(#)dblib.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Overriding philosophy is that the parent (reader or writer) sees a
 *	very long input stream that has a definite end, with no media changes
 *	being necessary.  The child handles all media switching and block
 *	handling.
 *
 *	Since little documentation exists on the "correct" way to deal with a
 *	tape device, I'll expound on the philosophy used here.  On writing, it
 *	is assumed that writes either write the whole amount requested, or
 *	fail (a partial write causes a call to the write recovery routine
 *	however, for safety).  On reading, if a partial read occurs, it is
 *	assumed to be the result of an earlier partial write, and the read
 *	recovery routine is invoked, thus discarding the partial read.  This
 *	insures that the stream out contains a consistent view of the data.
 *	If you want to use every last inch of the tape, then a tape blocksize
 *	matching the device blocksize (512 bytes for streaming tape usually)
 *	must be used.  (J.M. Barton)
 *
 *  AUTHOR
 *
 *	Original code by J. M. Barton at Silicon Graphics Inc.
 *
 */

#include "autoconfig.h"

#if HAVE_SHM

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>

/*
 * Child return values.
 */

#define RV_EOF		1		/* end of file on input */
#define RV_WFAIL	2		/* write recovery failed */
#define RV_RFAIL	3		/* read recovery failed */
#define RV_OK		4		/* writer acknowledges end of data */
#define RV_SYSERR	5		/* system interaction error */
#define RV_SIG		6		/* signal caught */
#define RV_NOMEM	7		/* out of memory */

/*
 * Various constants.
 */

#define MAXNBUF		10		/* depends on SHMSEG kernel constant */
#define DEFBSIZE	(64*1024)	/* default buffer size */
#define DEFNBUF		4		/* default buffer count */
#define DEFTPBLK	512		/* tape block size */

/*
 * If we happen to be on a machine which can't access shared memory
 * from the I/O subsystem (stupid Berkeley VM, anyway), then we
 * have to do another copy.  Add here if you need to turn it on.
 */

#if (sgi && m68000)
#  define COPY
#endif

/*
 * A message from the other side.
 */

typedef struct {
    long    m_type;
    int     m_action;
    int     m_buf;
    void    (*m_proc) ();
    int     m_nb;
    int     m_err;
    long    m_arg;
} bufmsg_t;

struct vars_s {
    long    bsize;		/* buffer size to use */
    int     nbuf;		/* number of buffers */
    long    tpblk;		/* tape block size */
    int     bufcache[MAXNBUF];	/* hysterisis cache */
    int     ncache;		/* number in cache */
    int     curcache;		/* next cache entry */
    bufmsg_t srpc;		/* saved RPC call */
    int     issrpc;		/* if a saved one present */
    int     shm[MAXNBUF];	/* buffer descriptors */
    char   *buf[MAXNBUF];	/* buffer locations */
    long   *len[MAXNBUF];	/* length of a buffer */
    long   *rval;		/* child return value */
    int     status;		/* exit status */
    int     chendseen;		/* child end message */
    char   *sspace;		/* shared space */
    int     msg;		/* message queue */
    int     dodb;		/* if double buffering */
    int     des;		/* I/O descriptor */
    int     (*outerr) ();	/* output error */
    int     (*inerr) ();	/* input error */
    int     (*uswrite) ();	/* user write routine */
    int     (*usread) ();	/* user read routine */
    int     (*switchproc) ();	/* to call on switches */
    jmp_buf errjump;		/* for bugging out */
    int     cpid;		/* child pid */
    int     inout;		/* direction */
    int     nomore;		/* used by parent for no data */
    int     deathok;		/* OK for child to die */
#ifdef COPY
    char    *copybuf;		/* local buffer if needed */
#endif
};

static struct vars_s vars;

extern int errno;		/* system call error */
void vperror (char *format, ...);
char *malloc ();
void (*savesig)();

/*
 * dbl_errp - set the error parameters.  Meaningful at any time.
 */

int dbl_errp (inerr, outerr)
int (*inerr) ();
int (*outerr) ();
{
    vars.inerr = inerr;
    vars.outerr = outerr;
    return (0);
}

/*
 * dbl_iop - set the I/O parameters.  Only meaningful before a call to
 *	dbl_setup.
 */

int dbl_iop (readf, writef)
int (*readf) ();
int (*writef) ();
{
    if (readf != 0) {
	vars.usread = readf;
    }
    if (writef != 0) {
	vars.uswrite = writef;
    }
    return (0);
}

/*
 * dbl_bufp - set the buffering parameters.  Only meaningful before a call
 * 	to dbl_setup.
 */

int dbl_bufp (bsize, nbuf, tpsize)
long bsize;
int nbuf;
long tpsize;
{
    if (nbuf > MAXNBUF) {
	return (1);
    }
    if (bsize != 0) {
	vars.bsize = bsize;
	}
    if (nbuf != 0 && nbuf < (MAXNBUF-1)) {
	/* -1 because of volume shm seg */
	vars.nbuf = nbuf;
    }
    if (tpsize != 0) {
	vars.tpblk = tpsize;
    }
    return (0);
}

/*
 * dbl_done - call when done with reading or writing.
 */

void dbl_done ()
{
    register int i;

    signal(SIGCLD, SIG_DFL);
    if (vars.cpid > 0)
	kill(vars.cpid, SIGTERM);
    vars.cpid = 0;
    signal(SIGCLD, savesig);
    /* this is used only to cleanup if we failed part way through the shmem
     * allocation code.  Olson */
    for (i = 0; i < MAXNBUF; i++) {
	if(vars.buf[i])
	    if(shmdt (vars.buf[i]))
		fprintf(stderr, "Unable to detach shared memory segment at %#x: %s\n",
		    vars.buf[i], strerror(errno));
	    else
		vars.buf[i] = 0;
	if(vars.shm[i]) {
	    if(shmctl (vars.shm[i], IPC_RMID, 0) == -1) 
		fprintf(stderr, "Unable to remove shared memory segment %d: %s\n",
		    vars.shm[i], strerror(errno));
		vars.shm[i] = 0;
	}
    }
    if (vars.msg != -1) {
	msgctl (vars.msg, IPC_RMID, 0);
	vars.msg = -1;
    }
#ifdef COPY
    if (vars.copybuf != 0) {
	free(vars.copybuf);
	vars.copybuf = 0;
    }
#endif
}

/*
 * dbl_setup - initiate the double buffering.  If called with '0' for 
 *	'dodb', then no true double buffering is done (i.e., no
 *	other process is created).  If called with a '1' for the
 *	parameter, true double buffering is initiated.  This is useful
 *	for maintaining a single interface while retaining seekable
 *	aspects of random-access files.
 */

static int dbli_defouterr (err, ndes)
int err;
int *ndes;
{
    vperror ("dblib[%d]:", err);
    return (1);
}

static int dbli_definerr (err, ndes)
int err;
int *ndes;
{
    vperror ("dblib[%d]:", err);
    return (1);
}

static void dbli_catcher (x)
int x;
{
    longjmp (vars.errjump, RV_SIG);
}

static void dbli_intclean ()
{
    if (vars.inout)
        signal (SIGINT, dbli_intclean);
    else
        longjmp (vars.errjump, RV_SIG);
}

static void
deadone()
{
int	status;
int	pid;

	if ((pid = wait(&status)) == -1)
		return;
	if (pid != vars.cpid) {
		/*
		fprintf(stderr, "unknown child %d died, stat=%#x\n", pid,
			status);
		*/
		signal(SIGCLD, deadone);
	}
	else {
		if (*vars.rval == 0) {
			if (!vars.deathok) {
				if (status != 0)
					fprintf(stderr,
					"double buffer slave died, stat=%#x\n",
						status);
			}
		}
		else if (*vars.rval == RV_SYSERR || *vars.rval == RV_NOMEM)
			fprintf(stderr, "double buffer slave error %d\n",
				*vars.rval);
		signal(SIGCLD, savesig);
	}
	return;
}

int dbl_setup (des, dodb, inout)
int des;
int dodb;
int inout;
{
    int dbli_defouterr ();
    int dbli_definerr ();
    void dbli_catcher ();
    void dbli_intclean ();
    int read ();
    int write ();
	static firstinit = 1;	/* easier than initializing vars struct... */

    /* 
     * Reset back in case this is a multiple call.
	 * But, only if we have ever been called before;
	 * otherwise we remove the msgid 0, which breaks
	 * other programs on occasion...
     */

	if(!firstinit)
		dbl_done ();
    vars.des = des;
    vars.deathok = 0;
    vars.cpid = -1;
	vars.msg = -1;

    /* 
     * If in transparent mode, indicate such.
     */

    if (!dodb) {
	vars.dodb = 0;
	return (0);
    }
	firstinit = 0;
    vars.dodb = 1;

    /* 
     * Initialize all the parameters from the caller supplied
     * info.
     */

    if (vars.nbuf == 0) {
	vars.nbuf = DEFNBUF;
    }
    if (vars.tpblk == 0) {
	vars.tpblk = DEFTPBLK;
    }
    if (vars.bsize == 0) {
	vars.bsize = DEFBSIZE;
	}
    /* insure buffer is integral number of tape blocks in size, but must be
	 * non-zero! */
	if(vars.bsize > vars.tpblk)
		vars.bsize = (vars.bsize / vars.tpblk) * vars.tpblk;
	else
		vars.bsize = vars.tpblk;
#if DEBUG
    fprintf (stderr, "dblib: using %d buffers of %d bytes, tape block %d\n",
	     vars.nbuf, vars.bsize, vars.tpblk);
#endif

    if (vars.usread == 0) {
	vars.usread = read;
    }
    if (vars.uswrite == 0) {
	vars.uswrite = write;
    }
    /* 
     * Set up the buffers and message queue.
     */
    if (dbli_setupbuf ()) {
	dbl_done ();
	vars.bsize = 0;
	return (1);
    }
    *vars.rval = 0;
    vars.inout = inout;
    vars.nomore = 0;
    vars.ncache = 0;
    vars.issrpc = 0;
    vars.chendseen = 0;
    if (vars.inerr == 0) {
	vars.inerr = dbli_definerr;
    }
    if (vars.outerr == 0) {
	vars.outerr = dbli_defouterr;
    }
    /* 
     * Start up the child, who will actually deal with the
     * device/file.
     */
    savesig = signal(SIGCLD, deadone);
    if ((vars.cpid = fork ()) == 0) {
	signal (SIGTERM, dbli_catcher);
	signal (SIGQUIT, dbli_catcher);
	signal (SIGINT, dbli_intclean);
	if (!(*vars.rval = setjmp (vars.errjump))) {
	    if (inout) {
		*vars.rval = dbli_copyout ();
	    } else {
		*vars.rval = dbli_copyin ();
		trashmsg ();
	    }
	}
	if (*vars.rval == RV_SYSERR && errno == EINTR)
		*vars.rval = RV_SIG;
	close (vars.des);
	dbl_done();
	exit (0);
    } else if (vars.cpid == -1) {
	fprintf (stderr, "unable to fork, err=%d\n", errno);
	if(vars.msg != -1) {
		msgctl (vars.msg, IPC_RMID, 0);
		vars.msg = -1;
	}
	return (1);
    }
    if (inout) {
	close (vars.des);
    }
    return (0);
}

/*
 * dbl_read - read the given amount of data from the other side.
 */

long dbl_read (buf, size)
char *buf;
long size;
{
    long cnt;
    long left;
    static int lastbuf;
    static char *lastpos;
    static long lastsize = 0;
    int rval;

    /* 
     * If transparent, just go read the data.
     */
    if (!vars.dodb) {
	return (dbli_localread (buf, size));
    }
    /* 
     * Otherwise, then hang out sucking up data from the other side
     * until the request is filled.
     */
    if (vars.nomore) {
	if (vars.nomore == 1)
	    vars.nomore++;
	else
	    errno = EIO;
	return (0);
    }
    if (setjmp (vars.errjump)) {
	return (-1);
    }
    cnt = 0;
    while (cnt < size) {
	if (lastsize == 0) {
	    lastbuf = getfullbuf ();
	    if (lastbuf < 0 || !(lastsize = *(vars.len[lastbuf]))) {
		/* 
		 * This only happens when the end of
		 * everything has been reached by the child.
		 */
		vars.nomore++;
		return (cnt);
	    }
	    lastpos = vars.buf[lastbuf];
	}
	left = size - cnt;
	left = (left > lastsize ? lastsize : left);
	memcpy (buf + cnt, lastpos, left);
	cnt += left;
	lastsize -= left;
	lastpos += left;
	if (lastsize == 0) {
	    freebuf (lastbuf);
	}
    }
    return (size);
}

/*
 * Write state that needs to be shared with the flush routine.
 */

static char *lastpos;
static long lastsize = 0;
static int lastbuf;

/*
 * dbli_wflush - automatic flush generated by sending an RPC message
 *	to the child while writing.
 */

static int dbli_wflush ()
{
    if (lastsize != 0) {
	*(vars.len[lastbuf]) = vars.bsize - lastsize;
	fullbuf (lastbuf);
	lastsize = 0;
    }
}

/*
 * dbl_flush - flush any still queued data out to the other side.
 *	We expect the other side to go away after this, so we wait
 *	for him to die.
 */

int dbl_flush ()
{
    int sbuf;

    if (vars.dodb && vars.cpid != 0 && vars.inout) {
	if (vars.msg == -1 || setjmp (vars.errjump)) {
	    return (0);
	}
	dbli_wflush ();
	lastbuf = getfreebuf ();
	*(vars.len[lastbuf]) = 0;
	fullbuf (lastbuf);
	while (wait (&vars.status) == -1) {
	    if (errno != EINTR) {
		fprintf (stderr, "no dbl child to reap!\n");
		msgctl (vars.msg, IPC_RMID, 0);
		vars.msg = -1;
		return (0);
	    }
	}
	if (*(vars.rval) != RV_OK && *(vars.rval) != RV_EOF) {
	    fprintf (stderr, "dbl child error %d\n", *vars.rval);
	}
	msgctl (vars.msg, IPC_RMID, 0);
	vars.msg = -1;
    }
    return (0);
}

/*
 * dbl_write - write the given amount of data out.
 */

int dbl_write (buf, size)
char *buf;
long size;
{
    long cnt;
    long left;

    /* 
     * If in transparent mode, just write the data.
     */

    if (!vars.dodb) {
	return (dbli_localwrite (buf, size));
    }
    /* 
     * Otherwise, write by filling up buffers and sending them
     * down the pike as soon as filled.
     */
    if (setjmp (vars.errjump)) {
	return (-1);
    }
    cnt = 0;
    while (cnt < size) {
	if (lastsize == 0) {
	    lastbuf = getfreebuf ();
	    lastpos = vars.buf[lastbuf];
	    lastsize = vars.bsize;
	}
	left = size - cnt;
	left = (left > lastsize ? lastsize : left);
	memcpy (lastpos, buf + cnt, left);
	lastpos += left;
	cnt += left;
	lastsize -= left;
	if (lastsize == 0) {
	    *(vars.len[lastbuf]) = vars.bsize;
	    fullbuf (lastbuf);
	}
    }
    return (size);
}

/*
 * Write-through routines for direct files.
 */

static long dbli_localwrite (buf, size)
char *buf;
long size;
{
    patch_buffer_blk((union blk *)buf);
    return((*vars.uswrite) (vars.des, buf, size));
}

static long dbli_localread (buf, size)
char *buf;
long size;
{
    return((*vars.usread) (vars.des, buf, size));
}

/*
 * Internal recovery routines for implementing the correct behaviour.
 * Basically, on writes the child is responsible for switching volumes
 * and keeping everything sane.  On reads, the double buffering goes 
 * away on each switch and the parent is responsible for switching
 * volumes and keeping everything sane.
 */

static int dbli_insync (nb, err)
int nb;
int err;
{
    static void chend ();

    chend (nb, err);
}

static int dbli_outsync (nb, err)
int nb;
int err;
{
    if ((*vars.inerr) (nb, err, &vars.des)) {
	return (1);
    }
    return (0);
}

/*
 * dbli_copyin, dbli_copyout - the routines run by the child for actually
 *	reading or writing the data.
 */

static int dbli_copyin ()
{
    register long cnt;
    register long nb;
    register int i;
    register int saverr;
    int rval;
    int child_err = 0;
#ifdef COPY
    char *cbuf;
#endif

#ifdef COPY
    cbuf = vars.copybuf;
#endif
    while (1) {
	i = getfreebuf ();
	cnt = 0;
	while (cnt < vars.bsize) {
	    /* 
	     * See if we can read some data.
	     */
#ifndef COPY
	    if ((nb = (*vars.usread) (vars.des, vars.buf[i] + cnt, vars.tpblk)) == -1)
#else
	    if ((nb = (*vars.usread) (vars.des, cbuf+cnt, vars.tpblk)) == -1)
#endif
	    {
		child_err = 1;
		break;
	    } else if (nb == 0) {
		goto fail;
	    } else if (nb != vars.tpblk) {
		goto fail;
	    }
	    cnt += nb;
	}
	if(child_err) {
	    child_err = 0;
    	    saverr = errno;
    	    dbli_cherr (saverr);
	} else {
#ifdef COPY
            memcpy (vars.buf[i], cbuf, cnt);
#endif
	    /* 
	     * Release the buffer.
	     */
	    *(vars.len[i]) = cnt;
	    fullbuf (i);
	}
    }
fail: 
    saverr = errno;
    if (cnt > 0) {
	/* 
	 * Flush the buffer.
	 */
#ifdef COPY
        memcpy (vars.buf[i], cbuf, cnt);
#endif
	*(vars.len[i]) = cnt;
	fullbuf (i);
    }
    dbli_insync (nb, saverr);
    return (RV_RFAIL);
}

static int dbli_copyout ()
{
    register long out;
    register long cnt;
    register long nb;
    register int i;
#ifdef COPY
    char *cbuf;
#endif

#ifdef COPY
    cbuf = vars.copybuf;
#endif
    while (1) {
	i = getfullbuf ();
	if (i < 0) {
	    return (RV_WFAIL);
	}
	if ((out = *(vars.len[i])) == 0) {
	    /* 
	     * Returing here causes the child to dissappear.
	     */
	    freebuf (i);
	    return (RV_OK);
	}
	cnt = 0;
#ifdef COPY
	memcpy (cbuf, vars.buf[i], out);
	freebuf(i);
#endif
	while (out > 0) {
		/* the volume # and chksum have to be done just before
		we actually write the archive, or we can get out of sync
		since the reader can fill the buffer etc. while we are
		hitting end of media with the other buffer. */
#ifdef COPY
	    patch_buffer_blk((union blk *)(cbuf+cnt));
	    if ((nb = (*vars.uswrite) (vars.des, cbuf+cnt, vars.tpblk)) == -1)
#else
	    patch_buffer_blk((union blk *)(vars.buf[i]+cnt));
	    if ((nb = (*vars.uswrite) (vars.des, vars.buf[i] + cnt, vars.tpblk)) == -1)
#endif
	    {
		qeom: 
		if (errno == EINTR)
		    continue;
		/* 
		 * See if recovery is possible.
		 */
		if (dbli_outsync (0, errno)) {
		    /* 
		     * Returning here causes the child to
		     * disappear.
		     */
		    freebuf (i);
		    return (RV_WFAIL);
		}
		nb = 0;
	    } else if (nb == 0) {
		/* 
		 * "shouldn't happen"
		 */
		errno = 0;
		goto qeom;
	    } else if (nb != vars.tpblk) {
		/* 
		 * "shouldn't happen"
		 */
		if (dbli_outsync (0, errno)) {
		    freebuf (i);
		    return (RV_WFAIL);
		}
	    }
	    cnt += nb;
	    out -= nb;
	}
#ifndef COPY
	freebuf (i);
#endif
    }
}

/*
 * This routine goes through the rather gross setup needed to create the
 * buffers and message queue.
 */

#ifdef COPY
#define	ROOM	((4*vars.bsize)+0x100000)
#endif

static int dbli_setupbuf ()
{
    int i, bytes;
    extern int *volume;
    
#ifdef COPY
    /*
     * Some systems use a particularly bad selection algorithm for
     * picking where to put shared segments. In particular, they
     * just add some 'generic' amount to the last break value and
     * attach there (assuming you won't need to allocate more than the
     * space left).  To avoid this problem, move our break value up
     * at least far enough for the copy buffer, and leave some slop
     * for our caller.
     */
    sbrk(ROOM);
#endif
    if ((vars.msg = msgget (IPC_PRIVATE, IPC_CREAT | 0660)) == -1) {
	fprintf (stderr, "unable to allocate message queue, err=%d\n", errno);
	return (1);
    }

    bytes = vars.bsize + sizeof(long);
    /* the cleanup code will handle any stuff we have allocated if we have
     * a failure part way through */
    for (i = 0; i < vars.nbuf; i++) {
	int nbytes = bytes + (i ? 0 : sizeof(long));
	if ((vars.shm[i] = shmget (IPC_PRIVATE, nbytes, IPC_CREAT | 0600)) == -1) {
	    fprintf (stderr, "unable to allocate %d bytes, err=%d\n",
		     nbytes, errno);
	    vars.shm[i] = 0;
	    return (1);
	}
	if ((vars.buf[i] = (char *) shmat (vars.shm[i], 0, 0)) == (char *) - 1) {
	    fprintf (stderr, "unable to attach buffer segment, err=%d\n", errno);
	    return (1);
	}
	vars.len[i] = (long *) (vars.buf[i] + vars.bsize);
	/* we remove these so no one else can attach, and also in case we never
	 * get to the cleanup code. */
	if(shmctl (vars.shm[i], IPC_RMID, 0) == -1)  /* but continue anyway */
	    fprintf(stderr, "Unable to remove shared memory segment %d: %s\n",
		vars.shm[i], strerror(errno));
	else
	    vars.shm[i] = 0;	/* nothing to cleanup */
	freebuf (i);
    }
    vars.rval = (long *) (vars.buf[0] + vars.bsize + sizeof (long));
#ifdef COPY
    sbrk(-ROOM);
    if ((vars.copybuf = malloc(vars.bsize)) == 0) {
	    fprintf(stderr, "unable to allocate %d bytes, err=%d\n",
		    vars.bsize, errno);
	    return(1);
    }
#endif
    /* allocate the shared memory for the volume # */
    i = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0600);
    if(i == -1) {
	fprintf (stderr, "unable to allocate %d bytes, err=%d\n",
		 sizeof(int), errno);
	return 1;
    }
    vars.shm[vars.nbuf+1] = i;	/* for cleanup code */
    if ((volume = (int *) shmat (i, 0, 0)) == (int *) -1) {
	fprintf (stderr, "unable to attach buffer segment, err=%d\n", errno);
	return (1);
    }
    if(shmctl (i, IPC_RMID, 0) == -1)  /* but continue anyway */
	fprintf(stderr, "Unable to remove shared memory segment %d: %s\n",
	    i, strerror(errno));
	else
	    vars.shm[vars.nbuf+1] = 0;	/* nothing to cleanup */
    return (0);
}

/*
 * The following routines deal with the message queue, and implement the
 * buffer management protocol.  At initialization, messages for the number
 * of available buffers are placed on the queue as empty buffers.  The
 * reader simply blocks waiting for one of these messages.  When one arrives,
 * he fills the buffer and then sends a buffer full message.  The writer
 * blocks on the queue waiting for buffer full messages.  As he gets each
 * one, he writes the buffer and then enters it back on the queue as empty.
 * Thus, once started, the management is self-sustaining.  A criticial note:
 * this code relies on the message queue staying ordered.
 */

#define MSGSZ		(sizeof (pbuf) - sizeof (long))
#define DOBUF		1
#define FREEBUF		3
#define CALLDONE	4

#define CALLPROC	10
#define CHEND		11		/* child end */
#define CHERR		12		/* child error */

static int MSGSND (mdes, pbuf, sz, flags)
int mdes;
bufmsg_t *pbuf;
int sz;
int flags;
{
#if DEBUG
    fprintf (stderr, "%s: send msg type %d, action %d\n",
	     (vars.cpid ? "parent" : "child"), pbuf -> m_type,
	     pbuf -> m_action);
#endif
	while (msgsnd(mdes, (struct msgbuf *)pbuf, sz, flags) == -1) {
		if (errno == EINTR)
			continue;
		else
			return(-1);
	}
	return(0);
}

static int MSGRCV (mdes, pbuf, sz, type, flags)
int mdes;
bufmsg_t *pbuf;
int sz;
int type;
int flags;
{
#if DEBUG
    fprintf (stderr, "%s: waiting on msg type %d\n",
	     (vars.cpid ? "parent" : "child"), type);
#endif
	while (msgrcv(mdes, (struct msgbuf *)pbuf, sz, type, flags) == -1) {
		if (errno == EINTR)
			continue;
		else
			return(-1);
	}
#if DEBUG
    fprintf (stderr, "%s: rcv msg type %d, action %d\n",
	     (vars.cpid ? "parent" : "child"), pbuf -> m_type,
	     pbuf -> m_action);
#endif
    return (0);
}

/*
 * trashmsg - receive all messages on the queue intended for us
 *	and throw them away - if the parent queued an RPC call,
 *	then this insures he wakes up.
 */

trashmsg ()
{
    bufmsg_t pbuf;
    
    while (msgrcv(vars.msg,(struct msgbuf*)&pbuf,MSGSZ,FREEBUF,IPC_NOWAIT)!=-1){
	switch (pbuf.m_action) {
	    /* do nothing */
	}
    }
    pbuf.m_type = CALLDONE;
    MSGSND (vars.msg, pbuf, MSGSZ, 0);
}

/*
 * External routine to allow the parent in a write situation to
 * cause an action in the child (after all the data has been written,
 * of course).
 */

#define GONE()	if (*(vars.rval)!=0){wait(&vars.status);return(0);}

int dbl_rpcdown (func, arg)
void (*func) ();
long arg;
{
    bufmsg_t pbuf;
    
    GONE ();
    dbli_wflush ();
    if (vars.inout) {
	pbuf.m_type = DOBUF;
    } else {
	pbuf.m_type = FREEBUF;
    }
    pbuf.m_action = CALLPROC;
    pbuf.m_proc = func;
    pbuf.m_arg = arg;
    GONE ();
    vars.deathok = 1;
    if (MSGSND (vars.msg, &pbuf, MSGSZ, 0) == -1) {
	return (-1);
    }
    GONE ();
    while (MSGRCV (vars.msg, &pbuf, MSGSZ, CALLDONE, IPC_NOWAIT) == -1) {
	if (errno == ENOMSG) {
	    GONE ();
	    sleep (1);
	} else {
	    return (-1);
	}
    }
    vars.deathok = 0;
    return (0);
}

/*
 * chend - send a child end message to the other side.
 */

static void chend (nb, err)
int nb;
int err;
{
    bufmsg_t pbuf;
    
    pbuf.m_type = DOBUF;
    pbuf.m_action = CHEND;
    if (MSGSND (vars.msg, &pbuf, MSGSZ, 0) == -1) {
	longjmp (vars.errjump, RV_SYSERR);
    }
}

/*
 * fullbuf - release a full buffer for the other side.
 */

static int fullbuf (sp)
int sp;
{
    bufmsg_t pbuf;
    
    pbuf.m_type = pbuf.m_action = DOBUF;
    pbuf.m_buf = sp;
    if (MSGSND (vars.msg, &pbuf, MSGSZ, 0) == -1) {
	longjmp (vars.errjump, RV_SYSERR);
    }
}

/*
 * freebuf - release a now empty buffer for the other side.
 */

static int freebuf (sp)
int sp;
{
    bufmsg_t pbuf;
    
    pbuf.m_type = pbuf.m_action = FREEBUF;
    pbuf.m_buf = sp;
    if (MSGSND (vars.msg, &pbuf, MSGSZ, 0) == -1) {
	longjmp (vars.errjump, RV_SYSERR);
    }
}

/*
 * getcache - get the next buffer from the cache, and execute any
 *	queued procedure calls.
 */

static int getcache ()
{
    if (vars.ncache > vars.curcache) {
#if DEBUG
	fprintf (stderr,
		 "gfb: return buffer %d from cache, %d bytes\n",
		 vars.bufcache[vars.curcache],
		 *(vars.len[vars.bufcache[vars.curcache]]));
#endif
	return (vars.bufcache[vars.curcache++]);
    }
    if (vars.issrpc) {
	vars.issrpc = 0;
	if( (vars.srpc.m_action == CHEND) || (vars.srpc.m_action == CHERR) ) {
	    errno = vars.srpc.m_err;
	    return (-2);
	} else {
	    dorpc (&vars.srpc);
	}
    }
    return (-1);
}

/*
 * dorpc - call the specified procedure in the child.
 */

static int dorpc (pbuf)
bufmsg_t *pbuf;
{
#if DEBUG
    fprintf (stderr, "gfb: RPC call\n");
#endif
    (*pbuf -> m_proc) (pbuf -> m_arg, &vars.des);
    switch (pbuf -> m_action) {
	case CALLPROC: 
	    pbuf -> m_type = CALLDONE;
	    break;
    }
    if (MSGSND (vars.msg, pbuf, MSGSZ, 0) == -1) {
	longjmp (vars.errjump, RV_SYSERR);
    }
}

/*
 * getfullbuf - get a buffer full of data.
 */

getfullbuf ()
{
    bufmsg_t pbuf;
    int half = vars.nbuf / 2;
    int rval;
    
    /* 
     * This routine attempts to add some hysterisis to the output,
     * to help insure maximum overlap between input and output.
     * This is done by not starting to write buffers until at least
     * half are full, which helps if the reader is slower than the
     * writer, making the output flow stop less often.  Otherwise,
     * the writer could just chase the reader around the buffer
     * list, having to wait on each new buffer.
     */
    if ((rval = getcache ()) >= 0) {
	return (rval);
    } else if (rval == -2) {
	return (-1);
    }
    vars.ncache = 0;
    vars.curcache = 0;
    while (vars.ncache < half) {
	if (vars.chendseen) {
	    return (-1);
	}
	if (MSGRCV (vars.msg, &pbuf, MSGSZ, DOBUF, 0) == -1) {
	    longjmp (vars.errjump, RV_SYSERR);
	}
	switch (pbuf.m_action) {
	    case CHERR: 
	    case CHEND: 
		if (vars.ncache == 0) {
		    errno = pbuf.m_err;
		    return (-1);
		} else {
#if DEBUG
		    fprintf (stderr, "gfb: cached CHEND\n", vars.ncache);
#endif
		    vars.issrpc = 1;
		    vars.srpc = pbuf;
		    goto out;
		}
		break;
	    case CALLPROC: 
		if (vars.ncache == 0) {
		    dorpc (&pbuf);
		    continue;
		} else {
#if DEBUG
		    fprintf (stderr, "gfb: cached RPC\n", vars.ncache);
#endif
		    vars.issrpc = 1;
		    vars.srpc = pbuf;
		    goto out;
		}
		break;
	    case DOBUF: 
		vars.bufcache[vars.ncache] = pbuf.m_buf;
#if DEBUG
		fprintf (stderr, "gfb: cacheing buffer %d len %d\n",
			 pbuf.m_buf, *(vars.len[pbuf.m_buf]));
#endif
		vars.ncache++;
		if (*(vars.len[pbuf.m_buf]) == 0) {
		    goto out;
		}
		break;
	}
    }
    out: 
    return (getcache ());
}

/*
 * getfreebuf - get an empty buffer to fill with data.
 */

getfreebuf ()
{
    bufmsg_t pbuf;
    
    another: 
    if (*(vars.rval) != 0) {
	wait (&vars.status);
	longjmp (vars.errjump, *(vars.rval));
    }
    if (MSGRCV (vars.msg, &pbuf, MSGSZ, FREEBUF, 0) == -1) {
	longjmp (vars.errjump, RV_SYSERR);
    }
    switch (pbuf.m_action) {
	case FREEBUF: 
	    return (pbuf.m_buf);
	case CALLPROC: 
	    dorpc (&pbuf);
	    goto another;
    }
    longjmp (vars.errjump, RV_SYSERR);
}

/*
 * Various utility routines.
 */

/*
 * vperror - call the system 'perror' routine, but do at as
 *	a formatted call, similar to printf.
 */

void vperror (char *format, ...)
{
    va_list args;
    char vpbuf[BUFSIZ];
    
    va_start (args, format);
    vsprintf (vpbuf, format, args);
    perror (vpbuf);
    va_end (args);
}

/*
 * dbli_cherr - send a child error message to the other side.
 */

static int dbli_cherr (err)
int err;
{
    bufmsg_t pbuf;
    
    pbuf.m_type = DOBUF;
    pbuf.m_action = CHERR;
    if (MSGSND (vars.msg, &pbuf, MSGSZ, 0) == -1) {
	longjmp (vars.errjump, RV_SYSERR);
    }
}

#endif	/* HAVE_SHM */
