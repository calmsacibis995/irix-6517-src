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
 * prepare.c	- miscellaneous sort preparation functions
 *
 *	$Ordinal-Id: prepare.c,v 1.57 1996/12/23 23:50:36 chris Exp $")
 */
#ident	"$Revision: 1.2 $"

#include "otcsort.h"
#include "merge.h"
#include "radix.h"
#include <sys/prctl.h>
#include <fcntl.h>
#include <sys/sysmp.h>


/*
 * This macro calculates the number of refuge-area linelists that will
 * be needed to hold the lengths of a partition's variable length records.
 * The "% (sizeof(item_t) / sizeof(u2))" accounts for the fact that after
 * "(sizeof(item_t) / sizeof(u2))" (i.e. 6) linelists worth of lengths have been
 * put into items, that space may then be used for items itself.
 * The "1 +" is due to the fact that (nearly) every variable length sort
 * will need at least one linelist reserved solely for record lengths
 * (the last record length ll is not ever reused to hold items).
 * The "n_recs % LL_ITEMS" term comes from the fact that
 * gen_list usually starts with a 'short' line.  This simplifies the
 * merge strategy of gen_list, but it also delays the time at which a length ll
 * may be recycled and used for items. Suppose we are in a pointer sort which
 * has n_recs == 1310. The first ll is recycled for gen_line #6, the second
 * for #12, but the third isn't entirely freed until #19. Bummer. We account for
 * this deferral by increasing the number of refuge line lists by one when
 * recycling is possible (which is most of the time).
 */
/*
 * vll	- cumulative number of length lines, over and above the item lines
 *
 *	1 - #lines needed to hold two-byte lengths, rounded up if not even
 *	2 - 
 *	3 - subtract the number of lines which can be recycled as item lines
 *	    once all a length line's lengths have been stashed, rounded down
 */
int vll(int n_recs)
{
    int lenlls;

    lenlls = ROUND_UP_DIV((n_recs), LL_BYTES / 2);
    if (((n_recs) >= LL_ITEMS * (sizeof(item_t)/2)) &&
	((n_recs) % LL_ITEMS) == 0)
	lenlls++;
    lenlls -= (n_recs) / (LL_BYTES / 2);

    if (lenlls == 0 && n_recs != 0)	/* n_recs a multiple of LL_BYTES / 2 */
	lenlls = 1;
    return (lenlls);
}
#define VAR_LENGTH_LLS(n_recs)	vll(n_recs)

/*
 * Calculate the number of refuge-area linelists necessary to hold the
 * edited versions of a partition's pointer-sorted records.
 * If the local region is to be preferred (first alloc'd from) over the refuge
 *	1- calculate how many edited records will fit in a line
 *	2- calculate the number of lines needed to hold this many records
 * Else (always allocate the full refuge amount, then use the local):
 *	local_edited_n_recs * edited_size <= n_recs * orig_size
 *	local_edited_n_recs <= (n_recs * orig_size) / edited_size
 *	refuge_size = (n_recs - local_edited_n_recs) * edited_size, ll-rounded 
 *
 * This is a floating point calc to handler edited sizes larger than LL_BYTES
 */
int EDIT_REFUGE_LLS(int n_recs, int growth, int max_edited_size)
{
    int p = (n_recs * (max_edited_size - growth)) / max_edited_size;
    int bytes = (n_recs - p) * max_edited_size;

    return (ROUND_UP_DIV(bytes, LL_BYTES));
}

/*
 * optimize_keys	- coalesce adjacent compatible keys here.
 *			  This is done after prepare_edits()
 *			  has adjusted the keys' positions.
 *
 *	Optimizations:
 *		Merging of adjacent compatible keys is possible when
 *			- fixed length
 *			- same ascending/descending order
 *			- same key significance
 *			- adjacent in the data record
 *			- the second field need not be normalized
 */
void optimize_keys(sort_t *sort)
{
#if 0	/* XXX callers cannot yet handle keys moving */
    keydesc_t	*k1;
    keydesc_t	*k2;

    for (k1 = sort->keys; k1->type != typeEOF; k1++)
    {
	for (k2 = k1 + 1; k2->type != typeEOF; )
	{
	    /*
	     * If the same key appears twice the remove the later occurrence
	     * XXX Does the offset of the key before or after the deleted key
	     * need to be recalculated?  Will the 'offset space' gap hurt?
	     */
	    if (k1->position == k2->position && k1->type == k2->type &&
		k1->length == k2->length && k1->flags == k2->flags &&
		k1->pad == k2->pad && k1->delimiter == k2->delimiter)
	    {
		memmove(k2, k2+1, (sort->keycount - (k2 - sort->keys)) * sizeof(*k2));
		sort->keycount--;
		continue;
	    }
	    k2++;
	}
    }
#endif
}

/*
 * prepare_fields - fiddle with the fields to reduce recode's work
 *
 *	Parameters:
 *
 *	Tasks:
 *		Set the minimum and possibly maximum record lengths
 *		Set FIELD_TOGGLESIGN for floating point and signed integers?
 *		Set FIELD_SHIFTY (when there are field position tables?)?
 *
 */ 
void prepare_fields(sort_t *sort)
{
    field_t	*field;
    int		delimsize = (RECORD.flags & RECORD_STREAM) ? 1 : 0;

    if (RECORD.flags & RECORD_FIXED)
    {
	RECORD.minlength = RECORD.length;
	RECORD.maxlength = RECORD.length;
    }
    else
    {
	if (RECORD.minlength == 0 && (RECORD.flags & RECORD_STREAM))
	    RECORD.minlength = 1;	/* min, just for the newline */
	if (RECORD.maxlength == 0)
	{
	    if (RECORD.flags & RECORD_VARIABLE)
		RECORD.maxlength = USHRT_MAX + sizeof(u2);
	    else
		RECORD.maxlength = USHRT_MAX;
	}
    }

    for (field = RECORD.fields; field->type != typeEOF; field++)
    {
	/* For length-prefix varlen records the user specifies the fields
	 * as a byte offset from the end of the two-byte length.  Inside the
	 * sort the fields' position is relative to the start of the record.
	 */
	if ((RECORD.flags & RECORD_VARIABLE) &&
	    !(field->flags & FIELD_VARPOSITION))
	{
	    field->position += sizeof(u2);
	    field->flags |= FIELD_VARPOSITION;
	}

	if (!IsVariableType(field->type) &&
	    !(field->flags & FIELD_DERIVED) &&
	    RECORD.minlength < field->position + field->length + delimsize)
	{
	    if (RECORD.flags & RECORD_FIXED)
		runtime_error(sort, NSORT_FIELD_BEYOND_END, field->name);
	    RECORD.minlength = field->position + field->length + delimsize;
	}
    }

    if (RECORD.length < RECORD.minlength)
	RECORD.length = RECORD.minlength;
    if (RECORD.flags & RECORD_FIXED)
	RECORD.minlength = RECORD.length;
}

/*
 * prepare_edits - fiddle with the edit statements to reduce execute_edits' work
 *
 *	Parameters:
 *
 *	Tasks:
 *		append a terminating length == 0 marker editing instructions
 *		set sort->item_size
 *		adjust any key positions which change because key fields move
 *
 *	Optimizations:
 *		Merging of adjacent compatible edits when possible:
 *			- fixed length
 *			- adjacent in the data record
 */
void prepare_edits(sort_t *sort)
{
    keydesc_t	*key;
    field_t	*field;
    field_t	*next;
    edit_t	*edit;
    edit_t	*last;
    unsigned	position;

    sort->edit_growth = 0;
    if (sort->n_edits == 0)
    {
	if (sort->n_derived == 0)
	{
	    sort->edited_size = RECORD.length;
	    return;
	}
	
	/*
	** If we get here then there are derived fields but no
	** other editing statements.  Make up an edit which covers
	** the entire input record, then add one for each derived field.
	*/
	sort->n_edits = 1;
	edit = (edit_t *) malloc((2 + sort->n_derived) * sizeof(*edit));
	sort->edits = edit;
	edit->is_field = TRUE;
	edit->is_variable = (RECORD.flags & RECORD_TYPE) != RECORD_FIXED;
	edit->length = edit->is_variable ? EDIT_ACTUAL_LENGTH
					 : RECORD.length;
	edit->position = 0;
	edit->source.field_num = -1;
    }

    /*
    ** Put any derived fields 'after' the input record's fields.
    ** XXX Danger: derived fields after variable length fields?
    ** e.g. binary int4/8 summary after character delimited keys
    **
    ** 	uh oh - mixing 'text' and binary.
    ** 	uh oh - fields which sort has to access that are not at a fixed
    **		byte position relative to the start of the record.
    */
    position = RECORD.length;
    for (field = RECORD.fields; field->type != typeEOF; field++)
    {
	if (field->flags & FIELD_DERIVED)
	{
	    field->position = position;
	    position += field->length;
	    if (position >= MAXRECORDSIZE)
		runtime_error(sort, NSORT_DERIVED_TOO_FAR, field->name,
						      MAXRECORDSIZE - 1);
	    sort->edit_growth += field->length;
	    add_edit(sort, field);
	}
    }

    /*
    ** Transform the field and key positions from their input record values
    ** to the corresponding positions in the edited record.
    ** The keys may be optimized after this.
    */
    last = &sort->edits[sort->n_edits - 1];
    for (edit = sort->edits, position = 0; edit <= last; edit++)
    {
	if (edit->is_field && edit->source.field_num != -1)
	{
	    /*
	    ** Now that we have all the fields, translate the index to the
	    ** field into its pointer, so that execute_edit will have
	    ** a little less work to do.
	    */
	    edit->source.field = &RECORD.fields[edit->source.field_num];

	    for (key = sort->keys; key->type != typeEOF; key++)
	    {
		if (key->position == edit->source.field->position)
		{
		    key->position = position;
		    break;
		}
	    }
	    edit->source.field->position = position;
	}
	position += edit->length;
    }
    
    /*
    ** Coalesce edits of fields which are adjacent in both the
    ** original input record format and the edited sorting record format.
    */
    edit = sort->edits;
    sort->edited_size = edit->length;
    while (edit < last)
    {
	sort->edited_size += edit[1].length;
	if (edit[0].is_field && edit[1].is_field)
	{
	    field = edit[0].source.field;
	    next = edit[1].source.field;
	    if ((field->position + field->length == next->position)  &&
	        !(field->flags & FIELD_VARIABLE) &&
	        !(next->flags & FIELD_VARIABLE))
	    {
		edit->length += next->length;
		last--;
		sort->n_edits--;
		memmove(edit + 1, edit + 2, (last - edit) * sizeof(*edit));
		continue;
	    }
	}
	edit++;
    }

    /*
    ** Terminate the edit list with a zero length edit.
    */
    edit = (edit_t *) realloc(sort->edits, (sort->n_edits + 1) * sizeof(*edit));
    sort->edits = edit;
    edit[sort->n_edits].length = 0;
}

void prepare_temp(sort_t *sort)
{
    u4		min_recs;
    u4		tbuf_size;
    u4		size;
    u4		advance;
    char	tstr[40];

#if 0
    if (sort->internal)
    {
	RECORD.max_intern_bytes = 0;
	TEMP.adv_info_size = 0;
	TEMP.optimal_reads = FALSE;
	TEMP.tbuf_size = 0;
	return;
    }
#endif

    RECORD.max_intern_bytes = (RECORD.flags & RECORD_FIXED) ? sort->edited_size
							    : RECORD.maxlength;
    /* The advance info contains a header followed by a
     * record containing the advance keys. The actual adv_info size
     * varies withe the record size, but it must be aligned as required
     * for an eight byte integer (adv_info.off). So we aligned it up to
     * a four byte boundary in 32-bit mode, and eight in 64-bit mode,
     * as implicitly indicated by the size of ptrdiff_t. (ugh).
     */
    size = (u4) offsetof(adv_info_t, rec[0]) + RECORD.max_intern_bytes;
    TEMP.adv_info_size = (u4) ROUND_UP_COUNT(size, sizeof(ptrdiff_t));

    /* if we can fit the advance info for optimal reads in a block, 
     * do optimal reads.
     */
    advance = max(2, TEMP.n_files) + 1;		
    if (advance * TEMP.adv_info_size <= TEMP.blk_size - sizeof(blockheader_t))
    {
	TEMP.optimal_reads = TRUE;
	TEMP.advance = advance;
    }

    /* A tbuf has
     *	the tbuf header
     *	the temp file i/o block
     *	additional space for the items (which is smaller if there are optimal
     *					read ordering keys in the block)
     *  a NULL_OVC item
     * and is rounded up to a direct i/o memory alignment boundary.
     */
    min_recs = (TEMP.blk_size - (int) sizeof(blockheader_t)) /
		RECORD.max_intern_bytes; 
    if (TEMP.optimal_reads)
	min_recs--;	/* allow for advance record */

    if (RECORD.flags & RECORD_FIXED)
    {
	tbuf_size = (u4) sizeof(tbuf_t) + TEMP.blk_size + NULL_ITEM_SIZE;
	if (sort->method == SortMethRecord)
	    tbuf_size += min_recs * (sort->item_size - sort->edited_size); 
	else
	    tbuf_size += min_recs * sizeof(item_t);
	TEMP.tbuf_size = ROUND_UP_COUNT(tbuf_size, TEMP.dio_mem);
    }
    else
    {
	/* XXX what should this be? */
	TEMP.tbuf_size = TEMP.blk_size * 2;
    }

    if (Print_task)
	fprintf(stderr, "blk sz: %d, tbuf sz: %d @ %s\n", 
			TEMP.blk_size, TEMP.tbuf_size, get_time_str(sort, tstr));
}

/*
 * guess_input_count	- calculate/guess the number and size of input records
 *
 *	If we cannot determine the size any of the input files, then the
 *	input size and record count will be set to infinite (MAX_I8).
 *	read_input() will gobble up everything until EOF on the last input file.
 *
 *	Side Effects:
 *		sets n_recs
 *		sets input_size
 */
void guess_input_count(sort_t *sort)
{
    int	i;

    /* An api_invoked sort may have had its size already set
     */
    if (IN.input_size != 0)
	return;

    for (i = 0; i < IN.n_files; i++)
    {
	if (SIZE_VALID(IN.files[i].stbuf.st_mode))
	{
	    IN.input_size += IN.files[i].stbuf.st_size;
	}
	else
	{
	    IN.input_size = MAX_I8;
	    break;
	}
    }
}

#define AUTORECORDSORT	32
/*
 * choose_sort_method	- figure the type of sort (record, pointer) to do
 *
 *	This has to figure out how big the [possibly edited] record sort item
 *	will be.  The item size is crucial is determining the relative memory
 *	usage of a pointer sort versus a record sort.
 *	Returns:
 *		TRUE	iff there was an error which will abort the sort
 *		FALSE	only when the sort may proceed
 */
unsigned choose_sort_method(sort_t *sort)
{
    ASSERT(sort->edited_size > 0);

    /* For a while, at least, prehashing is disabled in 64-bit mode
    */
    if (sizeof(item_t *) != sizeof(ovc_t))
    {
	if (sort->prehashing == SortPreHashRequired)
	    die("prehashing does not yet work in 64-bit sorts");
	sort->prehashing = SortPreHashForbidden;
    }

    if (!(RECORD.flags & RECORD_FIXED))
    {
	if (sort->method == SortMethRecord)
	{
	    runtime_error(sort, NSORT_RECORD_SORTS_VARLEN);
	    return (TRUE);
	}
	sort->method = SortMethPointer;
    }
    else if (sort->method == SortMethUnspecified)
    {
	if (sort->edited_size > AUTORECORDSORT)
	    sort->method = SortMethPointer;
	else
	    sort->method = SortMethRecord;
    }
    else if (sort->method == SortMethRecord && sort->edited_size > MAXRECORDSORT)
    {
	runtime_error(sort, NSORT_RECORD_SORT_TOO_LARGE, MAXRECORDSORT);
	return (TRUE);
    }

    if (sort->method == SortMethPointer)
    {
	sort->item_size = sizeof(item_t);
	/* value codes are two bytes in pointer sorts,
	** offsets are also two bytes
	*/
	sort->ovcsize = 2;
    }
    else
    {
	sort->item_size = (short) ROUND_UP_NUM(sort->edited_size + sizeof(ovc_t), sizeof(item_t *));
	/* value codes are three bytes in record sorts; offsets only one byte
	*/
	sort->ovcsize = 3;

	/* XXX should record sorts try to do buffered writes directly?
	 */
	OUT.file->copies_write = FALSE;
    }
    sort->ll_items = LL_ITEMSPACE / sort->item_size;	/* ptr: 42/31 rec: 63/31->4 */
    sort->ll_item_space = sort->ll_items * sort->item_size;

    if (sort->radix)
    {
	/* Decide whether radix_disperse should attempt to delete/collapse recs.
	 * Currently its code handles a single fixed-length key
	 */
	sort->delete_in_disperse = (sort->delete_dups || sort->n_summarized) &&
				   sort->keycount == 1 &&
				   !IsVariableType(sort->keys->type);
	/* Don't do radix sorts and prehashing: too much trouble to support both
	 */
	if (sort->prehashing == SortPreHashRequired)
	{
	    runtime_error(sort, NSORT_RADIX_PREHASH_INCOMPAT);
	    return (TRUE);
	}
    }

    sort->virgin_ovc = ~0 << (sort->ovcsize * NBBY);
    sort->del_dup_ovc = 1 << (sort->ovcsize * NBBY);

    return (FALSE);
}

/*
 * simplified initial ovcs instructions may be suitable for certain
 * important cases  (e.g. fixed length fields at various, but fixed positions).
 * Now that the value code is 3 bytes (record sorts) or 4 bytes (all ptr sorts)
 * it seems worthwhile to try to come up with a sequence of one-byte
 * ovc filling instructions for fixed length strings, binary integers:
 *	position	byte offset from start of record to the byte
 *	xormask		usually 0, is  0x80 for first byte of signed integers
 *	
 * These will be interpreted by simple_initial_ovc().  If the first key
 * is a float or double type, then it will be handled by conditioning the
 * floating point value and copying the first three or four bytes to the ovc.
 *
 * How about something similar for variable length strings?
 * What are the possibilities for variable length fields?
 *  Lemma a:	if the first key is a 'padded' variable length type and
 *	the field may extend beyond the three or four bytes  of initial ovc
 *	then the initial ovc must contain data only from the first key.
 *  Why:
 *		Suppose there are are two keys for a sort, a variable length
 *		string and another, lesser-signficant key of any type.
 *		Suppoose further that there  are two records, one of which has
 *		long string as its first keys value, and the other of which
 *		has an empty first key field.  The ovc comparison of these
 *		two items must treat the latter record as if it were filled
 *		with pad characters. The latter item cannot be filled with
 *		'just enough' pads because at initial_ovc time it is unknown
 *		how long the 'other' items first key field is; it could be
 *		nearly any one in the line.  The only safe choice is to
 *		fill the initial ovc with bytes from the field up to
 *		the min of the actual field length and the ovc size.
 *		Any leftover bytes in the ovc must be filled with pads.
 *	This greatly simplifies initial ovc handling. 
 *		
 * If the initial value code for a sort includes data which is not of
 * an easily handleable type then we won't use the above very simple
 * instructions for the initial ovc.  There will be a more general version
 * (which looks at lot like recode???) to handle the other types.
 */

void compile_ovc_init(sort_t *sort)
{
    keydesc_t	*key;
    int		ovcleft = (sort->method == SortMethRecord) ? 3 : 4;
    ovc_init_t	*op;

    op = (ovc_init_t *) malloc(5 * sizeof(ovc_init_t));
    sort->ovc_init = op;
    for (key = sort->keys; key->type != typeEOF; key++)
    {
	op->position = key->position;
	op->max_length = key->length;
	op->xormask = key->xormask;
	op->length = min(ovcleft, key->length);
	switch (key->type)
	{
	  case typeFloat:
	    op->op = opFloat;
	    ovcleft = 0;	/* type size ensures it fills rest of ovc */
	    break;

	  case typeDouble:
	    op->op = opDouble;
	    ovcleft = 0;	/* type size ensures it fills rest of ovc */
	    break;

	  case typeI8:
	  case typeI4: 
	  case typeI2:
	  case typeI1: 
	    op->op = opCopy;
	    op->xormask ^= SIGN_BIT;
	    ovcleft -= key->length;	/* some will be <= 0 */
	    break;

	  case typeU8:
	  case typeU4: 
	  case typeU2:
	  case typeU1: 
	  case typeFixedString: 
	    op->op = opCopy;
	    ovcleft -= key->length;	/* some will be <= 0 */
	    break;

	  case typeFixedDecimalString: 
	    op->op = opDecimal;
	    ovcleft -= key->length;	/* some will be <= 0 */
	    break;

	  case typeDelimString:	
	  case typeDelimDecimalString:	
	    if (op > sort->ovc_init)	/* non-fixed-length padded strings */
	    {				/* must start in their own ovc */
		op->op = opPad;
	    }
	    else
	    {
		op->op = /* (key->type == typeDelimDecimalString) ? opVarDecimal
							       : */
		       opVariable;
		op->field_delim = key->delimiter;
		if (RECORD.flags & RECORD_STREAM)
		    op->record_delim = RECORD.delimiter;
		else
		    op->record_delim = key->delimiter;
		
	    }
	    op->pad = key->padword;
	    op->length = ovcleft;	/* must always be last */
	    ovcleft = 0;	/* padding semantics require it to end ovc */
	    break;

	  default:
	    die("compile_ovc_init: unknown type %d", key->type);
	}
	op++;
	if (ovcleft <= 0)
	    break;
    }

    if (ovcleft > 0)	/* is the total key smaller than ovc + eov? */
    {
	op->op = opPad;
	op->pad = 0;
	op->length = ovcleft;
	op++;
    }
    op->op = opEof;
}

/*
 * calc_mem_use	- figure how much memory we'll need, and how to divvy it
 *
 *	These calculations are described in "Partition Sorting"
 *
 *	Caveat:
 *		The calculations performed here must correspond with the
 *		assignment of memory done by map_io_buf()
 */
void calc_mem_use(sort_t *sort)
{
    i4		avg_local_lls;
    i4		part_lls;
    i4		part_recs;
    i8		run_recs;
    i8		run_data;
    u4		run_data_unit;		/* minimum unit of run_data changes */
    i8		high_data;
    i8		low_data;
    i8		mem_avail;
    i4		refuge_lls;
    i4		input_runs;
    i4		high_runs;
    i4		low_runs;
    i8		in_used;
    i8		out_used;
    i8		input_size;
    i8		file_size;
    in_run_t	*run;
    struct rlimit memlimit;
    long procsize;
    unsigned	long i, j;
    i8		temp;
#define RECS_PER_RUN_TARGET	400

    /* worst case radix ll need is that most records have the same hash value,
     * except one record at each level of radix_disperse (1 << RADIXSHIFT - 1)
     * per level (RADIXBITS / RADIXSHIFT).  That's a lot of lines, e.g. for
     * RADIXSHIFT == 4, 15 * 8 or 120 extra lines per sproc.  Ouch!
     */
    sort->n_ll_free = (1 << RADIXSHIFT) * ROUND_UP_DIV(RADIXBITS, RADIXSHIFT);
    if (!sort->radix || sort->n_ll_free < GEN_WIDTH)
	sort->n_ll_free = GEN_WIDTH;

    /*
     * Determine some of gen_line()'s parameters:
     *	How many lists it should build and then merge
     *	How many items should be in the all-but-last lists
     *	How many items should be in the last list, if ll_items is not
     *	an integer multiple of the number of lists
     */
    sort->n_sub_lists = (getenv("SUBLISTS") != NULL) ? atoi(getenv("SUBLISTS"))
						     : 3;
    if (sort->n_sub_lists > 6)	/* gen_line()'s local vars handle 6 sub lists */
	sort->n_sub_lists = 6;

    sort->sub_items = ROUND_UP_DIV(sort->ll_items, sort->n_sub_lists + 1);
    /* Avoid sublists which are too small; arbitrarily pick 7 as the min sublist
     */
    if (sort->sub_items < 7)
    {
	sort->sub_items = min(7, sort->ll_items);
	sort->n_sub_lists = sort->ll_items / sort->sub_items;
    }
    else
	ASSERT(sort->n_sub_lists == sort->ll_items / sort->sub_items);
    sort->rem_items = sort->ll_items - sort->n_sub_lists * sort->sub_items;

    /* if we haven't been told by the user how much memory to use, assume
     * we can allocate up to this process's resource limit.
     * We also subtract 8MB for other user programs.
     */
    if (getrlimit(RLIMIT_RSS, &memlimit) < 0)
	die("getrlimit(RLIMIT_RSS) failed: %s", strerror(errno));
    /* who cares? */
    procsize = (SYS.region_kbytes * 1024) + (40 * SYS.page_size);
    j = (unsigned) (memlimit.rlim_cur / 1024) - 8192;
    if (j > (unsigned) (SYS.user_data / 1024))
	j = (unsigned) (SYS.user_data / 1024);
    mem_avail = (j - SYS.region_kbytes) * 1024;
    if (sort->memory == 0)
    {
	if (j * 1024 > procsize)
	    mem_avail = j * 1024 - procsize;
	else if (sort->internal)
	    mem_avail = 10240 * 1024;	/* mem nearly full, try a min amt */ 
    }
    else if (sort->memory > SYS.free_swap / 1024)
	runtime_error(sort, NSORT_PAST_MEMORY_LIMIT, j);
    else
	mem_avail = sort->memory * 1024;

    /* set the minimum unit of run_data changes to be the maximum of 64K and
     * all input file transfer sizes.
     */
    run_data_unit = 64*1024;		/* MAXRECORDSIZE or something else? */
    if (!sort->api_invoked)
    {
	for (i = 0; i < IN.n_files; i++)
	    if (run_data_unit < IN.files[i].io_size)
		run_data_unit = IN.files[i].io_size;
	run_data_unit = (u4) ROUND_UP_COUNT(run_data_unit, IN.dio_io);
    }
	    
    if (Print_task)
	fprintf(stderr, "avail %lld\n"
   "data:  low        run       high  runs    in_used   out_used partrecs ref_lls\n",
			mem_avail);

    sort->begin_in_run = 0;

    /* In internal sorts the client has already allocated the memory for records
     * so we pretend that the data size is zero.  The refuge calculations
     * (except for editing, which is not supported by internal sorts) depend
     * only on the number of records and not their size.
     */
    if (sort->internal)
    {
	run_data = 0;		/* client already has the memory for records */
	input_runs = 2;		/* XXX 1 or 2? */
    }
    /* else if we know the exact input size
     */
    else if (IN.input_size != MAX_I8)
    {
	/* figure out how big the run_data could be with a one pass sort.
	 */
	if (sort->api_invoked)
	{
	    /* An api external sort with nsort_input_size/count()
	     * calculate a run_data size which is large enough for a one
	     * pass sort.
	     */
	    run_data = ROUND_UP_COUNT(IN.input_size / N_IN_RUNS, run_data_unit);
	    input_runs = ROUND_UP_DIV(IN.input_size, run_data);
	}
	else if (IN.n_files == 2)
	{
	    run_data = 
		max(IN.files[0].stbuf.st_size, IN.files[1].stbuf.st_size);
	    run_data = ROUND_UP_COUNT(run_data, run_data_unit);
	    input_runs = 2;
	}
	else
	{
	    /* calculate a run_data size which is large enough for a one
	     * pass sort.
	     */
	    run_data = ROUND_UP_COUNT(IN.input_size / N_IN_RUNS, run_data_unit);
	    
	    /* calculate the number of input runs.  This may not equal N_IN_RUNS
	     * because of fragmentation among multiple input files.
	     */
	    input_runs = 0;
	    input_size = IN.input_size;
	    for (i = 0; i < IN.n_files; i++)
	    {
		if (SIZE_VALID(IN.files[i].stbuf.st_mode))
		{
		    file_size = IN.files[i].stbuf.st_size;
		    if (file_size > input_size)
			file_size = input_size;
		    input_runs += ROUND_UP_DIV(file_size, run_data);
		    input_size -= file_size;
		    if (input_size <= 0)
			break;
		}
		else
		{
		    /* assume the worst case of one record input size.
		     */
		    input_runs++;
		}
	    }

	    /* if there are bytes leftover (because there are some
	     * files with unknown sizes)
	     */
	    if (input_size)
	    {
		/* round up remaining input_size, but subtract one run
		 * because in the worst case this remainder data belongs
		 * to the last input file with an unknown size, and we
		 * already added 1 to input_runs for that file.
		 */
		input_runs += ROUND_UP_DIV(input_size, run_data) - 1;
	    }
	}

	/* once we jump into the convergence loop below, if our initial
	 * run_data guess allows for N_IN_RUNS (a one pass sort) and the
	 * required amount of memory is <= the available memory, we will
	 * break out of the convergence loop and plan on a one pass sort.
	 * Otherwise we will do a binary search until we have found the
	 * minimum run_data value for a TWO PASS sort that allows for the
	 * predicted number of runs in the output phase (where the output
	 * phase uses the same amount of memory as the input phase). 
	 */
    }
    else
    {
	/* We don't really know how big the input is.  We will iterate
	 * until we find BOTH the maximum run_data size that will fit in 
	 * available memory, AND the maximum number of runs the output 
	 * phase can handle.
	 */
	run_data = (mem_avail / N_IN_RUNS) / run_data_unit * run_data_unit;
	input_runs = N_IN_RUNS * IN_RUN_PARTS;

	low_runs = N_IN_RUNS;
	high_runs = mem_avail / (2 * TEMP.tbuf_size);

	/* don't touch all allocated memory as we are not sure if we will
	 * need it all.
	 */
	sort->touch_memory = FlagFalse;
    }

    /* set the high limit on run_data to so that it is too big to
     * handle available memory.  If we later converge to this value for
     * a two pass sort, we will simply notice we don't have enough
     * memory and quit.
     */
    high_data = mem_avail;

    /* as the low run_data limit, round down the maximum record size to
     * the next lower run_data_unit boundry.  Note that run_data will be
     * constrained to be at least run_data_unit larger than this low_data.
     */
    low_data = MAXRECORDSIZE / run_data_unit * run_data_unit;

    /* iterate until we converge on optimal values of run_data and input_runs
     */
    for (j = 0; ; j++)
    {
	/* Since we do a binary search to find the optimal values,
	 * and there shouldn't be more than 64 bits in run_data or input_runs,
	 * die() after 64 iterations.
	 */
	if (j == 64)
	    die("can't determine run sizes - low_data %lld, high_data %lld\n"
		"    low_runs %d, high runs %d\n", 
		low_data, high_data, low_runs, high_runs);

	/* An internal sort is different: the space holding the records is
	 * part of the client's memory allocation - nsort doesn't need another
	 * copy of the records
	 */
	if (sort->internal)
	    run_recs = sort->n_recs;
	else
	{
	    run_recs = ROUND_UP_DIV(run_data, RECORD.minlength);
	    if (sort->n_recs != 0 &&
		run_recs > ROUND_UP_DIV(sort->n_recs, N_IN_RUNS))
		run_recs = ROUND_UP_DIV(sort->n_recs, N_IN_RUNS);
	}

	part_recs = ROUND_UP_DIV(run_recs, IN_RUN_PARTS);
	part_recs = ROUND_UP_COUNT(part_recs, sort->ll_items);
	
	/* Figure out refuge line-lists based on maximum number of partitions.
	 */
	if (sort->method == SortMethRecord)
	{
	    /* ll_items is the number of records per line - use it to find
	     * the number of lines needed for building items. Some of them
	     * will be in the low region into which records are read, others
	     * will be up in the refuge.
	     */
	    part_lls = ROUND_UP_DIV(part_recs, sort->ll_items) + MAX_GEN_N_LINES;

	    /*
	     *	Refuge size for record sorts
	     * On average a partition will 'lose' LL_BYTES because linelists
	     * need to be aligned, yet partitions have to be separated by
	     * record (not linelist) boundaries.
	     * Another LL_BYTES are not usuable for local linelists because
	     * records and their items cannot simultaneously share the same 
	     * line.  So, each partition has a two linelist wasted space cost.
	     * Internal sorts cannot reuse any of the input space for line lists
	     * (because the resulting records have to bo back into the same
	     * block of memory) so there are no local recyclable lls.
	     */
	    if (sort->internal)
		refuge_lls = part_lls;
	    else
	    {
		avg_local_lls = (part_recs * RECORD.length) / LL_BYTES;
		avg_local_lls -= 2;
		if (avg_local_lls <= 0)
		    avg_local_lls = 0;
		refuge_lls = part_lls - avg_local_lls;
	    }
	}
	else
	{
	    /* Items in a pointer sort are always in the refuge
	     */
	    refuge_lls = ROUND_UP_DIV(part_recs, sort->ll_items) +
			 MAX_GEN_N_LINES; 
	    /* Scanning partitions for variable length records temporarily
	     * uses refuge space for lengths.  Some of this space is reusable
	     * for items once enough records have been init_item_vs()'d.
	     * Therefore we don't have to reserve enough refuge space to hold
	     * simultaneously all of a partition's lengths and its items.
	     */
	    if (!(RECORD.flags & RECORD_FIXED))
		refuge_lls += VAR_LENGTH_LLS(part_recs);	

	    /* If record editing will cause the records to grow
	     * then reserve some refuge space for the larger records.
	     * Some will be grown in the refuge as they are grown.  This will
	     * free up space in the record area allowing some subsequent
	     * records to be edited into the record area.
	     * The edited_size is the (fixed length) record's maximum size.
	     * Currently we don't support editing of variable length records.
	     * When we do we'll need to define and set record.max_edited_size.
	     */
	    if (sort->edit_growth > 0)
		refuge_lls += 
		    EDIT_REFUGE_LLS(part_recs, sort->edit_growth, sort->edited_size);
	}
	sort->n_refuge_lls = 1 + IN_RUN_PARTS * refuge_lls;

	in_used = 0;
	for (run = sort->in_run; run < sort->in_run + N_IN_RUNS; run++)
	{
	    if (!sort->internal)
	    {
		run->read_buf = (byte *)in_used;

		/* leave at least REM_BUF_SIZE bytes to copy leftover 
		 * record(s) from previous run.
		 */
		in_used += REM_BUF_SIZE;

		/* allocate memory for reading data
		 */
		in_used += run_data;
	    }
	    run->read_target = (byte *)in_used;
	    /* calculate amount memory needed for refuge and sproc lls,
	     * then allocate them at the END of a IN.dio_mem boundry.
	     */
	    run->refuge_start = (item_line_t *) in_used;
	    in_used += LL_BYTES * (u8) sort->n_refuge_lls;
	    run->refuge_end = (item_line_t *) in_used;
	    run->refuge_next = (item_line_t *) in_used;
	    run->ll_free = (item_line_t *) in_used;
	    in_used += LL_BYTES * sort->n_ll_free * sort->n_sprocs;
	    in_used = ROUND_UP_COUNT(in_used, IN.dio_mem);

	    ASSERT(run->read_target <= (byte *)run->refuge_start);

	    /* Internal sorts have just one run. It contains all the data */
	    if (sort->internal)
		break;
	}
#if defined(tokPREHASH)
	if (sort->prehashing == SortPreHashRequired)
	{
	    int i;

	    for (i = 12; (1 << (i + 1)) < part_recs; i++)
		continue;
	    sort->hash_table_bits = i;
	    sort->hash_table_size = 1 << sort->hash_table_bits;

	    /* add in hash table size to run counts.
	     */
	    sort->hash_tables = (item_t **)in_used;
	    in_used += 
		sort->n_sprocs * sort->hash_table_size * sizeof(item_t *);
	}
#endif

	in_used = ROUND_UP_COUNT(in_used, max(OUT.dio_mem, TEMP.dio_mem));
	sort->end_in_run = (byte *)in_used;  /*note end of input oriented bufs*/
	
	if (sort->method == SortMethRecord)
	{
	    if (!sort->internal)
	    {
		/* allocate record output buffers
		 */
		in_used = ROUND_UP_COUNT(in_used, max(TEMP.dio_mem, OUT.dio_mem));
		MERGE.rec_bufs = (byte *)in_used;
		in_used += MERGE.rec_buf_size;
	    }

	    for (i = 0; i < MERGE.merge_cnt; i++)
	    {
		MERGE.rob[i].tree = (node_t *)in_used;
		in_used += N_IN_RUNS * IN_RUN_PARTS *
			   (sizeof(node_t) + sizeof(leaf_skip_t));
	    }
	}
	else
	{
	    /* allocate ptr partitions
	     */
	    for (i = 0; i < MERGE.merge_cnt; i++)
	    {
		if (!sort->internal)
		{
		    MERGE.ptr_part[i].ptrs = (byte *)in_used;
		    in_used += MERGE.part_ptr_cnt * PTR_PART_ENTRY_SIZE;
		    in_used = ROUND_UP_COUNT(in_used, sizeof(int *));
		}

		MERGE.ptr_part[i].tree = (node_t *)in_used;
		in_used += N_IN_RUNS * IN_RUN_PARTS * 
			   (sizeof(node_t) + sizeof(leaf_eov_skip_t));
	    }

	    if (!sort->internal)
	    {
		/* allocate write buffers for both temp and 'real' output
		 * In the input phase the TEMP file writes use OUT.buf.
		 */
		in_used = ROUND_UP_COUNT(in_used, max(OUT.dio_mem, TEMP.dio_mem));
		OUT.buf[0] = (byte *)in_used;
		in_used += 
		  max(OUT.buf_cnt*OUT.buf_size, TEMP.blk_size*TEMP.out_blk_cnt);
	    }
	}

	in_used = ROUND_UP_COUNT(in_used, SYS.page_size);
	/* XXX add another page of pad so that a ulw() done in the final byte
	 * of the last page will not cause a seg fault. The aggresive use of
	 * ulw() will still cause problems with the api version, where we can't
	 * control the location of data.
	 */
	in_used += SYS.page_size;
	sort->end_out_buf = (byte *) in_used;

	/* now figure out much memory will be required for the output
	 * phase if we assume we will merge input_runs runs during the
	 * output phase.
	 * XXX Doesn't this include the TEMP sizes even if this is one pass?
	 * XXX Internal effects??
	 */
	if (sort->internal)
	{
	    out_used = 0;
	}
	else if (sort->method == SortMethRecord)
	{
	    /* set output buf size so that around RECS_PER_RUN_TARGET
	     * records are consumed by each merge partition.  This is to
	     * keep the merge skip overhead down.
	     */
	    temp = input_runs * sort->edited_size * RECS_PER_RUN_TARGET;
	    TEMP.out_buf_size = ROUND_UP_COUNT(temp, OUT.buf_size);
	    out_used = MERGE.merge_cnt *
		       (TEMP.out_buf_size + 2 * OUT.rec_buf_pad);

	    /* calculate tree size based on the tentative input_runs.  round
	     * size up to 1K boundry so that different trees don't share cache
	     * lines.
	     */
	    TEMP.tree_size = (u4) (input_runs * (sizeof(node_t) + sizeof(leaf_skip_t)));
	    TEMP.tree_size = ROUND_UP_COUNT(TEMP.tree_size, 1024);
	    out_used += TEMP.tree_size * MERGE.merge_cnt;
	}
	else
	{
	    /* calculate memory for the output buffers.  these do not increase
	     * with the number of runs being merged.
	     */
	    out_used = OUT.buf_cnt * OUT.buf_size;

	    /* consume enough records per merge partition so that around
	     * RECS_PER_RUN_TARGET records are consumed per run (to keep
	     * the merge skip overhead down).
	     */
	    TEMP.merge_ptr_cnt = 
		max(MERGE_PART_RECS_MAX, input_runs * RECS_PER_RUN_TARGET);

	    /* get size of ptr partitions (merge output) and output buffers
	     */
	    temp = TEMP.merge_ptr_cnt * PTR_PART_ENTRY_SIZE;
	    temp = ROUND_UP_NUM(temp, sizeof(int *));

	    TEMP.tree_size = input_runs *
			     (int) (sizeof(node_t) + sizeof(leaf_eov_skip_t));
	    temp = ROUND_UP_NUM(temp + TEMP.tree_size, 1024);
	    out_used += temp * MERGE.merge_cnt;
	}

	if (TEMP.optimal_reads)
	{
	    TEMP.adv_tree = (node_t *)out_used;
	    out_used += input_runs * sizeof(node_t);
	    out_used += input_runs * TEMP.advance * TEMP.adv_info_size;
	}

	out_used = ROUND_UP_COUNT(out_used + sizeof(tbuf_t), TEMP.dio_mem);
	out_used = out_used - sizeof(tbuf_t);
	TEMP.tbuf = (tbuf_t *)out_used;
	out_used += input_runs * 2 * TEMP.tbuf_size;
	out_used = ROUND_UP_COUNT(out_used, SYS.page_size);
	
	if (Print_task)
	    fprintf(stderr, "%10lld %10lld %10lld %5d %10lld %10lld %8d %7d\n",
			    low_data, run_data, high_data,
			    input_runs, in_used, out_used,
			    part_recs, refuge_lls);

	if (sort->internal)
	{
	    if (input_runs <= N_IN_RUNS && in_used <= mem_avail)
		break;
	    runtime_error(sort, NSORT_MEMORY_TOO_SMALL,
		  (u4) ROUND_UP_DIV(max(in_used, out_used), 1024*1024));
	}
	/* If we don't know the actual input size
	 */
	else if (IN.input_size == MAX_I8)
	{
	    /* If we have found a run_data size that will fit in the
	     * available memory, and we know the maximum number of runs
	     * allowed, we're done iterating. 
	     */
	    if (in_used <= mem_avail)
	    {
		if (run_data == high_data - run_data_unit && 
		    input_runs == high_runs - 1)
		{
		    break;
		}

		/* remember highest value of run_data where the input phase
		 * fits in available memory.
		 */
		low_data = run_data;

		/* compute a new value of run_data as halfway between low_data
		 * and high_data.  Note that the new run_data value will not
		 * be equal to high_data, but we may get closer to it.
		 */
		run_data = (low_data + high_data) / 2;
		run_data = (run_data / run_data_unit) * run_data_unit;
	    }
	    else
	    {
		/* if the highest run_data value that fits in available memory
		 * (low_data) in fact DOESN'T fit in available memory, then
		 * low_data must be the original too-low-to-be-safe value
		 * that we set before entering the convergence loop.
		 */
		if (run_data == low_data)
		    runtime_error(sort, NSORT_MEMORY_TOO_SMALL,
			  (u4) ROUND_UP_DIV(max(in_used, out_used), 1024*1024));

		/* remember lowest value of run_data where the input phase
		 * does not fit in the available memory.
		 */
		high_data = run_data;

		/* compute a new value of run_data as halfway between low_data
		 * and high_data.  Note the new run_data may be rounded down
		 * to low_data (in which case we will converge on the next
		 * iteration).
		 */
		run_data = (low_data + high_data) / 2;
		run_data = (run_data / run_data_unit) * run_data_unit;
	    }

	    /* If we have found an input_runs that will fit in the
	     * available memory during the output phase
	     */
	    if (out_used <= mem_avail)
	    {
		/* remember highest number of runs that fit in the
		 * available memory during the output phase.
		 */
		low_runs = input_runs;

		/* compute a new value of input_runs as halfway between
		 * low_runs and high_runs.  Note that the new input_runs
		 * value will not be equal to high_runs, but we may get
		 * closer to it.  
		 */
		input_runs = (low_runs + high_runs) / 2;
	    }
	    else
	    {
		/* if the highest number of runs that fits in available
		 * output phase memory (low_runs) in fact DOESN'T fit in
		 * available memory, then low_runs must be the original
		 * too-low-to-be-safe value that we set before entering the
		 * convergence loop. 
		 */
		if (input_runs == low_runs)
		    runtime_error(sort, NSORT_MEMORY_TOO_SMALL,
			  (u4) ROUND_UP_DIV(max(in_used, out_used), 1024*1024));

		/* remember lowest number of runs that DON'T fit in the
		 * available memory during the output phase.
		 */
		high_runs = input_runs;

		/* compute a new value of input_runs as halfway between
		 * low_runs and high_runs.  Note the new input_runs may be
		 * rounded down to low_runs (in which case we will converge
		 * on the next iteration). 
		 */
		input_runs = (low_runs + high_runs) / 2;
	    }
	
	    if (Print_task)
		fprintf(stderr, "low,high_runs %d %d\n", low_runs, high_runs);
	    
	    continue;
	}

	/* if we have a one-pass sort that will fit in available memory
	 */
	if (input_runs <= N_IN_RUNS && in_used <= mem_avail)
	    break;

	/* if the value of run_data that we used allows the input phase
	 * memory size to be >= the output phase memory size.
	 */
	if (in_used >= out_used && input_runs <= MAX_MERGE_WIDTH)
	{
	    /* if run_data was equal to one io unit more than the highest
	     * run_data found so far that doesn't allow the input phase
	     * memory size to be >= the output phase memory size.
	     */
	    if (run_data == low_data + run_data_unit)
	    {
		/* if we don't have enough memory
		 */
		if (in_used >= mem_avail)
		    runtime_error(sort, NSORT_MEMORY_TOO_SMALL,
			  (u4) ROUND_UP_DIV(max(in_used, out_used), 1024*1024));
		break; /* we found the minimum and it fits in available memory*/
	    }
	    
	    high_data = run_data;   /* new lowest run_data size that's o.k. */
	}
	else
	{
	    /* Remember run_data size as highest value that did NOT allow
	     * the input phase memory size to be >= the output phase memory.
	     */
	    low_data = run_data;
	}

	/* compute a new value of run_data as halfway between low_data and
	 * high_data.
	 */
	run_data = (low_data + high_data) / 2;
	run_data = ROUND_UP_COUNT(run_data, run_data_unit);

	/* calculate the number of input runs.  This may not equal N_IN_RUNS
	 * because of fragmentation among multiple input files.
	 */
	input_size = IN.input_size;
	if (sort->api_invoked)
	{
	    input_runs = ROUND_UP_DIV(input_size, run_data);
	}
	else
	{
	    input_runs = 0;
	    for (i = 0; i < IN.n_files; i++)
	    {
		if (SIZE_VALID(IN.files[i].stbuf.st_mode))
		{
		    file_size = IN.files[i].stbuf.st_size;
		    if (file_size > input_size)
			file_size = input_size;
		    input_runs += ROUND_UP_DIV(file_size, run_data);
		    input_size -= file_size;
		    if (input_size <= 0)
			break;
		}
		else
		{
		    /* assume the worst case of one record input size.
		     */
		    input_runs++;
		}
	    }
	    
	    /* if there are bytes leftover (because there are some
	     * files with unknown sizes)
	     */
	    if (input_size)
	    {
		/* round up remaining input_size, but subtract one run
		 * because in the worst case this remainder data belongs
		 * to the last input file with an unknown size, and we
		 * already added 1 to input_runs for that file.
		 * XXX what happens if nsort is piped 100MB? does it die?
		 */
		input_runs += ROUND_UP_DIV(input_size, run_data) - 1;
	    }
	}
    }

    TEMP.run_limit = input_runs;
    TEMP.tbuf_cnt = (unsigned) (sort->end_out_buf - (byte *)TEMP.tbuf) /
			       TEMP.tbuf_size;
    ASSERT(input_runs <= 2 || TEMP.tbuf_cnt > input_runs);

    sort->max_n_parts = IN_RUN_PARTS;

    /* if we know the actual size of the data, and the number of input_runs
     * is greater than the number of runs that fit in memory, this will be
     * a two pass sort.
     */
    if (IN.input_size != MAX_I8 && input_runs > N_IN_RUNS)
    {
	sort->two_pass = TRUE;
	verify_temp_file(sort);
    }

    if (Print_task > TRUE)
    {
	fprintf(stderr, "part_recs = %d\n", part_recs);
	fprintf(stderr, "refuge_lls = %d\n", refuge_lls);
    }

    if (RECORD.flags & RECORD_FIXED)
    {
#if 0
	if (part_recs * RECORD.length < 192*1024)
	    part_recs = (192 * 1024.0) / RECORD.length;
#endif
	sort->part_data_size = part_recs * RECORD.length;
    }
    else
    {
	sort->part_data_size = run_data / IN_RUN_PARTS;

	/* A partition must be large enough to contain at least one record.
	 */
	if (sort->part_data_size < 2 * MAXRECORDSIZE)
	    sort->part_data_size = 2 * MAXRECORDSIZE;
    }
    sort->in_run_data = run_data;

    part_recs = ROUND_UP_COUNT(part_recs, sort->ll_items);
    sort->n_recs_per_part = part_recs;

    /*
    ** Reserve an extra partition - it is usually needed to detect
    ** that the end of the input data set has been reached.
    ** This extra partition does not contain data which is sorted.
    */
    for (run = sort->in_run; run < sort->in_run + N_IN_RUNS; run++)
    {
	part_t *part;

	run->n_parts = 0;
	part = (part_t *) calloc(IN_RUN_PARTS + 1, sizeof(part_t));
	run->partitions = part;
	if (RECORD.flags & RECORD_FIXED)
	{
	    for (i = 0; i < IN_RUN_PARTS; i++, part++)
	    {
		part->length_ll = (item_line_t *) 1;	/* cause fault on use */
		part->max_length = RECORD.length;
		part->avg_length = RECORD.length;
	    }
	}
	run->part_lists = (item_line_t **) 
				calloc(IN_RUN_PARTS + 1, sizeof(item_line_t *));
    }
    MERGE.run_list = (item_line_t **) 
	calloc(max(IN_RUN_PARTS * N_IN_RUNS, TEMP.run_limit),
	       sizeof(item_line_t *));

    TEMP.run = (tmp_run_t *)calloc(TEMP.run_limit, sizeof(tmp_run_t));

    if (Print_task > TRUE)
    {
	fprintf(stderr, "n_recs_per_part = %lld\n", sort->n_recs_per_part);
	fprintf(stderr, "n_refuge_lls = %d\n", sort->n_refuge_lls);
    }
}

/*
 * prepare_sort_def - after all sort def statements have been read, apply
 *		      defaults to unspecified items to prepare for sorting.
 *
 *	Returns:
 *		TRUE if the sort definition has some problem which
 *		     should prevent its operation
 *		FALSE otherwise (the sort may proceed)
 */
int prepare_sort_def(sort_t *sort)
{
    keydesc_t	*key;
    field_t	*field;
    
    PARSE.lineno = 0;	/* turn off line numbering in error messages */

    if (sort->n_errors != 0)
	return (TRUE);

    /* If no input file has been specified, use stdin
     */
    if (!sort->api_invoked && IN.n_files == 0)
	another_infile(sort, "-");
	
    guess_input_count(sort);

    if (sort->n_sprocs == 0)	/* if number of sprocs not specified */
    {
	if (IN.input_size < 1000000)	/* for tiny inputs, use one sproc */
	    sort->n_sprocs = 1;
	else
	    sort->n_sprocs = SYS.n_processors;
    }	
    if (sort->n_sprocs == 1 && sort->touch_memory == FlagUnspecified)
	sort->touch_memory = FlagFalse;

    /* If the number of partitions hasn't been set already, try to give
     * each sort sproc 8 partitions to divvy up the work nicely.  There is
     * a tradeoff between limiting sort partition sizes to fit in the 4MB
     * typical secondary cache, and limiting the merge width to the number
     * of tlb entries available (e.g. ~30 for r4K, ~60 for r10k)
     */
    if (IN_RUN_PARTS == 0)
	IN_RUN_PARTS = min((sort->n_sprocs - 1) * 4 + 8, 128);

    /*
    ** No "-format=..." statement?  Default to lines of text
    */
    if ((RECORD.flags & RECORD_TYPE) == 0)
    {
	RECORD.flags |= RECORD_STREAM;
	RECORD.delimiter = '\n';
	RECORD.minlength = 1;	/* minimum, just for the newline */
    }
    if (sort->keycount == 0 && !sort->check)
    {
	sort->keys = (keydesc_t *) calloc(1, sizeof(keydesc_t));
	sort->keycount = 1;
	sort->keys->pad = ' ';
	sort->keys->name = "<entire line>";

	switch (RECORD.flags & RECORD_TYPE)
	{
	  case RECORD_STREAM:
	    sort->keys->type = typeDelimString;
	    sort->keys->flags = FIELD_DELIMSTRING | FIELD_VARIABLE;
	    sort->keys->delimiter = RECORD.delimiter;
	    break;
	  
	  case RECORD_FIXED:
	    sort->keys->type = typeFixedString;
	    sort->keys->length = RECORD.length;
	    break;
	  
	  case RECORD_VARIABLE:
	    if (!sort->check)
		runtime_error(sort, NSORT_VARIABLE_NEEDS_KEY);
	    break;

	  default:
	    die("unknown record type %x", RECORD.flags);
	    break;
	}
    }

    if (sort->n_errors > 0)
	return (TRUE);

    if (RECORD.flags & RECORD_VARIABLE)
	RECORD.var_hdr_extra = 2;

    /*
    ** Put ``end of list'' markers at the end of the lists
    ** of the keys and summary fields
    */
    key = (keydesc_t *) realloc(sort->keys,
				(sort->keycount + 1) * sizeof(keydesc_t));
    sort->keys = key;
    key += sort->keycount;
    key->type = typeEOF;
    key->flags = 0;
    key->length = 0;
    key->position = 0;

    field = (field_t *) realloc(RECORD.fields,
				(RECORD.fieldcount + 1) * sizeof(field_t));
    RECORD.fields = field;
    field += RECORD.fieldcount;
    field->type = typeEOF;
    field->flags = 0;
    field->length = 0;
    field->position = 0;

    if (sort->n_summarized != 0)
    {
	sort->summarize = (short *) realloc(sort->summarize,
					    (sort->n_summarized + 1) *
					     sizeof(*sort->summarize));
	sort->summarize[sort->n_summarized] = -1;
    }

    prepare_fields(sort);
    prepare_edits(sort);

    /* if a record limit has been specified, potentially lower IN.input_size
     */
    if (sort->n_recs != 0)
    {
	i8	input_limit;
	
	input_limit = sort->n_recs * (RECORD.maxlength + RECORD.var_hdr_extra);

	if (input_limit < IN.input_size)
	    IN.input_size = input_limit;
    }

#if defined(CHECK)
    /*
     * For the check program (not nsort): say the edits have 'already happened';
     * this permits nearly the same command line for check as for nsort
     */
    if (sort->n_edits != 0)
	RECORD.minlength = RECORD.length = sort->edited_size;
#endif

    if (choose_sort_method(sort))
	return (TRUE);

    /* fix names, modes, transfer sizes, counts for all sort files.
     */
    fix_sort_files(sort);

    prepare_temp(sort);		/* after choose_sort_method to know item_size */

    prepare_keys(sort);

    optimize_keys(sort);

    /*
    ** Include the terminating record delimiter (and the length overhead?)
    ** in the minimum record length. Helps to avoid divide by zero.
    */
    ASSERT(RECORD.minlength != 0);
    ASSERT(RECORD.maxlength != 0);

#if !defined(CHECK)
    IN.part_final = MAX_U4;
    if (sort->method == SortMethRecord)
    {
	unsigned	in_size, out_size;

	/* Determine padding at beginning and ending of record output buffers
	 * by rounding up twice the maximum record size to a dio_io boundry.
	 * Use two times the maximum record size to allow the tail record
	 * and a remainder record to fit in the padding.
	 */
	if (sort->api_invoked)
	    OUT.rec_buf_pad = LL_BYTES;
	else
	    OUT.rec_buf_pad = (unsigned)ROUND_UP_NUM(2 * LL_BYTES, OUT.dio_io);

	/* set the number of merges during the input phase and also output
	 * phase merge output buffers to 3/2 the number of sorting sprocs
	 * plus the maximum number of concurrent aio's.
	 */
	MERGE.merge_cnt = (3 * sort->n_sprocs + 1) / 2 + OUT.aio_cnt;

	/* set the maximum number of records per temp file block.
	 * With optimal reads, leave room in block for advance record.
	 */
	TEMP.blk_recs = (int)
	    (TEMP.blk_size - sizeof(blockheader_t)
	     - (TEMP.optimal_reads ? sort->edited_size:0)) / sort->edited_size;

	/* set the nubmer of temp file blocks to be produced by each merge
	 * partition during the input phase.
	 */
	TEMP.merge_part_blks = ROUND_UP_DIV(IN_RUN_PARTS * 400, TEMP.blk_recs);

	/* allocate enough blocks for temp file writes to allow each of the
	 * sort sprocs to be performing a merge (TEMP.merge_part_blks each),
	 * enough blocks to hold advance information, and one block for each
	 * possible outstanding temp file write.  This is probably overkill
	 * for all but writing the last run to memory, since in all other
	 * cases most of the sort sprocs should by busy sorting input
	 * partitions, not merging.  
	 */
	TEMP.out_blk_cnt = MERGE.merge_cnt * TEMP.merge_part_blks + 
	    2 * TEMP.advance + TEMP.n_files * TEMP.aio_cnt;

	in_size = TEMP.blk_size * TEMP.out_blk_cnt;
	out_size = MERGE.merge_cnt * (OUT.buf_size + 2 * OUT.rec_buf_pad);
	MERGE.rec_buf_size = max(in_size, out_size);

	alloc_out_lists(sort);	/* allocate rec_out_buf struct for each merge */
    }
    else
    {
	/* set the number of ptr partitions (output of a merge partition)
	 * to 1 plus 3/4 the number of sorting sprocs (presumably most of
	 * the cpu work will be done in copying records, not merging).  
	 */
	MERGE.merge_cnt = 1 + ((3 * sort->n_sprocs + 1) / 4);

	/* set the number of output buffers to the maximum concurrent
	 * writes (aio_cnt) times 2 (double buffering).
	 */
	OUT.buf_cnt = OUT.aio_cnt * 2;

	/* allocate enough temp file write blocks so that we can have as
	 * many blocks being filled by copying, as there are blocks being
	 * written (TEMP.n_files * TEMP.aio_cnt) or waiting for advance
	 * information (TEMP.advance).
	 */
	TEMP.out_blk_cnt = 2 * (TEMP.advance + TEMP.n_files * TEMP.aio_cnt);

	alloc_ptr_out_lists(sort); /* alloc ptr_out_buf struct for each merge */
    }

    TEMP.out_blk = (out_blk_t *)malloc(TEMP.out_blk_cnt * sizeof(out_blk_t));

#endif	/* !CHECK program */

    if (sort->prehashing == SortPreHashRequired && sort->n_summarized == 0 &&
	sort->delete_dups == FALSE)
    {
	runtime_error(sort, NSORT_PREHASHING_INAPPLICABLE);
	return (TRUE);
    }

    sort->any_deletes = sort->delete_dups || sort->n_summarized;
    if (sort->n_summarized)
    {
	/* Several places in the code check for delete_dups before summarizing
	 * the to-be-deleted record into the surviving one - keep del_dups off
	 */
	sort->delete_dups = FALSE;

	/* don't verify that recode is doing the right thing -
	 * it's too much trouble to avoid doubling the summarized fields.
	 */
	sort->test = FALSE;
    }

#if !defined(CHECK)
    /* calculate memory usages and determine number of passes.
     */
    calc_mem_use(sort);

    /* Now we've entirely decided on the strategy to use for this sort
     * Remember the calculations' results that we'll need during the sort.
     */
    fill_sort_funcs(sort);

    if (!sort->api_invoked)
    {
	/* if (we are using just one sort process AND
	 *     this is just a one-pass sort AND
	 *     all input files are either mapped or serial AND
	 *     (the output file mode is serial or buffered))
	 */
	if (sort->n_sprocs == 1 &&
	    !sort->two_pass &&
	    IN.single_ok &&
	    (OUT.file->io_mode == IOMODE_SERIAL ||
	     OUT.file->io_mode == IOMODE_BUFFERED))
	{
	    /* use single process mode
	     */
	    sort->single_process = TRUE;
	}
    }    
#endif	/* !CHECK program */

    if (sort->n_recs != 0)
	sort->recs_left = sort->n_recs;

    compile_ovc_init(sort);

    if (sort->two_pass && TEMP.n_files == 0)
    {
	add_temp_file(sort, "/tmp");
	TEMP.files[0].aiocb = aiocb_alloc(TEMP.aio_cnt);
	obtain_aio_fds(sort, TEMP.files[0].aiocb, TEMP.aio_cnt, 
		       &TEMP.files[0].fileinfo, NSORT_OUTPUT_OPEN);
    }

    if (sort->print_definition)
	nsort_print_sort(sort, stderr);

    if (sort->n_errors > 0)
	return (TRUE);

    return (FALSE);
}

#if !defined(CHECK)
/*
 * prepare_part - determine interesting things about a partition
 *		 after it has been read into memory.
 *
 *	Parameters:
 *		sort
 *		run
 *		part
 *
 *	Returns:
 *		TRUE if this partition is empty -> all input has been taken
 *		FALSE if this partition has something to sort
 *
 *	Tasks include:
 *		- set part_end to point to the byte just past the last
 *		  record which will be handled by this partition.
 *		  Even for fixed length records the end may be incorrect;
 *		  e.g. we don't know how big a pipe is.
 *		- find the lengths of non-fixed-length records and save them
 *		  in the refuge for later use by init_item_vs.
 *
 *	The number of data linelists varies from partition to partition,
 *	depending of the alignments of rec_start and rec_end relative
 *	to LL_BYTES.  For fixed length records most partitions will get
 *		((n_recs_per_part * record_length) / LL_BYTES) - 1
 *	linelists from the 'local'/'data' area.  Occasionally there will
 *	be two lost to alignment for this partition, but the next one
 *	will probably not loose any.
 *
 *	The lengths are saved at the end of the refuge. refuge_end is
 *	aligned downwards so that the lengths won't be overwritten.
 *
 *	Fields which are set in the partition descriptor:
 *		rec_end
 *		refuge_start
 *		n_recs
 *	The following fields are set for variable length partitions:
 *		refuge_end
 *		length_ll
 *		local_edit
 *		
 *   Only one process may be in this function at a time,
 * so commpare-and-swap and locks are not needed
 */
unsigned prepare_part(sort_t *sort, in_run_t *run, part_t *part)
{
    byte	*start;
    int		n_recs;
    int		max_n_recs;
    unsigned	max_length;
    unsigned	length;
    byte	*end;
    byte	*rec;
    u2		*refuge_lengths;		
    ptrdiff_t	n_local_lls;
    ptrdiff_t	n_refuge_lls;
    char	delimiter;
    byte	*near_end;
    byte	*eorec;
    int		var_hdr_extra;	/* 0 if by stream, 2 if by prefix-length */

    STATS.serial_scan -= get_cpu_time();
    start = run->rec_used;
    ASSERT(start <= run->read_target);
    part->rec_start = start;
    end = start + sort->part_data_size;

    part->refuge_end = run->refuge_next;

    if (RECORD.flags & RECORD_FIXED)
    {
	length = RECORD.length;
	if (end > run->read_target)
	{
	    /* round down 'end' to a multiple of the input record size
	     */
	    n_recs = (int) ((run->read_target - start) / length);
	    end = start + n_recs * length;
	}
	else
	    n_recs = sort->n_recs_per_part;

	/* wait for input to either reach end-of-file or fill partition
	 */
	while (!run->eof && run->read_end < end)
	{
	    if (sort->single_process)
		chk_input(sort);
	    else
	    {
		IN.curr_file->stats.wait -= get_time();
		ordnap(1, "fixlen inwait");
		IN.curr_file->stats.wait += get_time();
	    }
	}
#if 0
	fprintf(stderr, "run->eof %d, run->read_end %08x, end %08x\n",
		run->eof, run->read_end, end);
#endif

	/* If we hit eof early then adjust n_recs down and check for
	 * a partial record at the end
	 */
	if (run->read_end < end)
	{
	    ASSERT(run->eof);
	    end = run->read_end;
	    n_recs = (int) ((end - start) / length);
	    if (start + n_recs * length != end)
	    {
		end = start + n_recs * length;
		runtime_error(sort, NSORT_PARTIAL_RECORD,
				    IN.files[run->file_num].name);
	    }
	}

	/*
	 * If this sort is limited by a record count, check whether
	 * this partition contains the last record we'll be sorting
	 * When a sort prepares the last count-limited partition,
	 * it reduces read_target to tell chk_input() to whoa, there;
	 * all the necessary data has been read.
	 */
	if (sort->n_recs != 0)
	{
	    if (n_recs > sort->recs_left)
	    {
		n_recs = sort->recs_left;
		end = start + n_recs * length;
	    }
	    sort->recs_left -= n_recs;
	    if (sort->recs_left == 0 && !sort->internal)
	    {
		int	till_io_end;

		/* Tell the i/o sproc not to initiate any more reads. In order
		 * not to confuse later users of read_target (are there any?)
		 * we adjust read_target to end as soon as possible after the
		 * end of the data.
		 */
		till_io_end = (int) (IN.dio_io -
				     ((end - run->read_start) % IN.dio_io));
		if (till_io_end == IN.dio_io)
		    till_io_end = 0;
		ASSERT(run->read_target >= end + till_io_end);
		run->read_target = end + till_io_end;
		run->final_run = TRUE;
	    }
	}

	max_length = length;
    }
    else	/* variable length records -- scan through to find lengths */
    {
	int	n_lines;

	refuge_lengths = (u2 *) part->refuge_end;
	part->lengths = refuge_lengths;

	/*
	 * Limit the records taken by this partition to the least of:
	 * -	a 1/IN_RUN_PARTS portion of this run's data (part_data_size)
	 * -	recs_left, if a maximum record count was specified
	 * -	the number of refuge lines allocatable before hitting target
	 *	(this calculation is inexact, but errs of the side of safety.
	 *	 the exact calculation is iterative and rarely significant.
	 *	 it starts by assuming all the lines could have items, and
	 *	 then subtract the number of length lines which would be needed
	 *	 to hold that many records' lengths. this reduced number of
	 *	 lines * LL_ITEMS is a conservative limit of max_n_recs.
	 */
	n_lines = (int) (part->refuge_end - (item_line_t *) run->read_target);
	n_lines -= VAR_LENGTH_LLS(n_lines * LL_ITEMS) + MAX_GEN_N_LINES;
	if (n_lines <= 0)
	    max_n_recs = 0;
	else
	{
	    max_n_recs = n_lines * LL_ITEMS;
	    if (sort->n_recs != 0 && max_n_recs > sort->recs_left)
		max_n_recs = sort->recs_left;
	}

	max_length = 0;
	delimiter = RECORD.delimiter;
	/*
	 * Keep the scan inside the data bytes
	 *	- filled in by read_input(), and
	 *	- interior to this sorting partition
	 * When the input dataset size is unknown and read_input() hits EOF
	 * it reduces rec_end to mark the end of valid data, so that 
	 * the sorting sprocs don't go too far.
	 * We must not look at the data at or beyond read_end - those bytes
	 * are not yet valid.  When the scan gets close to the end (within
	 * the maximum record size) it starts 'tiptoeing' carefully,
	 * to look at only valid data.
	 */
	if (end >= run->read_target)
	    end = run->read_target;
	near_end = /*min(end, */  run->read_end /* ) */ - MAXRECORDSIZE;
	var_hdr_extra = RECORD.var_hdr_extra;

	n_recs = 0;
	rec = start;
	for (; n_recs < max_n_recs; n_recs++)
	{
	    /* Handle the special cases which occur when rec is close to
	     * to the end of this sort partiton/the valid (read-in) data.
	     * Stop if we've reached the end of this sort partition
	     */
	    if (rec >= end)
		break;

	    if (rec >= near_end)
	    {
		char	timestr[30];
		int looped = FALSE;

		/* Here we know that rec is close to the end of the last
		 * known 'good' portion of this run's input data. Nap until
		 * - the next block of data has been read, or
		 * - the end of the run.
		 */
		for (;;)
		{
		    if (run->eof && end > run->read_end)
			end = run->read_end;
		    near_end = /*min(end, */ run->read_end /*)*/ - MAXRECORDSIZE;
		    if (rec < near_end || run->read_end == run->read_target ||
			run->eof)
		    {
			if (Print_task && looped)
			    fprintf(stderr,
				    "prepare_part: break rec %x near %x end %x "
				    "read_end %x targ %x @ %s\n",
				    rec, near_end, end,
				    run->read_end, run->read_target,
				    get_time_str(sort, timestr));
			break;
		    }

		    if (sort->single_process)
			chk_input(sort);
		    else
		    {
			/* XXX Instead of napping here, we could break out and
			 * sort however many records have already been read
			 */
			IN.curr_file->stats.wait -= get_time();
#if 0
			if (Print_task)
			    fprintf(stderr, "prepare_part: recs %d/%d, rec %x near %x read_end %x targ %x nap@ %s\n",
				    n_recs, max_n_recs,
				    rec, near_end,
				    run->read_end, run->read_target,
				    get_time_str(sort, timestr));
#endif
			looped = TRUE;
			ordnap(1, "varlen inwait");
			IN.curr_file->stats.wait += get_time();
		    }
		}

		if (rec > near_end)
		{
		    ASSERT(run->read_end == run->read_target || run->eof);
		    /* Here we're in the last MAXRECORDSIZE bytes
		     * of the run (not merely the sort partition).  
		     * If we knew that the smallest ante-ultimate
		     * sorting partition would always be larger than
		     * MAXRECORDSIZE, we could probably punt this data
		     * to the next run. But, that currently isn't required.
		     */
		    if (RECORD.flags & RECORD_STREAM)
		    {
			eorec = fmemchr(rec + RECORD.minlength - 1,
				        delimiter,
					run->read_end - rec);
			if (eorec == NULL)
			{
			    if (run->eof && (n_recs > 0 || part == run->partitions))
			    {
				runtime_error(sort, NSORT_PARTIAL_RECORD,
					      IN.files[run->file_num].name);
				run->scan_complete = TRUE;
			    }
			    break;		/* end of this partition */
			}
			goto did_memchr;
		    }
		    else if (run->read_end - rec < sizeof(u2))
		    {
			if (run->eof && (n_recs > 0 || part == run->partitions))
			    runtime_error(sort, NSORT_PARTIAL_RECORD,
					  IN.files[run->file_num].name);
			break;			/* end of this partition */
		    }
		    else
			goto get_reclen;
		}
	    }

	    /*
	     * Stream   : search for record delimiter, up to 64K or read_end
	     * Variable : fetch the halfword length preceding record data
	     */
	    if (RECORD.flags & RECORD_STREAM)
	    {
		eorec = fmemchr(rec + RECORD.minlength - 1,
				delimiter,
				MAXRECORDSIZE);
		if (eorec == NULL)
		{
		    runtime_error(sort, NSORT_DELIM_MISSING,
				  IN.begin_run_offset + rec - run->read_start,
				  IN.files[run->file_num].name);
		    break;		/* end of this partition */
		}
did_memchr:
		length = 1 + (unsigned) (eorec - rec);
	    }
	    else
	    {
get_reclen:
		length = 2 + ulh(rec);
		if ((rec + length) > run->read_end)
		{
		    if (run->eof && n_recs > 0)
		    {
			runtime_error(sort, NSORT_PARTIAL_RECORD,
				      IN.files[run->file_num].name);
			run->scan_complete = TRUE;
		    }
		    break;
		}
	    }
	    if (max_length <= length)
		max_length = length;
	    if (length < RECORD.minlength)
	    {
		runtime_error(sort, NSORT_RECORD_TOO_SHORT,
			      IN.begin_run_offset + rec - run->read_start,
			      IN.files[run->file_num].name,
			      RECORD.minlength);
		break;
	    }
	    else if (length > RECORD.maxlength)
	    {
		runtime_error(sort, NSORT_RECORD_TOO_LONG,
			      IN.begin_run_offset + rec - run->read_start,
			      IN.files[run->file_num].name,
			      RECORD.maxlength);
		break;
	    }
	    *--refuge_lengths = length - var_hdr_extra;
	    rec += length;
	}
	ASSERT(rec >= end || run->read_end == run->read_target || rec == run->read_end || n_recs == max_n_recs || run->eof);

#if 0		/* Why back up? */
	/*
	** It is most likely that the initial guess as to the end of the
	** partition will not be exactly at a record boundary.
	** If we went too far back up one record to get to
	** the last record totally in our partition.
	*/
	if (rec > end)
	{
	    rec -= length;
	    refuge_lengths++;
	    n_recs--;
	}
#endif
	end = rec;

	part->length_ll = part->refuge_end;
	part->refuge_end = (item_line_t *) ROUND_DOWN(refuge_lengths, LL_BYTES);
	part->max_length = max_length - var_hdr_extra;
	if (n_recs == 0)
	{
	    part->avg_length = 0;
	}
	else
	{
	    /* there's at least one record (and one length); set the
	     * pointer which, when crossed by a descending lengths pointer,
	     * means that a length line may be reused for items
	     */
	    part->length_ll--;
	    part->avg_length = (u2) (end - start) / n_recs;
	}

	/*
	 * If this sort is limited by a record count, check whether
	 * this partition contains the last record we'll be sorting
	 * When a variable length sort prepares the last count-limited
	 * partition, it reduces read_target to tell the other sorting sprocs
	 * that there is  no more data to sort	XXX ?? true?? it was rec_end
	 * and sets final_run so that the i/o sproc stops reading this file.
	 */
	if (sort->n_recs != 0 && (sort->recs_left -= n_recs) == 0)
	{
	    int	till_io_end;

	    /* Tell the i/o sproc not to initiate any more reads. In order
	     * not to confuse later users of read_target (are there any?)
	     * we adjust read_target to end as soon as possible after the
	     * end of the data.
	     */
	    till_io_end = (int) (IN.dio_io -
				 ((end - run->read_start) % IN.dio_io));
	    if (till_io_end == IN.dio_io)
		till_io_end = 0;
	    ASSERT(run->read_target >= end + till_io_end);
	    run->read_target = end + till_io_end;
	    run->final_run = TRUE;
	}
    }

    if (n_recs == 0)
	n_refuge_lls = 0;
    else
	n_refuge_lls = ROUND_UP_DIV(n_recs, sort->ll_items) + MAX_GEN_N_LINES;
    if (sort->method == SortMethRecord)
    {
	if (sort->internal)
	{
	    /* Internal sorts put the records back into the input array, so
	     * they cannot reuse the input space as recycled or local linelists.
	     * Set local_ll to a high value to prevent recycling in gen_list().
	     */
	    part->local_ll = (item_line_t *) ROUND_UP(run->read_target, LL_BYTES);
	}
	else
	{
	    part->local_ll = (item_line_t *) ROUND_UP(start + LL_BYTES, LL_BYTES);
	    n_local_lls = (item_line_t *) ROUND_DOWN(end, LL_BYTES) - part->local_ll;
	    if (n_local_lls > 0) /* last part may be too small for any lls */
		n_refuge_lls -= n_local_lls;
	}

	/* the first line must come from the refuge, even if there would be
	 * enough space in the local region for all the partition's items.
	 * The first records of an editing sort's partitions always go
	 * into the refuge - the items would need to be in the same places
	 * as the records are, at the beginning of the line.
	 */
	if (n_refuge_lls == 0)
	    n_refuge_lls = 1;
    }
    else
    {
	/*
	 * A pointer sort needs refuge space for:
	 * - the ovc (ovc/eov/record pointer) items. The space needs have
	 *	already been included in n_refuge_lls.
	 * - (record editing only)	space for some of the reformed records 
	 * - (variable only)		the two-byte lengths, one per record
	 *    If there are enough linelists worth of record lengths, then
	 * some of them are re-used for later items, after their lengths
	 * have been put into already-built items.
	 * The items are first (lower) in memory, optionally followed by
	 * at most one of the refuge record editing or record length `lls'.
	 */
	if (!(RECORD.flags & RECORD_FIXED))
	    n_refuge_lls += /*VAR_LENGTH_LLS(n_recs)*/ (n_recs == 0 ? 0 : 1) -
			    (part->length_ll - part->refuge_end);
	if (sort->edit_growth > 0)
	{
	    part->refuge_edit = (byte *) part->refuge_end;
	    part->refuge_end -= EDIT_REFUGE_LLS(n_recs, sort->edit_growth, max_length + sort->edit_growth);
	    part->local_edit = start;
	}
    }

    part->rec_end = end;
    part->n_recs = n_recs;
    part->refuge_next = part->refuge_end - 1;
    part->refuge_start = part->refuge_end - n_refuge_lls;
    ASSERT(part->refuge_start >= run->refuge_start);

    if (end != start)
	run->n_parts++;

    if (Print_task > 1 && n_recs > 0)
	fprintf(stderr, "run part %d, start %08x, end %08x, n_recs %d\n",
		part - run->partitions, start, end, n_recs);

    run->refuge_next = part->refuge_start;
    run->rec_used = end;

    /* if last partition in a run, tell the i/o sproc that the sorters
     * have decided on this run's final rec_used point.
     * Adjust the target_size for the next run, if this involves varlen records.
     */
    if (part == &run->partitions[IN_RUN_PARTS - 1])
    {
	int unused_lines = part->refuge_end - (item_line_t *) run->read_target;

	if (run->scan_complete)
	    run->rec_used = run->read_end;
	else
	    STATS.unused_item_lines += unused_lines;

	run->scan_complete = TRUE;
    }
    
    STATS.serial_scan += get_cpu_time();

    /* let another sproc scan next partition
     */
    IN.part_scanned++;

    return (end == start);
}

/*
 * prepare_part_ptr -	determine interesting things about a partition
 *		 	in an api internal pointer sort
 *
 *	Parameters:
 *		sort
 *		run
 *		part
 *
 *	Returns:
 *		TRUE if this partition is empty -> all input has been taken
 *		FALSE if this partition has something to sort
 *
 *	Tasks include:
 *		- set part_end to point to the POINTER just past the last
 *		  record which will be handled by this partition.
 *		  Even for fixed length records the end may be incorrect;
 *		  e.g. we don't know how big a pipe is.
 *
 *	Fields which are set in the partition descriptor:
 *		rec_end		(byte **) points to the pointer of the 
 *		refuge_start
 *		n_recs
 *	The following fields are set for variable length partitions:
 *		refuge_end
 *		length_ll
 *		local_edit
 *		
 *   Only one process may be in this function at a time,
 * so commpare-and-swap and locks are not needed
 */
unsigned prepare_part_ptr(sort_t *sort, in_run_t *run, part_t *part)
{
    byte	**start;
    byte	**end;
    int		n_recs;
    ptrdiff_t	n_refuge_lls;

    ASSERT(IN.ptr_input && sort->method == SortMethPointer);
    STATS.serial_scan -= get_cpu_time();
    start = (byte **) run->rec_used;
    part->rec_start = (byte *) start;
    n_recs = sort->n_recs_per_part;
    if (sort->n_recs != 0)
    {
	if (n_recs > sort->recs_left)
	    n_recs = sort->recs_left;
	sort->recs_left -= n_recs;
    }
    end = start + n_recs;

    part->refuge_end = run->refuge_next;

    if (RECORD.flags & RECORD_FIXED)
    {
	part->max_length = RECORD.length;	/* XXX anybody using these? */
	part->avg_length = RECORD.length;
    }

    if (n_recs == 0)
	n_refuge_lls = 0;
    else
    {
	n_refuge_lls = ROUND_UP_DIV(n_recs, sort->ll_items) + MAX_GEN_N_LINES;
	run->n_parts++;
    }

    part->rec_end = (byte *) end;
    part->n_recs = n_recs;
    part->refuge_next = part->refuge_end - 1;
    part->refuge_start = part->refuge_end - n_refuge_lls;
    ASSERT(part->refuge_start >= run->refuge_start);

    if (Print_task > 1 && n_recs > 0)
	fprintf(stderr, "run part %d, start %08x, end %08x, n_recs %d\n",
		part - run->partitions, start, end, n_recs);

    run->refuge_next = part->refuge_start;
    run->rec_used = (byte *) end;

    /* if last partition in a run, tell the i/o sproc that the sorters
     * have decided on this run's final rec_used point.
     * Adjust the target_size for the next run, if this involves varlen records.
     */
    if (part == &run->partitions[IN_RUN_PARTS - 1])
    {
	ptrdiff_t unused_lines;
	
	unused_lines = part->refuge_end - (item_line_t *) run->read_target;

	if (run->scan_complete)
	    run->rec_used = run->read_end;
	else
	    STATS.unused_item_lines += unused_lines;

	run->scan_complete = TRUE;
    }
    
    STATS.serial_scan += get_cpu_time();

    /* let another sproc scan next partition
     */
    IN.part_scanned++;

    return (end == start);
}

void prepare_in_run(sort_t *sort, unsigned run_index)
{
    in_run_t		*run;
    in_run_t		*prev_run;
    unsigned		rem_size;

    run = &sort->in_run[run_index % N_IN_RUNS];

    /* if previous run had remainder data
     */
    if (run->first_rec != run->read_start)
    {
	/* copy remainder data to this run's buffer so it ends at
	 * read_start, thereby making it contiguous with whatever data is
	 * read for this run.  
	 */
	prev_run = &sort->in_run[(run_index - 1) % N_IN_RUNS];
	rem_size = (unsigned) (prev_run->read_end - prev_run->rec_used);
	if (Print_task)
	    fprintf(stderr, "run %d carrying over %d bytes from previous run\n",
			    run_index, rem_size);
	memmove(run->first_rec, prev_run->rec_used, rem_size);
    }
    run->rec_used = run->first_rec;

    run->refuge_next = run->refuge_end;
    run->n_parts = 0;
}
#endif

/* map_output_pass - calculate addresses for various data structures
 *			during the output phase of a two pass sort.
 */
void map_output_pass(sort_t *sort)
{
    i8		diff = (i8) sort->begin_in_run;
    int		i;
    byte	*used;
    tbuf_t	*tbuf;
    int		min_recs_per_blk;

    used = sort->begin_in_run;
    if (sort->method == SortMethRecord)
    {
	OUT.buf_size = TEMP.out_buf_size;
	for (i = 0; i < MERGE.merge_cnt; i++)
	{
	    MERGE.rob[i].u.buf = used;
	    used += OUT.buf_size + 2 * OUT.rec_buf_pad;
	}
	for (i = 0; i < MERGE.merge_cnt; i++)
	{
	    MERGE.rob[i].tree = (node_t *)used;
	    used += TEMP.tree_size;
	}
    }
    else
    {
	/* allocate output buffers
	 */
	for (i = 0; i < OUT.buf_cnt; i++)
	{
	    OUT.buf[i] = (byte *)used;
	    used += OUT.buf_size;
	}

	/* set the number of pointers (both record and tbuf) in a merge 
	 * partition output array.
	 */
	MERGE.part_ptr_cnt = TEMP.merge_ptr_cnt;

	/* determine minimum records per block
	 */
	min_recs_per_blk = (int) ((TEMP.blk_size - sizeof(blockheader_t)) / 
				  RECORD.max_intern_bytes);
	if (TEMP.optimal_reads)
	    min_recs_per_blk--;

	/* set target number of records to be consumed by each merge partition.
	 *
	 * subtract one entry for each run being merged.  This is for the
	 * unfortunate case we quickly consume the last record of a tbuf
	 * for each of the runs.  Note this will also cause one record to
	 * be consumed for each run.
	 */
	MERGE.part_recs = MERGE.part_ptr_cnt - MERGE.width;

	/* subtract out the maximum number of tbuf pointers that can occur
	 * in the MERGE.part_recs - MERGE.width pointers left in the array.
	 */
	MERGE.part_recs -= (MERGE.part_recs - MERGE.width) /
			   (min_recs_per_blk + 1);
	/* 
	 * note the last (and partially filled) block at the end of each
	 * run is not a consideration since it will contain the EOL_OVC
	 * item and will never have its tbuf pointer appear in a merge
	 * partition array. */

	/* allocate ptr partitions
	 */
	for (i = 0; i < MERGE.merge_cnt; i++)
	{
	    MERGE.ptr_part[i].ptrs = (byte *)used;
	    used += TEMP.merge_ptr_cnt * PTR_PART_ENTRY_SIZE;
	    used = ROUND_UP(used, sizeof(int *));

	    MERGE.ptr_part[i].tree = (node_t *)used;
	    used += TEMP.tree_size;
	    used = ROUND_UP(used, 1024);
	}
    }
    
    /* if doing optimal reads, allocate advance info data for each run.
     */
    if (TEMP.optimal_reads)
    {
	TEMP.adv_tree = (node_t *)((byte *)TEMP.adv_tree + diff);
	ASSERT(used <= (byte *)TEMP.adv_tree);

	used = (byte *)(TEMP.adv_tree + MERGE.width);
	for (i = 0; i < MERGE.width; i++)
	{
	    TEMP.run[i].adv_info = used;
	    TEMP.adv_tree[i].ovc = EOL_OVC;
	    used += TEMP.advance * TEMP.adv_info_size;
	}

	/* prohibit advance reads until the tree is fully initialized
	 * by build_adv_info().
	 */
	TEMP.adv_tree[0].ovc = EOL_OVC;
    }
    
    /* create an array of tbuf's from the in_run memory.
     * align end of first tbuf on a TEMP.dio_mem boundry so that we
     * read blocks there.
     */
    TEMP.tbuf = (tbuf_t *)((byte *)TEMP.tbuf + diff);
    ASSERT(used <= (byte *)TEMP.tbuf);

    tbuf = TEMP.tbuf;
    tbuf->r_next = NULL;
    tbuf->run = TBUF_RUN_FREELIST;
    tbuf->file = -1;
    tbuf->read_done = FALSE;
    for (i = 1; i < TEMP.tbuf_cnt; i++)
    {
	tbuf_t	*prev_tbuf;

	prev_tbuf = tbuf;
	tbuf = (tbuf_t *)((byte *)tbuf + TEMP.tbuf_size);
	tbuf->r_next = prev_tbuf;
	tbuf->run = TBUF_RUN_FREELIST;
	tbuf->file = -1;
	tbuf->read_done = FALSE;
    }
    TEMP.free = tbuf;
    ASSERT((byte *)tbuf + TEMP.tbuf_size <= sort->end_out_buf);
}
