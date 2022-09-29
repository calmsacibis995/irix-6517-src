#ident "$Header: "

/* Private header file for liballoc. Note that it is installed in the
 * $ROOT/usr/include/klib directory for use by ICRASH (which contains
 * some alloc debugging commands). The alloc.c module includes the
 * local copy of the header file, not the one in the include/klib
 * directory.
 */

/* Block Memory Allocator 
 * ----------------------
 *
 * Blocks of memory are allocated from an array of buckets. Requests
 * for blocks of memory larger than one page are allocated in
 * full-page increments -- to the next largest page. Such blocks are
 * not allocated from a bucket. Instead, they are are malloced
 * directly from the system. The last bucket in buckt table controls
 * these oversize blocks while they are active. Oversized blocks may
 * be freed back to the system when they are no longer needed, or
 * they can be left linked into the pagelist of the last bucket (yet
 * to be implemented). Note that with oversized blocks, a "page"
 * consists of more than one page (IO_NBPC) of memory.
 *
 * Free blocks (for each page) are are linked together using a block
 * header structure. For blocks on a page's freelist, the beginning
 * portion of the block is used as a header (to link the blocks on
 * the freelist). For blocks that are allocated on a temporary basis
 * (B_TEMP), a block header is obtained from the mempool.blkhdrs
 * freelist and placed on a doubly linked list (temp_blks). This
 * allows us to free all temporary blocks in the event of a
 * longjmp() (that occurs when Ctrl-C is pressed). Perminent blocks
 * are not linked together in any way. We COULD walk the currently
 * active pages and determine exactly which blocks are allocated if
 * we wanted to, but we wouldn't be able to determine who owns the
 * blocks. To do this would require too much memory for the
 * aditional block header structs.
 */

/* Block header
 */
typedef struct block_s {
	struct block_s 	*next;	   /* next block on list */
	struct block_s 	*prev;     /* previous block on list */
	void   			*addr;	   /* pointer to block of memory */
#ifdef ALLOC_DEBUG
    int              flag;     /* flag indicating state of block */
    struct page_s   *page;     /* pointer to page block is part of */
	void   			*alloc_pc; /* program counter of calling function */
#endif
} block_t;

/* Page header
 */
typedef struct page_s {
	struct page_s	*next;    /* pointer to next page */
	struct page_s	*prev;    /* pointer to previous page */
	void   			*addr;	  /* start address of page */
	block_t			*blklist; /* linked list of free blocks from this page */
	unsigned	 	 blksz;	  /* size of blocks page is broken up into */
	unsigned short 	 nblocks; /* number of blocks per page */
	unsigned short 	 nfree;	  /* number of size blocks free on this page */
	unsigned short 	 state;	  /* current state of this page (see below) */
	unsigned short 	 index;	  /* index of bucket page is allocated to */
	struct page_s	*hash;	  /* has table forward pointer */
} page_t;

/* Values for state field
 */
#define P_FREE  0	/* Page header on freelist -- no page allocated */
#define	P_AVAIL 1	/* Page allocated, currently in memory pool */
#define P_INUSE 2	/* Page in use (on bucket pagelist) */
#define P_OVRSZ 3	/* Memory not from memory pool (oversized block) */

/* Memory allocation stats 
 */
typedef struct stat_s {
	unsigned	alloc;	/* total allocations */
	unsigned 	free;	/* total frees */
	unsigned	high;   /* high water mark -- max allocated at one time */
} stat_t;

/* Memory bucket
 */
typedef struct bucket_s {
	page_t *pagelist;  /* linked list of pages */
	int		blksize;   /* size of each block in this bucket  */
	int		npages;	   /* Number of pages allocated to this bucket */
	int		nblks;	   /* total blocks allocated for this bucket */
	int		freeblks;  /* number of free blocks in bucket */
	stat_t	stats;	   /* memory allocation stats for bucket */
} bucket_t;
#define s_alloc stats.alloc
#define s_free stats.free
#define s_high stats.high

#define MAX_MEMCHUNKS	1022

typedef struct memchunk_s {
	struct memchunk_s	*next;
	int					 count;
	void				*ptr[MAX_MEMCHUNKS];
} memchunk_t;

/* Memory pool
 *
 *   Available memory pages are held in the mempool structure. In
 *   adition to the linked list of free pages, the mempool struct also
 *   contains statistical information on page memory utilization.
 */
typedef struct mempool_s {
	page_t 	     *pgs;     	    /* linked list of available pages */
	page_t	  	 *ovrszpgs;	    /* linked list of oversized blocks */
	int		  	  npgs;	        /* Total number of pages allocated */
	int		  	  free_pgs;     /* number of free pages */
	stat_t	  	  stats;	    /* page utalization stats */
	page_t 	     *pghdrs;  	    /* list of available page headers */
	int 		  npghdrs;	    /* number of page headers allocated */
	int			  free_pghdrs;  /* number of available page headers */
    block_t	     *blkhdrs;      /* free block headers */
	int    	      nblkhdrs;     /* number of block headers allocated */
	int    		  free_blkhdrs; /* number of available block headers */
	memchunk_t   *memchunks;    /* mem chunks to be freed by free_mempool() */
} mempool_t;
#define m_alloc stats.alloc
#define m_free stats.free
#define m_high stats.high

/* Defines for controlling allocation of page/block resources
 */
#define PAGE_INIT 10	 /* number of pages to allocate at startup */
#define PAGE_ALLOC 1     /* number of pages to allocate when low */
#define PGHDR_INIT 20    /* number of page headers to allocate at startup */
#define PGHDR_ALLOC 10   /* number of page headers to allocate when low */
#define BLKHDR_INIT 32   /* number of block headers to allocate at startup */
#define BLKHDR_ALLOC 16  /* number of block headers to allocate when low */

/* Defines that relate to the page hash table
 */
#define PHASHSIZE 32
#define PHASH(X) (&phash[((X)>>12)%PHASHSIZE])
#define NBUCKETS 11
#define OVRSZBKT (NBUCKETS - 1)

/* External variables
 */
extern page_t *phash[];
extern bucket_t bucket[];
extern mempool_t mempool;
extern block_t *temp_blks;        /* List of block headers for K_TEMP blocks */
extern int bucket_size[];
#ifdef ALLOC_DEBUG
extern block_t *perm_blks;
#endif
