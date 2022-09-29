/* Exception handler for tlb tests.
 */

#ident	"$Revision: 1.18 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/RACER/IP30addrs.h>
#include <fault.h>
#include <setjmp.h>
#include <libsk.h>
#include "tlb.h"
#include "IP30/cache/cache.h"

#define	MEMORY_SIZE	(PAGESIZE * NTLBENTRIES * 2)

int
utlb_miss_exception(void)
{
	volatile u_char	bucket;
	int		error = 0;
	jmp_buf		faultbuf;
	u_int		pfn;
	u_int		tlb = 0;
	u_int		tlb_attrib = TLBLO_G | TLBLO_V | TLBLO_NONCOHRNT;
	__psunsigned_t	vaddr;
	k_machreg_t	old_page;

	msg_printf(VRB|PRCPU, "Translation Lookaside Buffer Exception (UTLB) test\n");

	tlbpurge();
#if _PAGESZ == 4096
	old_page = set_pagesize(_page4K);
#endif
#if _PAGESZ == 16384
	old_page = set_pagesize(_page16K);
#endif
#if _PAGESZ != 4096 && _PAGESZ != 16384
	need_to_set_pagesize properly(); /* this should generate an error */
#endif

	for (vaddr = 0x0; vaddr < MEMORY_SIZE; vaddr += PAGESIZE * 2) {
		nofault = 0;
	
		if (setjmp(faultbuf)) {
			if (_exc_save == EXCEPT_UTLB) {

				if (_badvaddr_save != vaddr) {
					msg_printf(ERR|PRCPU,
						"BadVaddr: Expected: 0x%08x, Actual: 0x%08x\n",
						vaddr, _badvaddr_save);
					DoEret();
					error++;
					goto done;
				}

				/* might truncate but phys addr < 32-bit max */
				pfn = (uint)(vaddr + IP30_SCRATCH_MEM); 
				tlbwired(tlb++, 0, (caddr_t)vaddr,
					(pfn>>6) | tlb_attrib,
					((pfn+PAGESIZE)>>6) | tlb_attrib);

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
			bucket = *(volatile u_char *)vaddr;
			nofault = 0;

			msg_printf(ERR|PRCPU,
				"Did not receive UTLB MISS exception\n");
			error++;
			goto done;
		}

		if (setjmp(faultbuf)) {
			show_fault();
			DoEret();
			error++;
			goto done;
		}
		else {
			nofault = faultbuf;
			bucket = *(volatile u_char *)vaddr;
			bucket = *(volatile u_char *)(vaddr + PAGESIZE);
			nofault = 0;
		}
	}

done:
	restore_pagesize(old_page);
	if (!error)
		okydoky();
	else
		sum_error("UTLB MISS exception");

	return error;
}
