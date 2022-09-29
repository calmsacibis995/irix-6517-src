#ident "$Header: "

/* The string table structure
 * 
 * String space is allocated from 4K blocks which are allocated 
 * as needed. The first four bytes of each block are reserved so 
 * that the blocks can be chained together (to make it easy to free 
 * them when the string table is no longer necessary).
 */
typedef struct string_table_s {
	int             num_strings;
	k_ptr_t         block_list;
} string_table_t;

#define NO_STRINGTAB    0
#define USE_STRINGTAB   1

/**
 ** Function prototypes
 **/

/* Initialize a string table. Depending on the value of the flag
 * parameter, either temporary or permenent blocks (see kl_alloc.h)
 * will be used. Upon success, a pointer to a string table will be
 * returned. Otherwise, a NULL pointer will be returned.
 */
string_table_t *init_string_table(
	int				/* flag (K_TEMP/K_PERM)*/);

/* Free all memory blocks allocated for a particular string table 
 * and then free the table itself.
 */
void free_string_table(
	string_table_t*	/* pointer to string table */);

/* Search for a string in a string table. If the string does not 
 * exist, allocate space from the string table and add the string.
 * In either event, a pointer to the string (from the table) will
 * be returned.
 */
char *get_string(
	string_table_t*	/* pointer to string table */, 
	char*			/* string to get/add from/to string table */, 
	int				/* flag (K_TEMP/K_PERM)*/);
