/* Test tlb modified exception
 */

#ident "$Revision: 1.4 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <fault.h>
#include <libsk.h>
#include "tlb.h"
#include "fforward/cache/cache.h"




/********************Remember to change the magical # 12!!!!! */

/*  In 64 bit mode we need to explicitly extend small ints to pointer size
 * before pointer cast.
 */
#define const2ptr(addr)	((volatile u_char *)(__psunsigned_t)(addr))

int
tlbmod(__psunsigned_t *pt, u_int *not_used1, u_int *not_used2)
{
    int	error = 0;
    jmp_buf	faultbuf;
    int	i;
         
    u_int	tlb_attrib = TLBLO_V | TLBLO_UNCACHED;
    int	set,count;
    unsigned long	tmp; 
	long tmp2;

    msg_printf(VRB, "TLB mod bit test\n");


    for (count = 0; count < NTLBENTRIES; count++)
      {
	  msg_printf(DBG,"pt[%d]=%016x\t %016x\n", count,pt[count],
		     *( (volatile u_char *)(__psunsigned_t)PHYS_TO_K1(pt[count]) ) );
      }


    for (set = 0; set < NTLBSETS; set++)  {
	msg_printf(DBG,"Set: %d \n", set );
	tmp2 = 0;

	for (i = 0; i < NTLBENTRIES; i++) {

	    tfptlbfill(set,0, i << (TLBHI_VPNSHIFT), ( (pt[i] & TLBLO_PFNMASK) | tlb_attrib) );

			/*tlbwired(i, 0, 0,
			    (pt[i * 2] >> 6) | tlb_attrib,
			    (pt[i * 2 + 1] >> 6) | tlb_attrib);
*/

	    if (setjmp(faultbuf)) {

		if ( _badvaddr_save != (__psunsigned_t)( i << TLBHI_VPNSHIFT )){
		    msg_printf (ERR,
				"BadVaddr, Expected: %016x, Actual: 0x%x\n",
				(i << TLBHI_VPNSHIFT), _badvaddr_save);
		    error++;
		}
	  
		DoEret();
	    } else {
		nofault = faultbuf;

		*const2ptr( i << TLBHI_VPNSHIFT ) = i;

		nofault = 0;
		msg_printf (ERR,
			    "Did not receive expected exception\n");
		error++;
	    }

/*				invaltlb(i);*/
	    tlbpurge();


#ifdef TFP
	    tmp2 = tmp2 + NEXTVPN;
#endif
	}
#ifdef TFP
    }
#endif


#ifdef TFP

    for (set = 0; set < NTLBSETS; set++)  {

	msg_printf(DBG,"Set: %d \n", set );
	tmp2 = 0;

#endif

	for (i = 0; i < NTLBENTRIES; i++) {

	    msg_printf(DBG,"before tlbtfpfill ... %016x\t%d  \n", pt[i] | tlb_attrib | TLBLO_D , i) ;
	    tfptlbfill(set, 0, i << ( TLBHI_VPNSHIFT ) ,  ((pt[i] & TLBLO_PFNMASK) | tlb_attrib | TLBLO_D ) );
	    tmp = get_cp0_tlbhi();
	    msg_printf(DBG,"tlbhi  = %016x  ", tmp);
	    tmp = get_cp0_tlblo();
	    msg_printf(DBG,"tlblo  = %016x  ", tmp);
	    tmp = get_cp0_badvaddr();
	    msg_printf(DBG,"badvaddr = %016x\n", tmp);

	    if (setjmp(faultbuf)) {
		show_fault();
		DoEret();
		return ++error;
	    } else {
		nofault = faultbuf;
	  
			  *const2ptr( (i << TLBHI_VPNSHIFT) + 1) = i; 

		/*	*const2ptr((i << TLBHI_VPNSHIFT)) = i;*/
/*		*(volatile u_char *)( (__psunsigned_t)((i << TLBHI_VPNSHIFT)) + 1 ) = i;*/

		msg_printf(DBG,"0(addr): 0x%016x, 1(addr): 0x%016x \n\t\tContents = %016x\n",
			   (volatile u_char *)(__psunsigned_t)((i << TLBHI_VPNSHIFT)),
			   (volatile u_char *)( (__psunsigned_t)((i << TLBHI_VPNSHIFT)) + 1 ),
			   *(volatile u_char *)( (__psunsigned_t)((i << TLBHI_VPNSHIFT)) + 1 ));

		/*	  *const2ptr( i << TLBHI_VPNSHIFT ) = i;	  */
		nofault = 0;

		if (*(volatile u_char *)(__psunsigned_t)((i << TLBHI_VPNSHIFT)) != *(volatile u_char *)( (__psunsigned_t)((i << TLBHI_VPNSHIFT)) + 1 ) ){
		    msg_printf(ERR,"Bad data, Expected: 0x%02x, Actual: 0x%02x\n",
			       *(volatile u_char *)(__psunsigned_t)((i << TLBHI_VPNSHIFT)),
			       *(volatile u_char *)( (__psunsigned_t)((i << TLBHI_VPNSHIFT)) + 1 ) );
		    error++;
		}

	    }

/*	invaltlb(i);*/
	    tlbpurge();

	    tmp2 = tmp2 + NEXTVPN;

	}


    }


    return error;
}

