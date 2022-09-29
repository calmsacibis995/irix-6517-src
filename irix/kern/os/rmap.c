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

#include <sys/types.h>
#include <sys/immu.h>	
#include <sys/cmn_err.h>
#include <sys/atomic_ops.h>
#include <sys/pfdat.h>
#include <sys/pda.h>
#include <sys/kmem.h>
#include <sys/systm.h>	/* splhi */
#include <ksys/vproc.h>
#include <sys/debug.h>
#include <sys/sysmp.h>	/* numainfo */
#include <sys/idbgentry.h>
#include <sys/nodepda.h>
#include <sys/numa.h>
#include <ksys/rmap.h>
#include "os/as/pmap.h"

#ifdef DEBUG_PFNLOCKS
#include <sys/proc.h>
#endif

#include <sys/ktrace.h>
#include <ksys/vnode_pcache.h>

/*
 * Reverse Mapping layer (rmap)
 * rmap layer would be responsible for creating and maintaining the reverse
 * mapping from the pfdat structures to individual process page
 * table entries (ptes)
 */

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
 * The reverse map structure is growable table (array) of pte pointers. The 
 * smallest table is the size of a cache line (128 bytes) and the largest size 
 * matches the system page size. The first entry (8 bytes) of the table or in 
 * the case of 32 bit systems, the first two entries are used to keep 
 * housekeeping information. If the number of ptes per page grows beyond this, 
 * reverse map structure becomes a two level table with the first level table 
 * containing pointers to the second level table. The second level table
 * can have 32 entries. The maximum number of rmaps we can have in the system
 * is limited by the size of the pf_use field of the pfdat which is a short.
 * To begin with the entries in the table are linked into a singly linked list 
 * of free entries. The head of this list is kept in the rmap_freelist_head 
 * field of the rmap_t structure. When pte is added to the list, the first 
 * entry from free list is selected and used. The pte carries an rmap_index 
 * field where the index of the rmap entry is stored. This makes the deleting 
 * the pte entry from the rmap a simple operation. It is for this reason, the 
 * rmap structure was chosen to be a linear array. If the array is a two level 
 * array the the lower bits of the rmap index select the into the first level 
 * table, and the higher bits select the second level table. This enables us 
 * to not recalculate the indices of existing ptes in an rmap when a second 
 * level table is created.
 * Each second level table carries its own free list. This helps free the
 * table when all entries in it are free. 
 * The rmap structure is protected by its own lock. It resides in the least
 * significant bit of the pf_pdep1 field of the rmap. The next bit in the
 * field indicates if the pf_pdep2 field contains a pointer to a pte or
 * a reverse map structure.
 * To least significant bit of the first level table entry indicates if
 * that entry contains a pointer to a second level table or a pte.
 * The various arrays are allocated using zones. There is a zone for
 * each array size. The rmap_t carries a zone index which indicates the
 * zone from which that structure is allocated.
 * For systems where the pte is not big enough to hold the rmap index,
 * the array is sequentially searched to find the pte during the delete
 * operation. This should be the case for low end systems.
 */
#define	SUCCESS		0
#define	FAILURE		1

/*
 * Flag indicates that the first level entry is pointing to a second
 * level table. This is the lowest significant bit of the first level entry
 * which is guaranteed to be zero for a pointer of type 
 * rmap_second_level_table_t.
 */
#define	SECOND_LEVEL_TABLE_FLAG		1

/*
 * Starting index for first level table. We skip 0. It has housekeeping
 * entries.
 */

#if	_PAGESZ == 16384
#define MAX_RMAP_FIRST_LEVEL_TABLE_SIZE 2047
#define FIRST_LEVEL_INDEX_OFFSET        1

#elif   _PAGESZ == 4096

#define MAX_RMAP_FIRST_LEVEL_TABLE_SIZE 1022
#define FIRST_LEVEL_INDEX_OFFSET        2
#else
#error  "Unknown page size"
#endif

#define SECOND_LEVEL_TABLE_SIZE         32

/*
 * Max number of ptes that can be added to the rmap of a page.
 */
#define	MAX_NUM_RMAP_ENTRIES	(MAX_RMAP_FIRST_LEVEL_TABLE_SIZE * \
					SECOND_LEVEL_TABLE_SIZE)

/*
 * Max. first level table index.
 */
#define	MAX_RMAP_FIRST_LEVEL_INDEX	(MAX_RMAP_FIRST_LEVEL_TABLE_SIZE + \
						FIRST_LEVEL_INDEX_OFFSET)


/*
 * Indicates that the entry is free and does not contain a pte or a 
 * second level table pointer.
 */

#define	IS_FREE_ENTRY(table_ent)	(table_ent->freelist_index  \
						< MAX_NUM_RMAP_ENTRIES)
/*
 * Returns true if a first level entry points to a second level table.
 */
#define	IS_SECOND_LEVEL(table_ent)	\
				((__psint_t)((table_ent)->second_level_table) \
						& SECOND_LEVEL_TABLE_FLAG)

#define RMAPNULL        (rmap_t *)0

/*
 * Sets a flag in the first level entry to indicate that it points to
 * second level table.
 */
#define	SET_RMAP_SECOND_LEVEL_FLAG(table_ent) \
		(((table_ent)->second_level_table) = \
			((rmap_second_level_table_t *)\
			((__psint_t)((table_ent)->second_level_table) | \
				SECOND_LEVEL_TABLE_FLAG)))

#define	CLR_RMAP_SECOND_LEVEL_FLAG(second_level_table) \
		((second_level_table) = \
			((rmap_second_level_table_t *)\
			((__psint_t)(second_level_table) & \
				(~SECOND_LEVEL_TABLE_FLAG))))

/*
 * Get a pointer to the beginning of the first level table.
 */
#define	GET_FIRST_LEVEL_TABLE(rmap)	((table_ent_t *)\
					((caddr_t)(rmap) + sizeof(rmap_t)))


/*
 * Get the second level table pointer given the first level entry. This 
 * just gets the pointer and clears the SECOND_LEVEL_TABLE_FLAG.
 */
#define	GET_SECOND_LEVEL_TABLE(first_level_entry) \
	((rmap_second_level_table_t *)((__psint_t)\
	((first_level_entry)->second_level_table) & (~SECOND_LEVEL_TABLE_FLAG)))

/*
 * Gets the second level table entry given first level entry and the second
 * level table index.
 */
#define	GET_SECOND_LEVEL_ENTRY(first_level_entry, indx) \
	((GET_SECOND_LEVEL_TABLE(first_level_entry))->table_entries + (indx))


/*
 * Get a pointer to the entry in the first level table for a given
 * index.
 */

#define	GET_FIRST_LEVEL_ENTRY(rmap, indx)	(table_ent_t *)\
					((table_ent_t *)(rmap) + (indx))

/*
 * Given the rmap_index get the first level and second level indices.
 */

#define	GET_FIRST_LEVEL_INDEX(rmap_index)	\
		((rmap_index) % MAX_RMAP_FIRST_LEVEL_INDEX)

#define	GET_SECOND_LEVEL_INDEX(rmap_index)	\
		((rmap_index) / MAX_RMAP_FIRST_LEVEL_INDEX)

/*
 * Compute the rmap index given the first and second level indices.
 */
#define	RMAP_INDEX(first_level_index, second_level_index) \
				((second_level_index) * \
					MAX_RMAP_FIRST_LEVEL_INDEX + \
				(first_level_index))

/*
 * Gets the first level table size of a given rmap.
 */ 
#define	FIRST_LEVEL_TABLE_SIZE(rmap)	(rmap_table_sizes[(rmap)\
					->rmap_zone_index])


typedef	unsigned short	table_index_t;

/*
 * The first level table entry can either point to a pte or a second
 * level table entry.
 */

typedef union {
	pde_t				*pdep;
	struct rmap_second_level_table 	*second_level_table;

	/*
	 * This is a ulong instead of table_index_t to match the size of the 
	 * other entries.
	 * Assignmet to freelist_index should clear other bits in the field.
	 */

	ulong				freelist_index;
} table_ent_t;

/*
 * Second level table. It has a count and a free list head in addition
 * to the table.
 */
typedef struct  rmap_second_level_table {
        cnt_t		num_free_entries;
	table_index_t	freelist_head;
        table_ent_t	table_entries[SECOND_LEVEL_TABLE_SIZE];
} rmap_second_level_table_t;

typedef struct rmap {
	table_index_t 		rmap_freelist_head; /* Head of free list */
	table_index_t		rmap_zone_index;
	table_index_t		rmap_freetable_hint;
	pf_use_t		num_ptes;
} rmap_t;


#if	_PAGESZ == 16384

static int rmap_table_sizes[] = {
	15,     /* This + rmap_t size fits into a cache line (128 bytes) */
	31,     /* 256 bytes */
	47,     /* 384 bytes */
	63,     /* 512 bytes */
	95,     /* 768 bytes */
	127,    /* 1024 bytes */
	191,    /* 1536 bytes */
	255,    /* 2048 bytes */
	511,    /* 4096 bytes */
	1023,   /* 8192 bytes */
	MAX_RMAP_FIRST_LEVEL_TABLE_SIZE, /* Fits exactly into a page */
};

#elif   _PAGESZ == 4096

static int rmap_table_sizes[] = {
	  30,   /* This + rmap_t size fits into a cache line */
	  62,   /* 256 bytes */
	  94,   /* 384 bytes */
	 126,   /* 512 bytes */
	 254,
	MAX_RMAP_FIRST_LEVEL_TABLE_SIZE, /* Fits exactly into a page */
};

#else
#error  "Unknown page size"
#endif

#define	NUM_RMAP_TABLE_SIZES	(sizeof(rmap_table_sizes)/sizeof(int))

/*
 * Max. zone index value.
 */
#define	MAX_RMAP_ZONE_INDEX		(NUM_RMAP_TABLE_SIZES - 1)

/* For scalability purpose, we would need one zone per node. 
 * In that case, rmap_init would be called once for each node?
 * Perhaps there would be a per node data structure where this 
 * pointer could be attached.
 */


/*
 * Zone for first level table.
 */
zone_t	*rmap_zone[NUM_RMAP_TABLE_SIZES];

/*
 * Zone for second level table.
 */
zone_t	*second_level_table_zone;

/* Any static references go here */
static void 	rmap_init_freelist(table_ent_t *, table_index_t, table_index_t);
static int	rmap_grow(pfd_t *, pde_t *);
static int	rmap_second_level_table_add(pfd_t *, pde_t *);
static int	rmap_grow_second_level_table(rmap_t *, pde_t *);
static int	rmap_doop(uint, pde_t *, void *, void *);
static int 	rmap_lockpfns(pfd_t *);
static int	rmap_mark_shotdown(pfd_t *);
static int      rmap_verify_locks(pfd_t *, int);
#ifdef MH_R10000_SPECULATION_WAR
static int rmap_invalidate_uptbl_entry(pde_t *pde, void *counts);
#endif


/*
 * Debugging and statistics.
 */

#ifdef	RMAP_STATS
struct rmapinfo_s	rmapinfo;
#endif

/* IP22 has a space crunch - debug kernel needs to fit in 7.25M. */
/* CELL_IRIX also has a space crunch too since it uses MAPPED_KERNEL */
#if defined(DEBUG) && !defined(IP22) && !defined(CELL_IRIX) && !defined(SN0XXL)
#define	RMAP_DEBUG 
#endif	/* DEBUG */

#ifdef	RMAP_DEBUG
/* Setup a circular list, to track the operation done on 
 * the pde/pfdat passed
 */
extern void idbg_rmapprint(pfn_t);

#define	RMAP_DBGENTS	1024*16 /* Power of 2 */
#define	RMAP_DBGMASK	(RMAP_DBGENTS - 1)

struct rmap_dbgchain {
	char	rmap_op;
	char	rmap_fl;
	short	rmap_pid;
	int	rmap_tm;
	pde_t	*rmap_pdep;
	pfd_t	*rmap_pfdp;
	void	*rmap_ra;
} rmap_dbglist[RMAP_DBGENTS];

int	rmap_dbgindx = 0;
extern	time_t lbolt;

#define RMAP_ADDMAPLOG          1
#define RMAP_DELMAPLOG          2
#define	RMAP_SWAPMAP		3
#define	RMAP_SCANMAP		4
#define RMAP_ADDMAP_FIRST       5
#define RMAP_DELMAP_LAST        6 

#define	rflag(pf)		(IS_LONG_RMAP(pf)  ? 1 : 0)
#define	RMAP_LOGENT(o,pf,pd,ra)	rmap_dbglist[rmap_dbgindx].rmap_op = o;    \
				rmap_dbglist[rmap_dbgindx].rmap_fl= rflag(pf);\
			    	rmap_dbglist[rmap_dbgindx].rmap_tm = lbolt; \
			    	rmap_dbglist[rmap_dbgindx].rmap_pdep = pd; \
			    	rmap_dbglist[rmap_dbgindx].rmap_pfdp = pf; \
			    	rmap_dbglist[rmap_dbgindx].rmap_ra =(void *)ra;\
				rmap_dbglist[rmap_dbgindx].rmap_pid = \
					current_pid(); \
			   	rmap_dbgindx = ++rmap_dbgindx & RMAP_DBGMASK;

#define	RMAP_MAXCALLERS		16
#define	RMAP_ADDMAPCLR		0
#define	RMAP_DELMAPCLR		1
unsigned long rmap_callers[2][RMAP_MAXCALLERS];

static	void	rmap_logcallers(int, unsigned long);

#define	RMAP_LOGCALLER(x, y)	rmap_logcallers(x, (unsigned long)y)

#else	/* !RMAP_DEBUG */

#define	RMAP_LOGENT(x,y,z,p)

#define	RMAP_ADDMAPCLR		0
#define	RMAP_DELMAPCLR		1
#define	RMAP_LOGCALLER(x, y)

#endif	/* RMAP_DEBUG */

#ifdef  DEBUG_PFNLOCKS
extern void rmap_verify_lock_consistency(pfd_t*);
#define RMAP_VERIFY_LOCK_CONSISTENCY(pfdp)  rmap_verify_lock_consistency((pfdp))
#else /* ! DEBUG_PFNLOCKS */
#define RMAP_VERIFY_LOCK_CONSISTENCY(pfdp)
#endif /* ! DEBUG_PFNLOCKS */



/* 
 * rmap_init : Initialize rmap specific data structures 
 * Called from pmap_init 
 * Initialize all the zone for the different sizes.
 */
void 
rmap_init(void)
{

	int	i;

	for ( i = 0; i < NUM_RMAP_TABLE_SIZES; i++) {
		rmap_zone[i] = kmem_zone_init(sizeof(rmap_t) 
			+ rmap_table_sizes[i] * sizeof(table_ent_t), "Rmap");
		ASSERT(rmap_zone[i]);
	}
	second_level_table_zone = kmem_zone_init(sizeof(rmap_second_level_table_t),
				"Rmap second level table");

#ifdef	RMAP_DEBUG
	idbg_addfunc("rmaplog", (void (*)())idbg_rmaplog);
	idbg_addfunc("rmapp", (void (*)())idbg_rmapprint);
#endif
#ifdef	RMAP_STATS
	idbg_addfunc("rmapstat", (void (*)())idbg_rmap_stats);
#endif
}


/*
 * Grow the first level table or add a second level table to the rmap.
 * Called when no more free entries are left in the current rmap.
 */
static int
rmap_grow(pfd_t *pfdp, pde_t *pdep)
{
	table_index_t		new_zone_index, new_table_size, 
				cur_table_size;
	table_ent_t		*new_table, *cur_table;
	rmap_t			*new_rmap;
	rmap_t			*rmap = pfdp->pf_rmapp;
	pde_t			*old_pdep;
	table_index_t		start_index;
	

	if (IS_LONG_RMAP(pfdp)) {
		/*
		 * Nothing in the free list.
		 */

		ASSERT(rmap->rmap_freelist_head == 0);

		if (rmap->rmap_zone_index >= MAX_RMAP_ZONE_INDEX) {
			return rmap_grow_second_level_table(rmap, pdep);
		} else {
			new_zone_index = rmap->rmap_zone_index + 1;

			new_table_size = rmap_table_sizes[new_zone_index];
		}

		cur_table = GET_FIRST_LEVEL_TABLE(rmap);

		cur_table_size = rmap_table_sizes[
				rmap->rmap_zone_index];

		new_rmap = kmem_zone_zalloc(rmap_zone[new_zone_index], 
						KM_NOSLEEP);
		if (new_rmap == NULL) return 0;

		new_table = GET_FIRST_LEVEL_TABLE(new_rmap);

		new_rmap->rmap_zone_index = new_zone_index;

		/*
		 * Copy the old contents to the new list.
		 */

		bcopy(cur_table, new_table, cur_table_size*sizeof(table_ent_t));

		/*
		 * Get to the start of the free entry.
		 */

		new_table += cur_table_size;
		
		new_table->pdep = pdep;

		start_index = cur_table_size + FIRST_LEVEL_INDEX_OFFSET;

		pg_set_rmap_index(pdep, start_index);

		new_table++;
		new_rmap->num_ptes = rmap->num_ptes + 1;
		start_index++;

		/*
		 * Initialize the freelist.
		 */


		new_rmap->rmap_freelist_head = start_index;


		rmap_init_freelist(new_table, start_index,
					new_table_size - cur_table_size - 1);
		/*
		 * Free the old table.
		 */

		pfdp->pf_rmapp = new_rmap;

		kmem_zone_free(rmap_zone[new_zone_index - 1], rmap);

	} else {

		new_table_size = rmap_table_sizes[0];
		/*
		 * By allocating one chunk we get the whole data structure
		 * in one cache line.
		 */

		new_rmap = kmem_zone_zalloc(rmap_zone[0],  KM_NOSLEEP);

		if (new_rmap == NULL) return 0;

		new_rmap->rmap_zone_index = 0;

		new_table = GET_FIRST_LEVEL_TABLE(new_rmap);

		start_index = FIRST_LEVEL_INDEX_OFFSET;

		old_pdep = pfdp->pf_pdep2;

		new_table->pdep = old_pdep;
		pg_set_rmap_index(old_pdep, start_index);
		new_table++; 
		start_index++;

		new_table->pdep = pdep;

		pg_set_rmap_index(pdep, start_index);
		new_table++;
		start_index++;

		/*
		 * Set the rmap in the pfdat structure.
		 */

		pfdp->pf_rmapp = new_rmap;

		SET_LONG_RMAP(pfdp);
		
		/*
		 * Initialize the freelist.
		 */

		new_rmap->num_ptes = 2;

		new_rmap->rmap_freelist_head = start_index;
		
		rmap_init_freelist(new_table, start_index, new_table_size 
				- start_index + FIRST_LEVEL_INDEX_OFFSET);
	}

	return 1;
}


/*
 * Initialize the rmap freelist.
 */
static void
rmap_init_freelist(table_ent_t *table, table_index_t start_index, 
				table_index_t table_length)
{
	table_ent_t 	*table_end;
	
	table_end = table + table_length - 1;
	start_index++;
	for (;table < table_end; table++)
		table->freelist_index = start_index++;

	table->freelist_index = 0;
}

static int
rmap_grow_second_level_table(rmap_t *rmap, pde_t *pdep)
{
	table_ent_t		*first_level_entry;
	pde_t			*tmp_pdep;
	rmap_second_level_table_t *second_level_table;
	table_index_t		first_level_index, 
				second_level_index;
	table_ent_t		*second_level_entry;

	ASSERT(rmap->rmap_zone_index == MAX_RMAP_ZONE_INDEX);


	if (rmap->num_ptes == MAX_NUM_RMAP_ENTRIES) {
		cmn_err(CE_PANIC,"Trying to grow rmaps beyond size %d\n",
				MAX_NUM_RMAP_ENTRIES);
		/* NOTREACHED */
	}

	first_level_entry = GET_FIRST_LEVEL_TABLE(rmap);

	/*
	 * Nothing in the free list.
	 */

	ASSERT(rmap->rmap_freelist_head == 0);

	for (first_level_index = FIRST_LEVEL_INDEX_OFFSET; 
			first_level_index < MAX_RMAP_FIRST_LEVEL_INDEX; 
				first_level_index++, first_level_entry++) {

		if (!IS_SECOND_LEVEL(first_level_entry)) {

			second_level_index = 0;

			tmp_pdep = first_level_entry->pdep;

			second_level_table = kmem_zone_zalloc(
					second_level_table_zone, KM_NOSLEEP);

			if (second_level_table == NULL) return 0;

			second_level_entry = 
					second_level_table->table_entries;

			first_level_entry->second_level_table = 
						second_level_table;

			/*
			 * Add the two ptes.
			 * One was in the first_level_entry and the
			 * other is the one for which the addmap was 
			 * invoked.
			 */

			second_level_entry->pdep = tmp_pdep;
			second_level_entry++;

			pg_set_rmap_index(tmp_pdep, 
				RMAP_INDEX(first_level_index, 
						second_level_index));

			second_level_index++;
			second_level_entry->pdep = pdep;
			second_level_entry++;
			pg_set_rmap_index(pdep, 
				RMAP_INDEX(first_level_index, 
					second_level_index));

			second_level_index++;

			second_level_table->freelist_head = second_level_index;

			SET_RMAP_SECOND_LEVEL_FLAG(first_level_entry);

			/*
			 * Update count field.
			 */
			second_level_table->num_free_entries = 
						SECOND_LEVEL_TABLE_SIZE - 
							second_level_index;

			rmap->rmap_freetable_hint = first_level_index;

			rmap_init_freelist(second_level_entry, 
					second_level_index,
					SECOND_LEVEL_TABLE_SIZE 
						- second_level_index);
			break;
		}
	}

	rmap->num_ptes++;

	if ( first_level_index == MAX_RMAP_FIRST_LEVEL_INDEX)
		cmn_err(CE_PANIC, "Ran out of rmap entries\n");
	return 1;
}

/*
 * rmap_addmap:
 * Add a reverse mapping to the given pfdat.
 * pfdp points to the pfdat mapping the physical page, and 
 * pdep points to the page table entry which would be
 * mapping to physical page represented by pfdp
 *
 * If either of the two pde pointers in pfdat is free, use them to 
 * point to pdep. 
 *
 * If there is an rmap structure, then check if there is a place for
 * attaching the new pointer, and do so if possible. Otherwise, 
 * allocate a new rmap_t structure, and setup the pointers appropriately.
 *
 * Return 0 to indicate succesful insertion, 1 to indicate the
 * caller should go sxbrk due to memory shortage, and retry operation.
 */
/* ARGSUSED */
int
rmap_addmap_nolock(pfd_t *pfdp, pde_t *pdep, struct pm* pm)
{ 
	rmap_t	*pf_rmap = 0;
	table_index_t	free_entry_index;
	table_ent_t	*rmap_free_entry;

	ASSERT(pfdp);
	ASSERT(pdep);
	/*
	 * It would be nice to assert that the pte has a pfn that
	 * corresponds to the pfdat, but that is not true if the
	 * call comes out of VPAG_UPDATE_RMAP_ADDMAP.
	 */

	ASSERT(pfdp->pf_use);	/* There should be >= 1 user of this page!! */

	RMAP_DOSTAT(rmapadd); 
	
	
	RMAP_LOGENT(RMAP_ADDMAPLOG, pfdp, pdep, __return_address);
	RMAP_LOGCALLER(RMAP_ADDMAPCLR, __return_address);


		/* If there is place in the pfdat for this pde use it */
		if (GET_RMAP_PDEP1(pfdp) == PDENULL){
			ASSERT(pdep != pfdp->pf_pdep2);
			SET_RMAP_PDEP1(pfdp, pdep);

			if (pfdp->pf_pdep2 == 0) {
				/* 
				 * This is the first pde in the map, 
				 * start the migration for the page.
				 */

				migr_start(pfdattopfn(pfdp), pm);
				RMAP_LOGENT(RMAP_ADDMAP_FIRST, pfdp, pdep, __return_address);
			}
			return 0;
		}
		else if (pfdp->pf_pdep2 == PDENULL) {
			ASSERT(GET_RMAP_PDEP1(pfdp) != NULL);
			ASSERT(pdep != GET_RMAP_PDEP1(pfdp));
			pfdp->pf_pdep2 = pdep;

			return 0;
		}
		else if (IS_LONG_RMAP(pfdp)) {
			pf_rmap = pfdp->pf_rmapp;
			free_entry_index = pf_rmap->rmap_freelist_head;
			if (free_entry_index) {
				RMAP_DOSTAT(rmapladd);
				rmap_free_entry = 
					GET_FIRST_LEVEL_ENTRY(pf_rmap, 
							free_entry_index);
				pf_rmap->rmap_freelist_head = (table_index_t)
						rmap_free_entry->freelist_index;
				rmap_free_entry->pdep = pdep;
				pg_set_rmap_index(pdep, free_entry_index);
				pf_rmap->num_ptes++;

				return 0;
			} else if (pf_rmap->rmap_zone_index 
						== MAX_RMAP_ZONE_INDEX) {

				if (rmap_second_level_table_add(pfdp, pdep)) {
					return 0;
				}
			}
		}

		/* Looks like the current rmap structure does not have 
		 * sufficient memory to drop in a new pde entry. So, we
		 * need to grow the rmap structure, and use that.
		 */

		if (rmap_grow(pfdp, pdep)) {
			RMAP_DOSTAT(rmapladd);
			RMAP_DOSTAT(rmapinuse); 
			return 0;
		} else {
			return 1;
		}
}

void
rmap_addmap(pfd_t *pfdp, pde_t *pdep, struct pm* pm)
{
	int s = RMAP_LOCK(pfdp);

	while (rmap_addmap_nolock(pfdp, pdep, pm)) {
		RMAP_UNLOCK(pfdp, s);
		setsxbrk();
		s = RMAP_LOCK(pfdp);
	}
	RMAP_UNLOCK(pfdp, s);
}


/*
 * Try and find a place for the pdep in the second level table.
 */

static int
rmap_second_level_table_add(pfd_t *pfdp, pde_t *pdep)
{
	rmap_t	*pf_rmap;

	table_ent_t	*rmap_free_entry, 
			*first_level_entry;
	/* REFERENCED */
	table_index_t	free_entry_index, 
			first_level_index, 
			second_level_index;
	table_index_t	i;

	rmap_second_level_table_t	*second_level_table;

	ASSERT(IS_LONG_RMAP(pfdp));


	pf_rmap = pfdp->pf_rmapp;

	/* Search for a second level table entry */

	first_level_index = pf_rmap->rmap_freetable_hint;

	/*
	 * Hint is NULL. Try to grow second level table.
	 */
	if (first_level_index == 0) return 0;

	first_level_entry = GET_FIRST_LEVEL_ENTRY(pf_rmap, first_level_index);

	for ( i = FIRST_LEVEL_INDEX_OFFSET; 
			i < MAX_RMAP_FIRST_LEVEL_INDEX; i++) {

		second_level_table = GET_SECOND_LEVEL_TABLE(first_level_entry);

		if (IS_SECOND_LEVEL(first_level_entry)
			&& (second_level_table->num_free_entries)) {


			second_level_index = 
				second_level_table->freelist_head;

			rmap_free_entry = 
				second_level_table->table_entries +
				second_level_index;
			
			second_level_table->freelist_head = (table_index_t)
					rmap_free_entry->freelist_index;
			second_level_table->num_free_entries--;

			pf_rmap->rmap_freetable_hint = first_level_index;

			free_entry_index = 
				RMAP_INDEX(first_level_index,
					second_level_index); 

			rmap_free_entry->pdep = pdep;
			pg_set_rmap_index(pdep, free_entry_index);
			pf_rmap->num_ptes++;

			return 1;
		}

		first_level_index++;
		first_level_entry++;

		if (first_level_index == MAX_RMAP_FIRST_LEVEL_INDEX) {
			first_level_index = FIRST_LEVEL_INDEX_OFFSET;
			first_level_entry = GET_FIRST_LEVEL_ENTRY(pf_rmap, 
						first_level_index);
		}
	}

	return 0;
}


#ifdef	RMAP_PTE_INDEX

/* 
 * Delete pde entry from the reverse map 
 *
 * If the pde to be deleted is the last pde in the rmap, we will 
 * disable the migration interrupt for the page since it is no longer
 * associated with any other address space.  And, logically, we will
 * shut down the migration activities associated with the page.
 */

void      
rmap_delmap_nolock(pfd_t *pfdp, pde_t *pdep)
{
	rmap_t				*pf_rmap;
	table_index_t			rmap_index;
	table_index_t			first_level_index;
	table_index_t			second_level_index;
	rmap_second_level_table_t	*second_level_table;
	table_ent_t			*second_level_entry;
	table_ent_t			*first_level_entry;

	ASSERT((pfdp && pdep && (pfdattopfn(pfdp) == pg_getpfn(pdep))) || 
		pg_isshotdn(pdep));

	RMAP_DOSTAT(rmapdel);

	if (pg_isshotdn(pdep)) {
		/* We are racing with a process that is shooting down
		 * replicated pages. pdep seems to be one of those ptes
		 * that was mapping a page which got tossed out.
		 * Since process shooting down the pages has taken care
		 * of disassociating this process from the page, we just
		 * need to return without doing anything.
		 *
		 * Since either the pfn is locked or the pfdat is held,
		 * we dont really need to hold the memory_lock to check
		 * if pdep has been shot down.
		 */
		return;
	}

	RMAP_LOGENT(RMAP_DELMAPLOG, pfdp, pdep, __return_address);
	RMAP_LOGCALLER(RMAP_DELMAPCLR, __return_address);

	if (GET_RMAP_PDEP1(pfdp) == pdep) {
		SET_RMAP_PDEP1(pfdp, 0);

		if (pfdp->pf_pdep2 == 0) {

			ASSERT(pfdp->pf_rmapp == 0);

			/* 
			 * This pde is the last one in the map,
			 * stop the migration for this page frame
			 */
			migr_stop(pfdattopfn(pfdp));
			RMAP_LOGENT(RMAP_DELMAP_LAST, pfdp, pdep, 
				    __return_address);
		} else {
			ASSERT(RMAP_ISLOCKED(pfdp));
		}

		return;
	}


	if (IS_LONG_RMAP(pfdp) == 0) {

		if (pfdp->pf_pdep2 == pdep) {
			pfdp->pf_pdep2 = 0;

			if (GET_RMAP_PDEP1(pfdp) == 0) {
				/* 
				 * This pde is the last one in the map,
				 * stop the migration for this page frame
				 */
				migr_stop(pfdattopfn(pfdp));
				RMAP_LOGENT(RMAP_DELMAP_LAST, pfdp, pdep, 
					    __return_address);
			} 
		}
		else {
			cmn_err(CE_PANIC,
				"rmap_del: pde 0x%x not in pfdat 0x%x",
				pdep, pfdp);
		}

		return;
	}

	RMAP_DOSTAT(rmapldel);

	pf_rmap = pfdp->pf_rmapp;

	rmap_index = (table_index_t)pg_get_rmap_index(pdep);

	first_level_index = GET_FIRST_LEVEL_INDEX(rmap_index);

	ASSERT(first_level_index);
	ASSERT(first_level_index < (FIRST_LEVEL_TABLE_SIZE(pf_rmap)
					+ FIRST_LEVEL_INDEX_OFFSET));

	first_level_entry = GET_FIRST_LEVEL_ENTRY(pf_rmap, first_level_index);

	if (IS_SECOND_LEVEL(first_level_entry)) {

		second_level_index = GET_SECOND_LEVEL_INDEX(rmap_index);

                ASSERT (second_level_index < SECOND_LEVEL_TABLE_SIZE);

		second_level_table = GET_SECOND_LEVEL_TABLE(first_level_entry);

		second_level_entry = second_level_table->table_entries 
						+ second_level_index;

		ASSERT(pdep == second_level_entry->pdep);

		second_level_table->num_free_entries++;

		second_level_entry->freelist_index = 
					second_level_table->freelist_head;
		second_level_table->freelist_head = second_level_index;

		/*
		 * Lets see if its time to free the table.
		 */
		if (second_level_table->num_free_entries == 
						SECOND_LEVEL_TABLE_SIZE) {
			kmem_zone_free(second_level_table_zone, 
				second_level_table);
			
			first_level_entry->freelist_index = 
					pf_rmap->rmap_freelist_head;
			pf_rmap->rmap_freelist_head = first_level_index;
		}

		pf_rmap->num_ptes--;

		if (pf_rmap->num_ptes == 0) {
			CLR_LONG_RMAP(pfdp);
			pfdp->pf_rmapp = RMAPNULL;
			if (GET_RMAP_PDEP1(pfdp) == 0) {
				/*
				 * This deleted pde is the last one
				 * in the map, stop the migration
				 * activiti for this page frame
				 */
				migr_stop(pfdattopfn(pfdp));
				RMAP_LOGENT(RMAP_DELMAP_LAST, pfdp, pdep,
					    __return_address);
			}
			/*
			 * Free the first level table.
			 */
			kmem_zone_free(
				rmap_zone[pf_rmap->rmap_zone_index], pf_rmap);

			return;
		}

		return;
	} else {
		ASSERT(pdep == first_level_entry->pdep);
		ASSERT(rmap_index == first_level_index); 

		first_level_entry->freelist_index = pf_rmap->rmap_freelist_head;
		pf_rmap->rmap_freelist_head = first_level_index;
		pf_rmap->num_ptes--;

		if (pf_rmap->num_ptes == 0) {

			/*
			 * Free the first level table.
			 */
			CLR_LONG_RMAP(pfdp);
			pfdp->pf_rmapp = RMAPNULL;
			if (GET_RMAP_PDEP1(pfdp) == 0) {
				/*
				 * This deleted pde is the last one
				 * in the map, stop the migration
				 * activiti for this page frame
				 */
				migr_stop(pfdattopfn(pfdp));
				RMAP_LOGENT(RMAP_DELMAP_LAST, pfdp, pdep,
					    __return_address);
			}
			/*
			 * Free the first level table.
			 */
			kmem_zone_free(
				rmap_zone[pf_rmap->rmap_zone_index], pf_rmap);

			return;
		}

		return;
	}
}

/*
 * rmap_swapmap:
 * 	Search for the place where opde exists, and replace it with npde.
 * 	At this time this's called only from seg_commonsplit, which is 
 *	trying to separate the shared and private areas for sproc processes,
 *	and from replace_pmap, which is trying to replace the pmap for
 *	a process which is switching abis
 */
void      
rmap_swapmap(pfd_t *pfdp, pde_t *opde, pde_t *npde )
{

	rmap_t	*pf_rmap;
	int	s;
	table_index_t	rmap_index, first_level_index, second_level_index;
	table_ent_t	*first_level_entry, *second_level_entry;

	RMAP_DOSTAT(rmapswap);

	RMAP_LOGENT(RMAP_SWAPMAP, pfdp, opde, __return_address);

	s = RMAP_LOCK(pfdp);
	
	if (GET_RMAP_PDEP1(pfdp) == opde) {
		SET_RMAP_PDEP1(pfdp, npde);
		RMAP_UNLOCK(pfdp, s);
		return;
	}

	if (!(IS_LONG_RMAP(pfdp))) {
		ASSERT(pfdp->pf_pdep2 == opde);	/* it better be true */
		pfdp->pf_pdep2 = npde;
		RMAP_UNLOCK(pfdp, s);
		return;
	}

	/* This pfdat has one or more users. */
	ASSERT(pfdp->pf_use >= 1);

	rmap_index = (table_index_t)pg_get_rmap_index(opde);

	first_level_index = GET_FIRST_LEVEL_INDEX(rmap_index);

	pf_rmap = pfdp->pf_rmapp;

	ASSERT(first_level_index < (FIRST_LEVEL_TABLE_SIZE(pf_rmap)
						+ FIRST_LEVEL_INDEX_OFFSET));

	first_level_entry = GET_FIRST_LEVEL_ENTRY(pf_rmap, first_level_index);

	if (IS_SECOND_LEVEL(first_level_entry)) {

		second_level_index = GET_SECOND_LEVEL_INDEX(rmap_index);

                ASSERT (second_level_index < SECOND_LEVEL_TABLE_SIZE);

		second_level_entry = GET_SECOND_LEVEL_ENTRY(first_level_entry, 
						second_level_index);

		ASSERT(opde == second_level_entry->pdep);

		second_level_entry->pdep = npde;
		pg_set_rmap_index(npde, rmap_index);
		pg_set_rmap_index(opde, 0);

		RMAP_UNLOCK(pfdp, s);
		return;
	} else {
		ASSERT(opde == first_level_entry->pdep);
		ASSERT(rmap_index == first_level_index); 

		/*
		 * Pdep is cleared before entering the freelist index.
		 * as freelist index is shorter (2 bytes) than pdep (8 bytes)
		 * and they share the same memory location.
		 */

		first_level_entry->pdep = npde;
		pg_set_rmap_index(npde, rmap_index);
		pg_set_rmap_index(opde, 0);

		RMAP_UNLOCK(pfdp, s);
		return;
	}
		
	/*NOTREACHED*/
}

#else /* RMAP_PTE_INDEX */
/*
 * Versioon of rmap_delmap with no support for pte indices.
 * Need by 32 bit systems.
 */

void      
rmap_delmap_nolock(pfd_t *pfdp, pde_t *pdep)
{
	rmap_t				*pf_rmap;
	int				i, j;
	table_index_t			first_level_index;
	rmap_second_level_table_t	*second_level_table;
	table_ent_t			*second_level_entry;
	table_ent_t			*first_level_entry;

	ASSERT((pfdp && pdep && (pfdattopfn(pfdp) == pg_getpfn(pdep))) || 
		pg_isshotdn(pdep));

	RMAP_DOSTAT(rmapdel);

	if (pg_isshotdn(pdep)) {
		/* We are racing with a process that is shooting down
		 * replicated pages. pdep seems to be one of those ptes
		 * that was mapping a page which got tossed out.
		 * Since process shooting down the pages has taken care
		 * of disassociating this process from the page, we just
		 * need to return without doing anything.
		 *
		 * Since either the pfn is locked or the pfdat is held,
		 * we dont really need to hold the memory_lock to check
		 * if pdep has been shot down.
		 */
		return;
	}

	RMAP_LOGENT(RMAP_DELMAPLOG, pfdp, pdep, __return_address);
	RMAP_LOGCALLER(RMAP_DELMAPCLR, __return_address);

	if (GET_RMAP_PDEP1(pfdp) == pdep) {
		SET_RMAP_PDEP1(pfdp, 0);

		if (pfdp->pf_pdep2 == 0) {

			ASSERT(pfdp->pf_rmapp == 0);

			/* 
			 * This pde is the last one in the map,
			 * stop the migration for this page frame
			 */
			migr_stop(pfdattopfn(pfdp));
			RMAP_LOGENT(RMAP_DELMAP_LAST, pfdp, pdep, 
				    __return_address);
		}
		return;
	}


	if (IS_LONG_RMAP(pfdp) == 0){

		if (pfdp->pf_pdep2 == pdep) {
			pfdp->pf_pdep2 = 0;

			if (GET_RMAP_PDEP1(pfdp) == 0) {
				/* 
				 * This pde is the last one in the map,
				 * stop the migration for this page frame
				 */
				migr_stop(pfdattopfn(pfdp));
				RMAP_LOGENT(RMAP_DELMAP_LAST, pfdp, pdep, 
					    __return_address);
			}
		}
		else {
			cmn_err(CE_PANIC,
				"rmap_del: pde 0x%x not in pfdat 0x%x",
				pdep, pfdp);
		}

		return;
	}

	ASSERT(IS_LONG_RMAP(pfdp));

	RMAP_DOSTAT(rmapldel);

	pf_rmap = pfdp->pf_rmapp;

	first_level_index = FIRST_LEVEL_INDEX_OFFSET;
	first_level_entry = GET_FIRST_LEVEL_TABLE(pf_rmap);

	for (i = 0; i < FIRST_LEVEL_TABLE_SIZE(pf_rmap); i++, 
				first_level_index++, first_level_entry++) {

		if (IS_FREE_ENTRY(first_level_entry)) continue;

		if (!IS_SECOND_LEVEL(first_level_entry)) {
			if (first_level_entry->pdep == pdep) {
				first_level_entry->freelist_index = 
						pf_rmap->rmap_freelist_head;
				pf_rmap->rmap_freelist_head = i + 
						FIRST_LEVEL_INDEX_OFFSET;
				pf_rmap->num_ptes--;
				if (pf_rmap->num_ptes == 0) {
					/*
					 * Free the first level table.
					 */
					kmem_zone_free(rmap_zone
						[pf_rmap->rmap_zone_index], 
							pf_rmap);
					CLR_LONG_RMAP(pfdp);
					pfdp->pf_rmapp = RMAPNULL;

					if (GET_RMAP_PDEP1(pfdp) == 0) {
					/*
					 * This deleted pde is the last one
					 * in the map, stop the migration
					 * activiti for this page frame
					 */
						migr_stop(pfdattopfn(pfdp));
						RMAP_LOGENT(RMAP_DELMAP_LAST, 
								pfdp, pdep,
							    __return_address);
					}
				}
                                return;
			}
			continue;
		}

		second_level_table = GET_SECOND_LEVEL_TABLE(first_level_entry);

		second_level_entry = second_level_table->table_entries;

		for (j = 0; j < SECOND_LEVEL_TABLE_SIZE; j++,
					second_level_entry++) {
			if (IS_FREE_ENTRY(second_level_entry))
				continue;
			if (second_level_entry->pdep == pdep) {
                                second_level_entry->freelist_index = 
					second_level_table->freelist_head;
                                second_level_table->freelist_head = j;
				second_level_table->num_free_entries++;

				if (second_level_table->num_free_entries ==
						SECOND_LEVEL_TABLE_SIZE) {

                        		kmem_zone_free(second_level_table_zone,
                                			second_level_table);

                        		first_level_entry->freelist_index =
                                        	pf_rmap->rmap_freelist_head;
                        		pf_rmap->rmap_freelist_head = 
						first_level_index;
                		}

                                pf_rmap->num_ptes--;

				if (pf_rmap->num_ptes == 0) {
					/*
					 * Free the first level table.
					 */
					kmem_zone_free(rmap_zone
						[pf_rmap->rmap_zone_index], 
							pf_rmap);
					CLR_LONG_RMAP(pfdp);
					pfdp->pf_rmapp = RMAPNULL;
					if (GET_RMAP_PDEP1(pfdp) == 0) {
					/*
					 * This deleted pde is the last one
					 * in the map, stop the migration
					 * activiti for this page frame
					 */
						migr_stop(pfdattopfn(pfdp));
						RMAP_LOGENT(RMAP_DELMAP_LAST, 
								pfdp, pdep,
							    __return_address);
					}
				}
                                return;
                        }

		}
	}
	cmn_err(CE_PANIC, "Rmap delmap: pde not in rmap pfdp 0x%x pdep 0x%x \n",
			pfdp, pdep);
	return;
}

/*
 * rmap_swapmap:
 * 	Search for the place where opde exists, and replace it with npde.
 * 	At this time this's called only from seg_commonsplit, which is 
 *	trying to separate the shared and private areas for sproc processes.
 *	32 bit pte version.
 */
void      
rmap_swapmap(pfd_t *pfdp, pde_t *opde, pde_t *npde )
{

	rmap_t	*pf_rmap;
	int	s,i, j;
	table_ent_t	*first_level_entry, *second_level_entry;
	rmap_second_level_table_t	*second_level_table;

	RMAP_DOSTAT(rmapswap);

	RMAP_LOGENT(RMAP_SWAPMAP, pfdp, opde, __return_address);

	s = RMAP_LOCK(pfdp);
	
	if (GET_RMAP_PDEP1(pfdp) == opde) {
		SET_RMAP_PDEP1(pfdp, npde);
		RMAP_UNLOCK(pfdp, s);
		return;
	}

	if (!(IS_LONG_RMAP(pfdp))) {
		ASSERT(pfdp->pf_pdep2 == opde);	/* it better be true */
		pfdp->pf_pdep2 = npde;
		RMAP_UNLOCK(pfdp, s);
		return;
	}

	/* This pfdat has one or more users. */
	ASSERT(pfdp->pf_use >= 1);

	pf_rmap = pfdp->pf_rmapp;

	first_level_entry = GET_FIRST_LEVEL_TABLE(pf_rmap);

	for (i = 0; i < FIRST_LEVEL_TABLE_SIZE(pf_rmap); i++, 
						first_level_entry++) {

		if (IS_FREE_ENTRY(first_level_entry)) continue;

		if (!IS_SECOND_LEVEL(first_level_entry)) {
			if (first_level_entry->pdep == opde) {
				
				first_level_entry->pdep = npde;
				RMAP_UNLOCK(pfdp, s);
				return;
			}
			continue;
		}

		second_level_table = GET_SECOND_LEVEL_TABLE(first_level_entry);

		second_level_entry = second_level_table->table_entries;

		for (j = 0; j < SECOND_LEVEL_TABLE_SIZE; j++,
					second_level_entry++) {
			if (IS_FREE_ENTRY(second_level_entry))
				continue;
			if (second_level_entry->pdep == opde) {
                                second_level_entry->freelist_index = 
					second_level_table->freelist_head;
				second_level_entry->pdep = npde;
				RMAP_UNLOCK(pfdp, s);
				return;
			}
		}
	}

	cmn_err(CE_PANIC, "rmap_swap: opde 0x%x not in rmap\n", opde);
	/*NOTREACHED*/
}
#endif /* RMAP_PTE_INDEX */

void      
rmap_delmap(pfd_t *pfdp, pde_t *pdep)
{
	int s = RMAP_LOCK(pfdp);

	rmap_delmap_nolock(pfdp, pdep);
	RMAP_UNLOCK(pfdp, s);
}

/*
 * rmap_scanmap:
 * Scan through the pdes mapping to a particular pfdat, and 
 * do the required operation on those pdes.
 *
 * LOCKING:
 * This method does NOT do any locking. It is up to the
 * caller to do the appropriate locking. At least
 * the rmap lock for the corresponding page must be taken.
 */

int
rmap_scanmap(pfd_t *pfdp, uint_t op, void *datap)
{
	
	int	retval = 0, counter = 0;
	table_index_t	i, j;
	table_ent_t	*first_level_entry, *second_level_entry;
	rmap_t		*pf_rmap;

        ASSERT(pfdp);

	RMAP_LOGENT(RMAP_SCANMAP, pfdp, (pde_t *)((long)op), __return_address);
        
#ifdef	RMAP_SCANTEST
	/* If in scantest mode, Dont do anything unless it's anon page */
	if (!(pfdp->pf_flags & (P_ANON|P_SQUEUE|P_DQUEUE|P_DIRTY|P_DONE))){
		return;
	}
#endif	/* RMAP_SCANTEST */

	RMAP_DOSTAT(rmapscan);
	ASSERT(RMAP_ISLOCKED(pfdp));

	/*
	 * It's possible to have an outer for loop, and a switch statement
	 * within the for loop for the required operation. But that would
	 * be less efficient in executing the required operation, than 
	 * having a for loop in each case statement.
	 * Hence the replicated code in each case statement.
	 */
	switch(op) {
	case RMAP_LOCKPFN:
		retval = rmap_lockpfns(pfdp);
		break;

	case RMAP_VERIFYLOCKS:
		retval = rmap_verify_locks(pfdp, (int)(__uint64_t)datap);
		break;

	case RMAP_SHOOTPFN:
		retval = rmap_mark_shotdown(pfdp);
		break;

        case RMAP_COUNTLINKS:

		if (GET_RMAP_PDEP1(pfdp))
			retval++;

		if (!IS_LONG_RMAP(pfdp)) {
			if (pfdp->pf_pdep2)
				retval++;
			break;
		}

		retval += pfdp->pf_rmapp->num_ptes;

		break;

	case	RMAP_CLRVALID:
	case 	RMAP_SETVALID:
	case	RMAP_ZEROPDE:
	case	RMAP_SETPFN:
	case	RMAP_UNLOCKPFN:
	case	RMAP_SETPFN_AND_UNLOCK:
	case	RMAP_CHECK_LPAGE:
	case	RMAP_JOBRSS_TWO:
	case	RMAP_JOBRSS_ANY:
	case	RMAP_MIGR_CHECK:
#ifdef MH_R10000_SPECULATION_WAR
	case	RMAP_MH_SPECULATION_WAR:
#endif

		if (GET_RMAP_PDEP1(pfdp))
			retval = rmap_doop(op, GET_RMAP_PDEP1(pfdp), datap, 
						(void *)&counter);

		if (retval)
			return retval;	

		if (!(IS_LONG_RMAP(pfdp))) {
			if (pfdp->pf_pdep2)
				retval = rmap_doop(op, pfdp->pf_pdep2, datap, 
						(void *)&counter);
			return retval;
		}

		pf_rmap = pfdp->pf_rmapp;
		ASSERT(pf_rmap);

		first_level_entry = GET_FIRST_LEVEL_TABLE(pf_rmap);
		for (i = 0; i < FIRST_LEVEL_TABLE_SIZE(pf_rmap); i++, 
							first_level_entry++) {

			if (IS_FREE_ENTRY(first_level_entry)) continue;

			if (!IS_SECOND_LEVEL(first_level_entry)) {
				retval = rmap_doop(op, first_level_entry->pdep, 
						datap, (void *)&counter);
				if (retval)
					return retval;
						
				continue;
			}

			second_level_entry = 
				GET_SECOND_LEVEL_ENTRY(first_level_entry, 0);
			for (j = 0; j < SECOND_LEVEL_TABLE_SIZE; j++,
						second_level_entry++) {
				if (IS_FREE_ENTRY(second_level_entry))
					continue;
				retval = rmap_doop(op, second_level_entry->pdep, 
						datap, (void *)&counter);
				if (retval)
					return retval;
			}
		}
		break;	

		
	default:
		cmn_err(CE_PANIC,"Invalid option %d to rmap_scanmap",op);
		break;
		/*NOTREACHED*/

	}

        return (retval);
}



/*
 * Returns 0 on success, non zero on failure.
 */

int
rmap_doop(uint op, pde_t *pdep, void *datap, void *counter)
{
	int	retval = 0;
	/* REFERENCED */
	long	rmap_index;

	switch(op) {

	case RMAP_CLRVALID:
		pg_clrvalid(pdep);
		break;

	case RMAP_SETVALID:
		pg_setsftval(pdep);
		break;

	case RMAP_ZEROPDE:
		pg_clrpgi(pdep);
		break;

	case RMAP_SETPFN:
		pg_setpfn(pdep, *(pfn_t *)datap);
		break;

	case RMAP_UNLOCKPFN:
		pg_pfnrelease(pdep);
		break;

	case RMAP_SETPFN_AND_UNLOCK:
		rmap_index = pg_get_rmap_index(pdep);
		pg_setpfn(pdep, *(pfn_t *)datap);
		pg_pfnrelease(pdep);
		ASSERT(rmap_index == pg_get_rmap_index(pdep));
		break;

	case RMAP_CHECK_LPAGE:
		retval = (int)pg_get_page_mask_index(pdep);
		break;

#ifdef MH_R10000_SPECULATION_WAR
	case RMAP_MH_SPECULATION_WAR:
		rmap_invalidate_uptbl_entry(pdep, datap);
		break;
#endif
	case RMAP_JOBRSS_ANY:
		retval = pmap_pte_scan(pdep, miser_jobcount, datap, 0, 	
								JOB_SCAN);
		break;
	case RMAP_JOBRSS_TWO:
		retval = pmap_pte_scan(pdep, miser_jobcount, datap, counter,
								JOB_SCAN);
		break;
	case RMAP_MIGR_CHECK:
		retval = pg_isfetchop(pdep);
		break;
	default:
		cmn_err(CE_PANIC,"Invalid option %d to rmap_scanmap",op);
		break;
		/*NOTREACHED*/

	}

	return retval;
}

#ifdef MH_R10000_SPECULATION_WAR
static int
rmap_invalidate_uptbl_entry(pde_t *pde, void *counts)
{
	pfd_t *pfd;
	extern is_in_pfdat(pgno_t pfn);

	if (! pg_isvalid(pde) ||
		pg_isnoncache(pde))
		return 0;

	if (pg_ismod(pde) &&
		is_in_pfdat(pdetopfn(pde))) {
		    pfd = pdetopfdat(pde);
		    pfd->pf_flags |= P_DIRTY;
	}
	if (pg_ishrdvalid(pde)) {
		pg_clrhrdvalid(pde);
		((int *)counts)[1]++;
	}
	((int *)counts)[0]++;
	return 0;
}
#endif

/*
 * Lock all pfns.
 * Returns :
 *	-1 if unable to lock pdep
 *	-2 if unable to lock pdep2
 *	-3 if unable to lock a pfn in any of the reverse maps.
 *	number of locked pfns if success.
 *
 */
static int
rmap_lockpfns(pfd_t *pfdp)
{

	int	retval = 0;
	rmap_t	*pf_rmap;
	int	lock_failure = 0;
	table_index_t	i, j;
	table_ent_t	*first_level_entry, *second_level_entry;
	pde_t	*pdep1;

	RMAP_VERIFY_LOCK_CONSISTENCY(pfdp);

	pdep1 = GET_RMAP_PDEP1(pfdp);

	if (pdep1) {
		if (pg_pfncondacq(pdep1)) {
			RMAP_VERIFY_LOCK_CONSISTENCY(pfdp);
			return -1;
		}
		retval++;
	}
		
	if (!(IS_LONG_RMAP(pfdp))) {
		if (pfdp->pf_pdep2) {
			if (pg_pfncondacq(pfdp->pf_pdep2)) {
				 /*
				  * We need to release the locks we've
				  * already acquired.
				  */
				 if (pdep1)
					 pg_pfnrelease(pdep1);
				RMAP_VERIFY_LOCK_CONSISTENCY(pfdp);
				 return -2;
			 }
			 retval++;
		}
		return retval;
	}

		
	pf_rmap = pfdp->pf_rmapp;

	ASSERT(pf_rmap);

	first_level_entry = GET_FIRST_LEVEL_TABLE(pf_rmap);

	for (i = 0; i < FIRST_LEVEL_TABLE_SIZE(pf_rmap); i++, 
							first_level_entry++) {
		if (IS_FREE_ENTRY(first_level_entry)) continue;

		if (!IS_SECOND_LEVEL(first_level_entry)) {


			if (!(pg_pfncondacq(first_level_entry->pdep))) {
				/* Got the lock, keep going */
				retval++;
				continue;
			} 

			/* Screeeeeeeeeeeeech, could not get a lock. 
			 * need to unroll everything done so far.
			 */

			lock_failure++;
			break;
		}

		/*
		 * Need to look at second level table.
		 */
		second_level_entry = 
				GET_SECOND_LEVEL_ENTRY(first_level_entry, 0);

		for (j = 0; j < SECOND_LEVEL_TABLE_SIZE; j++,
					second_level_entry++) {

			if (IS_FREE_ENTRY(second_level_entry))
				continue;

			if (!(pg_pfncondacq(second_level_entry->pdep))){
				/* Got the lock, keep going */
				retval++;
				continue;
			}

			/* Screeeeeeeeeeeeech, could not get a lock. 
			 * need to unroll everything done so far.
			 */
		
			/*
			 * Unlock all the second level entries for this
			 * table here. We will break out of the loop
			 * and unlock the rest of the ptes.
			 */
			second_level_entry--;
			for (; j > 0; j--, second_level_entry--) {
				if (IS_FREE_ENTRY(second_level_entry))
					continue;
				pg_pfnrelease(second_level_entry->pdep);
			}

			lock_failure++;
			break;
		}

		if (lock_failure) 
			break;
	}

	/*
	 * If there are no lock failures return success.
	 */
	if (!lock_failure ) return retval;

	/*
	 * First we unlock the first direct
	 * pte.
	 */
	if(pdep1)
		pg_pfnrelease(pdep1);
	
	/*
	 * Unlock all the first level entries.
	 */

	while (i--) {
		first_level_entry--;
		if (IS_FREE_ENTRY(first_level_entry)) continue;

		if (!IS_SECOND_LEVEL(first_level_entry)) {
			pg_pfnrelease(first_level_entry->pdep);
			continue;
		}

		second_level_entry = 
				GET_SECOND_LEVEL_ENTRY(first_level_entry, 0);

		for (j = 0; j < SECOND_LEVEL_TABLE_SIZE; j++,
						second_level_entry++) {

			if (!IS_FREE_ENTRY(second_level_entry))
				pg_pfnrelease(second_level_entry->pdep);
		}

	}

	RMAP_VERIFY_LOCK_CONSISTENCY(pfdp);
	return -3;
}


/*ARGSUSED*/
static int
rmap_verify_locks(pfd_t *pfdp, int process_id)
{
        int 	error_counter = 0;
#if defined(PTE_64BIT) && defined(NUMA_BASE)
	rmap_t		*pf_rmap;
	table_index_t	i, j;
	table_ent_t	*first_level_entry, *second_level_entry;
	pde_t	*pdep1;


	pdep1 = GET_RMAP_PDEP1(pfdp);
	if (pdep1) {
		if ((pdep1->pte.pte_numa_home & 0xFFFF) == process_id) {
			error_counter++;
		}
	}

	if (!(IS_LONG_RMAP(pfdp))) {
		if ((pfdp->pf_pdep2)  && 
		((pfdp->pf_pdep2->pte.pte_numa_home & 0xFFFF) == process_id)) {
			error_counter++;
		}
		return error_counter;
	}

	pf_rmap = pfdp->pf_rmapp;
	ASSERT(pf_rmap);

	first_level_entry = GET_FIRST_LEVEL_TABLE(pf_rmap);

	for (i = 0; i < FIRST_LEVEL_TABLE_SIZE(pf_rmap); i++, 
						first_level_entry++) {
		if (IS_FREE_ENTRY(first_level_entry)) 
			continue;
		if (!IS_SECOND_LEVEL(first_level_entry)) {
			ASSERT(first_level_entry->pdep);
			if ((first_level_entry->pdep->pte.pte_numa_home 
					& 0xFFFF) == process_id) {
				error_counter++;
			}
			continue;
		}

		second_level_entry = 
				GET_SECOND_LEVEL_ENTRY(first_level_entry, 0);

		for (j = 0; j < SECOND_LEVEL_TABLE_SIZE; j++,
					second_level_entry++) {
			if (!IS_FREE_ENTRY(second_level_entry)) {
				ASSERT(second_level_entry->pdep);
				if ((second_level_entry->pdep->pte.pte_numa_home 
						& 0xFFFF) == process_id) {
					error_counter++;
				}
			}
		}
	}
#endif /* PTE_64BIT */        

	return (error_counter);
}


/*
 * rmap_mark_shotdown:
 *	Need to do all the things in a pde to
 *	indicate that this has been shotdown.
 *	Assumes that the pde has been locked
 * 	SIDE EFFECT
 *		This function has the side effect of unlocking the pfns
 *		and freeing up the reverse maps associated with the page
 *		This has been specifically to make the page shoot down
 *		to be completed with minimal lock acquire/free cycles
 */
static int
rmap_mark_shotdown(pfd_t *pfdp)
{
	rmap_t	*pf_rmap;
	int	rmap_cnt = 0;
	table_index_t	i, j;
	table_ent_t	*first_level_entry, *second_level_entry;
	pde_t		*pdep1 = GET_RMAP_PDEP1(pfdp);

	if (pdep1){
		/* Make it a single atomic OP ? */
		pg_clrvalid(pdep1);
		pg_setpfn(pdep1, 0);
		pg_setshotdn(pdep1);
		pg_pfnrelease(pdep1);
		rmap_cnt++;
		SET_RMAP_PDEP1(pfdp, 0);
	}

	if (!(IS_LONG_RMAP(pfdp))) { 
		if (pfdp->pf_pdep2){
			/* Make it a single atomic OP ? */
			pg_clrvalid(pfdp->pf_pdep2);
			pg_setpfn(pfdp->pf_pdep2, 0);
			pg_setshotdn(pfdp->pf_pdep2);
			pg_pfnrelease(pfdp->pf_pdep2);
			rmap_cnt++;
			pfdp->pf_pdep2 = 0;
		}
		migr_stop(pfdattopfn(pfdp));
		return rmap_cnt;	
	}

	pf_rmap = pfdp->pf_rmapp;
	ASSERT(pf_rmap);
	pfdp->pf_rmapp = 0;
	CLR_LONG_RMAP(pfdp);

	first_level_entry = GET_FIRST_LEVEL_TABLE(pf_rmap);

	for (i = 0; i < FIRST_LEVEL_TABLE_SIZE(pf_rmap); i++, 
						first_level_entry++) {
		if (IS_FREE_ENTRY(first_level_entry))
			continue;
		if (!IS_SECOND_LEVEL(first_level_entry)) {
			/* Make it a single atomic OP ? */
			pg_clrvalid(first_level_entry->pdep);
			pg_setpfn(first_level_entry->pdep, 0);
			pg_setshotdn(first_level_entry->pdep);
			pg_pfnrelease(first_level_entry->pdep);
			rmap_cnt++;
			continue;
		}

		second_level_entry = 
				GET_SECOND_LEVEL_ENTRY(first_level_entry, 0);

		for (j = 0; j < SECOND_LEVEL_TABLE_SIZE; j++,
					second_level_entry++) {
			if (!IS_FREE_ENTRY(second_level_entry)) {
				/* Make it a single atomic OP ? */
				pg_clrvalid(second_level_entry->pdep);
				pg_setpfn(second_level_entry->pdep, 0);
				pg_setshotdn(second_level_entry->pdep);
				pg_pfnrelease(second_level_entry->pdep);
				rmap_cnt++;
			}
		}

		/* 
		 * Free the second level table.
		 */
		kmem_zone_free(second_level_table_zone, 
				GET_SECOND_LEVEL_TABLE(first_level_entry));
	}

	/*
	 * Free the first level table.
	 */
	kmem_zone_free(rmap_zone[pf_rmap->rmap_zone_index], pf_rmap);

	if (rmap_cnt != 0) {
		/* Stop the migration activity for the page */
		migr_stop(pfdattopfn(pfdp));
		RMAP_LOGENT(RMAP_DELMAP_LAST, pfdp, (pde_t *)NULL,
			    __return_address);
	}

	return rmap_cnt;
}



/*
 * rmap_xfer:
 *  Transfer all reverse mapping state from old pfdat to new pfdat.
 *  This includes, moving the reverse mapping pointer if any to new
 *  pfdat, as well as any other flags/state that may be in the old pfdat.
 * 
 * Since rmap_xfer is called when the page migration is destined to succeed,
 * the migration interrupt is turned off for the old pfd and turned on 
 * for the new pfd to keep the notion of enabling migration interrupt 
 * consistent with rmap operations. 
 * 
 * We'd better enable and disable the migration interrupt for one page
 * while we're having the lock for rmap.  Thus, we can avoid some possible
 * races.  One scenario is:  after releasing the rmap lock and before 
 * enabling the migration interrupt for the new page, one mapping is gone
 * and one assertion fails. 
 *
 * TBD: Scheme for locking the two pfdats. 
 * Transferring other pfdat fields (e.g. tag, count, flags etc)  needs to 
 * be done elsewhere.
 */
void
rmap_xfer(pfd_t	*opfd, pfd_t *npfd)
{

	/*
	 * XXX
	 * It may be necessary to lock the two pfdats here. One possible 
	 * scheme to avoid dead-locks is to lock the pfdat with lower addr
	 * first, and then the higher addressed one.
	 * For now, we assume mem_lock is taken by the caller.
	 */

	/* Update new pde */
	npfd->pf_rmapp = opfd->pf_rmapp;
	SET_RMAP_PDEP1(npfd, GET_RMAP_PDEP1(opfd));
	if IS_LONG_RMAP(opfd) 
		SET_LONG_RMAP(npfd);

	/* Zero out old pde fields */
	opfd->pf_rmapp = 0;
	SET_RMAP_PDEP1(opfd, 0);
	CLR_LONG_RMAP(opfd);

}

/*
 * Returns true if a page is valid or it is migrating.
 * We serialize the testing using the pfdat_lock() as it is held before
 * clearing the P_ISMIGRATING bit in migr_page_epilogue().
 */
int
pg_ismigrating(pde_t *pd)
{
	pfd_t *pfd;
	int	s;
	pfn_t	pfn;

	/*
	 * YUCK ALERT!! : dont use
	 *	pdetopfdat(pd)
	 *
	 * here. The way the macros are currently constructed, this 
	 * may cause pdetopfn to be evaluated twice. If migration 
	 * is in the process of changing the pd.pfn, we may get a bogus
	 * pfd if the first evaluation sees the old pfn & the second
	 * evaluation see the new pfn. (Macros should be changed to fix
	 * this, but now is not the time to do it!!).
	 *
	 * Note: problem exists only if you compile at low optimizations.
	 *
	 */
	pfn = pdetopfn(pd);
	pfd = (pfn > PG_SENTRY) ? pfntopfdat(pfn) : (pfd_t *)0;

	if (!pfd) return pg_isvalid(pd);

	if (pfdat_ismigrating(pfd)) return TRUE;

	s = pfdat_lock(pfd);

	KTRACE_ENTER(vnode_ktrace, VNODE_PG_ISMIGRATING, pfd->pf_tag, pg_isvalid(pd), pfd->pf_flags);
	if (pg_isvalid(pd) || pfdat_ismigrating(pfd)) {
	    pfdat_unlock(pfd, s);
	    return TRUE;
	}
	pfdat_unlock(pfd, s);
	return FALSE;
}

#ifdef DEBUG_PFNLOCKS
/*
 * Debugging support for pde-pfnlocks
 */

/*
 * The following locking routines are intended to be used
 * ONLY for debugging migration when migration is done via
 * a syssgi syscall. Obviously these routines do NOT work
 * within intr handlers.
 */
void
pg_pfnacquire(pde_t* pde)
{
        ASSERT(pde);

        /*****
        printf("ACQUIRING: PFNLOCK FROM 0x%llx, pte_pfnlock: %d, pfn: 0x%x\n",
               pde, pde->pte.pte_pfnlock, pde->pte.pte_pfn);
         *****/
        
        PG_LOCK((pde)->pgi, PG_PFNLOCK);
}

void
pg_pfnrelease(pde_t* pde)
{
        ASSERT(pde);
        PG_UNLOCK((pde)->pgi, PG_PFNLOCK);
}

int
pg_pfncondacq(pde_t* pde)
{
        if (PG_TRYLOCK((pde)->pgi, PG_PFNLOCK) == 0) {
                /* success */
                return (0);
        } else {
                return (1);
        }
}



void
rmap_verify_lock_consistency(pfd_t *pfdp)
{
        rmap_t* irmap;
        int i;
        
#ifdef R10000
        return;
#else /* !R10000 */
	ASSERT(curvprocp);
	rmap_verify_locks(pfdp, current_pid());
#endif /* !R10000 */       
}

#endif /* DEBUG_PFNLOCKS */



#ifdef	RMAP_DEBUG

/*
 * Print the rmaps for a given pfdat
 */

#define DUMP_PFDAT 0x1

void
idbg_rmapprint(pfn_t pfn)
{
        pfd_t *pfdp = pfntopfdat(pfn);
	rmap_t	*pf_rmap;
	table_index_t	i, j;
	table_ent_t	*first_level_entry, *second_level_entry;
	pde_t		*pdep1;
        
	pdep1 = GET_RMAP_PDEP1(pfdp);
        if (pdep1) {
                qprintf("[RMAP1] 0x%x ", pdep1);
                idbg_dopde(pdep1, 0, DUMP_PFDAT);
                if (pfn != pdep1->pte.pg_pfn) {
                        qprintf("*** Previous RMAP is not consistent\n");
                }
        }
                
        if (!(IS_LONG_RMAP(pfdp))) {
		if (pfdp->pf_pdep2) {
			qprintf("[RMAP2] 0x%x ", pfdp->pf_pdep2);
			idbg_dopde(pfdp->pf_pdep2, 0, DUMP_PFDAT);
			if (pfn != pfdp->pf_pdep2->pte.pg_pfn) {
				qprintf("*** Previous RMAP is not consistent\n");
			}
		}
		return;
        }

	pf_rmap = pfdp->pf_rmapp;
	ASSERT(pf_rmap);

	qprintf("Printing Rmap info at 0x%x \n", pf_rmap);
	qprintf("First level table size %d num ptes %d\n",
				FIRST_LEVEL_TABLE_SIZE(pf_rmap),
				pf_rmap->num_ptes);
	qprintf("Freelist head %d free table hint %d\n", 
				pf_rmap->rmap_freelist_head,
				pf_rmap->rmap_freetable_hint);

	first_level_entry = GET_FIRST_LEVEL_TABLE(pf_rmap);

	for (i = 0; i < FIRST_LEVEL_TABLE_SIZE(pf_rmap); i++, 
						first_level_entry++) {

		if (IS_FREE_ENTRY(first_level_entry)) {
			qprintf("[RMAPI] free entry 0x%x\n",
				first_level_entry->freelist_index);
			continue;
		}

		if (!IS_SECOND_LEVEL(first_level_entry)) {
			qprintf("[RMAPI] 0x%x ", first_level_entry->pdep);
			idbg_dopde(first_level_entry->pdep, 0, DUMP_PFDAT);
			if (pfn != first_level_entry->pdep->pte.pg_pfn) {
				qprintf("*** Previous RMAP is not consistent\n");
			}
			continue;
		}

		second_level_entry = 
				GET_SECOND_LEVEL_ENTRY(first_level_entry, 0);
		for (j = 0; j < SECOND_LEVEL_TABLE_SIZE; j++,
					second_level_entry++) {
			if (!IS_FREE_ENTRY(second_level_entry)) {
                                qprintf("[RMAPII] 0x%x ", 
						second_level_entry->pdep);
                                idbg_dopde(second_level_entry->pdep,0,DUMP_PFDAT);
                                if (pfn != 
					second_level_entry->pdep->pte.pg_pfn) {
                                        qprintf("*** Previous RMAP is not consistent\n");
                                }
			} else
				qprintf("[RMAPII] second level free entry 0x%x\n",
					second_level_entry->freelist_index);
		}
	}

}

char *rmap_type[] = {
	"",
	"Add ",
	"Del ",
	"Swap",
	"Scan",
	"Add_First ",
	"Del_Last ",
	""
};

/*
 * Print the Rmap log entries which correspond to the given entry 
 */
int
idbg_rmaplog(void *rmap_data)
{
	int	i;
	qprintf("Printing rmaplog for data 0x%x\n", rmap_data);
	i = rmap_dbgindx;
	do {
		if (rmap_dbglist[i].rmap_pdep == (pde_t *)rmap_data ||
		    rmap_dbglist[i].rmap_pfdp == (pfd_t *)rmap_data ) 

			qprintf("%s@%x rmap %d pid %d pd %x pf %x ra %x\n",
				rmap_type[rmap_dbglist[i].rmap_op & 0xF],
				rmap_dbglist[i].rmap_tm,
				rmap_dbglist[i].rmap_fl, 
				rmap_dbglist[i].rmap_pid,
				rmap_dbglist[i].rmap_pdep, 
				rmap_dbglist[i].rmap_pfdp, 
				rmap_dbglist[i].rmap_ra);

		if (++i == RMAP_DBGENTS)
			i = 0;
	} while (i != rmap_dbgindx);
	return 0;
}

extern int	numnodes;

#include <sys/nodepda.h>

int	rmap_toscan = 128;
void
rmap_scantest()
{
	static cnodeid_t node = 0;
	static 	pfd_t	*pfd1 = 0;
	int		s;

	int		pfdstoscan;
	pfd_t		*pfd2;

	if (pfd1 == 0)
		pfd1 = PFD_LOW(node);

	pfdstoscan = rmap_toscan;

	while (pfdstoscan) {

		pfd2 = PFD_HIGH(node);

		for (; pfdstoscan && (pfd1 <= pfd2); pfd1++, pfdstoscan--) {
		
			if ((pfd1->pf_use == 0) || (pfd1->pf_flags & P_QUEUE))
				/* Page not in use, or on free list */
				continue;
			
			/* Dont do anything unless it's anon page */
			if (!(pfd1->pf_flags & P_ANON))
				continue;

			if (!pfd1->pf_pdep2  && !pfd1->pf_pdep1)
				/* No reverse map attached */
				continue;

                        s = RMAP_LOCK(pfd1);
			if (rmap_scanmap(pfd1, RMAP_CLRVALID, 0) != 0) {
                                ASSERT(0);
                        }
                        RMAP_UNLOCK(pfd1, s);

		}

		if (!pfdstoscan)
			break;

		if (++node == numnodes){
			node = 0;
		}
		pfd1 = PFD_LOW(node);

	}

}

static void
rmap_logcallers(int type, unsigned long	caller_addr)
{
	int	i, freeent = RMAP_MAXCALLERS;

	for (i=0; i < RMAP_MAXCALLERS; i++){
		if (!rmap_callers[type][i])
			freeent = i;
		if (rmap_callers[type][i] == caller_addr)
			return;
	}

	if (freeent == RMAP_MAXCALLERS){
		cmn_err_tag(290,CE_WARN,"No free slot for address 0x%x type: %d\n",
			    caller_addr, type);
		return;
	}
	rmap_callers[type][freeent] = caller_addr;
}

#endif	/* RMAP_DEBUG */


#ifdef	RMAP_STATS

#define	MAX_PFDATS	8192

struct	rmap_pfdat_stat {
	int	maxlen;
	int	len;
} rmap_pfdat_stat_buf[MAX_PFDATS]; 



struct vnode;
int		vnode_max_len;
struct vnode	*max_vnode;

void
pfdat_add(pfd_t *pfd)
{
        struct  rmap_pfdat_stat *p;

        if (( pfd - PFD_LOW(0)) > MAX_PFDATS) {
                qprintf("Running out entries pfdat %x pfd_low %x\n",
                                pfd, PFD_LOW(0));
                return;
        }

        p = &rmap_pfdat_stat_buf[pfd - PFD_LOW(0)];
	p->len++;
	if (p->len > p->maxlen) p->maxlen = p->len;
	if (p->maxlen  > vnode_max_len) {
		vnode_max_len = p->maxlen;
		max_vnode = pfd->pf_vp;
	}
}

void
pfdat_del(pfd_t *pfd)
{
        struct  rmap_pfdat_stat *p;

        if (( pfd - PFD_LOW(0)) > MAX_PFDATS) {
                qprintf("Running out entries pfdat %x pfd_low %x\n",
                                pfd, PFD_LOW(0));
                return;
        }
        p = &rmap_pfdat_stat_buf[pfd - PFD_LOW(0)];
	p->len--;
}


int
idbg_rmap_stats()
{
#ifdef	NOTDEF
	pfd_t 	*pfd;
	int	i;
#endif

	qprintf("Rmap stats adds 0x%x del 0x%x ladds 0x%x ldels 0x%x\n",
		rmapinfo.rmapadd, rmapinfo.rmapdel, rmapinfo.rmapladd,
		rmapinfo.rmapldel);
	
#ifdef	NOTDEF
	qprintf("System Max rmap len 0x%x vnode 0x%x\n",
				vnode_max_len, max_vnode);
	for (pfd = PFD_LOW(0), i = 0; pfd < PFD_HIGH(0); pfd++, i++)
		if (rmap_pfdat_stat_buf[i].maxlen)
			qprintf("Pfd 0x%x maxlen 0x%x\n",
				pfd, rmap_pfdat_stat_buf[i].maxlen);
#endif
	return 0;
}
#endif /* RMAP_STATS */


