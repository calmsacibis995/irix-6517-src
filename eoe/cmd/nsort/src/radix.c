/*
 ****************************************************************************
 *    Copyright (c) 1996 Ordinal Technology Corp.  All Rights Reserved      *
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
 ****************************************************************************/

#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include "merge.h"
#include "radix.h"
#include "math.h"

int list_count(sort_t *sort, list_desc_t *list)
{
    int		count = 0;
    item_line_t	*ll;
    item_t	*item;

    for (ll = list->head; ll != list->tail; ll = LINE_NEXTLINE(ll, sort->ll_item_space))
	count += sort->ll_items;

    for (item = ll->item; item->ovc != EOL_OVC; item = item_math(item, sort->item_size))
	count++;
    return (count);
}

/*
 * hist_alloc	- allocate a histogram with either a linear or exponential
 *		  pattern of successive bucket sizes
 *
 * Paramters:
 *	gran	== 0? each bucket covers the 'first' entries more then prev one
 *		!= 0? each bucket covers 'gran' times as many entries as prev
 */
histogram_t *hist_alloc(int first, int last, float gran)
{
    int		n_elements;
    int		range_end;
    int		i;
    histogram_t	*hist;
    
    if (gran == 0)
	n_elements = last / first;
    else for (n_elements = 1, range_end = first; range_end < last; range_end *= gran)
	n_elements++;
    hist = (histogram_t *) malloc((n_elements + 1) * sizeof(*hist));
    for (i = 0, range_end = first; i != n_elements; i++)
    {
	hist[i].population = 0;
	hist[i].end = range_end;
	if (gran == 0)
	    range_end += first;
	else
	    range_end *= gran;
    }
    hist[i].end = UINT_MAX;
    hist[i].population = 0;
    return (hist);
}

void hist_popgrow(histogram_t *hist, int citizen)
{
    while (hist->end < citizen)
	hist++;
    hist->population++;
}

void hist_print(const histogram_t *hist, const char *title, FILE *fp)
{
    int	i;
    int widths[40];
    int last_entry;

    for (i = 0, last_entry = 0; hist[i].end != UINT_MAX; i++)
    {
	if (hist[i].population != 0)
	    last_entry = i;
	widths[i] = 2 + log10(max(hist[i].end, hist[i].population));
    }

    if (title)
	fprintf(fp, "%s\n", title);
    for (i = 0; i <= last_entry; i++)
	fprintf(fp, "%*d ", widths[i], hist[i].end);
    fputc('\n', fp);

    for (i = 0; i <= last_entry; i++)
	fprintf(fp, "%*d ", widths[i], hist[i].population);
    if (hist[i].population != 0)
	fprintf(fp, " %d", hist[i].population);
    
    fputc('\n', fp);
}


/*
 * init_buckets	- make the array of to-be-dispered-to buckets all empty
 *
 * Caveat:
 *	The setting of next_item to item_space to a non-null invalid pointer
 *	is intentional - so that the FULL_RADIX_LINE() macro returns TRUE
 *	for a bucket which is still empty and has neither head nor tail.
 */
void init_buckets(sort_t *sort, bucket_t *bucket)
{
    unsigned	k;
    unsigned	item_space = sort->ll_item_space;

    for (k = 0; k < (1 << RADIXSHIFT); k++)
    {
	bucket[k].head = bucket[k].tail = NULL;
	bucket[k].next_item = (byte *) item_math(NULL, item_space);
	bucket[k].line_count = 0;
    }
}

/*
 * radix_sort
 */
void radix_sort(sort_t		*sort,
		bucket_t	*in_bucket, 
		unsigned	shift, 
		list_desc_t	*out_list,
		merge_args_t	*args)
{
    unsigned	i;
    bucket_t	out_buckets[1 << RADIXSHIFT];
    radix_count_fp	count;
    radix_disperse_fp	disperse;
    
    if (Print_task)
	fprintf(stderr, "radix_sort:starting @ shift %d\n", shift);
    if (sort->method == SortMethPointer)
    {
	count = radix_count;
	disperse = radix_disperse;
    }
    else
    {
	count = radix_count_rs;
	disperse = radix_disperse_rs;
    }

    for (i = 0; i < (1 << RADIXSHIFT); i++)
    {
	ASSERT(out_list->tail == LINE_START(out_list->next_item));
	if (in_bucket[i].line_count == 0)
	    continue;

	/* if we have no more radix bits
	 */
	if (shift < RADIXSHIFT)
	{
	    /* perform ovc-style sorts to complete ordering this bucket */

	    last_resort(sort, &in_bucket[i], out_list);
	    continue;
	}

	/* if the bucket is 8K or less in size
	 */
	if (in_bucket[i].line_count <= 16)
	{
#if defined(DEBUG2)
	    if (Print_task)
		fprintf(stderr,
			"radix_count-sorting bucket %d of %d recs @ shift %d\n",
			i, 
			sort->ll_items * (in_bucket[i].line_count - 1) +
			 (in_bucket[i].next_item - FIRST_RADIX_ITEM(in_bucket[i].tail)) /
			  sort->item_size,
			shift);
#endif
	    (*count)(sort, &in_bucket[i], shift, out_list, args);
	    continue;
	}

	init_buckets(sort, out_buckets);
	
	if (Print_task)
	    fprintf(stderr, "radix_sort:dispersing bucket %d @ shift %d\n",
			    i, shift);
	(*disperse)(sort, &in_bucket[i], out_buckets, shift - RADIXSHIFT, out_list);	

	radix_sort(sort, out_buckets, shift - RADIXSHIFT, out_list, args);
    }
}

item_line_t *gen_radix_list(sort_t	*sort,
			    part_t	*part,
			    item_line_t	**ll_sproc)
{
    unsigned		group_size;
    byte		*rec;
#if defined(DEBUG1)
    int			recs_genned = 0;
#endif
    unsigned		i, j;
    item_line_t		*ll;
    unsigned		n_recs;
    merge_args_t	args;
    list_desc_t		result;
    radix_disperse_fp	disperse;
    bucket_t		in_bucket, out_buckets[1 << RADIXSHIFT];
    node_t		node_buf[1 + GEN_WIDTH * 2];

    ARENACHECK;

    if (STATS.backwards_histogram == NULL)
    {
	STATS.backwards_histogram = hist_alloc(4, part->n_recs / 16, 1.5);
	STATS.radixcount_histogram = hist_alloc(RADIXSHIFT, RADIXBITS, 0);
    }
    part->sort_time = get_cpu_time();

    args.node_buf = node_buf;
    args.recode.summarize = sort->n_summarized;
    args.recode.n_soft = 0;
    args.recode.n_hard = 0;

    /* make an initial result list containing a single empty line
     */
    result.head = result.tail = *ll_sproc;
#if defined(INITIAL_FREELIST)
    result.free = *((item_line_t **) result.head);
#else
    result.free = LINE_NEXTLINE(result.head, sort->ll_item_space);
#endif
    result.next_item = (byte *) result.head->item;

    init_buckets(sort, out_buckets);
    
    group_size = sort->ll_items * MAX_GEN_N_RADIX_LINES;
    rec = part->rec_start;

    if (sort->method == SortMethPointer)
	disperse = radix_disperse;
    else
	disperse = radix_disperse_rs;

    /* Perform the first dispersion using the most significant bits of the key
     */	
    for (n_recs = part->n_recs; n_recs > 0; n_recs -= group_size)
    {
	if (n_recs <= group_size)	/* in the last records? reduce count */
	    group_size = n_recs;

#if 1
	in_bucket.head = in_bucket.tail = NULL;
	in_bucket.line_count = 0;
	in_bucket.next_item = FIRST_RADIX_ITEM(in_bucket.head);
#endif

	/* create radix items in lines, append to in_bucket */
	rec = ((init_radix_t) sort->init_item)(sort, group_size, rec, &in_bucket, part);

#if defined(DEBUG1)
	recs_genned += group_size;
#endif

	(*disperse)(sort, &in_bucket, out_buckets, RADIXBITS - RADIXSHIFT, &result);
    }

    /* recursively sort the first-pass buckets
     */
    radix_sort(sort, out_buckets, RADIXBITS - RADIXSHIFT, &result, &args);

    ASSERT(result.tail == LINE_START(result.next_item));
    ((item_t *) result.next_item)->ovc = EOL_OVC;

    *ll_sproc = result.free;

    part->sort_time = get_cpu_time() - part->sort_time;

    /*
     * Add this sproc's recode counts to the sums in the sort struct
     */
    {
	mpcount_t recodes;
	for (;;)
	{
	    recodes = TIMES.hard_recodes;

	    if (uscas(&TIMES.hard_recodes,
		      recodes,
		      recodes + args.recode.n_hard,
		      SYS.ua))
		break;
	}

	for (;;)
	{
	    recodes = TIMES.soft_recodes;

	    if (uscas(&TIMES.soft_recodes,
		      recodes,
		      recodes + args.recode.n_soft,
		      SYS.ua))
		break;
	}
    }

    ARENACHECK;

#if defined(DEBUG1)
    i = check_ll(sort, result.head->item);
    if (i != recs_genned && !sort->delete_dups && !sort->summarize)
	die("gen_radix_list:%d records missing", recs_genned - i);
#endif

    return (result.head);
}
