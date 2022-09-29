/*
 *			S T O R A G E . C
 *
 * Ray Tracing program, storage manager.
 *
 *  Functions -
 *	rt_malloc	Allocate storage, with visibility & checking
 *	rt_free		Similarly, free storage
 *	rt_prmem	When debugging, print memory map
 *	calloc, cfree	Which call rt_malloc, rt_free
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSstorage[] = "@(#)$Revision: 1.2 $";
#endif

#include <sys/param.h>
#include <syslog.h>
#include <malloc.h>

#define magic

#ifndef orig
#define MDB_SIZE	80000
#define MDB_MAGIC	0xFEEDBABE
struct memdebug {
	char	*mdb_addr;
	char	*mdb_str;
	int	mdb_len;
} rt_mdb[MDB_SIZE];

static void * realmalloc(size_t);
static void  realfree(void *);

/*
 *			R T _ M A L L O C
 */
void *
malloc(cnt)
unsigned int cnt;
{
	register char *ptr;

	cnt = (cnt+2*sizeof(int)-1) & (~(sizeof(int)-1));
	ptr = realmalloc(cnt);

	if( ptr==(char *)0 ) {
		syslog(LOG_ERR, "rt_malloc: malloc failure");
		abort();
	} else 	{
		register struct memdebug *mp = rt_mdb;
		for( ; mp < &rt_mdb[MDB_SIZE]; mp++ )  {
			if( mp->mdb_len > 0 )  continue;
			mp->mdb_addr = ptr;
			mp->mdb_len = cnt;
			mp->mdb_str = "???";
			goto ok;
		}
		syslog(LOG_ERR, "rt_malloc:  memdebug overflow\n");
	}
ok:	;
#ifdef magic
	{
		register int *ip = (int *)(ptr+cnt-sizeof(int));
		*ip = MDB_MAGIC;
	}
#endif
	return(ptr);
}

int free_smash = 1;

/*
 *			R T _ F R E E
 */
void
free(ptr)
void *ptr;
{
	register struct memdebug *mp = rt_mdb;
	for( ; mp < &rt_mdb[MDB_SIZE]; mp++ )  {
			if( mp->mdb_len <= 0 )  continue;
		if( mp->mdb_addr != ptr )  continue;
#ifdef magic
		{
			register int *ip = 
				(int *)(((char *)ptr)+mp->mdb_len-sizeof(int));
			if( *ip != MDB_MAGIC )  {
				syslog(LOG_ERR, 
				 "ERROR rt_free(x%x, %s) corrupted! x%x!=x%x\n",
				    ptr, "???", *ip, MDB_MAGIC);
				abort();
			}
		}
#endif
		mp->mdb_len = 0;	/* successful free */
		goto ok;
	}
	syslog(LOG_ERR, "ERROR rt_free(x%x, %s) bad pointer!\n", ptr, "???");
	abort();
ok:	;

	if (free_smash)
		*((int *)ptr) = 0xDEADBEEF;	/* zappo! */
	realfree(ptr);
}
#endif

/*
 *			R T _ P R M E M
 * 
 *  Print map of memory currently in use.
 */
void
rt_prmem(str)
char *str;
{
	register struct memdebug *mp = rt_mdb;
	register int *ip;

	printf("\nRT memory use\t\t%s\n", str);
	for( ; mp < &rt_mdb[MDB_SIZE]; mp++ )  {
		if( mp->mdb_len <= 0 )  continue;
		ip = (int *)(mp->mdb_addr+mp->mdb_len-sizeof(int));
		printf("%7x %5x %s %s\n",
			mp->mdb_addr, mp->mdb_len, mp->mdb_str,
			*ip!=MDB_MAGIC ? "-BAD-" : "" );
		if( *ip != MDB_MAGIC )
			printf("\t%x\t%x\n", *ip, MDB_MAGIC);
	}
}

void *
calloc(num, size)
	register unsigned num, size;
{
	register char *p;

	size *= num;
	if (p = malloc(size))
		bzero(p, size);
	return (p);
}

cfree(p, num, size)
	char *p;
	unsigned num;
	unsigned size;
{
	free(p);
}

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986. 1989, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include "shlib.h"
#include "values.h"

/*LINTLIBRARY*/
#define NDEBUG
#ifdef debug
#undef NDEBUG
#endif
#include <assert.h>
#undef malloc
#undef free
#undef realloc

/*	avoid break bug */
#if pdp11
#define GRANULE 64
#else
#define GRANULE 0
#endif
#if pdp11 || vax || u3b || M32 || mips
/* this make the assumption that MAXINT is one less than an
   even block */
#define MAXALLOC (MAXINT-BLOCK+1)
#endif
/*      C storage allocator
 *	circular first-fit strategy
 *	works with noncontiguous, but monotonically linked, arena
 *	each block is preceded by a ptr to the (pointer of) 
 *	the next following block
 *	blocks are exact number of words long 
 *	aligned to the data type requirements of ALIGN
 *	pointers to blocks must have BUSY bit 0
 *	bit in ptr is 1 for busy, 0 for idle
 *	gaps in arena are merely noted as busy blocks
 *	last block of arena (pointed to by alloct) is empty and
 *	has a pointer to first
 *	idle blocks are coalesced during space search
 *
 *	a different implementation may need to redefine
 *	ALIGN, NALIGN, BLOCK, BUSY, INT
 *	where INT is integer type to which a pointer can be cast
*/
#define INT int
#define ALIGN int
#define NALIGN 1
#define WORD sizeof(union store)
#define BLOCK (8*1024)	/* a multiple of WORD*/
#define BUSY 1
#define NULL 0
#define testbusy(p) ((INT)(p)&BUSY)
#define setbusy(p) (union store *)((INT)(p)|BUSY)
#define clearbusy(p) (union store *)((INT)(p)&~BUSY)

union store {
	union store *ptr;
	ALIGN dummy[NALIGN];
	int calloc;		/*calloc clears an array of integers*/
};

extern char *_sbrk(), *memcpy();
extern int _brk();

static union store allocs[2];	/*initial arena*/
static union store *allocp;	/*search ptr*/
static union store *alloct;	/*arena top*/
static union store *allocx;	/*for benefit of realloc*/
static union store *allocend;	/*the last block, if it is free, or
				  *alloct */
#ifdef orig
void *
malloc(size_t nbytes)
#else
static void *
realmalloc(size_t nbytes)
#endif
{
	register union store *p, *q;
	register int nw;
	register unsigned int temp;
	register unsigned int incr = 0;
	unsigned int sav_temp;

	if(allocs[0].ptr == 0) {		/*first time*/
		allocs[0].ptr = setbusy(&allocs[1]);
		allocs[1].ptr = setbusy(&allocs[0]);
		alloct = &allocs[1];
		allocp = &allocs[0];
		allocend = alloct;
	}
	nw = (nbytes+WORD+WORD-1)/WORD;
	assert(allocp >= allocs && allocp <= alloct);
	assert(allock());
	for(p=allocp; ; ) {
		for(temp=0; ; ) {
			if(!testbusy(p->ptr)) {
				while(!testbusy((q=p->ptr)->ptr)) {
					assert(q > p && q < alloct);
					allocp = p;
					p->ptr = q->ptr;
					if (allocend == q) allocend = p;
				}
				if(q >= p+nw && p+nw >= p)
					goto found;
			}
			q = p;
			p = clearbusy(p->ptr);
			if(p > q)
				assert(p <= alloct);
			else if(q != alloct || p != allocs) {
				assert(q == alloct && p == allocs);
				return(NULL);
			} else if(++temp > 1)
				break;
		}
		/* set block to search next */
		p = allocend;
		q = (union store *)_sbrk(0);
		if (q != alloct+1)  {
			/* the addition must be done in words to
			   prevent overflow.  Also, use temporaries,
			   since order of operations may be changed,
			   otherwise. */
			temp = ((nw+BLOCK/WORD - 1)/(BLOCK/WORD));
			temp = temp * BLOCK;
			if (((INT)q%WORD) != 0)  {
				incr = (WORD-(INT)q%WORD);
				q = (union store *)((char *)q + incr);
				temp += incr;
			} 
		}  else  {
			temp = nw - (alloct - allocend);
			temp = ((temp+BLOCK/WORD)/(BLOCK/WORD));
			temp = temp * BLOCK;
		}
		if(((unsigned int)q)+temp+GRANULE < (unsigned int)q) {
			return(NULL);
		}
		
		sav_temp = temp;
		if (temp > MAXALLOC)  {
			if ((INT)_sbrk(MAXALLOC) == -1)  {
				return NULL;
			}
			temp -= MAXALLOC;
		}
		if((INT)_sbrk(temp) == -1) {
			_brk(q);   	/* move brkval back */
			return(NULL);
		}
		allocend = q;
		assert(q > alloct);
		alloct->ptr = q;
		/* must subtract incr, since both q and temp had
		   incr added */
		q->ptr = (union store *)
			   ((unsigned char *)q + sav_temp - incr) - 1;
		if(q != alloct+1)
			alloct->ptr = setbusy(alloct->ptr);
		alloct = q->ptr;
		alloct->ptr = setbusy(allocs);
		q = p;
	}
found:
	allocp = p + nw;
	assert(allocp <= alloct);
	if(q > allocp) {
		allocx = allocp->ptr;
		allocp->ptr = p->ptr;
	}
	p->ptr = setbusy(allocp);
	/* move last block ptr, if necessary */
	if (allocend == p)  allocend = allocp;
	return((char*)(p+1));
}

/*      freeing strategy tuned for LIFO allocation */

#ifdef orig
void
free(register void *ap)
#else
static void
realfree(register void *ap)
#endif
{
	register union store *p = (union store *)ap;

	/* ANSI - free(0) does nothing */
	if (ap == (void *)0) return;

	assert(p > clearbusy(allocs[1].ptr) && p <= alloct);
	assert(allock());
	allocp = --p;
	assert(testbusy(p->ptr));
	/* if just freed last block in arena */
	p->ptr = clearbusy(p->ptr);
	if (p->ptr == alloct)  allocend = p;
	assert(p->ptr > allocp && p->ptr <= alloct);
	assert(allocend <= alloct);
}

/*
 *	realloc(p, nbytes) reallocates a block obtained from malloc()
 *	and freed since last call of malloc()
 *	to have new size nbytes, and old content
 *	returns new location, or 0 on failure
 */

void *
realloc(void *p, size_t nbytes)
{
	register void *q;
	register union store *ap, *aq;
	register unsigned nw;
	unsigned onw;

	/* ANSI allows ptr to be NULL, in which case 
	   realloc behaves like malloc (4.10.3.3)
	*/
	if (p != (void *)NULL) {
		ap = (union store *)p;
		if(testbusy(ap[-1].ptr))
			free(p);
		onw = ap[-1].ptr - ap;
		q = malloc(nbytes);
		/* ANSI: if nbytes == 0, no need to copy */
		if(q == NULL || q == p || nbytes == 0)
			return(q);
		nw = (nbytes+WORD-1)/WORD;
		if(nw < onw)
			onw = nw;
		aq = (union store *) memcpy(q, p, onw * WORD);
		if(aq < ap && aq+nw >= ap)
			(aq+(aq+nw-ap))->ptr = allocx;
	}
	else
		q = malloc(nbytes);
	return(q);
}

#ifdef debug
allock()
{
#ifdef longdebug
	register union store *p;
	int x;
	x = 0;
	for(p= &allocs[0]; clearbusy(p->ptr) > p; p=clearbusy(p->ptr)) {
		if(p == allocp)
			x++;
	}
	assert(p == alloct);
	return(x == 1 || p == allocp);
#else
	return(1);
#endif
}
#endif
/*	For debugging purposes only
/*rstalloc()
/*{
/*	
/*	allocs[0].ptr = 0;
/*	brk(clearbusy(allocs[1].ptr));
/*}
*/
