#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsk.h>
#include "tlb.h"

/*
 * Test the tlb as a small memory array.  Just see if all the read/write
 * bits can be toggled and that all undefined bit read back zero.
 */
 
int
tlbmem(__psunsigned_t *not_used1, u_int *not_used2, u_int *not_used3)
{
       
    int	error = 0;
    int	i;
    int	j;
	

/*	from immu.h, since tlbwired declaration is a mismatch, include this */
/*	for now								    */

#if     _PAGESZ == 4096
#define PNUMSHFT        12              /* Shift for page number from addr. */
#define PFNHWMASK       TLBLO_PFNMASK
#endif
#if     _PAGESZ == 16384
#define PNUMSHFT        14              /* Shift for page number from addr. */
#define	PFNHWMASK	(TLBLO_PFNMASK << 2) & TLBLO_PFNMASK
#endif
#if     _PAGESZ == 65536
#define PNUMSHFT        16              /* Shift for page number from addr. */
#define	PFNHWMASK	(TLBLO_PFNMASK << 4) & TLBLO_PFNMASK
#endif

/*************************End of stuff borrowed from immu.h *****************/

#define  TLBHIUSED	0xc000fffffff80ff0
#define  TLBHIREGVPNMASK  0x0000fffffff80000

    int set;
    __psunsigned_t	expected_pat;
    __psunsigned_t	rpat;
    __psunsigned_t	tlbhi_mask = TLBHIUSED;
    __psunsigned_t	tlblo_mask = PFNHWMASK | TLBLO_CACHMASK | TLBLO_D | TLBLO_V; 
    __psunsigned_t	wpat, tmp;

    msg_printf(VRB, "TLB data test\n");
    wpat = 0xa5a5a5a5a5a5a5a5;

    /*
     * dwong: without the following read, this test will fail within IDE
     * if report level is set to verbose and the machine has GR2 graphics
     */
    
    *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);

    msg_printf(VRB,"\tTesting lower entries \n");
    
    for (set = 0; set < NTLBSETS; set++)  {

	msg_printf(DBG,"Set: %d\n", set);

	for (i = 0; i < NTLBENTRIES; i++) {
		msg_printf(DBG,"\tEntry = %d\n",i);
	    for (j = 0; j < 2; j++) {

/* 
 *  We want to run through all the indices of the tlb, and these are bits 18:12
 *  of the virtual address.  These bits are NOT stored in the EntryHi register
 *  The wpat will be written to the ASID field and the VPN fields of the EntryHi
 */


/*
 *  The arguments for the tfptlbfill are:
 *  a0 set
 *  a1 ASID
 *  a2 virtual address
 *  a3 pte content
 *
 */


/*		       tfptlbfill(set, 0, ( i << PNUMSHFT ) << 7 , wpat & tlblo_mask );*/

		msg_printf(DBG,"Writing      = %016x\n", wpat & tlblo_mask);

		tfptlbfill(set, 0, ( i << 12 ) , wpat & tlblo_mask );

		rpat = 0;
		set_cp0_tlbset(set);
		set_cp0_tlblo(0);
		set_cp0_tlbhi(0);
		set_cp0_tlbhi( ( (i << 21 ) & 0xfffffff80000 ) ); 
		set_cp0_badvaddr( ( i << 12 ) ); 
		cp0_tlb_read();
		rpat = get_cp0_tlblo();

		msg_printf(DBG,"        rpat = %016x\n", rpat);

/*	Need to verify why bits 12-13 don't match at times  
 *	Are they valid?  Are the page size dependent?
 *
 */


		rpat = rpat & tlblo_mask;

		msg_printf(DBG," masked rpat = %016x\n", rpat);

		expected_pat = ( wpat  & tlblo_mask);

		
		msg_printf(DBG,"expected_pat = %016x\n\n", expected_pat);
		if (rpat != expected_pat) {
		    msg_printf(ERR,"TLB entry %2d LO 0: Expected:0x%x, Actual:0x%x\n",
			       i, expected_pat, rpat);
		    error++;
		}

		wpat = ~wpat;

	    }

	    /*invaltlb_in_set(i,set);*/
	    tlbpurge();
	}

    }


    msg_printf(VRB,"\tTesting upper entries\n");

    for (set = 0; set < NTLBSETS; set++) {
	
	msg_printf(DBG,"Set: %d\n",set);
	
	for (i = 0; i < NTLBENTRIES; i++) {
	
	    for (j = 0; j < 2; j++) {

		msg_printf(DBG,"\n\nvaddr = %016x\tindex = %016x vpn = %016x\n", 
			   (( i << 12 )) | ( wpat & TLBHI_VPNMASK), 
			   i << 12, 
			   wpat & TLBHI_VPNMASK );

		/* tfptlbfill(set, wpat & 0xff, ( i << PNUMSHFT ) << 7) | ( wpat & TLBHI_VPNMASK) , 0);*/

		tfptlbfill(set, 
			   wpat & 0xff, 
			   (i << 12) | (wpat & TLBHI_VPNMASK | (wpat & TLBHI_REGIONMASK)),
			   0);

		msg_printf(DBG,"set = %d\tentry = %d\n",set,i);

		expected_pat = (wpat & 0xff) << TLBHI_PIDSHIFT;
		expected_pat =  expected_pat | ( wpat & TLBHI_VPNMASK );
		expected_pat = expected_pat | (wpat & TLBHI_REGIONMASK);
		expected_pat =  expected_pat & TLBHIUSED ;

		set_cp0_tlbset(set);
		set_cp0_badvaddr( (i << 12) | ( wpat & TLBHI_VPNMASK ) );
		cp0_tlb_read();
		rpat = get_cp0_tlbhi();
		msg_printf(DBG,"        rpat = %016x\n", rpat);
/*		msg_printf(DBG,"VPN %x\t ASID %x \n", 
			   (rpat & TLBHI_VPNMASK) >> 19, 
			   (rpat & TLBHI_PIDMASK) >> 4);*/
		rpat = rpat & tlbhi_mask ; 
		msg_printf(DBG,"expected_pat = %016x\n",expected_pat);

		if (rpat != expected_pat) {
		    msg_printf(ERR,
			       "TLB set %d entry %2d HI: Expected:0x%016x, Actual:0x%016x\n",
			       set, i, expected_pat, rpat);
		    error++;
		}

		wpat = ~wpat;
	    }
	
	    tlbpurge();

	}

    }

    return error;
}



















