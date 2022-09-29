/* tlb valid test
 */

#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <libsk.h>
#include "tlb.h"
#include "fforward/cache/cache.h"

int
tlbvalid(__psunsigned_t *pt, u_int *not_used1, u_int *not_used2)
{
     int	error = 0;
     jmp_buf	faultbuf;
     int	i, set;
     unsigned long	tmp;
     msg_printf(VRB, "TLB valid bit test\n");

     tlbpurge ();

     for (set = 0; set < NTLBSETS; set++)  {

	  msg_printf(DBG,"Set: %d\n",set);

	  for (i = 0; i < NTLBENTRIES; i++) {
	       u_char *addr;
	       addr = (u_char *)(K2BASE + i * PAGESIZE );

	       msg_printf(DBG,"Current address is %016x\n",addr);

	       tfptlbfill(set, 0, (unsigned long)addr,
			  (pt[i] & TLBLO_PFNMASK) | TLBLO_UNCACHED );

	       tmp = get_cp0_tlblo();
		msg_printf(DBG,"tlblo = %016x\t", tmp);
		tmp = get_cp0_tlbhi();
		msg_printf(DBG,"tlbhi = %016x\t", tmp);
		tmp = get_cp0_badvaddr();
		msg_printf(DBG,"badvaddr = %016x\n", tmp);


	       if (setjmp(faultbuf)) {
			msg_printf(DBG,"Inside setjmp\n");
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

		    msg_printf (ERR,"Did not receive expected exception\n");
		    error++;
	       }
	       
	       /*	       invaltlb(i);*/
	       tlbpurge();
	  }



     }

     return error;
}
