/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.4 $"

#ifdef NEVER
Path: sgi!daemon
From: alf@rains.sgi.com
Newsgroups: sgi.bugs.clover31
Subject: (none)
Message-ID: <20658@sgi.SGI.COM>
Date: 29 Aug 88 18:18:08 GMT
Sender: daemon@sgi.SGI.COM
Lines: 114

2932. *                 *                       *               *           
--------------------------------------------------------------------------------
From: guest@maytag
Date: Mon, 29 Aug 88 11:16:58 PDT
Subject: 

The 4D malloc is just too slow at mallocing new memory.  If it were just
twice as slow I wouldnot complain, but its about 80 times slower than
the old 3000 malloc (shown below under the 4Dme column).

Below are timings (in seconds) for programs 'memory' plus 4 random tests.
Memory just mallocs memory without ever freeing.  The random tests are all
slightly different.  They try to randomly malloc and free data, in an attempt
to reproduce a typical applications pattern.  The columns with a /1 over
them indicate running the test so that only half the malloced chunks are
freed on average.  The columns with /0 over them should establish a steady
state size, ie they always free a malloced chunk.

The 3Dme column is for my malloc (a fixed 3.5 malloc) on a 3030.
The 4Dme column is for my malloc (a fixed 3.5 malloc) on a 4D/70.
The 4D3.0 column is for the 3.0 libc malloc on a 4D/70.
The 4D3.1 column is for the 3.1 libc malloc on a 4D/70.

Note that the 4D3.0 malloc is not that bad except for initial mallocs, as
evidenced by the slow time for 'memory' and the init time for 'random3',
where it is about 20-50 times worse than mine.  The /1 numbers are about
twice as bad but that is probably due to the same inefficiency evidenced in
'memory'.

The 4D3.1 is really atrocious at mallocing memory without freeing!  About 2.6
times slower than the 3.0 release.  The /1 columns are about 2 times worse
than 3.0, probably due to the same problem.  Note that the /0 columns are
respectable, indicating that if the malloced cell was gotten from the free
list then its not too slow.  My conclusion is that if just mallocing without
freeing were improved, I think the numbers would be very respectable. In
other words, if the 'memory' test ran in less than 3 seconds I think all the
other numbers would fall in line.

memory	- malloc many without freeing any
	mallocs  3Dme		4Dme		4D3.0		4D3.1
	-------  ----		----		-----		-----
	 100000	 3.5		 1.5		 39.5		110
	 200000	 7.2		 3.0		155.0

random	- random malloc+free into 1K array
	passes  3Dme/1	/0	4Dme/1	/0	4D3.0/1	  /0	4D3.1/1	  /0
	------  ----		----		-----		-----
	200	58	14	17.6	3.6	35.0	 3.4	63.5	  4.0
	400		28		7.1		 6.8		  7.8

random1	- random malloc+free
	passes  3Dme/1	/0	4Dme/1	/0	4D3.0/1	  /0	4D3.1/1	  /0
	------  ----		----		-----		-----
	200	58	20	17.6	4.9	34.2	 5.4	62.2	  5.8
	400		39		9.8		10.3		 11.1

random2	- random malloc+free into 8K array
	passes  3Dme/1	/0	4Dme/1	/0	4D3.0/1	  /0	4D3.1/1	  /0
	------  ----		----		-----		-----
	200	261	27	95.4	 8	134	  10	161	 12
	400		43		12		  16		 19
	1000		96		26		  35		 42

random3	- random malloc+free into 128K array after 75% full
	    time reported is init time + pass time
	passes  3Dme		4Dme		4D3.0		4D3.1
	------  ----		----		-----		-----
	 10	15+31=46	4+12=16		128+20=148	375+45=420
	 20	15+53=68	4+20=24		125+40=165

Here are editted profile listings from 4D3.0 and 4D3.1 when running 'memory'.
Note that the 3.1 malloc is 2.6 times slower than the 3.0 malloc.  Sbrk is
also called more often (about 2.6 times as much).

----------------------------------------------------------------------------
*  -p[rocedures] using basic-block counts;                                 *
*  sorted in descending order by the number of cycles executed in each     *
*  procedure; unexecuted procedures are excluded                           *
----------------------------------------------------------------------------

13344057 cycles (1.0675 seconds at 12.50 megahertz)
    cycles %cycles  cum %  seconds     cycles  bytes procedure (file)
                                        /call  /line

  13198048   98.91  98.91   1.0558        660     13 malloc (malloc.c)
    140017    1.05  99.96   0.0112     140017     23 main (memory.c)
      2360    0.02  99.99   0.0002         10      6 sbrk (sbrk.s)


33216885 cycles (2.6574 seconds at 12.50 megahertz)
    cycles %cycles  cum %  seconds     cycles  bytes procedure (file)
                                        /call  /line

  32826530   98.82  98.82   2.6261       1642     14 _lmalloc (malloc.c)
    240007    0.72  99.55   0.0192         13     16 malloc (malloc.c)
    140016    0.42  99.97   0.0112     140016     23 main (memory.c)
      6886    0.02  99.99   0.0006         11      7 sbrk (sbrk.s)

Finally, here is the source for memory.c.  Note that to do 100000 mallocs
you must run it as "memory 1000000".

#endif

#include "stdio.h"
#include "stdlib.h"
#include "malloc.h"
#include "unistd.h"
void doamalloc(long e, int setblk);

int
main(int argc, char **argv)
{
    register long e;
    int setblk = 0;

    if (argc <= 1) {
	fprintf(stderr, "Usage:%s nmallocs\n", argv[0]);
	exit(-1);
    }
    if (argc > 2)
	setblk++;
    e = atoi(argv[1])*10;
    printf("%s:#mallocs %d\n", argv[0], e/10);
    fprintf(stderr, "%s:#mallocs %d\n", argv[0], e/10);
#if AMALLOC
    doamalloc(e, setblk);
    return 0;
#else
#if LIBMALLOC
    if (setblk)
	    mallopt(M_NLBLKS, 128*1024);
#endif
    { int i;
    for (i=0; i<e && malloc(10); i+= 10) {
    }
    }
    return 0;
#endif
}

#ifdef AMALLOC
void
doamalloc(long e, int setblk)
{
	register int i;
	void *ap;
	char *buf;

	buf = sbrk(e+100);
	if ((ap = acreate(buf, e+100, 0, 0, 0)) == NULL) {
		fprintf(stderr, "acreate failed for %d bytes\n", e+100);
		return;
	}
	if (setblk)
	    amallopt(M_NLBLKS, 128*1024, ap);
	for (i=0; i<e&& amalloc(10, ap); i+= 10) {
	}
}
#endif
