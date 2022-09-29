#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libutil/RCS/alloc.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <assert.h>
#include "icrash.h"
#include "alloc.h"
#include "extern.h"

/* Local variables
 */
page_t *phash[PHASHSIZE];
bucket_t bucket[NBUCKETS];
mempool_t mempool;
block_t *temp_blks = NULL;        /* List of block headers for B_TEMP blocks */
int bucket_size[] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, -1};

#ifdef ICRASH_DEBUG
block_t *perm_blks = NULL;
#endif

/*
 * add_phash() -- Put page header on phash[] hash table.
 */
void
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
int
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
 * find_page() -- See if page for addr is in phash[] hash table.
 * 				  If it is, return a pointer to the page header. 
 *				  Otherwise return NULL.
 */
page_t  *
find_page(caddr_t *addr)
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
void
alloc_pghdrs(int count)
{
	int i;
	page_t *p;

	if (!(p = (page_t *)malloc(sizeof(page_t) * count))) {
		fatal("not enough memory to allocate page headers!\n");
	}
	bzero(p, sizeof(page_t) * count);
	for (i = 0; i < count; i++) {
		ENQUEUE(&mempool.pghdrs, p++);
	}
	mempool.npghdrs += count;
	mempool.free_pghdrs += count;
}

/*
 * get_pghdr()
 */
page_t *
get_pghdr()
{
	page_t *p;

	if (!(p = (page_t *)DEQUEUE(&mempool.pghdrs))) {
		alloc_pghdrs(PGHDR_ALLOC);
		p = (page_t *)DEQUEUE(&mempool.pghdrs);	
	}
	mempool.free_pghdrs--;
	return(p);
}

/*
 * alloc_blkhdrs() -- Allocate count number of block headers and put them 
 *				  	  on mempool.free_blkhdrs. 
 */
void
alloc_blkhdrs(int count)
{
	int i;
	block_t *b;

	if (!(b = (block_t *)malloc(sizeof(block_t) * count))) {
		fatal("not enough memory to allocate block headers!\n");
	}
	bzero(b, sizeof(block_t) * count);
	for (i = 0; i < count; i++) {
		ENQUEUE(&mempool.blkhdrs, b++);
	}
	mempool.nblkhdrs += count;
	mempool.free_blkhdrs += count;
}

/*
 * get_blkhdr()
 */
block_t *
get_blkhdr()
{
	block_t *b;

	if (!(b = (block_t *)DEQUEUE(&mempool.blkhdrs))) {
		alloc_blkhdrs(BLKHDR_ALLOC);
		b = (block_t *)DEQUEUE(&mempool.blkhdrs);	
	}
	mempool.free_blkhdrs--;
	return(b);
}

/*
 * alloc_page() -- Allocate count pages of memory
 */
caddr_t *
alloc_pages(int count)
{
	caddr_t *page;

	if (!(page = (caddr_t *)memalign(IO_NBPC, IO_NBPC * count))) {
		fprintf(KL_ERRORFP, "%s: not enough memory to allocate %d pages!\n", 
			program, count);
			exit(1);
	}
	return(page);
}

/*
 * get_page()
 */
page_t *
get_page(int index)
{
	int i;
	block_t *b;
	caddr_t *page;
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
		page = (caddr_t *)alloc_pages(PAGE_ALLOC);
		for (i = 0; i < PAGE_ALLOC; i++) {
			p = get_pghdr();
			p->addr = page;
			p->blksz = 0;
			p->state = P_AVAIL;
			p->nfree = 0;
			p->index = 0;
			ENQUEUE(&mempool.pgs, p);
			page += (IO_NBPC/4);
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
 */
void
init_mempool() 
{
	int i;
	caddr_t *page;
	page_t *p;

	/* zero out the buckets and phash[] list 
	 */
	bzero(&bucket, sizeof (bucket));
	bzero(&phash, sizeof (phash));

	/* allocate some block and page headers to start with 
	 */
	alloc_pghdrs(PGHDR_INIT);
	alloc_blkhdrs(BLKHDR_INIT);

	/* Now get some pages for the memory pool. 
	 */
	page = (caddr_t *)alloc_pages(PAGE_INIT);
	for (i = 0; i < PAGE_INIT; i++) {
		p = get_pghdr();
		p->addr = page;
		p->blksz = 0;
		p->state = P_AVAIL;
		p->nfree = 0;
		p->index = 0;
		ENQUEUE(&mempool.pgs, p);
		page += (IO_NBPC/4);
	}

	/* Initialize memory pool structure
	 */
	mempool.npgs = PAGE_INIT;
	mempool.free_pgs = PAGE_INIT;
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
}

/*
 * alloc_block() -- Allocate a block of memory. 
 * 
 *   Similar in function to the memalign() system call.
 */
k_ptr_t
#ifdef ICRASH_DEBUG
_alloc_block(int size, int flag, caddr_t *ra)
#else
alloc_block(int size, int flag)
#endif
{
	int i, j;
	caddr_t *blk;
	page_t *p;
	block_t	*b = (block_t *)NULL;

	/* silly, but .... 
	 */
	if (size == 0) {
		return((void *)NULL);
	}

	kl_hold_signals();

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
			p = get_pghdr();

			/* Get a page of memory 
			 */
			if (!(p->addr = (caddr_t *)memalign(IO_NBPC, size))) {
				if (DEBUG(DC_GLOBAL, 1)) {
					fprintf(KL_ERRORFP, "not enough memory to allocate "
							"%d bytes!\n", size);
				}
				return((void *)NULL);
			}

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

#ifdef ICRASH_DEBUG
		b = get_blkhdr();
#else
		/* All temporary blocks need to have a block header -- so they
		 * can be freed up in free_temp_blocks().
		 */
		if (flag == B_TEMP) {
			b = get_blkhdr();
		}
#endif

		if (b) {
			b->addr = p->addr;
#ifdef ICRASH_DEBUG
			b->flag = flag;
			b->page = p;
			b->alloc_pc = ra - 8;
#endif
		}

		if (flag == B_TEMP) {
			ENQUEUE(&temp_blks, b);
		}
#ifdef ICRASH_DEBUG
		else {
			ENQUEUE(&perm_blks, b);
		}
#endif

		/* Put the page on the phash[] list
		 */
		add_phash(p);

		kl_release_signals();
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
#ifdef ICRASH_DEBUG
			assert(p != bucket[i].pagelist);
#endif
		}
	}
	else {
		if (!(p = get_page(i))) {
			return((void *)NULL);
		}
	}

	blk = (caddr_t *)DEQUEUE(&p->blklist);
	p->nfree--;
	bzero(blk, p->blksz);

#ifdef ICRASH_DEBUG
	b = get_blkhdr();
#else
	/* All temporary blocks need to have a block header -- so they
	 * can be freed up in free_temp_blocks().
	 */
	if (flag == B_TEMP) {
		b = get_blkhdr();
	}
#endif

	if (b) {
		b->addr = blk;
#ifdef ICRASH_DEBUG
		b->flag = flag;
		b->page = p;
		b->alloc_pc = ra - 8;
#endif
	}

	if (flag == B_TEMP) {
		ENQUEUE(&temp_blks, b);
	}
#ifdef ICRASH_DEBUG
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
	kl_release_signals();
	return(blk);
}

/*
 * dup_block()
 */
k_ptr_t
dup_block(caddr_t *b, int flag)
{
	page_t *p;
	block_t *b1;

	kl_hold_signals();

	/* Locate the page header for this block from the phash[] table
	 */
	if (!(p = find_page(b))) {
		return((k_ptr_t)NULL);
	}

	b1 = alloc_block(p->blksz, flag);
	bcopy(b, b1, p->blksz);
	kl_release_signals();
	return(b1);
}

/*
 * free_blk()
 */
free_blk(page_t *p, caddr_t *block)
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

#ifdef ICRASH_DEBUG
	/* Try and find the block on the permenant block list
	 */
	if (!b) {
		b = perm_blks;
		do {
			if (b->addr == block) {
				REMQUEUE(&perm_blks, b);
				b->addr = (caddr_t *)NULL;
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
#endif

	if (DEBUG(DC_ALLOC, 5)) {
		if (b) {
			fprintf(KL_ERRORFP, "free_blk: b=0x%x, addr=0x%x, alloc_pc=0x%x\n",
				b, b->addr, b->alloc_pc);
		}
	}

#ifdef ICRASH_DEBUG
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
		return(0);
	}

	/* Queue block on page's freelist
	 */
	ENQUEUE(&p->blklist, block);
	p->nfree++;
	bucket[p->index].freeblks++;
	bucket[p->index].s_free++;
	return(0);
}

/*
 * free_block()
 */
int
free_block(caddr_t *block)
{
	int ret;
	page_t *p;

	if (DEBUG(DC_ALLOC, 2) || DEBUG(DC_FUNCTRACE, 8)) {
		fprintf(KL_ERRORFP, "free_block: block=0x%x\n", block);
	}

	kl_hold_signals();

#ifdef ICRASH_DEBUG
	assert(block);
#endif

	/* Locate the page header for this block from the phash[] table
	 */
	p = find_page(block);

#ifdef ICRASH_DEBUG
	assert(p);
#endif

	ret = free_blk(p, block);
	kl_release_signals();
	return(ret);
}

/*
 * free_temp_blocks() -- Free all temporarily allocated blocks
 */
void
free_temp_blocks()
{
	block_t *b;
	page_t *p;
	caddr_t *addr;

	kl_hold_signals();

	while(temp_blks) {
		b = temp_blks;
		p = find_page(b->addr);

		if (DEBUG(DC_ALLOC, 1)) {
			fprintf(KL_ERRORFP, "free_temp_blocks: b=0x%x, addr=0x%x, size=%d, "
				"alloc_pc=0x%x\n", b, b->addr, p->blksz, b->alloc_pc);
		}
#ifdef ICRASH_DEBUG
		assert(p);
#endif
		free_blk(p, b->addr);
	} 
	kl_release_signals();
}

/*
 * is_temp_block()
 */
int
is_temp_block(caddr_t *addr)
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
