#if IP32 /* whole file */
#ifndef _flash_h_
#define _flash_h_

/*
 * Flash rom segment architecture
 *
 *      +-------+   <---+
 *	| Header|	|
 *	+-------+	|
 *	| Body  |	|
 *	|       |	|  FLash Segment
 *	|       |	|
 *      +-------+	|
 *      | CHKSUM|	|
 *	+-------+   <---+
 *          .
 *          .
 *          .
 *
 * The total size of a segment in bytes must be a multiple of 8
 *
 */


/**********************
 * Flash segment header
 **********************
 *
 * Segment headers are located on flash page boundies.
 *
 * The sum of all long's in the segment header including the checksum field
 * must be zero.
 *
 * The total segment length must be a multiple of 8.
 *
 * The "version" portion of the header must be zero padded such that the total
 * length in bytes of the segment header is a multiple of 8.
 *
 * A valid header with a zero lengh body marks the end of the segments in a
 * flash ROM.
 */
#define SEG_MAX_NAME	32
#define SEG_MAX_VSN	8
#ifdef _LANGUAGE_C

typedef struct {
  long long reserved;			/* 00 Reserved */
  long      magic;			/* 08 Segment magic */
  long	    segLen;			/* 0c Segment length in bytes (includes hdr) */
  unsigned long nameLen : 8,		/* 10 Length of name in bytes */
                vsnLen  : 8,		/* 11 Length of vsn data in bytes */
                segType : 8,		/* 12 Segment type */
		pad	: 8;		/* 13 Pad */
  char      name[SEG_MAX_NAME];		/* 14 segment name */
  char      version[SEG_MAX_VSN];	/* 34 of version data */
  long      chksum;             	/* 3c Header checksum */
} FlashSegment;

#endif

#ifdef _LANGUAGE_ASSEMBLY

/* Offsets into FlashSegment */
#define FS_MAGIC   8
#define FS_SEGLEN  0xc
#define FS_NAMLEN  0x10
#define FS_VSNLEN  0x11
#define FS_SEGTYPE 0x12
#define FS_NAME    0x14
#define FS_VSN     0x34
#define FS_SUM     0x3c
#define FS_HDR_LEN 0x40

#endif


/*
 * Segment type bits
 *
 */
#define FS_CODE  	(1)	/* Executable segment */
#define FS_RAM          (2)	/* Copy to RAM for execution */
#define FS_PIC          (4)	/* Code is PIC */
#define FS_COMPRESS     (8)	/* Segment is compressed */

#define FS_Xcopy        (FS_CODE|FS_RAM)

/*
 * Flash blocks
 *
 * Segment bodies of Xcopy and Xcompress segments are composed of a sequence
 * blocks.  Each block begins with an block address and a block length.
 * A block with a length of zero terminates the list of blocks.
 * The addr of the zero length block is the execution address of the segment.
 *
 * For an Xcopy segment, the blocks are copied to RAM and then executed.
 * For an XcopyPIC segment, the entire segment may be copied to any region
 * so long as alignment modulo 64KB is preserved.
 *
 * For Xcompress segments, the data portion of the block are compressed and
 * must be decompressed during the copy.
 *
 * THE FORMAT OF COMPRESSED DATA IS UNSPECIFIED AT THIS TIME.
 */
#ifdef _LANGUAGE_C

typedef struct{
  unsigned long  addr;		/* 00 Block address */
  unsigned long  length;	/* 04 Block length */
#if 0
  long           data[1];	/* 08 */
#endif
} FlashBlock;

#endif

#ifdef _LANGUAGE_ASSEMBLY

/* Offsets into FlashBlock */
#define FSB_ADDR    0
#define FSB_LENGTH  4
#define FSB_DATA    8

#endif

/***********
 * flash rom	
 ***********
 * Depending on the implementation, the low level interface could get
 * complicated.  We assume the following:
 *
 *  1) Byte, halfword and word reads are handled just as with any other RAM.
 *  2) Writes are implementation specific and are handled by a "write" rtn.
 *  3) Page buffering, erasures, and so on are handled automatically by the
 *     write routine.
 *  4) The write model is similar to tape, you move to the appropriate
 *     page aligned location and then write sequentially.
 *  5) When finished writing, the "flush" rtn is called to flush any
 *     partially written page buffer.
 *
 * There seems little need for any write granularity other than word writes.
 * The current implementation is quite sensitive to the validity of (1).
 */
#ifdef _LANGUAGE_C

typedef struct{
  FlashSegment* base;		/* Start of flash */
  FlashSegment* limit;		/* Past end of flash */
  long          pageSize;	/* Flash page size in bytes */
  long		flashSize;	/* Flash size in bytes */
  void          (*write)(long*,long,long*);
  void		(*writeFlush)(void);
  void          (*writeEnable)(int);
} FlashROM;

#endif

/*
 * A "nasty" set of macros:
 *
 * __RUP(x,t) 	rounds x up to the  next long and casts to (t).
 * name(f)      char*   name string addr
 * version(f) 	char*	version string addr
 * chksum(f)	long*	segment header checksum addr
 * body(f)    	long*	segment body addr
 * segChksum(f)	long*	segment checksum addr
 * hdrSize(f)	long 	size of segment header in bytes (includes checksum)
 * bodySize	long	size of segment in bytes (includes checksums)
 * newSegChksum	long*	checksum addr for the newly create segment.
 */
#ifdef _LANGUAGE_C

#define __RUP(x,t)      ((t)(((unsigned long)(x)+3)&~3))
#define name(f)         (f->name)
#define version(f) 	(f->version)
#define chksum(f)  	(&f->chksum)
#define body(f)    	(chksum(f)+1)
#define segChksum(f)  	(__RUP(((char*)f)+f->segLen,long*)-1)
#define newSegChksum(f,n)   (__RUP(((char*)f)+n->segLen,long*)-1)
#define hdrSize(f)   	(long)(sizeof(FlashSegment))
#define bodySize(f)	(f->segLen - hdrSize(f))

#endif

#ifdef _LANGUAGE_C

FlashSegment* findFlashSegment(const FlashROM*, char*,FlashSegment*);
FlashSegment* findFlashSegmentHdr(const FlashROM*, char*, FlashSegment*);
FlashSegment* nextSegment(const FlashROM*, FlashSegment*);
int rewriteFlashSegment(const FlashROM*, FlashSegment*, long* buffer);
int writeFlashSegment(const FlashROM*,
		      FlashSegment*,
		      char* name,
		      char* vsn,
		      long* body,
		      int   bodyLen);
extern FlashROM envFlash;

#endif

/* Standard segment names */
#define SLOADER_FLASH_SEGMENT "sloader"
#define ENV_FLASH_SEGMENT     "env"
#define ENV_FLASH_VSN         "v1.0"
#define FW_SEGMENT            "firmware"
#define POST1_SEGMENT         "post1"
#define VER_SEGMENT           "version"

/* Flash header magic */
#define FLASH_SEGMENT_MAGIC 'SHDR'
#define FLASH_SEGMENT_MAGICx 0x53484452

#endif

#endif /* IP32 */
