/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char sccsid[] = "@(#)tftpsubs.c	1.2 (Berkeley) 2/7/86";
#endif /* not lint */

/* Simple minded read-ahead/write-behind subroutines for tftp user and
   server.  Written originally with multiple buffers in mind, but current
   implementation has two buffer logic wired in.

   Todo:  add some sort of final error check so when the write-buffer
   is finally flushed, the caller can detect if the disk filled up
   (or had an i/o error) and return a nak to the other side.

			Jim Guyton 10/85
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/tftp.h>
#include <stdio.h>
#ifdef sgi
#include <errno.h>
#endif

#define PKTSIZE SEGSIZE+4       /* should be moved to tftp.h */

struct bf {
	int counter;            /* size of data in buffer, or flag */
#ifdef sgi
	int errno;		/* error if counter == BF_ERROR */
#endif
	char buf[PKTSIZE];      /* room for data packet */
} bfs[2];

				/* Values for bf.counter  */
#define BF_ALLOC -3             /* alloc'd but not yet filled */
#define BF_FREE  -2             /* free */
#ifdef sgi
#define BF_ERROR -1             /* error occurred on this buffer */
#endif

/* [-1 .. SEGSIZE] = size of data in the data buffer */

static int nextone;     /* index of next buffer to use */
static int current;     /* index of buffer in use */

			/* control flags for crlf conversions */
int newline = 0;        /* fillbuf: in middle of newline expansion */
int prevchar = -1;      /* putbuf: previous char (cr check) */

struct tftphdr *rw_init();

struct tftphdr *w_init() { return rw_init(0); }         /* write-behind */
struct tftphdr *r_init() { return rw_init(1); }         /* read-ahead */

struct tftphdr *
rw_init(x)              /* init for either read-ahead or write-behind */
int x;                  /* zero for write-behind, one for read-head */
{
	newline = 0;            /* init crlf flag */
	prevchar = -1;
	bfs[0].counter =  BF_ALLOC;     /* pass out the first buffer */
	current = 0;
	bfs[1].counter = BF_FREE;
	nextone = x;                    /* ahead or behind? */
	return (struct tftphdr *)bfs[0].buf;
}


/* Have emptied current buffer by sending to net and getting ack.
   Free it and return next buffer filled with data.
 */
readit(file, dpp, convert)
	FILE *file;                     /* file opened for read */
	struct tftphdr **dpp;
	int convert;                    /* if true, convert to ascii */
{
	struct bf *b;

	bfs[current].counter = BF_FREE; /* free old one */
	current = !current;             /* "incr" current */

	b = &bfs[current];              /* look at new buffer */
	if (b->counter == BF_FREE)      /* if it's empty */
		read_ahead(file, convert);      /* fill it */
/*      assert(b->counter != BF_FREE);  */ /* check */
	*dpp = (struct tftphdr *)b->buf;        /* set caller's ptr */
#ifdef sgi
	if (b->counter == BF_ERROR)
		errno = b->errno;
#endif
	return b->counter;
}

/*
 * fill the input buffer, doing ascii conversions if requested
 * conversions are  lf -> cr,lf  and cr -> cr, nul
 */
read_ahead(file, convert)
	FILE *file;                     /* file opened for read */
	int convert;                    /* if true, convert to ascii */
{
	register int i;
	register char *p;
	register int c;
	struct bf *b;
	struct tftphdr *dp;

	b = &bfs[nextone];              /* look at "next" buffer */
	if (b->counter != BF_FREE)      /* nop if not free */
		return 0;
	nextone = !nextone;             /* "incr" next buffer ptr */

	dp = (struct tftphdr *)b->buf;

	if (convert == 0) {
#ifdef sgi
		b->counter = read_buffered(fileno(file), dp->th_data, SEGSIZE);
		if (b->counter == BF_ERROR)
			b->errno = errno;
#else
		b->counter = read(fileno(file), dp->th_data, SEGSIZE);
#endif
		return 0;
	}

	p = dp->th_data;
	for (i = 0 ; i < SEGSIZE; i++) {
		if (newline) {
			if (prevchar == '\n')
				c = '\n';       /* lf to cr,lf */
			else    c = '\0';       /* cr to cr,nul */
			newline = 0;
		}
		else {
			c = getc(file);
			if (c == EOF) break;
			if (c == '\n' || c == '\r') {
				prevchar = c;
				c = '\r';
				newline = 1;
			}
		}
	       *p++ = c;
	}
	b->counter = (int)(p - dp->th_data);
	return 0;
}

/* Update count associated with the buffer, get new buffer
   from the queue.  Calls write_behind only if next buffer not
   available.
 */
writeit(file, dpp, ct, convert)
	FILE *file;
	struct tftphdr **dpp;
	int convert;
{
	bfs[current].counter = ct;      /* set size of data to write */
	current = !current;             /* switch to other buffer */
	if (bfs[current].counter != BF_FREE)     /* if not free */
		write_behind(file, convert);     /* flush it */
	bfs[current].counter = BF_ALLOC;        /* mark as alloc'd */
	*dpp =  (struct tftphdr *)bfs[current].buf;
	return ct;                      /* this is a lie of course */
}

/*
 * Output a buffer to a file, converting from netascii if requested.
 * CR,NUL -> CR  and CR,LF => LF.
 * Note spec is undefined if we get CR as last byte of file or a
 * CR followed by anything else.  In this case we leave it alone.
 */
write_behind(file, convert)
	FILE *file;
	int convert;
{
	char *buf;
	int count;
	register int ct;
	register char *p;
	register int c;                 /* current character */
	struct bf *b;
	struct tftphdr *dp;

	b = &bfs[nextone];
	if (b->counter < -1)            /* anything to flush? */
		return 0;               /* just nop if nothing to do */

	count = b->counter;             /* remember byte count */
	b->counter = BF_FREE;           /* reset flag */
	dp = (struct tftphdr *)b->buf;
	nextone = !nextone;             /* incr for next time */
	buf = dp->th_data;

	if (count <= 0) return -1;      /* nak logic? */

	if (convert == 0)
		return write(fileno(file), buf, count);

	p = buf;
	ct = count;
	while (ct--) {                  /* loop over the buffer */
	    c = *p++;                   /* pick up a character */
	    if (prevchar == '\r') {     /* if prev char was cr */
		if (c == '\n')          /* if have cr,lf then just */
		   fseek(file, -1, 1);  /* smash lf on top of the cr */
		else
		   if (c == '\0')       /* if have cr,nul then */
			goto skipit;    /* just skip over the putc */
		/* else just fall through and allow it */
	    }
	    putc(c, file);
skipit:
	    prevchar = c;
	}
	return count;
}


/* When an error has occurred, it is possible that the two sides
 * are out of synch.  Ie: that what I think is the other side's
 * response to packet N is really their response to packet N-1.
 *
 * So, to try to prevent that, we flush all the input queued up
 * for us on the network connection on our host.
 *
 * We return the number of packets we flushed (mostly for reporting
 * when trace is active).
 */

int
synchnet(f)
int	f;		/* socket to flush */
{
	int i, j = 0;
	char rbuf[PKTSIZE];
	struct sockaddr_in from;
	int fromlen;

	while (1) {
		(void) ioctl(f, FIONREAD, &i);
		if (i) {
			j++;
			fromlen = sizeof from;
			(void) recvfrom(f, rbuf, sizeof (rbuf), 0,
				(caddr_t)&from, &fromlen);
		} else {
			return(j);
		}
	}
}

#ifdef sgi
/*
 * At the expense of burning more CPU time doing byte
 * moves, do IO in larger chunks to get better performance.
 * This extra buffering is especially helpful when reading
 * streaming tape drives across the network.  This is actually
 * done when doing cold network boots of machines that don't
 * have tape drives.
 *
 * Note that the actual IO size is carefully chosen to be
 * the LCM of the default buffer sizes that tar and cpio
 * use when writing to the cartridge tape.  This is because
 * of deficiencies in the streaming tape drivers that mean
 * you MUST read the tape with a buffer size that evenly
 * divides the buffer size with which it was originally
 * written.  Sigh...
 */
#define REAL_IO_SIZE	51200	/* LCM(500*512, 400*512) [See above comment] */
char *readbuf = NULL;
int rbrem;
char *rbcur;

#define MIN(x, y)	(((x) < (y)) ? (x) : (y))

int
read_buffered(fd, dp, size)
register int fd;
register char *dp;
register int size;
{
	register int cpsize;
	register int svsize = size;

	if (readbuf == NULL) {
		if ((readbuf = (char *) malloc(REAL_IO_SIZE)) == NULL) {
			fprintf(stderr, "can't malloc %d\n", REAL_IO_SIZE);
			errno = ENOMEM;
			return (BF_ERROR);
		}
		rbrem = 0;
		rbcur = readbuf;
	}

	while (size > 0) {
		/*
		 * Replenish buffer if necessary
		 */
		if (rbrem == 0) {
			rbrem = read(fd, readbuf, REAL_IO_SIZE);
			if (rbrem < 0)
				return (BF_ERROR);
			if (rbrem == 0)
				break;
			rbcur = readbuf;
		}
		cpsize = MIN(size, rbrem);
		bcopy(rbcur, dp, cpsize);
		size -= cpsize;
		dp += cpsize;
		rbcur += cpsize;
		rbrem -= cpsize;
	}
	
	return (svsize - size);
}
#endif
