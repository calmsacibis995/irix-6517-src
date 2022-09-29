/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pack:pack.c	1.16.1.4"
/*
 *	Huffman encoding program 
 *	Usage:	pack [[ - ] filename ... ] filename ...
 *		- option: enable/disable listing of statistics
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>
#include <pfmt.h>
#include <errno.h>
#include <string.h>
#ifdef sgi
#include <sys/param.h>
#include <limits.h>
#endif /* sgi */

#define	END	256
#ifdef sgi
#define NAMELEN PATH_MAX
#else
#define NAMELEN 80
#endif /* sgi */
#define PACKED 017436 /* <US><RS> - Unlikely value */
#define	SUF0	'.'
#define	SUF1	'z'
#define ZSUFFIX	0x40000

static const char badread[] = ":28:Read error: %s";

struct stat status, ostatus;

#ifdef sgi
static struct utimbuf __utimes;
#else
struct utimbuf {
	time_t actime;	/* access time */
	time_t modtime;	/* modification time */
} __utimes;
#endif /* sgi */

/* union for overlaying a long int with a set of four characters */
union FOUR {
	struct { long long lng; } lint;
	struct { char c4,c5,c6,c7,c0, c1, c2, c3; } chars;
};

/* character counters */
long long	count [END+1];
union	FOUR insize;
long long	outsize;
long dictsize;
int	diffbytes;

/* i/o stuff */
char	vflag = 0;
int	force = 0;	/* allow forced packing for consistency in directory */
char	filename [NAMELEN+1];		/* space for terminating null */
int	infile;		/* unpacked file */
int	outfile;	/* packed file */
char	inbuff [BUFSIZ];
union	{ long long dummy; char _outbuff[BUFSIZ + 8]; } dummy; /* Force alignment */
char	*outbuff = dummy._outbuff;

/* variables associated with the tree */
int	maxlev;
int	levcount [25];
int	lastnode;
int	parent [2*END+1];

/* variables associated with the encoding process */
char	length [END+1];
long long	bits [END+1];
union	FOUR mask;
long	inc;

#ifdef i386
char	*maskshuff[4]  = {&(mask.chars.c3), &(mask.chars.c2), &(mask.chars.c1), &(mask.chars.c0)};
#else
#ifdef vax
char	*maskshuff[4]  = {&(mask.chars.c3), &(mask.chars.c2), &(mask.chars.c1), &(mask.chars.c0)};
#else
#ifdef pdp11
char	*maskshuff[4]  = {&(mask.chars.c1), &(mask.chars.c0), &(mask.chars.c3), &(mask.chars.c2)};
#else	/* u370 or 3b20 */
char	*maskshuff[4]  = {&(mask.chars.c0), &(mask.chars.c1), &(mask.chars.c2), &(mask.chars.c3)};
#endif
#endif
#endif

/* the heap */
int	n;
struct	heap {
	long long count;
	int node;
} heap [END+2];
#define hmove(a,b) {(b).count = (a).count; (b).node = (a).node;}

long	errorm;		/* Error marker for eprintf() */
char	*argvk;		/* Current file under scrutiny */

/* gather character frequency statistics */
/* return 1 if successful, 0 otherwise */
input ()
{
	register int i;
	for (i=0; i<END; i++)
		count[i] = 0;
	while ((i = read(infile, inbuff, BUFSIZ)) > 0)
		while (i > 0)
			count[inbuff[--i]&0377] += 2;
	if (i == 0)
		return (1);
	eprintf (MM_ERROR, badread, strerror (errno));
	return (0);
}

/* encode the current file */
/* return 1 if successful, 0 otherwise */
output ()
{
	int c, i, inleft;
	char *inp;
	register char **q, *outp;
	register int bitsleft;
	long long temp;

	/* output ``PACKED'' header */
	outbuff[0] = 037; 	/* ascii US */
	outbuff[1] = 036; 	/* ascii RS */
	/* output the length and the dictionary */
	temp = insize.lint.lng;
	for (i=5; i>=2; i--) {
		outbuff[i] =  (char) (temp & 0377);
		temp >>= 8;
	}
	outp = &outbuff[6];
	*outp++ = maxlev;
	for (i=1; i<maxlev; i++)
		*outp++ = levcount[i];
	*outp++ = levcount[maxlev]-2;
	for (i=1; i<=maxlev; i++)
		for (c=0; c<END; c++)
			if (length[c] == i)
				*outp++ = c;
	dictsize = outp-&outbuff[0];

	/* output the text */
	lseek(infile, 0L, 0);
	outsize = 0;
	bitsleft = 8;
	inleft = 0;
	do {
		if (inleft <= 0) {
			inleft = read(infile, inp = &inbuff[0], BUFSIZ);
			if (inleft < 0) {
				eprintf (MM_ERROR, badread, strerror (errno));
				return (0);
			}
		}
		c = (--inleft < 0) ? END : (*inp++ & 0377);
		mask.lint.lng = bits[c]<<bitsleft;
		q = &maskshuff[0];
		if (bitsleft == 8)
			*outp = **q++;
		else
			*outp |= **q++;
		bitsleft -= length[c];
		while (bitsleft < 0) {
			*++outp = **q++;
			bitsleft += 8;
		}
		if (outp >= &outbuff[BUFSIZ]) {
			if (write(outfile, outbuff, BUFSIZ) != BUFSIZ) {
wrerr:
				eprintf (ZSUFFIX|MM_ERROR, ":29:Write error: %s",
					strerror (errno));
				return (0);
			}
			((union FOUR *) outbuff)->lint.lng = ((union FOUR *) &outbuff[BUFSIZ])->lint.lng;
			outp -= BUFSIZ;
			outsize += BUFSIZ;
		}
	} while (c != END);
	if (bitsleft < 8)
		outp++;
	c = outp-outbuff;
	if (write(outfile, outbuff, c) != c)
		goto wrerr;
	outsize += c;
	return (1);
}

/* makes a heap out of heap[i],...,heap[n] */
heapify (i)
{
	register int k;
	int lastparent;
	struct heap heapsubi;
	hmove (heap[i], heapsubi);
	lastparent = n/2;
	while (i <= lastparent) {
		k = 2*i;
		if (heap[k].count > heap[k+1].count && k < n)
			k++;
		if (heapsubi.count < heap[k].count)
			break;
		hmove (heap[k], heap[i]);
		i = k;
	}
	hmove (heapsubi, heap[i]);
}

/* return 1 after successful packing, 0 otherwise */
int packfile ()
{
	register int c, i, p;
	long long bitsout;

	/* gather frequency statistics */
	if (input() == 0)
		return (0);

	/* put occurring chars in heap with their counts */
	diffbytes = -1;
	count[END] = 1;
	insize.lint.lng = n = 0;
	for (i=END; i>=0; i--) {
		parent[i] = 0;
		if (count[i] > 0) {
			diffbytes++;
			insize.lint.lng += count[i];
			heap[++n].count = count[i];
			heap[n].node = i;
		}
	}
	if (diffbytes == 1) {
		eprintf (MM_INFO, ":30:Trivial file");
		return (0);
	}
	insize.lint.lng >>= 1;
	for (i=n/2; i>=1; i--)
		heapify(i);

	/* build Huffman tree */
	lastnode = END;
	while (n > 1) {
		parent[heap[1].node] = ++lastnode;
		inc = heap[1].count;
		hmove (heap[n], heap[1]);
		n--;
		heapify(1);
		parent[heap[1].node] = lastnode;
		heap[1].node = lastnode;
		heap[1].count += inc;
		heapify(1);
	}
	parent[lastnode] = 0;

	/* assign lengths to encoding for each character */
	bitsout = maxlev = 0;
	for (i=1; i<=24; i++)
		levcount[i] = 0;
	for (i=0; i<=END; i++) {
		c = 0;
		for (p=parent[i]; p!=0; p=parent[p])
			c++;
		levcount[c]++;
		length[i] = c;
		if (c > maxlev)
			maxlev = c;
		bitsout += c*(count[i]>>1);
	}
	if (maxlev > 24) {
		/* can't occur unless insize.lint.lng >= 2**24 */
		eprintf (MM_ERROR, ":31:Huffman tree has too many levels");
		return(0);
	}

	/* don't bother if no compression results */
	outsize = ((bitsout+7)>>3)+6+maxlev+diffbytes;
#ifdef sgi
	if ((insize.lint.lng+BBSIZE-1)/BBSIZE <= (outsize+BBSIZE-1)/BBSIZE
#else
	if ((insize.lint.lng+BUFSIZ-1)/BUFSIZ <= (outsize+BUFSIZ-1)/BUFSIZ
#endif /* sgi */
	    && !force) {
		eprintf (MM_INFO, ":32:No saving");
		return(0);
	}

	/* compute bit patterns for each character */
	inc = 1L << 24;
	inc >>= maxlev;
	mask.lint.lng = 0;
	for (i=maxlev; i>0; i--) {
		for (c=0; c<=END; c++)
			if (length[c] == i) {
				bits[c] = mask.lint.lng;
				mask.lint.lng += inc;
			}
		mask.lint.lng &= ~inc;
		inc <<= 1;
	}
	return (output());
}

main(argc, argv)
int argc; char *argv[];
{
	register int i;
	register char *cp;
	int k, sep;
	int fcount =0; /* count failures */

	(void)setlocale (LC_ALL, "");
	(void)setcat("uxdfm");
	(void)setlabel("UX:pack");

	errorm = -1;
	for (k=1; k<argc; k++) {
		if (argv[k][0] == '-' && argv[k][1] == '\0') {
			vflag = 1 - vflag;
			continue;
		}
		if (argv[k][0] == '-' && argv[k][1] == 'f') {
			force++;
			continue;
		}
		fcount++; /* increase failure count - expect the worst */
		argvk = argv[k];
		if (errorm != -1) {
			(void) fputc ('\n', stderr);
			errorm = -1;
		}
		sep = -1;  cp = filename;
		for (i=0; i < (NAMELEN-3) && (*cp = argv[k][i]); i++)
			if (*cp++ == '/') sep = i;
		if (cp[-1]==SUF1 && cp[-2]==SUF0) {
			eprintf (MM_ERROR, ":33:Already packed");
			continue;
		}
#ifdef sgi
		if(i>(NAMELEN-2)||((i-sep-1)>NAME_MAX-2)) {   /* } */
#else
		if (i >= (NAMELEN-3) || (i-sep) > 13) {
#endif /* sgi */
			eprintf (MM_ERROR, "uxsyserr:81:File name too long");
			continue;
		}
		if ((infile = open (filename, 0)) < 0) {
			eprintf (MM_ERROR, ":64:Cannot open: %s", strerror (errno));
			continue;
		}
		fstat(infile,&status);
		if (status.st_mode&S_IFDIR) {
			eprintf (MM_ERROR, ":35:Cannot pack a directory");
			goto closein2;
		}
		if (status.st_size == 0) {
			eprintf (MM_ERROR, ":36:Cannot pack a zero length file");
			goto closein2;
		}
		if( status.st_nlink != 1 ) {
			eprintf (MM_ERROR, ":37:File has links");
			goto closein2;
		}
		*cp++ = SUF0;  *cp++ = SUF1;  *cp = '\0';
		if( stat(filename, &ostatus) != -1) {
			eprintf (ZSUFFIX|MM_ERROR, ":38:Already exists");
			goto closein2;
		}
		if ((outfile = creat (filename, status.st_mode)) < 0) {
			eprintf (ZSUFFIX|MM_ERROR, ":39:Cannot create: %s",
				strerror (errno));
	closein2:	(void) close (infile);
			continue;
		}
		if (packfile()) {
			if (unlink(argv[k]) != 0)
				eprintf (MM_WARNING, ":40:Cannot unlink: %s",
					strerror (errno));
			fcount--;  /* success after all */
#ifdef sgi
			efprintf (MM_INFO, ":41:%.1f%% Compression\n",
#else
			eprintf (MM_INFO, ":41:%.1f%% Compression\n",
#endif /* sgi */
				((double)(-outsize+(insize.lint.lng))/(double)insize.lint.lng)*100);
			errorm = -1;	/* Done the "\n" */
			/* output statistics */
			if (vflag) {
				pfmt(stderr, MM_INFO, 
					":42:\tFrom %lld to %lld bytes\n", insize.
					lint.lng, outsize);
				pfmt(stderr, MM_NOSTD, 
					":43:\tHuffman tree has %d levels below root\n",
					maxlev);
				pfmt(stderr, MM_NOSTD, 
					":44:\t%d distinct bytes in input\n", 
					diffbytes);
				pfmt(stderr, MM_NOSTD, 
					":45:\tDictionary overhead = %ld bytes\n",
					dictsize);
				pfmt(stderr, MM_NOSTD, 
					":46:\tEffective  entropy  = %.2f bits/byte\n",
					((double) outsize / (double) insize.
					lint.lng) * 8 );
				pfmt(stderr, MM_NOSTD, 
					":47:\tAsymptotic entropy  = %.2f bits/byte\n",
					((double) (outsize-dictsize) / (double)
					insize.lint.lng) * 8 );
			}
		}
		else
		{       eprintf (MM_INFO, ":48:File unchanged");
			(void) close (outfile);
			(void) close (infile);
			unlink(filename);
			continue;
		}

      closein:	close (outfile);
		close (infile);
		__utimes.actime = status.st_atime;
		__utimes.modtime = status.st_mtime;
		if(utime(filename, &__utimes)!=0) {
			eprintf(ZSUFFIX|MM_WARNING, ":49:Cannot change times: %s",
				strerror (errno));
		}
		if (chmod (filename, status.st_mode) != 0) {
			eprintf(ZSUFFIX|MM_WARNING, ":50:Cannot change mode to %o: %s",
				status.st_mode, strerror (errno));
		}
		chown (filename, status.st_uid, status.st_gid);
	}
	if (errorm != -1) {
		(void) fputc ('\n', stderr);
		errorm = -1;
	}
	return (fcount);
}

eprintf (flag, s, a1, a2)
	int  flag;
	char *s, *a1, *a2;
{
	int loc_flag = flag & ~ZSUFFIX;
	if (errorm == -1 || errorm != flag) {
		if (errorm != -1)
			fprintf(stderr, "\n");
		errorm = flag;
		pfmt(stderr, (loc_flag | MM_NOGET),
			flag & ZSUFFIX ? "%s.z" : "%s", argvk);
	}
	pfmt(stderr, MM_NOSTD, "uxsyserr:2:: ");
	pfmt(stderr, MM_NOSTD, s, a1, a2);
}

#ifdef sgi

efprintf (flag, s, a1, a2)
	int  flag;
	char *s;
	double a1, a2;
{
	int loc_flag = flag & ~ZSUFFIX;
	if (errorm == -1 || errorm != flag) {
		if (errorm != -1)
			fprintf(stderr, "\n");
		errorm = flag;
		pfmt(stderr, (loc_flag | MM_NOGET),
			flag & ZSUFFIX ? "%s.z" : "%s", argvk);
	}
	pfmt(stderr, MM_NOSTD, "uxsyserr:2:: ");
	pfmt(stderr, MM_NOSTD, s, a1, a2);
}

#endif /* sgi */
