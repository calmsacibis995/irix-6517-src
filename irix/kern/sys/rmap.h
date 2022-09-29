/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1999, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef	__SYS_RMAP_H__
#define	__SYS_RMAP_H__

#ifdef	__cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.22 $"


/*
 * Data structures for reverse mapping.
 * 
 * Reverse mapping involves keeping track of the mapping from the 
 * physical page to the page table entries corresponding to the virtual
 * address of all the processes mapping this physical page.
 * 
 * Each physical page is represented by a pfdat data structure, and 
 * each pfdat has a pointer which could either be used as a pointer 
 * to the page table entry, or to point to the reverse map structure
 * which in turn keeps track of the list of pointers pointing to
 * page table entries
 *
 * Read comments in os/rmap.c for more details.
 */
 

/*
 * Bit mask in pf_pdep1 field that is used for rmap lock.
 */

#define	RMAP_LOCK_BIT		1

/*
 * If this flag is set in pf_pdep1, it indicates, pf_pdep2 points to an
 * rmap_t structure.
 */
#define	RMAP_LONG		2

#define	RMAP_FLAGS		(RMAP_LOCK_BIT|RMAP_LONG)



/*
 * Returns the pointer to the pte stored in pf_pdep1.
 */
#define	GET_RMAP_PDEP1(pfdp)	\
			((pde_t *)((__psunsigned_t)((pfdp)->pf_pdep1) & \
					(~RMAP_FLAGS)))

/*
 * Sets a pte pointer into the pf_pdep1 field.
 */
#define	SET_RMAP_PDEP1(pfdp, pdep)\
		((pfdp)->pf_pdep1 = ((pde_t *)(((__psunsigned_t)pdep) | \
				(((__psunsigned_t)((pfdp)->pf_pdep1) & \
                                        RMAP_FLAGS)))))

#define	SET_LONG_RMAP(pfdp)	\
		((pfdp)->pf_pdep1 \
		= ((pde_t *)((__psunsigned_t)((pfdp)->pf_pdep1) | RMAP_LONG)))

/*
 * Sets and clears RMAP_LONG flag pf_pdep1 field.
 */
#define	CLR_LONG_RMAP(pfdp)	\
		((pfdp)->pf_pdep1 \
	= ((pde_t *)((__psunsigned_t)((pfdp)->pf_pdep1) & (~RMAP_LONG))))

#define	IS_LONG_RMAP(pfdp) \
		((__psunsigned_t)((pfdp)->pf_pdep1) & RMAP_LONG)

/*
 * Operation definition for rmap_scanmap routine 
 * ALL NEW OPCODES SHOULD BE DEFINED IN KSYS/RMAP.H
 */
#define	RMAP_CLRVALID		0x1	/* Invalidate pfns for ref faulting */
#define	RMAP_SETVALID		0x2	/* Validate pfns */

#define	RMAP_ZEROPDE		0x4	/* Zero all the pdes mapping to the 
					 * given pfdat
					 */
#define	RMAP_SETPFN		0x8	/* Set the pfn in each pde  */
#define RMAP_LOCKPFN            0x10    /* Lock the pfn field in all ptes */
#define RMAP_UNLOCKPFN          0x20    /* Unlock the pfn field in all ptes */
#define RMAP_COUNTLINKS         0x40    /* Count the number of rmap links */
#define	RMAP_SHOOTPFN		0x80	/* Do needed to shoot down replicas */
#define RMAP_VERIFYLOCKS        0x100   /* Count # of locks taken */
#define	RMAP_SETPFN_AND_UNLOCK	0x200	/* Set the pfn in each pde to 
					 * given value & unlock. This is needed 
					 * for page migration operation.
					 */
#define	RMAP_CHECK_LPAGE	0x400	/* 
					 * Verify if pfdat is part of a 
					 * large page 
					 */
#ifdef	MH_R10000_SPECULATION_WAR
#define RMAP_MH_SPECULATION_WAR	0x800	/* R10K Moosehead workaround */
#endif

#define PDENULL         (pde_t *)0

/*
 * RMAP_LOCK passes the pfdat for which we want to lock the rmap
 */

#if _MIPS_SIM == _ABI64
#define	RMAP_LOCK(pfd)		\
			mutex_64bitlock((__uint64_t *)(&(pfd)->pf_pdep1), \
					RMAP_LOCK_BIT)
#define	RMAP_UNLOCK(pfd,T)	\
			mutex_64bitunlock((__uint64_t *)(&(pfd)->pf_pdep1), \
					RMAP_LOCK_BIT, (T))

#ifdef	MP
#define	RMAP_ISLOCKED(pfd)	\
	bitlock64bit_islocked((__uint64_t * )(&(pfd)->pf_pdep1), RMAP_LOCK_BIT)
#else
#define	RMAP_ISLOCKED(pfd)	\
	bitlock_islocked((uint *)(&(pfd)->pf_pdep1), RMAP_LOCK_BIT)
#endif

#define	NESTED_RMAP_LOCK(pfd) \
	nested_64bitlock((__uint64_t *)(&(pfd)->pf_pdep1), RMAP_LOCK_BIT)

#define	NESTED_RMAP_TRYLOCK(pfd) \
	nested_64bittrylock((__uint64_t *)(&(pfd)->pf_pdep1), RMAP_LOCK_BIT)

#define	NESTED_RMAP_UNLOCK(pfd) \
	nested_64bitunlock((__uint64_t *)(&(pfd)->pf_pdep1), RMAP_LOCK_BIT)
#else
#define	RMAP_LOCK(pfd)		\
			mutex_bitlock((uint *)(&(pfd)->pf_pdep1), \
					RMAP_LOCK_BIT)
#define	RMAP_UNLOCK(pfd,T)	\
			mutex_bitunlock((uint *)(&(pfd)->pf_pdep1), \
					RMAP_LOCK_BIT, (T))

#define	RMAP_ISLOCKED(pfd)	\
	bitlock_islocked((uint *)(&(pfd)->pf_pdep1), RMAP_LOCK_BIT)

#define	NESTED_RMAP_LOCK(pfd) \
	nested_bitlock((uint *)(&(pfd)->pf_pdep1), RMAP_LOCK_BIT)

#define	NESTED_RMAP_TRYLOCK(pfd) \
	nested_bittrylock((uint *)(&(pfd)->pf_pdep1), RMAP_LOCK_BIT)

#define	NESTED_RMAP_UNLOCK(pfd) \
	nested_bitunlock((uint *)(&(pfd)->pf_pdep1), RMAP_LOCK_BIT)

#endif 

/*
 * Return value of NESTED_RMAP_TRYLOCK on failure.
 */
#define	RMAP_LOCKFAIL	0	

/*
 * XXX
 * At this time, statistics gathering is enabled at all time. Once the code
 * goes through a few months of testing, it could be moved under #ifdef DEBUG. 
 */

#ifdef	RMAP_STATS
struct rmapinfo_s {
	__uint32_t	rmapfastadd;
	__uint32_t	rmapfastdel;
	__uint32_t	rmapadd;
	__uint32_t	rmapdel;
	__uint32_t	rmapinuse;
	__uint32_t	rmapscan;
	__uint32_t	rmapswap;
	__uint32_t	rmapladd;
	__uint32_t	rmapldel;
	__uint32_t	rmap_maxlen;
};

extern	struct rmapinfo_s	rmapinfo;
#define	RMAP_DOSTAT(x)	((rmapinfo.x)++)

#else
#define	RMAP_DOSTAT(x)
#endif /* RMAP_STATS */


/*
 * RMAP_ADDMAP/RMAP_DELMAP have been made macros to speed up the common case of
 * adding and deleting first two ptes to a page. These ptes reside in the pfdat.
 */

#define	RMAP_ADDMAP(pfdp, pdep, pm) \
	do { \
		int s; \
		s = RMAP_LOCK(pfdp); \
		RMAP_DOSTAT(rmapfastadd); \
		if (GET_RMAP_PDEP1(pfdp) == PDENULL) { \
			SET_RMAP_PDEP1(pfdp, pdep);\
			if (pfdp->pf_pdep2 == 0) { \
                                migr_start(pfdattopfn(pfdp), (pm)); \
                        } \
			RMAP_UNLOCK(pfdp, s); \
                } \
                else if (pfdp->pf_pdep2 == PDENULL) { \
                        ASSERT(GET_RMAP_PDEP1(pfdp) != NULL); \
                        ASSERT(pdep != GET_RMAP_PDEP1(pfdp)); \
                        pfdp->pf_pdep2 = pdep; \
			RMAP_UNLOCK(pfdp, s); \
                } \
		else { \
			RMAP_UNLOCK(pfdp, s); \
			rmap_addmap(pfdp, pdep, (pm)); \
		} \
	} while (0)


#define	RMAP_DELMAP(pfdp, pdep) \
	do { \
		int s; \
		s = RMAP_LOCK(pfdp); \
		RMAP_DOSTAT(rmapfastdel); \
		if (pg_isshotdn(pdep)) { \
                	RMAP_UNLOCK(pfdp, s); \
        	} else if (GET_RMAP_PDEP1(pfdp) == pdep) { \
			SET_RMAP_PDEP1(pfdp, 0); \
			if (pfdp->pf_pdep2 == 0) { \
				ASSERT(pfdp->pf_rmapp == 0); \
				migr_stop(pfdattopfn(pfdp)); \
			} \
			RMAP_UNLOCK(pfdp, s); \
		} else if (pfdp->pf_pdep2 == pdep) { \
                        pfdp->pf_pdep2 = 0; \
                        if (GET_RMAP_PDEP1(pfdp) == 0) { \
                                migr_stop(pfdattopfn(pfdp)); \
                        } \
                	RMAP_UNLOCK(pfdp, s); \
                } \
                else { \
			RMAP_UNLOCK(pfdp, s); \
			rmap_delmap(pfdp, pdep); \
                } \
        } while (0)


#ifdef	_KERNEL

struct pm;
extern void	rmap_init(void);
extern void	rmap_addmap(pfd_t *, pde_t *, struct pm *);
extern void	rmap_delmap(pfd_t *, pde_t *);
extern int	rmap_scanmap(pfd_t *, uint_t op, void *data);
extern void	rmap_swapmap(pfd_t *, pde_t *, pde_t *);
extern void	rmap_xfer (pfd_t *, pfd_t *);
extern int	idbg_rmaplog(void *);
extern void     idbg_rmapprint(pfn_t);
extern int	idbg_rmap_stats(void);

#endif	/* _KERNEL */

#ifdef	RMAP_DEBUG
extern void rmap_scantest(void);
#endif	/* RMAP_DEBUG */



#ifdef	__cplusplus
}
#endif

#endif	/* __SYS_RMAP_H__ */
