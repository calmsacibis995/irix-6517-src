/*  TLB pid (ASID) test.
 */

#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <libsk.h>
#include "tlb.h"
#include "fforward/cache/cache.h"
/*  Note, change 8 to bound vpid  */
int
tlbpid(__psunsigned_t *pt, u_int *not_used1, u_int *not_used2)
{
	
    int	error = 0;
    jmp_buf	faultbuf;
    long	i;
    bool_t	pidmismatch;
    u_int	tlblo_attrib = TLBLO_V | TLBLO_UNCACHED;
    int		vpid, tpid, set, region1,indexvpid,indextpid;
    static  	int  pat[] = {
                                        0x0,
                                        0x7f,
                                        0x2a,
                                        0x55,
                                };

    msg_printf(VRB, "TLB pid test\n");

    for (set =0; set < NTLBSETS; set++)  {
	msg_printf(DBG,"Set: %d\n", set);
	region1 = 0;
	for (i = 0; i < NTLBENTRIES; i++) {
	    for ( indexvpid = 0; indexvpid <  4; indexvpid++) {
		vpid = pat[indexvpid];
		msg_printf(DBG,"Entry : %d\n",i);
		for (indextpid = 0; indextpid < 4 ; indextpid++) {
		tpid = pat[indextpid];

		region1++;
#ifdef TFP
		    tfptlbfill(set, tpid, i << 12, 
			       (pt[i] & TLBLO_PFNMASK) | tlblo_attrib);  

#else
		    tlbwired(i, tpid, 0,
			     (pt[i * 2] >> 6) | tlblo_attrib,
			     (pt[i * 2 + 1] >> 6) | tlblo_attrib);
#endif
			
		    set_tlbpid(vpid);
		    pidmismatch = (tpid != vpid);
		    msg_printf(DBG,"\ttpid=%x\tvpid=%x\n",tpid,vpid);

		    if (setjmp(faultbuf)) {
			msg_printf(DBG,"Inside setjmp\n");
			if (!pidmismatch) {
			    show_fault();
			    DoEret();
			    tlbpurge();
			    return ++error;
					}
					
			if (_badvaddr_save != ( i << 12) ) {
			    msg_printf(ERR, "BadVaddr, Expected: %016x, Actual: 0x%08x\n",
				       i << 12,
				       _badvaddr_save);
			    error++;
			}

			DoEret();

		    } else {
			volatile u_char bucket;

			nofault = faultbuf;
#ifdef TFP
			bucket = *(volatile u_char *)(i << 12);
#else
			bucket = *(volatile u_char *)0;

#endif
			nofault = 0;
			if (pidmismatch) {
			    msg_printf (ERR,
					"Did not get expected exception\n");
			    error++;
			}
		    }

				/*invaltlb(i);*/
		    tlbpurge();
				
		}
	    }

	}

#ifdef TFP

    }

#endif
	msg_printf(DBG,"region1 = %d\n",region1);
    return error;
}


