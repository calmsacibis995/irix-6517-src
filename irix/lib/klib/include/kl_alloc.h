#ident "$Header: "

/** liballoc wrapper function prototypes
 **/

/* Memory block allocator. Returns a pointer to an allocated block
 * of size bytes. In case of error, a NULL pointer will be returned
 * and klib_error will be set to indicate exactly what error occurred.
 * Note that the flag value will determine if the block allocated is 
 * temporary (can be freed via a call to kl_free_temp_blks()) or 
 * permenant (must be freed with a call to kl_free_block())..
 */
#ifdef ALLOC_DEBUG
typedef k_ptr_t (*klib_block_alloc_func) (
	int32       /* size of block required */,
	int32       /* flag value (B_TEMP/B_PERM) */,
	caddr_t	*	/* return address */);
#else
typedef k_ptr_t (*klib_block_alloc_func) (
	int32       /* size of block required */,
	int32       /* flag value (B_TEMP/B_PERM) */);
#endif

/* Memory block reallocator. Returns a pointer to a block of new_size
 * bytes. In case of error, a NULL pointer will be returned and 
 * klib_error will be set to indicate exactly what error occurred.
 * Note that the flag value will determine if the block allocated is 
 * temporary (can be free via a call to kl_free_temp_blks()) or 
 * permenant.
 */
#ifdef ALLOC_DEBUG
typedef k_ptr_t (*klib_block_realloc_func) (
	k_ptr_t     /* pointer to block to realloc */,
	int32       /* size of new block required */,
	int32       /* flag value (K_TEMP/K_PERM) */,
	caddr_t *   /* return address */);
#else
typedef k_ptr_t (*klib_block_realloc_func) (
	k_ptr_t     /* pointer to block to realloc */,
	int32       /* size of new block required */,
	int32       /* flag value (B_TEMP/B_PERM) */);
#endif

/* Memory block duplicator. Returns a pointer to a block that is
 * a copy of the block passed in via pointer. In case of error, a 
 * NULL pointer will be returned and klib_error will be set to 
 * indicate exactly what error occurred. Note that the flag value 
 * will determine if the block allocated is temporary (will be freed 
 * via a call to kl_free_temp_blks()) or permenant. Note that this
 * function is only supported when liballoc is used (there is no
 * way to tell the size of a malloced block.
 */
#ifdef ALLOC_DEBUG
typedef k_ptr_t (*klib_block_dup_func) (
	k_ptr_t     /* pointer to block to dup */,
	int32       /* flag value (B_TEMP/B_PERM) */,
	caddr_t *   /* return address */);
#else
typedef k_ptr_t (*klib_block_dup_func) (
	k_ptr_t     /* pointer to block to dup */,
	int32       /* flag value (B_TEMP/B_PERM) */);
#endif

/* Allocates a block large enough to hold a string (plus the terminating
 * NULL character).
 */
#ifdef ALLOC_DEBUG
typedef k_ptr_t (*klib_str_to_block_func) (
	char *      /* pointer to character string */,
	int32       /* flag value (B_TEMP/B_PERM) */,
	void *      /* return address */);
#else
typedef k_ptr_t (*klib_str_to_block_func) (
	char *      /* pointer to character string */,
	int32       /* flag value (B_TEMP/B_PERM) */);
#endif

/* Frees blocks that were previously allocated. 
 */
typedef void (*klib_block_free_func) (
	k_ptr_t     /* pointer to block */);

/* liballoc wrapper function table structure
 */
typedef struct klib_functions {
	int							flag;		   /* Functions initialized?     */
	klib_block_alloc_func		block_alloc;   /* Returns pointer to block   */
	klib_block_realloc_func		block_realloc; /* Returns pointer to new blk */
	klib_block_dup_func    		block_dup; 	   /* Returns pointer to new blk */
	klib_str_to_block_func    	str_to_block;  /* Returns pointer to new blk */
	klib_block_free_func		block_free;    /* Frees memory block 		 */
} klib_functions_t;

/* Macros for accessing functions in klib_functions table
 */
#define KL_BLOCK_ALLOC()		(klib_functions.block_alloc)
#define KL_BLOCK_REALLOC()		(klib_functions.block_realloc)
#define KL_BLOCK_DUP()			(klib_functions.block_dup)
#define KL_STR_TO_BLOCK()		(klib_functions.str_to_block)
#define KL_BLOCK_FREE()			(klib_functions.block_free)

extern klib_functions_t klib_functions;

/** Function prototypes for liballoc wrapper functions
 **/

#ifdef ALLOC_DEBUG

void *kl_get_ra();

#define kl_alloc_block(size, flags) _kl_alloc_block(size, flags, kl_get_ra())

void *_kl_alloc_block(
	int         /* block size */,
	int         /* flags (K_TEMP/K_PERM) */,
	void *      /* return address */);

#define kl_realloc_block(b, new_size, flags) \
		_kl_realloc_block(b, new_size, flags, kl_get_ra())

void *_kl_realloc_block(
	void *	    /* pointer to block to reallocate */, 
	int 		/* new_size */, 
	int 		/* flags */,
	void *      /* return address */);

#define kl_dup_block(b, flags) _kl_dup_block(b, flags, kl_get_ra())

void *_kl_dup_block(
	void *		/* pointer to block to dup */, 
	int 		/* flag */,
	void *      /* return address */);

#define kl_str_to_block(s, flags) _kl_str_to_block(s, flags, kl_get_ra())

void *_kl_str_to_block(
	char * 		/* pointer to string */, 
	int 		/* flags */, 
	void *		/* return address */);

#else

void *kl_alloc_block(
	int         /* block size */,
	int         /* flags (K_TEMP/K_PERM) */);

void *kl_realloc_block(
	void *	    /* pointer to block to reallocate */, 
	int 		/* new_size */); 

void *kl_dup_block(
	void *		/* pointer to block to dup */, 
	int 		/* flag */);

void *kl_str_to_block(
	char * 		/* pointer to string */, 
	int 		/* flags */); 

#endif /* ALLOC_DEBUG */

void kl_free_block(
	void *      /* block pointer */);

/* Function prototypes for liballoc wrapper funcs that get loaded
 * into the function table. These functions are then called by the
 * API functions (e.g., kl_alloc_block()).
 */
#ifdef ALLOC_DEBUG
k_ptr_t kl_block_alloc_func(
	int			/* size of block */, 
	int 		/* flags (B_TEMP/B_PERM) */,
	caddr_t *   /* return address */);
#else
k_ptr_t kl_block_alloc_func(
	int			/* size of block */, 
	int 		/* flags (B_TEMP/B_PERM) */);
#endif

#ifdef ALLOC_DEBUG
k_ptr_t kl_block_realloc_func(
	k_ptr_t     /* pointer to block to realloc */,
	int			/* size of block */, 
	int 		/* flags (B_TEMP/B_PERM) */,
	caddr_t *   /* return address */);
#else
k_ptr_t kl_block_realloc_func(
	k_ptr_t     /* pointer to block to realloc */,
	int			/* size of block */, 
	int 		/* flags (B_TEMP/B_PERM) */);
#endif

#ifdef ALLOC_DEBUG
k_ptr_t kl_block_dup_func(
	k_ptr_t     /* pointer to block to dup */,
	int 		/* flags (B_TEMP/B_PERM) */,
	caddr_t *   /* return address */);
#else
k_ptr_t kl_block_dup_func(
	k_ptr_t     /* pointer to block to dup */,
	int 		/* flags (B_TEMP/B_PERM) */);
#endif

#ifdef ALLOC_DEBUG
k_ptr_t kl_str_to_block_func(
	char *      /* pointer to string */,
	int 		/* flags (B_TEMP/B_PERM) */,
	void *      /* return address */);
#else
k_ptr_t kl_str_to_block_func(
	char *      /* pointer to string */,
	int 		/* flags (B_TEMP/B_PERM) */);
#endif

void kl_block_free_func(
	k_ptr_t 	/* pointer to block to be freed */);

void kl_init_mempool(
	int 		/* page header count */, 
	int 		/* block header count */, 
	int 		/* page count */);

void kl_free_mempool();

