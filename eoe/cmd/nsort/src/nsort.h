/*
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.
 *
 *	Nsort.h	- API for the nsort subroutine library
 *	$Ordinal-Id$
 *	$Revision: 1.1 $
 */
#ifndef _NSORT_H_
#define _NSORT_H_

typedef int nsort_t;
typedef int nsort_msg_t;


/* Nsort_define - allocate and initiailize a sort based on the
 *		  sort definition statements, input file names,
 *		  and 'nsort (1sgi)' command line options specified in def
 */
extern nsort_t nsort_define(const char *def);

/* Nsort_alloc_context - allocate a sort context.
 *	options		see list of options below 
 *	size_delim	 either record size or delimiter character 
 */

extern nsort_t nsort_alloc_context(nsort_t options, int size_delim);

/* values for the options argument to nsort_alloc_context():
 */

/* internal or external sort?	XXX Not needed: set by nsort_internal/release_*
 */
#define NSORT_INTERNAL          0x00000000
#define NSORT_EXTERNAL          0x00000001

/* are records fixed-length, variable (preceded by 2-byte length),
 * or delimited by a given character?   If fixed, the size_delim argument
 * contains the record size.  If delimited, the size_delim argument contains
 * the delimiter character.
 */
#define NSORT_FIXED             0x00008000
#define NSORT_DELIMITED         0x00000002
#define NSORT_VARIABLE          0x00000004

/* is the method for producing runs radix or merge-based?  If neither is
 * selected, one will be selected for you.
 */
#define NSORT_RADIX             0x00000008
#define NSORT_MERGE             0x00000010

/* should the keys values be hashed, usually allowing for faster deletion of
 * duplicates (or summarization), but not returning records in key order?
 */
#define NSORT_HASH              0x00000020

/* should records with duplicate keys be deleted?  This option does not need
 * to be specified if field summarization is defined.
 */
#define NSORT_DELETE_DUP        0x00000040

/* sort records by moving pointers or records?  Record sorts are only possible
 * with fixed-length records smaller than 104 bytes, and tend to be faster
 * for small records (16 bytes or smaller).  If neither option is specified,
 * one will be chosen based on the record type and size.
 */
#define NSORT_POINTER           0x00000080
#define NSORT_RECORD            0x00000100

/* should sort memory and extrat sort sprocs be automatically be
 * freed/terminated after each sort?
 */
#define NSORT_AUTO_FREE         0x00000200

/* should the /usr/lib/nsort.params file be read to obtain default temporary
 * file information?
 */
#define NSORT_READ_PARAM        0x00000400


/* defines for the field and key definition routines:
 */
#define NSORT_UNSIGNED          0x00000000  /* unsigned integer key */
#define NSORT_SIGNED            0x00000800  /* signed integer key */
#define NSORT_ASCEND            0x00000000  /* sort in ascending key order */
#define NSORT_DESCEND           0x00001000  /* sort in descending key order */
#define NSORT_CHAR_FIXED        0x00000000  /* character key is fixed length */
#define NSORT_CHAR_DELIMITED    0x00002000  /* character key is delimited */
#define NSORT_CHAR_DECIMAL	0x00004000  /* interpret key as a decimal string */


/* Nsort_min_rec - define minimum record size of a sort.
 *	size	the minimum size of delimited or variable records
 */
extern nsort_msg_t nsort_min_rec(nsort_t context, int size);


/* Nsort_max_rec - define maximum record size of a sort.
 *
 *	size	the maximum size of delimited or variable records
 */
extern nsort_msg_t nsort_max_rec(nsort_t context, int size);


/* Nsort_int_key - define binary integer key
 *
 *      This function can be called multiple times or with other key
 *      definition functions to define multiple keys.  The order of
 *      key definitions defines the order of the keys for the sort.
 *      (The first key definition defines the primary key.)
 *		flags	NSORT_SIGNED?, NSORT_DESCEND?
 *		offset	bytes from beginning of record to key
 *		size	number of bytes: 1, 2, 4 or 8
 */
extern nsort_msg_t nsort_int_key(nsort_t context, int flags, int offset, int size);


/* Nsort_float_key - define floating point key
 *
 *      This function can be called multiple times or with other key
 *      definition functions to define multiple keys.  The order of
 *      key definitions defines the order of the keys for the sort.
 *      (The first key definition defines the primary key.)
 *		flags	NSORT_DESCEND?
 *		offset	bytes from beginning of record to key
 *		size	number of bytes: 4 or 8
 */
extern nsort_msg_t nsort_float_key(nsort_t context, int flags, int offset, int size);


/* Nsort_char_key - define character key
 *
 *      This function can be called multiple times or with other key
 *      definition functions to define multiple keys.  The order of
 *      key definitions defines the order of the keys for the sort.
 *      (The first key definition defines the primary key.)
 *
 *      Character keys are sorted as unsigned byte strings.
 *		flags		NSORT_DESCEND?NSORT_CHAR_FIXED or NSORT_CHAR_DELIMITED,
 *		offset		bytes from beginning of record to key
 *		size_delim	size of character string, or key delimiter 
 *		pad		pad character to resolve comparisons of different
 *				length character strings
 */
extern nsort_msg_t nsort_char_key(nsort_t context, int flags, int offset, int size_delim, char pad);


/* Nsort_input_size - define size of sort input.
 *
 *      This call helps nsort allocate the correct amount of process memory.
 *	It is useful only for external sorts.
 *	size	maximum number of bytes in sort input
 */
extern nsort_msg_t nsort_input_size(nsort_t context, long long size);         

/* Nsort_input_count - set the maximum size of the input data set
 *
 *      This call helps nsort allocate the correct amount of process memory
 *      It is useful only for external sorts.
 *	maximum number of records to be sorted
 */
extern nsort_msg_t nsort_input_count(nsort_t context, long long count);


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
 *		flags	NSORT_SIGNED?
 *		offset	
 *		size	number of bytes: 4 or 8
 */
extern nsort_msg_t nsort_int_sum(nsort_t context, int flags, int offset, int size);


/* Nsort_temp_file - define temporary file for an external sort
 */
extern nsort_msg_t nsort_temp_file(nsort_t context,
                int xfer_size,    /* transfer size */
                int xfer_count,   /* max simultaneous transfers per temp file */
                int file_count,   /* number of files */
                char *file[]);/* argv-style list of temp file names */


/* Nsort_procs - define the number of sorting sprocs for the sort.
 *
 *      For the first version, this call can only be made for external sorts.
 *      In the absence of this call, the number of sorting sprocs will be the
 *      number of active processors.
 *	context		context returned by nsort_alloc_context()
 *	n_procs		define the number of processors to be assumed
 */
extern nsort_msg_t nsort_procs(nsort_t context, int n_procs);


/* Nsort_mem_limit - define limit on process memory to be used by sort.
 *
 *	limit	memory limit in bytes 
 */
extern nsort_msg_t nsort_mem_limit(nsort_t context, long limit);

/*
Internal Sort Routines
----------------------
The nsort_internal() subroutine sorts records in memory according to the
sort defined in the given sort context.  It is the only call needed to
perform a defined internal sort.
	context		context returned by nsort_alloc_context()
	n_recs		number of records input, output
	first_rec	pointer to first record for fixed-length, sequential array.
			If NULL, pointer array points to input records
	void **rec	pointer array
*/

int nsort_internal(nsort_t context, unsigned *n_recs, void *first_rec, void **rec);

nsort_msg_t nsort_release_recs(nsort_t context, unsigned n_recs, void *recs);
nsort_msg_t nsort_release_ptrs(nsort_t context, unsigned n_recs, void **ptrs);
nsort_msg_t nsort_return_recs(nsort_t context, unsigned *n_recs, void *recs);
nsort_msg_t nsort_return_ptrs(nsort_t context, unsigned *n_recs, void **ptrs);
nsort_msg_t nsort_end_input(nsort_t context);
#if 0

External Sort Routines
----------------------
Once an external sort context has been defined, sorting can take place
by iteratively releasing records to the sort routines, declaring the end
of released records with a call to nsort_end_input(), and then iteratively
returning the sorted records.

Records may be released to the nsort routines using nsort_release_ptrs.  The
number of records and an array of pointers to those records are passed as
arguments (in addition to the sort context):

A single sort MAY call both nsort_release_ptrs and nsort_release_recs()

nsort_release_ptrs(context,
                   unsigned n_recs, /* number of records (and pointers) */
                   void     **rec); /* array of pointers to released records */

/*
For fixed length records where the records to be released are stored
sequentially in memory, the nsort_release_recs() routine may alternately be
used to release the records.  In this case a pointer to the first record
to be released is used instead of the array of record pointers used in
nsort_release_ptrs():
*/

nsort_release_recs(context,
                   unsigned n_recs, /* number of sequentially stored records */
                   void     *rec)   /* pointer to first record to be released */

extern int nsort_end_input(context);

/*
Records can be returned in sorted order by using the nsort_return_ptrs()
routine.  The second argument is a pointer to maximum number of record
pointers that can be returned.  This value is overwritten by
nsort_return_ptrs() with the actual number of record pointers returned.
The third argument is the address of a pointer array where the returned
record pointers can be written:

A single sort may NOT call both nsort_return_ptrs and nsort_return_recs()
*/
nsort_return_ptrs(context,
                  unsigned *n_recs,  /* max number of pointer array elements is
                                        passed on input, the number of record
                                        pointers written is returned */
                  void     **rec);   /* array of pointers to returned records */

/*

The caller should copy the pointed-to records to its own memory area before
initiating another call to nsort_return_ptrs().  When the number of
pointers returned is 0, the end of the returned records has been reached.

If the records are fixed-length, nsort_return_recs() can be used to have
the returned records written into sequential memory:
*/

nsort_return_recs(context,
                  unsigned *n_recs,  /* max number of record array elements is
                                        passed on input, the number of record
                                        array elements written is returned */
                  void       *rec);  /* pointer to record array to be filled
                                        with returned records */

#endif

/*
Sort Cleanup
------------
*/


/* Nsort_cancel - cancel an external sort in progress.
 *
 *      This call cancels any sort in progress and resets the sort definition.
 *	The sort context may then be used to define and execute another sort.
 */
extern nsort_msg_t nsort_cancel(nsort_t context);


/* Nsort_get_stats - get sort statistics.  (To be defined.)
 */
extern nsort_msg_t nsort_get_stats(nsort_t context);


/* Nsort_free_mem - free sort memory (for records or record pointers) for the
 *                  associated sort context.  The sort definition associated
 *                  with the sort context is left intact.  If the context is
 *                  used to perform another sort, new sort memory will be
 *                  dynamically allocated.
 */
extern nsort_msg_t nsort_free_mem(nsort_t context);


/* Nsort_free_context - free the given sort context.
 */
extern nsort_msg_t nsort_free_context(nsort_t context);


/* parse the sort definition statement[s] in the null-terminated string
 */
extern nsort_msg_t nsort_parse_string(nsort_t context, const char *str);

/*
 * Error handling
 *	When nsort detects a fatal error or a non-fatal warning condition 
 *	it invokes a message handling function.
 *	If a fatal error has occurred the library cancels the sort
 *	before calling the message handler.
 *	The default function prints a string onto stderr.
 *	A sort may assign its own message handler via nsort_msg_handler()
 *	If the handler returns (rather than, e.g. executing a longjmp()) the
 *	sort api call returns the integer value of the msg.
 */
typedef void	(*nsort_msg_funcp)(nsort_t context, void *client_ptr,
						    nsort_msg_t msg,
						    char **argv);
extern void	nsort_msg_handler(nsort_t context, nsort_msg_funcp, void *client_ptr);
extern char	*nsort_msg_string(nsort_t context, nsort_msg_t msg);

#endif /* !_NSORT_H_ */
