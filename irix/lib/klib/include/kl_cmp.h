#ident "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/include/RCS/kl_cmp.h,v 1.3 1999/11/30 19:24:52 lucc Exp $"

/* bump up CMP_VERSION is you change NUM_BUCKETS. 
   We use the hash value from the index file */
#define CMP_VERSION  21942
#define NUM_BUCKETS  65535

/*
 * This structure defines a page table entry, what each value will
 * contain.  Since these can be cached or uncached, we have a flags
 * variable to specify this.
 */
typedef struct _ptableentry {
	int          flags;                /* flags for page in cache   */
	int          length;               /* length of page            */
	int          cached;               /* cached (1 = yes, cached)  */
	kaddr_t      addr;                 /* addr of page              */
	char         *data;                /* data in page              */
	struct _ptableentry *next;         /* ptr to next dump page     */
	struct _ptableentry *prev;         /* ptr to prev dump page     */
	struct _ptableentry *nextcache;    /* ptr to next cached page   */
	struct _ptableentry *prevcache;    /* ptr to prev cached page   */
} ptableentry;

/*
 * This structure will hold the directory header entries for each
 * directory entry in the compressed core dump.  We need this more
 * for getting the page offsets initially into our index table.
 */
typedef struct _dir_entry_header {
	uint addr_hi;   /* high word at page's starting physaddr        */
	uint addr_lo;   /* low word at page's starting physaddr         */
	int  length;    /* compressed length of page                    */
	int flags;  	/* flags: identify the memory information       */
} dir_entry_header;

/*
 * This is for the page table index from the compressed core dump.
 * This is separate from the page table entries because these are
 * simply addresses off of the compressed core dump, and not the
 * actual data from the core dump.  If we hash these values, we gain
 * a lot of performance because we only have 1 to search for the
 * page data, 1 to search for the index, and return if both searches
 * failed.
 */
typedef struct _ptableindex {
	dir_entry_header dir;        /* directory entry of page         */
	kaddr_t addr;                /* address of page offset          */
	kaddr_t coreaddr;            /* address of page in core         */
	unsigned int hash;           /* hash value for this index item  */
	struct _ptableindex *next;   /* next pointer                    */
} ptableindex;

extern ptableentry *cache_head;
extern ptableentry *cache_tail;
#define CMP_END(p) (((p)->dir.addr_lo & 0x0fff) & DUMP_END)

#define KL_REPORT_FLAG	1

/* Definitions for compressed cached reads.  I've recently lowered
 * these ... If they need to be increased later, I'll do so.
 */
#define CMP_HIGH_WATER_MARK 25
#define CMP_LOW_WATER_MARK  10

#define CMP_VM_CACHED   0x01
#define CMP_VM_UNCACHED 0x02
