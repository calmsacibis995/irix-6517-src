#ifdef SCCSID
static char	*SccsId = "@(#)compress.c	1.14	9/24/87";
#endif /* SCCSID */
static char rcs_ident[] = "Based on compress.c,v 4.0 85/07/30 12:50:00 joe Release";

/* 
 * Compress - data compression program 
 */
#define	min(a,b)	((a>b) ? b : a)

/*
 * machine variants which require cc -Dmachine:  pdp11, z8000, pcxt
 */

/*
 * Set USERMEM to the maximum amount of physical user memory available
 * in bytes.  USERMEM is used to determine the maximum BITS that can be used
 * for compression.
 *
 * SACREDMEM is the amount of physical memory saved for others; compress
 * will hog the rest.
 */
#ifndef SACREDMEM
#define SACREDMEM	0
#endif

#ifndef USERMEM
# define USERMEM 	450000	/* default user memory */
#endif

#ifdef interdata		/* (Perkin-Elmer) */
#define SIGNED_COMPARE_SLOW	/* signed compare is slower than unsigned */
#endif

#ifdef pdp11
# define BITS 	12	/* max bits/code for 16-bit machine */
# define NO_UCHAR	/* also if "unsigned char" functions as signed char */
# undef USERMEM 
#endif /* pdp11 */	/* don't forget to compile with -i */

#ifdef z8000
# define BITS 	12
# undef vax		/* weird preprocessor */
# undef USERMEM 
#endif /* z8000 */

#ifdef pcxt
# define BITS   12
# undef USERMEM
#endif /* pcxt */

#ifdef USERMEM
# if USERMEM >= (433484+SACREDMEM)
#  define PBITS	16
# else
#  if USERMEM >= (229600+SACREDMEM)
#   define PBITS	15
#  else
#   if USERMEM >= (127536+SACREDMEM)
#    define PBITS	14
#   else
#    if USERMEM >= (73464+SACREDMEM)
#     define PBITS	13
#    else
#     define PBITS	12
#    endif
#   endif
#  endif
# endif
# undef USERMEM
#endif /* USERMEM */

#ifdef PBITS		/* Preferred BITS for this memory size */
# ifndef BITS
#  define BITS PBITS
# endif /* BITS */
#endif /* PBITS */

#if BITS == 16
# define HSIZE	69001		/* 95% occupancy */
#endif
#if BITS == 15
# define HSIZE	35023		/* 94% occupancy */
#endif
#if BITS == 14
# define HSIZE	18013		/* 91% occupancy */
#endif
#if BITS == 13
# define HSIZE	9001		/* 91% occupancy */
#endif
#if BITS <= 12
# define HSIZE	5003		/* 80% occupancy */
#endif

#ifdef M_XENIX			/* Stupid compiler can't handle arrays with */
# if BITS == 16			/* more than 65535 bytes - so we fake it */
#  define XENIX_16
# else
#  if BITS > 13			/* Code only handles BITS = 12, 13, or 16 */
#   define BITS	13
#  endif
# endif
#endif

/*
 * a code_int must be able to hold 2**BITS values of type int, and also -1
 */
#if BITS > 15
typedef int		code_int;
#else
typedef int		code_int;
#endif

#ifdef SIGNED_COMPARE_SLOW
typedef unsigned int count_int;
typedef unsigned short int count_short;
#else
typedef int	  count_int;
#endif

#ifdef NO_UCHAR
 typedef char	char_type;
#else
 typedef	unsigned char	char_type;
#endif /* UCHAR */
char_type magic_header[] = { "\037\235" };	/* 1F 9D */

/* Defines for third byte of header */
#define BIT_MASK	0x1f
#define BLOCK_MASK	0x80
/* Masks 0x40 and 0x20 are free.  I think 0x20 should mean that there is
   a fourth header byte (for expansion).
*/
#define INIT_BITS 9			/* initial number of bits/code */

/*
 * compress.c - File compression ala IEEE Computer, June 1984.
 *
 * Authors:	Spencer W. Thomas	(decvax!harpo!utah-cs!utah-gr!thomas)
 *		Jim McKie		(decvax!mcvax!jim)
 *		Steve Davies		(decvax!vax135!petsd!peora!srd)
 *		Ken Turkowski		(decvax!decwrl!turtlevax!ken)
 *		James A. Woods		(decvax!ihnp4!ames!jaw)
 *		Joe Orost		(decvax!vax135!petsd!joe)
 *
 */
#ifdef sgi
#include <limits.h>
#define _BSD_SIGNALS
#endif
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _STANDALONE

code_int	getcode(void);

#include "lzw.h"

#define writeerr()	return -1
#ifdef ferror
#undef ferror
#endif /* ferror */
#define ferror(_fp)	(((_fp == stdin) && (in_next >= in_size)) || \
			 ((_fp == stdout) && (out_next >= out_size)))
#define fflush(_x)	;

char *in_buf;
char *out_buf;
int  in_next;
int  out_next;
int  in_size;
int  out_size;

#ifdef CHECKSUM

uint in_checksum;
uint out_checksum;

#endif

#ifdef getchar
#undef getchar
#endif

#ifdef putchar
#undef putchar
#endif

#define getchar()	(in_next < in_size) ? in_buf[in_next++] : -1
#define putchar(_x)	(out_next < out_size) ? out_buf[out_next++] = (_x) : -1
#define fread(_buf, _size, _num, _fd)	myread(_buf, _size, _num)
#define fwrite(_buf, _size, _num, _fd)	mywrite(_buf, _size, _num)

#ifdef DECOMPRESS_ONLY

/*
 * We're the segment loader version of compress.  Load appropriate defines
 */
#include "sl.h"

/*
 * Define stdio stuff since we won't really have stdio.
 */
#define fprintf(_a, _b)		loprintf(_b)

#ifdef stdin
#undef stdin
#undef stdout
#undef stderr
#endif /* stdin */

#define stdin	1
#define stdout	2
#define stderr	3

void
bcopy(char *src, char *dst, int count)
{
    while (count--)
	*dst++ = *src++;
}

#endif /* DECOMPRESS_ONLY */

int myread(char *buf, int size, int num)
{
	int numread;

	if (in_next + (size * num) > in_size)
		numread = (in_size - in_next) / size;
	else
		numread = num;

	if (numread)
		bcopy(in_buf + in_next, buf, numread * size);

	in_next += numread * size;

	return numread;
}

int mywrite(char *buf, int size, int num)
{
	int numwritten;

	if (out_next + (size * num) > in_size)
		numwritten = (out_size - out_next) / size;
	else
		numwritten = num;

	if (numwritten)
		bcopy(buf, out_buf + out_next, numwritten * size);

	out_next += numwritten * size;

	return numwritten;
}

#endif /* _STANDALONE */

#define ARGVAL() (*++(*argv) || (--argc && *++argv))

int suser = 0;
int n_bits;				/* number of bits/code */
int maxbits = BITS;			/* user settable max # bits/code */
code_int maxcode;			/* maximum code, given n_bits */
code_int maxmaxcode = 1L << BITS;	/* should NEVER generate this code */
#ifdef COMPATIBLE		/* But wrong! */
# define MAXCODE(n_bits)	(1L << (n_bits) - 1)
#else
# define MAXCODE(n_bits)	((code_int)((1L << (n_bits)) - 1))
#endif /* COMPATIBLE */

#ifdef XENIX_16
count_int htab0[8192];
count_int htab1[8192];
count_int htab2[8192];
count_int htab3[8192];
count_int htab4[8192];
count_int htab5[8192];
count_int htab6[8192];
count_int htab7[8192];
count_int htab8[HSIZE-65536];
count_int * htab[9] = {
	htab0, htab1, htab2, htab3, htab4, htab5, htab6, htab7, htab8 };

#define htabof(i)	(htab[(i) >> 13][(i) & 0x1fff])
unsigned short code0tab[16384];
unsigned short code1tab[16384];
unsigned short code2tab[16384];
unsigned short code3tab[16384];
unsigned short code4tab[16384];
unsigned short * codetab[5] = {
	code0tab, code1tab, code2tab, code3tab, code4tab };

#define codetabof(i)	(codetab[(i) >> 14][(i) & 0x3fff])

#else	/* Normal machine */

#ifndef DECOMPRESS_ONLY
# ifdef sel
/* support gould base register problems */
/*NOBASE*/
count_int htab [HSIZE];
unsigned short codetab [HSIZE];
/*NOBASE*/
# else /* !gould */
count_int htab [HSIZE];
unsigned short codetab [HSIZE];
# endif /* !gould */

#else /* DECOMPRESS_ONLY */

count_int *htab;
unsigned short *codetab;

#endif /* DECOMPRESS_ONLY */

#define htabof(i)	htab[i]
#define codetabof(i)	codetab[i]
#endif	/* !XENIX_16 */
code_int hsize = HSIZE;			/* for dynamic table sizing */
count_int fsize;

/*
 * To save much memory, we overlay the table used by compress() with those
 * used by decompress().  The tab_prefix table is the same size and type
 * as the codetab.  The tab_suffix table needs 2**BITS characters.  We
 * get this from the beginning of htab.  The output stack uses the rest
 * of htab, and contains characters.  There is plenty of room for any
 * possible stack (stack used to be 8000 characters).
 */

#define tab_prefixof(i)	codetabof(i)
#ifdef XENIX_16
# define tab_suffixof(i)	((char_type *)htab[(i)>>15])[(i) & 0x7fff]
# define de_stack		((char_type *)(htab2))
#else	/* Normal machine */
# define tab_suffixof(i)	((char_type *)(htab))[i]
# define de_stack		((char_type *)&tab_suffixof(1<<BITS))
#endif	/* XENIX_16 */

code_int free_ent = 0;			/* first unused entry */
int exit_stat = 0;			/* per-file status */
int perm_stat = 0;			/* permanent status */

code_int getcode(void);

#ifndef _STANDALONE

Usage() {
#ifdef DEBUG
fprintf(stderr,"Usage: compress [-dDVfc] [-b maxbits] [file ...]\n");
}
int debug = 0;
#else
fprintf(stderr,"Usage: compress [-dfvcV] [-b maxbits] [file ...]\n");
}
#endif /* DEBUG */

#endif /* _STANDALONE */

int nomagic = 0;	/* Use a 3-byte magic number header, unless old file */
int zcat_flg = 0;	/* Write output on stdout, suppress messages */
int precious = 1;	/* Don't unlink output file on interrupt */
int quiet = 1;		/* don't tell me about compression */

/*
 * block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */
int block_compress = BLOCK_MASK;
int clear_flg = 0;
int ratio = 0;
#define CHECK_GAP 10000	/* ratio check interval */
count_int checkpoint = CHECK_GAP;
/*
 * the next two codes should not be changed lightly, as they must not
 * lie within the contiguous general code space.
 */ 
#define FIRST	257	/* first free entry */
#define	CLEAR	256	/* table clear output code */

int force = 0;

#ifdef DEBUG
int verbose = 0;
#endif /* DEBUG */
#ifdef sgi
void (*bgnd_flag)();
#else
int (*bgnd_flag)();
#endif

#ifndef _STANDALONE

#ifdef sgi
char ofname [PATH_MAX];
#else
char ofname [100];
#endif

int do_decomp = 0;

/*****************************************************************
 * TAG( main )
 *
 * Algorithm from "A Technique for High Performance Data Compression",
 * Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 *
 * Usage: compress [-dfvc] [-b bits] [file ...]
 * Inputs:
 *	-d:	    If given, decompression is done instead.
 *
 *      -c:         Write output on stdout, don't remove original.
 *
 *      -b:         Parameter limits the max number of bits/code.
 *
 *	-f:	    Forces output file to be generated, even if one already
 *		    exists, and even if no space is saved by compressing.
 *		    If -f is not used, the user will be prompted if stdin is
 *		    a tty, otherwise, the output file will not be overwritten.
 *
 *      -v:	    Write compression statistics
 *
 * 	file ...:   Files to be compressed.  If none specified, stdin
 *		    is used.
 * Outputs:
 *	file.Z:	    Compressed form of file with same mode, owner, and utimes
 * 	or stdout   (if stdin used as input)
 *
 * Assumptions:
 *	When filenames are given, replaces with the compressed version
 *	(.Z suffix) only if the file decreases in size.
 * Algorithm:
 * 	Modified Lempel-Ziv method (LZW).  Basically finds common
 * substrings and replaces them with a variable size code.  This is
 * deterministic, and can be done on the fly.  Thus, the decompression
 * procedure needs no input table, but tracks the way the table was built.
 */

main( argc, argv )
register int argc; char **argv;
{
    int overwrite = 0;	/* Do not overwrite unless given -f flag */
#ifdef sgi
    char tempname[PATH_MAX];
#else
    char tempname[100];
#endif
    char **filelist, **fileptr;
    char *cp, *rindex(), *malloc();
    struct stat statbuf;
#ifdef sgi
    extern void onintr(), oops();
#else
    extern onintr(), oops();
#endif


    if (geteuid() == 0)
	suser++;
    if ( (bgnd_flag = signal ( SIGINT, SIG_IGN )) != SIG_IGN ) {
	signal ( SIGINT, onintr );
	signal ( SIGSEGV, oops );
    }

#ifdef COMPATIBLE
    nomagic = 1;	/* Original didn't have a magic number */
#endif /* COMPATIBLE */

    filelist = fileptr = (char **)(malloc(argc * sizeof(*argv)));
    *filelist = NULL;

    if((cp = rindex(argv[0], '/')) != 0) {
	cp++;
    } else {
	cp = argv[0];
    }
    if(strcmp(cp, "uncompress") == 0) {
	do_decomp = 1;
    } else if(strcmp(cp, "zcat") == 0) {
	do_decomp = 1;
	zcat_flg = 1;
    }

#ifdef BSD4_2
    /* 4.2BSD dependent - take it out if not */
    setlinebuf( stderr );
#endif /* BSD4_2 */

    /* Argument Processing
     * All flags are optional.
     * -D => debug
     * -V => print Version; debug verbose
     * -d => do_decomp
     * -v => unquiet
     * -f => force overwrite of output file
     * -n => no header: useful to uncompress old files
     * -b maxbits => maxbits.  If -b is specified, then maxbits MUST be
     *	    given also.
     * -c => cat all output to stdout
     * -C => generate output compatible with compress 2.0.
     * if a string is left, must be an input filename.
     */
    for (argc--, argv++; argc > 0; argc--, argv++) {
	if (**argv == '-') {	/* A flag argument */
	    while (*++(*argv)) {	/* Process all flags in this arg */
		switch (**argv) {
#ifdef DEBUG
		    case 'D':
			debug = 1;
			break;
		    case 'V':
			verbose = 1;
			version();
			break;
#else
		    case 'V':
			version();
			break;
#endif /* DEBUG */
		    case 'v':
			quiet = 0;
			break;
		    case 'd':
			do_decomp = 1;
			break;
		    case 'f':
		    case 'F':
			overwrite = 1;
			force = 1;
			break;
		    case 'n':
			nomagic = 1;
			break;
		    case 'C':
			block_compress = 0;
			break;
		    case 'b':
			if (!ARGVAL()) {
			    fprintf(stderr, "Missing maxbits\n");
			    Usage();
			    exit(1);
			}
			maxbits = atoi(*argv);
			goto nextarg;
		    case 'c':
			zcat_flg = 1;
			break;
		    case 'q':
			quiet = 1;
			break;
		    default:
			fprintf(stderr, "Unknown flag: '%c'; ", **argv);
			Usage();
			exit(1);
		}
	    }
	}
	else {		/* Input file name */
	    *fileptr++ = *argv;	/* Build input file list */
	    *fileptr = NULL;
	    /* process nextarg; */
	}
	nextarg: continue;
    }

    if(maxbits < INIT_BITS) maxbits = INIT_BITS;
    if (maxbits > BITS) maxbits = BITS;
    maxmaxcode = 1L << maxbits;

    if (*filelist != NULL) {
	for (fileptr = filelist; *fileptr; fileptr++) {
	    exit_stat = 0;
	    if (do_decomp != 0) {			/* DECOMPRESSION */
		/* Check for .Z suffix */
		if (strcmp(*fileptr + strlen(*fileptr) - 2, ".Z") != 0) {
		    /* No .Z: tack one on */
		    strcpy(tempname, *fileptr);
		    strcat(tempname, ".Z");
		    *fileptr = tempname;
		}
		/* Open input file */
		if ((freopen(*fileptr, "r", stdin)) == NULL) {
		    perror(*fileptr);
		    perm_stat = 1;
		    continue;
		}
		/* Check the magic number */
		if (nomagic == 0) {
		    if ((getchar() != (magic_header[0] & 0xFF))
		     || (getchar() != (magic_header[1] & 0xFF))) {
			fprintf(stderr, "%s: not in compressed format\n",
			    *fileptr);
			perm_stat = 1;
		        continue;
		    }
		    maxbits = getchar();	/* set -b from file */
		    block_compress = maxbits & BLOCK_MASK;
		    maxbits &= BIT_MASK;
		    maxmaxcode = 1L << maxbits;
		    if(maxbits > BITS) {
			fprintf(stderr,
			"%s: compressed with %d bits, can only handle %d bits\n",
			*fileptr, maxbits, BITS);
			perm_stat = 1;
			continue;
		    }
		}
		/* Generate output filename */
		strcpy(ofname, *fileptr);
		ofname[strlen(*fileptr) - 2] = '\0';  /* Strip off .Z */
	    } else {					/* COMPRESSION */
		if (strcmp(*fileptr + strlen(*fileptr) - 2, ".Z") == 0) {
		    	fprintf(stderr, "%s: already has .Z suffix -- no change\n",
			    *fileptr);
			perm_stat = 1;
		        continue;
		}
		/* Open input file */
		if ((freopen(*fileptr, "r", stdin)) == NULL) {
		    perror(*fileptr);
		    perm_stat = 1;
		    continue;
		}
		stat ( *fileptr, &statbuf );
		fsize = (int) statbuf.st_size;
		/*
		 * tune hash table size for small files -- ad hoc,
		 * but the sizes match earlier #defines, which
		 * serve as upper bounds on the number of output codes. 
		 */
		hsize = HSIZE;
		if ( fsize < (1 << 12) )
		    hsize = min ( 5003, HSIZE );
		else if ( fsize < (1 << 13) )
		    hsize = min ( 9001, HSIZE );
		else if ( fsize < (1 << 14) )
		    hsize = min ( 18013, HSIZE );
		else if ( fsize < (1 << 15) )
		    hsize = min ( 35023, HSIZE );
		else if ( fsize < 47000 )
		    hsize = min ( 50021, HSIZE );

		/* Generate output filename */
		strcpy(ofname, *fileptr);
#ifdef SVR3
#ifndef BSD4_2		/* Short filenames */
		if ((cp=rindex(ofname,'/')) != NULL)	cp++;
		else					cp = ofname;
		if (strlen(cp) > 12) {
		    fprintf(stderr,"%s: filename too long to tack on .Z\n",cp);
		    continue;
		}
#endif  /* BSD4_2		Long filenames allowed */
#endif /* SVR3 */
		strcat(ofname, ".Z");
	    }
	    /* Check for overwrite of existing file */
	    if (overwrite == 0 && zcat_flg == 0) {
		if (stat(ofname, &statbuf) == 0) {
		    char response[2];
		    response[0] = 'n';
		    fprintf(stderr, "%s already exists;", ofname);
		    if (foreground()) {
			fprintf(stderr, " do you wish to overwrite %s (y or n)? ",
			ofname);
			fflush(stderr);
			read(2, response, 2);
			while (response[1] != '\n') {
			    if (read(2, response+1, 1) < 0) {	/* Ack! */
				perror("stderr"); break;
			    }
			}
		    }
		    if (response[0] != 'y') {
			fprintf(stderr, "\tnot overwritten\n");
			continue;
		    }
		}
	    }
	    if(zcat_flg == 0) {		/* Open output file */
		if (freopen(ofname, "w", stdout) == NULL) {
		    perror(ofname);
		    perm_stat = 1;
		    continue;
		}
		precious = 0;
		if(!quiet)
			fprintf(stderr, "%s: ", *fileptr);
	    }

	    /* Actually do the compression/decompression */
	    if (do_decomp == 0)	compress();
#ifndef DEBUG
	    else			decompress();
#else
	    else if (debug == 0)	decompress();
	    else			printcodes();
	    if (verbose)		dump_tab();
#endif /* DEBUG */
	    if(zcat_flg == 0) {
		copystat(*fileptr, ofname);	/* Copy stats */
		precious = 1;
		if((exit_stat == 1) || (!quiet))
			putc('\n', stderr);
	    }
	}
    } else {		/* Standard input */
	if (do_decomp == 0) {
		compress();
#ifdef DEBUG
		if(verbose)		dump_tab();
#endif /* DEBUG */
		if(!quiet)
			putc('\n', stderr);
	} else {
	    /* Check the magic number */
	    if (nomagic == 0) {
		if ((getchar()!=(magic_header[0] & 0xFF))
		 || (getchar()!=(magic_header[1] & 0xFF))) {
		    fprintf(stderr, "stdin: not in compressed format\n");
		    exit(1);
		}
		maxbits = getchar();	/* set -b from file */
		block_compress = maxbits & BLOCK_MASK;
		maxbits &= BIT_MASK;
		maxmaxcode = 1L << maxbits;
		fsize = 100000;		/* assume stdin large for USERMEM */
		if(maxbits > BITS) {
			fprintf(stderr,
			"stdin: compressed with %d bits, can only handle %d bits\n",
			maxbits, BITS);
			exit(1);
		}
	    }
#ifndef DEBUG
	    decompress();
#else
	    if (debug == 0)	decompress();
	    else		printcodes();
	    if (verbose)	dump_tab();
#endif /* DEBUG */
	}
    }
    exit(perm_stat ? perm_stat : exit_stat);
    /* NOTREACHED */
}

#endif /* !_STANDALONE */


#ifndef DECOMPRESS_ONLY

static int offset;
int in_count = 1;			/* length of input */
int bytes_out;			/* length of compressed output */
int out_count = 0;			/* # of codes output (for debugging) */

/*
 * compress stdin to stdout
 *
 * Algorithm:  use open addressing double hashing (no chaining) on the 
 * prefix code / next character combination.  We do a variant of Knuth's
 * algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 * secondary probe.  Here, the modular division first probe is gives way
 * to a faster exclusive-or manipulation.  Also do block compression with
 * an adaptive reset, whereby the code table is cleared when the compression
 * ratio decreases, but after the table fills.  The variable-length output
 * codes are re-sized at this point, and a special CLEAR code is generated
 * for the decompressor.  Late addition:  construct the table according to
 * file size for noticeable speed improvement on small files.  Please direct
 * questions about this implementation to ames!jaw.
 */

#ifdef _STANDALONE
int
compress() {
#else
compress() {
#endif
    register int fcode;
    register code_int i = 0;
    register int c;
    register code_int ent;
#ifdef XENIX_16
    register code_int disp;
#else	/* Normal machine */
    register int disp;
#endif
    register code_int hsize_reg;
    register int hshift;

#ifndef COMPATIBLE
    if (nomagic == 0) {
	putchar(magic_header[0]); putchar(magic_header[1]);
	putchar((char)(maxbits | block_compress));
	if(ferror(stdout))
		writeerr();
    }
#endif /* COMPATIBLE */

    offset = 0;
    bytes_out = 3;		/* includes 3-byte header mojo */
    out_count = 0;
    clear_flg = 0;
    ratio = 0;
    in_count = 1;
    checkpoint = CHECK_GAP;
    maxcode = MAXCODE(n_bits = INIT_BITS);
    free_ent = ((block_compress) ? FIRST : 256 );

    ent = getchar ();
#ifdef CHECKSUM
    in_checksum += ent;
#endif

    hshift = 0;
    for ( fcode = (int) hsize;  fcode < 65536L; fcode *= 2L )
    	hshift++;
    hshift = 8 - hshift;		/* set hash code range bound */

    hsize_reg = hsize;
    cl_hash( (count_int) hsize_reg);		/* clear hash table */

#ifdef SIGNED_COMPARE_SLOW
    while ( (c = getchar()) != (unsigned) EOF ) {
#else
    while ( (c = getchar()) != EOF ) {
#endif
#ifdef CHECKSUM
	in_checksum += c;
#endif
	in_count++;
	fcode = (int) (((int) c << maxbits) + ent);
 	i = (((int)c << hshift) ^ ent);	/* xor hashing */

	if ( htabof (i) == fcode ) {
	    ent = codetabof (i);
	    continue;
	} else if ( (int)htabof (i) < 0 )	/* empty slot */
	    goto nomatch;
 	disp = hsize_reg - i;		/* secondary hash (after G. Knott) */
	if ( i == 0 )
	    disp = 1;
probe:
	if ( (i -= disp) < 0 )
	    i += hsize_reg;

	if ( htabof (i) == fcode ) {
	    ent = codetabof (i);
	    continue;
	}
	if ( (int)htabof (i) > 0 ) 
	    goto probe;
nomatch:
	output ( (code_int) ent );
	out_count++;
 	ent = c;
#ifdef SIGNED_COMPARE_SLOW
	if ( (unsigned) free_ent < (unsigned) maxmaxcode) {
#else
	if ( free_ent < maxmaxcode ) {
#endif
 	    codetabof (i) = free_ent++;	/* code -> hashtable */
	    htabof (i) = fcode;
	}
	else if ( (count_int)in_count >= checkpoint && block_compress )
	    cl_block ();
    }
    /*
     * Put out the final code.
     */
    output( (code_int)ent );
    out_count++;
    output( (code_int)-1 );

    /*
     * Print out stats on stderr
     */
    if(zcat_flg == 0 && !quiet) {
#ifdef DEBUG
	fprintf( stderr,
		"%ld chars in, %ld codes (%ld bytes) out, compression factor: ",
		in_count, out_count, bytes_out );
	prratio( stderr, in_count, bytes_out );
	fprintf( stderr, "\n");
	fprintf( stderr, "\tCompression as in compact: " );
	prratio( stderr, in_count-bytes_out, in_count );
	fprintf( stderr, "\n");
	fprintf( stderr, "\tLargest code (of last block) was %d (%d bits)\n",
		free_ent - 1, n_bits );
#else /* !DEBUG */
	fprintf( stderr, "Compression: " );
	prratio( stderr, in_count-bytes_out, in_count );
#endif /* DEBUG */
    }
    if(bytes_out > in_count)	/* exit(2) if no savings */
	exit_stat = 2;
#ifdef _STANDALONE
    return 0;
#else
    return;
#endif
}

#endif /* DECOMPRESS_ONLY */

/*****************************************************************
 * TAG( output )
 *
 * Output the given code.
 * Inputs:
 * 	code:	A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *		that n_bits =< (int)wordsize - 1.
 * Outputs:
 * 	Outputs code to the file.
 * Assumptions:
 *	Chars are 8 bits long.
 * Algorithm:
 * 	Maintain a BITS character long buffer (so that 8 codes will
 * fit in it exactly).  Use the VAX insv instruction to insert each
 * code in turn.  When the buffer fills up empty it and start over.
 */

static char loc_buf[BITS];

#ifndef vax
char_type lmask[9] = {0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00};
char_type rmask[9] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};
#endif /* vax */

#ifndef DECOMPRESS_ONLY

output( code )
code_int  code;
{
#ifdef DEBUG
    static int col = 0;
#endif /* DEBUG */

    /*
     * On the VAX, it is important to have the register declarations
     * in exactly the order given, or the asm will break.
     */
    register int r_off = offset, bits= n_bits;
    register char * bp = loc_buf;

#ifdef DEBUG
	if ( verbose )
	    fprintf( stderr, "%5d%c", code,
		    (col+=6) >= 74 ? (col = 0, '\n') : ' ' );
#endif /* DEBUG */
    if ( code >= 0 ) {
#ifdef vax
	/* VAX DEPENDENT!! Implementation on other machines is below.
	 *
	 * Translation: Insert BITS bits from the argument starting at
	 * offset bits from the beginning of loc_buf.
	 */
	0;	/* Work around for pcc -O bug with asm and if stmt */
	asm( "insv	4(ap),r11,r10,(r9)" );
#else /* not a vax */
/* 
 * byte/bit numbering on the VAX is simulated by the following code
 */
	/*
	 * Get to the first byte.
	 */
	bp += (r_off >> 3);
	r_off &= 7;
	/*
	 * Since code is always >= 8 bits, only need to mask the first
	 * hunk on the left.
	 */
	*bp = (*bp & rmask[r_off]) | (code << r_off) & lmask[r_off];
	bp++;
	bits -= (8 - r_off);
	code >>= 8 - r_off;
	/* Get any 8 bit parts in the middle (<=1 for up to 16 bits). */
	if ( bits >= 8 ) {
	    *bp++ = code;
	    code >>= 8;
	    bits -= 8;
	}
	/* Last bits. */
	if(bits)
	    *bp = code;
#endif /* vax */
	offset += n_bits;
	if ( offset == (n_bits << 3) ) {
	    bp = loc_buf;
	    bits = n_bits;
	    bytes_out += bits;
	    do
		putchar(*bp++);
	    while(--bits);
	    offset = 0;
	}

	/*
	 * If the next entry is going to be too big for the code size,
	 * then increase it, if possible.
	 */
	if ( free_ent > maxcode || (clear_flg > 0))
	{
	    /*
	     * Write the whole buffer, because the input side won't
	     * discover the size increase until after it has read it.
	     */
	    if ( offset > 0 ) {
		if( fwrite( loc_buf, 1, n_bits, stdout ) != n_bits)
			writeerr();
		bytes_out += n_bits;
	    }
	    offset = 0;

	    if ( clear_flg ) {
    	        maxcode = MAXCODE (n_bits = INIT_BITS);
	        clear_flg = 0;
	    }
	    else {
	    	n_bits++;
	    	if ( n_bits == maxbits )
		    maxcode = maxmaxcode;
	    	else
		    maxcode = MAXCODE(n_bits);
	    }
#ifdef DEBUG
	    if ( debug ) {
		fprintf( stderr, "\nChange to %d bits\n", n_bits );
		col = 0;
	    }
#endif /* DEBUG */
	}
    } else {
	/*
	 * At EOF, write the rest of the buffer.
	 */
	if ( offset > 0 )
	    fwrite( loc_buf, 1, (offset + 7) / 8, stdout );
	bytes_out += (offset + 7) / 8;
	offset = 0;
	fflush( stdout );
#ifdef DEBUG
	if ( verbose )
	    fprintf( stderr, "\n" );
#endif /* DEBUG */
	if( ferror( stdout ) )
		writeerr();
    }
}

#endif /* ! DECOMPRESS_ONLY */

/*
 * Decompress stdin to stdout.  This routine adapts to the codes in the
 * file building the "string" table on-the-fly; requiring no table to
 * be stored in the compressed file.  The tables used herein are shared
 * with those of the compress() routine.  See the definitions above.
 */

int decompress(void) {
    register char_type *stackp;
    register int finchar;
    register code_int code, oldcode, incode;

    /*
     * As above, initialize the first 256 entries in the table.
     */
    maxcode = MAXCODE(n_bits = INIT_BITS);
    for ( code = 255; code >= 0; code-- ) {
	tab_prefixof(code) = 0;
	tab_suffixof(code) = (char_type)code;
    }
    free_ent = ((block_compress) ? FIRST : 256 );

    finchar = oldcode = getcode();
    if(oldcode == -1)	/* EOF already? */
#ifdef	_STANDALONE
	return 0;
#else
	return;			/* Get out of here */
#endif
    putchar( (char)finchar );		/* first code must be 8 bits = char */
#ifdef CHECKSUM
    out_checksum += finchar;
#endif /* CHECKSUM */
    if(ferror(stdout))		/* Crash if can't write */
	writeerr();
    stackp = de_stack;

    while ( (code = getcode()) > -1 ) {

	if ( (code == CLEAR) && block_compress ) {
	    for ( code = 255; code >= 0; code-- )
		tab_prefixof(code) = 0;
	    clear_flg = 1;
	    free_ent = FIRST - 1;
	    if ( (code = getcode ()) == -1 )	/* O, untimely death! */
		break;
	}
	incode = code;
	/*
	 * Special case for KwKwK string.
	 */
	if ( code >= free_ent ) {
            *stackp++ = finchar;
	    code = oldcode;
	}

	/*
	 * Generate output characters in reverse order
	 */
#ifdef SIGNED_COMPARE_SLOW
	while ( ((unsigned int)code) >= ((unsigned int)256) ) {
#else
	while ( code >= 256 ) {
#endif
	    *stackp++ = tab_suffixof(code);
	    code = tab_prefixof(code);
	}
	*stackp++ = finchar = tab_suffixof(code);

	/*
	 * And put them out in forward order
	 */
	do {
#ifdef CHECKSUM
	    out_checksum += *(stackp - 1);
#endif /* CHECKSUM */
	    putchar ( *--stackp );
	} while ( stackp > de_stack );

	/*
	 * Generate the new entry.
	 */
	if ( (code=free_ent) < maxmaxcode ) {
	    tab_prefixof(code) = (unsigned short)oldcode;
	    tab_suffixof(code) = finchar;
	    free_ent = code+1;
	} 
	/*
	 * Remember previous code.
	 */
	oldcode = incode;
    }
    fflush( stdout );
    if(ferror(stdout))
	writeerr();
#ifdef _STANDALONE
    return 0;
#endif
}

/*****************************************************************
 * TAG( getcode )
 *
 * Read one code from the standard input.  If EOF, return -1.
 * Inputs:
 * 	stdin
 * Outputs:
 * 	code or -1 is returned.
 */

code_int
getcode(void) {
    /*
     * On the VAX, it is important to have the register declarations
     * in exactly the order given, or the asm will break.
     */
    register code_int code;
    static int offset = 0, size = 0;
    static char_type buf[BITS];
    register int r_off, bits;
    register char_type *bp = buf;

    if ( clear_flg > 0 || offset >= size || free_ent > maxcode ) {
	/*
	 * If the next entry will be too big for the current code
	 * size, then we must increase the size.  This implies reading
	 * a new buffer full, too.
	 */
	if ( free_ent > maxcode ) {
	    n_bits++;
	    if ( n_bits == maxbits )
		maxcode = maxmaxcode;	/* won't get any bigger now */
	    else
		maxcode = MAXCODE(n_bits);
	}
	if ( clear_flg > 0) {
    	    maxcode = MAXCODE (n_bits = INIT_BITS);
	    clear_flg = 0;
	}
	size = fread( (char *)buf, 1, n_bits, stdin );
	if ( size <= 0 )
	    return -1;			/* end of file */
	offset = 0;
	/* Round size down to integral number of codes */
	size = (size << 3) - (n_bits - 1);
    }
    r_off = offset;
    bits = n_bits;
#ifdef vax
    asm( "extzv   r10,r9,(r8),r11" );
#else /* not a vax */
	/*
	 * Get to the first byte.
	 */
	bp += (r_off >> 3);
	r_off &= 7;
	/* Get first part (low order bits) */
#ifdef NO_UCHAR
	code = ((*bp++ >> r_off) & rmask[8 - r_off]) & 0xff;
#else
	code = (*bp++ >> r_off);
#endif /* NO_UCHAR */
	bits -= (8 - r_off);
	r_off = 8 - r_off;		/* now, offset into code word */
	/* Get any 8 bit parts in the middle (<=1 for up to 16 bits). */
	if ( bits >= 8 ) {
#ifdef NO_UCHAR
	    code |= (*bp++ & 0xff) << r_off;
#else
	    code |= *bp++ << r_off;
#endif /* NO_UCHAR */
	    r_off += 8;
	    bits -= 8;
	}
	/* high order bits. */
	code |= (*bp & rmask[bits]) << r_off;
#endif /* vax */
    offset += n_bits;

    return code;
}

#ifndef _STANDALONE

char *
rindex(s, c)		/* For those who don't have it in libc.a */
register char *s, c;
{
	char *p;
	for (p = NULL; *s; s++)
	    if (*s == c)
		p = s;
	return(p);
}

#endif

#ifdef DEBUG
printcodes()
{
    /*
     * Just print out codes from input file.  For debugging.
     */
    code_int code;
    int col = 0, bits;

    bits = n_bits = INIT_BITS;
    maxcode = MAXCODE(n_bits);
    free_ent = ((block_compress) ? FIRST : 256 );
    while ( ( code = getcode() ) >= 0 ) {
	if ( (code == CLEAR) && block_compress ) {
   	    free_ent = FIRST - 1;
   	    clear_flg = 1;
	}
	else if ( free_ent < maxmaxcode )
	    free_ent++;
	if ( bits != n_bits ) {
	    fprintf(stderr, "\nChange to %d bits\n", n_bits );
	    bits = n_bits;
	    col = 0;
	}
	fprintf(stderr, "%5d%c", code, (col+=6) >= 74 ? (col = 0, '\n') : ' ' );
    }
    putc( '\n', stderr );
    exit( 0 );
}

#ifdef XENIX_16
code_int stab1[8192] ;
code_int stab2[8192] ;
code_int stab3[8192] ;
code_int stab4[8192] ;
code_int stab5[8192] ;
code_int stab6[8192] ;
code_int stab7[8192] ;
code_int stab8[8192] ;
code_int * sorttab[8] = {stab1, stab2, stab3, stab4, stab5, stab6, stab7,
						 stab8 } ;
#define stabof(i) (sorttab[(i) >> 13][(i) & 0x1fff]) 
#else
code_int sorttab[HSIZE];	/* sorted pointers into htab */
#define stabof(i) (sorttab[i])
#endif

dump_tab()	/* dump string table */
{
    register int i, first;
    register ent;
#define STACK_SIZE	15000
    int stack_top = STACK_SIZE;
    register c;
	unsigned mbshift ;

    if(do_decomp == 0) {	/* compressing */
	register int flag = 1;

	for(i=0; i<hsize; i++) {	/* build sort pointers */
		if((int)htabof(i) >= 0) {
			stabof(codetabof(i)) = i;
		}
	}
	first = block_compress ? FIRST : 256;
	for(i = first; i < free_ent; i++) {
		fprintf(stderr, "%5d: \"", i);
		de_stack[--stack_top] = '\n';
		de_stack[--stack_top] = '"';
		stack_top = in_stack((htabof(stabof(i))>>maxbits)&0xff, 
                                     stack_top);
/*		for(ent=htabof(stabof(i)) & ((1<<maxbits)-1); */
		mbshift = ((1 << maxbits) - 1) ;
		ent = htabof(stabof(i)) & mbshift ;
		for(;
		    ent > 256;
		    /* ent=htabof(stabof(ent)) & ((1<<maxbits)-1)) { */
		    ent=htabof(stabof(ent)) & mbshift) {
			stack_top = in_stack(htabof(stabof(ent)) >> maxbits,
						stack_top);
		}
		stack_top = in_stack(ent, stack_top);
		fwrite( &de_stack[stack_top], 1, STACK_SIZE-stack_top, stderr);
	   	stack_top = STACK_SIZE;
	}
   } else if(!debug) {	/* decompressing */

       for ( i = 0; i < free_ent; i++ ) {
	   ent = i;
	   c = tab_suffixof(ent);
	   if ( isascii(c) && isprint(c) )
	       fprintf( stderr, "%5d: %5d/'%c'  \"",
			   ent, tab_prefixof(ent), c );
	   else
	       fprintf( stderr, "%5d: %5d/\\%03o \"",
			   ent, tab_prefixof(ent), c );
	   de_stack[--stack_top] = '\n';
	   de_stack[--stack_top] = '"';
	   for ( ; ent != 0;
		   ent = (ent >= FIRST ? tab_prefixof(ent) : 0) ) {
	       stack_top = in_stack(tab_suffixof(ent), stack_top);
	   }
	   fwrite( &de_stack[stack_top], 1, STACK_SIZE - stack_top, stderr );
	   stack_top = STACK_SIZE;
       }
    }
}

int
in_stack(c, stack_top)
	register c, stack_top;
{
	if ( (isascii(c) && isprint(c) && c != '\\') || c == ' ' ) {
	    de_stack[--stack_top] = c;
	} else {
	    switch( c ) {
	    case '\n': de_stack[--stack_top] = 'n'; break;
	    case '\t': de_stack[--stack_top] = 't'; break;
	    case '\b': de_stack[--stack_top] = 'b'; break;
	    case '\f': de_stack[--stack_top] = 'f'; break;
	    case '\r': de_stack[--stack_top] = 'r'; break;
	    case '\\': de_stack[--stack_top] = '\\'; break;
	    default:
	 	de_stack[--stack_top] = '0' + c % 8;
	 	de_stack[--stack_top] = '0' + (c / 8) % 8;
	 	de_stack[--stack_top] = '0' + c / 64;
	 	break;
	    }
	    de_stack[--stack_top] = '\\';
	}
	return stack_top;
}
#endif /* DEBUG */

#ifndef _STANDALONE

writeerr()
{
    perror ( ofname );
    unlink ( ofname );
    exit ( 1 );
}

copystat(ifname, ofname)
char *ifname, *ofname;
{
    struct stat statbuf;
    int mode;
    time_t timep[2];

    fclose(stdout);
    if (stat(ifname, &statbuf)) {		/* Get stat on input file */
	perror(ifname);
	return;
    }
    if ((statbuf.st_mode & S_IFMT/*0170000*/) != S_IFREG/*0100000*/) {
	if(quiet)
	    	fprintf(stderr, "%s: ", ifname);
	fprintf(stderr, " -- not a regular file: unchanged");
	exit_stat = 1;
	perm_stat = 1;
    } else if (statbuf.st_nlink > 1) {
	if(quiet)
	    	fprintf(stderr, "%s: ", ifname);
	fprintf(stderr, " -- has %d other links: unchanged",
		statbuf.st_nlink - 1);
	exit_stat = 1;
	perm_stat = 1;
    } else if (exit_stat == 2 && (!force)) { /* No compression: remove file.Z */
	if(!quiet)
		fprintf(stderr, " -- file unchanged");
    } else {			/* ***** Successful Compression ***** */
	exit_stat = 0;
	mode = statbuf.st_mode & 07777;
	if (chmod(ofname, mode))		/* Copy modes */
	    perror(ofname);
	if (suser)
	    chown(ofname, statbuf.st_uid, statbuf.st_gid); /* Copy ownership */
	timep[0] = statbuf.st_atime;
	timep[1] = statbuf.st_mtime;
	utime(ofname, timep);	/* Update last accessed and modified times */
	if (unlink(ifname))	/* Remove input file */
	    perror(ifname);
	if(!quiet)
		fprintf(stderr, " -- replaced with %s", ofname);
	return;		/* Successful return */
    }

    /* Unsuccessful return -- one of the tests failed */
    if (unlink(ofname))
	perror(ofname);
}
/*
 * This routine returns 1 if we are running in the foreground and stderr
 * is a tty.
 */
foreground()
{
	if(bgnd_flag) {	/* background? */
		return(0);
	} else {			/* foreground */
		if(isatty(2)) {		/* and stderr is a tty */
			return(1);
		} else {
			return(0);
		}
	}
}

#ifdef sgi
void
#endif
onintr ( )
{
    if (!precious)
	unlink ( ofname );
    exit ( 1 );
}

#ifdef sgi
void
#endif
oops ( )	/* wild pointer -- assume bad input */
{
    if ( do_decomp == 1 ) 
    	fprintf ( stderr, "uncompress: corrupt input\n" );
    unlink ( ofname );
    exit ( 1 );
}

#endif /* !_STANDALONE */

#ifndef DECOMPRESS_ONLY

cl_block ()		/* table clear for block compress */
{
    register int rat;

    checkpoint = in_count + CHECK_GAP;
#ifdef DEBUG
	if ( debug ) {
    		fprintf ( stderr, "count: %ld, ratio: ", in_count );
     		prratio ( stderr, in_count, bytes_out );
		fprintf ( stderr, "\n");
	}
#endif /* DEBUG */

    if(in_count > 0x007fffff) {	/* shift will overflow */
	rat = bytes_out >> 8;
	if(rat == 0) {		/* Don't divide by zero */
	    rat = 0x7fffffff;
	} else {
	    rat = in_count / rat;
	}
    } else {
	rat = (in_count << 8) / bytes_out;	/* 8 fractional bits */
    }
    if ( rat > ratio ) {
	ratio = rat;
    } else {
	ratio = 0;
#ifdef DEBUG
	if(verbose)
		dump_tab();	/* dump string table */
#endif
 	cl_hash ( (count_int) hsize );
	free_ent = FIRST;
	clear_flg = 1;
	output ( (code_int) CLEAR );
#ifdef DEBUG
	if(debug)
    		fprintf ( stderr, "clear\n" );
#endif /* DEBUG */
    }
}

cl_hash(hsize)		/* reset code table */
	register count_int hsize;
{
#ifndef XENIX_16	/* Normal machine */
	register count_int *htab_p = htab+hsize;
#else
	register j;
	register int = hsize;
	register count_int *htab_p;
#endif
	register int i;
	register int m1 = -1;

#ifdef XENIX_16
    for(j=0; j<=8 && k>=0; j++,k-=8192) {
	i = 8192;
	if(k < 8192) {
		i = k;
	}
	htab_p = &(htab[j][i]);
	i -= 16;
	if(i > 0) {
#else
	i = hsize - 16;
#endif
 	do {				/* might use Sys V memset(3) here */
		*(htab_p-16) = m1;
		*(htab_p-15) = m1;
		*(htab_p-14) = m1;
		*(htab_p-13) = m1;
		*(htab_p-12) = m1;
		*(htab_p-11) = m1;
		*(htab_p-10) = m1;
		*(htab_p-9) = m1;
		*(htab_p-8) = m1;
		*(htab_p-7) = m1;
		*(htab_p-6) = m1;
		*(htab_p-5) = m1;
		*(htab_p-4) = m1;
		*(htab_p-3) = m1;
		*(htab_p-2) = m1;
		*(htab_p-1) = m1;
		htab_p -= 16;
	} while ((i -= 16) >= 0);
#ifdef XENIX_16
	}
    }
#endif
    	for ( i += 16; i > 0; i-- )
		*--htab_p = m1;
}

prratio(stream, num, den)
FILE *stream;
int num, den;
{
	register int q;			/* Doesn't need to be long */

	if(num > 214748L) {		/* 2147483647/10000 */
		q = num / (den / 10000L);
	} else {
		q = 10000L * num / den;		/* Long calculations, though */
	}
	if (q < 0) {
		putc('-', stream);
		q = -q;
	}
	fprintf(stream, "%d.%02d%%", q / 100, q % 100);
}

version()
{
	fprintf(stderr, "%s\n", rcs_ident);
	fprintf(stderr, "Options: ");
#ifdef vax
	fprintf(stderr, "vax, ");
#endif
#ifdef NO_UCHAR
	fprintf(stderr, "NO_UCHAR, ");
#endif
#ifdef SIGNED_COMPARE_SLOW
	fprintf(stderr, "SIGNED_COMPARE_SLOW, ");
#endif
#ifdef XENIX_16
	fprintf(stderr, "XENIX_16, ");
#endif
#ifdef COMPATIBLE
	fprintf(stderr, "COMPATIBLE, ");
#endif
#ifdef DEBUG
	fprintf(stderr, "DEBUG, ");
#endif
#ifdef BSD4_2
	fprintf(stderr, "BSD4_2, ");
#endif
	fprintf(stderr, "BITS = %d\n", BITS);
}

#endif /* DECOMPRESS_ONLY */

#ifdef _STANDALONE
void
setup_compress(void)
{

#ifdef DECOMPRESS_ONLY

    /* Don't put the big arrays in our BSS, promcvt likes to put BSS into
     * the .bin images.  It's broken.
     */
    extern int _end;
    char *free_data;

    free_data = ((char *)&_end + SEG0STACK_SIZE);

    htab = (count_int *)free_data;

    codetab = ((unsigned short *)((count_int *)free_data + HSIZE * sizeof(count_int)));

    /* htab is HSIZE count_ints long */
    /* codetab is HSIZE unsigned shorts long */

#endif /* DECOMPRESS_ONLY */

    hsize = HSIZE;
    if(maxbits < INIT_BITS) maxbits = INIT_BITS;
    if (maxbits > BITS) maxbits = BITS;
    maxmaxcode = 1L << maxbits;
}
#endif /* _STANDALONE */

