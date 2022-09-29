#ident  "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#ifdef ALLOC_DEBUG
#include <assert.h>
#endif
#include <klib/alloc.h>
#include "alloc_private.h"

/* Local variables
 */
page_t *phash[PHASHSIZE];
bucket_t bucket[NBUCKETS];
int bucket_size[] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, -1};
mempool_t mempool;
block_t *temp_blks = NULL;        /* List of block headers for B_TEMP blocks */
#ifdef ALLOC_DEBUG
block_t *perm_blks = NULL;
#endif

#ifdef ALLOC_DEBUG
int alloc_debug = 0;
FILE *alloc_fp = stderr;
#endif

/* List element header
 */
typedef struct element_s {
	struct element_s    *next;
	struct element_s    *prev;
} element_t;

/* Some useful macros
 */
#define ENQUEUE(list, elem) enqueue((element_t **)list, (element_t *)elem)
#define DEQUEUE(list) dequeue((element_t **)list)
#define REMQUEUE(list, elem) remqueue((element_t **)list, (element_t *)elem)

/* 
 * enqueue() -- Add a new element to the tail of doubly linked list.
 */
static void
enqueue(element_t **list, element_t *new)
{
	element_t *head;

	/* 
	 * If there aren't any elements on the list, then make new element the 
	 * head of the list and make it point to itself (next and prev).
	 */
	if (!(head = *list)) {
		new->next = new;
		new->prev = new;
		*list = new;
	}
	else {
		head->prev->next = new;
		new->prev = head->prev;
		new->next = head;
		head->prev = new;
	}
}

/* 
 * dequeue() -- Remove an element from the head of doubly linked list.
 */
static element_t *
dequeue(element_t **list)
{
	element_t *head;

	/* If there's nothing queued up, just return 
	 */
	if (!*list) {
		return((element_t *)NULL);
	}

	head = *list;

	/* If there is only one element on list, just remove it 
	 */
	if (head->next == head) {
		*list = (element_t *)NULL;
	}
	else {
		head->next->prev = head->prev;
		head->prev->next = head->next;
		*list = head->next;
	}
	head->next = 0;
	return(head);
}

/* 
 * remqueue() -- Remove specified element from doubly linked list.
 */
static void
remqueue(element_t **list, element_t *item)
{
	/* Check to see if item is first on the list
	 */
	if (*list == item) {
		if (item->next == item) {
			*list = (element_t *)NULL;
			return;
		}
		else {
			*list = item->next;
		}
	}

	/* Remove item from list
	 */
	item->next->prev = item->prev;
	item->prev->next = item->next;
}

/*
 * hold_signals() -- Hold signals in critical blocks of code
 */
static void
hold_signals()
{
	sighold((int)SIGINT);
	sighold((int)SIGPIPE);
}

/*
 * release_signals() -- Allow signals again
 */
static void
release_signals()
{
	sigrelse((int)SIGINT);
	sigrelse((int)SIGPIPE);
}

/*
 * add_phash() -- Put page header on phash[] hash table.
 */
static void
add_phash(page_t *page)
{
	page_t **ph, *p;

	ph = PHASH((unsigned)page->addr);

	if (p = *ph) {
		while(p) {
			if (p->hash) {
				p = p->hash;
			}
			else {
				break;
			}
		}
		p->hash = page;
	}
	else {
		*ph = page;
	}
	page->hash = (page_t *)NULL;
}

/*
 * rem_phash() -- Remove a page header from the phash[] hash table.
 */
static int
rem_phash(page_t *page)
{
	page_t **ph, *p;

	ph = PHASH((unsigned)page->addr);

	/* Get first page on hash list and see if it's the one we want
	 */
	if ((p = *ph) == page) {
		*ph = page->hash;
		page->hash = (page_t *)NULL;
		return(0);
	}
	
	/* Otherwise walk the hash list
	 */
	while(p) {
		if (p->hash == page) {
			p->hash = page->hash;
			page->hash = (page_t *)NULL;
			return(0);
		}
		else {
			p = p->hash;
		}
	}

	/* We should NEVER get here -- active pages MUST be on page 
	 * hash table.
	 */
	return(1);
}

/* 
 * add_memchunk_rec()
 */
memchunk_t *
add_memchunk_rec()
{
	memchunk_t *mcp;

	if (mcp = mempool.memchunks) {
		while (mcp->next) {
			mcp = mcp->next;
		}
		mcp->next = (memchunk_t *)memalign(IO_NBPC, sizeof(memchunk_t));
		mcp = mcp->next;
	}
	else {
		mempool.memchunks = 
			(memchunk_t *)memalign(IO_NBPC, sizeof(memchunk_t));
		mcp = mempool.memchunks;
	}
	bzero(mcp, sizeof(memchunk_t));
	return(mcp);
}

/*
 * add_memchunk()
 */
void
add_memchunk(void *ptr) 
{
	memchunk_t *mcp;

	if (mcp = mempool.memchunks) {
		while (mcp->next) {
			mcp = mcp->next;
		}
		if (mcp->count == MAX_MEMCHUNKS) {
			mcp = add_memchunk_rec();
		}
	}
	else {
		mcp = add_memchunk_rec();
	}
	mcp->ptr[mcp->count] = ptr;
	mcp->count++;
}

/*
 * find_page() -- See if page for addr is in phash[] hash table.
 * 				  If it is, return a pointer to the page header. 
 *				  Otherwise return NULL.
 */
static page_t *
find_page(void *addr)
{
	page_t *p;

	p = *(page_t **)PHASH((unsigned)addr);

	/* Search for page in hash queue 
	 */
	while(p) {
		if (((unsigned)addr >= (unsigned)p->addr) && 
				((unsigned)addr < ((unsigned)p->addr + IO_NBPC))) {
			break;
		}
		p = p->hash;
	}
	return(p);
}

/*
 * alloc_pghdrs() -- Allocate count number of page headers and put them on 
 *				  	 pghdrs freelist in mempool. 
 */
static int
alloc_pghdrs(int count)
{
	int i;
	page_t *p;

	if (!(p = (page_t *)malloc(sizeof(page_t) * count))) {
		return(1);
	}
	add_memchunk(p);
	bzero(p, sizeof(page_t) * count);
	for (i = 0; i < count; i++) {
		ENQUEUE(&mempool.pghdrs, p++);
	}
	mempool.npghdrs += count;
	mempool.free_pghdrs += count;
	return(0);
}

/*
 * get_pghdr()
 */
static page_t *
get_pghdr()
{
	page_t *p;

	if (!(p = (page_t *)DEQUEUE(&mempool.pghdrs))) {
		if (alloc_pghdrs(PGHDR_ALLOC)) {
			return((page_t *)NULL);
		}
		p = (page_t *)DEQUEUE(&mempool.pghdrs);	
	}
	mempool.free_pghdrs--;
	return(p);
}

/*
 * alloc_blkhdrs() -- Allocate count number of block headers and put them 
 *				  	  on mempool.free_blkhdrs. 
 */
static int
alloc_blkhdrs(int count)
{
	int i;
	block_t *b;

	if (!(b = (block_t *)malloc(sizeof(block_t) * count))) {
		return(1);
	}
	add_memchunk(b);
	bzero(b, sizeof(block_t) * count);
	for (i = 0; i < count; i++) {
		ENQUEUE(&mempool.blkhdrs, b++);
	}
	mempool.nblkhdrs += count;
	mempool.free_blkhdrs += count;
	return(0);
}

/*
 * get_blkhdr()
 */
static block_t *
get_blkhdr()
{
	block_t *b;

	if (!(b = (block_t *)DEQUEUE(&mempool.blkhdrs))) {
		if (alloc_blkhdrs(BLKHDR_ALLOC)) {
			return((block_t *)NULL);
		}
		b = (block_t *)DEQUEUE(&mempool.blkhdrs);	
	}
	mempool.free_blkhdrs--;
	return(b);
}

/*
 * alloc_page() -- Allocate count pages of memory
 */
static void *
alloc_pages(int count)
{
	void *page;

	if (!(page = (void *)memalign(IO_NBPC, IO_NBPC * count))) {
		return((void *)NULL);
	}
	add_memchunk(page);
	return(page);
}

/*
 * get_page()
 */
static page_t *
get_page(int index)
{
	int i;
	block_t *b;
	void *page;
	page_t *p;

TRY_AGAIN:

	/* Check to see if there are any free pages in memory pool 
	 */
	if (mempool.free_pgs) {
		p = (page_t *)DEQUEUE(&mempool.pgs);
		p->next = p->prev = (page_t *)NULL;
		mempool.free_pgs--;
	}
	else {

		/* Get some more pages for the memory pool
		 */
		page = alloc_pages(PAGE_ALLOC);
		for (i = 0; i < PAGE_ALLOC; i++) {
			if (!(p = get_pghdr())) {
				/* XXX -- free page ? */
				return((page_t *)NULL);
			}
			p->addr = page;
			p->blksz = 0;
			p->state = P_AVAIL;
			p->nfree = 0;
			p->index = 0;
			ENQUEUE(&mempool.pgs, p);
			page = (void *)((unsigned)page + IO_NBPC);
		}

		/* Bump global page/free counts
		 */
		mempool.npgs += PAGE_ALLOC;
		mempool.free_pgs += PAGE_ALLOC;
		goto TRY_AGAIN;
	}

	/* Update memory pool stats 
	 */
	mempool.m_alloc++;
	if (mempool.m_high < (mempool.npgs - mempool.free_pgs)) {
		mempool.m_high = (mempool.npgs - mempool.free_pgs);
	}
		
	/* Update info for bucket we're getting page for
	 */
	bucket[index].nblks += (IO_NBPC / bucket_size[index]);
	bucket[index].freeblks += (IO_NBPC / bucket_size[index]);
	bucket[index].npages++;

	/* Set up page header
	 */
	p->state = P_INUSE;
	p->index = index;
	p->blksz = bucket_size[index];
	p->nblocks = p->nfree = (IO_NBPC / bucket_size[index]);

	/* Break page into blocks and queue them on freelist 
	 */
	for (i = 0; i < p->nblocks; i++) {
		b = (block_t *)((unsigned)p->addr +  (i * p->blksz));
		ENQUEUE(&p->blklist, b);
	}

	/* put the new page in the hash table and on the bucket pagelist
	 */
	ENQUEUE(&bucket[index].pagelist, p);
	add_phash(p);
	return(p);
}

/*
 * init_mempool() -- initialize the block memory buckets.
 *
 * Pass in the initial number of page headers, block headers, 
 * and pages to allocate. If the event that any of the values 
 * are zero, then the default values will be used (PGHDR_INIT, 
 * BLKHDR_INIT, PAGE_INIT).
 */
int
init_mempool(int pghdr_cnt, int blkhdr_cnt, int page_cnt) 
{
	int i;
	void *page;
	page_t *p;

	/* zero out the buckets and phash[] list 
	 */
	bzero(&mempool, sizeof(mempool));
	bzero(&bucket, sizeof(bucket));
	bzero(&phash, sizeof(phash));

	if (pghdr_cnt == 0) {
		pghdr_cnt = PGHDR_INIT;
	}
	if (blkhdr_cnt == 0) {
		blkhdr_cnt = BLKHDR_INIT;
	}
	if (page_cnt == 0) {
		page_cnt = PAGE_INIT;
	}

	/* allocate some block and page headers to start with 
	 */
	if (alloc_pghdrs(pghdr_cnt)) {
		return(1);
	}
	if (alloc_blkhdrs(blkhdr_cnt)) {
		return(1);
	}

	/* Now get some pages for the memory pool. 
	 */
	page = (void *)alloc_pages(page_cnt);
	for (i = 0; i < page_cnt; i++) {
		if (!(p = get_pghdr())) {
			return(1);
		}
		p->addr = page;
		p->blksz = 0;
		p->state = P_AVAIL;
		p->nfree = 0;
		p->index = 0;
		ENQUEUE(&mempool.pgs, p);
		page = (void *)((unsigned)page + IO_NBPC);
	}

	/* Initialize memory pool structure
	 */
	mempool.npgs = page_cnt;
	mempool.free_pgs = page_cnt;
	mempool.m_alloc = 0;
	mempool.m_free = 0;
	mempool.m_high = 0;	   

	/* Initialize the bucket structs (with no memory) 
	 */
	for (i = 0; i < NBUCKETS; i++) {
		bucket[i].blksize = bucket_size[i];
		bucket[i].nblks = 0;
		bucket[i].freeblks = 0;
		bucket[i].npages = 0;
		bucket[i].s_alloc = 0;
		bucket[i].s_free = 0;
		bucket[i].s_high = 0;
	}
	return(0);
}

/*
 * free_mempool() -- Free up all liballoc memory 
 */
void
free_mempool()
{
	int i;
	memchunk_t *mcp, *t;

	mcp = mempool.memchunks;
	while (mcp) {
		t = mcp;
		mcp = mcp->next;
		for(i = 0; i < t->count; i++) {
			free(t->ptr[i]);
		}
		free(t);
	}
	bzero(&mempool, sizeof(mempool));
}


/*
 * alloc_block() -- Allocate a block of memory. 
 * 
 *   Similar in function to the memalign() system call.
 */
void *
#ifdef ALLOC_DEBUG
_alloc_block(int size, int flag, void *ra)
#else
alloc_block(int size, int flag)
#endif
{
	int i, j;
	void *blk;
	page_t *p;
	block_t	*b = (block_t *)NULL;

	/* silly, but .... 
	 */
	if (size == 0) {
		return((void *)NULL);
	}

	hold_signals();

	if (size > IO_NBPC) {

		/* round size upto the next full page size 
		 */
		if (size % IO_NBPC) {
			size += (IO_NBPC - (size % IO_NBPC));
		}

		/* Check to see if there are any free oversized blocks that are
		 * greater than or equal to size.
		 */
		if (p = mempool.ovrszpgs) {
			if (p == p->next) {
				if (p->blksz >= size) {
					REMQUEUE(&mempool.ovrszpgs, p);
				}
				else {
					p = (page_t *)NULL;
				}
			}
			else {
				do {
					if (p->blksz >= size) {
						REMQUEUE(&mempool.ovrszpgs, p);
						break;
					}
					p = p->next;
				} while (p != mempool.ovrszpgs);
				if (p == mempool.ovrszpgs) {
					p = (page_t *)NULL;
				}
			}
		}

		if (!p) {

			/* Get a page header
			 */
			if (!(p = get_pghdr())) {
				return((void *)NULL);
			}

			/* Get a page of memory 
			 */
			if (!(p->addr = (void *)memalign(IO_NBPC, size))) {
				return((void *)NULL);
			}
			add_memchunk(p->addr);

			/* Set up the page header 
			 */
			p->nblocks = 1;
			p->blksz = size;
			p->state = P_OVRSZ;
			p->nfree = 0;
			p->blklist = (block_t *)NULL;
			p->index = OVRSZBKT; /* set index to last bucket */
		}

		/* Now zero out the page
		 */
		bzero(p->addr, size);
		 
		/* Add the new page on the bucket pagelist 
		 */
		ENQUEUE(&bucket[OVRSZBKT].pagelist, p);

		/* Update info for oversize bucket 
		 */
		bucket[OVRSZBKT].nblks++;
		bucket[OVRSZBKT].npages += (p->blksz / IO_NBPC);
		bucket[OVRSZBKT].s_alloc++;

		/* Set the high water mark
		 */
		if (bucket[OVRSZBKT].s_high < 
				(bucket[OVRSZBKT].nblks - bucket[OVRSZBKT].freeblks)) {
			bucket[OVRSZBKT].s_high = 
					(bucket[OVRSZBKT].nblks - bucket[OVRSZBKT].freeblks);
		}

#ifdef ALLOC_DEBUG
		if (!(b = get_blkhdr())) {
			return((void *)NULL);
		}
#else
		/* All temporary blocks need to have a block header -- so they
		 * can be freed up in free_temp_blocks().
		 */
		if (flag == B_TEMP) {
			if (!(b = get_blkhdr())) {
				return((void *)NULL);
			}
		}
#endif

		if (b) {
			b->addr = p->addr;
#ifdef ALLOC_DEBUG
			b->flag = flag;
			b->page = p;
			b->alloc_pc = (void *)((unsigned)ra - 8);
#endif
		}

		if (flag == B_TEMP) {
			ENQUEUE(&temp_blks, b);
		}
#ifdef ALLOC_DEBUG
		else {
			ENQUEUE(&perm_blks, b);
		}
#endif

		/* Put the page on the phash[] list
		 */
		add_phash(p);

		release_signals();
		return(p->addr);
	}

	/* Find the bucket with the closest fit (next largest) block size.
	 * Then, get a block from that bucket. 
	 */
	for (i = 0; i < (NBUCKETS - 1); i++) {
		if (bucket[i].blksize < size) {
			continue;
		}
		break;
	}

	/* Check to see if there are any free blocks. 
	 */
	if (bucket[i].freeblks) {
		p = bucket[i].pagelist;
		while (p->blklist == 0) {
			p = p->next;
#ifdef ALLOC_DEBUG
			assert(p != bucket[i].pagelist);
#endif
		}
	}
	else {
		if (!(p = get_page(i))) {
			return((void *)NULL);
		}
	}

	blk = (void *)DEQUEUE(&p->blklist);
	p->nfree--;
	bzero(blk, p->blksz);

#ifdef ALLOC_DEBUG
	if (!(b = get_blkhdr())) {
		return((void *)NULL);
	}
#else
	/* All temporary blocks need to have a block header -- so they
	 * can be freed up in free_temp_blocks().
	 */
	if (flag == B_TEMP) {
		if (!(b = get_blkhdr())) {
			return((void *)NULL);
		}
	}
#endif

	if (b) {
		b->addr = blk;
#ifdef ALLOC_DEBUG
		b->flag = flag;
		b->page = p;
		b->alloc_pc = (void *)((unsigned)ra - 8);
#endif
	}

	if (flag == B_TEMP) {
		ENQUEUE(&temp_blks, b);
	}
#ifdef ALLOC_DEBUG
	else {
		ENQUEUE(&perm_blks, b);
	}
#endif

	bucket[i].freeblks--;
	bucket[i].s_alloc++;
	if (bucket[i].s_high < (bucket[i].nblks - bucket[i].freeblks)) {
		bucket[i].s_high = (bucket[i].nblks - bucket[i].freeblks);
	}

	/* Put page header on phash[]
	 */
	release_signals();
	return(blk);
}

/*
 * realloc_block()
 */
void *
#ifdef ALLOC_DEBUG
_realloc_block(void *b, int new_size, int flag, void *ra) 
#else
realloc_block(void *b, int new_size, int flag) 
#endif
{
	page_t *p;
	void *b1 = (void *)NULL;

	hold_signals();

	/* Locate the page header for this block from the phash[] table
	 */
	if (!(p = find_page(b))) {
		return((void *)NULL);
	}

#ifdef ALLOC_DEBUG
	if (b1 = _alloc_block(new_size, flag, ra)) {
#else
	if (b1 = alloc_block(new_size, flag)) {
#endif
		bcopy(b, b1, p->blksz);
		free_block(b);
	}
	release_signals();
	return(b1);
}

/*
 * dup_block()
 */
void *
#ifdef ALLOC_DEBUG
_dup_block(void *b, int flag, void *ra)
#else
dup_block(void *b, int flag)
#endif
{
	page_t *p;
	block_t *b1;

	hold_signals();

	/* Locate the page header for this block from the phash[] table
	 */
	if (!(p = find_page(b))) {
		release_signals();
		return((void *)NULL);
	}

#ifdef ALLOC_DEBUG
	b1 = _alloc_block(p->blksz, flag, ra);
#else
	b1 = alloc_block(p->blksz, flag);
#endif
	bcopy(b, b1, p->blksz);
	release_signals();
	return(b1);
}

/*
 * str_to_block()
 */
void *
#ifdef ALLOC_DEBUG
_str_to_block(char *s, int flag, void *ra)
#else
str_to_block(char *s, int flag)
#endif
{
	int size;
	void *b;

	hold_signals();

	size = strlen(s) + 1;

#ifdef ALLOC_DEBUG
	b = _alloc_block(size, flag, ra);
#else
	b = alloc_block(size, flag);
#endif
	bcopy(s, b, size);
	release_signals();
	return(b);
}

/*
 * free_blk()
 */
void
free_blk(page_t *p, void *block)
{
	block_t *b;

	/* Get the block header (if there is one) off the temp_blks list
	 */
	if (b = temp_blks) {
		do {
			if (b->addr == block) {
				REMQUEUE(&temp_blks, b);
				ENQUEUE(&mempool.blkhdrs, b);
				mempool.free_blkhdrs++;
				break;
			}
			b = b->next;
		} while (b != temp_blks);
		if (b == temp_blks) {
			b = (block_t *)NULL;
		}
	}

#ifdef ALLOC_DEBUG
	/* Try and find the block on the permenant block list
	 */
	if (!b) {
		b = perm_blks;
		do {
			if (b->addr == block) {
				REMQUEUE(&perm_blks, b);
				b->addr = (void *)NULL;
				ENQUEUE(&mempool.blkhdrs, b);
				mempool.free_blkhdrs++;
				break;
			}
			b = b->next;
		} while (b != perm_blks);
		if (b == perm_blks) {
			b = (block_t *)NULL;
		}
	}

	assert(b);
#endif

	/* If the block is oversized...
	 */
	if (p->index == OVRSZBKT) {

		/* Remove page from bucket pagelist and phash[] list
		 */
		REMQUEUE(&bucket[OVRSZBKT].pagelist, p);
		bucket[OVRSZBKT].nblks--; 
		bucket[OVRSZBKT].npages -= (p->blksz / IO_NBPC);
		bucket[OVRSZBKT].s_free++;
		rem_phash(p);

#ifdef FREE_OVSZBLKS
		/* Free the memory 
		 */
		free (p->addr);
		p->state = P_FREE;
		ENQUEUE(&mempool.pghdrs, p);
		mempool.free_pghdrs++;
#else
		ENQUEUE(&mempool.ovrszpgs, p);
#endif
		return;
	}

	/* Queue block on page's freelist
	 */
	ENQUEUE(&p->blklist, block);
	p->nfree++;
	bucket[p->index].freeblks++;
	bucket[p->index].s_free++;
	return;
}

/*
 * free_block()
 */
void
free_block(void *block)
{
	int ret;
	page_t *p;

	hold_signals();

#ifdef ALLOC_DEBUG
	assert(block);
#endif

	/* Locate the page header for this block from the phash[] table
	 */
	p = find_page(block);

#ifdef ALLOC_DEBUG
	assert(p);
#endif

	free_blk(p, block);
	release_signals();
	return;
}

/*
 * free_temp_blocks() -- Free all temporarily allocated blocks
 */
void
free_temp_blocks()
{
	block_t *b;
	page_t *p;

	hold_signals();

	while(temp_blks) {
		b = temp_blks;
		p = find_page(b->addr);

#ifdef ALLOC_DEBUG
		if (alloc_debug) {
			fprintf(alloc_fp, "free_temp_blocks: b=0x%x, addr=0x%x, size=%d, "                "alloc_pc=0x%x\n", b, b->addr, p->blksz, b->alloc_pc);
		}
		assert(p);
#endif
		free_blk(p, b->addr);
	} 
	release_signals();
}

/*
 * is_temp_block()
 */
int
is_temp_block(void *addr)
{
	block_t *b1;

	if (b1 = temp_blks) {
		do {
			if (b1->addr == addr) {
				return(1);
			}
			b1 = b1->next;
		} while (b1 != temp_blks);
	}
	return(0);
}
