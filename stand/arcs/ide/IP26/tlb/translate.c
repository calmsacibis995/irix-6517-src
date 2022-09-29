/* tlb translation test.
 */

#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <libsk.h>
#include "tlb.h"
#include "fforward/cache/cache.h"



int
tlbtranslate(__psunsigned_t *pt, u_int *uncachedp, uint *not_used)
{
     int	error = 0;
     jmp_buf	faultbuf;
     int	i;

     int	set;
     unsigned long	tmp;
     u_int   *address_save;

     u_int	tlblo_attrib = TLBLO_V | TLBLO_UNCACHED;
     msg_printf(VRB, "TLB translation test\n");


     for (tmp = 0; tmp < NTLBENTRIES; tmp++)
       {
	    msg_printf(DBG,"pt[%03d] = %016x\n", tmp, pt[tmp]);
       }

     msg_printf(DBG,"page size mask = %016x\t KUSIZE = %016x\n NEXTVPN = %016x\n", 
	~(PAGESIZE-1), KUSIZE, NEXTVPN);

     address_save = uncachedp;


     for(set = 0; set < NTLBSETS; set++) {
	  msg_printf(DBG,"Set: %d \n", set );

	  uncachedp = address_save;

	  for (i = 0; i < NTLBENTRIES; i++) {
	       __psunsigned_t      addr;		
	       msg_printf(DBG,"\t\ti = %016x\n", i );		

	       for (addr = KUBASE; addr < KUSIZE; addr =   ( addr << 1  ) + (NEXTVPN) ) {
		    __psunsigned_t      _addr = addr & ~(PAGESIZE-1);  
		    u_char      bucket;
		    u_char     *ptr = (u_char *)uncachedp;
		    msg_printf(DBG, "_addr = %016x\n", _addr);
		    msg_printf(DBG, "addr  = %016x\n", addr);
		    msg_printf(DBG, "a2 for tlbhi = %016x\n",
			       ( (_addr)  & TLBHI_VPNMASK ) >> 12 ); 

		    tfptlbfill(set, 0, 
			       ( _addr & (TLBHI_VPNMASK << 7) ),
			       ( (pt[i] & TLBLO_PFNMASK )  | tlblo_attrib) );

		    tmp = get_cp0_tlblo();
		    msg_printf(DBG,"tlblo = %016x\t", tmp);
		    tmp = get_cp0_tlbhi();
		    msg_printf(DBG,"tlbhi = %016x\n\n", tmp);

		    if (setjmp(faultbuf)) {
			 show_fault();
			 DoEret();
			 return ++error;
		    }

		    nofault = faultbuf;
		    bucket = *(u_char *)(_addr);
		    msg_printf (DBG,
				"*ptr: 0x%016x, ptr: 0x%016x bucket: 0x%016x\n",
				*ptr, ptr, bucket);

		    nofault = 0;

		    if (bucket != *ptr) {
			 msg_printf (ERR,
				     "looking at *0x%x(uncached) vs *0x%x(cached)\n",
				     ptr, _addr);

			 msg_printf (ERR,
				     "Data, Expected: 0x%x, Actual: 0x%x\n",
				     *ptr, bucket);

			 msg_printf ( ERR,
				     "i = %d\t set = %d \t uncachedp = 0x%016x \n",
				     i, set, uncachedp);
					
			 error++;
		    }

		    nofault = faultbuf;
		    ptr += PAGESIZE;

/*		    invaltlb(i);*/
		   /* tlbpurge();*/

	       }

	       uncachedp += PAGESIZE / sizeof(*uncachedp);
	  } 
	tlbpurge();
     }

     return error;
}



