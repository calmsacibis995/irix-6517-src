/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sh:io.c	1.10.6.1"
/*
 * UNIX shell
 */

#include	"defs.h"
#include	"dup.h"
#include	<fcntl.h>

int	 topfd;

static void	pushtemp();

extern int	close();
extern int	pipe();
extern int	write();
extern int	link();

/* ========	input output and file copying ======== */

void
initf(fd)
int	fd;
{
	register struct fileblk *f = standin;

	f->fdes = fd;
	f->fsiz = ((flags & oneflg) == 0 ? BUFSIZE : 1);
	f->fnxt = f->fend = f->fbuf;
	f->feval = 0;
	f->flin = 1;
	f->feof = FALSE;
}

int
estabf(s)
register unsigned char *s;
{
	register struct fileblk *f;

	(f = standin)->fdes = -1;
	f->fend = length(s) + (f->fnxt = s);
	f->flin = 1;
	return(f->feof = (s == 0));
}

void
push(af)
struct fileblk *af;
{
	register struct fileblk *f;

	(f = af)->fstak = standin;
	f->feof = 0;
	f->feval = 0;
	standin = f;
}

int
pop()
{
	register struct fileblk *f;

	if ((f = standin)->fstak)
	{
		if (f->fdes >= 0)
			(void)close(f->fdes);
		standin = f->fstak;
		return(TRUE);
	}
	else
		return(FALSE);
}

static struct tempblk *tmpfptr;

static void
pushtemp(fd,tb)
	int fd;
	struct tempblk *tb;
{
	tb->fdes = fd;
	tb->fstak = tmpfptr;
	tmpfptr = tb;
}

int
poptemp()
{
	if (tmpfptr)
	{
		(void)close(tmpfptr->fdes);
		tmpfptr = tmpfptr->fstak;
		return(TRUE);
	}
	else
		return(FALSE);
}
	
void
chkpipe(pv)
int	*pv;
{
	if (pipe(pv) < 0 || pv[INPIPE] < 0 || pv[OTPIPE] < 0)
		error(0, piperr, piperrid);
}

int
chkopen(idf)
unsigned char *idf;
{
	register int	rc;

	if ((rc = open((char *)idf, 0)) < 0)
		failed(0, idf, badopen, badopenid); /*Does not return*/
	return(rc);
}

void
renam(f1, f2)
register int	f1, f2;
{
#ifdef RES
	if (f1 != f2)
	{
		dup(f1 | DUPFLG, f2);
		(void)close(f1);
		if (f2 == 0)
			ioset |= 1;
	}
#else
	int	fs;

	if (f1 != f2)
	{
		fs = fcntl(f2, 1, 0);
		(void)close(f2);
		fcntl(f1, 0, f2);
		(void)close(f1);
		if (fs == 1)
			fcntl(f2, 2, 1);
		if (f2 == 0)
			ioset |= 1;
	}
#endif
}

int
create(s)
unsigned char *s;
{
	register int	rc;

	if ((rc = creat((char *)s, 0666)) < 0)
		failed(0, s, badcreate, badcreateid); /*does not return*/
	return(rc);
}

int
tmpfil(tb)
	struct tempblk *tb;
{
	int fd;

	itos(serial++);
	movstr(numbuf, tmpname);
	fd = create(tmpout);
	pushtemp(fd,tb);
	return(fd);
}

/*
 * set by trim
 */
extern BOOL		nosubst;
#define			CPYSIZ		512
void
copy(ioparg)
struct ionod	*ioparg;
{
	register unsigned char	*cline;
	register unsigned char	*clinep;
	register struct ionod	*iop;
	unsigned char	c;
	unsigned char	*ends;
	unsigned char	*start;
	int		fd;
	int		i;
	int		stripflg;
	

	iop = ioparg;
	if (iop)
	{
		struct tempblk tb;

		copy(iop->iolst);
		ends = mactrim(iop->ioname);
		stripflg = iop->iofile & IOSTRIP;
		if (nosubst)
			iop->iofile &= ~IODOC;
		fd = tmpfil(&tb);

		if (fndef)
			iop->ioname = make(tmpout);
		else
			iop->ioname = cpystak(tmpout);

		iop->iolst = iotemp;
		iotemp = iop;

		cline = clinep = start = locstak();
		if (stripflg)
		{
			iop->iofile &= ~IOSTRIP;
			while (*ends == '\t')
				ends++;
		}
		for (;;)
		{
			chkpr();
			if (nosubst)
			{
				c = readc();
				if (stripflg)
					while (c == '\t')
						c = readc();

				while (!eolchar(c))
				{
					*clinep++ = c;
					c = readc();
				}
			}
			else
			{
				c = nextc();
				if (stripflg)
					while (c == '\t')
						c = nextc();
				
				while (!eolchar(c))
				{
					*clinep++ = c;
					if(c == '\\')
						*clinep++ = readc();
					c = nextc();
				}
			}

			*clinep = 0;
			if (eof || eq(cline, ends))
			{
				if ((i = cline - start) > 0)
					(void)write(fd, start, i);
				break;
			}
			else
				*clinep++ = NL;

			if ((i = clinep - start) < CPYSIZ)
				cline = clinep;
			else
			{
				(void)write(fd, start, i);
				cline = clinep = start;
			}
		}

		poptemp();		/* pushed in tmpfil -- bug fix for problem
					   deleting in-line scripts */
	}
}

void
link_iodocs(i)
	struct ionod	*i;
{
	while(i)
	{
		free(i->iolink);

		itos(serial++);
		movstr(numbuf, tmpname);
		i->iolink = make(tmpout);
		(void)link(i->ioname, i->iolink);

		i = i->iolst;
	}
}

void
swap_iodoc_nm(i)
	struct ionod	*i;
{
	while(i)
	{
		free(i->ioname);
		i->ioname = i->iolink;
		i->iolink = 0;

		i = i->iolst;
	}
}

int
savefd(fd)
	int fd;
{
	register int	f;

	f = fcntl(fd, F_DUPFD, 10);
	return(f);
}

void
restore(last)
	register int	last;
{
	register int 	i;
	register int	dupfd;

	for (i = topfd - 1; i >= last; i--)
	{
		if ((dupfd = fdmap[i].dup_fd) > 0)
			renam(dupfd, fdmap[i].org_fd);
		else
			(void)close(fdmap[i].org_fd);
	}
	topfd = last;
}
