#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libklib/RCS/klib_page.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include "klib.h"
#include "klib_extern.h"

/* pmap_types defines lifted from sys/pmap.h
 */
#define PMAP_INACTIVE   0
#define PMAP_SEGMENT    1
#define PMAP_TRILEVEL   2
#define PMAP_FORWARD    3
#define PMAP_REVERSE    4
#define PMAP_LGPAGES    5

/*
 * kl_get_pgi() -- Extract the pgi from a pde structure
 *
 *   Takes into account that the size of the pde structure (it
 *   could be four bytes or eight bytes).
 */
k_uint_t
kl_get_pgi(klib_t *klp, k_ptr_t pde)
{
	if (PDE_SIZE(klp) == 4) {
		return((k_uint_t)(*(uint*)pde));
	}
	else {
		return(*(k_uint_t*)pde);
	}
}

/*
 * kl_pte_to_pfn()
 */
int
kl_pte_to_pfn(klib_t *klp, k_ptr_t pte)
{
	k_uint_t pfn, sv;

	kl_reset_error();

	if (K_MEMBER_BASEVAL(klp)(pte, "pte", "pte_pfn", &pfn) == -1) {
		 KL_SET_ERROR(KLE_BAD_PDE);
		 return(0);
	}
	if (pfn == 0) {
		/* It's remotely possible that pfn 0 is correct (based on 
		 * the contents of the pte struct). So, make sure it is...
		 */
		if ((K_MEMBER_BASEVAL(klp)(pte, "pte", "pte_sv", &sv) == -1) ||
			(sv == 0)) {
			KL_SET_ERROR(KLE_BAD_PDE);
		}
	}
	return((int)pfn);
}

/*
 * kl_locate_pfdat() -- Locate a pfdat structure in the page hash table
 */
k_ptr_t
kl_locate_pfdat(klib_t *klp, k_uint_t pgno, 
	kaddr_t tag, k_ptr_t pfbuf, kaddr_t *pfaddr)
{
	kaddr_t pfdatpp, pfdatp;
	k_ptr_t pfp;

	if (DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP,
			"kl_locate_pfdat: pgno=%lld, tag=0x%llx, pfbuf=0x%x, pfaddr=0x%x\n",
			pgno, tag, pfbuf, pfaddr);
	}

#ifdef NOT

There is no longer a global pfdat table. We have to come up with a
different way of doing this with the distributed pfdat tables in 
the various nodes.

	/* Get the pointer to the pfdat hash bucket for pgno/tag
	 */
	pfdatpp = _phash + (((pgno + (tag >> 5)) & phashmask) * K_NBPW(klp));
	
	/* Get the pointer to the first pfdat struct in the hash chain
	 */
	if ((!kl_get_kaddr(klp, pfdatpp, &pfdatp, "pfdat")) || (!pfdatp)) {
		return ((k_ptr_t)NULL);
	}

	if (DEBUG(KLDC_PAGE, 1)) {
		fprintf(KL_ERRORFP, "kl_locate_pfdat: pfdatpp=0x%llx, "
			"pfdatp=0x%llx\n", pfdatpp, pfdatp);
	}

	/* Loop through the pfdat hash chain looking for a pfdat struct that
	 * matches pgno and tag.
	 */
	while (pfdatp) {
		if (!(pfp = get_struct(pfdatp, pfdat_size, pfbuf, "pfdat"))) {
			return ((k_ptr_t)NULL);
		}
		if ((kl_uint(klp, pfp, "pfdat", "pf_pageno", 0) == pgno) &&
						(kl_kaddr(klp, pfp, "pfdat", "p_un") == tag)) {
			*pfaddr = pfdatp;
			return (pfp);
		} 
		else {
			pfdatp = kl_kaddr(klp, pfp, "pfdat", "pf_hchain");
		}
	}
#endif
	return ((k_ptr_t)NULL);
}

/*
 * kl_pmap_to_pde() -- Return a pointer to the page table entry 
 */
k_ptr_t *
kl_pmap_to_pde(klib_t *klp, k_ptr_t pp, kaddr_t vaddr, 
	k_ptr_t pdebuf, kaddr_t *pde)
{
	unsigned char type;
	k_ptr_t pte;
	kaddr_t segptr, sp;

	/* These defines are for the PMAP_TRILEVEL stuff. */
	kaddr_t trilevelptr;
	int segno = BTOST(vaddr); /* index of segment in segment table. */
	int segindex;
	k_ptr_t Psegptr;
	int len;

	if (DEBUG(KLDC_FUNCTRACE, 3)) {
		fprintf(KL_ERRORFP, "kl_pmap_to_pde: pp=0x%x, vaddr=0x%llx "
			"pdebuf=0x%x, pde=0x%x\n", pp, vaddr, pdebuf, pde);
	} 

	if (DEBUG(KLDC_PAGE, 1)) {
		fprintf(KL_ERRORFP, "kl_pmap_to_pde:BTOST(0x%llx)=%d\n", 
			vaddr, BTOST(vaddr));
	}

	type = KL_UINT(klp, pp, "pmap", "pmap_type");
	if (type == PMAP_SEGMENT) {
		segptr = kl_kaddr(klp, pp, "pmap", "pmap_ptr") + 
			(BTOST(vaddr) * K_NBPW(klp));

		if (DEBUG(KLDC_PAGE, 1)) {
			fprintf(KL_ERRORFP, "kl_pmap_to_pde:segptr=0x%llx\n",
				segptr);
		}

		if (!kl_get_kaddr(klp, segptr, &sp, "segptr")) {
			if (DEBUG(KLDC_PAGE, 1)) {
				fprintf(KL_ERRORFP, "kl_pmap_to_pde: couldn't "
					"read segment pointer at 0x%llx\n", 
					segptr);
			}
			return((k_ptr_t)NULL);
		}
		 
		*pde = sp + (((vaddr%K_NBPS(klp))/K_NBPC(klp)) * PDE_SIZE(klp));

		kl_get_pde(klp, *pde, pdebuf);
		if (KL_ERROR) {
			if (DEBUG(KLDC_PAGE, 1)) {
				fprintf(KL_ERRORFP, "kl_pmap_to_pde: could not"
					" read pde at 0x%llx\n", *pde);
			}
			return((k_ptr_t)NULL);
		}
		return(pdebuf);
	} 
	else if (type == PMAP_TRILEVEL)
	{
		/* 
		 *  This code is the same as in function pmap_pte. following 
		 *  the trilevel_pte route...  If that changes or some of the 
		 *  macros it uses changes then we will have to change this 
		 *  code too. 
		 *  File name is os/as/pmap.c
		 */
		
		/*
		 * Top most level also called base table. We probably should 
		 * verify if BTOBASETABINDEX(STOB(segno)) is less than 
		 * SEGTABSIZE as a sanity test?. Or just test if vaddr is 
		 * less than HIUSRATTACH_64.
		 * 
		 * The * 2 is for sizeof pcom_t which is not usually visible
		 * to icrash. We should first look for struct 
		 */
		if(!(len=kl_struct_len(klp,"pmap_segcommon")))
		{
			len = 2 * K_NBPW(klp);
			if(DEBUG(KLDC_PAGE, 1)) 
			{
				fprintf(KL_ERRORFP,
					"kl_pmap_to_pde:Couldn't find "
					"structure pmap_segcommon.\n"
					"kl_pmap_to_pde:Assuming its size to"
					" be %d bytes\n",len);
			}
		}
					
		trilevelptr = kl_kaddr(klp, pp, "pmap", "pmap_ptr") 
			+ (KL_BTOBASETABINDEX(STOB(segno)) * len);
		if (!(Psegptr = kl_get_struct(klp,trilevelptr,
					      PMAP_SIZE(klp), NULL, 
					      "pmap"))) 
		{
			if (DEBUG(KLDC_PAGE, 1)) {
				fprintf(KL_ERRORFP, 
					"kl_pmap_to_pde:could not get pmap "
					"0x%llx\n",
					trilevelptr);
			}
			return (k_ptr_t)NULL;
		}

		segindex = KL_BTOSEGTABINDEX(STOB(segno));
		segptr = kl_kaddr(klp, Psegptr, "pmap", "pmap_ptr")
			+ K_NBPW(klp)*segindex;
		K_BLOCK_FREE(klp)(NULL,Psegptr);
		
		if (!kl_get_kaddr(klp, segptr, &sp, "segptr")) {
			if (DEBUG(KLDC_PAGE, 1)) {
				fprintf(KL_ERRORFP, "kl_pmap_to_pde: "
					"couldn't read segment "
					"pointer at 0x%llx\n", segptr);
			}
			return((k_ptr_t)NULL);
		}
	    
		/*
		 * The code is the same from here on as in the PMAP_SEGMENT 
		 * case..
		 */
		*pde = sp + (((vaddr%K_NBPS(klp))/K_NBPC(klp)) * 
			     PDE_SIZE(klp));
	      
		kl_get_pde(klp, *pde, pdebuf);
		if (KL_ERROR) {
			if (DEBUG(KLDC_PAGE, 1)) {
				fprintf(KL_ERRORFP, "kl_pmap_to_pde: "
					"couldn't read pde at 0x%llx\n", 
					*pde);
			}
			return((k_ptr_t)NULL);
		}
		return(pdebuf);
	}
	return((k_ptr_t)NULL);
}
