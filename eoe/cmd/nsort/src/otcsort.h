/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.       *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 *
 * otcsort.h	- header for offset value code based sorting
 *
 *		supports record, pointer, and variable length  sorts
 *
 *	$Ordinal-Id: otcsort.h,v 1.53 1996/12/19 18:51:47 chris Exp $
 *
 *	$Revision: 1.1 $
 */

#ifndef _OTCSORT_H
#define _OTCSORT_H

#include <stdio.h>

#define VOLATILE	/* to nothing, could be "volatile" */

#include "ordinal.h"
#include "record.h"
#include "nsort.h"	/* the client api header file */
#include "parse.h"
#include <sys/stat.h>
#include <sys/resource.h>
#include <setjmp.h>
#include <fcntl.h>
#include "ordaio.h"

typedef u4 ovc_t;

#define MAXRECORDSIZE	(64*1024)

#define EOL_OVC		0xFFFFFFFF
#define NULL_OVC	0      /* this value is used to initialize the
			        * tournament tree in the 
			        * merge_{rec,ptr}_{line,skip,out}() routines.
			        * It is also used in a line-list to indicate
			        * extra.next points to next line in the list */

typedef struct recode_args {
    u2		win_id;		/* the record with the lower id */
    u2		curr_id;	/* appears earlier in the input stream */
    ovc_t	win_ovc;	/* for record sorts only: the current */
    ovc_t	curr_ovc;	/* ovcs for the records */
    u1		summarize;	/* do? or don't delete records in recode */
    u4		n_hard;		/* cumulative count of recodes which get data */
    u4		n_soft;		/* cumulative count of recodes which use eov */
} recode_args_t;

/* Define the width of merges used by gen_list() to build its output line-list.
 * Also define maximum number of merge levels for the gen_list() merges.
 * The maximum number of lines that can be output by gen_list() is 
 * GEN_WIDTH**MAX_GEN_LEVEL.
 */
#define GEN_WIDTH      36
#define MAX_GEN_LEVEL   4
#define MAX_GEN_N_LINES	9

#define N_IN_RUNS	2	/* number of input runs in memory */
#if 0
#define IN_RUN_PARTS	104	/* number of partitions per input run */
#else
#define IN_RUN_PARTS	(sort->in_run_parts)
#endif

#define TBUF_RUN_FREELIST	USHRT_MAX /* pseudo-run for tbufs on freelist */

#ifdef ARENACHECK
#undef ARENACHECK
#define ARENACHECK do { extern void free(void *);free(malloc(4)); } while (0)
#else
#define ARENACHECK
#endif

/* Hash function parameter inspired by Knuth chapter 6.4 - 2^32/golden mean */
#define KNUTH_GOLDEN    2654435769

union id
{
    /*
    struct node_line	*next;
    struct ptr_line	*pnext;
    */
    byte 		*rec;
};

/*
** merge tree node for a pointer sort.  The ovc is the first field for
** both pointer and record sorts.  The extra offset value is the second
** field in order to simplify recode()'s work a little.
** A node which is at the end of a (logical or physical) line has an ovc
** of EOL_OVC.  This means that the eov field contains the pointer to the
** next linelist, not an extra offset value.
**
** A 512 byte linelist can hold LL_ITEMS (42) real ptritems
*/

typedef union item_extra
{
    u4		data;		/* record sort: first 4 bytes of data */
    u4		eov;		/* extra offset value for fixlen ptr sorts */
    struct {			/* variable length pointer sorts: */
	u2	var_eov;	/* two bytes of extra offset value */
	u2	var_length;	/* two bytes of this record's length */
    } var_extra;
#if 0
    struct item_line	*next;	/* if item.ovc == NULL_OVC, and this is either
				 * the input phase, or the output phase during
				 * a one pass sort, next is the next line the
				 * line list */
    struct tbuf	*tbuf;		/* if item.ovc == NULL_OVC and this is the					 * output phase during a two pass sort, this
				 * is the last item for the tbuf, and 'tbuf'
				 * points to the tbuf for the item list which
				 * this item ends. */
#endif
} item_extra_t;

typedef struct item
{
    ovc_t	    ovc;	/* offset value code */
    item_extra_t    extra;	/* eov/var length/next linelist ptr */
    byte	    *rec;	/* ptr to record, or next 4 bytes of record */
} item_t;

/* An item list that ends with a NULL_OVC is followed by a pointer
 * to the next line or tbuf. NULL_ITEM_SIZE is the space needed for these
 * two items - the list termination 'flag' ovc value, and the 'next' pointer.
 */
typedef struct null_item
{
    ovc_t		ovc;
    struct item_line	*next_line;
} null_item_t;

#define	NULL_ITEM_SIZE	((int) sizeof(null_item_t))

typedef struct tbuf_item
{
    ovc_t	ovc;
    struct tbuf	*tbuf;
} tbuf_item_t;

#define LL_BYTES	512
#define	LL_ITEMSPACE	(LL_BYTES - NULL_ITEM_SIZE)
#define	LL_ITEMS	(LL_ITEMSPACE / (int) sizeof(item_t))	/* 42 or 31 */
#define LL_MASK		(LL_BYTES - 1)

/*
 * Hack permitting 32 bit ptr items to be 12 bytes
 * AND null items to be 8 bytes in 32-bit mode (giving 42 ptr items/line)
 * 16 bytes in 64-bit
 */
#define ITEM_NEXTLINE(item)	((null_item_t *) (item))->next_line
#define ITEM_TBUF(item)		((tbuf_item_t *) (item))->tbuf
#define LINE_NEXTLINE(ll, itemspace) ITEM_NEXTLINE(item_math((ll), (itemspace)))

typedef struct item_line
{
    item_t		item[LL_ITEMS];
    char		filler[LL_BYTES - LL_ITEMS * sizeof(item_t)];
} item_line_t;

typedef item_line_t node_line_t;

#define MAXRECORDSORT	108

#define item_math(recnode, amount)	((item_t *) ((byte *) (recnode) + (amount)))
#define next_item(recnode, amount)	item_math((recnode), (amount))

#define LINE_START(item)	((item_line_t *) ((ptrdiff_t) (item) & ~LL_MASK))

/*
 * Radix sorts
 *
 * radix line	- a linelist element containing radix items.  It differs from
 *		  the normal item_line in that the 'next' pointer is always at
 *		  a known location (at the front of the line) rather than being
 *		  after the last radix item in the line: there's no equivalent
 *		  to a NULL_OVC that indicates "we've reached the last item"
 *		  The next_item field of a bucket or list_desc points to the
 *		  next radix item to which to append, a.k.a. the space just
 *		  after the last useful record in the radix line.
 *		    The final rad_line of a radix line list has next == NULL.
 * bucket
 * line-list description
 */
typedef struct rad_line
{
    byte	rec[LL_BYTES - sizeof(struct rad_line *)];
} rad_line_t;

#define FIRST_RADIX_ITEM(radline)	((byte *) (radline))
#define ENDING_RADIX_ITEM(radline, spc)	((byte *) (radline) + (spc))
#define FULL_RADIX_LINE(raditem, spc)	(((unsigned) (raditem) & LL_MASK) == (spc))

#define RADIX_LINE_START(item)		((rad_line_t *) LINE_START(item))

typedef struct bucket
{
    rad_line_t	*head;		/* only radix-style lls, no EOL_OVC/next lls */
    rad_line_t	*tail;
    byte	*next_item;	/* next radix item space to fill, in *tail */
    unsigned	line_count;	/* #lines in this bucket, all but last full */
} bucket_t;

typedef struct list_desc
{
    item_line_t	*head;		/* only EOL_OVC/next lls, no 'next' rad_lines */
    item_line_t	*tail;
    byte	*next_item;	/* next ovc item space to fill, in *tail */
    item_line_t	*free;		/* both item and radix lines are on free list?*/
} list_desc_t;

#define MAX_GEN_N_RADIX_LINES	9

/*
 * The merge tree node (and not an ovc sorting item)
 */
typedef struct node
{
    u2	next;	/* distance in bytes backwards to parent node */
    u2	id;	/* input stream index of losing record at this node */
    u4	ovc;	/* ovc of losing record at this node */
} node_t;

/* Since 
 * 1) the next member in a node_t is 16 bits in size, and
 * 2) next is the distance in bytes to the parent node of a node, and
 * 3) the size of a node is 8 bytes,
 * then the largest number of nodes that can be skipped over is 8192-1
 * (in which case 'next' would be 0xFFF8).  
 *
 * Since the largest skip distance is from the bottom-most level of node
 * to the next higher level, and since the number of inputs to the merge
 * can be twice the number of nodes at the bottom level, the maximum width
 * of a merge is then 2 * (8192 - 1), or the following:
 */
#define MAX_MERGE_WIDTH	(16384 - 2)

typedef struct merge_level
{
    unsigned		n_lists;
    item_line_t		*list[GEN_WIDTH];
} merge_level_t;

#define MAX_SUB_SIZE	16

/*
** The output buffer structure describes the results of a merge_rec_out().
**	The input offset for a particular record N in the sorted output is:
**		input_offset = sum(each record[i] for i from 0 to N-1)
**	This is the same as the output offset if and only if there
**	are no duplicates deleted before record N
**
**	Find the particular output for an 'input offset' by
**		outbuf_index = input_offset % Out_buf_size;
**		sub_buf_index = input_offset % Out_sub_size;
**	Out_buf_size must be an integer multiple of Out_sub_size
**
**	Out_buf[J].file_offset = Out_buf[J - 1].file_offset + sum(subs.sub_size)
**	write_output can write a buffer when its file_offset is non-zero
**		(except that it is possible for the entire first buffer
**		 to be dups deleted - bummer)
**	
** write_output() can have its own input offset to determine when
**	an out_buf may be written?  
*/
#define	REGION_SIZE	(1 << 23)	/* should be at least 1 MB */
#define	STK_SZ		(1 << 16)

#define MAX_ASYNC	20

/* A histogram contains a population count for each range of 'index values'
 */
typedef struct hist
{
    unsigned	end;
    unsigned	population;
} histogram_t;

typedef struct part
{
    byte	*rec_start;
    /* only one of the following two fields is used at a time: (use union?)
    ** 	local_ll for items in record sorts
    **	local_edit for records in editing pointer sorts
    */
    byte	*local_edit;	/* start of local free record region */
				/* ends at current record in gen_line */
    item_line_t	*local_ll;	/* END of next 'local' item ll to use */

    byte	*rec_end;	/* one byte beyond the end of this */
				/* partition's last record */
				/* have read_input, etc. set this? */
    u4		n_recs;
    u2		max_length;	/* length of this part's largest rec */
				/* (excluding var_hdr_extra) */
				/* it is sort->record.length for fixed recs */
    u2		avg_length;	/* average length of this part's records */
				/* (excluding var_hdr_extra) */
    u2		sproc_id;	/* sorting process which took this partition */
    item_line_t	*refuge_start;	/* first ll of this part's refuge */
    item_line_t	*refuge_next;	/* next ll to be alloc'd from refuge */
    item_line_t	*refuge_end;	/* last ll of this part's refuge area */
    byte	*refuge_edit;	/* start of refuge free record region */
				/* above refuge_end, below lengths */
    u2		*lengths;	/* variable sort: lengths set by scan */
    item_line_t	*length_ll;	/* END of next 'lengths' ll to use */
    u4		hash_empty;	/* sum(#slots not used by a partition) */
    u4		hash_used;	/* sum(#slots used by a partition) */
    u4		hash_avg_length;/* average length of overflow chains */
    u4		hash_deletes;
    u4		hash_replaces;
    u4		hash_stopsearches;
    u4		sort_time;	/* #ticks this partition sort took (gen_list)*/
    u4		genline_time;	/* #ticks this partition took in gen_line */
    u4		hash_time;	/* #ticks this partition's hashing took */
} part_t;

typedef struct sprocstat
{
	struct rusage	in_usage;
	u4		in_elapsed;
	u4		in_idle;
	struct rusage	total_usage;
	u4		total_elapsed;
	u4		out_idle;
} sprocstat_t;

#define MAXSORTSPROC	32

typedef struct sortstat
{
    u4		n_parts;	/* number of input partitions sorted */
    i1		hash_details;	/* accumulate per-partition hash stats? */
    i1		wizard;		/* print internal, not well documented stats */
    u4		hash_deletes;	/* records deleted via hash table part. sort */
    u4		hash_replaces;	/* records replaced in hash table due to */
				/* summarization overflow */
    u4		hash_stopsearches;  /* #times a hash table chain wasn't fully */
				/* scanned, because the list was too long */

    u4		unused_item_lines;  /* #lines from read_consume to last item built */
    u4		serial_scan;	/* bottleneck times in partition sorting */
    u4		hash_partition;	/* hashing during partition sorts */
    u4		sort_partition;	/* partition sorting (wizard's sp subtracts above)*/
    u4		genline_time;	/* total time in gen_line (included in above)*/
    mpcount_t	backwards_calls;/* #of times backwards_sort(_rs) was called */
    mpcount_t	backwards_moves;/* total #of items moved during backward sorts*/
    mpcount_t	radix_deletes;	/* total #of records deleted by radix_count() */
    mpcount_t	merge_lines;	/* # of merge_{rec,ptr}_line() calls */
    mpcount_t	merge_outs;	/* # of merge_{rec,ptr}_out() calls */
    mpcount_t	merge_skipped;	/* # of reminder skipped records */
    mpcount_t	serial_in_merge;/* bottleneck times merge skipping input phase*/
    mpcount_t	parallel_in_merge;  /* mp-friendly merging in input phase */
    mpcount_t	input_copy;	/* copying records in input phase */
    mpcount_t	build_tbufs;	/* building tbufs during output phase */
    mpcount_t	serial_out_merge; /* bottleneck times merge skipping output phase */
    mpcount_t	parallel_out_merge; /* mp-friendly merging in output phase */
    mpcount_t	output_copy;	    /* copying records in output phase */
    sprocstat_t	*usages;
    histogram_t	*backwards_histogram;
    histogram_t	*radixcount_histogram;
} sortstat_t;

typedef enum sortmethod {
	SortMethUnspecified = 0,
	SortMethRecord,
	SortMethPointer
} sortmethod_t;

typedef enum sorthashspec {
	SortPreHashUnspecified = 0,
	SortPreHashRequired,
	SortPreHashForbidden
} sorthashspec_t;

typedef enum sortstyle {
	SortStyleUnspecified = 0,
	SortStyleMerge,
	SortStyleRadix
} sortstyle_t;

typedef enum flagstate {
	FlagUnspecified = -1,
	FlagFalse = 0,
	FlagTrue = 1
} flag_t;
/*
 * The initial ovc is assign via special interpreted instructions
 * which take advantage of the unique characterists of the initial ovc:
 *	There is no other record to compare 
 */
typedef enum {
	opCopy,
	opSigned,
	opFloat,
	opDouble,
	opDecimal,
	opVariable,
	opPad,
	opEof
} init_op;

typedef struct ovc_init
{
	u2	position;	/* key->position: bytes from the start of rec */
	u2	max_length;	/* key->length */
	u1	length;		/* number of bytes of ovc this fills: 1-(3|4) */
	u1	field_delim;	/* key->delimiter */
	u1	record_delim;	/* record.delimiter */
	u4	pad;		/* key->pad replicated in all four bytes */
	u4	xormask;	/* (0 or sign bit for signed ints) is xor'ed */
				/* with ~0 for descending sorts */
	init_op	op;
} ovc_init_t;

struct sortsproc;
typedef void	(*taskmain_t)(struct sortsproc *ssp, size_t stack_size);

/*
 * This is the argument passed to sproc_main.
 */
typedef struct sortsproc
{
    short	spid;		/* relative sproc # (io is 0, first sorter 1) */
    short	sproc_pid;	/* unix pid of the sproc */
    struct sort	*sort;		/* */
    taskmain_t	func;		/* is it an i/o, sorting, or free (null) sproc*/
} sortsproc_t;

typedef struct adv_info
{
    i8	 	off; 		/* temp file offset */
    u2		tfile;		/* temp file index */
    u2		valid;		/* valid flag */
    byte	rec[1];		/* record w/ key value that determines when this
				 * block will be needed in the final merge */
} adv_info_t;

typedef struct filestats
{
    u4		ios;		/* #ios performed for this file */
    u4		busy;		/* sum of times having at least 1 i/o active */
    mpcount_t	wait;		/* time at least one ssproc couldn't work
				 * while waiting for an i/o completion*/
    u4		bytes;		/* number of bytes of data transferred */
    u4		start_first;	/* ticks at start of first io */
    u4		finish_last;	/* tick at most recent io's completion*/
} filestats_t;

struct ministat {		/* only the few fields needed by nsort */
    off64_t	st_size;
    mode_t	st_mode;
    dev_t	st_dev;
};

typedef enum {
	IOMODE_UNSPECIFIED = 0,
	IOMODE_DIRECT,		/* do direct i/o, avoid most disk cache copies*/
	IOMODE_BUFFERED,	/* do non-direct i/o using the disk cache, it
				 * may be better than copy on write mmap */
	IOMODE_MAPPED,		/* do some kind?? of mmap */
	IOMODE_SERIAL		/* main sproc does non-direct r/w's serially */
} io_mode_t;

typedef struct fileinfo
{
    char		*name;		/* strdup'd copy of the file name */
    short		fd;		/* file descriptor, -1 if closed */
    short		openmode;	/* mode for re-opening it */
    short		aio_count;	/* # from {in,out,temp}file count=N */
    i1			copies_write;	/* buf'd ptr sort: copy tasks write */
    i1			sync;		/* force buffered output file to disk?*/
    io_mode_t		io_mode;	/* use direct or buffered i/o? */
    unsigned		io_size;	/* # from /{in,out,temp}file trans=N */
    unsigned		io_unit;	/* optimal i/o size (e.g. 32KB, track)*/
    struct ministat	stbuf;		/* subset of stat fields used by nsort*/
    unsigned		mem_align;	/* memory alignment required for i/o */
    unsigned		min_io;		/* minimum i/o size */
    unsigned		max_io;		/* maximum i/o size */
    filestats_t		stats;
} fileinfo_t;

typedef struct fsinfo
{
    dev_t	dev;		/* major & minor device number for the fs */
    unsigned	io_size;	/* if != 0, user has specified an i/o size */
    short	aio_count;	/* if != 0, user has specified #aio sprocs */
    i1		sync;		/* flush a buffered output file to disk */
    io_mode_t	io_mode;	/* default to direct or buffered i/o? */
} fsinfo_t;

typedef struct blockheader
{
    u4		first_offset;	/* from start of block to first record */
    u4		last_offset;	/* from start of block to start of last rec */
    u4		n_recs;		/* number of valid records in this block */
    u4		ovc;		/* ovc of first record in block, relative
				   to last record in previous block */
    union
    {
	item_extra_t       extra; /* eov/length for 1st rec (ptr sorts only) */
	struct blockheader *next; /* link for in-memory link-lists */
    }		u;
    u2		run;		/* consistency check: this block's run # */
    u2		block;		/* consistency check: position of block in run*/
    u2		adv_tfile;	/* temp file index for block TEMP.advance
				 * blocks ahead */
    i8	 	adv_off; 	/* offset of block TEMP.advance blocks ahead */
    /* the records are here, where first_offset indicates */
} blockheader_t;

#define	TEMPBLOCK_FIRST(th)	((byte *) (th) + (th)->first_offset)

typedef struct out_blk
{
    blockheader_t *head;
    u2		tfile;		/* temp file index for this block */
    i8		off;		/* offset of this block */
} out_blk_t;

/*
 * A tbuf describes the buffer holding a blockhdr and its item list.  The
 * buffer size is approx 64K-128K bytes.  The tbuf struct is found in front
 * of the buffer; the blockheader starts just afterwards and is aligned to
 * a dio boundry.  The items are found/built at the end of the tbuf; there
 * is some unused space between the end of the records at the first item.
 *	tbuf 	block header	records		space	items
 */
typedef struct tbuf
{
    struct tbuf	*r_next;	/* next tbuf in run.  NULL if next block
				 * not yet read and initialized. */
    struct tbuf	*f_next;	/* next tbuf in the relevant temp file's queue
				 * of tbuf's to read */
    item_t	*item;		/* list of items for this tbuf.  NULL if block
				 * has not been read or the item list built */
    i8		offset;		/* offset of block in the temp file */
    u2		run;		/* consistency check: this block's run # */
    u2		block;		/* consistency check: position of block in run*/
    u2		file;		/* the temp file number which holds this blk */
    i1		copy_done;	/* indicator that all copying form this tbuf has
				 * completed (only used for pointer sorts) */
    i1		read_done;	/* temp read statistic: set iff io finished */

    /* The following 3 members indicate when the tbuf has been totally 
     * consumed by the merge partitions.  A tbuf is no longer needed for
     * merging (and may safely be recycled) if all of the following are true:
     * 	1) (norm_consumer != MAX_U4) --> tbuf has been normally consumed and
     *		all merge partitions up to the consuming merge have completed
     *	2) (skip_consumer != MAX_U4) --> tbuf has been skip consumed
     *  3) (rem_consumers + 1 >= skip_consumer - norm_consumer) --> all merge 
     *		partitions between the norm_consumer and skip_consumer have 
     *		remainder consumed the tbuf
     */
    u4		norm_consumer;	/* merge partition that consumes this tbuf in
				 * producing its output.  if none yet, MAX_U4 */
    mpcount_t	rem_consumers;	/* count of merge partitions that consume this
				 * tbuf in their remainder skip */
    u4		skip_consumer;	/* merge partition that skip consumes this tbuf.
				 * if none yet, MAX_U4 */
} tbuf_t;

typedef struct tmp_run
{
    u2		tfile;		/* index of temp file with first block of run */
    i8		offset;		/* offset of first block */
    u4		n_blks;		/* number of blocks in the run */
    u4		eov;		/* extra offset value for computing optimal
				 * advance read ordering */
    u4		blks_read;	/* # of blocks scheduled for reading.  Also the
				 * current block for this run being considered
				 * for advance reading */
    tbuf_t	*old;		/* tbuf for oldest block in memory. tbuf->r_next
				 * points to the next newer (one higher block #)
				 * block */
    tbuf_t	*new;		/* tbuf for newest block in memory.  its r_next
				 * pointer is always NULL */
    struct tmp_run *next;	/* next run in the attention list */
    mpcount_t	attn;		/* flag indicating this tmp_run struct is on the
				 * attention list */
    byte	*adv_info;	/* the TEMP.advance advance info entries for
				 * this run */
} tmp_run_t;

typedef struct tempfile
{
    fileinfo_t	fileinfo;
    i1		rm_on_close;	/* set iff nsort created the file */
    unsigned	io_size;	/* #bytes to use for reads, writes */
				/* tis a multiple of fileinfo.io_unit */
    i8		write_offset;	/* write offset for this temp file */
    u4		io_issued;
    u4		io_done;
    filestats_t	write_stats;
    tbuf_t	*head;		/* head of queue of tbuf's to read */
    tbuf_t	*tail;		/* tail of queue of tbuf's to read */
    aiocb_t	*aiocb;
} tempfile_t;

typedef void	(*sortfunc_t)(struct sort *sort);
typedef void	(*copyitemfunc_t)(item_t *dest, CONST item_t *src, size_t size);
typedef void	(*copyfunc_t)(void *dest, const void *src, size_t size);
typedef unsigned (*recode_t)(struct sort *, recode_args_t *, item_t *win, item_t *curr);
typedef unsigned (*recode_rec_t)(struct sort *, recode_args_t *, byte *win, byte *curr);
typedef byte	*(*init_item_t)(struct sort *, unsigned n_recs, byte *rec, item_t *item, unsigned skip, part_t *part);
typedef byte	**(*init_item_int_t)(struct sort *, unsigned n_recs, byte **input_ptrs, item_t *item, unsigned skip, part_t *part);
typedef byte	*(*init_radix_t)(struct sort *, unsigned n_recs, byte *rec, bucket_t *existing, part_t *part);
typedef void	(*insert_sort_t)(struct sort *, recode_args_t *args, item_t *begin, item_t *end);
typedef void	(*edit_func_t)(byte *dest, byte *rec, int rec_len, edit_t *edits);
typedef unsigned (*calc_fixed_hash_t)(struct sort *sort, byte *rec);
typedef unsigned (*calc_hash_func_t)(struct sort *, const byte *rec, unsigned reclen);
typedef unsigned (*calc_hash_fixed_func_t)(struct sort *, const byte *rec);

typedef void	(*final_write_t)(struct sort *sort, byte *base, unsigned size);

/* forward declarations from merge.h */
struct merge_args;
struct rec_out;
struct rec_out_buf;

typedef item_line_t *(*merge_line_func_t)  (struct sort	*sort,
					    struct merge_args *args,
					    item_line_t **ll_free,
					    list_desc_t *result);

typedef int (*merge_skip_func_t)  (struct sort	*sort,
				   struct merge_args *args,
				   int		recs_to_skip,
				   int		skip_interval);


typedef unsigned (*merge_out_func_t)  (struct sort	*sort,
				       struct merge_args *args,
				       struct rec_out 	*rec_out,
				       int		recs_to_skip,
				       int		recs_to_consume);

typedef struct input
{
    i8			input_size;	/* sum of input file sizes (bytes) */
    unsigned		read_size;	/* size of the input reads */
    unsigned		aio_cnt;	/* number of aiocb's in aiocb */
    aiocb_t		*aiocb;		/* array of aio_cnt aiocb's */
    unsigned		reads_issued;	/* */
    unsigned		reads_done;	/* */
    i8			intra_run_offset; /* offset from beginning of run */
    i8			begin_run_offset; /* curr input file offset at 
					   * beginning of run */
    unsigned		begin_reads_issued; /* reads_issued at start of run */
    unsigned		dio_mem;	/* dio buf's mem alignment requirement*/
    unsigned		dio_io;		/* unit of io size for in files */
    i1			end_of_input;	/* flag indicating all input files 
					 * have been completely read */
    i1			eof_remainder;	/* flag indicating current in_run
					 * begins with remainder data from the
					 * previous in_run, AND that eof
					 * was reached on previous in_run. */
    i1			ptr_input;	/* api: client gives rec ptrs to us */
    i1			recs_aligned;	/* api: are rec sort records aligned? */
    i1			single_ok;	/* all input file access modes ok for
					 * single process mode? */
    i8			run_remainder;	/* how much of the current in_run
					 * begins with remainder data from the
					 * previous in_run (isn't u4 enough?) */
    i8			target_size;	/* target read size for next in_run */
    unsigned		runs_init;	/* in_runs initialized for reading
					 * by the io sproc */
    unsigned		runs_read;	/* in_runs completely read */
    unsigned		runs_sorted;	/* in_runs sorted into partitions */
    unsigned		runs_filled;	/* in_runs filled to boclk buffers */
    unsigned		runs_write_sched; /* in_runs whose blocks have been
					   * completely scheduled for writing */
    unsigned		runs_flushed;	/* in_runs temp blks flushed to disk */
    VOLATILE mpcount_t	part_avail;	/* number input partitions available */
    VOLATILE mpcount_t	part_taken;	/* next input partition as yet 
					 * unsorted */
    unsigned		part_scanned;	/* number input partitions scanned */
    unsigned		part_final;	/* final number of input partitions */
    unsigned		n_files;	/* number of input files */
    fileinfo_t		*files;		/* argv-like [] of input file info */
    fileinfo_t		*curr_file;	/* current input file.  if equal to
					 * files + n_files, no more input */
} input_t;

typedef struct merge
{
    /* for record sorts, the records from a merge partition are put in the
     * following buffer */
    struct rec_out_buf	*rob;		/* record output buffer */
    /* for pointer sorts, the pointers from a merge partition are put in the
     * following buffer */
    struct ptr_out_buf	*ptr_part;	/* pointer partition */
    unsigned		merge_cnt;	/* number of entries in rob
					 * or ptr_part array */
    volatile unsigned	skip_done;	/* number of merge skips done.  each
					 * merge partition does a skip at the
					 * beginning of its merge.  A merge
					 * cannot be taken until the previous
					 * merge has finished it skipping.
					 * i.e., skipping is serial */
    VOLATILE mpcount_t	merge_avail;	/* merge partitions available */
    VOLATILE mpcount_t	merge_taken;	/* merge partitions taken */
    volatile unsigned	merge_done;	/* number of merge partitions that have
					 * been completed the sort sprocs. */
    unsigned		merge_scanned;	/* number of merges completely scanned
					 * for copy scheduling (ptr sort) or
					 * for writing (rec sort) */
    volatile unsigned	tails_done;	/* number of merge partitions whose tail
					 * recs have been checked for potential
					 * duplicate elimiation by the
					 * subsequent merge */
    unsigned		tails_scanned;  /* number of merge partitions whose tail
					 * rec portion have been completely
					 * scanned for copy scheduling by the io
					 * sproc (pointer sort only) */
    unsigned		skip_interval;	/* number of items to skip during 
					 * skipping */
    unsigned		width;		/* the number of input streams in 
					 * the merge */
    unsigned		part_recs;	/* for pointer sorts: the number of
					 * records to be consumed by the first
					 * merge partition, all subsequent
					 * partitions will consume one record
					 * less than this */
    unsigned		part_ptr_cnt;	/* for pointer sorts: number of pointers
					 * in a merge partition's output array*/
    unsigned		rec_buf_size;	/* size of rec_bufs in bytes */
    byte		*rec_bufs;	/* pointer to the union of:
					 * 1) blocks for temporary file writes
					 *    during the output phase
					 * 2) buffers for output file writes  */

    item_line_t		**run_list;	/* array of current item pointers for
					 * each of the 'width' input streams.
					 * this array is used by merge
					 * partitions to begin their merge. */
    volatile unsigned	recs_consumed;	/* the number of records that have been
					 * consumed from the beginning of the
					 * merge until the current item
					 * pointers in run_list */
    mpcount_t		run_list_lock;	/* spinlock for updating run_list
					 * and recs_consumed */
    
    unsigned		end_of_merge : 1; /* indicator of end of merge */
} merge_t;

typedef struct copy
{
    byte		**curr_rec;	/* current record pointer in current
					 * merge partition, or NULL if we 
					 * are in between merge results */
    byte		**limit_rec;	/* record pointer limit for current
					 * merge partition */
    unsigned		skip_bytes;	/* number of bytes to skip in copy
					 * of *curr_rec */
    unsigned		target_size;	/* target and maximum copy size */
    unsigned		bufs_planned;	/* number of bufs completely 
					 * planned for copying */
    VOLATILE mpcount_t	copy_avail;	/* copy tasks that are/were avail */
    VOLATILE mpcount_t	copy_taken;	/* copy tasks that have been taken */
    unsigned		copy_done;	/* copy tasks i/o sproc has noticed 
					 * are done */
    unsigned		copy_cnt;	/* total number of tasks in copy[] */
    byte		*buf_position;  /* ending copy position in curr buf */
    off64_t		write_position;	/* copies_write? next copy writes here*/
    struct copy_task	*copy;		/* copy task array */
} copy_t;

typedef struct temp
{
    u4			blk_size;	/* each tempfile's read&write size */
    u4			n_files;	/* number of temporary files */
    i1			aio_cnt;	/* #aios for each temp file (e.g. 2) */
    i1			run_filled;	/* indicates the current run being
					 * filled has had all its blks filled */
    i1			run_planned;	/* indicates the run being filled
					 * has had all its blks copy planned */
    unsigned		blks_filled;	/* temp blocks filled for ALL runs */
    unsigned		writes_issued;	/* block writes issue for ALL runs */
    unsigned		writes_done;	/* block writes done for ALL runs */
    unsigned	        writes_final;	/* limit on # of direct writes, set
					 * when final # of writes is known */
    unsigned		begin_run_blks;	/* number of blks filled (by
                                         * previous runs) at the beginning
                                         * of the run being filled */
    unsigned		start_wait;	/* when the `waiting's wait started */
    mpcount_t		n_idling;	/* #sprocs waiting for temp io */
    u2			next_tfile;	/* next temp file to place a temp blk */
    u2			advance;	/* # of blocks ahead to keep advance
					 * blk location and, for optimal reads,
					 * "need" record */

    unsigned		tbuf_cnt;	/* number of tbuf array elements */
    unsigned		tbuf_size;	/* # of bytes for each temp buf.
					 * larger than blk_size to allow the
					 * item list to be built */
    unsigned		out_blk_cnt;	/* count of out_blk elements */
    tempfile_t		*files;		/* temporary file array */    
    tempfile_t		*waiting;	/* tempfile causing the current wait */
    tbuf_t		*free;		/* list of free tbufs */
    tbuf_t		*tbuf;		/* pointer to the array of all tbufs */

    out_blk_t		*out_blk;	/* array of out_blk structs, used for
					 * keeping track of temp file output
					 * blocks during the input phase */

    tbuf_t		**build;	/* array of tbuf_t pointers used for
					 * for queuing tbufs for item list
					 * building */
    unsigned		build_cnt;	/* count of ptrs in build array */
    VOLATILE mpcount_t	build_avail;	/* count of tbufs that are or have been
					 * available for list building */
    VOLATILE mpcount_t	build_taken;	/* count of tbufs that have been taken
					 * by sort sprocs for list building */
    unsigned		build_done;	/* count of tbufs that the io sproc has
					 * recognized as having had their item
					 * lists built */
    unsigned		dio_mem;	/* max dio mem alignment for tfiles */
    unsigned		dio_io;		/* max minimum io size for tfiles */
    tmp_run_t		*run;		/* info for runs in tmp files */
    tmp_run_t		*attn;		/* list of runs need io sproc attention
					 * because they contain a done buffer or
					 * for initialization.  After output
					 * phase initialization, sort sprocs add
					 * runs to the list, the io sproc
					 * removes them. */
    i1			optimal_reads;	/* flag indicating that optimally
					 * ordered block reads are being done,
					 * and that an extra record needs to be
					 * placed on each temp file block to
					 * predict the optimal read order */
    u4			adv_info_size;  /* size of an adv_info structure */
    node_t		*adv_tree;	/* tournament tree for calculating
					 * next advance block to read */
    u4			tree_size;	/* size of each merge partition tree */
    u4			run_limit; 	/* limit of the number of runs flushed
					 * to temporary files */

    /* the following members are used for record sorts only */
    int			blk_recs;	  /* # fixlen recs in most temp blks */
    short		merge_part_blks;  /* # blks per merge partition */
    unsigned		part_block_sched; /* blocks in current merge partition
					   * being scheduled for writing that
					   * have been scheduled for writing */
    blockheader_t	*free_blk;	  /* free-list of blocks for writing
					   * temp file output during the input
					   * phase */
    unsigned		free_blk_cnt;	  /* number of blocks on free list */
    blockheader_t	*next_part_blk;	  /* next block in merge partition being
					   * scanned for completed blocks */
    u4			out_buf_size;	  /* number of record bytes output by
					   * each merge partition in the output
					   * phase of a two pass sort */
    
    /* the following members are used only for variable-length record sorts */
    u4			items_left;	/* items left in the current block 
					 * being filled with variable-length
					 * records. */
    u4			merge_ptr_cnt;	/* size of the merge partition pointer
					 * arrays in elements for the output
					 * phase of a two pass sort */
} temp_t;

typedef struct output
{
    fileinfo_t		*file;		/* output file info */
    byte		**buf;
    unsigned		buf_cnt;
    unsigned		buf_size;	/* size of the output buffers */
    unsigned		dio_mem;	/* dio mem alignment for out file */
    unsigned		dio_io;		/* minimum io size for out file */
    unsigned		rec_buf_pad;	/* pad at begin AND end of rec_out_buf
					 * (the result of a merge partition
					 * for a record sort) */
    unsigned short	aio_cnt;	/* number of aiocb's in aiocb */
    i1			waiting;	/* ssprocs could be stalled on writes */
    i1			ptr_output;	/* api: we return rec ptrs to client */
    unsigned		end_sort_output : 1;	/* sort sproc exit signal */
    unsigned		end_io_output : 1;	/* i/o sproc exit signal */
    aiocb_t		*aiocb;		/* array of aio_cnt aiocb's */
    byte		*nondirect_begin; /* beginning byte of said write */
    unsigned		nondirect_size;	/* size of a final, nondirect write */
    i8			write_offset;	/* curr write offset in output file */
    unsigned		writes_issued;	/* including zero-length writes */
    unsigned		writes_done;	/* including zero-length writes */
    unsigned	        writes_final;	/* limit on # of direct writes, set
					 * when final # of writes is known */

    /* the following members are used for record sorts only */
    unsigned		rob_writes_issued;/* writes issued for current merge
					   * partition being scheduled for
					   * writing */
    unsigned		rob_writes_done;/* writes done for current merge
					 * partition completing writes */
    unsigned		write_size;	/* preferred write size */  
} output_t;

typedef struct times
{
    unsigned	begin;
    unsigned	copy_first;
    unsigned	mmap_done;
    unsigned	sproc_alloc;
    mpcount_t	touch_cpu;	/* cpu time spent by sprocs in touching mem */
    unsigned	touch_done;
    unsigned	read_first;
    unsigned	read_done;
    unsigned	list_first;
    unsigned	list_done;
    unsigned	write_first;
    unsigned	merge_done;
    unsigned	write_done;
    unsigned	done;
    mpcount_t	hard_recodes;	/* #'hard' recode() calls (get data) */
    mpcount_t	soft_recodes;	/* #simple ptr sort (eov) recodes */
    unsigned	temp_reads;
    unsigned	chk_output_merge_naps;
    unsigned	chk_attns;
    unsigned	io_output_naps;
    unsigned	io_output_busys;
} times_t;

typedef struct in_run
{
    byte		*read_buf;	/* read buffer for this run. */
    byte		*read_start;	/* where reading will start.  this is
					 * normally REM_BUF_SIZE bytes after
					 * read_buf, but it may be beyond there
					 * if there is an extraordinarly large
					 * amount of data to copy from the 
					 * previous run. */
    byte		*first_rec;	/* first record.  if prev run ended
					 * with a partial record read, then
					 * this pointer will be less than
					 * read_start by the length of the
					 * partial record.  the partial record
					 * will be copied to this ptr, thereby
					 * making the record whole. */
    byte		*read_end;	/* current ending place of completed 
					 * reads */
    u4 /* yes, u4! */ 	eof;		/* indicates to sort sprocs that an eof
	  				 * has been reached on an input file
					 * for this in_run and that we can't
					 * read past read_end.  this variable
					 * is a u4 so that it doesn't share the
					 * same word as the scan_complete 
					 * variable which can simultaneously be
					 * set by a different sproc. */
    u4 /* yes, u4! */	final_run;     	/* indicator from sort sproc to io sproc
					 * that the record limit was reached in
					 * scanning this run.  hence the io
					 * sproc should not read further. */
    byte		*read_target;	/* target ending point for the io sproc
					 * to read.  must be an IN.dio_io
					 * multiple from read_start in order to
					 * meet direct i/o size requirements */
    byte		*rec_used;	/* end point of data used for sorting.
					 * if less than read_consume, there is
					 * remainder data that must be copied
					 * to start of the next in_run. */
    u4 /* yes, u4! */	scan_complete;  /* flag set by sorting sprocs indicating
					 * to the io sproc that scanning for
					 * this run is complete.  This means:
					 * 1) the sorting sprocs will NOT 
					 *    increase run_target for this run,
					 *    so the io sproc can start reading
					 *    data for the next run if that is
					 *    possible.
					 * 2) rec_used has been set to its final
					 *    value.  The io sproc uses this to
					 *    determine how much data at the end
					 *    of this run's data needs to be
					 *    copied to the next run.  
					 * this variable is a u4 so that it
					 * doesn't share the same word as the
					 * eof variable which can simultaneously
					 * be set by a different sproc. */
    short		file_num;	/* in.files[] index, for error msgs */
    short		unused;		/* XXX put n_parts here? */
    unsigned		n_parts;	/* actual number of partitions in the
					 * the in_run. */
    item_line_t		*refuge_start;	/*bottom of refuge 'stack', last part*/
    item_line_t		*refuge_next;	/* next partition allocs refuge here */
    item_line_t		*refuge_end;	/*end of the refuge 'stack', 1st part*/
    item_line_t		*ll_free;	/*GEN_WIDTH linelists for each sproc */
    part_t		*partitions;	/* per-partition information */
    item_line_t		**part_lists;	/* line-list output for each partition*/
} in_run_t;
	
/* Regions of /dev/zero mmap()'d memory.  At most one per simultaneously
 * active sort?
 */
typedef struct
{
    byte	*base;	/* starting address of the region */
    unsigned	size;	/* in bytes */
    unsigned	busy;	/* a sort is actively using this right now */
} region_t;

typedef struct sys
{
    unsigned	page_size;	/* e.g. indy: 4K, homegrown:16K */
    u2		aio_max;	/* simultaneous aios: sysconf(AIO_MAX)*/
    u2		zero_fd;	/* for /dev/zero */
    u1		n_processors;	/* # cpus available/directed to use */
    u1		n_filesys;	/* # of filesystems of which we know a little */
    u1		n_sorts;	/* # of sorts in (malloc'd) sorts[] */
    u1		n_regions;	/* # of memory regions currently mapped in */
    u1		n_sprocs_cached;/* # of valid pids in the sprocs[] cache */
    unsigned	region_kbytes;	/* total size of mapped regions, in kbytes */
    struct sort	**sorts;	/* sorts currently defined */
    sortsproc_t	*sprocs;	/* info on prev. used i/o & sorting sprocs 
				 * the first never exits; else aio sprocs exit*/
    usptr_t	*ua;		/* user arena for compare-and-swap */
    fsinfo_t	*filesys;	/* user or correctly determinable filesys info*/
    region_t	*regions;	/* remembers /dev/zero mmap'd memory locations
				 * sorted by base address */
    u8		buffer_cache_sz;/* #bytes in the buffer cache */
    u8		free_memory;	/* #bytes of totally free physical RAM */
    u8		free_swap;	/* #bytes of swap spaces avail to this proc */
    u8		user_data;	/* #bytes of physical RAM avail to this proc */

    /* the following structure members are used only for the sort program */
    byte	*touch_avail;	/* next mem region available for touching */
    volatile byte *end_touched;	/* ptr to end of mem allocated so far */
    mpcount_t	*done_array;	/* array of done (allocated) memory segments */
    unsigned	touch_sum;	/* write-only bogus member */
    ptrdiff_t	max_region_size; /* maximum region size */
} sys_t;

typedef struct parse
{
    short		charno;		/* character position in line/arg */
    short		lineno;		/* line number or argument number */
    short		specfile_depth;	/* depth of -spec=file nesting */
    const char		*line;		/* current line from which token came */
    const char		*title;		/* -spec file name or null from argv */
} parse_t;

#define SPECFILE_DEPTH_LIMIT	20

/* An api_invoked sort (especially an `external' one) could be mightily
 * confused by the client calling some function is an unsupported sequence.
 * This type helps detect such errors
 */
typedef enum { API_NEEDSINIT,
	       API_INPUT_RECS, API_INPUT_PTRS, API_END_INPUT,
	       API_OUTPUT_RECS, API_OUTPUT_PTRS, API_OUTPUT_DONE, API_ERROR
} api_phase_t;

typedef struct sort
{
	keydesc_t	*keys;
	u2		keycount;
	i1		go_sprocs;
	i1		check;		/* prog is check, rather than nsort */
	i1		test;		/* exceptional testing: e.g. check recode */
	i1		old_recode;
	i1		mergeonly;	/* to do: merge already-sorted files */
	i1		delete_dups;	/* keys appear at most once in output */
	i1		any_deletes;	/* delete_dups || n_summarized != 0 */
	i1		delete_in_disperse; /* should radix_disperse try delete? */
	i1		silent;
	i1		check_ll;	/* are items in linelists ordered? */
	i1		print_definition;
	i1		read_done;	/* read_input reached (final) EOF */
	i1		ovcsize;	/* 3 for record sorts, 2 for pointers */
	i1		sub_items;	/* for the typical gen_line() */
	i1		rem_items;
	i1		n_sub_lists;
	i1		two_pass;	/* flag indicating input runs will be
					 * flushed to temporary files during
					 * the input phase, then read back 
					 * during the output phase */
	i1		input_phase;	/* flag indicating we are in the first
					 * half (input phase) of a sort, we 
					 * may or may not be writing in_runs
					 * to temp files (see two_pass) */
	i1		output_phase;	/* flag indicating we are in the second
					 * half (output phase) of a sort, 
					 * we may or may not be reading runs
					 * from temp files (see two_pass) */
	u1		n_sprocs;	/* #sorting (non-io) sprocs to create */
	i1		single_process;	/* single-process mode? */
	u1		nth;		/* index of this sort in SYS.sorts[] */
	i1		simple_genline;	/* just insert-sort all the items */
	i1		compute_hash;	/* dyn. hash value; computed w/o edit */
	i1		radix;		/* do a radix sort, modifies method */
	i1		api_invoked;	/* called directly in a client program*/
	i1		internal;	/* api: internal (or external) */
	i1		cancelled;	/* api: explicitly canceled */
	i1		auto_free;	/* api: free all resources when done */
	i1		in_create;	/* api: totally free the sort on err */
	u2		n_ll_free;	/* #lines in in_run[].ll_free */
	u2		in_run_parts;	/* IN_RUN_PARTS, set by -P<N> */
	u2		io_sproc;	/* unix pid of i/o sproc; 0 if none */
	short		n_errors;	/* # of fatal parse & runtime errors */
	short		last_error;
	short		client_pid;	/* unix pid of client program */
	flag_t		touch_memory;	/* touch memory in parallel? */
	api_phase_t	api_phase;	/* detects out-of-sequence api calls */
	short		*sprocs;	/* indices into SYS.sprocs[] of the 
					 * sprocs which i/o, sort, merge, copy*/
	jmp_buf		api_fail;	/* api funcs 'setjmp(fail)' on entry */
	unsigned	virgin_ovc;	/* for source files which want this */
	unsigned	del_dup_ovc;	/* sort's version of these values */
					/* without needing separate {rec,ptr} */
	sortmethod_t	method;		/* SortMeth{Pointer,Record,Unspecified} */
	sorthashspec_t	prehashing;	/* SortPreHash{Enabled,Disabled,Unspecified} */
	u8		memory;		/* #kbytes of shared mem to use */
	unsigned	edited_size;	/* fixlen only: size after editing */
	short		n_edits;	/* # of editing statements allocated */
	short		edit_growth;	/* #bytes added by editing, may be <0 */
	short		n_derived;
	short		n_summarized;
	short		item_size;	/* sizeof(item_t)|RndUp(rec_size+4,4) */
	short		ll_items;	/* #items in a linelist (42 for ptrs) */
	short		ll_item_space;	/* ll_items * item_size */
	short		hash_table_bits;/* log2(hash_table_size), max 16 bits */
	unsigned	part_data_size;	/* preliminary size of each sort part */
	unsigned	hash_table_size;/* #slots in a hash table, power of 2 */
	edit_t		*edits;		/* edits to perform if any */
	short		*summarize;	/* the list of summarizing fields */
	ovc_init_t	*ovc_init;	/* instructs which build initial ovc */
	i8		n_recs;		/* sort no more than this many recs */
	i8		recs_left;	/* n_recs was set; this many remain to be partition sorted */
	u4		n_recs_per_part;/* each partition handles <= this# */

	in_run_t	in_run[N_IN_RUNS];
	unsigned	in_run_data;	/* size of data in each in_run */

	i8		calc_mem_use;	/* calculated memory usage */
	byte		*begin_in_run;	/* ptr to beginning of in_run data 
					 * and all mmap()ed memory */
	byte		*end_in_run;	/* end of buffers for last in_run */
	region_t	*region;
	item_t		**hash_tables;	/* hash tables for each sproc */
	byte		*end_out_buf;	/* ptr to end output buffers and
					 * and all mmap()ed memory */
	u4		n_refuge_lls;	/* total refuge linelist count */
	byte		*pinned;
	byte		*next_pin;
	short		max_n_parts;	/* per-run input parts possible */
	sortstat_t	*statistics;
	sortfunc_t	preparer;
	sortfunc_t	reader;
	sortfunc_t	writer;
	taskmain_t	worker_main;
	copyitemfunc_t	itemcopier;	/* copies this sort's ovc items */
	copyfunc_t	copier;		/* copy (edited) data in record sort */

	recode_t	recode;
	init_item_t	init_item;
	insert_sort_t	insert_sort;
	edit_func_t	edit_func;
	merge_line_func_t merge_line;
	merge_skip_func_t merge_skip;
	merge_out_func_t merge_out;
	calc_hash_func_t calc_hash;
	int		(*read_input)(aiocb_t *);	/* start read of inp */
	int		(*write_output)(aiocb_t *);	/* start write of out */
	int		(*get_error)(const aiocb_t *);	/* !EINPROGRESS?->fini*/
	ssize_t		(*get_return)(aiocb_t *);	/* e.g. bytes written */
	final_write_t	final_write;
	input_t		in;
	record_t	record;
	merge_t		merge;
	copy_t		copy;
	output_t	out;
	temp_t		temp;
	times_t		times;
	parse_t		parse;
} sort_t;


/* define macros for quickly accessing sort structure sub-structures
 */
#define IN		(sort->in)
#define	MERGE		(sort->merge)
#define COPY		(sort->copy)
#define OUT		(sort->out)
#define TEMP		(sort->temp)
#define	RECORD		(sort->record)
#define TIMES		(sort->times)
#define PARSE		(sort->parse)
#define STATS		(*sort->statistics)

extern	sys_t		NsortSys;	/* per-client globals */

#define SYS		NsortSys

/* define the number of bytes after in_run.read_buf reserved for remainder
 * data from the previous run.  this must be a power of 2, and is the
 * maximum of the largest record size minus one (the largest possible
 * partial record) and the minimum input file size.
 */
#define	REM_BUF_SIZE	(max(65536, IN.dio_io))

/*
** Recode - two slightly different interfaces
**
*/
unsigned recode_pointer(sort_t *, recode_args_t *, item_t *win_node, item_t *curr_node);
unsigned recode_variable(sort_t *, recode_args_t *, item_t *win_node, item_t *curr_node);
unsigned recode_record(sort_t *, recode_args_t *, byte *win_rec, byte *curr_rec);
extern byte	*fill_fpt_entry(sort_t *sort, fpt_t *fpt, unsigned position);
int	summarize_pointer(sort_t *sort, item_t *win_item, CONST item_t *curr_item);
int	summarize_record(sort_t *sort, byte *win_rec, CONST byte *curr_rec);

	/* prepare.c */
void		prepare_fields(sort_t *sort);
void		prepare_keys(sort_t *sort);
void		prepare_edits(sort_t *sort);
int		process_specfile(sort_t *sort, const char *specfile);
int		prepare_sort_def(sort_t *sort);
unsigned 	prepare_part(sort_t *sort, in_run_t *run, part_t *part);
unsigned	prepare_part_ptr(sort_t *sort, in_run_t *run, part_t *part);
void		prepare_in_run(sort_t *sort, unsigned run_index);
void		map_output_pass(sort_t *sort);

	/* fileinfo.c */
void		get_dio_info(fileinfo_t *file);
void		fix_sort_files(sort_t *sort);
const char	*another_infile(sort_t *sort, const char *p);
const char	*assign_outfile(sort_t *sort, const char *p);
nsort_msg_t	add_temp_file(sort_t *sort, const char *tempname);
const char	*another_tempfile(sort_t *sort, const char *p);
const char	*another_filesys(sort_t *sort, const char *p);
int		get_filesys_aiocount(sort_t *sort, fileinfo_t *file);
int		get_filesys_iosize(sort_t *sort, fileinfo_t *file);
int		get_filesys_iomode(sort_t *sort, fileinfo_t *file);
void		set_file_flags(fileinfo_t *file, int newflags);


unsigned	check_ll(sort_t *sort, CONST item_t *start);
void		check_pair(sort_t *sort, CONST item_t *lesser, CONST item_t *greater);

byte *init_item(sort_t *sort, unsigned n_recs, byte *rec, item_t *item, unsigned skip, part_t *part);
byte *init_item_vs(sort_t *sort, unsigned n_recs, byte *rec, item_t *item, unsigned skip, part_t *part);
byte *init_item_rs(sort_t *sort, unsigned n_recs, byte *rec, item_t *item, unsigned skip, part_t *part);

byte *init_radix(sort_t *sort, unsigned n_recs, byte *rec, bucket_t *freelist, part_t *part);
byte *init_radix_vs(sort_t *sort, unsigned n_recs, byte *rec, bucket_t *freelist, part_t *part);
byte *init_radix_rs(sort_t *sort, unsigned n_recs, byte *rec, bucket_t *freelist, part_t *part);

byte *build_hash(sort_t *sort, byte *rec, item_t *item, part_t *part,
		 item_t **hashtab, item_t **tail);
byte *build_hash_rs(sort_t *sort, byte *rec, item_t *item, part_t *part,
		    item_t **hashtab, item_t **tail);
byte *build_hash_vs(sort_t *sort, byte *rec, item_t *item, part_t *part,
		    item_t **hashtab, item_t **tail);

byte **init_item_int(sort_t *sort, unsigned n_recs, byte **input_ptrs, item_t *item, unsigned skip, part_t *part);
byte **init_item_vs_int(sort_t *sort, unsigned n_recs, byte **input_ptrs, item_t *item, unsigned skip, part_t *part);

byte *init_hash_items(sort_t *sort, unsigned n_recs, item_t *item,
		      item_t *scratch, unsigned skip, part_t *part);
byte *init_hash_items_rs(sort_t *sort, unsigned n_recs, item_t *item,
			 item_t *scratch, unsigned skip, part_t *part);
byte *copy_hash_items(sort_t *sort, unsigned n_recs, item_t *item, item_t *scratch, unsigned skip, part_t *part);


void insert_sort(sort_t *, recode_args_t *, item_t *begin, item_t *end);
void insert_sort_rs(sort_t *, recode_args_t *, item_t *begin, item_t *end);
item_t *backwards_sort(sort_t *, recode_args_t *, item_t *begin, item_t *end);
item_t *backwards_sort_rs(sort_t *, recode_args_t *, item_t *begin,item_t *end);

	/* genlist.c */
item_line_t *gen_list(sort_t *, part_t *, item_line_t **ll_sproc);
item_line_t *gen_radix_list(sort_t *, part_t *, item_line_t **ll_sproc);
item_line_t *last_resort(sort_t *, bucket_t *in, list_desc_t *out);

	/* genline.c */
void check_free(sort_t *sort, item_line_t *ll_free);
byte *gen_line(sort_t *, unsigned n_recs, byte *in_rec,
			 item_line_t *out_list,
			 part_t *part,
			 struct merge_args *);
byte *gen_n_lines(sort_t *, unsigned n_recs, byte *in_rec,
			    item_line_t *out_list,
			    part_t *part,
			    struct merge_args *,
			    int n_lines);
rad_line_t *sort_n_lines(sort_t *, unsigned n_recs,
				   rad_line_t *in_list,
				   item_line_t **ll_free,
				   struct merge_args *,
				   item_line_t **headptr,
				   list_desc_t *existing,
				   item_t *prev_tail);

	/* parsing and scanning a sort definition */
toktype_t	get_token(sort_t *sort, const char **charp, token_t *token, const toktype_t *expecting);
const char	*unget_token(sort_t *sort, const token_t *token);
void		matching_paren(sort_t *sort, const char **strp, token_t *token);
int		expect_token(struct sort *sort, const char **strp, token_t *,
			     toktype_t expect, nsort_msg_t err);

int		skipcomma(sort_t *sort, const char **p, int prior_errs);
const keyword_t	*keyword_lookup(sort_t *sort, token_t *token, const char *text,
				const toktype_t *expecting);
void		parser_error(sort_t *sort, token_t *token, nsort_msg_t err, ...);
nsort_msg_t	runtime_error(sort_t *sort, nsort_msg_t err, ...);
void		print_token_context(sort_t *sort, token_t *token);

nsort_msg_t	parse_statement(sort_t *sort, const char *source, const char *title);
void		parse_string(sort_t *sort, const char *source, const char *title);
char		*nsort_statement(const char *str);

unsigned	compute_checksum(byte *data, int length);
int		compare_data(sort_t *sort, CONST byte *rec1, CONST byte *rec2);
int		compare_string(keydesc_t *key, CONST byte *col1, CONST byte *col2, int len1, int len2);
int		compare_delim_string(keydesc_t *key, CONST byte *rec1, CONST byte *rec2, int len1, int len2);
int		get_delim_length(const byte *field, const keydesc_t *key, int till_eorec);

	/* parse.c */
field_t	*get_field_desc(record_t *rec, const char *field_name);
void	nsort_parse(sort_t *sort, char *text);
void	add_edit(sort_t *sort, field_t *field);
void	parse_file_options(sort_t *sort, fileinfo_t *file, const char **p,
			   int errs, const toktype_t *expecting);
void nsort_options(sort_t *sort, int argc, char **argv);

	/* print.c */
void	nsort_print_sort(sort_t *sort, FILE *fp);
void	nsort_print_record(record_t *rec, FILE *fp);
void	nsort_print_fields(record_t *rec, FILE *fp);
void	nsort_print_keys(sort_t *sort, FILE *fp);
void	nsort_print_data(sort_t *sort, CONST byte *data, FILE *fp, unsigned print_header, unsigned filler_too);
void	nsort_print_data_header(sort_t *sort, unsigned print_filler, FILE *fp);
void	nsort_print_keydata_header(sort_t *sort, FILE *fp);
void	nsort_print_keydata(sort_t *sort, CONST byte *data, FILE *fp, unsigned print_header);

	/* build.c */
void    build_tbuf_list(sort_t *sort, tbuf_t *tbuf);


	/* process.c */
int	tbuf_recyclable(sort_t *sort, tbuf_t *tbuf);
void	cause_run_attn(sort_t *sort, unsigned run_index);
void	read_defaults(sort_t *sort);

extern unsigned Print_task;

	/* io.c */
void    io_input_init(sort_t *sort);
void	io_input(sort_t *sort);
void    io_output_init(sort_t *sort);
void    io_output_end(sort_t *sort);
int	chk_io_output(sort_t *sort);
void	io_output(sort_t *sort);
void	gather_run_stats(sort_t *sort, in_run_t *run);
void	free_files(sort_t *sort);
int	chk_input(sort_t *sort);

	/* io_init.c */
void	init_aio(sort_t *sort);
int	obtain_aio_fds(sort_t *sort, aiocb_t aiocbs[], 
		       int count, fileinfo_t *file, nsort_msg_t err);
void	map_io_buf(sort_t *sort);
void	touch_mem(sort_t *sort, unsigned sproc_id);
void	prepare_shared(sort_t *sort); 
sort_t	*sort_alloc(void);
void	reset_sort(sort_t *sort);
void	free_resources(sort_t *sort);
void	reset_definition(sort_t *sort);
void	sort_free(sort_t *sort);
void	verify_temp_file(sort_t *sort);
void	close_aio_fds(sort_t		*sort,
		      aiocb_t		aiocbs[],
		      int		count,
		      char		*name,
		      nsort_msg_t	err);
aiocb_t	*aiocb_alloc(int num_aios);
void	nsort_cleanup(void);
void	unpin_range(byte *start, byte *end);
void	rm_temps(sort_t *sort);

	/* copy.c */
void	copy_4(void *dest, const void *src, size_t size);
void	copy_8(void *dest, const void *src, size_t size);
void	copy_12(void *dest, const void *src, size_t size);
void	copy_16(void *dest, const void *src, size_t size);
void	copy_20(void *dest, const void *src, size_t size);
void	copy_24(void *dest, const void *src, size_t size);
void	copy_28(void *dest, const void *src, size_t size);
void	copy_32(void *dest, const void *src, size_t size);
void	copy_36(void *dest, const void *src, size_t size);

	/* process.c */
void	nsort_init(sort_t *sort);
void	do_sort(sort_t *sort); 
void	null_function(sort_t *sort);
void	sort_input(sort_t *sort, unsigned sproc_id);
void	sort_output(sort_t *sort, unsigned sproc_id);
void	chk_output_merge(sort_t *sort);
void	sortproc_main(sortsproc_t *ssp, size_t stack_size);

	/* build.c */
item_t *build_tbuf_items(sort_t *sort, tbuf_t *tbuf);
item_t *build_tbuf_items_vs(sort_t *sort, tbuf_t *tbuf);
item_t *build_tbuf_items_rs(sort_t *sort, tbuf_t *tbuf);

	/* funcs.c */
void fill_sort_funcs(sort_t *sort);

	/* nsortapi.c non-public functions */
void	external_io(sortsproc_t *ssp, size_t stack);
void	start_external_sprocs(sort_t *sort);
void	start_external(sort_t *sort);
int	runio_error(const aiocb_t *aio);
ssize_t runio_return(aiocb_t *aio);
int	runio_read(aiocb_t *aio);
int	runio_write(aiocb_t *aio);
/*int	runio_final_write(aiocb_t *aio);*/
void	runio_newrun(sort_t *sort, in_run_t *run);

	/* Where should these functions really belong? */
void	api_sort_done(sort_t *sort);
void	do_cancel(sort_t *sort);
void	suspend_others(void);

	/* hash.c */
unsigned calc_hash(sort_t *sort, const byte *rec, unsigned rec_len);
unsigned calc_hash_fast(sort_t *sort, const byte *rec, unsigned rec_len);

histogram_t	*hist_alloc(int first, int last, float gran);
void	hist_popgrow(histogram_t *hist, int citizen);
void	hist_print(const histogram_t *hist, const char *title, FILE *fp);

	/* ordutils.c */
char *get_time_str(sort_t *sort, char *buf);

	/* ordaio.c */
void print_total_time(sort_t *sort, const char *title);

extern char	NsortSourceDate[];

#if !defined(_SGIAPI)		/* exists in IRIX 5.3, but is not declared! */
extern ssize_t pread(int, void *, size_t, off64_t);
extern ssize_t pwrite(int, const void *, size_t, off64_t);
#endif

#if defined(DEBUG1)
extern byte	*DebugKey;
extern byte	*DebugPtr;
#endif

#endif /* _OTCSORT_H */
