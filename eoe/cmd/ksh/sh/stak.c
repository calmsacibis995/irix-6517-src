/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ksh:sh/stak.c	1.2.4.1"

/*
 *   Routines to implement a stack-like storage library
 *
 *   A stack consists of a link list of variable size frames
 *   The beginning of each frame is initialized with a frame structure
 *   that contains a pointer to the previous frame and a pointer to the
 *   end of the current frame.
 *
 *   David Korn
 *   AT&T Bell Laboratories
 *   Room 3C-526B
 *   Murray Hill, N. J. 07974
 *   Tel. x7975
 *   ulysses!dgk
 *
 */

extern char	*malloc();
extern char	*strcpy();
#ifdef USE_REALLOC
    /* can only be used when realloc() to smaller size will not move data */
    extern char	*realloc();
#endif /* USE_REALLOC */

#define _STAK_PRIVATE \
	short		stakflags;	/* stack attributes */ \
	struct  _stak_	*stakcur;	/* current stack pointer  */ \
	char		*stakbase;	/* base of current stack frame */ \
	char		*stakend;	/* end of current stack frame */ \
	char		*(*stakoverflow)();	/* called when malloc fails */

#include	<sys/types.h>
#include	"stak.h"

#define STAK_MYSIZE	2		/* local argument to stakcreate */
#define STAK_FSIZE	(1024*sizeof(int))
#define	rndsize		sizeof(off_t)
#define round(a,b)	((((a)+b)-1)&~((b)-1))

#ifdef STAKSTATS
    static struct
    {
	int	create;
	int	delete;
	int	install;
	int	alloc;
	int	copy;
	int	puts;
	int	seek;
	int	set;
	int	grow;
	int	addsize;
	int	delsize;
	int	movsize;
    } _stakstats;
#   define increment(x)	(_stakstats.x++)
#   define count(x,n)	(_stakstats.x += (n))
#else
#   define increment(x)
#   define count(x,n)
#endif /* STAKSTATS */

struct frame
{
	char	*prev;
	char	*end;
};

static char *overflow();
static const char Omsg[] = "malloc failed while growing stack\n";
static int minsize = STAK_FSIZE;

Stak_t _stak_cur =
{
	0,			/* stakleft */
	(char*)(&_stak_cur),	/* staktop */
	(char*)(&_stak_cur),	/* stakbot */
	1,			/* stakref */
	0,			/* stakflags */
	&_stak_cur,		/* stakcur */
	(char*)(&_stak_cur),	/* stakbase */
	(char*)(&_stak_cur),	/* stakend */
	overflow		/* stakoverflow */
};

/*
 * create a stack
 * minsize is normally STAK_FSIZE but may be larger when set by _stakgrow()
 */

Stak_t *stakcreate(flags)
register int flags;
{
	register Stak_t *sp;
	register char *cp;
	register struct frame *fp;
	register int size,fsize;
	if(flags&STAK_MYSIZE)
		fsize = minsize;
#ifndef USE_REALLOC
	else if(flags&STAK_SMALL)
		fsize = STAK_FSIZE/16;
#endif /* USE_REALLOC */
	else
		fsize = STAK_FSIZE;
	minsize = STAK_FSIZE;
	size = fsize + sizeof(struct frame)+sizeof(Stak_t);
	if((cp=malloc(size))==0)
		return((Stak_t*)0);
	increment(create);
	count(addsize,size);
	sp = (Stak_t*)cp;
	sp->stakcur = sp;
	cp += sizeof(Stak_t);
	fp = (struct frame*)cp;
	fp->prev = 0;
	sp->stakbase = cp;
	sp->stakref = 1;
	cp += sizeof(struct frame);
	sp->staktop = sp->stakbot = cp;
	sp->stakflags = (flags&STAK_SMALL);
	sp->stakoverflow = _stak_cur.stakoverflow;
	sp->stakend  = fp->end = cp+fsize;
	sp->stakleft = fsize;
	return(sp);
}

/*
 * return a pointer to the current stack
 * if <sp> is not null, it becomes the new current stack
 * <oflow> becomes the new overflow function
 */

#if defined(__STDC__)
    Stak_t *stakinstall(Stak_t *sp, char *(*oflow)(int))
#else
    Stak_t *stakinstall(sp,oflow)
    Stak_t *sp;
    char *(*oflow)();
#endif /* __STDC__ */
{
	Stak_t *oldsp = _stak_cur.stakcur;
	increment(install);
	if(sp)
	{
#ifdef USE_REALLOC
		register struct frame *fp;
		register char *cp;
		/* only works if realloc() to reduce does not relocate */
		if(_stak_cur.stakflags&STAK_SMALL)
		{
			/* shrink the last frame */
			fp = (struct frame*)(cp=_stak_cur.stakbase);
			if(fp->prev==0)
				cp = (char*)oldsp;
			_stak_cur.stakend = fp->end = _stak_cur.staktop;
			_stak_cur.stakleft = 0;
			if(realloc(cp,_stak_cur.stakend-cp)!=cp)
				return(0);
		}
#endif /* USE_REALLOC */
		*oldsp = _stak_cur;
		_stak_cur = *sp;
	}
	else
		sp = oldsp;
	if(oflow)
		sp->stakoverflow = (char*(*)())oflow;
	return(oldsp);
}

/*
 * terminate a stack and free up the space
 */

stakdelete(sp)
Stak_t *sp;
{
	register char *cp = sp->stakbase;
	register struct frame *fp;
	if(--sp->stakref>0)
		return(1);
	increment(delete);
	while(1)
	{
		fp = (struct frame*)cp;
		if(fp->prev)
		{
			cp = fp->prev;
			free((char*)fp);
		}
		else
			break;
	}
	/* now free the first frame */
	if(sp != &_stak_cur)
		free((void*)sp);
	return(0);
}

/*
 * reset the bottom of the current stack back to <loc>
 * if <loc> is not in this stack, then the stack is reset to the beginning
 * otherwise, the top of the stack is set to stakbot+<offset>
 *
 */

char *stakset(loc,offset)
register char *loc;
unsigned offset;
{
	register Stak_t *sp = &_stak_cur;
	register char *cp;
	register struct frame *fp;
	if(sp==sp->stakcur)
		return(sp->stakbot);
	increment(set);
	while(1)
	{
		/* see whether <loc> is in current stack frame */
		if(loc>=(cp=sp->stakbase) && loc<=sp->stakend)
		{
			sp->stakbot = cp + round(loc-cp,rndsize);
			sp->staktop = loc+offset;
			goto found;
		}
		fp = (struct frame*)cp;
		if(fp->prev)
		{
			sp->stakbase = fp->prev;
			sp->stakend = ((struct frame*)(fp->prev))->end;
			free(cp);
		}
		else
			break;
	}
	/* set stack back to the beginning */
	sp->staktop = sp->stakbot = ((char*)(sp->stakcur+1))+sizeof(struct frame);
found:
	sp->stakleft = sp->stakend - sp->staktop;
	return(sp->stakbot);
}

/*
 * allocate <n> bytes on the current stack
 */

#if defined(__STDC__)
    char *stakalloc(register unsigned n)
#else
    char *stakalloc(n)
    register unsigned n;
#endif /* __STDC__ */
{
	register Stak_t *sp = &_stak_cur;
	register char *old;
	increment(alloc);
	n = round(n,rndsize);
	if((sp->stakleft += ((sp->staktop-sp->stakbot)-(int)n)) <=0)
		_stakgrow(n);
	old = sp->stakbot;
	sp->stakbot = sp->staktop = old+n;
	return(old);
}

/*
 * begin a new stack word of at least <n> bytes
 */
char *stakseek(n)
register unsigned n;
{
	register Stak_t *sp = &_stak_cur;
	increment(seek);
	if((sp->stakleft += ((sp->staktop-sp->stakbot)-(int)n)) <=0)
		_stakgrow(n);
	sp->staktop = sp->stakbot+n;
	return(sp->stakbot);
}

/*
 * put the string <str> onto the stack
 * returns the length of the string
 */
int	stakputs(str)
register const char *str;
{
	register Stak_t *sp = &_stak_cur;
	register const char *cp=str;
	register int n;
	while(*cp++);
	n = cp-str;
	increment(puts);
	if((sp->stakleft -= n) <=0)
		_stakgrow(n);
	strcpy(sp->staktop,str);
	sp->staktop += --n;
	return(n);
}

/*
 * advance the stack to the current top
 * if extra is non-zero, first add a extra bytes and zero the first
 */
char	*stakfreeze(extra)
register unsigned extra;
{
	register Stak_t *sp = &_stak_cur;
	register char *old = sp->stakbot;
	register char *top = sp->staktop;
	if(extra)
	{
		if(extra > sp->stakleft)
		{
			top = _stakgrow(extra);
			old = sp->stakbot;
		}
		*top = 0;
		top += extra;
	}
	sp->staktop = sp->stakbot += round(top-old,rndsize);
	sp->stakleft = sp->stakend-sp->staktop;
	return(old);
}

/*
 * copy string <str> onto the stack as a new stack word
 */
char	*stakcopy(str)
const char *str;
{
	register Stak_t *sp = &_stak_cur;
	register char *cp = (char*)str;
	register int n;
	while(*cp++);
	n = round(cp-str,rndsize);
	increment(copy);
	if((sp->stakleft += ((sp->staktop-sp->stakbot)-n)) <=0)
		_stakgrow(n);
	strcpy(cp=sp->stakbot,str);
	sp->stakbot = sp->staktop = cp+n;
	return(cp);
}

/*
 * add a new stack frame of size >= <n> to the current stack.
 * if <n> > 0, copy the bytes from stakbot to staktop to the new stack
 * if <n> is zero, then copy the remainder of the stack frame from stakbot
 * to the end is copied into the new stack frame
 */

char *_stakgrow(size)
unsigned size;
{
	register int n = size;
	register Stak_t *sp = &_stak_cur;
	register struct frame *fp;
	register char *cp;
	register unsigned m = (n?sp->staktop:sp->stakend)-sp->stakbot;
	n += (m + sizeof(struct frame)+1);
	if(sp->stakflags&STAK_SMALL)
#ifndef USE_REALLOC
		n = round(n,STAK_FSIZE/16);
	else
#endif /* !USE_REALLOC */
		n = round(n,STAK_FSIZE);

	/* this is done because we otherwise allocate huge amounts
	 * of memory from comsubst(), when somebody does things like
	 *	for v in `cat /tmp/y`; do something; done
	 * for large files, because we copy the whole stack on each grow.
	 * As a specific example, with a 1MB file, the shell would grow to
	 * 120MB; this hack causes us to grow the stack by larger amounts
	 * when we have already grown it to large sizes, so we do fewer
	 * copies.  See bug #437625.  Olson, 11/96
	*/
	if(n > (4*STAK_FSIZE)) {
		if(n <= (64*STAK_FSIZE))
			n *= 2;
		else
			n += 64*STAK_FSIZE;
	}

	/* check for first time default stack reference */
	if(sp==sp->stakcur)
	{
		minsize = n;
		if((sp = stakcreate(STAK_MYSIZE))==0)
			sp = (Stak_t*)overflow(sizeof(Stak_t));
		sp->stakleft -= size;
		_stak_cur = *sp;
		return(sp->stakbot);
	}
	cp = malloc(n);
	if(cp==(char*)0)
		cp = (*sp->stakoverflow)(n);
	increment(grow);
	count(addsize,n);
	fp = (struct frame*)cp;
	fp->prev = sp->stakbase;
	sp->stakbase = cp;
	sp->stakend = fp->end = cp+n;
	sp->stakleft = n-(m+size+sizeof(struct frame));
	cp = (char*)(fp+1);
	if(m)
		memcpy(cp,sp->stakbot,m);
	count(movsize,m);
	sp->stakbot = cp;
	return(sp->staktop = sp->stakbot+m);
}


static char *overflow(n)
int n;
{
	(&n,1);
	write(2,Omsg, sizeof(Omsg)-1);
	exit(2);
	/* NOTREACHED
	return(0);
	 */
}

