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

#ifndef __UNCOMP_H__
#define __UNCOMP_H__

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Data compression streams: uncompression support
 */

/*
 * Guts of bsd/compress/compress.c, ripped out to form
 * a subroutine package usable by applications so that they can
 * compress/uncompress without invoking separate executables.
 *
 * Applications wishing to uncompress data should:
 *
 *  1) include this uncomp.h header file
 *  2) define an instance of a UNCOMP_DATA stream (let's call it xxx)
 *  3) define memory allocator method: void *xxxmalloc(size_t)
 *  4) define corresponding memory free method: void xxxfree(buf)
 *  5) define output handler method: ssize_t xxxoutput(buf, buflen)
 *  6) define an instance of UNCOMP_OPTIONS options structure: xxxopts
 *  7) invoke uncomp_options_default(&xxxopts) to set default options
 *  8) invoke uncomp_init(&xxx, &xxxmalloc, &xxxfree, &xxxoutput)
 * --- main decompression loop ---
 *  9) explicitly set any non-default options desired in xxxopts
 *  10) invoke uncomp_begin(&xxx, &xxxopts)
 *  11) repeatedly invoke uncomp_uncompress(&xxx, buf, buflen)
 *  12) invoke uncomp_end(&xxx)
 * --- end main loop ---
 *  13) it's ok to reuse uncompress structs for other compressed
 *	streams, by invoking uncomp_begin() on them again.
 *  14) invoke uncomp_destroy(&xxx) to free up allocated memory
 *
 * Expect during above:
 *  1) calls to xxxmalloc(size_t) during uncomp_begin()
 *      for space that uncompressor might need
 *  2) calls to xxxoutput(buf, buflen) during uncomp_compress()
 *	and uncomp_end(), to emit uncompressed results.
 *  3) calls to xxxfree(buf) during uncomp_destroy() and uncomp_begin().
 *
 * The observant will guess that I also program in C++ ...  pj.
 *
 * To compress data, see the matching comp.h/comp.c files.
 */

#ifndef __compress_types__
#define __compress_types__

typedef int32_t code_int;   /* Must hold 2**MAXBITS values, as well as -1 */
typedef int32_t count_int;
typedef	unsigned char char_type;

#endif /* !__compress_types__ */

#define COMPRESS_MAGIC_1 ((unsigned char)(0x1F))
#define COMPRESS_MAGIC_2 ((unsigned char)(0x9D))

/*
 * compress.c - File compression ala IEEE Computer, June 1984.
 *
 * Authors:	Spencer W. Thomas	(decvax!harpo!utah-cs!utah-gr!thomas)
 *		Jim McKie		(decvax!mcvax!jim)
 *		Steve Davies		(decvax!vax135!petsd!peora!srd)
 *		Ken Turkowski		(decvax!decwrl!turtlevax!ken)
 *		James A. Woods		(decvax!ihnp4!ames!jaw)
 *		Joe Orost		(decvax!vax135!petsd!joe)
 */

/*
 * Reworked for SGI only as separate linkable routines Dec 1995, by:
 *		Paul Jackson		pj@sgi.com
 *
 * I removed the code specific to 16 bit and VAX systems, because
 * I figured that the increased costs to SGI of maintenance caused by
 * the code confusion resulting from this unused code were greater
 * than the increased costs of maintenance caused by dropping any
 * pretense of supporting small systems, such as 8086's, or special
 * VAX bit-field instructions.  The insertion and extraction of
 * variable length (9 to 16 bit) compression codes has been reoptimized
 * for systems that have barrel shifters.  The code "should" work on
 * on any system with 32 or more bit flat addressing and either endian.
 */


/*
 * Three function typedefs, for the three application provided
 * methods to allocate and free memory, and to output the resulting
 * uncompressed data.
 *
 * The uncomp_init() method, below, expects one of each of these
 * function types to be passed in, for use by this compression stream.
 */

typedef void *(*uncomp_allocator_t)(size_t);
    /*
     * Memory allocator method type.
     * Should allocate and return pointer to specified number
     * of bytes of memory.  Or else return 0 if unable to allocate.
     * The libc "malloc" method can be used for an instance of this type.
     */

typedef void (*uncomp_free_t)(void *);
    /*
     * Memory free method type.
     * Should deallocate the pointed to memory, under the assumption
     * that this is memory that was provided by the corresponding
     * allocator method.  If "malloc" is the allocator,  then "free"
     * can be used for an instance of this type.
     */

typedef ssize_t (*uncomp_output_t)(void *param, void *buf, size_t buflen);
    /*
     * Data output method type.
     * Should send the described bytes on their merry way.
     * The uncompressor assumes that it can reuse the memory
     * that contained these bytes as soon as the output call
     * claims to have output them.  The output method should
     * return a count of the bytes successfully output, or -1
     * on error.  The compressor stream will repeatedly invoke
     * the output method on the remaining bytes in the event
     * of a partial output (return value less than entire buffer
     * to be output).  A simple wrapper to the libc "write"
     * method, that provides a file descriptor, can be used
     * for an instance of this type.  The value of param is
     * taken from the application controlled setting of the
     * output_param option.
     */


/*
 * The following flags and values are provided to enable the
 * invoking application to control the detailed behaviour of
 * the uncompressor.  Use the method uncomp_options_default() to
 * set all these options to their default value, then explicitly
 * modify the options for which some other value is desired, then
 * pass in the resulting options as part of the uncomp_begin() call.
 */


typedef struct uncomp_options {
    /*
     * The items in this struct are "public", in the C++ sense.
     * The application is invited to know, access and modify these
     * items by name.
     */

    enum { notstripped, stripped, missing, required } uncomp_magic_disposition;
	 /*
	  * Current compressed data streams have a 2-byte magic number header.
	  * From the compress code, apparently an old compressed format
	  * lacked this header.
	  * Some applications will find it convenient to read this
	  * header themselves, before deciding that the data stream is
	  * indeed compressed data and passing what's left in the stream
	  * to the decompressor.
	  * Other applications will prefer to pass the entire stream with
	  * header to the decompressor, and let it handle the magic stuff.
	  *
	  * The uncomp_magic_disposition flag lets the application control
	  * the compressors disposition of this magic header:
	  *
	  *	notstripped	The default value: no magic number has
	  *			stripped.  Decompressor should accept both
	  *			streams with the magic, and old streams
	  *			without it.
	  *
	  *	stripped	Magic number was present and stripped.
	  *
	  *	missing		There was no magic number (old format).
	  *
	  *	required	Decompressor will require that the stream
	  *			starts with the magic number, and will
	  *			fail, setting the errno to 3, if it doesn't.
	  *
	  * Observe that "stripped" is not the same as "missing", because
	  * the decompressor has different defaults in these two cases:
	  * namely if there was a magic number, then the next byte in the
	  * compressed data stream has a special meaning.
	  */

    size_t outbufsize;			/* total outbuf[] size to allocate */
	 /*
	  * Set the dynamically allocated output buffer size.
	  * Defaults to 64 Kb.
	  */

    void *output_param;			/* pass param through to fp_output() */
	 /*
	  * To make it easier for an application to have one fp_output()
	  * method handling multiple compression/decompression streams,
	  * an application can set this option to point to whatever data
	  * it wants passed through to the fp_output() method.  In the
	  * simplest case, it might point to a simple int file descriptor,
	  * telling fp_output() where to write the buffer.  Default is 0.
	  */

    int quiet;				/* set by default; comp stats if 0 */
	 /*
	  * Set the quiet flag for the specified compression stream.  By
	  * default this flag is 1.  Set it to 0 for compression statistics.
	  */

    int verbose;			/* If -DDEBUG, dump debug info */
	 /*
	  * Set the verbose flag for the specified compression stream.  By
	  * default this flag is 0.  Set it to 1 to dump debugging
	  * information.  Only has affect if uncomp.c compiled -DDEBUG.
	  */

    int debug;				/* If -DDEBUG, dump more debug info */
	 /*
	  * Set the debug flag for the specified compression stream.  By
	  * default this flag is 0.  Set it to 1 to dump debugging
	  * information.  Only has affect if uncomp.c compiled -DDEBUG.
	  * Causes even more debug information to dump than verbose.
	  */

    int printcodes;			/* If -DDEBUG, just print codes */
	 /*
	  * If compiled -DDEBUG and this flag is set, then uncompressing
	  * a stream causes diagnostic output displaying each
	  * compression code.  To emulate the "compress -Dd" behaviour,
	  * which is to display the compression codes, but not create
	  * the uncompressed file, see to it that your output method
	  * is a successful no-op.
	  */

    FILE *diagstream;			/* debug, stats written here */
	/*
	 * Set the output FILE stream to which -DDEBUG information, and
	 * if quiet is turned off, compression ratio, is written.
	 * Defaults to stderr.
	 */

} UNCOMP_OPTIONS;


/*
 * A data compression stream: the private data.
 */

typedef struct uncomp_data {
    /*
     * This entire struct is "private", in the C++ sense.
     * Use the methods that are declared below to access
     * the member data of this struct.
     */

/* internal uncompressor state */
    int first_uncompress_call;		/* set once uncomp_uncompress called */
    int need_first_code;		/* clr'd when first code byte seen */
    int block_compress;			/* enables restarting codes */
    int maxbits;			/* input settable max # bits/code */
    int n_bits;				/* number of bits/code */
    code_int maxcode;			/* maximum code, given n_bits */
    code_int maxmaxcode;		/* should NEVER generate this code */
    code_int free_ent;			/* next unused entry */
    int finchar;			/* first input char */
    code_int oldcode;			/* Remember previous code */
    int clear_flg;			/* set when clearing hash table */
    int uncomp_errno;			/* set if something fails */
    int end_of_input;			/* uncomp_end() sets: no more input */

/* application provided methods */
    uncomp_allocator_t fp_allocator;	/* application provided malloc */
    uncomp_free_t fp_free;		/* application provided free */
    uncomp_output_t fp_output;		/* application provided output */

/*
 * The current <= 8 compressed codes to decompress.
 *
 * The decompressor (_uncomp_getcode(), actually) always
 * (until end_of_input is set) consumes its compressed input
 * exactly 8 codes at a time.
 *
 * Codes vary in size from 9 to 16 bits, and the 8 consecutive codes
 * in each set all have the same code size.  Hence a set of 8 codes
 * varies in size from 9 to 16 bytes, and each set of 8 codes obeys
 * byte boundaries, for easier input.
 *
 * The main decompressor loop uses the getcode() macro to extract
 * the next valid code from codebuf[].  That macro invokes
 * _uncomp_getcode() to refill codebuf[] as needed.
 */
    code_int *codebuf;			/* current <= 8 compressed codes */
    int nextcode;			/* next input code in codebuf[] */
    int endcode;			/* offset of 1st invalid code */

/*
 * The current input byte stream of compressed data.
 * Usually inputbuf points to within appbuf[], but it points into
 * defragbuf[] when appbuf[] doesn't have enough bytes to
 * form 8 codes at the current code size.
 */
    const char_type *inputbuf;		/* current compressed data input */
    int nextinput;			/* next input byte in inputbuf[] */
    int endinput;			/* offset of 1st invalid input byte */
    enum { fromdefragbuf, fromappbuf, fromnowhere } inputbufsource;

/*
 * The buffer of input compressed currently presented by the application
 * to the uncomp_uncompress() method.
 */
    const char_type *appbuf;		/* current app provided input */
    int endapp;				/* offset of 1st invalid app byte */

/* reassemble input fragments into nbit-sized byte (i.e., 8 code) sequences */
    char_type *defragbuf;		/* reassemble input fragments here */

/* dynamically allocated prefix and suffix tables */
    unsigned short *prefixtab;		/* dynamically allocated prefix tab */
    code_int prefixtabsize;		/* total prefixtab[] size */
    char *suffixtab;			/* dynamically allocated suffix tab */
    code_int suffixtabsize;		/* total suffixtab[] size */

/* generate output characters in reverse order in this stack */
    char_type *stack;			/* dynamically allocated output stack */
    code_int stacksize;			/* total stack[] size */

/* uncomp_uncompress() buffers output here until calling fp_output() method */
    char_type *outbuf;			/* dynamically allocated output buf */
    long outbufcnt;			/* number available chars in buffer */
    long outbuflast;			/* previous value of outbufsize */
    char_type *outbufptr;		/* next char goes here in buffer */

/* application controlled option flags */
    UNCOMP_OPTIONS options;		/* copy uncomp_init() provided opts */

} UNCOMP_DATA;


/*
 * The following methods are provided for use on streams to be uncompressed.
 * Each method takes an explicit pointer to a UNCOMP_DATA stream as its
 * first parameter, rather like the implied "this" pointer in C++.
 */


void uncomp_options_default(UNCOMP_OPTIONS *);
/*
 * Initialize an UNCOMP_OPTIONS structure to the standard default values.
 * Use this function before calling uncomp_init().
 */


void uncomp_init(UNCOMP_DATA *, uncomp_allocator_t, uncomp_free_t,
		 uncomp_output_t);
    /*
     * Initialize (or reinitialize) an UNCOMP_DATA structure, including
     *      setting the function pointers for the
     *      allocator, free and output methods to the passed in values.
     */

int uncomp_begin(UNCOMP_DATA *, UNCOMP_OPTIONS *);
    /*
     * Call before each uncompression to re-initialize the UNCOMP_DATA
     *      structure, resetting the UNCOMP_OPTIONS.
     *	    returns 0 on success, -1 on failure.
     * Can fail if invocations of allocator fail during initialization.
     * The allocator will be called during uncomp_begin().
     * The free might be called during subsequent uncomp_destroy()
     *	    or uncomp_begin() invocations.
     * The output might be called during subsequent uncomp_uncompress() or
     *	    uncomp_end() invocations.
     */

int uncomp_uncompress (UNCOMP_DATA *, const char_type *buf, long buflen);
    /*
     * Provide some more data at location buf, length buflen,
     * to be uncompressed by the specified uncompression stream.
     * Returns 0 if reaches end of provided input without error.
     * Returns -1 on failure.  Can fail on internal error checking
     * or if an invocation of the output method returns -1.
     */

int uncomp_end (UNCOMP_DATA *);
    /*
     * Notify compression stream that input data has ended.
     * Should cause stream to uncompress and output any remaining
     * data still in its buffers.  Return 0 on success, -1 on failure.
     * Can fail on internal error checking or if an invocation of
     * the output method returns -1.
     */

void uncomp_destroy (UNCOMP_DATA *);
    /*
     * Destroy specified uncompression stream.  Primary consequence
     * is to invoke the previously provided free method to free
     * up any internally allocated buffer space.
     */

int uncomp_geterrno (UNCOMP_DATA *);
    /*
     * Return value of last internal error number.
     *
     * Error code 1 means input was compressed with more than
     * MAXBITS == 16 bits per code -- this decompressor can't cope.
     *
     * Error code 2 means that the input compressed data stream
     * is corrupt -- it contains an invalid compressed data code.
     *
     * Error code 3 means that uncomp_magic_disposition option was
     * set to "required", but the compressed data stream didn't
     * start with the correct magic number.
     *
     * Error code -1 means that a call to an application provided
     * malloc, free or output method failed (as defined above).
     *
     * Error code 0 means no error has been observed.
     */

void uncomp_clrerrno (UNCOMP_DATA *);
    /*
     * Clear (set to 0) the last internal error number for this stream.
     * Calls to uncomp_uncompress() return with no work done if this error
     * number is currently not zero.
     */

void uncomp_dump_tab (UNCOMP_DATA *);
    /*
     * Dump the dictionary. No-op unless uncomp.c compiled -DDEBUG.
     */

#ifdef __cplusplus
}
#endif
#endif /* !__UNCOMP_H__ */
