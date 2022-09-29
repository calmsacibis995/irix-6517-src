/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/flush.c	1.23"
/*LINTLIBRARY*/		/* This file always part of stdio usage */

#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"
#include <stdlib.h>
#include <stdio.h>
#include "stdiom.h"
#include <memory.h>
#include <unistd.h>

#undef _cleanup
#undef end

#define FILE_ARY_SZ	8 /* a nice size for FILE array & end_buffer_ptrs */

typedef struct _link_ Link;	/* list of iob's */

struct _link_	/* manages a list of streams */
{
	FILE	*iobp;	/* the array of FILE's */
	Uchar 	**endbuf; /* the array of end buffer pointers */
	Uchar	*endbufadj; /* amount endbuf adjusted for ungetc's */
	Uchar	*bufdirty; /* buffer is dirty, flush on seek */
	int	niob;	/* length of the arrays */
	Link	*next;	/* next in the list */
};

Link __first_link =	/* first in linked list */
{
	&_iob[0],
	&_bufendtab[0],
	&_bufendadj[0],
	&_bufdirtytab[0],
	_NFILE,
	0
};
static int iopgen = 0;

/*
* All functions that understand the linked list of iob's follow.
*/

void
_cleanup(void)	/* called at process end to flush output streams */
{
	fflush(NULL);
}

void
_flushlbf(void)	/* fflush() all line-buffered streams */
{
	register FILE *fp;
	register int i;
	register Link *lp;
	int ogen;
	LOCKDECLINIT(l, LOCKOPEN);

again:
	ogen = iopgen;
	lp = &__first_link;
	if ((lp->niob = (int)(_lastbuf - _iob)) > _NFILE)
		lp->niob = _NFILE;

	do {
		fp = lp->iobp;
		for (i = lp->niob; --i >= 0; fp++)
		{
			if ((fp->_flag & (_IOLBF | _IOWRT)) == (_IOLBF | _IOWRT)) {
				UNLOCKOPEN(l);
				(void)fflush(fp);
				LOCKINIT(l, LOCKOPEN);
				if (ogen != iopgen)
					goto again;
			}
		}
	} while ((lp = lp->next) != 0);
	UNLOCKOPEN(l);
}

FILE *
_findiop(void)	/* allocate an unused stream; 0 if cannot */
{
	register Link *lp, **prev;
	typedef struct		/* used so there only needs to be one malloc() */
	{
		Link	hdr;
		FILE	iob[FILE_ARY_SZ];
		Uchar	*nbuf[FILE_ARY_SZ]; /* array of end buffer pointers */
		Uchar	nbufadj[FILE_ARY_SZ]; /* array of adjustments to ... */
		Uchar	nbufdirty[FILE_ARY_SZ]; /* array of dirty flags */
	} Pkg;
	register Pkg *pkgp;
	register FILE *fp;

	assert(ISLOCKOPEN);
	iopgen++;
	lp = &__first_link;
	if ((lp->niob = (int)(_lastbuf - _iob)) > _NFILE)
		lp->niob = _NFILE;

	do {
		register int i;

		prev = &lp->next;
		fp = lp->iobp;
		for (i = lp->niob; --i >= 0; fp++)
		{
			if (fp->_flag == 0)	/* unused */
			{
				fp->_cnt = 0;
				fp->_ptr = 0;
				fp->_base = 0;
				return fp;
			}
		}
	} while ((lp = lp->next) != 0);
	/*
	* Need to allocate another and put it in the linked list.
	*/
	if ((pkgp = (Pkg *)malloc(sizeof(Pkg))) == 0)
		return 0;
	(void)memset(pkgp, 0, sizeof(Pkg));
	pkgp->hdr.iobp = &pkgp->iob[0];
	pkgp->hdr.niob = sizeof(pkgp->iob) / sizeof(FILE);
	pkgp->hdr.endbuf = &pkgp->nbuf[0];
	pkgp->hdr.endbufadj = &pkgp->nbufadj[0];
	pkgp->hdr.bufdirty = &pkgp->nbufdirty[0];
	*prev = &pkgp->hdr;
	return &pkgp->iob[0];
}

void
_setbufend(register FILE *iop, Uchar *end)	/* make sure _bufendtab[] big enough, set end ptr */
{
	register Link *lp;
	ptrdiff_t i;
	LOCKDECLINIT(l, LOCKOPEN);

	assert(ISLOCKFILE(iop));
	lp = &__first_link;

	do {
		if ((lp->iobp <= iop)  && (iop < ( lp->iobp + lp->niob)))
		{
			i = iop - lp->iobp;
			if (lp->endbuf[i] != end)
				lp->endbuf[i] = end;
			if (lp->endbufadj[i] != 0)
				lp->endbufadj[i] = 0;
			break;
		}
	} while ((lp = lp->next) != 0);
	UNLOCKOPEN(l);
}

Uchar *
_realbufend(FILE *iop) 	/* get the end pointer for this iop */
{
	register Link *lp;
	Uchar *e;

	LOCKDECLINIT(l, LOCKOPEN);
	assert(ISLOCKFILE(iop));
	lp = &__first_link;

	do {
		if ((lp->iobp <= iop)  && (iop < ( lp->iobp + lp->niob)))
		{
			e = lp->endbuf[iop - lp->iobp];
			UNLOCKOPEN(l);
			return e;
		}
	} while ((lp = lp->next) != 0);
	UNLOCKOPEN(l);
	return 0;
}

/* ARGSUSED */ 
void
_incbufend(FILE *iop, int cnt)
{
	register Link *lp;
	ptrdiff_t i;
	LOCKDECLINIT(l, LOCKOPEN);

	assert(ISLOCKFILE(iop));
	lp = &__first_link;

	do {
		if ((lp->iobp <= iop)  && (iop < ( lp->iobp + lp->niob)))
		{
			lp->endbuf[i = iop - lp->iobp]++;
			lp->endbufadj[i]++;
			break;
		}
	} while ((lp = lp->next) != 0);
	UNLOCKOPEN(l);
}

void
_resetbufend(FILE *iop)
{
	register Link *lp;
	ptrdiff_t i;
	LOCKDECLINIT(l, LOCKOPEN);

	assert(ISLOCKFILE(iop));
	lp = &__first_link;

	do {
		if ((lp->iobp <= iop)  && (iop < ( lp->iobp + lp->niob)))
		{
			i = iop - lp->iobp;
			lp->endbuf[i] -= lp->endbufadj[i];
			lp->endbufadj[i] = 0;
			break;
		}
	} while ((lp = lp->next) != 0);
	UNLOCKOPEN(l);
}

int
_realbufendadj(FILE *iop)
{
	register Link *lp;
	int rval = 0;
	LOCKDECLINIT(l, LOCKOPEN);

	assert(ISLOCKFILE(iop));
	lp = &__first_link;

	do {
		if ((lp->iobp <= iop)  && (iop < ( lp->iobp + lp->niob)))
		{
			rval = lp->endbufadj[iop - lp->iobp];
			break;
		}
	} while ((lp = lp->next) != 0);
	UNLOCKOPEN(l);
	return rval;
}

int
_realbufdirty(FILE *iop)
{
	register Link *lp;
	int rval = 0;
	LOCKDECLINIT(l, LOCKOPEN);

	assert(ISLOCKFILE(iop));
	lp = &__first_link;

	do {
		if ((lp->iobp <= iop)  && (iop < ( lp->iobp + lp->niob)))
		{
			rval = lp->bufdirty[iop - lp->iobp];
			break;
		}
	} while ((lp = lp->next) != 0);
	UNLOCKOPEN(l);
	return rval;
}

void
_setbufdirty(FILE *iop)	/* note that buffer is dirty */
{
	register Link *lp;
	LOCKDECLINIT(l, LOCKOPEN);

	assert(ISLOCKFILE(iop));
	lp = &__first_link;

	do {
		if ((lp->iobp <= iop)  && (iop < ( lp->iobp + lp->niob)))
		{
			lp->bufdirty[iop - lp->iobp] = 1;
			break;
		}
	} while ((lp = lp->next) != 0);
	UNLOCKOPEN(l);
}

void
_setbufclean(FILE *iop)	/* mark buffer as clean */
{
	register Link *lp;
	LOCKDECLINIT(l, LOCKOPEN);

	assert(ISLOCKFILE(iop));
	lp = &__first_link;

	do {
		if ((lp->iobp <= iop)  && (iop < ( lp->iobp + lp->niob)))
		{
			lp->bufdirty[iop - lp->iobp] = 0;
			break;
		}
	} while ((lp = lp->next) != 0);
	UNLOCKOPEN(l);
}

void
_bufsync(FILE *iop, Uchar *bufend)	/* make sure _cnt, _ptr are correct */
{
	register ptrdiff_t spaceleft;
	assert(ISLOCKFILE(iop));

	if ((spaceleft = bufend - iop->_ptr) < 0) {
		iop->_ptr = bufend;
		iop->_cnt = 0;
	} else if (spaceleft < iop->_cnt)
		iop->_cnt = spaceleft;
}


int
_xflsbuf(register FILE *iop)	/* really write out current buffer contents */
{
	register size_t n;
	register Uchar *base = iop->_base;
	register Uchar *bufend;
	ssize_t num_wrote;

	assert(ISLOCKFILE(iop));
	/*
	 * Hopefully, be stable with respect to interrupts...
	 */
	n = (size_t) (iop->_ptr - base);
	iop->_ptr = base;
	bufend = _bufend(iop);
	if (iop->_flag & (_IOLBF | _IONBF))
		iop->_cnt = 0; /* always go to a flush */
	else
		iop->_cnt = bufend - base;
	if (_needsync(iop, bufend))   /* recover from interrupts */
		_bufsync(iop, bufend);
	if (bufendadj(iop))
		resetbufend(iop);
	if (n == 0)
		return 0;
	while((num_wrote = write(iop->_file, base, n)) != n) {
		if (num_wrote <= 0) {
			iop->_flag |= _IOERR;
			return EOF;
		}
		n -= num_wrote;
		base += num_wrote;
	}
	return 0;
}

/*
 * WARNING - this code is more complex than it appears!
 * In particular one must underrstand POSIX section 8.2.3, and consider what
 * happens on fork and subsequent child exit or fclose or fflush.
 * In particular, its important the fflush(NULL) not affect read descriptors
 * since fflush(NULL) is what is called on process exit. If it does
 * then a child which simply exits will affect the parent.
 *
 * Note that the current behavior DOES break programs like lpd which
 * fopen/fread -> fork -> child calls fclose() -> parent continues to
 * fread.
 */
int
fflush(register FILE *iop)	/* flush (write) buffer */
{
	int res = 0;
	FILE tmp_iop;
	LOCKDECL(l2);

	if (iop == NULL) {
		register int i;
		register Link *lp;
		int ogen;
		LOCKDECLINIT(l, LOCKOPEN);

again:
		ogen = iopgen;
		lp = &__first_link;
		if ((lp->niob = (int)(_lastbuf - _iob)) > _NFILE)
			lp->niob = _NFILE;

		do {
			iop = lp->iobp;
			for (i = lp->niob; --i >= 0; iop++) {
				if ((iop->_flag & _IOWRT) == 0)
					continue;
				UNLOCKOPEN(l);
				res |= fflush(iop);
				LOCKINIT(l, LOCKOPEN);
				if (ogen != iopgen)
					goto again;
			}
		} while ((lp = lp->next) != 0);
		UNLOCKOPEN(l);
		return res;
	}

	/*
	 * The file descriptor of the iop may change while we are waiting on
	 * its file lock (see bug 527886). Further, fopen can reset an iop
	 * without acquiring its corresponding lock.
	 *
	 * Using tmp_iop guarantees the file descriptor, which is used as the
	 * index into the iop semaphore table, remains constant while we access
	 * the iop lock via flockfile and funlockfile.
	 *
	 * We only care about this case in fflush, since fflush(0) is the
	 * always called upon exit.
	 */
	tmp_iop._file = iop->_file;
	LOCKINIT(l2, LOCKFILE(&tmp_iop));
	if (tmp_iop._file != iop->_file) {
		UNLOCKFILE(&tmp_iop, l2);
		return EOF;
	}

	if (!(iop->_flag & _IOWRT))
	{
		if (iop->_cnt > 0)
			lseek64(iop->_file, (off64_t)-iop->_cnt, SEEK_CUR);
		iop->_cnt = 0;
		iop->_ptr = iop->_base;	/* needed for ungetc & multibyte pushbacks */
		if (bufendadj(iop))
			resetbufend(iop);
		if (iop->_flag & _IORW) {
			iop->_flag &= (unsigned short)~_IOREAD;
		}
		UNLOCKFILE(&tmp_iop, l2);
		return 0;
	}
	if (iop->_base != 0 && iop->_ptr > iop->_base)
		res = _xflsbuf(iop);
	if (iop->_flag & _IORW) {
		iop->_flag &= (unsigned short)~_IOWRT;
		iop->_cnt = 0;
	}
	UNLOCKFILE(&tmp_iop, l2);
	return res;
}

int
fclose(register FILE *iop)	/* flush buffer and close stream */
{
	register int res = 0;
	LOCKDECL(l);
	LOCKDECL(l2);

	if (iop == 0 || iop->_flag == 0)
		return EOF;
	LOCKINIT(l, LOCKFILE(iop));
	LOCKINIT(l2, LOCKOPEN);
	/* POSIX 1990: B.8.2.3.2 requires readable case to work also */
	/* Is it readable/writeable and not unbuffered? */
	if (iop->_flag & (_IOREAD | _IOWRT | _IORW))
		if (!(iop->_flag & _IONBF))
			res = fflush(iop);
	
	if (close(iop->_file) < 0)
		res = EOF;
	if (iop->_flag & _IOMYBUF) {
		free((char *)iop->_base);
		/* free((VOID *)iop->_base); */
	}
	iop->_base = 0;
	iop->_ptr = 0;
	iop->_cnt = 0;
	iop->_flag = 0;			/* marks it as available */
	/* unlock file first since once the open lock is released
	 * someone else can grab the file descriptor and since the stdio
	 * semaphore array is indexed by descriptor
	 */
	UNLOCKFILE(iop, l);
	UNLOCKOPEN(l2);
	return res;
}
