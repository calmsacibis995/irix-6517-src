/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)unpack:unpack.c	1.23"
/*
 *	Huffman decompressor
 *	Usage:	pcat filename...
 *	or	unpack filename...
 */

#include <stdio.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
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

#define	CMDCLASS	"UX:"	/* Command classification */

#ifdef sgi

struct utimbuf __utimes;

#else

struct utimbuf {
	time_t	actime;		/* access time */
	time_t	modtime;	/* modification time */
} __utimes;

#endif /* sgi */

#ifdef lint
	int	_void_;
#	define VOID	_void_ = (int)
#else
#	define VOID
#endif

jmp_buf env;
struct	stat status;
char	*argv0, *argvk;
int	rmflg = 0;	/* rmflg, when set it's ok to rm arvk file on caught signals */
long	errorm;

static	const	char
	badread[]    = ":28:Read error: %s",
	badwrite[]   = ":29:Write error: %s";

#ifdef sgi
#define NAMELEN PATH_MAX
#else
#define NAMELEN 80
#endif /* sgi */
#define SUF0	'.'
#define SUF1	'z'
#define ZSUFFIX	0x40000
#define US	037
#define RS	036

/* variables associated with i/o */
char	filename[NAMELEN+2];
short	infile;
short	outfile;
short	inleft;
char	*inp;
char	*outp;
char	inbuff[BUFSIZ];
char	outbuff[BUFSIZ];

/* the dictionary */
long	origsize;
short	maxlev;
short	intnodes[25];
char	*tree[25];
char	characters[256];
char	*eof;

/* read in the dictionary portion and build decoding structures */
/* return 1 if successful, 0 otherwise */
getdict ()
{
	register int c, i, nchildren;

	/*
	 * check two-byte header
	 * get size of original file,
	 * get number of levels in maxlev,
	 * get number of leaves on level i in intnodes[i],
	 * set tree[i] to point to leaves for level i
	 */
	eof = &characters[0];

	inbuff[6] = 25;
	inleft = read (infile, &inbuff[0], BUFSIZ);
	if (inleft < 0) {
		eprintf (ZSUFFIX|MM_ERROR, badread, strerror(errno));
		return (0);
	}
	if (inbuff[0] != US)
		goto goof;

	if (inbuff[1] == US) {		/* oldstyle packing */
		if (setjmp (env))
			return (0);
		expand ();
		return (1);
	}
	if (inbuff[1] != RS)
		goto goof;

	inp = &inbuff[2];
	origsize = 0;
	for (i=0; i<4; i++)
		origsize = origsize*256 + ((*inp++) & 0377);
	maxlev = *inp++ & 0377;
	if (maxlev > 24) {
goof:		eprintf (ZSUFFIX|MM_ERROR, ":61:Not in packed format");
		return (0);
	}
	for (i=1; i<=maxlev; i++)
		intnodes[i] = *inp++ & 0377;
	for (i=1; i<=maxlev; i++) {
		tree[i] = eof;
		for (c=intnodes[i]; c>0; c--) {
			if (eof >= &characters[255])
				goto goof;
			*eof++ = *inp++;
		}
	}
	*eof++ = *inp++;
	intnodes[maxlev] += 2;
	inleft -= inp - &inbuff[0];
	if (inleft < 0)
		goto goof;

	/*
	 * convert intnodes[i] to be number of
	 * internal nodes possessed by level i
	 */

	nchildren = 0;
	for (i=maxlev; i>=1; i--) {
		c = intnodes[i];
		intnodes[i] = nchildren /= 2;
		nchildren += c;
	}
	return (decode ());
}

/* unpack the file */
/* return 1 if successful, 0 otherwise */
decode ()
{
	register int bitsleft, c, i;
	int j, lev;
	char *p;

	outp = &outbuff[0];
	lev = 1;
	i = 0;
	while (1) {
		if (inleft <= 0) {
			inleft = read (infile, inp = &inbuff[0], BUFSIZ);
			if (inleft < 0) {
				eprintf (ZSUFFIX|MM_ERROR, badread, strerror(errno));
				return (0);
			}
		}
		if (--inleft < 0) {
uggh:			eprintf (ZSUFFIX|MM_ERROR, ":62:Unpacking error");
			return (0);
		}
		c = *inp++;
		bitsleft = 8;
		while (--bitsleft >= 0) {
			i *= 2;
			if (c & 0200)
				i++;
			c <<= 1;
			if ((j = i - intnodes[lev]) >= 0) {
				p = &tree[lev][j];
				if (p == eof) {
					c = outp - &outbuff[0];
					if (write (outfile, &outbuff[0], c) != c) {
wrerr:						eprintf (MM_ERROR, badwrite,
							strerror(errno));
						return (0);
					}
					origsize -= c;
					if (origsize != 0)
						goto uggh;
					return (1);
				}
				*outp++ = *p;
				if (outp == &outbuff[BUFSIZ]) {
					if (write (outfile, outp = &outbuff[0], BUFSIZ) != BUFSIZ)
						goto wrerr;
					origsize -= BUFSIZ;
				}
				lev = 1;
				i = 0;
			} else
				lev++;
		}
	}
}

void onsig();

main (argc, argv)
	char *argv[];
{
	register i, k;
	int sep, pcat = 0;
	register char *p1, *cp;
	char label[MAXLABEL+1];	/* Space for the catalogue label */
	int fcount = 0;		/* failure count */

	(void)setlocale (LC_ALL, "");

	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		(void) signal(SIGHUP, onsig);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void) signal(SIGINT, onsig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		(void) signal(SIGTERM, onsig);

	p1 = *argv;
	while(*p1++);		/* Point p1 to end of argv[0] string */
	while(--p1 >= *argv)
		if(*p1 == '/')break;
	*argv = p1 + 1;
	argv0 = argv[0];
	(void)strcpy(label, CMDCLASS);
	(void)strncat(label, argv0, (MAXLABEL - sizeof(CMDCLASS) - 1));
	(void)setcat("uxdfm");
	(void)setlabel(label);
	if(**argv == 'p')pcat++;	/* User entered pcat (or /xx/xx/pcat) */
	for (k=1; k<argc; k++) {
		errorm = -1;
		sep = -1;
		cp = filename;
		argvk = argv[k];
		for (i=0; i < (NAMELEN-3) && (*cp = argvk[i]); i++)
			if (*cp++ == '/')
				sep = i;
		if (cp[-1] == SUF1 && cp[-2] == SUF0) {
			argvk[i-2] = '\0'; /* Remove suffix and try again */
			k--;
			continue;
		}

		fcount++;	/* expect the worst */
#ifdef sgi
		if(i>(NAMELEN-2)||(i-sep-1)>(NAME_MAX-2)) { /* } */
#else
		if (i >= (NAMELEN-3) || (i - sep) > 13) {
#endif /* sgi */
			eprintf (MM_ERROR, "uxsyserr:81:File name too long");
			goto done;
		}
		*cp++ = SUF0;
		*cp++ = SUF1;
		*cp = '\0';
		if ((infile = open (filename, O_RDONLY)) == -1) {
			eprintf (ZSUFFIX|MM_ERROR, ":64:Cannot open: %s",
				strerror(errno));
			goto done;
		}

		if (pcat)
			outfile = 1;	/* standard output */
		else {
			if (stat (argvk, &status) != -1) {
				eprintf (MM_ERROR, ":38:Already exists");
				goto done;
			}
			VOID fstat (infile, &status);
			if (status.st_nlink != 1)
				eprintf (ZSUFFIX|MM_WARNING,
					":37:File has links");
			if ((outfile = creat (argvk, status.st_mode)) == -1) {
				eprintf (MM_ERROR, ":39:Cannot create: %s",
					strerror(errno));
				goto done;
			}

			rmflg = 1;
		}

		if (getdict ()) {	/* unpack */
			fcount--; 	/* success after all */
			if (!pcat) {
				/*
				 * preserve acc & mod dates
				 */
				__utimes.actime = status.st_atime;
				__utimes.modtime = status.st_mtime;
				if(utime(argvk,&__utimes)!=0)
			    		eprintf(MM_WARNING, 
			    			":49:Cannot change times: %s", strerror (errno));
				if (chmod (argvk, status.st_mode) != 0)
			    		eprintf(MM_WARNING, 
			    			":50:Cannot change mode to %o: %s",
			    			status.st_mode, strerror (errno));
				VOID chown (argvk, status.st_uid, status.st_gid);
				rmflg = 0;
				eprintf (MM_INFO, ":63:Unpacked");
				VOID unlink (filename);

			}
		}
		else
			if (!pcat)
				VOID unlink (argvk);
done:		if (errorm != -1)
			VOID fprintf (stderr, "\n");
		VOID close (infile);
		if (!pcat)
			VOID close (outfile);
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

/*
 * This code is for unpacking files that
 * were packed using the previous algorithm.
 */

int	Tree[1024];

expand ()
{
	register tp, bit;
	short word;
	int keysize, i, *t;

	outp = outbuff;
	inp = &inbuff[2];
	inleft -= 2;
	origsize = ((long) (unsigned) getword ())*256*256;
	origsize += (unsigned) getword ();
	t = Tree;
	for (keysize = getword (); keysize--; ) {
		if ((i = getch ()) == 0377)
			*t++ = getword ();
		else
			*t++ = i & 0377;
	}

	bit = tp = 0;
	for (;;) {
		if (bit <= 0) {
			word = getword ();
			bit = 16;
		}
		tp += Tree[tp + (word<0)];
		word <<= 1;
		bit--;
		if (Tree[tp] == 0) {
			putch (Tree[tp+1]);
			tp = 0;
			if ((origsize -= 1) == 0) {
				write (outfile, outbuff, outp - outbuff);
				return;
			}
		}
	}
}

getch ()
{
	if (inleft <= 0) {
		inleft = read (infile, inp = inbuff, BUFSIZ);
		if (inleft < 0) {
			eprintf (ZSUFFIX|MM_ERROR, badread, strerror(errno));
			longjmp (env, 1);
		}
	}
	inleft--;
	return (*inp++ & 0377);
}

getword ()
{
	register char c;
	register d;
	c = getch ();
	d = getch ();
	d <<= 8;
	d |= c & 0377;
	return (d);
}

void
onsig()
{
				/* could be running as unpack or pcat	*/
				/* but rmflg is set only when running	*/
				/* as unpack and only when file is	*/
				/* created by unpack and not yet done	*/
	if (rmflg == 1)
		VOID unlink(argvk);
	exit(1);
}

putch (c)
	char c;
{
	register n;

	*outp++ = c;
	if (outp == &outbuff[BUFSIZ]) {
		n = write (outfile, outp = outbuff, BUFSIZ);
		if (n < BUFSIZ) {
			eprintf (MM_ERROR, badwrite, strerror(errno));
			longjmp (env, 2);
		}
	}
}
