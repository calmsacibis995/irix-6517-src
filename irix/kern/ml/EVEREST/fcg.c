/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.34 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/vmereg.h>
#include <sys/immu.h>
#include <sys/sbd.h>
#include <sys/pda.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/iotlb.h>
#include <sys/debug.h>
#include <sys/pio.h>
#include <sys/proc.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/fchip.h>
#include <sys/kmem.h>
#include <ksys/as.h>

#ifdef DEBUG
#include <sys/idbgentry.h>

void fcg_dump(long id);
#endif

#define IO_NPGPT      (IO_NBPP/sizeof(pte_t))
#define IO_SOFFMASK   ((IO_NPGPT * IO_NBPP) - 1)   /* segment offset mask */
#define io_soff(x)    ((__psunsigned_t)(x) & IO_SOFFMASK)


/* # of io pages per processor page */
#define IO_PAGES_PER_PPG  (NBPP / IO_NBPP)  

/* # of IA Level 1 pages per processor page */
#define L1_PAGES_PER_PPG  (NBPP / IAMAP_L2_BLOCKSIZE1)

/* amount of io-virtual space mapped by 1 (system-sized) page of "pte's" */
#define FCG_SEG_SIZE ((NBPP / sizeof(int)) * IO_NBPP)
/* # of io-virtual page entries stored in one processor page */
#define NUM_IO_PAGES_PER_FCG_SEG (NBPP / sizeof(int))

/* Bounds of the system virtual IO address space.
 *
 * These values come from gfx/kern/graphics/VENICE/venice.c 
 * where they are defined as IO_KCOMM_AREA (beginning of system IO
 * virtual space things) and IO_DMA_AREA (area of IO virtual space
 * reserved for DMA's).  I can't find a decent place to stick those
 * #define's from venice.h in some header file that makes sense to
 * include here.  Sigh.
 */
#define SYS_IO_START         0x40000000      /* start at 1gig */
#define SYS_IO_END           0x80000000      /* end   at 2gig */


#define FCG_MAX_ID 3	/* max of 4 FCGs per system */

static struct fcg_t {
	unchar  ioslot;	  /* physical slot for IO board */
	unchar  padap;	  /* physical adaptor within IO board */
	unchar  window;	  /* assigned window for IO board */
	unchar  iobase;	  /* high 2 bits of 34 bit space */
	evreg_t *mapram;  /* pointer to private mapram for DMA */
	uint    **shadow; /* shadow of mapram with additional info */
	unchar  konaflag; /* true if on a kona */
} fcg[FCG_MAX_ID+1];

static int fcg_cnt;

/*
 * called at EARLY startup time.
 *	o no printfs allowed
 *	o no memory allocation allowed
 *
 * just record the minimum information so that edtinit
 * can ask us for the facts later.
 */
void
fcg_init(unchar ioslot, unchar padap, unchar window,
	 uint fci_id, unchar konaflag)
{
	fcg[fcg_cnt].ioslot = ioslot;
	fcg[fcg_cnt].padap = padap;
	fcg[fcg_cnt].window = window;
	fcg[fcg_cnt].mapram = (evreg_t *)(io4[window].mapram + IOA_MAPRAM_SIZE * fci_id);
	fcg[fcg_cnt].iobase = fci_id;
	fcg[fcg_cnt].konaflag = konaflag;
	++fcg_cnt;
}

/*
 * called at edtinit time.
 *
 * given a slot number and the physical adaptor,
 * return the FCG ID that matches.  (-1 if none does)
 *
 * this ID is important for future DMA mapping routines.
 *
 * we assume that if we are never asked, we don't have to
 * allocate any DMA mapping resources.
 */
int
fcg_get_ioslot(unchar id)
{
	if (id > FCG_MAX_ID)
		return -1;
	if (fcg[id].ioslot && fcg[id].padap && fcg[id].window) {
		if (!fcg[id].shadow) {
			uint size;

			/* allocate space to shadow first table mapping info */
			size = (((uint)0xffffffff / FCG_SEG_SIZE) + 1) * sizeof(uint *);
			fcg[id].shadow = (uint **)kern_malloc(size);
			bzero(fcg[id].shadow,size);
		}
		return fcg[id].ioslot;
	}
	return -1;
}

int
fcg_get_padap(unchar id)
{
	if (id > FCG_MAX_ID)
		return -1;
	if (fcg[id].ioslot && fcg[id].padap && fcg[id].window)
		return fcg[id].padap;
	return -1;
}

int
fcg_get_iobase(unchar id)
{
	if (id > FCG_MAX_ID)
		return -1;
	if (fcg[id].ioslot && fcg[id].padap && fcg[id].window)
		return fcg[id].iobase;
	return -1;
}

/*
 * given an FCG ID, this returns the allocated window number.
 */
int
fcg_get_window(unchar id)
{
	return fcg[id].window;
}




/*
 * specify an FCG ID and a 32 bit IO address/size that you
 * would like to reserve for future DMAs to/from this FCG.
 * We also pass the system virtual address that this io
 * virtual address will be similar to (i.e. both areas
 * will map to the same physical pages, but one happens
 * to be in IO virtual space while the other is in system
 * virtual space.
 */

uint
fcg_dma_mapalloc(unchar id, uint ioaddr, ulong iolen)
{
	uint i, pgcnt, **shadow, **shad;
	uint l1_index;	
 
#ifdef DEBUG
	static inited=0;
	if (inited == 0) {
   		idbg_addfunc("fcg", fcg_dump);
		inited = 1;
	}
#endif	


	/*
	 * FCG DMA OVERFLOW TRANSLATION WORKAROUND:
	 *	pad an extra mapping beyond the end of the user's buffer
	 *	if it ends on an exact page boundary, since ODMA overflows
	 *	still send out a transaction that must must have a valid
	 *	tranlation even there are no bytes specified in the enable mask.
	 *
	 *      We only do this for ioaddrs that are not part of the system
	 *      IO area (whose bounds are SYS_IO_START and SYS_IO_END, defined
	 *      up at the beginning of this file).
	 */
	if (!fcg[id].konaflag && (ioaddr < SYS_IO_START || ioaddr >= SYS_IO_END) && !poff(ioaddr+iolen))
		++iolen;	/* make sure we get the extra map space we need */


	pgcnt = numpages(ioaddr,iolen);
	ASSERT(iolen && (pgcnt < ctob(1)));
	shadow = fcg[id].shadow;

	/* printf("MAPALLOC: ioaddr==0x%lx  iolen==%ld\n", (ulong)ioaddr, iolen); */
	
	for (i = 0; i < pgcnt; ++i) {

		/* see if we already have a first level table-pair for this region */
	        l1_index = (ioaddr+ctob(i)) / FCG_SEG_SIZE;
		shad = &shadow[l1_index];
		if (!*shad) {
			volatile evreg_t *level1;
			int j;

			/* we don't... create one and update the first level pair */
			*shad = (uint *)kvpalloc(1,0,0);
#ifdef DEBUG
			{
				int j;
				for (j = 0; j < NUM_IO_PAGES_PER_FCG_SEG; ++j)
					(*shad)[j] = (unsigned)(0x800000ff | (ioaddr + j*IO_NBPP));
				
				/* (*shad)[j] = (unsigned)-1; */
			}
#endif
			level1 = &fcg[id].mapram[(l1_index * L1_PAGES_PER_PPG)]; 


			for(j=0; j < L1_PAGES_PER_PPG; j++,level1++)
			    *level1 = IAMAP_L2_ENTRY(kvtophys(*shad)
						     +j*IAMAP_L2_BLOCKSIZE1);
		}

		/* increment usage count in low bits */
		if (!fcg[id].konaflag && ioaddr >= SYS_IO_START && ioaddr < SYS_IO_END)
		  *shad = (uint *)((__psint_t)*shad | 1);  /* always keep the count at 1 */
		else
		  *shad = (uint *)((caddr_t)*shad + 1);
		ASSERT(poff(*shad));
	}

	/* return handle containing page aligned io addr and page count in low bits */
	return ctob(btoct(ioaddr)) | pgcnt;
}

/*
 * deallocate DMA maps for a previously allocated iospace.
 */
void
fcg_dma_mapfree(unchar id, uint handle)
{
	uint ioaddr = ctob(btoct(handle));
	uint pgcnt  = poff(handle);
	uint i, **shadow, **shad;
	uint l1_index;

	ASSERT(pgcnt);
	shadow = fcg[id].shadow;
	for (i = 0; i < pgcnt; ++i) {
	        l1_index = (ioaddr+ctob(i)) / FCG_SEG_SIZE;
		shad = &shadow[l1_index];

		/* decrement usage count in low bits (only for non-system IO areas) */
		ASSERT(poff(*shad));
		if (fcg[id].konaflag || (ioaddr < SYS_IO_START || ioaddr >= SYS_IO_END))
		  *shad = (uint *)((caddr_t)*shad - 1);

		/* release second level table if unused */
		if (!poff(*shad)) {
			kvpfree(*shad,1);
			*shad = 0;
		}
	}
}

/*
 * fill in the IO mapping hardware to map a given virtual
 * address space into a previously allocated iospace.
 *
 * here we examine the vaddr and do the right thing if
 * the vaddr is in userland or kernel land.
 */
void
fcg_dma_map(unchar id, uint handle, caddr_t vaddr, ulong len)
{
	uint ioaddr = ctob(btoct(handle));
	uint pgcnt = poff(handle);
	uint i,n, **shadow;

	/* sanity check that alloc'd map is large enough */
	ASSERT(pgcnt && (pgcnt >= numpages(ioaddr,len)));

	/* printf("fcg_dma_map: handle == 0x%x  vaddr == 0x%x  len == %d\n",
	       (ulong)handle, vaddr, len); */

	
	/* round vaddr back to page boundary and minimize pagecount */
	pgcnt = numpages(vaddr,len);

	/* printf("fcg_dma_map: ioaddr == 0x%x  pgcnt == 0x%x, vaddr == 0x%x\n",
	       (ulong) ioaddr, (ulong)pgcnt, (ulong)vaddr); */

	/*
	 * FCG DMA OVERFLOW TRANSLATION WORKAROUND:
	 *	pad an extra mapping beyond the end of the user's buffer
	 *	if it ends on an exact page boundary, since ODMA overflows
	 *	still send out a transaction that must must have a valid
	 *	tranlation even there are no bytes specified in the enable mask.
	 *	ie, a memory transaction will take place, but it will have
	 *	no effect on memory contents.  harmless... yeah, right...
	 */
	/* if (vaddr < (caddr_t)K0BASE && !poff(vaddr+len)) */
	if ((!fcg[id].konaflag) && (ioaddr < SYS_IO_START || ioaddr >= SYS_IO_END) && !poff(vaddr+len))
		fcg_dma_map(id,handle+ctob(pgcnt),(caddr_t)ctob(btoct(vaddr)),1);

	vaddr = (caddr_t)ctob(btoct(vaddr));

	shadow = fcg[id].shadow;
	for (i = 0; i < pgcnt; i += n) {
		uint **shad;
		uint index, nn, l1_index;
		pfn_t *level2;

		/* compute address of second level entries to patch */
	        l1_index = ioaddr / FCG_SEG_SIZE;
		shad = &shadow[l1_index];
		ASSERT(poff(*shad));

		level2 = (pfn_t *)ctob(btoct(*shad));
		index = (ioaddr % FCG_SEG_SIZE) / IO_NBPP;
		level2 += index;

		/* figure out how many we can do in one shot */
		n = MIN(pgcnt-i,(NUM_IO_PAGES_PER_FCG_SEG-index)/IO_PAGES_PER_PPG);
		/* printf("fcg_dma_map:(1) ioaddr=0x%x l1_index=%d vaddr=0x%x\n",
		       ioaddr,l1_index,vaddr); */
		/* printf("fcg_dma_map:(1) level2 == 0x%x n=%d pgcnt=%d i=%d index=%d\n",
		       level2,n,pgcnt,i,index); */

		for (nn=0; nn < n; ++nn) {
		        int j;
			pfn_t pfn;
				
			if (vaddr < (caddr_t)K0BASE)
		  		vtopv(vaddr, 1, level2, sizeof(*level2),
			              (PNUMSHFT - IO_PNUMSHFT), 0);
			else
			        *level2 = kvtophyspnum(vaddr) << (PNUMSHFT - IO_PNUMSHFT);

			/* printf("fcg_dma_map:(2) level2 == 0x%x nn=%d,n=%d\n", level2,nn,n); */

			pfn = *level2++;
			/* printf("vaddr 0x%x and ioaddr 0x%x map to pfn 0x%x\n", vaddr, ioaddr, pfn); */

			for (j=1; j < IO_PAGES_PER_PPG; j++)
				*level2++ = pfn + j;
				
			vaddr += ctob(1);				
		}
		ioaddr += ctob(n);
	}
	fchip_tlbflushall(SWIN_BASE(fcg[id].window,fcg[id].padap));
}

/*
 * fill in the IO mapping hardware to map a given list of
 * physical page frame numbers into a previously allocated iospace.
 */
void
fcg_dma_maplist(unchar id, uint handle, uint *pfn, uint cnt)
{
	uint ioaddr = ctob(btoct(handle));
	uint i, n, **shadow, **shad, *level2;
#ifdef DEBUG
	uint pgcnt = poff(handle);
#endif
	/* printf("id = %d handle == 0x%x pfn == 0x%x, cnt == %d\n",
	       id, handle, pfn, cnt); */

	/* sanity check that alloc'd map is large enough */
	ASSERT(cnt);
	ASSERT(pgcnt);
	ASSERT(pgcnt >= cnt);

	shadow = fcg[id].shadow;
	for (i = 0; i < cnt; i += n) {
		uint index, nn, l1_index;

		/* compute address of second level entries to patch */
	        l1_index = ioaddr / FCG_SEG_SIZE;
		shad = &shadow[l1_index];
		ASSERT(poff(*shad));
		level2 = (uint *)ctob(btoct(*shad));
		index = (ioaddr % FCG_SEG_SIZE) / IO_NBPP;
		level2 += index;


		/* figure out how many we can do in one shot */
		n = MIN(cnt-i,NUM_IO_PAGES_PER_FCG_SEG-index);
		for (nn = 0; nn < n; nn++, pfn++) {
		   	int j;
		   
		   	*level2++ = (*pfn) << (PNUMSHFT - IO_PNUMSHFT);
		   
		   	for (j=1; j < IO_PAGES_PER_PPG; j++)
		     		*level2++ = ((*pfn) << (PNUMSHFT - IO_PNUMSHFT)) + j;

		 }
		ioaddr += ctob(n);
	}
	fchip_tlbflushall(SWIN_BASE(fcg[id].window,fcg[id].padap));
}



#ifdef DEBUG

/*
 * Utility routines to dump the IO virtual mapping tables.
 *
 * The main entry point is fcg_dump(), which will dump all
 * translations for a given FCG chip.
 */
void
print_translations(uint **shadow, int which)
{
  int i, cnt;
  uint *shad;

  shad = shadow[which];
  cnt = poff(shad);
  shad = (uint *)((ulong)shad & ~0xfff);

  printf("1st level entry #%d maps the virtual IO segment starting at: 0x%x\n",
	 which, which*FCG_SEG_SIZE);
  printf("2nd level pte's are at: 0x%x, usage count == %d\n", shad, cnt);

  printf("\tIO VAddr  -->  Ebus PFN\n");
  for(i=0; i < NUM_IO_PAGES_PER_FCG_SEG; i++)
   {
     int ioaddr;

     /* if (shad[i] != (~0)) */
     ioaddr = which*FCG_SEG_SIZE + i*IO_NBPP;
     if (shad[i] != (ioaddr | 0x800000ff))
      {
	qprintf("\t0x%x       0x%x (un-shifted == 0x%x)\n",
	       which*FCG_SEG_SIZE + i*IO_NBPP, shad[i] >> 2, shad[i]);
      }
   }
}


void
fcg_dump(long id)
{
  uint **shadow;
  int i, num_entries;

  if (id < 0 || id > FCG_MAX_ID)
   {
     printf("Bad FCG ID (%d)\n", id);
     return;
   }

  shadow = fcg[id].shadow;
  num_entries = ((uint)0xffffffff / FCG_SEG_SIZE) + 1;
  qprintf("There are %d 1'st level FCG mappings at 0x%x.\n",
	 num_entries, shadow);
  for(i=0; i < num_entries; i++)
   {
     if (shadow[i] != NULL)
       print_translations(shadow, i);
   }
  
}
#endif /* DEBUG */
