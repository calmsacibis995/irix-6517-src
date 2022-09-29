/*
 *  FILE
 *
 *	compress.c    file compression ala IEEE Computer, June 1984.
 *
 *  DESCRIPTION
 *
 *	Algorithm from "A Technique for High Performance Data Compression",
 *	Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 *
 *	Algorithm:
 *
 * 	Modified Lempel-Ziv method (LZW).  Basically finds common
 *	substrings and replaces them with a variable size code.  This is
 *	deterministic, and can be done on the fly.  Thus, the decompression
 *	procedure needs no input table, but tracks the way the table was built.
 *
 *  NOTES
 *
 *	This code is derived from public domain code and is not subject
 *	to any of the restrictions of the rest of the bru source code
 *	with respect to copyright or licensing.  The contributions of
 *	the original authors, noted below, are gratefully acknowledged.
 *
 *	Based on: compress.c,v 4.0 85/07/30 12:50:00 joe Release $";
 *
 *  AUTHORS
 *
 *	Spencer W. Thomas	decvax!harpo!utah-cs!utah-gr!thomas
 *	Jim McKie		decvax!mcvax!jim
 *	Steve Davies		decvax!vax135!petsd!peora!srd
 *	Ken Turkowski		decvax!decwrl!turtlevax!ken
 *	James A. Woods		decvax!ihnp4!ames!jaw
 *	Joe Orost		decvax!vax135!petsd!joe
 *
 */

#include <stdio.h>
#include <limits.h>

#if unix || xenix
#  include <sys/types.h>
#  include <sys/stat.h>
#else
#  include "sys.h"
#endif

#include <ctype.h>

#include "typedefs.h"		/* Locally defined types */
#include "dbug.h"
#include "finfo.h"		/* File information structure */
#include "errors.h"

#define BITS 	12		/* max bits/code for 16-bit machine */

#ifdef pdp11
# define NO_UCHAR	/* also if "unsigned char" functions as signed char */
#endif			/* pdp11 *//* don't forget to compile with  -i */

#ifdef z8000
# undef vax			/* weird preprocessor */
#endif	/* z8000 */

#define HSIZE	5003		/* 80% occupancy */

/*
 * a code_int must be able to hold 2**BITS values of type int, and also -1
 */

typedef int code_int;
typedef long int count_int;

#ifdef NO_UCHAR
typedef char char_type;
#else
typedef unsigned char char_type;
#endif /* UCHAR */

/* Defines for third byte of header */

#define BIT_MASK	0x1f
#define BLOCK_MASK	0x80

/*
 * Masks 0x40 and 0x20 are free.  I think 0x20 should mean that there is
 * a fourth header byte (for expansion).
 */

#define INIT_BITS 9		/* initial number of bits/code */

#define MAXCODE(n_bits)	((1 << (n_bits)) - 1)

/*
 * To save much memory, we overlay the table used by compress() with those
 * used by decompress().  The tab_prefix table is the same size and type
 * as the codetab.  The tab_suffix table needs 2**BITS characters.  We
 * get this from the beginning of htab.  The output stack uses the rest
 * of htab, and contains characters.  There is plenty of room for any
 * possible stack (stack used to be 8000 characters).
 */

#define tab_prefixof(i)		codetabof(i)
#define htabof(i)		htab[i]
#define codetabof(i)		codetab[i]
#define tab_suffixof(i)		((char_type *)(htab))[i]
#define de_stack		((char_type *)&tab_suffixof(1<<BITS))

/*
 * the next two codes should not be changed lightly, as they must not
 * lie within the contiguous general code space.
 */

#define FIRST	257		/* first free entry */
#define	CLEAR	256		/* table clear output code */

#define CHECK_GAP 10000LL		/* ratio check interval */

static int offset;

char_type magic_header[] = {
    "\037\235"
}; /* 1F 9D */

static int n_bits;			/* number of bits/code */
static int maxbits = BITS;		/* user setable max number bits/code */
static code_int maxcode;		/* maximum code, given n_bits */
static code_int maxmaxcode = 1 << BITS;	/* should NEVER generate this code */

static count_int htab[HSIZE];
static unsigned short codetab[HSIZE];

static code_int hsize = HSIZE;		/* for dynamic table sizing */
static code_int free_ent = 0;		/* first unused entry */

/*
 * block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */

static int block_compress = BLOCK_MASK;
static int clear_flg = 0;
static long int ratio = 0;

static long long checkpoint = CHECK_GAP;

static long long  int in_count = 1;		/* length of input */
static long long  int bytes_out;		/* length of compressed output */
static long int out_count = 0;		/* # of codes output (for debugging) */

static char_type lmask[9] = {
    0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00
};

static char_type rmask[9] = {
    0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff
};

static char buf[BITS];

static int resultflag;

static void output ();
static void cl_block ();
static void cl_hash ();
static code_int getcode ();

extern char *strrchr ();
extern int s_read ();
extern int s_write ();

/*
 *	The following macros and variables allow us to do our own
 *	buffering of input and output streams, without involving
 *	stdio.  This gains us slightly better error handling and
 *	fits in more smoothly with the rest of the bru code
 *	(low level I/O when possible).
 */

#define IOSIZE (1024)		/* Chunk size for I/O */

static unsigned char inbuf[IOSIZE];
static unsigned char outbuf[IOSIZE];
static int incnt;
static int outcnt;
static unsigned char *inbufp = inbuf;
static unsigned char *outbufp = outbuf;
static int fillinbuf ();
static void flushoutbuf ();

#define PUTC(ch,fip)	*outbufp++ = ch; if (++outcnt == IOSIZE) flushoutbuf (fip,0)
#define GETC(fip)	(incnt-- > 0 ? (int) *inbufp++ : fillinbuf (fip,0))
#define FFLUSH(fip)	flushoutbuf(fip,0)

#define ZPUTC(ch,fip)	*outbufp++ = ch; if (++outcnt == IOSIZE) flushoutbuf (fip,1)
#define ZGETC(fip)	(incnt-- > 0 ? (int) *inbufp++ : fillinbuf (fip,1))
#define ZFFLUSH(fip)	flushoutbuf(fip,1)


/*  kludgy, but the problem of not dealing with short read/writes
 * exists all over bru, I just discovered; it is a bit more
 * likely to show up here...  The attempt to write the remainder
 * SHOULD succeed, or get the correct errno set.  check for
 * 0 so we don't loop on some weird error; clear errno so bru_error
 * prints info about unknown error.
*/
static void
cwrite(int fd, char *buf, int cnt, char *fname)
{
    int res;
    extern int errno;

    while(cnt > 0) {
	res = s_write (fd, buf, cnt);
	if(res <= 0) {
	    if(res == 0)
		errno = 0;
	    bru_error (ERR_WRITE, fname);
	    resultflag = 0;
	    break;
	}
	cnt -= res;
    }
}

/*
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

int compress (fip)
struct finfo *fip;
{
    register long fcode;
    register code_int i = 0;
    register int c;
    register code_int ent;
    register int disp;
    register code_int hsize_reg;
    register int hshift;

    DBUG_ENTER ("compress");
    resultflag = 1;
    ZPUTC ((char) magic_header[0], fip);
    ZPUTC ((char) magic_header[1], fip);
    ZPUTC ((char) (maxbits | block_compress), fip);
    offset = 0;
    bytes_out = 3;		/* includes 3-byte header mojo */
    out_count = 0;
    clear_flg = 0;
    ratio = 0;
    in_count = 1;
    checkpoint = CHECK_GAP;
    maxcode = MAXCODE (n_bits = INIT_BITS);
    free_ent = ((block_compress) ? FIRST : 256);

    ent = GETC (fip);

    hshift = 0;
    for (fcode = (long) hsize; fcode < 65536L; fcode *= 2L) {
	hshift++;
    }
    hshift = 8 - hshift;	/* set hash code range bound */

    hsize_reg = hsize;
    cl_hash ((count_int) hsize_reg);	/* clear hash table */

	/* until input file processed, or we get a write error */
    while (resultflag && (c = GETC (fip)) != EOF) {
	DBUG_PRINT ("Z", ("next input = %#x", c));
	in_count++;
	fcode = (long) (((long) c << maxbits) + ent);
	i = ((c << hshift) ^ ent);	/* xor hashing */
	DBUG_PRINT ("Z", ("i = %d", i));
	if (htabof (i) == fcode) {
	    ent = codetabof (i);
	    DBUG_PRINT ("Z", ("continue with ent %#x", ent));
	    continue;
	} else if ((long) htabof (i) < 0) {	/* empty slot */
	    DBUG_PRINT ("Z", ("goto nomatch"));
	    goto nomatch;
	}
	disp = hsize_reg - i;	/* secondary hash (after G. Knott) */
	if (i == 0) {
	    disp = 1;
	}
probe: 
	DBUG_PRINT ("Z", ("at probe"));
	if ((i -= disp) < 0) {
	    i += hsize_reg;
	}	
	DBUG_PRINT ("Z", ("i = %d", i));
	if (htabof (i) == fcode) {
	    ent = codetabof (i);
	    DBUG_PRINT ("Z", ("continue with ent %#x", ent));
	    continue;
	}
	if ((long) htabof (i) > 0) {
	    DBUG_PRINT ("Z", ("goto probe"));
	    goto probe;
	}
nomatch: 
	DBUG_PRINT ("Z", ("at nomatch"));
	output ((code_int) ent, fip);
	out_count++;
	/*
	 * Added for xfs
	 */
	if (bytes_out > LONG_MAX)
		resultflag=0;

	DBUG_PRINT ("Z", ("new out_count %d", out_count));
	ent = c;
	if (free_ent < maxmaxcode) {
	    codetabof (i) = free_ent++;		/* code -> hashtable */
	    htabof (i) = fcode;
	} else if (in_count >= checkpoint && block_compress) {
	    cl_block (fip);
	}
    }

	if(resultflag) {
		/* Put out the final code, but not if we had a write error */
		output ((code_int) ent, fip);
		out_count++;
		DBUG_PRINT ("Z", ("final out_count %d", out_count));
		output ((code_int) -1, fip);
		DBUG_PRINT ("cmpr", ("%lld chars in", in_count));
		DBUG_PRINT ("cmpr", ("%ld codes (%lld bytes) out", out_count, bytes_out));
		DBUG_PRINT ("cmpr", ("largest code was %d (%d bits)", free_ent - 1, n_bits));
		ZFFLUSH (fip);
	}
    DBUG_RETURN (resultflag);
}

/*
 * TAG( output )
 *
 * Output the given code.
 * Inputs:
 * 	code:	A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *		that n_bits =< (long)wordsize - 1.
 * Outputs:
 * 	Outputs code to the file.
 * Assumptions:
 *	Chars are 8 bits long.
 * Algorithm:
 * 	Maintain a BITS character long buffer (so that 8 codes will
 * fit in it exactly).  Use the VAX insv instruction to insert each
 * code in turn.  When the buffer fills up empty it and start over.
 * if resultflag is 0, don't do writes, because there must already
 * have been a write error in the flush routine.  Don't return
 * early so if debug is turned on we get the matching entry and
 * exit junk; doesn't matter because caller will abort anyway.
 */

static void output (code, fip)
code_int code;
struct finfo *fip;
{
    register int r_off = offset;
    register int bits = n_bits;
    register char *bp = buf;
	int res;

    DBUG_ENTER ("output");
    DBUG_PRINT ("Z", ("output code %#x", code));
    if (code >= 0) {
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
	if (bits >= 8) {
	    *bp++ = code;
	    code >>= 8;
	    bits -= 8;
	}
	/* Last bits. */
	if (bits) {
	    *bp = code;
	}
	offset += n_bits;
	if (offset == (n_bits << 3)) {
	    bp = buf;
	    bits = n_bits;
	    bytes_out += bits;
	    do {
		ZPUTC (*bp++, fip);
	    } while (--bits);
	    offset = 0;
	}
	/* 
	 * If the next entry is going to be too big for the code size,
	 * then increase it, if possible.
	 */
	if (free_ent > maxcode || (clear_flg > 0)) {
	    /* 
	     * Write the whole buffer, because the input side won't
	     * discover the size increase until after it has read it.
	     */
	    if (offset > 0) {
		ZFFLUSH (fip);
		DBUG_PRINT ("Z", ("write %d bytes out", n_bits));
		cwrite(fip->zfildes, buf, n_bits, fip->zfname);
		bytes_out += n_bits;
	    }
	    offset = 0;
	    if (clear_flg) {
		maxcode = MAXCODE (n_bits = INIT_BITS);
		clear_flg = 0;
	    } else {
		n_bits++;
		if (n_bits == maxbits) {
		    maxcode = maxmaxcode;
		} else {
		    maxcode = MAXCODE (n_bits);
		}
	    }
	    DBUG_PRINT ("cmpr", ("change to %d bits", n_bits));
	}
    } else {
	/* 
	 * At EOF, write the rest of the buffer.
	 */
	r_off = (offset + 7) / 8;
	if (offset > 0) {
	    ZFFLUSH (fip);
	    DBUG_PRINT ("Z", ("write %d bytes out", r_off));
	    cwrite(fip->zfildes, buf, r_off, fip->zfname);
	}
	bytes_out += r_off;
	offset = 0;
	ZFFLUSH (fip);
    }
    DBUG_VOID_RETURN;
}

static void cl_block (fip)
struct finfo *fip;
{
    register long int rat;
    
    DBUG_ENTER ("cl_block");
    checkpoint = in_count + CHECK_GAP;
    DBUG_PRINT ("cmpr", ("count = %lld", in_count));
    if (in_count > 0x007fffff) {	/* shift will overflow */
	rat = bytes_out >> 8;
	if (rat == 0) {			/* Don't divide by zero */
	    rat = 0x7fffffff;
	} else {
	    rat = in_count / rat;
	}
    } else {
	rat = (in_count << 8) / bytes_out;
	/* 8 fractional bits */
    }
    if (rat > ratio) {
	ratio = rat;
    } else {
	ratio = 0;
	cl_hash ((count_int) hsize);
	free_ent = FIRST;
	clear_flg = 1;
	output ((code_int) CLEAR, fip);
    }
    DBUG_VOID_RETURN;
}
    
/* reset code table */

static void cl_hash (clhsize)
register count_int clhsize;
{
    register count_int *htab_p = htab + clhsize;
    register long i;
    register long m1 = -1;
	
    DBUG_ENTER ("cl_hash");
	    i = clhsize - 16;
	    do {	/* might use Sys V memset(3) here */
		*(htab_p - 16) = m1;
		*(htab_p - 15) = m1;
		*(htab_p - 14) = m1;
		*(htab_p - 13) = m1;
		*(htab_p - 12) = m1;
		*(htab_p - 11) = m1;
		*(htab_p - 10) = m1;
		*(htab_p - 9) = m1;
		*(htab_p - 8) = m1;
		*(htab_p - 7) = m1;
		*(htab_p - 6) = m1;
		*(htab_p - 5) = m1;
		*(htab_p - 4) = m1;
		*(htab_p - 3) = m1;
		*(htab_p - 2) = m1;
		*(htab_p - 1) = m1;
		htab_p -= 16;
	    } while ((i -= 16) >= 0);
    for (i += 16; i > 0; i--) {
	*--htab_p = m1;
    }
    DBUG_VOID_RETURN;
}
    
/*
 * Decompress fip.  This routine adapts to the codes in the
 * file building the "string" table on-the-fly; requiring no table to
 * be stored in the compressed file.  The tables used herein are shared
 * with those of the compress() routine.  See the definitions above.
 */

int decompress (fip)
struct finfo *fip;
{
    register char_type *stackp;
    register int finchar;
    register code_int code;
    register code_int oldcode;
    register code_int incode;

    incnt = 0;	/* in case earlier decompress() failed */

    DBUG_ENTER ("decompress");
    resultflag = 1;
    /* Check the magic number */
    if ((ZGETC (fip) != (magic_header[0] & 0xFF))
	|| (ZGETC (fip) != (magic_header[1] & 0xFF))) {
	resultflag = 0;
    } else {
	maxbits = ZGETC (fip);		/* set -b from file */
	block_compress = maxbits & BLOCK_MASK;
	maxbits &= BIT_MASK;
	maxmaxcode = 1 << maxbits;
	if (maxbits > BITS) {
	    fprintf (stderr, "Urk, compress bad number of bits!\n");
	    exit (1);
	}
	/* 
	 * Initialize the first 256 entries in the table.
	 */
	maxcode = MAXCODE (n_bits = INIT_BITS);
	for (code = 255; code >= 0; code--) {
	    tab_prefixof (code) = 0;
	    tab_suffixof (code) = (char_type) code;
	}
	free_ent = ((block_compress) ? FIRST : 256);
	
	finchar = oldcode = getcode (fip);
	if (oldcode == -1) {	/* EOF already? */
	    DBUG_RETURN (0);			/* Get out of here */
	}
	PUTC ((char) finchar, fip);
	/* first code must be 8 bits = char */
	stackp = de_stack;
	
	while ((code = getcode (fip)) > -1) {
	    DBUG_PRINT ("code", ("next code = %#x", code));
	    if ((code == CLEAR) && block_compress) {
		for (code = 255; code >= 0; code--) {
		    tab_prefixof (code) = 0;
		}
		clear_flg = 1;
		free_ent = FIRST - 1;
		if ((code = getcode (fip)) == -1) {
		    DBUG_PRINT ("code", ("next code = %#x", code));
		    /* O, untimely death! */
		    DBUG_PRINT ("code", ("terminate from getcode gets EOF"));
		    resultflag = 0;
		    break;
		}
	    }
	    incode = code;
	    /* 
	     * Special case for KwKwK string.
	     */
	    if (code >= free_ent) {
		*stackp++ = finchar;
		code = oldcode;
	    }
	    /* 
	     * Generate output characters in reverse order
	     */
	    while (code >= 256 && code < HSIZE) {
		*stackp++ = tab_suffixof (code);
		code = tab_prefixof (code);
	    }
	    if(code >= HSIZE) {
		DBUG_PRINT ("code", ("Bogus compression table entry %x", code));
		resultflag = 0;
		break;
	    }
	    *stackp++ = finchar = tab_suffixof (code);
	    /* 
	     * And put them out in forward order
	     */
	    do {
		PUTC ((char) *--stackp, fip);
		DBUG_PRINT ("code", ("*stackp = %#x", *stackp));
	    } while (stackp > de_stack);
	    /* 
	     * Generate the new entry.
	     */
	    if ((code = free_ent) < maxmaxcode) {
		tab_prefixof (code) = (unsigned short) oldcode;
		tab_suffixof (code) = finchar;
		free_ent = code + 1;
	    }
	    /* 
	     * Remember previous code.
	     */
	    oldcode = incode;
	}
	FFLUSH (fip);
    }
    DBUG_RETURN (resultflag);
}


/*
 * TAG( getcode )
 *
 * Read one code from the standard input.  If EOF, return -1.
 * Inputs:
 * 	fip
 * Outputs:
 * 	code or -1 is returned.
 */

static code_int getcode (fip)
struct finfo *fip;
{
    register code_int code;
    static int offset = 0;
    static int size = 0;
    static char_type buf[BITS];
    register int r_off;
    register int bits;
    register char_type *bp = buf;
    register int nextch;
    
    DBUG_ENTER ("getcode");
    if (clear_flg > 0 || offset >= size || free_ent > maxcode) {
	/* 
	 * If the next entry will be too big for the current code
	 * size, then we must increase the size.  This implies reading
	 * a new buffer full, too.
	 */
	if (free_ent > maxcode) {
	    n_bits++;
	    if (n_bits == maxbits) {
		maxcode = maxmaxcode;	    /* won't get any bigger now */
	    } else {
		maxcode = MAXCODE (n_bits);
	    }
	}
	if (clear_flg > 0) {
	    maxcode = MAXCODE (n_bits = INIT_BITS);
	    clear_flg = 0;
	}
	bp = buf;
	for (size = 0; size < n_bits; size++) {
	    if ((nextch = ZGETC (fip)) == EOF) {
		break;
	    } else {
		DBUG_PRINT ("code", ("next byte from input = %#x", nextch));
		*bp++ = nextch;
	    }
	}
	if (size == 0) {
	    DBUG_PRINT ("code", ("return code %#x", -1));
	    DBUG_RETURN (-1);
	}
	bp = buf;
	offset = 0;
	/* Round size down to integral number of codes */
	size = (size << 3) - (n_bits - 1);
    }
    r_off = offset;
    bits = n_bits;
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
#endif	/* NO_UCHAR */
    bits -= (8 - r_off);
    r_off = 8 - r_off;		/* now, offset into code word */
    /* Get any 8 bit parts in the middle (<=1 for up to 16 bits). */
    if (bits >= 8) {
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
    offset += n_bits;
    DBUG_PRINT ("code", ("return code %#x", code));
    DBUG_RETURN (code);
}



static int fillinbuf (fip, zflag)
struct finfo *fip;
int zflag;
{
    int firstchar;
    int fildes;
    char *fname;

    DBUG_ENTER ("fillinbuf");
    inbufp = inbuf;
    if (zflag) {
	fildes = fip -> zfildes;
	fname = fip -> zfname;
    } else {
	fildes = fip -> fildes;
	fname = fip -> fname;
    }
    if ((incnt = s_read (fildes, inbuf, IOSIZE)) > 0) {
	DBUG_PRINT ("inbuf", ("read %d bytes from input", incnt));
	firstchar = *inbufp++;
	incnt--;
    } else if (incnt == -1) {
	bru_error (ERR_READ, fname);
	resultflag = 0;
	firstchar = EOF;
	incnt = 0;
    } else {
	DBUG_PRINT ("inbuf", ("found EOF"));
	firstchar = EOF;
	incnt = 0;
    }
    DBUG_RETURN (firstchar);
}

static void flushoutbuf (fip, zflag)
struct finfo *fip;
int zflag;
{
    int fildes;
    char *fname;

    DBUG_ENTER ("flushoutbuf");
	if(resultflag) { /* else already a write error */
		if (zflag) {
		fildes = fip -> zfildes;
		fname = fip -> zfname;
		} else {
		fildes = fip -> fildes;
		fname = fip -> fname;
		}
		DBUG_PRINT ("outbuf", ("flush %d bytes from outbuf", outcnt));
		if (outcnt > 0)
			cwrite(fildes, (char *)outbuf, outcnt, fname);
		outcnt = 0;
		outbufp = outbuf;
	}
    DBUG_VOID_RETURN;
}
