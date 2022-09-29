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
 ****************************************************************************
 *
 *	$Ordinal-Id:$
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include "merge.h"
#include "nsort.h"
#include "nsorterrno.h"
#include <sys/sysmp.h>

#define runio_curpos	aio_filesize	/* reuse for byte position in file */
#define runio_final	aio_lio_opcode	/* reuse for 'doing last write' flag */

#define api_error	runtime_error

#define UPDATE_RUN	0

int		IOInit;

/* This macro should contain the first executable code of any nsort api
 * function except for nsort_alloc_context()
 */
#define API_SETUP	nsort_msg_t err; \
			if (context >= SYS.n_sorts || context < 0 || \
			    (sort = SYS.sorts[context]) == NULL) \
			    return (NSORT_INVALID_CONTEXT); \
			if (err = setjmp(sort->api_fail)) \
			    return (err)

cleanup_error(sort_t *sort)
{
    api_sort_done(sort);
}

/*
 * nsort_define	- define a sort from a string of nsort.c's command line args
 *
 *	The program name (e.g. "nsort") is NOT included as an arg
 */
nsort_t	nsort_define(const char *str)
{
    nsort_t	ret;
    sort_t	*sort;
    nsort_msg_t	err;
    char	*s;
    char	*end_s;
    int		cmd_argc;
    char	*cmd_argv[30];

    while (isspace(*str))
	str++;
    str = strdup(str);
    if (str == NULL)
	api_error(NULL, NSORT_MALLOC_FAIL);

    ret = nsort_alloc_context(0, 0);
    if (ret < 0)
	return (ret);
    sort = SYS.sorts[ret];
    if (err = setjmp(sort->api_fail))
	return (err);

    for (s = (char *) str, cmd_argc = 0; s[0] != NULL; s = end_s, cmd_argc++)
    {
	end_s = strchr(s + 1, ' ');
	if (end_s != NULL)
	    *end_s = '\0';
	cmd_argv[cmd_argc] = s;
	if (end_s == NULL)
	    break;
	while (isspace(*++end_s))
	    if (*end_s == '\n')
		*end_s = ' ';
    }
    nsort_options(sort, cmd_argc, cmd_argv);

    free((void *) str);
    return (ret);
}

/*
 * nsort_alloc_context
 *
 */
nsort_t nsort_alloc_context(int options, int size_delim)
{
    sort_t	*sort;
    nsort_msg_t err;
    int		i;

    sort = sort_alloc();
    sort->in_create = TRUE;
    sort->api_invoked = TRUE;
    if (err = setjmp(sort->api_fail))
    {
	sort_free(sort);
	return (err);
    }

    /*
    if (!(options & NSORT_EXTERNAL))
	sort->internal = TRUE;
     */
    switch (options & (NSORT_FIXED | NSORT_DELIMITED | NSORT_VARIABLE))
    {
      case NSORT_FIXED:
	RECORD.flags = RECORD_FIXED;
	if (size_delim < 1 || size_delim > 65535)
	    api_error(sort, NSORT_BAD_REC_SIZE);
	RECORD.length = size_delim;
	break;

      case NSORT_DELIMITED:
	if (size_delim > UCHAR_MAX)
	    api_error(sort, NSORT_INVALID_ARGUMENT);
	RECORD.flags = RECORD_STREAM;
	RECORD.delimiter = size_delim;
	break;

      case NSORT_VARIABLE:
	RECORD.flags = RECORD_VARIABLE;
	break;

      case 0:
	break;

      default:
	api_error(sort, NSORT_INVALID_ARGUMENT);
    }

    /* is the method for producing runs radix or merge-based?  If neither is
     * selected, one will be selected for you. XXX not yet automatic
     */
    if (options & NSORT_RADIX)
	sort->radix = TRUE;

    /* should the keys values be hashed, usually allowing for faster deletion of
     * duplicates (or summarization), but not returning records in key order?
     */
    if (options & NSORT_HASH)
	sort->compute_hash = TRUE;

    if (options & NSORT_DELETE_DUP)
	sort->delete_dups = TRUE;

    if (options & NSORT_AUTO_FREE)
	sort->auto_free = TRUE;

    switch (options & (NSORT_POINTER | NSORT_RECORD))
    {
      case NSORT_RECORD:  sort->method = SortMethRecord; break;
      case NSORT_POINTER: sort->method = SortMethPointer; break;
      case 0: break;
      default: api_error(sort, NSORT_INVALID_ARGUMENT);
    }

    if (options & NSORT_READ_PARAM)
	read_defaults(sort);

    sort->in_create = FALSE;
    return (sort->nth);
}

/* nsort_min_rec - define minimum record size of a sort.
 *
 *	For a delimited record this also happens to be the position
 *	of the first byte which might be the delimiter
 */
nsort_msg_t nsort_min_rec(nsort_t context, int size)   
{
    sort_t	*sort;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    RECORD.minlength = size;
    return (NSORT_NOERROR);
}

/* nsort_max_rec - define maximum record size of a sort.
 */
nsort_msg_t nsort_max_rec(nsort_t context, int size)   
{
    sort_t	*sort;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    RECORD.maxlength = size;
    return (NSORT_NOERROR);
}

/* nsort_int_key - define binary integer key
 *
 *      This function can be called multiple times or with other key
 *      definition functions to define multiple keys.  The order of
 *      key definitions defines the order of the keys for the sort.
 *      (The first key definition defines the primary key.)
 *	NSORT_SIGNED?, NSORT_DESCEND?
 */
nsort_msg_t nsort_int_key(nsort_t context, int flags, int offset, int size)
{
    sort_t	*sort;
    keydesc_t	*key;
    int		keynum;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    keynum = sort->keycount;
    key = (keydesc_t *) realloc(sort->keys, (keynum + 1) * sizeof(*key));
    sort->keys = key;
    key += keynum;

    switch (size)
    {
      case  1: key->type = (flags & NSORT_SIGNED) ? typeI1 : typeU1; break;
      case  2: key->type = (flags & NSORT_SIGNED) ? typeI2 : typeU2; break;
      case  4: key->type = (flags & NSORT_SIGNED) ? typeI4 : typeU4; break;
      case  8: key->type = (flags & NSORT_SIGNED) ? typeI8 : typeU8; break;
      default: api_error(sort, NSORT_INVALID_ARGUMENT); break;
    }
    key->length = size;
    key->position = offset;
    key->flags = (flags & NSORT_DESCEND) ? FIELD_DESCENDING : 0;
    key->pad = 0;
    key->number = keynum + 1;
    key->name = "<unnamed int>";
    sort->keycount = keynum + 1;

    return (NSORT_NOERROR);
}


/* Nsort_float_key - define floating point key
 *
 *      This function can be called multiple times or with other key
 *      definition functions to define multiple keys.  The order of
 *      key definitions defines the order of the keys for the sort.
 *	NSORT_DESCEND?
 */
nsort_float_key(nsort_t context, int flags, int offset, int size)
{
    sort_t	*sort;
    keydesc_t	*key;
    int		keynum;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    keynum = sort->keycount;
    key = (keydesc_t *) realloc(sort->keys, (keynum + 1) * sizeof(*key));
    sort->keys = key;
    key += keynum;

    switch (size)
    {
      case  4: key->type = typeFloat; break;
      case  8: key->type = typeFloat; break;
      default: api_error(sort, NSORT_INVALID_ARGUMENT); break;
    }
    key->length = size;
    key->position = offset;
    key->flags = (flags & NSORT_DESCEND) ? FIELD_DESCENDING : 0;
    key->pad = 0;
    key->number = keynum + 1;
    key->name = "<unnamed float/double>";
    sort->keycount = keynum + 1;

    return (NSORT_NOERROR);
}

/* Nsort_char_key - define character key
 *
 *      This function can be called multiple times or with other key
 *      definition functions to define multiple keys.  The order of
 *      key definitions defines the order of the keys for the sort.
 *      (The first key definition defines the primary key.)
 *      NSORT_CHAR_FIXED or NSORT_CHAR_DELIMITED
 *	NSORT_DESCEND?
 *      Character keys are sorted as unsigned byte strings.
 */
nsort_char_key(nsort_t context, int flags, int offset, int size_delim, char pad)
{
    sort_t	*sort;
    keydesc_t	*key;
    int		keynum;

    API_SETUP;

    keynum = sort->keycount;
    key = (keydesc_t *) realloc(sort->keys, (keynum + 1) * sizeof(*key));
    sort->keys = key;
    key += keynum;

    switch (flags & (NSORT_CHAR_FIXED | NSORT_CHAR_DELIMITED))
    {
      case NSORT_CHAR_FIXED:
	key->type = typeFixedString;
	key->length = size_delim;
	key->delimiter = 0;
	break;

      case NSORT_CHAR_DELIMITED:
	key->type = typeDelimString;
	key->delimiter = size_delim;
	key->length = 0;
	break;

      default: api_error(sort, NSORT_KEY_SYNTAX);
    }
    key->position = offset;
    key->flags = (flags & NSORT_DESCEND) ? FIELD_DESCENDING : 0;
    key->pad = pad;
    key->number = keynum + 1;
    key->name = "<unnamed char>";
    sort->keycount = keynum + 1;

    return (NSORT_NOERROR);
}

/* Nsort_decimal_key - define decimal key (an ASCII numeric string, with dec pt)
 *
 *      This function can be called multiple times or with other key
 *      definition functions to define multiple keys.  The order of
 *      key definitions defines the order of the keys for the sort.
 *      (The first key definition defines the primary key.)
 *      NSORT_CHAR_FIXED or NSORT_CHAR_DELIMITED
 *	NSORT_DESCEND?
 *
 *      Decimal keys are sorted according to their numeric values
 *	Leading white space is ignored.
 */
nsort_decimal_key(nsort_t context, int flags, int offset, int size_delim, char pad)
{
    sort_t	*sort;
    keydesc_t	*key;
    int		keynum;

    API_SETUP;

    keynum = sort->keycount;
    key = (keydesc_t *) realloc(sort->keys, (keynum + 1) * sizeof(*key));
    sort->keys = key;
    key += keynum;

    switch (flags & (NSORT_CHAR_FIXED | NSORT_CHAR_DELIMITED))
    {
      case NSORT_CHAR_FIXED:
	key->type = typeFixedDecimalString;
	key->length = size_delim;
	key->delimiter = 0;
	break;

      case NSORT_CHAR_DELIMITED:
	key->type = typeDelimDecimalString;
	key->delimiter = size_delim;
	key->length = 0;
	break;

      default: api_error(sort, NSORT_KEY_SYNTAX);
    }
    key->position = offset;
    key->flags = (flags & NSORT_DESCEND) ? FIELD_DESCENDING : 0;
    key->pad = pad;
    key->number = keynum + 1;
    key->name = "<unnamed decimal>";
    sort->keycount = keynum + 1;

    return (NSORT_NOERROR);
}


/* Nsort_input_size - define size of sort input.
 *
 *      This call helps nsort allocate the correct amount of process memory.
 *	It is most useful for external sorts.
 */
nsort_msg_t nsort_input_size(nsort_t context, long long size)   
{
    sort_t	*sort;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    IN.input_size = size;
    return (NSORT_NOERROR);
}

/* Nsort_input_count - set the maximum number of records to be sorted
 *
 *      This call helps nsort allocate the correct amount of process memory
 *      It is most useful for external sorts.
 */
nsort_msg_t nsort_input_count(nsort_t context, long long count)   
{
    sort_t	*sort;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    sort->n_recs = count;
    return (NSORT_NOERROR);
}
                 
/* Nsort_int_sum - define an integer field to be summarized
 *
 *      This call causes records with duplicate keys to be deleted.
 *      When nsort detects two records with duplicate keys, it will add
 *      the contents of the field specified by this call in the record to
 *      be deleted, to the same field in the surviving record.  If the
 *      addition would cause an overflow condition, the addition is not
 *      performed and the duplicate record is not deleted - leading to
 *      duplicate keys in the output.
 *
 *      Multiple summarized fields may be specified with multiple calls.
 *      For instance, a count and a sum field might both be summarized
 *      in order to calculate the average of the sum field by key value.
 */
nsort_msg_t nsort_int_sum(nsort_t context, int flags, int offset, int size)
{
    sort_t	*sort;
    field_t	*field;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    field = (field_t *) realloc(RECORD.fields, (RECORD.fieldcount + 1) * sizeof(*field));
    RECORD.fields = field;
    field += RECORD.fieldcount;

    switch (size)
    {
      case  4: field->type = (flags & NSORT_SIGNED) ? typeI4 : typeU4; break;
      case  8: field->type = (flags & NSORT_SIGNED) ? typeI8 : typeU8; break;
      default: api_error(sort, NSORT_SUMMARY_NEEDS_INT); break;
    }

    field->name = "<unnamed sum int>";
    field->position = offset;
    field->flags = 0;
    field->length = size;
    field->delimiter = 0;
    field->pad = 0;
    field->derivation = NULL;
    field->number = -1;

    sort->summarize = (short *) realloc(sort->summarize,
				      (sort->n_summarized + 1) * sizeof(short));
    sort->summarize[sort->n_summarized] = (short) RECORD.fieldcount;

    /* Make both the field definition and the summarization visible and real
     */
    RECORD.fieldcount++;
    sort->n_summarized++;

    return (NSORT_NOERROR);
}

/* Nsort_temp_file - define temporary files for an external sort
 */
nsort_msg_t nsort_temp_file(nsort_t context, int xfer_size, int xfer_count,
					     int file_count,
					     char *file[])
{
    sort_t	*sort;
    int		i;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);
	
    rm_temps(sort);
    if (xfer_size <= 0 || xfer_count <= 0 || file_count < 0)
	api_error(sort, NSORT_INVALID_ARGUMENT);

    TEMP.blk_size = xfer_size;
    TEMP.aio_cnt = xfer_count;
    for (i = 0; i != file_count; i++)
    {
	if ((err = add_temp_file(sort, file[i])) != NSORT_NOERROR)
	    return (err);
    }

    return (NSORT_NOERROR);
}


/* Nsort_procs - define the number of sorting sprocs for the sort.
 *
 *      For the first version, this call can only be made for external sorts.
 *      In the absence of this call, the number of sorting sprocs will be the
 *      number of active processors.
 *
 */
nsort_msg_t nsort_procs(nsort_t context, int n_procs)
{
    sort_t	*sort;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    if (prctl(PR_MAXPPROCS) < sysmp(MP_NPROCS))
    {
	/* XXX hard error? soft warning?  see parser's tokPROCESSES */
	if (n_procs < 1 || n_procs > SYS.n_processors)
	    api_error(sort, NSORT_CPU_REQ_TOOBIG);
    }
    sort->n_sprocs = n_procs;
    return (NSORT_NOERROR);
}
                 

/* nsort_mem_limit - set the maximum amount of process memory to be used by sort
 *
 *      nbytes   memory limit in bytes
 */
nsort_msg_t nsort_mem_limit(nsort_t context, long nbytes)
{
    sort_t	*sort;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    sort->memory = nbytes / 1024;
    return (NSORT_NOERROR);
}

/* api_sort_done -	clear the per-execution fields of an api_invoked sort
 *			needed to re-execute the same sort over and over
 *
 *	The changes made here do not change the record/key/summarizing/... defs
 *
 *	Called in the client context, from nsort_internal/return_recs/ptrs
 *	This completes a successful sort - use do_cancel() for errors
 */
void api_sort_done(sort_t *sort)
{
#if 0
    OUT.end_io_output = TRUE;
	OUT.end_sort_output = TRUE;
#endif

    /* wait for the i/o sproc to stop looking at this sort struct
     */
    if (!sort->internal && sort->go_sprocs)
    {
	while (sort->input_phase || sort->output_phase)
	    ordnap(1, "api_sort_done");
    }

    ASSERT(OUT.end_sort_output == TRUE);
    ASSERT(IN.part_final == IN.part_taken);
    sort->api_phase = API_OUTPUT_DONE;

    sort->internal = FALSE;

    if (sort->auto_free)
	free_resources(sort);

    if (sort->region)
    {
	sort->region->busy = FALSE;
	sort->region = NULL;
    }
    sort->go_sprocs = FALSE;
    sort->two_pass = FALSE;

    IN.input_size = 0;
    CONDITIONAL_FREE(IN.aiocb);
    CONDITIONAL_FREE(OUT.aiocb);

    sort->api_phase = API_NEEDSINIT;
}

/*
Internal Sort Routines
----------------------
The nsort_internal() subroutine sorts records in memory according to the
sort defined in the given sort context.  It is the only call needed to
perform a defined internal sort.
               unsigned *n_recs, number of records input, output 
               void *first_rec,  pointer to first record for fixed-length,
                                    sequential array.  If NULL, pointer array
                                    points to input records 
               void **rec)       pointer array
Besides the sort context argument, nsort_internal() takes record count,
record pointer and record pointer array arguments.  The record count
argument is used to both indicate the number of input records to the sort,
and to return the number of output records.  Nsort_internal() returns a
status value.

If a record sort has been defined (as opposed to a pointer sort), the
record pointer should point to the array of records to be sorted.  The
pointer array argument is not used.  The sorted records will be returned in
the same array:

    nsort_internal(context, &n_recs, record_array, NULL);

For pointer sorts, the returned record pointers are always written into the
pointer array.  The input record pointers may also be passed to
nsort_internal() in the pointer array:

    nsort_internal(context, &n_recs, NULL, ptr_array);

For pointer sorts with fixed-length records and where the records are in a
sequential array in memory, the input record pointers may alternately be
defined by passing the array address in the record pointer argument.  In
this case the input values of the pointer array are not used:

    nsort_internal(context, &n_recs, record_array, ptr_array);
*/

/* nsort_internal - perform a one-pass sort of data which already is in memory
 *
 *	n_recs		Address of the number of records to sort.
 *			Upon return is set to the number of resulting records.
 *	first_rec -	If non-null then the records to be sorted are layed out
 *			sequentially in memory starting from this address;
 *			the sorted results are placed here if ptr_array is null.
 *			If null then ptr_array must not be null.
 *	ptr_array -	This selects whether to do a record or pointer sort.
 *			If null then the records pointed to by first_rec
 *			will be sorted in place.
 *			If non-null then this will be a pointer sort;
 *			the result pointers will be put into this array.
 *			The input data for a pointer sort may come from either
 *			first_rec (if that is non-null), or from the initial
 *			values in ptr_array (if first_rec is null).
 *
 *	There are three valid combinations of first_rec and ptr_array:
 *	first_rec	ptr_array	Method	Data Source	Result Dest
 *	valid		NULL		Record	first_rec	first_rec
 *	valid		valid		Pointer	first_rec	ptr_array
 *	NULL		valid		Pointer	ptr_array	ptr_array
 *	
 *	Note that if a ptr_array is provided:
 *		the sorting method is pointer sort
 *		the actual records are not modified
 *		the resulting pointers are put into ptr_array
 *	Contrasting with no ptr_array being provided:
 *		the sorting method is record sort,
 *		the records are updated in place
 *		the record size is limited (to 104 bytes, currently)
 *
 * Implementation Notes: first_rec valid: internal, !IN.ptr_input
 *	This is similar to a one-pass sort in which all the data
 *	has been read in by the time the sorting sprocs start.
 *
 * Implementation Notes: first_rec NULL, ptr_array valid: internal, IN.ptr_input
 *	This is different from other kinds of sorts.
 *	It is a pointer sort which has its input from an array of pointers,
 *	rather than a sequential set of records.
 *	At the bottom 'levels' of calls (e.g. init_item_int{ernal}) this fact is
 *	know.  At the upper 'levels' the calls behave as if the pointers
 *	themselves were the records.
 */
nsort_msg_t nsort_internal(nsort_t context, unsigned *n_recs,
					    void *first_rec,
					    void **ptrs)
{
    sort_t	*sort;
    int		i;
    in_run_t	*run;
    int		sproc_id = 1;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    if (IN.n_files != 0 || OUT.file->fd != -1 || sort->n_derived != 0)
	api_error(sort, NSORT_APIFILES);

    reset_sort(sort);
    sort->internal = TRUE;
    sort->n_recs = *n_recs;
    sort->client_pid = getpid();
    *n_recs = 0;			/* in case of error */
    sort->method = (ptrs == NULL) ? SortMethRecord : SortMethPointer;
    IN.ptr_input = first_rec == NULL;
    IN.recs_aligned = !IN.ptr_input && ((ptrdiff_t) first_rec & 0x3) == 0;
    prepare_sort_def(sort);

    /*prepare_shared(sort);*/
    map_io_buf(sort);
    /*nsort_init(sort);*/

    /* Set up the sort's input and output areas
     */
    run = &sort->in_run[0];
    if (first_rec == NULL)
    {
	if (ptrs == NULL)
	    api_error(sort, NSORT_INVALID_ARGUMENT);
	run->read_start = (byte *) ptrs;
	run->read_end = (byte *) ptrs + sort->n_recs;
    }
    else
    {
	run->read_start = (byte *) first_rec;
	run->read_end = (byte *) first_rec + sort->n_recs * RECORD.length;
    }
    run->first_rec = run->read_start;
    run->read_target = run->read_end;

    if (sort->method == SortMethRecord)
    {
	/* For this use, pretend that the output i/o alignment requirement
	 * covers the entire result buffer, & that it is the one output buffer.
	 */
	OUT.dio_io = sort->n_recs * RECORD.length;
	OUT.rec_buf_pad = 0;
	/* For internal rec sorts guess_rem_offset is relative to the start of
	 * the result buffer 
	 */
	for (i = 0; i < MERGE.merge_cnt; i++)
	    MERGE.rob[i].u.buf = first_rec;
    }
    else
    {
	for (i = 0; i < MERGE.merge_cnt; i++)
	    MERGE.ptr_part[i].ptrs = (byte *) ptrs;
    }

    run->eof = TRUE;
    IN.part_final = IN_RUN_PARTS;
    IN.part_avail = IN_RUN_PARTS;

    sort_input(sort, sproc_id);

    if (STATS.usages != NULL)
    {
	getrusage(RUSAGE_SELF, &STATS.usages[sproc_id].in_usage);
	STATS.usages[sproc_id].in_elapsed = get_time() - TIMES.begin;
    }
    
    if (sort->n_errors == 0)
    {
	ASSERT(MERGE.merge_avail == 0);
	IN.runs_read = 1;
	sort->input_phase = FALSE;
	sort->output_phase = TRUE;
	chk_output_merge(sort);
	ASSERT(MERGE.merge_avail > 0);
	sort_output(sort, sproc_id);
    }

    if (STATS.usages != NULL)
    {
	getrusage(RUSAGE_SELF, &STATS.usages[sproc_id].total_usage);
	STATS.usages[sproc_id].total_elapsed =  get_time() - TIMES.begin;
    }

    /* Find out where the last merge ended.
     * XXX merge_done probably is wrong when multiple sorting sprocs are present
     */
    i = (MERGE.merge_done - 1) % MERGE.merge_cnt;
    if (sort->method == SortMethRecord)
    {
	/* Determine the number of records written from the eof-finding merge?
	 */
	*n_recs = 1 + ((MERGE.rob[i].tail_rec - (byte *) first_rec) /
		       sort->edited_size);
	for (i = 0; i < MERGE.merge_cnt; i++)
	{
	    ASSERT(MERGE.rob[i].out_size == 0);
	    ASSERT(MERGE.rob[i].remainder == 0);
	}
    }
    else
    {
	*n_recs = 1 + (MERGE.ptr_part[i].tail_ptr - (volatile byte **) ptrs);
    }

    api_sort_done(sort);

    return (0);
}

/*
External Sort Routines
----------------------
Once an external sort context has been defined, sorting can take place
by iteratively releasing records to the sort routines, declaring the end
of released records with a call to nsort_end_input(), and then iteratively
returning the sorted records.

Records may be released to the nsort routines using nsort_release_ptrs().  The
number of records and an array of pointers to those records are passed as
arguments (in addition to the sort context):

A single sort MAY release both pointers and records
The 'release' behaves similarly to the i/o sproc with a pipe-like input file
of irregular block sizes.
*/

/* runio_error	- return the error status of the read/write, or EINPROGRESS
 */
int runio_error(const aiocb_t *aio)
{
    return (aio->aio_errno);
}

/* runio_return	- return the number of bytes read or written
 *
 *	die if the operation is still in progress
 */
ssize_t runio_return(aiocb_t *aio)
{
    if (runio_error(aio) == EINPROGRESS)
	die("runio_return: still busy");
    return (aio->aio_ret);
}

/* runio_read	- prepare an in-memory `aio' to be the destination to which
 *		  released records/pointers will be copied
 *
 *	runio_error will return EINPROGRESS until either:
 *	1: the entire buffer is filled
 *	2: nsort_end_input() is called, simulating an EOF
 */
int runio_read(aiocb_t *aio)
{
    aio->runio_curpos = 0;
    /* The previous read was the last one: make this one return 0 bytes
     */
    if (aio->runio_final)
    {
	aio->aio_ret = 0;
	aio->aio_errno = 0;
    }
    else
    {
	aio->aio_ret = INT_MAX;
	aio->aio_errno = EINPROGRESS;
    }
    return (0);
}

/* runio_write	- prepare an in-memory `aio' to be the source from which
 *		  returned records/pointers will be copied
 */
int runio_write(aiocb_t *aio)
{
    aio->runio_curpos = 0;
    aio->aio_ret = INT_MAX;
    aio->aio_errno = EINPROGRESS;
    return (0);
}

/* api_final_write	- prepare an in-memory `aio' to be the source from which
 *			  the last returned records/pointers will be copied
 *			  and wait for return_recs/ptrs to be done with it
 */
void api_final_write(sort_t *sort, byte *base, unsigned size)
{
    aiocb_t	*aio;

    aio = &OUT.aiocb[0 /* OUT.writes_issued % OUT.aio_cnt */ ];
    aio->aio_offset = OUT.write_offset;
    aio->aio_buf = (void *) base;
    aio->aio_nbytes = size;

    aio->runio_final = TRUE;
    runio_write(aio);
    do
    {
	ordnap(1, "api_final_write");
    } while (aio->aio_errno == EINPROGRESS && sort->n_errors == 0);
}

int runio_cancel(int fd, aiocb_t *aio)
{
    die("runio_cancel");
    return (AIO_ALLDONE);
}

/* external_io	- handles i/o for api external sorts
 *
 *	
 */
void external_io(sortsproc_t *ssp, size_t stksz)
{
    int		i;
    sort_t	*sort = ssp->sort;

    ASSERT(ssp->sproc_pid == getpid());

    init_aio(sort);

    IN.aiocb[0].runio_final = FALSE;
    OUT.aiocb[0].runio_final = FALSE;

    sort->go_sprocs = TRUE;

    (*sort->reader)(sort);	/* e.g. io_input() */

    if (sort->n_errors == 0)
	(*sort->writer)(sort);	/* e.g. io_output() */

    /* wait for other sprocs to complete by blocking ourself
     * for each other sproc created.
     */
    for (i = 1; i <= sort->n_sprocs; i++)
    {
	if (Print_task)
	    fprintf(stderr, "external_io blocking on %d\n", ssp->sproc_pid);
	blockproc(ssp->sproc_pid);
    }

    TIMES.done = get_time() - TIMES.begin;

    /* Clearing both tells anyone looking that this sort is done.
     * input_phase usually will be false here, unless there was a runtime_error
     */
    sort->input_phase = FALSE;
    sort->output_phase = FALSE;
}

#if defined(TEST_AIO)
void test_aio(void)
{
    aiocb_t	*aio = aiocb_alloc(1);
    int		size = 16384;
    char	*buf = (char *) malloc(size);
    int		fd = open("TEMP", O_RDWR | O_CREAT | O_TRUNC, 0666);
    int		err;
    int		i;
    int		j;

    if (fd < 0)
	die("external_io: open failed: %s", strerror(errno));
    aio->aio_fildes = fd;
    aio->aio_nbytes = size;
    aio->aio_buf = buf;
    errno = 0;
    for (i = 0; i != 20; i++)
    {
	aio->aio_offset = i * size;
	sprintf(buf, "I is %d @ %d\n", i, aio->aio_offset);
	if ((err = aio_write(aio)) < 0)
	    die("sproc_daemon: aio_write #%d failed: %s", i, strerror(errno));
	for (j = 0; j != 100; j++)
	    if ((err = aio_error(aio)) != EINPROGRESS)
		break;
	    else
		sginap(10);
	if (err != 0)
	    die("sproc_daemon: #%d @ %d naps aio_error failed: %s", i, j, strerror(err));
	if ((err = aio_return(aio)) != size)
	    die("sproc_daemon: #%d @ %d naps returned wrong: %s", i, j, strerror(errno));
    }
    close(fd);
    free(aio);
    free(buf);
}
#endif

/* sproc_daemon	- wait for, then perform i/o sproc or sorting sproc tasks
 *	
 */
void sproc_daemon(int which, size_t stksz)
{
    sortsproc_t	task;
    int		self = getpid();

#if 0
    /* This allows us to debug the early life of an sproc
     */
    fprintf(stderr, "sproc_daemon %d pausing for attach\n", self);
    sleep(10);
#endif

    /* Prepare to exit from the following loop when the client app exits
     */
    exit_with_parent();

    catch_signals(SIGINT, SIGQUIT, ordinal_exit);
    catch_signals(SIGILL, SIGPIPE, sigdeath);

    /* Loop performing this sort and then waiting for the next,
     * until the client app exits and we exit inside of sproc_hup()
     */
    for (;;)
    {
#if defined(TEST_AIO)
	if (which == 0)
	{
	    system("ps;date");
	    test_aio();
	    system("date;ps");
	}
#endif

	/* Wait for the client to define the next sort and start providing data
	 */
	blockproc(self);

	/* XXX MP-unsafe: sprocs array could be in realloc() by client
	 */
	task = NsortSys.sprocs[which];

	ASSERT(task.func == external_io || task.func == sortproc_main);
	(*task.func)(&task, stksz);

	if (Print_task)
	    fprintf(stderr, "sproc_daemon %d which %d done with %s\n",
		    self, which, (task.func == external_io) ? "external_io"
							    : "sortproc_main");
	/* Say that this sproc is now available for another task
	 * XXX MP-unsafe: sprocs array could be in realloc() by client
	 */
	NsortSys.sprocs[which].func = NULL;
    }

    /* NOTREACHED */
}

/* get_sproc	- find a free sproc in the cached pool, or allocate a new one
 */
sortsproc_t *get_sproc(void)
{
    sortsproc_t	*sp;
    int		i;

    for (i = 0; i != NsortSys.n_sprocs_cached; i++)
	if (SYS.sprocs[i].func == NULL)
	    return (&NsortSys.sprocs[i]);

    SYS.sprocs = (sortsproc_t *) realloc(SYS.sprocs, (i + 1) * sizeof(*sp));
    sp = &NsortSys.sprocs[i];
    memset(sp, 0, sizeof(*sp));
    NsortSys.n_sprocs_cached++;
    sp->sproc_pid = sprocsp((void (*)(void *arg, size_t stack)) sproc_daemon,
			    PR_SADDR|PR_SFDS, (void *) i, (caddr_t) 0, STK_SZ);
    if (sp->sproc_pid < 0)
	die("start_sproc:sprocsp failure (brk == %08x) : %s",
	    sbrk(0), strerror(errno));

    if (Print_task)
	fprintf(stderr, "get_sproc: %d new pid %d\n", i, sp->sproc_pid);

    return (sp);
}

void start_external_sprocs(sort_t *sort)
{
    short	*sprocs;
    sortsproc_t	*sp;
    int		i;

    /* is this the first external sort? init our aio sprocs here
     */
    if (SYS.n_sprocs_cached == 0)
    {
#if 1
       aioinit_t aioinit;

	memset(&aioinit, 0, sizeof(aioinit));
	/* threads: # of i/o's passable to the kernel
	 * locks:   # sprocs which call aio_suspend() (only the i/o sproc does)
	 * numusers:# total sprocs in this sort - (XXX what to do in lib vers?)
	 */
	aioinit.aio_threads = 2;
	aioinit.aio_locks = 5;
	aioinit.aio_numusers = 1 + 10 + aioinit.aio_threads;
	aio_sgi_init(&aioinit);
#endif
    }

    sprocs = (short *) malloc((1 + sort->n_sprocs) * sizeof(short));
    sort->sprocs = sprocs;
    for (i = 0; i <= sort->n_sprocs; i++)
    {
	sp = get_sproc();
	sprocs[i] = sp - NsortSys.sprocs;
	sp->sort = sort;
	sp->spid = i;
	sp->func = (i == 0) ? external_io : sort->worker_main;
    }
    sort->io_sproc = SYS.sprocs[sprocs[0]].sproc_pid;

#if defined(DEBUG1)
    fprintf(stderr, "i/o sproc: %d; sort sproc: %d; nshare %d\n",
		    sort->io_sproc, SYS.sprocs[sprocs[1]].sproc_pid, prctl(PR_GETNSHARE));
#endif

    for (i = 0; i <= sort->n_sprocs; i++)
	if (unblockproc(SYS.sprocs[sprocs[i]].sproc_pid) < 0)
	    die("start_external_sprocs:sproc %d/%d: %s", i,
		SYS.sprocs[sprocs[i]].sproc_pid, strerror(errno));

    /* Wait for the i/o sproc to init_aio(), required by release_ptrs/recs
     */
    while (!sort->go_sprocs && sort->n_errors == 0)
	ordnap(1, "start_external_sprocs");
}

void start_external(sort_t *sort)
{
    if (IN.n_files != 0 || OUT.file->fd != -1)
	api_error(sort, NSORT_APIFILES);

    if (sort->internal || sort->n_derived != 0)
	api_error(sort, NSORT_INVALID_ARGUMENT);

    reset_sort(sort);

    sort->touch_memory = FlagFalse;

    sort->client_pid = getpid();

    /* The i/o sproc treats an external api sort as if it were a single input
     * file. Fake up the file information so that io.c is at most only barely
     * aware that an external api sort is different from a standalone file sort.
     */
    IN.n_files = 1;
    IN.files = (fileinfo_t *) realloc(IN.files, sizeof(fileinfo_t));
    memset(IN.files, 0, sizeof(fileinfo_t));
    IN.files->fd = -1;
    IN.files->openmode = -1;
    IN.files->io_size = 128 * 1024;
    IN.files->stbuf.st_mode = S_IFIFO;		/* non-seekable, !SIZE_VALID */
    IN.files->mem_align = 1;
    IN.files->min_io = 1;
    IN.files->max_io = INT_MAX;
    IN.ptr_input = FALSE;

    /* The current versions of release_recs/ptrs depend on the same aiocb_t
     * being used for successive i/o sproc input read requests.
     * Probably record-sort output requests will also need to have exactly 1
     */
    IN.files->aio_count = 1;

    OUT.file->fd = -1;
    OUT.file->openmode = -1;
    OUT.file->io_size = OUT.write_size = 128 * 1024;
    OUT.dio_io = LL_BYTES;
    OUT.file->stbuf.st_mode = S_IFIFO;		/* non-seekable, !SIZE_VALID */
    OUT.file->mem_align = OUT.dio_mem = 1;
    OUT.file->min_io = 1;
    OUT.file->max_io = INT_MAX;
    OUT.aio_cnt = OUT.file->aio_count = 1;

    prepare_sort_def(sort);

    sort->preparer = prepare_shared;
    sort->worker_main = (taskmain_t) sortproc_main;
    sort->reader = io_input;
    sort->writer = io_output;

    sort->read_input = runio_read;
    sort->write_output = runio_write;
    sort->get_error = runio_error;
    sort->get_return = runio_return;
    sort->final_write = api_final_write;

    map_io_buf(sort);

    /* Make/recycle the i/o and sorting sprocs
     */
    start_external_sprocs(sort);
}

/* nsort_release_ptrs	- give a batch of pointers to the sorting sprocs
 *
 *	n_recs	number of records (and pointers)
 *	ptrs	array of pointers to records to add to the input data set
 */
nsort_msg_t nsort_release_ptrs(nsort_t context, unsigned n_recs, void **ptrs)
{
    sort_t	*sort;
    int		left;
    int		i;
    aiocb_t	*aiocb;
    byte	*start;
    byte	*curr;
    int		length;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    /* Is this the first set of input records? If so let the sorting sprocs run
     */
    if (sort->go_sprocs == FALSE)
    {
	start_external(sort);
	sort->api_phase = API_INPUT_PTRS;
    }
    else if (sort->api_phase != API_INPUT_PTRS && sort->api_phase != API_INPUT_RECS)
	api_error(sort, NSORT_INVALID_ARGUMENT);

    length = RECORD.length;
    aiocb = IN.aiocb;
    while (aiocb->aio_errno != EINPROGRESS && sort->n_errors == 0)
	ordnap(1, "release_ptrs");
    curr = start = (byte *) aiocb->aio_buf + aiocb->runio_curpos;
    left = aiocb->aio_nbytes - aiocb->runio_curpos;
    for (i = 0; i != n_recs; i++)
    {
	if (RECORD.flags & RECORD_STREAM)
	    length = 1 + (byte *) fmemchr(ptrs[i],
					  RECORD.delimiter,
					  MAXRECORDSIZE) - (byte *) ptrs[i];
	else if (RECORD.flags & RECORD_VARIABLE)
	    length = ulh(ptrs[i]) + 2;

	if (length <= left)
	{
	    memmove(curr, ptrs[i], length);
	    left -= length;
	    curr += length;
	}
	else
	{
	    /* This set of record pointers does't entirely fit in this aio/run.
	     * Mark this i/o as completed, then wait for the next aio/run
	     * to be initialized and for the runio_read to be re-posted
	     */
	    memmove(curr, ptrs[i], left);
	    ASSERT(aiocb->runio_curpos + (curr - start + left) == aiocb->aio_nbytes);
#if UPDATE_RUN
	    /* It would be nice to perform chk_input() updates to the run here
	     * so that the sorting sprocs can use these records right away.
	     * Doing so requires copying a bunch of end-of-run code from io.c
	     */
	    sort->in_run[IN.runs_read % N_IN_RUNS].read_end += ???
#endif
	    aiocb->runio_curpos = aiocb->aio_nbytes;
	    aiocb->aio_ret = aiocb->aio_nbytes;
	    aiocb->aio_errno = 0;
	    do
	    {
		ordnap(1, "release_ptrs next buf");
		if (sort->n_errors != 0)
		    return (sort->last_error);
	    } while (aiocb->aio_errno != EINPROGRESS);
	    ASSERT(length - left < aiocb->aio_nbytes);
	    start = (byte *) aiocb->aio_buf;
	    memmove(start, (byte *) ptrs[i] + left, length - left);
	    curr = start + (length - left);
	    left = aiocb->aio_nbytes - (length - left);
	}
    }
    aiocb->runio_curpos += curr - start;
#if UPDATE_RUN
    sort->in_run[IN.runs_read % N_IN_RUNS].read_end += curr - start
#endif

    return (NSORT_NOERROR);
}

/* nsort_release_recs
 *
 *	n_recs	number of sequentially stored records
 *	recs	pointer to first record to be released
 *	size? instead for varlen records?
 */
nsort_msg_t nsort_release_recs(nsort_t context, unsigned n_recs, void *recs) 
{
    sort_t	*sort;
    int		left;
    int		i;
    aiocb_t	*aiocb;
    int		length;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    /* Is this the first set of input records? If so let the sorting sprocs run
     */
    if (sort->go_sprocs == FALSE)
    {
	start_external(sort);
	sort->api_phase = API_INPUT_RECS;
    }
    else if (sort->api_phase != API_INPUT_PTRS && sort->api_phase != API_INPUT_RECS)
	api_error(sort, NSORT_INVALID_ARGUMENT);

    aiocb = IN.aiocb;
    length = RECORD.length * n_recs;
    while (length > 0)
    {
	left = aiocb->aio_nbytes - aiocb->runio_curpos;
	if (length <= left)
	{
	    memmove((byte *)aiocb->aio_buf + aiocb->runio_curpos, recs, length);
	    aiocb->runio_curpos += length;
#if UPDATE_RUN
	    sort->in_run[IN.runs_read % N_IN_RUNS].read_end += length;
#endif
	    break;
	}
	else
	{
	    /* This set of records does't entirely fit in this aio/run.
	     * Mark this i/o as completed, then wait for the next aio/run
	     * to be initialized and for the runio_read to be called again
	     */
	    memmove((byte *) aiocb->aio_buf + aiocb->runio_curpos, recs, left);
#if UPDATE_RUN
	    if (sort->in_run[IN.runs_read % N_IN_RUNS].read_end ==
		sort->in_run[IN.runs_read % N_IN_RUNS].read_start &&
		!IN.run_remainder)
	    sort->in_run[IN.runs_read % N_IN_RUNS].read_end += left;
#endif
	    ASSERT(aiocb->runio_curpos + left == aiocb->aio_nbytes);
	    aiocb->runio_curpos = aiocb->aio_nbytes;
	    aiocb->aio_ret = aiocb->aio_nbytes;
	    aiocb->aio_errno = 0;
	    do
	    {
		ordnap(1, "release_recs next buf");
		if (sort->n_errors != 0)
		    return (sort->last_error);
	    } while (aiocb->aio_errno != EINPROGRESS);
	    length -= left;
	    recs = (void *) ((byte *) recs + left);
	}
    }

    return (NSORT_NOERROR);
}

/* nsort_end_input	- start the output phase of an external api sort
 *
 *	Cause the external i/o sproc to switch to the output phase
 */
nsort_msg_t nsort_end_input(nsort_t context)
{
    sort_t	*sort;
    int		i;
    in_run_t	*run;
    int		sproc_id = 1;
    aiocb_t	*aiocb;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    if (sort->api_phase != API_INPUT_PTRS && sort->api_phase != API_INPUT_RECS)
	api_error(sort, NSORT_INVALID_ARGUMENT);

    sort->api_phase = API_END_INPUT;
    /* Complete the last aio_read where it is, set run's eof
     */
    aiocb = &IN.aiocb[0];
    aiocb->aio_ret = aiocb->runio_curpos;
#if UPDATE_RUN
    sort->in_run[IN.runs_read % N_IN_RUNS].eof = TRUE;
#endif
    aiocb->runio_final = TRUE;
    aiocb->aio_errno = 0;	/* clear EINPROGRESS (must be last) */

    return (NSORT_NOERROR);
}

/*
Records can be returned in sorted order by using the nsort_return_ptrs()
routine.  The second argument is a pointer to maximum number of record
pointers that can be returned.  This value is overwritten by
nsort_return_ptrs() with the actual number of record pointers returned.
*/

/* nsort_return_ptrs	- copy sorted pointers into a ptr[] the client provides
 *
 * A single sort may NOT call both nsort_return_ptrs and nsort_return_recs()
 *
 *	n_recs	max number of pointer array elements is passed on input,
 *		the number of record pointers written is returned
 *	ptrs	array of pointers to returned records
 *		This is filled in with ordered record pointers
 *		The number of pointers is given by the output value of *n_recs
 */
nsort_msg_t nsort_return_ptrs(nsort_t context, unsigned *n_recs, void **dest)
{
    sort_t	*sort;
    int		left;
    int		i;
    unsigned	taken;
    byte	*rec;
#if defined(DEBUG1)
    byte	*prev = NULL;
#endif
    byte	**ptrs;
    byte	**ptrs_end;
    byte	**p;
    copy_task_t	*task;
    tbuf_t	*tbuf;
    int		entry_size;

    API_SETUP;

    ptrs = (byte **) dest;
    i = *n_recs;
    ptrs_end = ptrs + i;
    *n_recs = 0;

    if (i == 0 || sort->method == SortMethRecord)
	api_error(sort, NSORT_INVALID_ARGUMENT);

    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    entry_size = (int) ((RECORD.flags & RECORD_FIXED)
			? sizeof(byte *)
			: sizeof(byte *) + sizeof(u2));
    if (sort->api_phase == API_END_INPUT)
    {
	/* Wait for the i/o and sorting sprocs to complete the input phase
	 * before we start copying the results out
	 */
	while (sort->input_phase)
	{
	    if (sort->n_errors != 0)
		return (sort->last_error);
	    ordnap(1, "return_ptrs input done");
	}
	sort->api_phase = API_OUTPUT_PTRS;
    }
    else if (sort->api_phase != API_OUTPUT_PTRS)
	api_error(sort, NSORT_INVALID_ARGUMENT);

    for (taken = COPY.copy_taken; taken < COPY.copy_avail; taken++)
    {
	/* The previous return_ptrs() may have consumed all the records
	 * of one or more copies.  Mark them as done, now that the client
	 * has processed all the previously returned batch of pointers.
	 */
	task = COPY.copy + (taken % COPY.copy_cnt);
	if (task->src_rec < task->limit_rec)
	{
	    COPY.copy_taken = taken;
	    break;
	}
	task->copy_done = TRUE;
    }

    while (ptrs != ptrs_end)
    {
	while (taken == COPY.copy_avail)
	{
	    if (sort->n_errors != 0)
		return (sort->last_error);

	    if (OUT.end_sort_output)
		goto done;
	    ordnap(1, "return_ptrs more output");
	}
	ASSERT(taken < COPY.copy_avail);
	task = COPY.copy + (taken % COPY.copy_cnt);
	/* This might be the first time return_ptrs has seen this copy task.
	 * If so then and skip_bytes is non-zero, then the pointer has already
	 * been placed into the client's pointer array: skip over it and
	 * clear skip_bytes to avoid skipping over the first pointer the next
	 * time we see the task (if we don't complete use it up this call).
	 */
	p = task->src_rec;
	if (task->skip_bytes != 0)
	{
	    p = (byte **) ((byte *) p + entry_size);
	    task->skip_bytes = 0;
	}
	if (Print_task)
	    fprintf(stderr, "return_ptrs copy %d %08x ptrs %08x->%08x\n",
			    COPY.copy_taken, task, p, task->limit_rec);

	for (; p != task->limit_rec; p = (byte **) ((byte *) p + entry_size))
	{
	    if (((ptrdiff_t) (rec = (byte *) ulw(p))) > 0)
	    {
		*ptrs++ = rec;
#if defined(DEBUG1)
		if (prev != NULL && compare_data(sort, prev, rec) > 0)
		{
		    nsort_print_data(sort, prev, stderr, FALSE, FALSE);
		    nsort_print_data(sort, rec, stderr, FALSE, FALSE);
		    die("return_ptrs: out of order @ %x: prev %x next %x\n",
			    p, prev, rec);
		}
		prev = rec;
#endif
		if (ptrs == ptrs_end)
		{
		    task->src_rec = (byte **) ((byte *) p + entry_size);
		    goto done;
		}
	    }
	    else
	    {
		/* record ptr is really tbuf ptr with the high order bit set.
		 * Link tbuf onto a list of tbufs completed by this copy task.
		 */
		tbuf = (tbuf_t *) ~(ptrdiff_t)(rec);
#if defined(DEBUG1)
		if (Print_task)
		    fprintf(stderr, "return_ptrs %x finds tbuf %x/%d/%d f_next %x at %x\n",
			    task, tbuf, tbuf->run, tbuf->block, task->tbuf, p);
#endif
		tbuf->f_next = task->tbuf;
		task->tbuf = tbuf;
	    }
	}
	task->src_rec = p;
	taken++;
	if (task->end_of_merge)
	    break;
    }

done:
    *n_recs = ptrs - (byte **) dest;
    if (*n_recs == 0)
	api_sort_done(sort);
    return (NSORT_NOERROR);
}

/* nsort_return_recs	- copy sorted records into a buffer the client provides
 *
 *	The caller should copy the pointed-to records to its own memory area
 *	before initiating another call to nsort_return_ptrs().
 *	When the number of pointers returned is 0, the end of the returned
 *	records has been reached.
 *	If the records are fixed-length, nsort_return_recs() can be used to have
 *	the returned records written into sequential memory:
 *
 *	n_recs	max number of record array elements is passed on input,
 *		the number of record array elements written is returned 
 *	rec	pointer to record array to be filled with returned records
 */

nsort_msg_t nsort_return_recs(nsort_t context, unsigned *n_recs, void *recs)
{
    sort_t	*sort;
    int		left;
    int		i;
    aiocb_t	*aiocb;
    int		client_space;
    byte	*orig_recs;

    API_SETUP;

    i = *n_recs;
    *n_recs = 0;

    if (i == 0 || sort->method == SortMethPointer)
	api_error(sort, NSORT_INVALID_ARGUMENT);

    orig_recs = (byte *) recs;
    client_space = RECORD.length * i;
    aiocb = OUT.aiocb;

    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    if (sort->api_phase == API_END_INPUT)
    {
	/* Wait for the i/o and sorting sprocs to complete the input phase
	 * before we start copying the results out
	 */
	while (sort->input_phase)
	{
	    if (sort->n_errors != 0)
		return (sort->last_error);
	    ordnap(1, "return_recs input done");
	}
	sort->api_phase = API_OUTPUT_RECS;
    }
    else if (sort->api_phase != API_OUTPUT_RECS)
	api_error(sort, NSORT_INVALID_ARGUMENT);

    while (client_space > 0)
    {
	/* Wait for the i/o sproc to fill in the buf&size and queue the write
	 */
	while (aiocb->aio_errno != EINPROGRESS && !OUT.end_io_output)
	    ordnap(1, "return_recs more output");
	if (sort->n_errors != 0)
	    return (sort->last_error);

	left = aiocb->aio_nbytes - aiocb->runio_curpos;
	if (client_space < /* <= */ left)
	{
	    memmove(recs, (byte *) aiocb->aio_buf + aiocb->runio_curpos,
			  client_space);
	    recs = (void *) ((byte *) recs + client_space);
	    aiocb->runio_curpos += client_space;
	    break;
	}
	else
	{
	    /* The client's remaining space for records exceeds
	     * the remaining amount of space in aio. Mark this i/o as completed
	     * and wait for the next aio to be initialized.
	     */
	    memmove(recs, (byte *) aiocb->aio_buf + aiocb->runio_curpos, left);
	    client_space -= left;
	    recs = (void *) ((byte *) recs + left);
	    aiocb->runio_curpos += left;
	    aiocb->aio_ret = aiocb->runio_curpos;
	    if (aiocb->runio_final)
	    {
		aiocb->aio_errno = 0;	/* clear EINPROGRESS (must be last) */
		break;
	    }
	    ASSERT(aiocb->runio_curpos == aiocb->aio_nbytes);
	    /*aiocb->runio_curpos = aiocb->aio_nbytes;*/
	    aiocb->aio_errno = 0;	/* clear EINPROGRESS (must be last) */
	}
    }

    *n_recs = ((byte *) recs - orig_recs) / RECORD.length;
    if (*n_recs == 0)
	api_sort_done(sort);
    return (NSORT_NOERROR);
}

/* do_cancel	- tell the io and sorting sprocs to stop now
 *
 *	This is called ONLY from a client sproc
 */
void do_cancel(sort_t *sort)
{
    /* Set these 'sort is done' flags which control sort_{input,output}'s loops 
     */
    OUT.end_sort_output = TRUE;
    if (sort->two_pass)
	IN.part_final = IN.runs_flushed * IN_RUN_PARTS;
    else
	IN.part_final = IN.part_taken;
    IN.end_of_input = TRUE;
    OUT.end_io_output = TRUE;

    /* wait for the external_io sproc to wait for the sorting sprocs to finish
     */
    if (!sort->internal && sort->go_sprocs && sort->client_pid == getpid())
	while (sort->input_phase || sort->output_phase)
	    ordnap(1, "cancel");
}

/* Nsort_cancel - cancel an external sort in progress.
 *
 *      This call cancels any sort in progress and resets the sort definition.
 *	The sort context may then be used to define and execute another sort.
 *
 *	This or free_context must always (?) be called to clear an error
 */
nsort_msg_t nsort_cancel(nsort_t context)
{
    sort_t	*sort;

    API_SETUP;

    if (sort->n_errors == 0)
    {
	sort->last_error = NSORT_CANCELLED;
	sort->n_errors = 1;
    }

    do_cancel(sort);

    reset_definition(sort);

    return (NSORT_NOERROR);
}


/* Nsort_get_stats - get sort statistics.  (To be defined.)
 */
nsort_msg_t nsort_get_stats(nsort_t context)
{}


/* Nsort_free_mem - free sort memory (for records or record pointers) for the
 *                  associated sort context.  The sort definition associated
 *                  with the sort context is left intact.  If the context is
 *                  used to perform another sort, new sort memory will be
 *                  dynamically allocated.
 */
nsort_msg_t nsort_free_mem(nsort_t context)
{
    sort_t	*sort;

    API_SETUP;
    if (sort->api_phase == API_ERROR)
	return (NSORT_ERROR_IGNORED);

    free_resources(sort);

    return (NSORT_NOERROR);
}


/* Nsort_free_context - free the given sort context.
 */
nsort_msg_t nsort_free_context(nsort_t context)
{
    sort_t	*sort;

    API_SETUP;
    if (sort->api_phase == API_ERROR)	/* XXX errors OK?? */
	return (NSORT_ERROR_IGNORED);

#if 0
    ASSERT(sort->input_phase == FALSE && sort->output_phase == FALSE);
#else
    /* wait for the external_io sproc to wait for the sorting sprocs to finish
     */
    if (!sort->internal && sort->go_sprocs)
	while (sort->input_phase || sort->output_phase)
	{
	    if (Print_task)	/* XXX api shouldn't write to stderr? */
		fprintf(stderr, "free_context napping till sprocs are done\n");
	    sginap(10);
	}
#endif

    sort_free(sort);

   return (NSORT_NOERROR);
}

/* parse the [single?] sort definition statementin the null-terminated string
 */
nsort_msg_t nsort_parse_string(nsort_t context, const char *str)
{
    sort_t	*sort;

    API_SETUP;
    if (sort->api_phase == API_ERROR) 
	return (NSORT_ERROR_IGNORED);

    parse_string(sort, str, "");

    return (NSORT_NOERROR);
}

/*
 * Error handling
 *	When nsort detects a fatal error or a non-fatal warning condition 
 *	it invokes a message handling function.
 *	If a fatal error has occurred the library cancels the sort
 *	before calling the message handler.
 *	The default function prints a string onto stderr.
 *	An application assigns its own message handler via nsort_msg_handler()
 *	If the message returns (rather than, e.g. executing a longjmp()) the
 *	sort api call returns the integer value of the msg.
typedef void	(*nsort_msg_funcp)(context, void *client_ptr,
					    nsort_msg_t msg,
					    char **argv);
 */
void nsort_msg_handler(nsort_t context, nsort_msg_funcp func, void *client_ptr)
{}

char *nsort_msg_string(nsort_t context, nsort_msg_t msg)
{}
