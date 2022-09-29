/*
 * Copyright 1996, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/*
 * Guts of bsd/compress/compress.c, ripped out to form
 * a subroutine usable by applications so that they can
 * compress/uncompress without invoking separate executables.
 *
 * See associated header file comp.h for usage.
 *
 * To uncompress data, see the matching uncomp.h/uncomp.c files.
 */

#include <comp.h>
#include <inttypes.h>
#include <string.h>

/* Defines for third byte of header */
#define BIT_MASK	0x1f
#define BLOCK_MASK	0x80

#define MAXBITS 16	    /* Upper limit on code size, in bits. */
#define HSIZE	69001	    /* 95% occupancy */
#define INIT_BITS 9	    /* initial number of bits/code */

#define MAXCODE(n_bits)    ((1L << (n_bits)) - 1)

/*
 * C++ like -- Presume pointer to COMP_DATA for
 *	       this stream is a local variable or argument
 *	       named "my_comp_data".  Resembles C++ "this".
 *	       "thiscd" stands for: THIS Comp_Data struct.
 */

#define thiscd(x) (my_comp_data->x)
#define thisopt(x) (thiscd (options.x))

#define htabof(i)	(thiscd (htab) [i])
#define codetabof(i)	(thiscd (codetab) [i])

#define CHECK_GAP 10000LL	/* ratio check interval */

#define FIRST	257	/* first free entry */
#define	CLEAR	256	/* table clear output code */

#define putcode(x) (thiscd (codebuf[thiscd (nextcode) ++]) = x, \
	thiscd (free_ent) <= thiscd (maxcode) && (thiscd (clear_flg) <= 0) && thiscd (nextcode) < 8 || \
	    _comp_output_codebuf (my_comp_data))

#undef putchar
#define putchar(x) ( ((-- thiscd (outbufcnt)) < 0) ? \
		    _comp_flush_outbuf (my_comp_data, x, 1) : \
		    (* thiscd (outbufptr) ++ = (char_type) (x)))

static int _comp_output_codebuf (COMP_DATA *);
static void _comp_flush_codebuf (COMP_DATA *);
static int _comp_flush_outbuf (COMP_DATA *, char_type, int);

static void _comp_cl_hash (COMP_DATA *);
static void _comp_prratio (FILE *, long long, int);
static void _comp_cl_block (COMP_DATA *);

void comp_options_default (COMP_OPTIONS *popt)
{
    popt->block_compress = BLOCK_MASK;
    popt->nomagic = 0;
    popt->maxbits = MAXBITS;
    popt->maxfilesize = 0;
    popt->outbufsize = 64*1024;
    popt->output_param = 0;
    popt->quiet = 1;
    popt->verbose = 0;
    popt->debug = 0;
    popt->diagstream = stderr;
}

/*
 * initialize a COMP_DATA structure using the given options,
 * get ready to compress
 */

int comp_begin(COMP_DATA *comp_data_, COMP_OPTIONS *comp_options_)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */
    register long fcode;

    thiscd (options) = *comp_options_;

    thiscd (first_compress_call) = 1;
    thiscd (n_bits) = INIT_BITS;
    thiscd (maxcode) = MAXCODE (thiscd (n_bits));
    thiscd (maxmaxcode) = 1 << thisopt (maxbits);
    thiscd (clear_flg) = 0;
    thiscd (ratio) = 0;
    thiscd (checkpoint) = CHECK_GAP;
    thiscd (in_count) = 1;
    thiscd (bytes_out) = 0;
    thiscd (out_count) = 0;
    thiscd (ent) = -1;

    if (thisopt (maxfilesize) <= 0 || thisopt (maxfilesize) >= (1 << 15))
	thiscd (hnumentries) = HSIZE;
    else if ( thisopt (maxfilesize) < (1 << 12) )
	thiscd (hnumentries) = 5003;
    else if ( thisopt (maxfilesize) < (1 << 13) )
	thiscd (hnumentries) = 9001;
    else if ( thisopt (maxfilesize) < (1 << 14) )
	thiscd (hnumentries) = 18013;
    else   /* thisopt (maxfilesize) < (1 << 15) */
	thiscd (hnumentries) = 35023;

#if 0
    /*
     * In Sept 94 and June 1995, jeffd and olson added a "hack fix"
     * to avoid out of bounds "disp" values in the main compress()
     * routine.  I have removed their fix, and instead deleted the two
     * lines of code that appear at the end of this comment.
     *
     * Turns out that these bogus disp values only occurred on already
     * compressed (or zip'd) files that were between 32768 and 47000
     * bytes in size.  On such files, the following computation from
     * compress():
     *
     *	    i = (c << hshift_reg) ^ ent;
     *
     * can produce any 16 bit value, that is any value in the half
     * closed interval [0..65536).  But the hash tables were only valid
     * for values in the range [0..47000).  That is, on uncompressible
     * files, which could generate codes over 32768 on less than 47000
     * bytes of input, the "eor" could generate bogus values of "i"
     * (values over 50021), and hence negative values of "disp".
     *
     * In other words,  the "Late addition" (circa 1984) to adjust the
     * hash table according to file size for faster startup on small
     * files was flawed.  The cases checking maxfilesize against even
     * powers of two were ok, but the one case below checking against
     * a non-power of two couldn't handle highly uncompressible data
     * input.
     */

    else    /* thisopt (maxfilesize) < 47000 */	/* DELETED CODE */
	thiscd (hnumentries) = 50021;		/* DELETED CODE */
#endif

    thiscd (hshift) = 0;
    for ( fcode = thiscd (hnumentries);  fcode < 65536; fcode *= 2 )
	thiscd (hshift) ++;
    thiscd (hshift) = 8 - thiscd (hshift);  /* set hash code range bound */

    thiscd (comp_errno) = 0;

    thiscd (nextcode) = 0;

    if (thiscd(htab) &&
	(thiscd(hnumentries) != thiscd(hnumentlast))) {
	thiscd(fp_free)(thiscd(htab));
	thiscd(htab) = 0;
    }

    if (thiscd(htab) == 0) {
	thiscd (htabsize) = thiscd (hnumentries) * sizeof (thiscd (htab[0]));
	thiscd (htab) = thiscd (fp_allocator) (thiscd (htabsize));
	if (thiscd (htab) == 0) {
	    thiscd (comp_errno) = -1;
	    return -1;
	}
    }
    thiscd(hnumentlast) = thiscd(hnumentries);

    _comp_cl_hash (my_comp_data);		    /* clear hash table */

    if (thiscd(codetab) == 0) {
	thiscd (codetabsize) = HSIZE * sizeof (thiscd (codetab[0]));
	thiscd (codetab) = thiscd (fp_allocator) (thiscd (codetabsize));
	if (thiscd (codetab) == 0) {
	    thiscd (comp_errno) = -1;
	    return -1;
	}
    }

    /* _comp_output_codebuf() needs at least MAXBITS (16) bytes in outbuf */
    if (thisopt (outbufsize) < MAXBITS)
	thisopt (outbufsize) = MAXBITS;

    if (thiscd(outbuf) &&
	(thisopt(outbufsize) != thiscd(outbuflast))) {
	thiscd(fp_free)(thiscd(outbuf));
	thiscd(outbuf) = 0;
    }

    if (thiscd(outbuf) == 0) {
	thiscd (outbuf) = thiscd (fp_allocator) (thisopt (outbufsize));
	if (thiscd (outbuf) == 0) {
	    thiscd (comp_errno) = -1;
	    return -1;
	}
    }
    thiscd(outbuflast) = thisopt(outbufsize);
    thiscd (outbufcnt) = thisopt (outbufsize);
    thiscd (outbufptr) = thiscd (outbuf);

    thiscd (free_ent) = (thisopt (block_compress) ? FIRST : 256 );

    return 0;
}

void comp_init(COMP_DATA *comp_data_, comp_allocator_t fp_allocator_,
	       comp_free_t fp_free_, comp_output_t fp_output_)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */
    register long fcode;

    thiscd (fp_allocator) = fp_allocator_;
    thiscd (fp_free) = fp_free_;
    thiscd (fp_output) = fp_output_;

    thiscd (htab) = 0;
    thiscd(hnumentlast) = 0;
    thiscd (codetab) = 0;
    thiscd (outbuf) = 0;
    thiscd(outbuflast) = 0;

}

void comp_destroy (COMP_DATA *comp_data_)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */

    thiscd (fp_free) (thiscd (htab));    thiscd (htab) = 0;
    thiscd (fp_free) (thiscd (codetab)); thiscd (codetab) = 0;
    thiscd (fp_free) (thiscd (outbuf));  thiscd (outbuf) = 0;
}

/*
 * Algorithm from "A Technique for High Performance Data Compression",
 * Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 *
 * 	Modified Lempel-Ziv method (LZW).  Basically finds common
 * substrings and replaces them with a variable size code.  This is
 * deterministic, and can be done on the fly.  Thus, the decompression
 * procedure needs no input table, but tracks the way the table was built.
 */

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

int
comp_compress (COMP_DATA *comp_data_, const char_type *buf, long buflen)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */
    register long fcode;	/* computed hash value of <ent, c> */
    register code_int i;	/* hash table index */
    register int c;		/* current character from input */
    register code_int ent;	/* previous character from input */
    register int disp;		/* secondary hash relative prime displacement */

    register code_int hnumentries_reg = thiscd (hnumentries);	/* cache */
    register int hshift_reg = thiscd (hshift);			/* cache */
    register const char_type *bufend = buf + buflen;		/* cache */
    register int in_count_reg = 0;				/* cache */

    if (thiscd (comp_errno))
	return -1;

    if (thiscd (first_compress_call)) {
	thiscd (first_compress_call) = 0;
	if (thisopt (nomagic) == 0) {
	    putchar (COMPRESS_MAGIC_1);
	    putchar (COMPRESS_MAGIC_2);
	    putchar ((char)(thisopt (maxbits) | thisopt (block_compress)));
	    if (thiscd (comp_errno))
		return -1;
	    _comp_flush_outbuf (my_comp_data, 0, 0);
	    thiscd (bytes_out) += 3;
	}
	if (buflen <= 0)
	    return -1;
	ent = *buf++;
    } else {
	ent = thiscd (ent);
    }

    while (buf < bufend) {
	c = *buf++;
	in_count_reg++;
	fcode = (c << thisopt (maxbits)) + ent;
 	i = (c << hshift_reg) ^ ent;	/* xor hashing */

	if ( htabof (i) == fcode ) {
	    ent = codetabof (i);
	    continue;
	} else if (htabof (i) < 0)	/* empty slot */
	    goto nomatch;

	disp = !i ? 1 : hnumentries_reg - i; /* secondary hash after G. Knott */
probe:
	if ( (i -= disp) < 0 )
	    i += hnumentries_reg;

	if (htabof (i) == fcode) {
	    ent = codetabof (i);
	    continue;
	}
	if (htabof (i) > 0 )
	    goto probe;
nomatch:
	putcode (ent);
	if (thiscd (comp_errno))
	    return -1;
	thiscd (out_count) ++;
 	ent = c;

	thiscd (in_count) += in_count_reg;
	in_count_reg = 0;

	if (thiscd (free_ent) < thiscd (maxmaxcode)) {
 	    codetabof (i) = thiscd (free_ent) ++;   /* code -> hashtable */
	    htabof (i) = fcode;
	} else if (thiscd (in_count) >= thiscd (checkpoint)
		&& thisopt (block_compress) ) {
		    _comp_cl_block (my_comp_data);
	}
    }

    thiscd (in_count) += in_count_reg;
    in_count_reg = 0;

    thiscd (ent) = ent;
    return 0;
}

int comp_end (COMP_DATA *comp_data_)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */

    if (thiscd (first_compress_call)) {
	thiscd (first_compress_call) = 0;
	if (thisopt (nomagic) == 0) {
	    putchar (COMPRESS_MAGIC_1);
	    putchar (COMPRESS_MAGIC_2);
	    putchar ((char)(thisopt (maxbits) | thisopt (block_compress)));
	    if (thiscd (comp_errno))
		return -1;
	    _comp_flush_outbuf (my_comp_data, 0, 0);
	    thiscd (bytes_out) += 3;
	}
    }
    if (thiscd (ent) != -1) {
	putcode (thiscd (ent));
	thiscd (out_count) ++;
    }
    _comp_flush_codebuf (my_comp_data);

    if (thiscd (comp_errno))
	return -1;

    /*
     * Print out stats on thisopt(diagstream)
     */
    if(! thisopt (quiet) ) {
#ifdef DEBUG
	fprintf( thisopt(diagstream),
		"%lld chars in, %lld codes (%lld bytes) out, compression factor: ",
		thiscd(in_count), thiscd(out_count), thiscd(bytes_out) );
	_comp_prratio (thisopt(diagstream), thiscd(in_count), thiscd(bytes_out) );
	fprintf( thisopt(diagstream), "\n");
	fprintf( thisopt(diagstream), "\tCompression as in compact: " );
	_comp_prratio (thisopt(diagstream), thiscd(in_count)-thiscd(bytes_out), thiscd(in_count) );
	fprintf( thisopt(diagstream), "\n");
	fprintf( thisopt(diagstream), "\tLargest code (of last block) was %d (%d bits)\n",
		thiscd(free_ent) - 1, thiscd(n_bits) );
#else /* !DEBUG */
	fprintf( thisopt(diagstream), "Compression: " );
	_comp_prratio (thisopt(diagstream), thiscd(in_count)-thiscd(bytes_out), thiscd(in_count) );
#endif /* DEBUG */
    }
    if (thiscd(bytes_out) > thiscd(in_count))	/* return -2 if no savings */
	return -2;
    return 0;
}

static int
_comp_output_codebuf (COMP_DATA *comp_data_)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */
    register char_type *bp;

    /*
     * Instead of calling an output routine once-per-code as in the earlier
     * compress code, we have inline code accumulate 8 codes in codebuf[8],
     * and only call the bit packing output subroutine once per 8 code line.
     * This reduces the overall time by about 20%.
     *
     * The code might look error prone, but a single change
     * to any of the numbers results in compressed output that differs
     * from that of the "standard" compress code in 10's of thousands
     * of places, when compressing my sample 3 Mb file.
     */

    if (thiscd (outbufcnt) < MAXBITS)
	_comp_flush_outbuf (my_comp_data, 0, 0);

    bp = thiscd (outbufptr);

#define code(x) ((uint32_t) thiscd (codebuf[x]))

    switch (thiscd (n_bits)) {
    case 9:
	bp[ 0] = ((code(0) << (32- 8)) >> (32-8));
	bp[ 1] = ((code(1) << (32- 7)) >> (32-8)) | (code(0) >>  8);
	bp[ 2] = ((code(2) << (32- 6)) >> (32-8)) | (code(1) >>  7);
	bp[ 3] = ((code(3) << (32- 5)) >> (32-8)) | (code(2) >>  6);
	bp[ 4] = ((code(4) << (32- 4)) >> (32-8)) | (code(3) >>  5);
	bp[ 5] = ((code(5) << (32- 3)) >> (32-8)) | (code(4) >>  4);
	bp[ 6] = ((code(6) << (32- 2)) >> (32-8)) | (code(5) >>  3);
	bp[ 7] = ((code(7) << (32- 1)) >> (32-8)) | (code(6) >>  2);
	bp[ 8] =				    (code(7) >>  1);
	break;
    case 10:
	bp[ 0] = ((code(0) << (32- 8)) >> (32-8));
	bp[ 1] = ((code(1) << (32- 6)) >> (32-8)) | (code(0) >>  8);
	bp[ 2] = ((code(2) << (32- 4)) >> (32-8)) | (code(1) >>  6);
	bp[ 3] = ((code(3) << (32- 2)) >> (32-8)) | (code(2) >>  4);
	bp[ 4] =				    (code(3) >>  2);
	bp[ 5] = ((code(4) << (32- 8)) >> (32-8));
	bp[ 6] = ((code(5) << (32- 6)) >> (32-8)) | (code(4) >>  8);
	bp[ 7] = ((code(6) << (32- 4)) >> (32-8)) | (code(5) >>  6);
	bp[ 8] = ((code(7) << (32- 2)) >> (32-8)) | (code(6) >>  4);
	bp[ 9] =				    (code(7) >>  2);
	break;
    case 11:
	bp[ 0] = ((code(0) << (32- 8)) >> (32-8));
	bp[ 1] = ((code(1) << (32- 5)) >> (32-8)) | (code(0) >>  8);
	bp[ 2] = ((code(2) << (32- 2)) >> (32-8)) | (code(1) >>  5);
	bp[ 3] = ((code(2) << (32-10)) >> (32-8));
	bp[ 4] = ((code(3) << (32- 7)) >> (32-8)) | (code(2) >> 10);
	bp[ 5] = ((code(4) << (32- 4)) >> (32-8)) | (code(3) >>  7);
	bp[ 6] = ((code(5) << (32- 1)) >> (32-8)) | (code(4) >>  4);
	bp[ 7] = ((code(5) << (32- 9)) >> (32-8));
	bp[ 8] = ((code(6) << (32- 6)) >> (32-8)) | (code(5) >>  9);
	bp[ 9] = ((code(7) << (32- 3)) >> (32-8)) | (code(6) >>  6);
	bp[10] =				    (code(7) >>  3);
	break;
    case 12:
	bp[ 0] = ((code(0) << (32- 8)) >> (32-8));
	bp[ 1] = ((code(1) << (32- 4)) >> (32-8)) | (code(0) >>  8);
	bp[ 2] =				    (code(1) >>  4);
	bp[ 3] = ((code(2) << (32- 8)) >> (32-8));
	bp[ 4] = ((code(3) << (32- 4)) >> (32-8)) | (code(2) >>  8);
	bp[ 5] =				    (code(3) >>  4);
	bp[ 6] = ((code(4) << (32- 8)) >> (32-8));
	bp[ 7] = ((code(5) << (32- 4)) >> (32-8)) | (code(4) >>  8);
	bp[ 8] =				    (code(5) >>  4);
	bp[ 9] = ((code(6) << (32- 8)) >> (32-8));
	bp[10] = ((code(7) << (32- 4)) >> (32-8)) | (code(6) >>  8);
	bp[11] =				    (code(7) >>  4);
	break;
    case 13:
	bp[ 0] = ((code(0) << (32- 8)) >> (32-8));
	bp[ 1] = ((code(1) << (32- 3)) >> (32-8)) | (code(0) >>  8);
	bp[ 2] =				    (code(1) >>  3);
	bp[ 3] = ((code(2) << (32- 6)) >> (32-8)) | (code(1) >> 11);
	bp[ 4] = ((code(3) << (32- 1)) >> (32-8)) | (code(2) >>  6);
	bp[ 5] =				    (code(3) >>  1);
	bp[ 6] = ((code(4) << (32- 4)) >> (32-8)) | (code(3) >>  9);
	bp[ 7] =				    (code(4) >>  4);
	bp[ 8] = ((code(5) << (32- 7)) >> (32-8)) | (code(4) >> 12);
	bp[ 9] = ((code(6) << (32- 2)) >> (32-8)) | (code(5) >>  7);
	bp[10] =				    (code(6) >>  2);
	bp[11] = ((code(7) << (32- 5)) >> (32-8)) | (code(6) >> 10);
	bp[12] =				    (code(7) >>  5);
	break;
    case 14:
	bp[ 0] = ((code(0) << (32- 8)) >> (32-8));
	bp[ 1] = ((code(1) << (32- 2)) >> (32-8)) | (code(0) >>  8);
	bp[ 2] =				    (code(1) >>  2);
	bp[ 3] = ((code(2) << (32- 4)) >> (32-8)) | (code(1) >> 10);
	bp[ 4] =				    (code(2) >>  4);
	bp[ 5] = ((code(3) << (32- 6)) >> (32-8)) | (code(2) >> 12);
	bp[ 6] =				    (code(3) >>  6);
	bp[ 7] = ((code(4) << (32- 8)) >> (32-8));
	bp[ 8] = ((code(5) << (32- 2)) >> (32-8)) | (code(4) >>  8);
	bp[ 9] =				    (code(5) >>  2);
	bp[10] = ((code(6) << (32- 4)) >> (32-8)) | (code(5) >> 10);
	bp[11] =				    (code(6) >>  4);
	bp[12] = ((code(7) << (32- 6)) >> (32-8)) | (code(6) >> 12);
	bp[13] =				    (code(7) >>  6);
	break;
    case 15:
	bp[ 0] = ((code(0) << (32- 8)) >> (32-8));
	bp[ 1] = ((code(1) << (32- 1)) >> (32-8)) | (code(0) >>  8);
	bp[ 2] =				    (code(1) >>  1);
	bp[ 3] = ((code(2) << (32- 2)) >> (32-8)) | (code(1) >>  9);
	bp[ 4] =				    (code(2) >>  2);
	bp[ 5] = ((code(3) << (32- 3)) >> (32-8)) | (code(2) >> 10);
	bp[ 6] =				    (code(3) >>  3);
	bp[ 7] = ((code(4) << (32- 4)) >> (32-8)) | (code(3) >> 11);
	bp[ 8] =				    (code(4) >>  4);
	bp[ 9] = ((code(5) << (32- 5)) >> (32-8)) | (code(4) >> 12);
	bp[10] =				    (code(5) >>  5);
	bp[11] = ((code(6) << (32- 6)) >> (32-8)) | (code(5) >> 13);
	bp[12] =				    (code(6) >>  6);
	bp[13] = ((code(7) << (32- 7)) >> (32-8)) | (code(6) >> 14);
	bp[14] =				    (code(7) >>  7);
	break;
    case 16:
        bp[ 0] = ((code(0) << (32- 8)) >> (32-8));
        bp[ 1] =				    (code(0) >>  8);
        bp[ 2] = ((code(1) << (32- 8)) >> (32-8));
        bp[ 3] =				    (code(1) >>  8);
        bp[ 4] = ((code(2) << (32- 8)) >> (32-8));
        bp[ 5] =				    (code(2) >>  8);
        bp[ 6] = ((code(3) << (32- 8)) >> (32-8));
        bp[ 7] =				    (code(3) >>  8);
        bp[ 8] = ((code(4) << (32- 8)) >> (32-8));
        bp[ 9] =				    (code(4) >>  8);
        bp[10] = ((code(5) << (32- 8)) >> (32-8));
        bp[11] =				    (code(5) >>  8);
        bp[12] = ((code(6) << (32- 8)) >> (32-8));
        bp[13] =				    (code(6) >>  8);
        bp[14] = ((code(7) << (32- 8)) >> (32-8));
        bp[15] =				    (code(7) >>  8);
	break;
    }

    thiscd (outbufcnt) -= thiscd (n_bits);
    thiscd (outbufptr) += thiscd (n_bits);
    thiscd (bytes_out) += thiscd (n_bits);

    thiscd (nextcode) = 0;

#ifdef DEBUG
     if ( thisopt (verbose) ) {
	int j;
	for (j = 0; j < 8; j++) {
	    int c = thiscd (codebuf[j]);
	    if (j) fprintf (thisopt (diagstream), ",");
	    if (c >= 040 && c <= 0176)
		fprintf (thisopt (diagstream), "%c", c);
	    else if (c == 10)
		fprintf (thisopt (diagstream), "\\n");
	    else
		fprintf (thisopt (diagstream), "%d", c);
	}
	fprintf (thisopt (diagstream), "\n");
     }
#endif /* DEBUG */

    /*
     * If the next entry is going to be too big for the code size,
     * then increase it, if possible.
     */
    if (thiscd (free_ent) > thiscd (maxcode) || (thiscd (clear_flg) > 0)) {
	if (thiscd (clear_flg)) {
	    thiscd (n_bits) = INIT_BITS;
	    thiscd (maxcode) = MAXCODE (thiscd (n_bits));
	    thiscd (clear_flg) = 0;
	} else {
	    thiscd (n_bits) ++;
	    if ( thiscd (n_bits) == thisopt (maxbits) )
		thiscd (maxcode) = thiscd (maxmaxcode);
	    else
		thiscd (maxcode) = MAXCODE(thiscd (n_bits));
	}
#ifdef DEBUG
	if (thisopt (debug)) {
	    fprintf (thisopt(diagstream), "\nChange to %d bits\n", thiscd (n_bits) );
	}
#endif /* DEBUG */
    }
    return 0;
}

static void
_comp_flush_codebuf (COMP_DATA *comp_data_)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */

    /*
     * At EOF, write the rest of the codebuf[].
     *
     * This is the only time we might output less than exactly 8 codes.
     * So this code:
     *	1) flushes what's already in outbuf (to keep the rest of this simple)
     *	2) lets _comp_output_codebuf() put a full line of 8 codes into outbuf.
     *	3) diddles with the outbufptr/outbufcnt to show just the (fewer)
     *	    actual number of output bytes as being there.
     * So the _comp_output_codebuf() method is tuned for the case of
     * writing exactly 8, and this code diddles with the output buffer
     * pointer after calling _comp_output_codebuf(), to make it look
     * look to _comp_flush_outbuf() like only the desired number of
     * bytes remain to be flushed.
     */

    _comp_flush_outbuf (my_comp_data, 0, 0);

    if (thiscd (nextcode)) {
	/* Output remaining partial (<= 8) set of codes in codebuf[]. */
	int bytes_to_output = (thiscd (nextcode) * thiscd (n_bits) + 7) / 8;
	/* first unused code should be 0 */
	if (thiscd (nextcode) < 8)
	    thiscd (codebuf[thiscd (nextcode)]) = 0;
	_comp_output_codebuf (my_comp_data);
	thiscd (outbufcnt) = thisopt (outbufsize) - bytes_to_output;
	thiscd (outbufptr) = thiscd (outbuf) + bytes_to_output;
	_comp_flush_outbuf (my_comp_data, 0, 0);
    }

#ifdef DEBUG
    if ( thisopt (verbose) )
	fprintf( thisopt(diagstream), "\n" );
#endif /* DEBUG */

}

int comp_geterrno (COMP_DATA *comp_data_)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */

    return thiscd (comp_errno);
}

void comp_clrerrno (COMP_DATA *comp_data_)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */

    thiscd (comp_errno) = 0;
}

#ifdef DEBUG

static code_int sorttab[HSIZE];	/* sorted pointers into htab */
#define stabof(i) (sorttab[i])
# define tab_suffixof(i)	((char_type *)(thiscd (htab)))[i]
# define de_stack		((char_type *)&tab_suffixof(1<<MAXBITS))
static int in_stack(COMP_DATA *, int, int);

void
comp_dump_tab (COMP_DATA *comp_data_)	/* dump string table */
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */
    register int i, first;
    register ent;
#define STACK_SIZE	15000
    int stack_top = STACK_SIZE;
    register c;
    unsigned mbshift;

    register int flag = 1;

    for(i=0; i<thiscd (hnumentries); i++) {	/* build sort pointers */
	if(htabof(i) >= 0)
	    stabof(codetabof(i)) = i;
    }
    first = thisopt (block_compress) ? FIRST : 256;
    if ((first%10) != 0)
	fprintf(thisopt(diagstream), "\n%5d: ", first);
    for(i = first; i < thiscd (free_ent); i++) {
	    if ((i%10) == 0)
		fprintf(thisopt(diagstream), "\n%5d: ", i);
	    if ((i%10) == 4) {
		de_stack[--stack_top] = ' ';
		de_stack[--stack_top] = ';';
	    } else if ((i%10) != 9) {
		de_stack[--stack_top] = ',';
	    }
	    de_stack[--stack_top] = '"';
	    stack_top = in_stack(my_comp_data, (htabof(stabof(i))>>thisopt (maxbits))&0xff,
				 stack_top);
	    mbshift = ((1 << thisopt (maxbits)) - 1) ;
	    ent = htabof(stabof(i)) & mbshift ;
	    while (ent > 256) {
		stack_top = in_stack(my_comp_data, htabof(stabof(ent)) >> thisopt (maxbits),
					stack_top);
		ent = htabof(stabof(ent)) & mbshift;
	    }
	    stack_top = in_stack(my_comp_data, ent, stack_top);
	    de_stack[--stack_top] = '"';
	    fwrite( &de_stack[stack_top], 1, STACK_SIZE-stack_top, thisopt(diagstream));
	    stack_top = STACK_SIZE;
    }
    fprintf (thisopt(diagstream), "\n");
}

static int
in_stack(COMP_DATA *comp_data_, int c, int stack_top)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */

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

#else

/* dump string table */
/*ARGSUSED*/
void comp_dump_tab(COMP_DATA *d) {}

#endif /* DEBUG */

static void
_comp_cl_block (COMP_DATA *comp_data_)	/* table clear for block compress */
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */
    register long int rat;

    thiscd (checkpoint) = thiscd (in_count) + CHECK_GAP;
#ifdef DEBUG
	if ( thisopt (debug) ) {
    		fprintf ( thisopt(diagstream), "count: %lld, ratio: ", thiscd (in_count) );
     		_comp_prratio (thisopt(diagstream), thiscd (in_count), thiscd (bytes_out) );
		fprintf ( thisopt(diagstream), "\n");
	}
#endif /* DEBUG */

    if(thiscd (in_count) > 0x007fffff) {	/* shift will overflow */
	rat = thiscd (bytes_out) >> 8;
	if(rat == 0) {				/* Don't divide by zero */
	    rat = 0x7fffffff;
	} else {
	    rat = thiscd (in_count) / rat;
	}
    } else {
	rat = (thiscd (in_count) << 8) / thiscd (bytes_out);
						/* 8 fractional bits */
    }
    if ( rat > thiscd (ratio) ) {
	thiscd (ratio) = rat;
    } else {
	thiscd (ratio) = 0;
#ifdef DEBUG
	if(thisopt (verbose))
		comp_dump_tab(my_comp_data);	/* dump string table */
#endif
 	_comp_cl_hash (my_comp_data);
	thiscd (free_ent) = FIRST;
	thiscd (clear_flg) = 1;
	putcode (CLEAR);
#ifdef DEBUG
	if(thisopt (debug))
    		fprintf ( thisopt(diagstream), "clear\n" );
#endif /* DEBUG */
    }
}

/* reset code table */
static void
_comp_cl_hash (COMP_DATA *comp_data_)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */
    memset (thiscd (htab), -1, thiscd (htabsize));
}

void
_comp_prratio (FILE *stream, long long num, int den)
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

/*
 * _comp_flush_outbuf()
 *
 * Internal routine called to flush output buffer
 * when putchar() macros finds that it is full, and
 * when comp_end() is called to finish compression.
 *
 * Return value of _comp_flush_outbuf() apparently unused,
 * except that the above ?: op in the putchar() macro
 * won't compile unless _comp_flush_outbuf returns something.
 */

static int
_comp_flush_outbuf (COMP_DATA *comp_data_, char_type x, int put_x_too)
{
    register COMP_DATA* my_comp_data = comp_data_; /* for thiscd() macro */
    ssize_t cnt_output;	    /* number bytes fp_output successfully wrote */
    ssize_t cnt_left;	    /* number of bytes left to output */
    char_type *buf_left;    /* starting address of remaining bytes to output */

    if (thiscd (comp_errno) != 0)
	return -1;
    buf_left = thiscd (outbuf);
    cnt_left = thiscd (outbufptr) - thiscd (outbuf);

    while (cnt_left > 0) {
	cnt_output = thiscd (fp_output)
				(thisopt (output_param), buf_left, cnt_left);
	if (cnt_output < 0) {
	    thiscd (comp_errno) = -1;
	    return -1;
	}
	buf_left += cnt_output;
	cnt_left -= cnt_output;
    }
    thiscd (outbufcnt) = thisopt (outbufsize);
    thiscd (outbufptr) = thiscd (outbuf);
    if (put_x_too)
	return putchar (x);
    return x;
}
