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

#ifndef __COMP_H__
#define __COMP_H__

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Data compression streams: compression support
 * 
 * NOTE: There is an alternative compression library that
 *	 firsttime users of this library might also want to
 *	 consider, depending on their needs.  That's zlib.
 *
 *	 As of this writing (June 1996) zlib is part of the
 *	 sources for ImageMagick_freeware, cosmocreate_1.0,
 *	 netscape/plugin-src/RGB_PNGPlug, webmagic_3.0/lib,
 *	 ifl_1.0/src/png and macromedia.  Look for zlib.h.
 *
 *	 If I had known of zlib earlier, I might not have
 *	 implemented this library version of compress.
 *
 *						-- pj
 */

/*
 * Guts of bsd/compress/compress.c, ripped out to form
 * a subroutine package usable by applications so that they can
 * compress/uncompress without invoking separate executables.
 *
 * Applications wishing to compress data should:
 *
 *  1) include this comp.h header file
 *  2) define an instance of a COMP_DATA stream (let's call it xxx)
 *  3) define memory allocator method: void *xxxmalloc(size_t)
 *  4) define corresponding memory free method: void xxxfree(buf)
 *  5) define output handler method: ssize_t xxxoutput(buf, buflen)
 *  6) define an instance of COMP_OPTIONS options structure: xxxopts
 *  7) invoke comp_options_default(&xxxopts) to set default options
 *  8) invoke comp_init(&xxx, &xxxmalloc, &xxxfree, &xxxoutput)
 * --- main compression loop ---
 *  9) explicitly set any non-default options desired in xxxopts
 *  10) invoke comp_begin(&xxx, &xxxopts)
 *  11) repeatedly invoke comp_compress(&xxx, buf, buflen)
 *  12) invoke comp_end(&xxx)
 * --- end main loop ---
 *  13) it's ok to reuse compress structs for other compressed
 *	streams, by invoking comp_begin() on them again.
 *  14) invoke comp_destroy(&xxx) to free up allocated memory
 *
 * Expect during above:
 *  1) calls to xxxmalloc(size_t) during comp_begin()
 *      for space that compressor might need
 *  2) calls to xxxoutput(buf, buflen) during comp_compress()
 *	and comp_end(), to emit compressed results.
 *  3) calls to xxxfree(buf) during comp_destroy() and comp_begin().
 *
 * The observant will guess that I also program in C++ ...  pj.
 *
 * To uncompress data, see the matching uncomp.h/uncomp.c files.
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
 *
 * Performance tests suggest that these various optimizations improve
 * the system time for compression by over 30% (thanks to bigger reads
 * or mmapping by the compress command, when recoded to make use of these
 * routines) and the number of application cpu cycles to compress the
 * 3 Mb /unix on my system from 546,579,559 to 369,443,157 for an
 * reduction of 32.5%.
 *
 * Decompression performance gains are less, with application cpu
 * cycles to decompress the above compressed /unix reduced from
 * 304,686,267 to 260,303,793 for a reduction of 14.6%.
 *
 * Combined average reduction in time to compress and decompress a
 * number of medium to large files is about 20% real time and about
 * 22-24% user+sys cpu times, where each compression and each
 * decompression operation is given equal weight in the average.
 *
 * In sum, the compressor is about 30% faster and the decompressor is
 * about 15% faster, for an average improvement of perhaps 22%.
 */


/*
 * Three function typedefs, for the three application provided
 * methods to allocate and free memory, and to output the resulting
 * compressed data.
 *
 * The comp_init() method, below, expects one of each of these
 * function types to be passed in, for use by this compression stream.
 */

typedef void *(*comp_allocator_t)(size_t);
    /*
     * Memory allocator method type.
     * Should allocate and return pointer to specified number
     * of bytes of memory.  Or else return 0 if unable to allocate.
     * The libc "malloc" method can be used for an instance of this type.
     */

typedef void (*comp_free_t)(void *);
    /*
     * Memory free method type.
     * Should deallocate the pointed to memory, under the assumption
     * that this is memory that was provided by the corresponding
     * allocator method.  If "malloc" is the allocator,  then "free"
     * can be used for an instance of this type.
     */

typedef ssize_t (*comp_output_t)(void *param, void *buf, size_t buflen);
    /*
     * Data output method type.
     * Should send the described bytes on their merry way.
     * The compressor assumes that it can reuse the memory
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
 * the compressor.  Use the method comp_options_default() to
 * set all these options to their default value, then explicitly
 * modify the options for which some other value is desired, then
 * pass in the resulting options as part of the comp_begin() call.
 */

typedef struct comp_options {
    /*
     * The items in this struct are "public", in the C++ sense.
     * The application is invited to know, access and modify these
     * items by name.
     */

    int block_compress;			/* enables restarting codes */
	 /*
	  * By default, this is set, allowing the compressor, after all
	  * codes are used up and compression rate drops, to start all
	  * over.  Set this flag to 0 to disable this feature.
	  */

    int nomagic;			/* Skip 3-byte magic number header? */
	 /*
	  * Set the nomagic flag for the specified compression stream.  By
	  * default this flag is 0.  Set it to 1 for generate a compressed
	  * stream that lacks the 2-byte magic number header.
	  */

    int maxbits;			/* user settable max # bits/code */
	 /*
	  * Set the maxbits value for the specified compression stream.  By
	  * default this value is MAXBITS.  It must be in the range
	  * [9..MAXBITS].
	  */

    code_int maxfilesize;		/* for dynamic hash table sizing */
	 /*
	  * Used to reduce the size of the hash table actually used
	  * for small files.  Defaults to 0, resulting in hnumentries of 69001,
	  * for 95% occupancy in the MAXBITS == 16 case.  Set to an upper
	  * bound of the number of bytes to be compressed, and if this size
	  * happens to be in the interval (1, 47000), comp_init() will speed
	  * things up a bit by initializing only the portion of the hash table
	  * that will actually be needed.
	  */

    size_t outbufsize;			/* total outbuf[] size to be allocated */
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
	  * information.  Only has affect if comp.c compiled -DDEBUG.
	  */

    int debug;				/* If -DDEBUG, dump more debug info */
	 /*
	  * Set the debug flag for the specified compression stream.  By
	  * default this flag is 0.  Set it to 1 to dump debugging
	  * information.  Only has affect if comp.c compiled -DDEBUG.  Causes
	  * even more debug information to dump than verbose.
	  */

    FILE *diagstream;			/* debug, stats written here */
	/*
	 * Set the output FILE stream to which -DDEBUG information, and
	 * if quiet is turned off, compression ratio, is written.
	 * Defaults to stderr.
	 */

} COMP_OPTIONS;


/*
 * A data compression stream: the private data.
 */

typedef struct comp_data {
    /*
     * This entire struct is "private", in the C++ sense.
     * Use the methods that are declared below to access
     * the member data of this struct.
     */

/* internal compressor state */
    int first_compress_call;		/* comp_init sets, comp_compress clrs */
    int n_bits;				/* number of bits/code */
    code_int maxcode;			/* maximum code, given n_bits */
    code_int maxmaxcode;		/* should NEVER generate this code */
    code_int free_ent;			/* first unused entry */
    int clear_flg;			/* set when clearing hash table */
    int ratio;				/* track current compression ratio */
    __int64_t checkpoint;		/* where to check for block compress */
    __int64_t in_count;			/* length of input */
    __int64_t bytes_out;		/* length of compressed output */
    __int64_t out_count;		/* # of codes output (for debugging) */
    int hshift;				/* hash shift */
    code_int ent;			/* previous character from input */
    int comp_errno;			/* set if something fails */

/* application provided methods */
    comp_allocator_t fp_allocator;	/* application provided malloc() */
    comp_free_t fp_free;		/* application provided free() */
    comp_output_t fp_output;		/* application provided fp_output() */

/*
 * Accumulate compression codes to be output in codebuf[8] until we have
 * eight of them (which will always put us at a byte boundary).  Then
 * call _comp_output_codebuf() to pack them into outbuf[].
 */
    code_int codebuf[8];		/* assemble compressed codes here */
    code_int nextcode;			/* next empty slot in codebuf[8] */

/* dynamically allocated hash and code tables */
    count_int *htab;			/* dynamically allocated hash table */
    code_int hnumentries;		/* number htab[] entries allocated */
    code_int hnumentlast;		/* previous value of hnumentries */
    code_int htabsize;			/* total htab[] size allocated */
    unsigned short *codetab;		/* dynamically allocated code table */
    code_int codetabsize;		/* total codetab[] size allocated */

/* comp_compress() buffers output here until calling the fp_output() method */
    unsigned char *outbuf;		/* dynamically allocated output buf */
    long outbufcnt;			/* number available chars in buffer */
    long outbuflast;			/* previous value of outbufsize */
    unsigned char *outbufptr;		/* next char goes here in buffer */

/* application controlled option flags */
    COMP_OPTIONS options;		/* copy of comp_init() provided opts */

} COMP_DATA;


/*
 * The following methods are provided for use on compression streams.
 * Each method takes an explicit pointer to a COMP_DATA stream as its
 * first parameter, rather like the implied "this" pointer in C++.
 */

void comp_options_default(COMP_OPTIONS *);
/*
 * Initialize a COMP_OPTIONS structure to the standard default values.
 * Use this function before calling comp_init().
 */


void comp_init(COMP_DATA *, comp_allocator_t, comp_free_t, comp_output_t);
    /*
     * Initialize (or reinitialize) a COMP_DATA structure, including setting
     *	    the function pointers for the allocator, free and output methods
     *	    to the passed in values.
     */

int comp_begin(COMP_DATA *, COMP_OPTIONS *);
    /*
     * Call before each compression to re-initialize the COMP_DATA
     *      structure, resetting the COMP_OPTIONS.
     *	    returns 0 on success, -1 on failure.
     * Can fail if invocations of allocator fail during initialization.
     * The allocator will be called during comp_begin().
     * The free might be called during subsequent comp_destroy()
     *	    or comp_begin() invocations.
     * The output might be called during subsequent comp_compress() or
     *	    comp_end() invocations.
     */

int comp_compress (COMP_DATA *, const unsigned char *buf, long buflen);
    /*
     * Provide some more data at location buf, length buflen,
     * to be compressed by the specified compression stream.  Returns 0
     * if reaches end of provided input without error,  else -1.
     */

int comp_end (COMP_DATA *);
    /*
     * Notify compression stream that input data has ended.
     * Should cause stream to compress and output any remaining
     * data still in its buffers.  Return 0 on success, -1 on failure
     * and -2 if success (no errors) but no compression savings.
     * Can fail on internal error checking or if an invocation of
     * the output method returns -1.
     */

void comp_destroy (COMP_DATA *);
    /*
     * Destroy specified compression stream.  Primary consequence
     * is to invoke the previously provided free method to free
     * up any internally allocated buffer space.
     */

int comp_geterrno (COMP_DATA *);
    /*
     * Return value of last internal error number.
     * Error code -1 means that a call to an application provided
     * malloc, free or output method failed (as defined above).
     * Error code 0 means no error has been observed.
     */

void comp_clrerrno (COMP_DATA *);
    /*
     * Clear (set to 0) the last internal error number for this stream.
     * Calls to comp_compress() return with no work done if this error
     * number is currently not zero.
     */

void comp_dump_tab (COMP_DATA *);
    /*
     * Dump the dictionary. No-op unless comp.c compiled -DDEBUG.
     */

#ifdef __cplusplus
}
#endif
#endif /* !__COMP_H__ */
