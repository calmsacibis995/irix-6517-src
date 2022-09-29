#ident "$Header: "

/** 
 ** Block Memory Allocator 
 **/

extern int alloc_debug;

/* Flags that determine if a block is temp or perm. Note that these values
 * are the same as the KLIB flags K_TEMP and K_PERM. These defines are 
 * here so that applicatons using only the liballoc library need not 
 * include klib.h.
 */
#define B_FREE 0
#define B_TEMP 1
#define B_PERM 2

#define ALLOCFLG(flags) ((flags & B_PERM) ? B_PERM : B_TEMP)

void *get_ra();

/**
 ** block allocation operation function prototypes
 **/
int init_mempool(int, int, int);

void free_mempool();

#ifdef ALLOC_DEBUG
void * _alloc_block(	
	int			/* size of block to allocate */, 
	int			/* flag (K_TEMP/K_PERM) */, 
	void * 	    /* return address */);

#define alloc_block(size, flag) _alloc_block(size, flag, get_ra())

void * _realloc_block(
	void *	    /* pointer to block to reallocate */,
	int 		/* size of new block */,
	int         /* flag (K_TEMP/K_PERM) */,
	void * 	    /* return address */);

#define realloc_block(b, new_size, flag) \
	_realloc_block(b, new_size, flag, get_ra())

void * _dup_block(
	void *	    /* pointer to block to dup */, 
	int			/* flag (K_TEMP/K_PERM) */,
	void * 	    /* return address */);

#define dup_block(b, flag) _dup_block(b, flag, get_ra())

void * _str_to_block(
	char *		/* pointer to string to copy */,
	int         /* flag (K_TEMP/K_PERM) */,
	void * 	    /* return address */);

#define str_to_block(b, flag) _str_to_block(b, flag, get_ra())
#else
void * alloc_block(
	int			/* size of block to allocate */, 
	int			/* flag (K_TEMP/K_PERM) */); 

void * realloc_block(
	void *	    /* pointer to block to reallocate */,
	int 		/* size of new block */,
	int         /* flag (K_TEMP/K_PERM) */);

void * dup_block(
	void *	    /* pointer to block to dup */, 
	int			/* flag (K_TEMP/K_PERM) */);

void * str_to_block(
	char *		/* pointer to string to copy */,
	int         /* flag (K_TEMP/K_PERM) */);
#endif

void free_block(
	void *	    /* address of block to free */);

void free_temp_blocks();

int is_temp_block(
	void *		/* address of block to check */);
