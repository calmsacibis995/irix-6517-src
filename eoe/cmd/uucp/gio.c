/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.5 $"

#include "uucp.h"

#include "pk.h"

jmp_buf Gfailbuf;
struct pack *Pk;

static int grdblk(char *, int, int);
static int gwrblk(char *, int, int);


void
pkfail(void)
{
	longjmp(Gfailbuf, 1);
}

gturnon()
{
	if (setjmp(Gfailbuf))
		return(FAIL);
	if (Debug > 4)
		pkdebug = 1;
	Pk = pkopen(Ifn, Ofn);
	if ((int) Pk == NULL)
		return(FAIL);
	return(0);
}


gturnoff()
{
	if(setjmp(Gfailbuf))
		return(FAIL);
	pkclose(Pk);
	return(0);
}


gwrmsg(char type, char *str, int fn)
{
	char bufr[BUFSIZ], *s;
	int len, i;

	if(setjmp(Gfailbuf))
		return(FAIL);
	bufr[0] = type;
	s = &bufr[1];
	while (*str)
		*s++ = *str++;
	*s = '\0';
	if (*(--s) == '\n')
		*s = '\0';
	len = strlen(bufr) + 1;
	if ((i = len % PACKSIZE)) {
		len = len + PACKSIZE - i;
		bufr[len - 1] = '\0';
	}
	gwrblk(bufr, len, fn);
	return(0);
}


/*ARGSUSED*/
grdmsg(str, fn)
char *str;
{
	unsigned len;

	if(setjmp(Gfailbuf))
		return(FAIL);
	for (;;) {
		len = pkread(Pk, str, PACKSIZE);
		if (len == 0)
			continue;
		str += len;
		if (*(str - 1) == '\0')
			break;
	}
	return(0);
}


gwrdata(fp1, fn)
FILE *fp1;
{
	char bufr[BUFSIZ];
	int fd1;
	int len;
	int ret;
	time_t ticks;
	long bytes;

	if(setjmp(Gfailbuf))
		return(FAIL);
	bytes = 0L;
	fd1 = fileno( fp1 );
	(void) millitick();	/* set msec timer */
	while ((len = read( fd1, bufr, BUFSIZ )) > 0) {
		bytes += len;
		ret = gwrblk(bufr, len, fn);
		if (ret != len) {
			return(FAIL);
		}
		if (len != BUFSIZ)
			break;
	}
	ret = gwrblk(bufr, 0, fn);
	ticks = millitick();
	statlog( "->", bytes, ticks );
	return(0);
}


grddata(fn, fp2)
FILE *fp2;
{
	int fd2;
	int len;
	char bufr[BUFSIZ];
	time_t ticks;
	long bytes;

	if(setjmp(Gfailbuf))
		return(FAIL);
	bytes = 0L;
	fd2 = fileno( fp2 );
	(void) millitick();	/* set msec timer */
	for (;;) {
		len = grdblk(bufr, BUFSIZ, fn);
		if (len < 0) {
			return(FAIL);
		}
		bytes += len;
		if (write( fd2, bufr, len ) != len) {
			DEBUG(7, "grddata: write to file failed, errno %d\n", errno);
			return(FAIL);
		}
		if (len < BUFSIZ)
			break;
	}
	ticks = millitick();
	statlog( "<-", bytes, ticks );
	return(0);
}


/*ARGSUSED*/
static int
grdblk(blk, len,  fn)
char *blk;
{
	int i, ret;

	for (i = 0; i < len; i += ret) {
		ret = pkread(Pk, blk, len - i);
		if (ret < 0)
			return(FAIL);
		blk += ret;
		if (ret == 0)
			return(i);
	}
	return(i);
}


/*ARGSUSED*/
static int
gwrblk(blk, len, fn)
char *blk;
{
	int ret;

	ret = pkwrite(Pk, blk, len);
	return(ret);
}
