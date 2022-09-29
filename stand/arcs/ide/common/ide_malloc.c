/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* 
 * ide_malloc.c - Storage allocator for large short lived memory in IDE.
 * 
 * This file is the same as malloc except for the fact that malloc
 * pool being used is found by ide with via GetMemoryDescriptor.
 * 
 * The standalone malloc is designed to be as simple as possible for 
 * easy debugging. It uses a circular, single-linked list. Blocks in 
 * the list are taken from a buffer called scratch_buf, in bss space. A 
 * first-fit search is used. Free blocks following a freed block are 
 * joined in free, while free blocks in the list ahead of the freed 
 * block are joined in malloc during the search for a free block.
 *
 * Two blocks are allocated to start and are reserved as place holders 
 * for the beginning and end of the list.
 */

#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/mips_addrspace.h>
#include <libsc.h>

/*#define	DEBUG	*/
/*#define	TRACE	*/

static int scratch_max;
static char *scratch_buf;

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

static struct block *getblock(int);

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
 * Pointer to scratch_buf - want this in bss in case we reinit by 
 * clearing bss. mptr always points to the next place in scratch_buf
 * where memory is availble. 
 */

static char *mptr; 

static void *
find_malloc_chunk(int size)
{
	int pages = (size+ARCS_NBPP)/ARCS_NBPP;
	MEMORYDESCRIPTOR *m=0;
	void *rc;

	while (m = GetMemoryDescriptor(m)) {
		if ((m->Type == FreeMemory) &&
		    (m->PageCount > pages)) {
			if (IS_KSEG0(&mptr))
				rc = (void *)PHYS_TO_K0(m->BasePage*ARCS_NBPP);
			else
				rc = (void *)PHYS_TO_K1(m->BasePage*ARCS_NBPP);

			return rc;
		}
	}

	return 0;
}

/* Try and find up to a 16MB memory space for ide malloc */
int
init_ide_malloc(int max)
{
	max += 4096;			/* malloc overhead */

	if ((scratch_buf = find_malloc_chunk(max)) == NULL)
		return 0;

	scratch_max = max;
	bblk = NULL;
	bzero(scratch_buf, scratch_max);

	return 1;
}

/*
 * malloc - malloc n bytes from the malloc buffer, scratch_buf.
 */
void *
ide_malloc(int nbytes) {
	register struct block *b1, *b2;	/* traversal block pointers */
	register struct block *tmp;
	int found = FALSE;
	int list_scanned = FALSE;

	if (nbytes == 0)
		return NULL;

	/*
	 * scratch_buf is clear - initialize
	 */
	if (bblk == NULL) {
		mptr = (char *)ROUNDUP((__psunsigned_t)scratch_buf);

		if ((bblk = getblock(BLOCKSZ)) == NULL ||
	    	    (eblk = getblock(BLOCKSZ)) == NULL) {
#ifdef	DEBUG
			printf("ide_malloc() failed to grow scratch_arena by 0x%x bytes during bblk/eblk initialization, mptr=0x%x, &scratch_buf[scratch_max]=0x%x\n",
				BLOCKSZ, mptr, &scratch_buf[scratch_max]);
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
		 * Didn't find a block big enough - get one from scratch_buf.
		 * Insert the new block in front of eblk.  Don't change
		 * cblk since it probably points to a free block and, if
		 * it is so, next scratch_malloc() should take next time than if
		 * we equate it with eblk.
		 */

		if (getblock(nbytes + BLOCKSZ) == NULL) {
#ifdef	DEBUG
			printf("scratch_malloc() failed to grow scratch_arena by 0x%x bytes, mptr=0x%x, &scratch_buf[scratch_max]=0x%x\n",
				nbytes + BLOCKSZ, mptr, &scratch_buf[scratch_max]);
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
 * getblock - get a chunk of memory out of scratch_buf. When entering this
 * routine, mptr points to the next spot of free memory in scratch_buf, which
 * is returned to the calling routine. mptr is then adjusted appropriately
 * and points to the next spot in scratch_buf.
 */
static struct block *
getblock(int n)
{
	char *m, *b;

#ifdef	TRACE
	printf("GETBLOCK: n=0x%x, mptr=0x%x\n", n, mptr);
#endif	/* TRACE */

	n = ROUNDUP(n);

	b = mptr + n;
	if (b < &scratch_buf[scratch_max]) {
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
ide_free(void *mem)
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
	    (char *)b1 < scratch_buf ||
	    (char *)b1 >= &scratch_buf[scratch_max]) {
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

/* allocate a buffer with the specified alignment */
void *
ide_align_malloc(int nbytes, uint_t align)
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
		return ide_malloc(nbytes);

	if ((ptr = ide_malloc(nbytes + align)) == NULL)
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
