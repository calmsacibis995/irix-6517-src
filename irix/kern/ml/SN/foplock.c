/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1986-1996 Silicon Graphics, Inc.                        *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/immu.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/page.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/pda.h>

#if defined (SN0)
#include <sys/SN/SN0/hubdev.h>
#include <sys/SN/SN0/addrs.h>
#endif /* SN0 */


typedef	volatile __uint64_t	fopvar_t;
typedef volatile __uint32_t	fopvar32_t;
typedef __uint64_t		fop_bmap_t;

#ifndef	FETCHOP_VAR_SIZE
#define FETCHOP_VAR_SIZE 	64 /* 64 byte per fetchop variable */
#define FETCHOP_LOAD 		0
#define FETCHOP_INCREMENT 	8
#define FETCHOP_DECREMENT 	16
#define FETCHOP_CLEAR 		24
#define FETCHOP_STORE 		0
#define FETCHOP_AND 		24
#define FETCHOP_OR 		32
#endif	/* FETCHOP_VAR_SIZE */

#define MSPEC_PROTECTION_MASK 0xffffffffffffffffULL


#define	FETCHOP_VAR_INIT(p)	(*(fopvar_t *)((caddr_t)p + FETCHOP_CLEAR))

#define	FOPVAR_SIZE_SHFT	6
#define	FOPLOCK_SIZE		(1 << FOPVAR_SIZE_SHFT)
#define	FOPLOCKS_PERPAGE	(NBPP/FOPLOCK_SIZE)
#define	FOPLOCK_BITSPERVAR	(NBBY * sizeof(long))
#define	FOPLOCK_BITMAP_LEN	(FOPLOCKS_PERPAGE/FOPLOCK_BITSPERVAR)
#define	FOPLOCK_BITMASK		((fop_bmap_t)-1LL)

typedef struct 	foplock_list {
	fop_bmap_t	bitmap[FOPLOCK_BITMAP_LEN];
	short		free_lock_count;
	caddr_t		fop_page;
	caddr_t		sys_page;
	struct foplock_list	*next;
} foplock_list_t;

extern void 	*foplock_alloc(void);
extern void	foplock_free(void *);

foplock_list_t		*foplock_head;
lock_t			foplist_lock;
int			fop_inited;

extern int ms1bit(unsigned long x);
extern void poison_state_alter_range(__psunsigned_t, int, int);

#define	FOPVAR_OFFS(x)	 ((poff(x) >> FOPVAR_SIZE_SHFT) / FOPLOCK_BITSPERVAR)
#define	FOPVAR_BITINDX(x) ((poff(x) >> FOPVAR_SIZE_SHFT) % FOPLOCK_BITSPERVAR)

#define FETCHOP32_LOAD_OP(addr, op) (                                        \
         *(fopvar32_t *)((__psunsigned_t) (addr) + (op)))

#define FETCHOP32_STORE_OP(addr, op, x) (                                    \
         *(fopvar32_t *)((__psunsigned_t) (addr) + (op)) =              \
                (x))


static __inline fop32_inc(fopvar32_t *var) 
{
	return(FETCHOP32_LOAD_OP(var, FETCHOP_INCREMENT));
}

static __inline fop32_dec(fopvar32_t *var)
{
	return(FETCHOP32_LOAD_OP(var, FETCHOP_DECREMENT));
}

static void __inline fop32_store(fopvar32_t *var, int val)
{
	FETCHOP32_STORE_OP(var, FETCHOP_STORE, val);
}

void
foplock_init(void)
{
	spinlock_init(&foplist_lock, "foplock");
	foplock_head = 0;
}


caddr_t
foplock_pagealloc(void)
{
        caddr_t 	kvaddr;
	caddr_t		xkvaddr;
        int     	i;
	int		s;
	foplock_list_t *flist;	

        kvaddr = kvpalloc(1, VM_DIRECT|VM_UNCACHED_PIO, 0);
        ASSERT(kvaddr && IS_KSEG1(kvaddr));

        bzero(kvaddr, NBPP);

        for (i = 0; i < (NBPP/FETCHOP_VAR_SIZE); i++) {
                *(__uint64_t *)(kvaddr + ( i * FETCHOP_VAR_SIZE)) =
                                                MSPEC_PROTECTION_MASK;
        }

	if (private.p_bte_info) {
		PAGE_POISON(kvatopfdat(kvaddr));
	} else  {
		/* 
		 * If the lock allocation request comes very early, 
		 * BTEs are not yet initialized. So, we would need to 
		 * poison by mucking with the directory.
		 */
		poison_state_alter_range((__psunsigned_t)K1_TO_PHYS(kvaddr), NBPP, 1);
	}

	xkvaddr = kvaddr;
        XKPHYS_UNCACHADDR(xkvaddr, VM_UNCACH_MSPEC);

	flist = kmem_alloc(sizeof(struct foplock_list), KM_SLEEP);

	flist->free_lock_count = FOPLOCKS_PERPAGE;
	flist->fop_page = xkvaddr;
	flist->sys_page = kvaddr;
	flist->next = 0;

	for (i = 0; i < FOPLOCK_BITMAP_LEN; i++)
		flist->bitmap[i] = FOPLOCK_BITMASK;

	s = mutex_spinlock(&foplist_lock);

	if (foplock_head) {
		flist->next = foplock_head->next;
		foplock_head->next = flist;
	} else {
		foplock_head = flist;
	}
	mutex_spinunlock(&foplist_lock, s);

#ifdef	FOPLOCK_DEBUG
        cmn_err(CE_NOTE, "Returning 0x%x from fetchopvar_alloc\n", kvaddr);
#endif
	return kvaddr;
}


static fopvar_t *
foplock_search(void)
{
	int		s;
	foplock_list_t	*flist;
	fopvar_t	*fopvar;
	fop_bmap_t	*bitmap;

retry:

	s = mutex_spinlock(&foplist_lock);
	
	for (flist = foplock_head; flist; flist = flist->next) {

		int	i;
		if (flist->free_lock_count == 0) 
			continue; 
	
		bitmap = flist->bitmap;
		for (i = 0; i < FOPLOCK_BITMAP_LEN; i++) {
			int	n;
			int	bitnum;
			if (bitmap[i]) {

				bitnum = ms1bit(bitmap[i]);
				bitmap[i] &= ~((fop_bmap_t)1 << bitnum);
				n = (FOPLOCK_BITSPERVAR - 1 ) - bitnum;

				flist->free_lock_count--;
				mutex_spinunlock(&foplist_lock, s);

				n += ( i * FOPLOCK_BITSPERVAR);
				fopvar = (fopvar_t *)(flist->fop_page + (n * FETCHOP_VAR_SIZE));
				FETCHOP_VAR_INIT(fopvar);
				return fopvar;
			}
		}
	}
	mutex_spinunlock(&foplist_lock, s);

	if (foplock_pagealloc())
		goto retry;

	return	0;
}

void *
foplock_alloc()
{

	if (!fop_inited){
		fop_inited = 1;
		foplock_init();
	}
	

	return (void *)foplock_search();
	
}


void
foplock_free(void *var)
{

	int		s;
	foplock_list_t	*flist, *prevlist;
	fopvar_t	*fopvar = var;


	s = mutex_spinlock(&foplist_lock);
	prevlist = foplock_head;
	for (flist = foplock_head; flist; flist = flist->next) {
		int	freepage = 0;
		if (pnum(flist->fop_page) == pnum(fopvar)) {
			int	bitnum;
			int	offs;

			flist->free_lock_count++;

			offs = FOPVAR_OFFS(fopvar);
			bitnum = FOPVAR_BITINDX(fopvar); 
			bitnum = (FOPLOCK_BITSPERVAR - 1 - bitnum);
			if (flist->bitmap[offs] & ((fop_bmap_t)1 << bitnum)) {
#ifdef	FOPLOCK_DEBUG
				printf("fopvar 0x%x already free ? flist 0x%x \n",
					fopvar, flist); 
				debug("ring");
#endif	/* DEBUG */
			}
			flist->bitmap[offs] |= ((fop_bmap_t)1 << bitnum);

			if (flist->free_lock_count == FOPLOCKS_PERPAGE) {
				/* We can free this page as no locks 
				 * are in use in this page. 
				 */
				prevlist->next = flist->next; 
				freepage = 1;
			}
			mutex_spinunlock(&foplist_lock, s);
			if (freepage) {
#ifdef	FOPLOCK_DEBUG
				printf("Freeing page 0x%x struct 0x%x\n",
					flist->fop_page, flist);
#endif
				kvpfree(flist->sys_page, 1);
				kmem_free(flist, sizeof(foplock_list_t));
			}

			return;
		}
		prevlist = flist;
	}
	mutex_spinunlock(&foplist_lock, s);
	cmn_err(CE_PANIC, 
		"Could not find the list corresponding to fopvar 0x%x\n",
				fopvar);

}

/*
 * Interface for allocating and freeing Fetchop Variables. 
 * These are used to do the Atomic operatins. 
 */
void *
fopvar_alloc(void)
{

	return	foplock_alloc();
}

void
fopvar_free(void *fopvar)
{
	foplock_free(fopvar);
}


int
foplock_lock_spl(void *fop, splfunc_t splr)
{
	fopvar32_t	*fop_token = (fopvar32_t *)fop;
	volatile int	loopcount;

	int		s;
	fopvar32_t	val;

	while (1) {
		s = (*splr)();
		if ((val = fop32_inc(fop_token)) == 0 ) {
			fop32_store(fop_token+1, cpuid());	/* Save owner */
			return s;
		}

		fop32_dec(fop_token);
		splx(s);

		/* Assuming cache hit for loopcount (on stack) 
		 * dcache access time = 1 clock. 
		 * both instructions in the loop execute in 
		 * one cycle (as dec can be in BD slot. 
		 * So, loopcount of 1 => 1*512 cycle => 2.6us
		 */
		loopcount = val << 9;
		while (--loopcount);
	}
}

int
foplock_lock(void *fop)
{
	return foplock_lock_spl(fop, splhi);
}


void
foplock_unlock(void *fop, int s)
{
	fopvar32_t	*fop_token = (fopvar32_t *)fop;

	fop32_dec(fop_token); 
	fop32_store(fop_token+1, 0);
	splx(s);
}

void
foplock_nestedlock(void *fop)
{
	fopvar32_t	*fop_token = (fopvar32_t *)fop;
	fopvar32_t	val;

	while ((val = fop32_inc(fop_token)) > 0 ){
		fop32_dec(fop_token);
		DELAY(val);
	}
	fop32_store(fop_token+1, cpuid());		/* Save owner */
}

int
foplock_nestedtrylock(void *fop)
{
	fopvar32_t	*fop_token = (fopvar32_t *)fop;
	if (fop32_inc(fop_token) == 0) {
		fop32_store(fop_token+1, cpuid());
		return 1;
	} else {
		fop32_dec(fop_token);
		return 0;
	}
}

void
foplock_nestedunlock(void *fop)
{
	fop32_dec((fopvar32_t *)fop);
	fop32_store((fopvar32_t *)fop+1, 0);
}

