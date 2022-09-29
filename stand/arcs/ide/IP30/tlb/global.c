/* Test TLB Global bit (on R10000).
 */
#include <fault.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <libsk.h>
#include "tlb.h"
#include "IP30/cache/cache.h"

/*ARGSUSED*/
int
tlbglobal(__psunsigned_t *pt, u_int *not_used1, u_int *not_used2)
{
	int	error = 0;
	jmp_buf	faultbuf;
	int	i;
#if IP26
#define TLBLO_G 0
#endif
	u_int	tlb_attrib = TLBLO_V | TLBLO_G | TLBLO_UNCACHED;
	__psunsigned_t	tpid;
	int	vpid;

	for (i = 0; i < NTLBENTRIES; i++) {
		for (vpid = 0x1; vpid < TLBHI_NPID; vpid <<= 1) {
			set_tlbpid(vpid);

			for (tpid = 0x1; tpid < TLBHI_NPID; tpid <<= 1) {
				volatile u_char bucket;

				tlbwired(i, vpid, (caddr_t)tpid,
					(uint)(pt[i * 2] >> 6) | tlb_attrib,
					(uint)(pt[i * 2 + 1] >> 6) | tlb_attrib);

				if (setjmp(faultbuf)) {
					show_fault();
					DoEret();
					return ++error;
				}

				nofault = faultbuf;
				bucket = *(volatile u_char *)0;
				nofault = 0;
				invaltlb(i);
			}
		}
	}

	return error;
}
