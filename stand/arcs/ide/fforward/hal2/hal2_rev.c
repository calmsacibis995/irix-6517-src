/**********************************************************
***     Audio Board Diagnostic Tests                    ***
***                                                     ***
***     Amit Shoham, 1993                               ***
**********************************************************/

#include "sys/sbd.h"
#include "sys/cpu.h"
#include "sys/hal2.h"
#include "uif.h"

/* Board and Hal2 revs */

#define REV_PRESENT_MASK	0x8000
#define REV_BOARD_MASK		0x7000
#define REV_HAL2_MAJ_MASK	0x00f0
#define REV_HAL2_MIN_MASK	0x000f

static unsigned short	Rev;		/* contents of rev register */

static unsigned short	KnownBoards [] = { 0x0000, 0x1000, 0x4000, 0xffff };
static unsigned short	KnownHal2Maj [] = { 0x0010, 0xffff };

/* check hal2 revision register */
int hal2_rev()
{
	volatile unsigned int *revptr=(unsigned int *)PHYS_TO_K1(HAL2_REV); 
	unsigned short r0, r1, r2, r3; 
	int i;

	msg_printf(DBG,"checking HAL2 REV...\n"); 
	Rev = *revptr; 



/*  My additions 12/11/93  */

        if (Rev==0xffff)  {
                msg_printf(VRB,"HAL2_REV=0x%x\nthis is a server model\n",Rev);
                return(-1);
        }

/*  End of additions    */



	if (Rev & REV_PRESENT_MASK) {
		msg_printf(ERR,"no audio system detected!\n");
		msg_printf(VRB,"revision register reads 0x%x\n",Rev); 
		return(-1);
	} 
	for (i = 0; KnownBoards [i] != 0xffff; ++i) {
		if ((Rev & REV_BOARD_MASK) == KnownBoards [i]) {
			break; 
		} 
	} 
	if (KnownBoards [i] == 0xffff) {
		msg_printf(ERR,"Unknown board revision!\n");
		msg_printf(VRB,
		    "Board rev = %x, HAL2 major rev = %x, minor rev = %x\n",
		    Rev & REV_BOARD_MASK, Rev & REV_HAL2_MAJ_MASK, Rev &
		    REV_HAL2_MIN_MASK); 
		return(-1); 
	}

	for (i = 0; KnownHal2Maj [i] != 0xffff; ++i) {
		if ((Rev & REV_HAL2_MAJ_MASK) == KnownHal2Maj [i]) {
			break; 
		} 
	} 
	if (KnownHal2Maj [i] == 0xffff) {
		msg_printf(ERR,"Unknown hal2 revision!\n"); 
		msg_printf(VRB,
		    "Board rev = %x, HAL2 major rev = %x, minor rev = %x\n",
		    Rev & REV_BOARD_MASK, Rev & REV_HAL2_MAJ_MASK, Rev &
		    REV_HAL2_MIN_MASK); 
		return(-1);
	}

	/* Read the rev register fourty times and be sure that each time is
	  the same */
	for(i=0;i<10;i++) {
		r0= *revptr;    /* make some of the reads be back to back */
		r1= *revptr;
		r2= *revptr;
		r3= *revptr;
		if(r0 != Rev || r1 !=
		    Rev || r2 != Rev || r3 != Rev) {
			msg_printf(ERR,"rev register readback failed:\n");
			msg_printf(ERR,
			    "Rev=0x%x, r0=0x%x r1=0x%x, r2=0x%x, r3=0x%x\n",
			    Rev,r0,r1,r2,r3);
			return(-1);
		}
	}
	msg_printf(DBG,"\nREV register contains 0x%x\n",Rev);
	return(0);
}
