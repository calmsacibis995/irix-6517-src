/* Exception handler for tlb tests.
 */

#ident	"$Revision: 1.4 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <fault.h>
#include <setjmp.h>
#include <libsk.h>
#include "tlb.h"
#include "fforward/cache/cache.h"

#define	MEMORY_SIZE	(PAGESIZE * NTLBENTRIES * 2)

int
utlb_miss_exception()
{
    volatile u_char	bucket;
    int		error = 0;
    jmp_buf		faultbuf;
    u_int		pfn;
    u_int		tlb = 0;
    u_int		region,region2,region3,region4;
    u_int		tlb_attrib = TLBLO_V | TLBLO_NONCOHRNT;
    __psunsigned_t		vaddr;
    k_machreg_t		old_page;
    u_int			set;
    

#if _PAGESZ == 4096
	old_page = set_pagesize(_page4K);
#endif
#if _PAGESZ == 16384
	old_page = set_pagesize(_page16K);
#endif
#if _PAGESZ != 4096 && _PAGESZ != 16384
	need_to_set_pagesize properly(); /* this should generate an error */
#endif

#define EXCEPT_DESIRED EXCEPT_UTLB


    msg_printf(VRB, "UTLB MISS exception test\n");
    flush_cache();

    for (set = 0; set < NTLBSETS; set++){
	msg_printf(DBG,"Set: %d\n",set);

	region = 0;
	region4 = 0;
	region2 = 0;
	region3 = 0;

	tlbpurge();
	
	for (vaddr = 0x0; vaddr < MEMORY_SIZE; vaddr += PAGESIZE ) {

	    msg_printf(DBG,"vaddr = %016x\n",vaddr);
	    if (setjmp(faultbuf)) {
		region2++;
		msg_printf(DBG,"_exc_save = %016x\n expected = %016x\n",
			   _exc_save, (EXCEPT_DESIRED) );
		msg_printf(DBG,"\t\t");	
		if (_exc_save == EXCEPT_DESIRED) {
		    if (_badvaddr_save != vaddr) {
			msg_printf(ERR,"BadVaddr: Expected: 0x%08x, Actual: 0x%08x\n",
				   vaddr, _badvaddr_save);
			DoEret();
			error++;
			goto done;

		    }
/*  Low Memory starts at 0x08000000 */

		    tfptlbfill(set, 0, vaddr,
			       ( ( vaddr +  0x08000000 ) & TLBLO_PFNMASK) | tlb_attrib);
		    DoEret();
		}
		else {
		    show_fault();
		    DoEret();
		    error++;
		    goto done;
		}
	    }
	    else {
		nofault = faultbuf;
		region++;
		bucket = *(volatile u_char *)vaddr;
		nofault = 0;

		msg_printf(ERR,
			   "Did not receive UTLB MISS exception\n");
		error++;
		goto done;
	    }

	    if (setjmp(faultbuf)) {
		region4++;
		show_fault();
		DoEret();
		error++;
		goto done;
	    }
	    else {
		nofault = faultbuf;
		region3++;
		bucket = *(volatile u_char *)vaddr;
		nofault = 0;
	    }
	}

    }

  done:
    nofault = 0;
    tlbpurge();
    restore_pagesize(old_page);
    msg_printf(DBG,"%016x\n",vaddr);
    msg_printf(DBG,"region  %016x  region2 %016x\n",region,region2);
    msg_printf(DBG,"region3 %016x  region4 %016x\n",region3, region4); 
    if (!error)
      okydoky();
    else
      sum_error("UTLB MISS exception");
    
    return error;
}



