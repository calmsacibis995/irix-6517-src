/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uucp:eio.c	2.9"

#include "uucp.h"

#ifdef ATTSV
#define     MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifdef sgi
#define MAXTCPTIME 300			/* TCP can sometimes be slooow */
#endif
static jmp_buf Failbuf;

static int erdblk(char *, int, int);


#define	EBUFSIZ	1024	/* "e" protocol buffer size */

/*	This constant MUST be the same on all machines running "e"
	uucico protocol!  It must remain this value forever!
*/
#define	CMSGSIZ	20	/* size of the initial file size message */

/*
 * error-free channel protocol
 */
static void
ealarm() {
	longjmp(Failbuf, 1);
}
static void (*esig)();

/*
 * turn on protocol timer
 */
eturnon()
{
	esig=signal(SIGALRM, ealarm);
	return(0);
}

eturnoff()
{
	signal(SIGALRM, esig);
	return(0);
}

/*
 * write message across link
 *	type	-> message type
 *	str	-> message body (ascii string)
 *	fn	-> link file descriptor
 * return
 *	FAIL	-> write failed
 *	0	-> write succeeded
 */
ewrmsg(char type, char *str, int fn)
{
	register char *s;
	char bufr[EBUFSIZ];
	int	s1, s2;

	bufr[0] = type;
	s = &bufr[1];
	while (*str)
		*s++ = *str++;
	*s = '\0';
	if (*(--s) == '\n')
		*s = '\0';
	s1 = strlen(bufr) + 1;
	if (setjmp(Failbuf)) {
		DEBUG0(7, "ewrmsg write failed\n");
		return(FAIL);
	}
#ifdef sgi
	alarm(MAXTCPTIME);
#else
	alarm(60);
#endif
	s2 = (*Write)(fn, bufr, (unsigned) s1);
	alarm(0);
	if (s1 != s2)
		return(FAIL);
	return(0);
}

/*
 * read message from link
 *	str	-> message buffer
 *	fn	-> file descriptor
 * return
 *	FAIL	-> read timed out
 *	0	-> ok message in str
 */
erdmsg(str, fn)
register char *str;
{
	register int i;
	register int len;

	if(setjmp(Failbuf)) {
		DEBUG0(7, "erdmsg read failed\n");
		return(FAIL);
	}

	i = EBUFSIZ;
	for (;;) {
#ifdef sgi
		alarm(MAXTCPTIME);
#else
		alarm(60);
#endif
		len = (*Read)(fn, str, i);
		alarm(0);
		if (len <= 0) return(FAIL);
		str += len; i -= len;
		if (*(str - 1) == '\0')
			break;
	}
	return(0);
}

/*
 * read data from file fp1 and write
 * on link
 *	fp1	-> file descriptor
 *	fn	-> link descriptor
 * returns:
 *	FAIL	->failure in link
 *	0	-> ok
 */
ewrdata(fp1, fn)
register FILE *fp1;
int	fn;
{
	register int ret;
	int	fd1;
	int len;
	off_t bytes;
	char bufr[EBUFSIZ];
#ifndef sgi
	char text[EBUFSIZ];
#endif
	time_t	ticks;
#ifndef sgi
	int	mil;
#endif
	struct stat	statbuf;
	off_t	msglen;
	char	cmsglen[CMSGSIZ];

	if (setjmp(Failbuf)) {
		DEBUG0(7, "ewrdata failed\n");
		return(FAIL);
	}
	bytes = 0L;
	fd1 = fileno(fp1);
	fstat(fd1, &statbuf);
	bytes = msglen = statbuf.st_size;
	(void) millitick();
#ifdef sgi
	bzero(cmsglen,sizeof(cmsglen));
#endif
	sprintf(cmsglen, "%ld", (long) msglen);
#ifdef sgi
	alarm(MAXTCPTIME);
#else
	alarm(60);
#endif
	ret = (*Write)(fn, cmsglen, sizeof(cmsglen));
	if (ret != sizeof(cmsglen))
		return(FAIL);
	DEBUG(7, "ewrdata write %lld\n", statbuf.st_size);
	while ((len = read( fd1, bufr, EBUFSIZ )) > 0) {
#ifdef sgi
		alarm(MAXTCPTIME);
#else
		alarm(60);
#endif
		ret = (*Write)(fn, bufr, (unsigned) len);
		alarm(0);
#ifdef sgi
		DEBUG(7, "ewrdata ret %d\n", ret);
#else
		DEBUG(7, "ewrmsg ret %d\n", ret);
#endif
		if (ret != len)
			return(FAIL);
		if ((msglen -= len) <= 0)
			break;
	}
	if (len < 0 || (len == 0 && msglen != 0)) return(FAIL);
	ticks = millitick();
	statlog( "->", (long)bytes, ticks );
	return(0);
}

/*
 * read data from link and
 * write into file
 *	fp2	-> file descriptor
 *	fn	-> link descriptor
 * returns:
 *	0	-> ok
 *	FAIL	-> failure on link
 */
erddata(fn, fp2)
register FILE *fp2;
{
	register int len;
	int	fd2;
	long bytes;
#ifndef sgi
	char text[EBUFSIZ];
#endif
	char bufr[EBUFSIZ];
	time_t	ticks;
#ifndef sgi
	int	mil;
#endif
	long	msglen;
	char	cmsglen[CMSGSIZ];
	int	writefile = TRUE, ret = SUCCESS;

#ifndef sgi
	bytes = 0L;
#endif
	(void) millitick();
	len = erdblk(cmsglen, sizeof(cmsglen), fn);
	if (len < 0) return(FAIL);
	sscanf(cmsglen, "%ld", &msglen);
	DEBUG(7, "erdblk msglen %d\n", msglen);
	if ( ((msglen-1)/512 +1) > Ulimit ) {
		ret = EFBIG;
		writefile = FALSE;
	}
	bytes = msglen;
	fd2 = fileno( fp2 );
	for (;;) {
		len = erdblk(bufr, MIN(msglen, EBUFSIZ), fn);
		DEBUG(7, "erdblk ret %d\n", len);
		if (len < 0) {
			DEBUG0(7, "erdblk failed\n");
			return(FAIL);
		}
		if ((msglen -= len) < 0) {
			DEBUG0(7, "erdblk read too much\n");
			return(FAIL);
		}
		/* this write is to file -- use write(2), not (*Write) */
		if ( writefile == TRUE && write( fd2, bufr, len ) != len ) {
			ret = errno;
			DEBUG(7, "erddata: write to file failed, errno %d\n", ret);
			writefile = FALSE;
		}
		if (msglen == 0)
			break;
	}
	if ( writefile == TRUE ) {
		ticks = millitick();
		statlog( "<-", bytes, ticks );
		return(SUCCESS);
	}
	else
		return(ret);
}

/*
 * read block from link
 * reads are timed
 *	blk	-> address of buffer
 *	len	-> size to read
 *	fn	-> link descriptor
 * returns:
 *	FAIL	-> link error timeout on link
 *	i	-> # of bytes read
 */
static int
erdblk(blk, len,  fn)
register char *blk;
{
	register int i, ret;

	if(setjmp(Failbuf)) {
		DEBUG0(7, "erdblk timeout\n");
		return(FAIL);
	}

	for (i = 0; i < len; i += ret) {
#ifdef sgi
		alarm(MAXTCPTIME);
#else
		alarm(60);
#endif
		DEBUG(7, "ask %d ", len - i);
		if ((ret = (*Read)(fn, blk, (unsigned) len - i)) <= 0) {
			alarm(0);
			DEBUG0(7, "read failed\n");
			return(FAIL);
		}
		DEBUG(7, "got %d\n", ret);
		blk += ret;
		if (ret == 0)
			break;
	}
	alarm(0);
	return(i);
}
