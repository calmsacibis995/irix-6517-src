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
 * $Ordinal-Id: merge.h,v 1.21 1996/10/25 22:51:11 chris Exp $
 * $Revision: 1.1 $
 */

#ifndef _MERGE_H_
#define _MERGE_H_

typedef struct
{
    item_t	*curr_item;
} leaf_t;

typedef struct
{
    item_t	*curr_item;
    item_t	*back_item;
} leaf_skip_t;

typedef struct
{
    item_t	*curr_item;
    unsigned	curr_eov;
} leaf_eov_t;

typedef struct
{
    item_t	*curr_item;
    unsigned	curr_eov;
    item_t	*back_item;
} leaf_eov_skip_t;

typedef struct rec_out
{
    byte		*first;
    byte		*limit;
    byte		*end;
} rec_out_t;

/* A record sort's merge_part() uses this 
 */
typedef struct rec_out_buf
{
    union
    {
        byte		*buf;
	blockheader_t	*blks;
    }			u;
    byte		*begin_out;
    unsigned		out_size;
    i8			end_offset;	/* for write()s after merge */
    unsigned		guess_rem_offset;
    unsigned char	end_of_merge;
    unsigned		n_temp_blocks;	/* for temp file output, number of 
					 * temp file blocks produced by this
					 * merge partition. */
    byte		*tail_rec;
    byte		*remainder;
    node_t		*tree;
    rec_out_t		*rec_outs;	/* for temp merge_part{,_ptr}() */
					/* #recs/merge * merge_cnt of them */
} rec_out_buf_t;

typedef struct ptr_out_buf
{
    byte		*ptrs;		/* array of ptrs [& sizes if variable]*/
    volatile byte	**begin_ptr;	/* position from which copying starts */
    volatile byte	**tail_ptr;	/* points into ptrs, >= begin_ptr */
    volatile byte	**limit_ptr;	/* limit of ptrs, >= tail_ptr */
    byte		**guess_tail_ptr;/* internal: prediction for tail_ptr */
					/* wrong iff this merge deletes dups */ 
    unsigned char	end_of_merge;
    node_t		*tree;
} ptr_out_buf_t;

typedef struct merge_args
{
    unsigned		n_inputs;
    item_line_t		**in_list;
    node_t		*node_buf;
    recode_args_t	recode;

    /* the following members are only used for a two-pass, output phase merge */
    unsigned		sproc_id;
    unsigned		merge_index;
    ptr_out_buf_t	*ptr_buf;     /* ptr_buf for this merge partition */
    tbuf_t		*norm;        /* list of tbufs that have been normally
				       * consumed by this merge partition, but
				       * have not yet been marked as norm 
				       * consumed because the previous merge
				       * partition has not completed */
    unsigned		end_consume;  /* end consumption point for this merge */
} merge_args_t;

/*
 * Merge_ptr_out results
 *
 *	buf and begin_out point to the output buffer, as in a record sort.
 *	merge_ptr out does not touch the output buffer directly, that is
 *	the task of copy_output.
 *	first_ptr points into the Out_line array of pointers (and sizes, in
 *	a variable length sort.)  Record pointers from first_ptr up to
 *	but not including tail_ptr are to be copied into the output buffer.
 *
 * A pointer merge fills in an array of pointers (for fixed length records)
 * or of pointers and two-byte sizes (for variable length records).
 * ptr_results_t would be a struct containing this union if the compiler
 * could be told to turn off alignement of the struct to word boundaries.
 * We really want the struct to be 6 bytes long, not 8 as alignment would have it
 */
#define MERGE_PART_DATA (65536 * 4)	/* approx #bytes in a single merge result (ptr_out_buf) */
#define MERGE_PART_RECS_MAX  MERGE_PART_DATA
#define PTR_RESULT_SIZE	((int) (sizeof(byte *)))
#define VAR_RESULT_SIZE	((int) (sizeof(byte *) + sizeof(u2)))
#define PTR_PART_ENTRY_SIZE ((sort->record.flags & RECORD_FIXED) ? PTR_RESULT_SIZE : VAR_RESULT_SIZE)

typedef struct copy_task
{
    byte	*dest_buf;	/* output buffer to be filled */
    unsigned	copy_bytes;	/* #bytes to copy into the output buffer */
    int		skip_bytes;	/* if != 0, part of the first record has */
				/* already been taken by another copy task */
    byte	**src_rec;	/* array of pointers [& sizes, if variable] */
    byte	**limit_rec;	/* limit (one past last entry) to ptr array */
    byte	**final_rec;	/* the last pointer acutally used by the copy */
    byte	*out_buf;	/* if not NULL, this copy is the last one
				   for the referenced output buffer */
    tbuf_t	*tbuf;		/* list of tbufs whose last record has been
				 * completely copied by this copy task. */
    off64_t	write_position;	/* if copies_write this is pos to write to */
    unsigned	copy_done : 1;
    unsigned	merge_part_done : 1;  /* this is last copy for a ptr parttn */
    unsigned	end_of_merge : 1;
#if defined(DEBUG2)
    merge_t	merge;
    copy_t	copy;
    ptr_out_buf_t pp;
#endif
} copy_task_t;


typedef enum consumetype {
	NormConsume = 0,
	RemConsume,
	SkipConsume
} consumetype_t;

void process_norms(sort_t *sort, tbuf_t *tbuf, unsigned merge_index);

CONST item_t *next_tbuf(sort_t	*sort,
		        CONST item_t	*last_item,
		        consumetype_t	type,
			merge_args_t	*mg_args);

item_line_t *merge_rec_line(sort_t	*sort,
			    merge_args_t *args,
			    item_line_t	**ll_free,
			    list_desc_t	*result);
item_line_t *merge_rec_line_12(sort_t	*sort,
			    merge_args_t *args,
			    item_line_t	**ll_free,
			    list_desc_t	*result);

int merge_rec_skip(sort_t	*sort,
		   merge_args_t *args,
		   int		recs_to_skip,
		   int		skip_interval);

int merge_rec_skip_12(sort_t	*sort,
		   merge_args_t *args,
		   int		recs_to_skip,
		   int		skip_interval);

unsigned merge_rec_out(sort_t		*sort,
		       merge_args_t	*args,
		       rec_out_t 	*rec_out,
		       int		recs_to_skip,
		       int		recs_to_consume);
unsigned merge_rec_out_12(sort_t		*sort,
		       merge_args_t	*args,
		       rec_out_t 	*rec_out,
		       int		recs_to_skip,
		       int		recs_to_consume);

item_line_t *merge_ptr_line(sort_t	*sort,
			    merge_args_t *args,
			    item_line_t	**ll_free,
			    list_desc_t	*result);
int merge_ptr_skip(sort_t	*sort,
		   merge_args_t *args,
		   int		recs_to_skip,
		   int		skip_interval);
unsigned  merge_ptr_out(sort_t		*sort,
			merge_args_t	*args,
			rec_out_t	*outbufs,
			int		recs_to_skip,
			int		recs_to_consume);

void merge_part(sort_t *sort, unsigned merge_index, unsigned sproc_id);
void merge_part_ptr(sort_t *sort, unsigned merge_index, unsigned sproc_id);
void tail_ptr_check(sort_t *sort, unsigned merge_index);

void copy_output(sort_t *sort, copy_task_t *copy);

void alloc_out_lists(sort_t *sort);
void alloc_ptr_out_lists(sort_t *sort);
#endif /* _MERGE_H_ */
