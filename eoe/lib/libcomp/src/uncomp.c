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
 * See associated header file uncomp.h for usage.
 *
 * To compress data, see the matching comp.h/comp.c files.
 */

#include <uncomp.h>
#include <string.h>

/* Defines for third byte of header */
#define BIT_MASK	0x1f
#define BLOCK_MASK	0x80

#define MAXBITS 16	    /* Upper limit on code size, in bits. */
#define HSIZE	69001	    /* 95% occupancy */
#define INIT_BITS 9	    /* initial number of bits/code */

/*
 * CODESETSIZE is the number of codes in each input set.
 * Since it is equal to the number of bits in a byte,
 * therefore each input code set takes an integral number
 * of compressed input data bytes, independent of the current
 * code size, or number bits per code, which varies between 9 and 16.
 */
#define CODESETSIZE 8

#define MAXCODE(n_bits)    ((1L << (n_bits)) - 1)

/*
 * C++ like -- Presume pointer to UNCOMP_DATA for
 *	       this stream is a local variable or argument
 *	       named "my_uncomp_data".  Resembles C++ "this".
 *	       "thisud" stands for: THIS Uncomp_Data struct.
 */

#define thisud(x) (my_uncomp_data->x)
#define thisopt(x) (thisud (options.x))

#define tab_prefixof(i) (thisud (prefixtab) [i])
#define tab_suffixof(i) (thisud (suffixtab) [i])
#define stackof(i)	(thisud (stack) [i])

#define FIRST	257	/* first free entry */
#define	CLEAR	256	/* table clear output code */

#define getcode()   ( \
			( \
			    (   thisud (clear_flg) == 0 && \
				thisud (nextcode) < thisud (endcode) && \
				thisud (free_ent) <= thisud (maxcode) \
			    ) || ( \
				_uncomp_fillcodebuf (my_uncomp_data) == 0 \
			    ) \
			) ? ( \
			    thisud (codebuf [thisud (nextcode) ++]) \
			) : ( \
			    -1 \
			) \
		    )

#undef putchar
#define putchar(x) ( ((-- thisud (outbufcnt)) < 0) ? \
		    _uncomp_flush_outbuf (my_uncomp_data, x, 1) : \
		    (* thisud (outbufptr) ++ = (char_type) (x)))

static int _uncomp_flush_outbuf (UNCOMP_DATA *, char_type, int);
static code_int _uncomp_fillcodebuf (UNCOMP_DATA *);
static int _uncomp_need_input (UNCOMP_DATA *, int how_many_need);

void uncomp_options_default (UNCOMP_OPTIONS *popt)
{
    popt->uncomp_magic_disposition = notstripped;
    popt->outbufsize = 64*1024;
    popt->output_param = 0;
    popt->quiet = 1;
    popt->verbose = 0;
    popt->debug = 0;
    popt->diagstream = stderr;
}

/*
 * initialize an UNCOMP_DATA structure using the given options,
 * get ready to uncompress
 */

int uncomp_begin(UNCOMP_DATA *uncomp_data_, UNCOMP_OPTIONS *uncomp_options_)
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisud() macro */

    thisud (options) = *uncomp_options_;

    thisud (first_uncompress_call) = 1;
    thisud (need_first_code) = 1;
    thisud (block_compress) = BLOCK_MASK;
    thisud (maxbits) = MAXBITS;
    thisud (n_bits) = INIT_BITS;
    thisud (maxcode) = 0;
    thisud (maxmaxcode) = 1L << MAXBITS;
    thisud (free_ent) = FIRST;
    thisud (clear_flg) = 0;
    thisud (uncomp_errno) = 0;
    thisud (end_of_input) = 0;

    if (thisud(codebuf) == 0) {
	thisud (codebuf) =
	    thisud (fp_allocator) (CODESETSIZE * sizeof (code_int));
	if (thisud (codebuf) == 0) {
	    thisud (uncomp_errno) = -1;
	    return -1;
	}
    }
    thisud (nextcode) = 0;
    thisud (endcode) = 0;

    thisud (inputbuf) = 0;
    thisud (nextinput) = 0;
    thisud (endinput) = 0;
    thisud (inputbufsource) = fromnowhere;

    thisud (appbuf) = 0;
    thisud (endapp) = 0;

    if (thisud(defragbuf) == 0) {
	thisud (defragbuf) = thisud (fp_allocator) (MAXBITS);
	if (thisud (defragbuf) == 0) {
	    thisud (uncomp_errno) = -1;
	    return -1;
	}
    }

    if (thisud(prefixtab) == 0) {
	thisud (prefixtabsize) = HSIZE * sizeof (thisud (prefixtab[0]));
	thisud (prefixtab) = thisud (fp_allocator) (thisud (prefixtabsize));
	if (thisud (prefixtab) == 0) {
	    thisud (uncomp_errno) = -1;
	    return -1;
	}
    }

    if (thisud(suffixtab) == 0) {
	thisud (suffixtabsize) = thisud (maxmaxcode) * sizeof (thisud (suffixtab[0]));
	thisud (suffixtab) = thisud (fp_allocator) (thisud (suffixtabsize));
	if (thisud (suffixtab) == 0) {
	    thisud (uncomp_errno) = -1;
	    return -1;
	}
    }

    /*
     * Actually only need 64K - 256 stack elements, since that is the
     * longest sequence of 16 bit codes from the alphabet (256 .. 64K)
     * without replacement possible.  Using maxmaxcode == 64K is slight
     * waste.  Input to the uncompressor the result of compressing a
     * sequence of the same byte repeated about 3 billion times to see
     * this worst case stack.
     */
    if (thisud(stack) == 0) {
	thisud (stacksize) = thisud (maxmaxcode) * sizeof (thisud (stack[0]));
	thisud (stack) = thisud (fp_allocator) (thisud (stacksize));
	if (thisud (stack) == 0) {
	    thisud (uncomp_errno) = -1;
	    return -1;
	}
    }

    if (thisud(outbuf) &&
	(thisopt(outbufsize) != thisud(outbuflast))) {
	thisud (fp_free) (thisud (outbuf));
	thisud(outbuf) = 0;
    }

    if (thisud(outbuf) == 0) {
	thisud (outbuf) = thisud (fp_allocator) (thisopt (outbufsize));
	if (thisud (outbuf) == 0) {
	    thisud (uncomp_errno) = -1;
	    return -1;
	}
    }
    thisud(outbuflast) = thisopt(outbufsize);
    thisud (outbufcnt) = thisopt (outbufsize);
    thisud (outbufptr) = thisud (outbuf);

    return 0;
}

void uncomp_init(UNCOMP_DATA *uncomp_data_, uncomp_allocator_t fp_allocator_,
		 uncomp_free_t fp_free_, uncomp_output_t fp_output_)
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisud() macro */

    thisud (fp_allocator) = fp_allocator_;
    thisud (fp_free) = fp_free_;
    thisud (fp_output) = fp_output_;

    thisud (codebuf) = 0;
    thisud (defragbuf) = 0;
    thisud (prefixtab) = 0;
    thisud (suffixtab) = 0;
    thisud (stack) = 0;
    thisud (outbuf) = 0;
    thisud(outbuflast) = 0;
}

void uncomp_destroy (UNCOMP_DATA *uncomp_data_)
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisud() macro */

    thisud (fp_free) (thisud (codebuf));   thisud (codebuf) = 0;
    thisud (fp_free) (thisud (defragbuf)); thisud (defragbuf) = 0;
    thisud (fp_free) (thisud (prefixtab)); thisud (prefixtab) = 0;
    thisud (fp_free) (thisud (suffixtab)); thisud (suffixtab) = 0;
    thisud (fp_free) (thisud (stack));     thisud (stack) = 0;
    thisud (fp_free) (thisud (outbuf));    thisud (outbuf) = 0;
}

int uncomp_geterrno (UNCOMP_DATA *uncomp_data_)
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisud() macro */

    return thisud (uncomp_errno);
}

void uncomp_clrerrno (UNCOMP_DATA *uncomp_data_)
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisud() macro */

    thisud (uncomp_errno) = 0;
}

/*
 * Old comments from ~1984 AT&T/Bell Labs version of this compress code:
 *
 * Algorithm from "A Technique for High Performance Data Compression",
 * Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 *
 * 	Modified Lempel-Ziv method (LZW).  Basically finds common
 * substrings and replaces them with a variable size code.  This is
 * deterministic, and can be done on the fly.  Thus, the decompression
 * procedure needs no input table, but tracks the way the table was built.
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

int
uncomp_uncompress (
    UNCOMP_DATA *uncomp_data_, const char_type *appbuf, long appbuflen)
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisud() macro */
    register code_int code;

    if (thisud (uncomp_errno))
	return -1;

    thisud (appbuf) = appbuf;
    thisud (endapp) = appbuflen;

    if (thisud (first_uncompress_call)) {

	switch (thisopt (uncomp_magic_disposition)) {
	    const char_type *magicp;

	    case notstripped:
	    case required:
		if (_uncomp_need_input (my_uncomp_data, 2) < 0)
		    return 0;	/* delay this until app provides more data */

		magicp = thisud (inputbuf) + thisud (nextinput);
		thisud (nextinput) += 2;

		if (magicp[0] != COMPRESS_MAGIC_1 ||
		    magicp[1] != COMPRESS_MAGIC_2
		) {
		    thisud (first_uncompress_call) = 0;
		    if (thisopt (uncomp_magic_disposition) == required) {
			thisud (uncomp_errno) = 3;
			return -1;
		    }
		    break;  /* equivalent to the missing case */
		}
		thisopt (uncomp_magic_disposition) = stripped;

		/*FALLTHRU*/

	    case stripped:
		if (_uncomp_need_input (my_uncomp_data, 1) < 0)
		    return 0;	/* delay this until app provides more data */
		thisud (first_uncompress_call) = 0;
		thisud (maxbits) = thisud (inputbuf [ thisud (nextinput) ++ ]);
		thisud (block_compress) = thisud (maxbits) & BLOCK_MASK;
		thisud (maxbits) &= BIT_MASK;
		thisud (maxmaxcode) = 1L << thisud (maxbits);
		if (thisud (maxbits) > MAXBITS) {
		    /* compressed with maxbits, can only handle MAXBITS */
		    thisud (uncomp_errno) = 1;
		    return -1;
		}
		break;

	    case missing:
		thisud (first_uncompress_call) = 0;
		/* no-op */
		break;
	}

	/* Initialize the first 256 entries in the tables. */
	for (code = 0; code < 256; code++)
	    tab_suffixof(code) = code;
	bzero (thisud (prefixtab), 256 * sizeof (thisud (prefixtab[0])));

	thisud (maxcode) = MAXCODE (thisud (n_bits) = INIT_BITS);
	thisud (free_ent) = ((thisud (block_compress)) ? FIRST : 256 );
    }

    if (thisud (need_first_code)) {
	code_int first_code = getcode();
	if (first_code < 0) {
	    if (thisud (uncomp_errno))
		return -1;
	    return 0;	    /* delay this until app provides more data */
	}
	thisud (need_first_code) = 0;

	thisud (finchar) = thisud (oldcode) = first_code;
	putchar (thisud (finchar));	/* first code must be 8 bits = char */
    }

    while ( (code = getcode()) >= 0 ) {
	register char_type *stackp = thisud (stack);
	register code_int incode;

	if (code == CLEAR && thisud (block_compress)) {
	    bzero (thisud (prefixtab), 256 * sizeof (thisud (prefixtab[0])));
	    thisud (clear_flg) = 1;
	    thisud (free_ent) = FIRST - 1;

	    if ( (code = getcode ()) < 0 )
		break;
	}
	incode = code;

	if ( code >= thisud (free_ent) ) {
	    /* two cases to consider here */

	    if (code == thisud (free_ent)) {
		/*
		 * We've been handed what will be the next allocated code,
		 * before we've had a chance to put it our tables.  The
		 * compressor generates this on 'aaa' repeating chars, when
		 * the char wasn't seen before and is repeated 3 times.
		 *
		 * I don't quite grok the next 2 lines, but they seem to
		 * unravel this case correctly.  The old code calls this the:
		 *	Special case for KwKwK string
		 * Whatever the h... that means.
		 */
		*stackp++ = thisud (finchar);
		code = thisud (oldcode);
	    } else {
		/* Code is too big -- corrupted input.  Bail out. */
		 thisud (uncomp_errno) = 2;
		 break;
	    }
	}

	/*
	 * Generate output characters in reverse order
	 */
	while ( code >= 256 ) {
	    *stackp++ = tab_suffixof(code);
	    code = tab_prefixof(code);
	}
	*stackp++ = thisud (finchar) = tab_suffixof(code);

	/*
	 * And put them out in forward order
	 */
	do
	    putchar ( *--stackp );
	while ( stackp > thisud (stack) );

	/*
	 * Generate the new entry.
	 */
	if ( (code = thisud (free_ent)) < thisud (maxmaxcode) ) {
	    tab_prefixof(code) = thisud (oldcode);
	    tab_suffixof(code) = thisud (finchar);
	    thisud (free_ent) = code+1;
	}
	/*
	 * Remember previous code.
	 */
	thisud (oldcode) = incode;
    }

    if (thisud (uncomp_errno))
	return -1;
    return 0;
}

int uncomp_end (UNCOMP_DATA *uncomp_data_)
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisucd() macro */

    thisud (end_of_input) = 1;
    uncomp_uncompress (my_uncomp_data, 0, 0);
    (void) _uncomp_flush_outbuf (my_uncomp_data, 0, 0);
    if (thisud (uncomp_errno))
	return -1;
    return 0;
}

static int _uncomp_flush_outbuf (UNCOMP_DATA *uncomp_data_, char_type x, int put_x_too)
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisud() macro */
    ssize_t cnt_output;	    /* number bytes fp_output successfully wrote */
    char_type *buf_left;    /* starting address of remaining bytes to output */
    ssize_t cnt_left;	    /* number of bytes left to output */

    if (thisud (uncomp_errno) != 0)
	return -1;
    buf_left = thisud (outbuf);
    cnt_left = thisud (outbufptr) - thisud (outbuf);

    while (cnt_left > 0) {
	cnt_output = thisud (fp_output)
				(thisopt (output_param), buf_left, cnt_left);
	if (cnt_output < 0) {
	    thisud (uncomp_errno) = -1;
	    return -1;
	}
	buf_left += cnt_output;
	cnt_left -= cnt_output;
    }
    thisud (outbufcnt) = thisopt (outbufsize);
    thisud (outbufptr) = thisud (outbuf);
    if (put_x_too)
	return putchar (x);
    return x;
}

static code_int
_uncomp_fillcodebuf (UNCOMP_DATA *uncomp_data_)
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisud() macro */
    register const char_type *ip;			/* cache inputbuf ptr */
    register int count_valid_bytes = thisud (n_bits); /* cnt bytes consumed */

    thisud (nextcode) = 0;
    thisud (endcode) = 0;

    if (thisud (clear_flg) > 0 ||
	thisud (endinput) - thisud (nextinput) < thisud (n_bits) ||
	thisud (free_ent) > thisud (maxcode)
    ) {
	int ret;

	/*
	 * If the next entry will be too big for the current code
	 * size, then we must increase the size.  This implies reading
	 * a new buffer full, too.
	 */
	if (thisud (free_ent) > thisud (maxcode)) {
	    thisud (n_bits)++;
	    if ( thisud (n_bits) == thisud (maxbits) )
		thisud (maxcode) = thisud (maxmaxcode);	/* won't get any bigger now */
	    else

    		thisud (maxcode) = MAXCODE(thisud (n_bits));
#if DEBUG
	    if (thisopt (printcodes))
		fprintf(thisopt (diagstream), "\nChange to %d bits\n", thisud (n_bits) );
#endif
	}
	if ( thisud (clear_flg) > 0) {
    	    thisud (maxcode) = MAXCODE (thisud (n_bits) = INIT_BITS);
	    thisud (clear_flg) = 0;
	}

	ret = _uncomp_need_input (my_uncomp_data, thisud (n_bits));
	if (ret < 0 && ! thisud (end_of_input))
	    return -1;	    /* delay this until app provides more data */
	count_valid_bytes = thisud (endinput) - thisud (nextinput);
	if (count_valid_bytes == 0 && thisud (end_of_input))
	    return -1;	    /* no more input, period. */
	if (count_valid_bytes > thisud (n_bits))
	    count_valid_bytes = thisud (n_bits);
    }

    ip = thisud (inputbuf) + thisud (nextinput);
    thisud (nextinput) += count_valid_bytes;
    thisud (endcode) = (count_valid_bytes * CODESETSIZE) /
				thisud (n_bits);

#define bp(x) ((uint32_t) (ip[x]))
#define code(x) thisud (codebuf[x])

    /*
     * Now inputbuf has at least enough bytes to decode 8 codes of the
     * current code size (n_bits).  If we are at end of input, then
     * not all these bytes are valid (*), in which case endcode has been
     * adjusted to the number of codes that will be valid.  But the
     * rest of this code doesn't care -- it just converts code size
     * bytes from inputbuf to 8 codes in codebuf, valid or not.
     *
     * (*) "not valid" means the bytes are there (you can load them)
     *	   but they're garbage, not real compressed data input.
     */

    switch (thisud (n_bits)) {
    case 9:
	code(0) = ((bp( 1) << (32-1)) >> (32- 9)) | (bp( 0) >> 0);
	code(1) = ((bp( 2) << (32-2)) >> (32- 9)) | (bp( 1) >> 1);
	code(2) = ((bp( 3) << (32-3)) >> (32- 9)) | (bp( 2) >> 2);
	code(3) = ((bp( 4) << (32-4)) >> (32- 9)) | (bp( 3) >> 3);
	code(4) = ((bp( 5) << (32-5)) >> (32- 9)) | (bp( 4) >> 4);
	code(5) = ((bp( 6) << (32-6)) >> (32- 9)) | (bp( 5) >> 5);
	code(6) = ((bp( 7) << (32-7)) >> (32- 9)) | (bp( 6) >> 6);
	code(7) = ((bp( 8) << (32-8)) >> (32- 9)) | (bp( 7) >> 7);
	break;
    case 10:
	code(0) = ((bp( 1) << (32-2)) >> (32-10)) | (bp( 0) >> 0);
	code(1) = ((bp( 2) << (32-4)) >> (32-10)) | (bp( 1) >> 2);
	code(2) = ((bp( 3) << (32-6)) >> (32-10)) | (bp( 2) >> 4);
	code(3) = ((bp( 4) << (32-8)) >> (32-10)) | (bp( 3) >> 6);
	code(4) = ((bp( 6) << (32-2)) >> (32-10)) | (bp( 5) >> 0);
	code(5) = ((bp( 7) << (32-4)) >> (32-10)) | (bp( 6) >> 2);
	code(6) = ((bp( 8) << (32-6)) >> (32-10)) | (bp( 7) >> 4);
	code(7) = ((bp( 9) << (32-8)) >> (32-10)) | (bp( 8) >> 6);
	break;
    case 11:
	code(0) = ((bp( 1) << (32-3)) >> (32-11)) | (bp( 0) >> 0);
	code(1) = ((bp( 2) << (32-6)) >> (32-11)) | (bp( 1) >> 3);
	code(2) = ((bp( 4) << (32-1)) >> (32-11)) |
		  ((bp( 3) << (32-8)) >> (32-10)) | (bp( 2) >> 6);
	code(3) = ((bp( 5) << (32-4)) >> (32-11)) | (bp( 4) >> 1);
	code(4) = ((bp( 6) << (32-7)) >> (32-11)) | (bp( 5) >> 4);
	code(5) = ((bp( 8) << (32-2)) >> (32-11)) |
		  ((bp( 7) << (32-8)) >> (32- 9)) | (bp( 6) >> 7);
	code(6) = ((bp( 9) << (32-5)) >> (32-11)) | (bp( 8) >> 2);
	code(7) = ((bp(10) << (32-8)) >> (32-11)) | (bp( 9) >> 5);
	break;
    case 12:
	code(0) = ((bp( 1) << (32-4)) >> (32-12)) | (bp( 0) >> 0);
	code(1) = ((bp( 2) << (32-8)) >> (32-12)) | (bp( 1) >> 4);
	code(2) = ((bp( 4) << (32-4)) >> (32-12)) | (bp( 3) >> 0);
	code(3) = ((bp( 5) << (32-8)) >> (32-12)) | (bp( 4) >> 4);
	code(4) = ((bp( 7) << (32-4)) >> (32-12)) | (bp( 6) >> 0);
	code(5) = ((bp( 8) << (32-8)) >> (32-12)) | (bp( 7) >> 4);
	code(6) = ((bp(10) << (32-4)) >> (32-12)) | (bp( 9) >> 0);
	code(7) = ((bp(11) << (32-8)) >> (32-12)) | (bp(10) >> 4);
	break;
    case 13:
	code(0) = ((bp( 1) << (32-5)) >> (32-13)) | (bp( 0) >> 0);
	code(1) = ((bp( 3) << (32-2)) >> (32-13)) |
		  ((bp( 2) << (32-8)) >> (32-11)) | (bp( 1) >> 5);
	code(2) = ((bp( 4) << (32-7)) >> (32-13)) | (bp( 3) >> 2);
	code(3) = ((bp( 6) << (32-4)) >> (32-13)) |
		  ((bp( 5) << (32-8)) >> (32- 9)) | (bp( 4) >> 7);
	code(4) = ((bp( 8) << (32-1)) >> (32-13)) |
		  ((bp( 7) << (32-8)) >> (32-12)) | (bp( 6) >> 4);
	code(5) = ((bp( 9) << (32-6)) >> (32-13)) | (bp( 8) >> 1);
	code(6) = ((bp(11) << (32-3)) >> (32-13)) |
		  ((bp(10) << (32-8)) >> (32-10)) | (bp( 9) >> 6);
	code(7) = ((bp(12) << (32-8)) >> (32-13)) | (bp(11) >> 3);
	break;
    case 14:
	code(0) = ((bp( 1) << (32-6)) >> (32-14)) | (bp( 0) >> 0);
	code(1) = ((bp( 3) << (32-4)) >> (32-14)) |
		  ((bp( 2) << (32-8)) >> (32-10)) | (bp( 1) >> 6);
	code(2) = ((bp( 5) << (32-2)) >> (32-14)) |
		  ((bp( 4) << (32-8)) >> (32-12)) | (bp( 3) >> 4);
	code(3) = ((bp( 6) << (32-8)) >> (32-14)) | (bp( 5) >> 2);
	code(4) = ((bp( 8) << (32-6)) >> (32-14)) | (bp( 7) >> 0);
	code(5) = ((bp(10) << (32-4)) >> (32-14)) |
		  ((bp( 9) << (32-8)) >> (32-10)) | (bp( 8) >> 6);
	code(6) = ((bp(12) << (32-2)) >> (32-14)) |
		  ((bp(11) << (32-8)) >> (32-12)) | (bp(10) >> 4);
	code(7) = ((bp(13) << (32-8)) >> (32-14)) | (bp(12) >> 2);
	break;
    case 15:
	code(0) = ((bp( 1) << (32-7)) >> (32-15)) | (bp( 0) >> 0);
	code(1) = ((bp( 3) << (32-6)) >> (32-15)) |
		  ((bp( 2) << (32-8)) >> (32- 9)) | (bp( 1) >> 7);
	code(2) = ((bp( 5) << (32-5)) >> (32-15)) |
		  ((bp( 4) << (32-8)) >> (32-10)) | (bp( 3) >> 6);
	code(3) = ((bp( 7) << (32-4)) >> (32-15)) |
		  ((bp( 6) << (32-8)) >> (32-11)) | (bp( 5) >> 5);
	code(4) = ((bp( 9) << (32-3)) >> (32-15)) |
		  ((bp( 8) << (32-8)) >> (32-12)) | (bp( 7) >> 4);
	code(5) = ((bp(11) << (32-2)) >> (32-15)) |
		  ((bp(10) << (32-8)) >> (32-13)) | (bp( 9) >> 3);
	code(6) = ((bp(13) << (32-1)) >> (32-15)) |
		  ((bp(12) << (32-8)) >> (32-14)) | (bp(11) >> 2);
	code(7) = ((bp(14) << (32-8)) >> (32-15)) | (bp(13) >> 1);
	break;
    case 16:
	code(0) = ((bp( 1) << (32-8)) >> (32-16)) | (bp( 0) >> 0);
	code(1) = ((bp( 3) << (32-8)) >> (32-16)) | (bp( 2) >> 0);
	code(2) = ((bp( 5) << (32-8)) >> (32-16)) | (bp( 4) >> 0);
	code(3) = ((bp( 7) << (32-8)) >> (32-16)) | (bp( 6) >> 0);
	code(4) = ((bp( 9) << (32-8)) >> (32-16)) | (bp( 8) >> 0);
	code(5) = ((bp(11) << (32-8)) >> (32-16)) | (bp(10) >> 0);
	code(6) = ((bp(13) << (32-8)) >> (32-16)) | (bp(12) >> 0);
	code(7) = ((bp(15) << (32-8)) >> (32-16)) | (bp(14) >> 0);
	break;
    }

#ifdef DEBUG
     if ( thisopt (printcodes) ) {
	int j;
	for (j = 0; j < 8; j++) {
	    int c = code (j);
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
#endif
    return 0;
}

/*
 * See to it that inputbuf[] describes at least "how_many_need" bytes.
 *
 * There might be some bytes left over in defragbuf[] from a
 * previous call to _uncomp_need_input(), or there might be
 * (usually will be) some more bytes in the application provided
 * appbuf[].
 *
 * If unable to completely satisfy the request, then do a couple
 * slightly odd things anyway:
 *   1) See to it that the place inputbuf[] points to has "how_many_need"
 *	bytes of real, accessible memory, even if (by the setting
 *	of endinput) not all these bytes are valid (actual app
 *	provided input compressed data codes).
 *   2) Leave _no_ data left in appbuf[].  If there is some data,
 *	but not enough to meet the request, see to it that this
 *	data is moved from appbuf[] to defragbuf[] (which is where
 *	inputbuf[] will end up pointing in this case).
 *
 * Return 0 if request fully met, else return -1.
 *
 * Don't call _uncomp_need_input() with how_many_need > MAXBITS (16).
 * That's how big defragbuf[] is, and this code assumes that defragbuf[]
 * is big enough to hold how_many_need bytes.
 *
 */
static int _uncomp_need_input (UNCOMP_DATA *uncomp_data_, int how_many_need)
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisud() macro */
    int whats_left = thisud (endinput) - thisud (nextinput);

    if (whats_left >= how_many_need)
	return 0;				/* success */

    if (thisud (inputbufsource) != fromappbuf) {
	/* input is coming from defragbuf or nowhere */

	if (whats_left == 0 || thisud (inputbufsource) == fromnowhere) {
	    /* nothing here: switch back to appbuf */
	    thisud (inputbuf) = thisud (appbuf);
	    thisud (nextinput) = 0;
	    whats_left = thisud (endinput) = thisud (endapp);
	    thisud (inputbufsource) = fromappbuf;
	    if (whats_left >= how_many_need)
		return 0;			/* success */
	    /* fall through to appbuf case ... */
	} else {
	    /* defragbuf is not empty, but not enough */

	    /* shift what's left back to the beginning of defragbuf */
	    if (thisud (nextinput) != 0) {
		memmove (
			thisud (defragbuf),
			(void *) (thisud (defragbuf) + thisud (nextinput)),
			whats_left
		);
		thisud (nextinput) = 0;
		thisud (endinput) = whats_left;
	    }

	    /* try to steal what we need from appbuf */
	    if (thisud (endapp) > 0) {
		/* amount_to_steal = min (remaining_need, whats_available); */
		int amount_to_steal = how_many_need - whats_left;
		if (amount_to_steal > thisud (endapp))
		    amount_to_steal = thisud (endapp);

		if (amount_to_steal > 0) {
		    /* assumes defragbuf[] can hold how_many_need bytes */
		    memmove (
			    (void *) (thisud (inputbuf) + thisud (endinput)),
			    thisud (appbuf),
			    amount_to_steal
		    );
		    thisud (endinput) += amount_to_steal;
		    whats_left += amount_to_steal;
		    thisud (appbuf) += amount_to_steal;
		    thisud (endapp) -= amount_to_steal;
		}
	    }

	    /* now is there enough in defragbuf? */
	    if (whats_left >= how_many_need)
		return 0;			/* success */
	    return -1;				/* failure */
	}
    }

    /*
     * Input is coming from appbuf, and isn't enough.
     * We'll have to switch to defragbuf, to meet guarantee
     * that where inputbuf points has how_many_need accessible
     * bytes, even if they're not all valid.
     */

    thisud (appbuf) += thisud (nextinput);
    thisud (endapp) -= thisud (nextinput);

    thisud (inputbuf) = thisud (defragbuf);
    thisud (nextinput) = 0;
    thisud (endinput) = 0;
    thisud (inputbufsource) = fromdefragbuf;

    if (thisud (endapp) > 0) {
	memmove ((void *) thisud (inputbuf), thisud (appbuf), thisud (endapp));
	thisud (endinput) = thisud (endapp);
	thisud (endapp) = 0;
    }

    return -1;					/* failure */
}

#ifdef DEBUG

int in_stack (UNCOMP_DATA *, int, int);

void
uncomp_dump_tab (UNCOMP_DATA *uncomp_data_)	/* dump string table */
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisud() macro */
    register int i;
    register ent;
    int stack_top = thisud (stacksize);
    register c;

    for ( i = 0; i < thisud (free_ent); i++ ) {
       ent = i;
       c = tab_suffixof(ent);
       if (isascii(c) && isprint(c))
	   fprintf (thisopt (diagstream), "%5d: %5d/'%c'  \"", ent, tab_prefixof(ent), c);
       else
	   fprintf (thisopt (diagstream), "%5d: %5d/\\%03o \"", ent, tab_prefixof(ent), c);
       stackof(--stack_top) = '\n';
       stackof(--stack_top) = '"';
       while (ent != 0) {
	   stack_top = in_stack (my_uncomp_data, tab_suffixof(ent), stack_top);
	   ent = (ent >= FIRST ? tab_prefixof(ent) : 0);
       }
       fwrite (&stackof(stack_top), 1, thisud (stacksize) - stack_top, thisopt (diagstream));
       stack_top = thisud (stacksize);
    }
}

int
in_stack(UNCOMP_DATA *uncomp_data_, int c, int stack_top)
{
    register UNCOMP_DATA* my_uncomp_data = uncomp_data_; /* for thisud() macro */

    if ( (isascii(c) && isprint(c) && c != '\\') || c == ' ' ) {
	stackof(--stack_top) = c;
    } else {
	switch( c ) {
	case '\n': stackof(--stack_top) = 'n'; break;
	case '\t': stackof(--stack_top) = 't'; break;
	case '\b': stackof(--stack_top) = 'b'; break;
	case '\f': stackof(--stack_top) = 'f'; break;
	case '\r': stackof(--stack_top) = 'r'; break;
	case '\\': stackof(--stack_top) = '\\'; break;
	default:
	    stackof(--stack_top) = '0' + c % 8;
	    stackof(--stack_top) = '0' + (c / 8) % 8;
	    stackof(--stack_top) = '0' + c / 64;
	    break;
	}
	stackof(--stack_top) = '\\';
    }
    return stack_top;
}
#else
/*ARGSUSED*/
void uncomp_dump_tab(UNCOMP_DATA *d) {}
#endif /* DEBUG */
