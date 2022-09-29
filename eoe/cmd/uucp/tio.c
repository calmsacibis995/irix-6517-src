/*
 * Common 't' for "TCP" protocol
 */


#ident	"$Revision: 1.4 $"

#include "uucp.h"
#include <netinet/in.h>

int twrmsg(char, char*, int);
int trdmsg(char*, int);
int twrdata(FILE *, int fn);
int trddata(int, FILE*);
static int trdblk(char*, int, int);


#define MAXTCPTIME 300			/* TCP can sometimes be slooow */
static jmp_buf Failbuf;

#define TPACKSIZE	512
#define TBUFSIZE	1024

struct tbuf {
	long t_nbytes;
	char t_data[TBUFSIZE];
};


static void
talarm() {
	longjmp(Failbuf, 1);
}
static void (*tsig)();

/*
 * turn on protocol timer
 */
tturnon(void)
{
	tsig=signal(SIGALRM, talarm);
	return(0);
}

tturnoff(void)
{
	signal(SIGALRM, tsig);
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
int
twrmsg(char type,
       char *str,
       int fn)
{
	char bufr[TBUFSIZE+1];
	int	s1, i, s2;

	bufr[0] = type;
	strncpy(&bufr[1], str, TBUFSIZE);
	bufr[TBUFSIZE] = '\0';
	s1 = strlen(bufr) + 1;
	if (s1 > TBUFSIZE) {
		DEBUG0(7, "twrmsg babble\n");
		return(FAIL);
	}
	if (bufr[s1-2] == '\n') {
		bufr[s1-2] = '\0';
		s1--;
	}
	if ((i = s1 % TPACKSIZE)) {
		s1 += TPACKSIZE - i;
		bufr[s1 - 1] = '\0';
	}
	if (setjmp(Failbuf)) {
		DEBUG0(7, "twrmsg write failed\n");
		return(FAIL);
	}
	alarm(MAXTCPTIME);
	s2 = (*Write)(fn, bufr, (unsigned) s1);
	alarm(0);
	if (s1 != s2)
		return(FAIL);
	return(SUCCESS);
}

/*
 * read message from link
 *	str	-> message buffer
 *	fn	-> file descriptor
 * return
 *	FAIL	-> read timed out
 *	0	-> ok message in str
 */
int
trdmsg(char *str,
       int fn)
{
	register int i;
	register int len;

	if (setjmp(Failbuf)) {
		DEBUG0(7, "trdmsg read failed\n");
		return(FAIL);
	}

	i = 0;
	alarm(MAXTCPTIME);
	do {
		if (i >= TBUFSIZE-1) {
			DEBUG0(7, "trdmsg read babble\n");
			alarm(0);
			return(FAIL);
		}
		len = (*Read)(fn, str, TPACKSIZE);
		if (len <= 0) {
			DEBUG0(7, "trdmsg read failed\n");
			alarm(0);
			return(FAIL);
		}
		str += len;
		i += len;
	} while (*(str - 1) != '\0' || (i % TPACKSIZE) != 0);
	alarm(0);
	return(SUCCESS);
}

/*
 * read data from file fp1 and write on link
 *	fp1	-> file descriptor
 *	fn	-> link descriptor
 * returns:
 *	FAIL	->failure in link
 *	0	-> ok
 */
int
twrdata(FILE *fp1,
	int fn)
{
	int ret;
	int len;
	int fd1;
	long bytes;
	struct tbuf bufr;

	if (setjmp(Failbuf)) {
		DEBUG0(7, "twrdata failed\n");
		return(FAIL);
	}
	bytes = 0L;
	fd1 = fileno(fp1);
	(void)millitick();
	alarm(MAXTCPTIME);
	do {
		len = read(fd1, bufr.t_data, TBUFSIZE);
		if (len < 0) {
			DEBUG(7, "twrdata read errno=%d\n", errno);
			return(FAIL);
		}
		if (len == 0)
			break;
		bytes += len;
		bufr.t_nbytes = htonl(len);
		DEBUG(7,"twrdata sending %d bytes\n",len);
		len += sizeof(long);
		alarm(MAXTCPTIME);
		ret = (*Write)(fn, (char*)&bufr, len);
		alarm(0);
		if (ret != len) {
			DEBUG(7, "twrdata ret %d\n", ret);
			return(FAIL);
		}
	} while (len == TBUFSIZE+sizeof(long));
	if (len < 0)
		return(FAIL);
	bufr.t_nbytes = 0;
	alarm(MAXTCPTIME);
	ret = (*Write)(fn, (char*)&bufr, sizeof(long));
	alarm(0);
	if (sizeof(long) != ret)
		return(FAIL);
	statlog("->", bytes, millitick());
	return(SUCCESS);
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
int
trddata(int fn,
	FILE *fp2)
{
	int len;
	int	fd2;
	long bytes;
	long nread;
	char bufr[TBUFSIZE];
	int	writefile = TRUE, ret = SUCCESS;

	bytes = 0L;
	(void) millitick();
	fd2 = fileno(fp2);
	for (;;) {
		len = trdblk((char *)&nread,sizeof(nread),fn);
		if (len != sizeof(nread) ) {
			DEBUG0(7, "trdblk of block length failed\n");
			return(FAIL);
		}
		nread = ntohl(nread);
		if (nread == 0)
			break;
		if (nread > TBUFSIZE) {
			DEBUG(7,"trddata block %ld bytes too big\n",nread);
			return(FAIL);
		}
		DEBUG(7,"trddata expecting %ld bytes\n",nread);
		len = trdblk(bufr, nread, fn);
		if (len != nread)
			return(FAIL);

		if (writefile == TRUE && write(fd2, bufr, len) != len ) {
			ret = errno;
			DEBUG(7, "erddata: write to file failed, errno %d\n",
			      ret);
			writefile = FALSE;
		}
		bytes += len;
	}
	if (writefile != TRUE)
		return(ret);
	statlog( "<-", bytes, millitick());
	return(SUCCESS);
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
trdblk(char *blk,
       int len,
       int fn)
{
	int i, ret;

	if(setjmp(Failbuf)) {
		DEBUG0(7, "trdblk timeout\n");
		return(FAIL);
	}

	for (i = 0; i < len; i += ret) {
		alarm(MAXTCPTIME);
		DEBUG(7, "ask %d ", len - i);
		ret = (*Read)(fn, blk, (unsigned) len - i);
		if (ret <= 0) {
			alarm(0);
			DEBUG0(7, "read failed\n");
			return(FAIL);
		}
		DEBUG(7, "got %d\n", ret);
		if (ret == 0)
			break;
		blk += ret;
	}
	alarm(0);
	return(i);
}
