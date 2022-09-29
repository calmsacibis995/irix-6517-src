/* tlb valid test
 */

#ident "$Revision: 1.14 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <libsk.h>
#include "tlb.h"
#include "IP30/cache/cache.h"

#if R4000 || R10000	/* R4k and R10k tlb tests pnly! */

/* This routine loads entries into the TLB with the
   valid bit turned off. This should cause an exception
   when the virtual address is accessed ...
*/

/*ARGSUSED*/
int
tlbvalid(__psunsigned_t *pt, u_int *not_used1, u_int *not_used2)
{
	int	error = 0;
	jmp_buf	faultbuf;
	int	i;

	tlbpurge ();
	for (i = 0; i < NTLBENTRIES; i++) {
		u_char *addr;

		addr = (u_char *)(K2BASE + i * PAGESIZE * 2);
		
		/* write tlb entries with valid bit turned off */
		tlbwired(i, 0, (caddr_t)addr,
			(uint)((pt[i * 2] >> 6) | TLBLO_UNCACHED),
			(uint)((pt[i * 2 + 1] >> 6) | TLBLO_UNCACHED));

		/* do set up for tlb exception */
		if (setjmp(faultbuf)) {

			/* executed upon tlb exception */
			if (_badvaddr_save != (__psunsigned_t)addr) {
				msg_printf(ERR|PRCPU,
					"BadVaddr, Expected: 0x%08x, Actual: 0x%08x\n",
					addr, _badvaddr_save);
				error++;
			}

			DoEret();
		} else {
			volatile u_char bucket;

			/* do memory access to cause tlb fault */
			nofault = faultbuf;
			bucket = *(volatile u_char *)addr;
			nofault = 0;

			msg_printf(ERR|PRCPU,
				"Did not receive expected exception\n");
			error++;
		}
		invaltlb(i);
	}

	return error;
}
#endif	/* R4k and R10k tlb tests pnly! */
