/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991, Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* 
 * malloc.c - Storage allocator for standalone - prom, ide, sash, 
 *   fx, etc. 
 *
 * The standalone malloc is designed to be as simple as possible for 
 * easy debugging. It uses a circular, single-linked list. Blocks in 
 * the list are taken from a buffer called malbuf, in bss space. A 
 * first-fit search is used. Free blocks following a freed block are 
 * joined in free, while free blocks in the list ahead of the freed 
 * block are joined in malloc during the search for a free block.
 *
 * Two blocks are allocated to start and are reserved as place holders 
 * for the beginning and end of the list.
 */

#ident "$Revision: 1.51 $"

#include <sys/types.h>
#include <libsc.h>

/*#define	DEBUG	*/
/*#define	TRACE	*/

/*
 * Dont initialized globals or statics
 * BSS is initialized to 0
 */
int sc_sable;

/*
 * the next 2 items are defined in arcs/include/tune.h
 * the size of malbuf[] is defined in one of these 3 files:
 * arcs/include/tune.h, arcs/sash/tune.cf, arcs/symmon/fsconf.cf
 */
extern int _max_malloc;	/* most we can ever allocate at once */
extern char malbuf[];

struct block {
	struct block *next;
	__scunsigned_t stat;
	/* address following block is returned by malloc */
};

/* BLOCKSZ must be 2^x */
#define BLOCKSZ			sizeof(struct block)
#define	DEFAULT_ALIGN		BLOCKSZ
#define	DEFAULT_ALIGN_MSK	(DEFAULT_ALIGN - 1)
#define	ROUNDUP(x)		(((x) + DEFAULT_ALIGN_MSK) & ~DEFAULT_ALIGN_MSK)

/* Forwards */

static struct block *getblock(size_t);

#ifdef	DEBUG
static void mal_showlist(int);
static void link_check(void);
#endif	/* DEBUG */

/* Local defines */

#define	TRUE	1
#define	FALSE	0

/* block status values */

#define	STAT_FREE		0x0
#define	STAT_MALLOCED		0xbeefed
#define	STAT_BEG		0xaabbcc
#define	STAT_END		0xddeeff

/* Local variables */

/* block pointers for traversing list */

static struct block *bblk;		/* beginning block */
static struct block *eblk;		/* end block */
static struct block *cblk;		/* current block pointer */

/* 
 * Pointer to malbuf - want this in bss in case we reinit by 
 * clearing bss. mptr always points to the next place in malbuf
 * where memory is availble. 
 */

static char *mptr; 

/* 
 * _init_malloc - reinitialize the malloc buffer without clearing 
 * all of bss.
 */
__scunsigned_t dmabuf_linemask;

void
_init_malloc(void)
{
	bblk = NULL;

	if (!sc_sable)
		bzero(malbuf, _max_malloc);

	/* this is for standalone programs that do not link with libsk.a */
	/* dmabuf_linemask gets initted in the asm routine config_cache */

	if (dmabuf_linemask == 0) {
		COMPONENT *c;
		union key_u key;

		c = find_type(GetChild((COMPONENT *)NULL), SecondaryCache);
		if (c == (COMPONENT *)NULL)
			c = find_type(GetChild((COMPONENT *)NULL),
				PrimaryDCache);
		if (c != NULL) {  /* XXX fixme */
		key.FullKey = c->Key;
		dmabuf_linemask =
			(__scunsigned_t)((1 << key.cache.c_lsize) - 1);
		} else {
		dmabuf_linemask =
                        (__scunsigned_t)((1 << 7) - 1);
		}
	}
}


/*
 * malloc - malloc n bytes from the malloc buffer, malbuf.
 */
void *
malloc(size_t nbytes) {
	register struct block *b1, *b2;	/* traversal block pointers */
	register struct block *tmp;
	int found = FALSE;
	int list_scanned = FALSE;

	if (nbytes == 0)
		return NULL;

	/*
	 * malbuf is clear - initialize
	 * this block is here instead of in malloc_init() because the
	 * power-on SCSI diagnostic uses malloc() before init_malloc()
	 * is ever called
	 */
	if (bblk == NULL) {
		mptr = (char *)ROUNDUP((__psunsigned_t)malbuf);

		if ((bblk = getblock(BLOCKSZ)) == NULL ||
	    	    (eblk = getblock(BLOCKSZ)) == NULL) {
#ifdef	DEBUG
			printf("malloc() failed to grow malloc_arena by 0x%x bytes during bblk/eblk initialization, mptr=0x%x, &malbuf[_max_malloc]=0x%x\n",
				BLOCKSZ, mptr, &malbuf[_max_malloc]);
#endif	/* DEBUG */
			return NULL;
		}

		bblk->stat = STAT_BEG;
		bblk->next = eblk;
		eblk->stat = STAT_END;
		eblk->next = bblk;

		cblk = bblk;
	}

	/* Search for a free block of the right size */

	b1 = cblk;
	do {
		if (b1->stat != STAT_FREE) {
			b1 = b1->next;
			continue;
		}

		/* Look for adjacent free blocks and join them */

		b2 = b1->next;
		while (b2->stat == STAT_FREE) {
			b1->next = b2->next;
			if (cblk == b2) {
				cblk = b1;
				/* prevent re-scanning of the list */
				list_scanned = TRUE;
			}
			b2 = b2->next;
		}

		/* Compute size of block needed */

		tmp = (struct block *)
			ROUNDUP((__psunsigned_t)b1 + nbytes + BLOCKSZ);

		if (tmp <= b2) {

			/* 
			 * Found a free block big enough. Mark as allocated,
			 * readjust current block pointer and end block 
			 * pointer if necessary. 
			 */

			/* 
			 * Create a free block if there's any leftover
			 */
			if (b2 == tmp)		/* no leftover */
				cblk = b2;
			else {
				cblk = tmp;
				cblk->next = b2;
				cblk->stat = STAT_FREE;
				b1->next = cblk;
			}
			b1->stat = STAT_MALLOCED;
			found = TRUE;
			break;

		}
		b1 = b1->next;
	} while (b1 != cblk && !list_scanned);

	if (!found) {

		/*
		 * Didn't find a block big enough - get one from malbuf.
		 * Insert the new block in front of eblk.  Don't change
		 * cblk since it probably points to a free block and, if
		 * it is so, next malloc() should take next time than if
		 * we equate it with eblk.
		 */

		if (getblock(nbytes + BLOCKSZ) == NULL) {
#ifdef	DEBUG
			printf("malloc() failed to grow malloc_arena by 0x%x bytes, mptr=0x%x, &malbuf[_max_malloc]=0x%x\n",
				nbytes + BLOCKSZ, mptr, &malbuf[_max_malloc]);
#endif	/* DEBUG */
			return NULL;
		}
		b1 = eblk;
		eblk = (struct block *)
			ROUNDUP((__psunsigned_t)b1 + nbytes + BLOCKSZ);
		eblk->next = bblk;
		eblk->stat = STAT_END;
		b1->stat = STAT_MALLOCED;
		b1->next = eblk;
	}

#ifdef	TRACE
	printf("MALLOC: nbytes=0x%x, blk ptr=0x%x\n", nbytes, b1);
#endif	/* TRACE */

#ifdef	DEBUG
	link_check();
#endif	/* DEBUG */

	return (void *)++b1;
}

/* 
 * getblock - get a chunk of memory out of malbuf. When entering this
 * routine, mptr points to the next spot of free memory in malbuf, which
 * is returned to the calling routine. mptr is then adjusted appropriately
 * and points to the next spot in malbuf.
 */
static struct block *
getblock(register size_t n)
{
	char *m, *b;

#ifdef	TRACE
	printf("GETBLOCK: n=0x%x, mptr=0x%x\n", n, mptr);
#endif	/* TRACE */

	n = ROUNDUP(n);

	b = mptr + n;
	if (b < &malbuf[_max_malloc]) {
		m = mptr;
		mptr = b;
		return (struct block *)m;
	}
	return NULL;
}


/*
 * free - free a block of memory. Free blocks following freed
 * block are joined here.
 */
void
free(void *mem)
{
	register struct block *b1, *b2;	/* traversal block pointers */

	if (mem == NULL)
		return;

	/* Set pointer to beginning of block structure */

	b1 = (struct block *)mem - 1;

#ifdef	TRACE
	printf("FREE: blk ptr=0x%x\n", b1);
#endif	/* TRACE */

	/* Check if pointer is from a valid malloced block */
	if (b1->stat != STAT_MALLOCED ||
	    (char *)b1 < malbuf ||
	    (char *)b1 >= &malbuf[_max_malloc]) {
#ifdef	DEBUG
		printf("free: couldn't free block at 0x%x\n", b1);
#endif	/* DEBUG */
		return;
	}

	/* Free the block of memory */
	b1->stat = STAT_FREE;
	cblk = b1;	

	/* Join any free blocks following the block just freed */

	b2 = b1->next;
	while (b2->stat == STAT_FREE) {
                b1->next = b2->next;
                b2 = b2->next;
        }

#ifdef	DEBUG
	link_check();
#endif	/* DEBUG */
}


/*
 * realloc - reallocate a block returned from malloc with new size.
 * Copy contents to the new block.
 */
void *
realloc(void *mem1, size_t nbytes)
{
	register struct block *b1;	/* temporary block pointer */
	register void *mem2;		/* new memory space */
	size_t pbytes;			/* previous bytes */

	/* Behaves as malloc() if mem1 is NULL */
	if (mem1 == NULL)
		return malloc(nbytes);

	if (nbytes == 0) {
		free(mem1);
		return NULL;
	}

	/* Set pointer to beginning of block structure */

	b1 = (struct block *)mem1 - 1;
	if (b1->stat != STAT_MALLOCED)
		return NULL;

	/* 
	 * Compute number of bytes to reallocate and previous number of
	 * bytes allocated for copying. 
	 */
 
	pbytes = (__psunsigned_t)b1->next - (__psunsigned_t)b1 - BLOCKSZ;
	if (nbytes < pbytes)
		pbytes = nbytes;

#ifdef	TRACE
	printf("REALLOC: blk ptr=0x%x, nbytes=0x%x, pbytes=0x%x\n",
		b1, nbytes, pbytes);
#endif	/* TRACE */

	/*
	 * Free the block, malloc a new block and copy the contents of
	 * the old block to the new block. Adjust the block pointer sent
	 * to free, it will adjust back.
	 */

	free(++b1);
	mem2 = malloc(nbytes);
	if (mem2 == mem1 || mem2 == NULL)
		return mem2;
	bcopy(mem1, mem2, (int)pbytes);
	return mem2;
}

void *
calloc(size_t x, size_t y)
{
	return kern_calloc(x, y);
}


void *
kern_calloc(size_t n, size_t sz)
{
	void *s = malloc(n * sz);

	if (s)
		bzero(s, (int)(n * sz));
	return s;
}


/* allocate a buffer with the specified alignment */
void *
align_malloc(size_t nbytes, uint_t align)
{
	__psunsigned_t align_msk = align - 1;
	void *ptr;
	struct block *ptr_blk;
	void *usr_ptr;
	struct block *usr_ptr_blk;

	/* alignment must be on 2^x boundary */
	if (align & align_msk)
		return NULL;

	if (align <= DEFAULT_ALIGN)
		return malloc(nbytes);

	if ((ptr = malloc(nbytes + align)) == NULL)
		return NULL;

	/*
	 * if allocated buffer not already aligned, make it aligned and
	 * free the extra space in front of it
	 */
	usr_ptr = ptr;
	if ((__psunsigned_t)usr_ptr & align_msk) {
		usr_ptr = (void *)
			(((__psunsigned_t)usr_ptr + align_msk) & ~align_msk);
		usr_ptr_blk = (struct block *)usr_ptr - 1;
		ptr_blk = (struct block *)ptr - 1;

		usr_ptr_blk->next = ptr_blk->next;
		usr_ptr_blk->stat = STAT_MALLOCED;
		ptr_blk->next = usr_ptr_blk;
		ptr_blk->stat = STAT_FREE;
	}

	/* free any extra space at the end of the allocated buffer */
	ptr_blk = (struct block *)ROUNDUP((__psunsigned_t)usr_ptr + nbytes);

	usr_ptr_blk = (struct block *)usr_ptr - 1;
	if (usr_ptr_blk->next > ptr_blk) {
		ptr_blk->next = usr_ptr_blk->next;
		ptr_blk->stat = STAT_FREE;
		usr_ptr_blk->next = ptr_blk;
	}

#ifdef	TRACE
	printf("ALIGN_MALLOC: nbytes=0x%x, align=0x%x, blk ptr=0x%x\n",
		nbytes, align, usr_ptr_blk);
#endif	/* TRACE */

	return usr_ptr;
}


void
align_free(void *ptr)
{
	free(ptr);
}


/*
 * dmabuf_malloc - malloc a buffer safe for DMA
 *
 * Allocate a buffer of nbytes appropriate for DMA operations.
 *
 * On machines with a writeback cache (R4000), DMA buffers must
 * be cache aligned and padded in order to simplify cache flushing.
 *
 * NOTE: It is the caller's responsibility to properly handle 
 * cache flushing for this buffer.
 */

void *
dmabuf_malloc(size_t nbytes)
{
	return align_malloc(nbytes, (uint_t)(dmabuf_linemask + 1));
}


/*
 * dmabuf_free(ptr) - free a block that was allocated through dmabuf_malloc()
 */
void
dmabuf_free(void *ptr)
{
	free(ptr);
}


#ifdef	DEBUG
/*
 * mal_showlist - debugging routine to show malloc buffer. Print all 
 * blocks or n blocks starting at cblk.
 */

static void
mal_showlist(int n)
{
	struct block *p;

	if (!n)
		n = -1;		/* print the whole list */

	printf("\nSHOWLIST\n");
	if (bblk == NULL) {
		printf("SHOWLIST: nothing malloced yet\n");
		return;
	}

	p = bblk;
	do {
		printf("blk=0x%x, next=0x%x, stat=0x%x, size=0x%x\n", 
		       p, p->next, p->stat, 
		       (__psunsigned_t)p->next - (__psunsigned_t)p);
		p = p->next;
	} while (--n && p != eblk);

	/* eblk */
	if (n)
		printf("blk=0x%x, next=0x%x, stat=0x%x\n", p, p->next, p->stat);
}


/* linked list consistency check */
static void
link_check(void)
{
	struct block *p1;
	struct block *p2;
	int free_blocks = 0;
	int malloced_blocks = 0;

	p1 = bblk;
	while (p1 != eblk) {
		p2 = p1->next;
		if (p1->stat == STAT_FREE)
			free_blocks += p2 - p1;
		else
			malloced_blocks += p2 - p1;

		if (p2 <= p1 || p2 > eblk) {
			printf("link_check() list consistency check failed\n");
#ifdef	NOTDEF
			mal_showlist(0);
#endif	/* NOTDEF */
			return;
		}
		p1 = p2;
	}

	malloced_blocks++;		/* add 1 for eblk */
	if ((struct block *)mptr - bblk != malloced_blocks + free_blocks)
		printf("link_check() possible memory leak, arena size=0x%x, malloced_blocks=0x%x, free_blocks=0x%x\n",
			(struct block *)mptr - bblk,
			malloced_blocks, free_blocks);
}
#endif	/* DEBUG */
