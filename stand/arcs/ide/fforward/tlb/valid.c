/* tlb valid test
 */

#ident "$Revision: 1.9 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <libsk.h>
#include "tlb.h"
#include "fforward/cache/cache.h"

/*ARGSUSED*/
int
tlbvalid(__psunsigned_t *pt, u_int *not_used1, u_int *not_used2)
{
	int	error = 0;
	jmp_buf	faultbuf;
	int	i;

	msg_printf(VRB, "TLB valid bit test\n");

	tlbpurge ();
	for (i = 0; i < NTLBENTRIES; i++) {
		u_char *addr;

		addr = (u_char *)(K2BASE + i * PAGESIZE * 2);
		tlbwired(i, 0, (caddr_t)addr,
			(uint)((pt[i * 2] >> 6) | TLBLO_UNCACHED),
			(uint)((pt[i * 2 + 1] >> 6) | TLBLO_UNCACHED));

		if (setjmp(faultbuf)) {
			if (_badvaddr_save != (__psunsigned_t)addr) {
				msg_printf (ERR,
					"BadVaddr, Expected: 0x%08x, Actual: 0x%08x\n",
					addr, _badvaddr_save);
				error++;
			}

			DoEret();
		} else {
			volatile u_char bucket;

			nofault = faultbuf;
			bucket = *(volatile u_char *)addr;
			nofault = 0;

			msg_printf (ERR,
				"Did not receive expected exception\n");
			error++;
		}

		invaltlb(i);
	}

	return error;
}
