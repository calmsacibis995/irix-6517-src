#ident	"IP22diags/tlb/tlbtest.c:  $Revision: 1.4 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>
#include "tlb.h"

extern void __dcache_inval_all(void);

static int (*testfun[])(__psunsigned_t *,u_int *, u_int *) = {
	tlbmem,
/*	tlbprobe,    	not applicable to TFP*/
	tlbtranslate,
	tlbvalid,
	tlbmod,
	tlbpid,
/*	tlbglobal,	not applicable to TFP*/
	tlbnocache,
	0,
};

/*
 * The tlb test functions wire tlb entries from a page table which maps
 * various test-specific virtual addresses to NTLBENTRIES * PAGESIZE * 2 worth
 * of physical memory.
 */
int
tlbtest()
{
	u_int	*cachedp;
	int	error = 0;
	int	i;
	u_char	*k;
	u_char	*p;
	__psunsigned_t	pt[NTLBENTRIES];
	int	(**tf)();
	u_int	*uncachedp;
	k_machreg_t		old_page;

	msg_printf(VRB, "Translation Lookaside Buffer (TLB) test\n");

#if _PAGESZ == 4096
	old_page = set_pagesize(_page4K);
#endif
#if _PAGESZ == 16384
	old_page = set_pagesize(_page16K);
#endif
#if _PAGESZ != 4096 && _PAGESZ != 16384
	need_to_set_pagesize properly(); /* this should generate an error */
#endif
	k = (u_char *)((PHYS_TO_K1(0x08800000) + PAGESIZE - 1) & (-PAGESIZE));
	/*
	 * The memory mapped via pt[] starts at the
	 * K1 address of the biggest free memory chunk.
	 */
	p = (u_char *)K1_TO_PHYS(k);
	uncachedp = (u_int *)k;
	cachedp = (u_int *)PHYS_TO_K0(p);

	msg_printf(DBG,"uncachedp = %016x \t cachedp = %016x \n", uncachedp, cachedp);

	for (i = 0; i < NTLBENTRIES ; i ++ ) {
		*k = i;
		pt[i] = (__psunsigned_t) p;
		k += PAGESIZE;
		p += PAGESIZE;

/*		*k = i + 1;
		pt[i + 1] = (__psunsigned_t) p;
		k += PAGESIZE;
		p += PAGESIZE;*/
	}

	for (tf = testfun; *tf != 0; tf++) {
		tlbpurge();
		set_tlbpid(0);
		error += (**tf)(pt, uncachedp, cachedp); 
	}

	restore_pagesize(old_page);

	__dcache_inval_all();			/* Invalidate G+D cache */
	flush_cache();				/* Flush I, G+D caches */
	
	tlbpurge();
	flush_cache();

	if (!error)
		okydoky();
	else
		sum_error("translation lookaside buffer");

	return error;
}
