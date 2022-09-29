/*  TLB pid (ASID) test.
 */

#ident "$Revision: 1.8 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <libsk.h>
#include "tlb.h"
#include "fforward/cache/cache.h"

/*ARGSUSED*/
int
tlbpid(__psunsigned_t *pt, u_int *not_used1, u_int *not_used2)
{
	int	error = 0;
	jmp_buf	faultbuf;
	int	i;
	bool_t	pidmismatch;
	int	vpid;
	u_int	tlblo_attrib = TLBLO_V | TLBLO_UNCACHED;
	int	tpid;

	msg_printf(VRB, "TLB pid test\n");

	for (i = 0; i < NTLBENTRIES; i++) {
		for (vpid = 0x1; vpid < TLBHI_NPID; vpid <<= 1) {
			set_tlbpid(vpid);
			for (tpid = 0x1; tpid < TLBHI_NPID; tpid <<= 1) {
				tlbwired(i, tpid, 0,
					(uint)((pt[i * 2] >> 6) | tlblo_attrib),
					(uint)((pt[i * 2 + 1] >> 6) | tlblo_attrib));
				pidmismatch = (tpid != vpid);

				if (setjmp(faultbuf)) {
					if (!pidmismatch) {
						show_fault();
						DoEret();
						return ++error;
					}
					
					if (_badvaddr_save != 0x0) {
						msg_printf(ERR, "BadVaddr, Expected: 0x0, Actual: 0x%08x\n",
							_badvaddr_save);
						error++;
					}

					DoEret();
				} else {
					volatile u_char bucket;

					nofault = faultbuf;
					bucket = *(volatile u_char *)0;
					nofault = 0;
					if (pidmismatch) {
						msg_printf (ERR,
							"Did not get expected exception\n");
						error++;
					}
				}

				invaltlb(i);
			}
		}
	}

	return error;
}
