/*
 * pbus_int_logic.c  - test the pbus logic thro DSP int status register
 *
 * Related Files: slot_dsp.h slot_dsp.asm
 */

#include "uif.h"
#include "sys/cpu.h"
#include "sys/param.h"
#include "sys/sbd.h"
#include "sys/types.h"

#include "slot_dsp.h"

#define	DSP_TEXT_SIZE	(sizeof(slot_dsp_text) / sizeof(u_int))
#define	DSP_DATA_SIZE	(sizeof(slot_dsp_ydata) / sizeof(u_int))

#define	DSP_SRAM_DATA	(u_int *)PHYS_TO_K1(HPC1MEMORY + (0x4000 << 2))
#define	DSP_SRAM_TEXT	(u_int *)PHYS_TO_K1(HPC1MEMORY + (0x2000 << 2))
#define	DSP_SRAM	(u_int *)PHYS_TO_K1(HPC1MEMORY)
#define	WAIT_COUNT		1000

#define	PBUS_INT_SLOT0_ADDR	0x1fbc0800  /* a6-a2 , all zeroes */
#define	PBUS_INT_SLOT1_ADDR	0x1fbc087c  /* a6-a2 , all ones */


#define		HNDLR_WRTN_VALUE	0x4321

static	volatile unsigned int  *slot_addr[] = {
		(unsigned int*)PHYS_TO_K1(PBUS_INT_SLOT0_ADDR),
		(unsigned int*)PHYS_TO_K1(PBUS_INT_SLOT1_ADDR)
		};
/*
   It checks for two logics.

   Write onto specific address on  slot0/slot1. That should make pint1(duart3)
   occur and the interrupt handler(running on DSP) should let CPU know
   int has occured by writing onto some SRAM location.

   Start graphics DMA with maximum burst. That should(?) guarantee a
   bus preempt(BPRE- goes active) and that inturn causes pint1(duart3).
   Check that same way as earlier logic.

   Read from the slot board and wait for a while and check no new pint1(duart3)
   has occured.
*/

int
pbus_interrupt_test( argc,argv)
int argc ;
char * argv [] ;
{
    int error_count0 = 0;
    int error_count1 = 0;
       int slot_no ;


	if(is_v50()) {
		return errcount;
	}
	if ( argc < 2 ) {
		printf("Usage:- %s <slot number>\n",argv[0]);
		printf("slot number : 0 - slot 0, 1 - slot 1, 2 - both\n");
		return(-1);
	}	

	if ( atoi(argv[1]) == 1 ) slot_no = 1;
	else if ( atoi(argv[1]) == 2 ) slot_no = 2;
	else slot_no = 0 ;



	msg_printf(DBG, "\n----- PBUS INTERRUPTS LOGIC TEST -----\n");
	switch(slot_no){
	 case 0: pbus_int(0,0); /* did not record error first time */ 
		 error_count0 = pbus_int(0,0)+pbus_int(0,1); break ;
	 case 1: error_count1 = pbus_int(1,0)+pbus_int(1,1); break ;
	 case 2: error_count0 = pbus_int(0,0)+ pbus_int(0,1);
		 error_count1 =	pbus_int(1,0)+pbus_int(1,1);break ;
	 default: break ;
	}

	if(error_count0)
		sum_error("Pbus Interrupt Logic - Slot 0");
	if(error_count1)
		sum_error("Pbus Interrupt Logic - Slot 1");

	msg_printf(DBG, "\n---- END OF PBUS INTERRUPTS LOGIC TEST -----\n");
	   

	return (error_count0+error_count1);
}

pbus_int(slot_no,bus_preempt_checking)
int slot_no,bus_preempt_checking;
{
    volatile u_int *hpc1cintstat = (u_int *)PHYS_TO_K1(HPC1CINTSTAT);
    volatile u_int *cpu_config =  (u_int *)PHYS_TO_K1(CPU_CONFIG);
    int gse_set = 0;
    int	i,j;
	volatile int tmp ;
    u_int *p;
    volatile u_int *q;
	int errcount = 0;
	int is_both ; /* pbus int logic checking is slightly different if both 
				 slots are present */

	is_both = new_hpc_probe(2) && new_hpc_probe(3) ;

    msg_printf(DBG,bus_preempt_checking ? 
		"Graphics DMA - BUS PREEMPT - PBUS interrupt functionality test\n":
		"DSP functionality test\n");

/* put the DSP in reset state */

    *(volatile u_int *)PHYS_TO_K1(HPC1MISCSR) = HPC1MISC_RESET;

/* copy DSP program text and data sections into DSP static RAM */

    for (i = 0, p = slot_dsp_text, q = DSP_SRAM_TEXT; i < DSP_TEXT_SIZE; i++)
	*q++ = *p++;
    for (i = 0, p = slot_dsp_ydata, q = DSP_SRAM_DATA; i < DSP_DATA_SIZE; i++)
	*q++ = *p++;

/*  Clear the HPC1CINTSTAT Interrupt Status bits */

    *hpc1cintstat=0;

/*  Make sure that they actually cleared */

    msg_printf(DBG,"Clearing the HPC1CINTSTAT register\n");
    i = *hpc1cintstat;
    if ((i & 0x7) != 0x0) {
		errcount++;
		msg_printf(VRB,
	    	"Stuck bit(s) in HPC1CINTSTAT register, 0x%x\n", i);
		goto slot_dsp_done;
    } else {
	msg_printf(DBG,"    Succeeded\n"); 
    }

/* Initialize the DSP Startup Flag */

    q = DSP_SRAM;
    *q = 0xdead;

/* unreset the DSP, mask out all interrupts, sign extension on */

    *(volatile u_int *)PHYS_TO_K1(HPC1CINTMASK) = 0x0;
    *(volatile u_int *)PHYS_TO_K1(HPC1MISCSR) = 0x48;

/* Wait for the DSP to set the startup flag. */

    msg_printf(DBG,"Waiting for DSP startup\n");

    for(i=0;i<WAIT_COUNT;i++) {
		j = *q;
		if(j != 0xdead)break;
		DELAY(100);
    }

    if(j == 0xdead) {
		errcount ++;
        msg_printf(VRB,
	    "DSP microcode failed to clear the startup flag\n");
		goto slot_dsp_done ;
    } 
	else {
        msg_printf(DBG,"    Succeeded %#x\n",j);
    }

    msg_printf(DBG,"DSP startup complete\n");


	*slot_addr[slot_no] = 0 ;
	*slot_addr[slot_no] = 0 ;
	*q = 0xabcd ;

	msg_printf(DBG,"Value in SRAM = #%x\n",j = *q);

		msg_printf(DBG,"Writing onto slot %d\n",slot_no);

		*slot_addr[slot_no] = 0 ;

/*
    for (i = 0; (i < WAIT_COUNT) ; i++)
			DELAY(1000);
*/
	DELAY(2500);

	if ( (j = *q) !=  HNDLR_WRTN_VALUE) {
		msg_printf(DBG,"int expected, but not happened %#x\n",j);
		errcount ++ ;
	}
	else 
		msg_printf(DBG,"expected int has happened %#x\n",j);

	

	msg_printf(DBG,"Reading from the slot %d\n",slot_no);

	tmp = *slot_addr[slot_no] ;

	*q = 0xabcd ; /* again reset to some known value */
/*
    for (i = 0; (i < WAIT_COUNT) ; i++)
			DELAY(1000);
*/

	DELAY(25000);
	if (  (j = *q ) !=  0xabcd  ) {

		if ( is_both ) {

			/* It may be the case when both the slots are connected.
			** In this case the other slot may still activate the interrupt.
			** So read from other slot also and again check.
			** (04-25-90)
			*/
			msg_printf(DBG,"Reading from the (other)slot %d\n",slot_no==0 ? 1 :0);
			tmp = *slot_addr[slot_no == 0 ? 1 : 0 ] ;

			*q = 0xabcd ; /* again reset to some known value */
/*
    		for (i = 0; (i < WAIT_COUNT) ; i++)
					DELAY(1000);
*/
			DELAY(25000);

			if (  (j = *q ) !=  0xabcd  ) {

					msg_printf(DBG,"int not expected, but happened %#x\n",j);
					errcount ++;
			}
		}
		else {
					msg_printf(DBG,"int not expected, but happened %#x\n",j);
					errcount ++;
		}
	}
	else
		msg_printf(DBG,"int not expected, and not happened %#x\n",j);
		

slot_dsp_done:
    *(volatile u_int *)PHYS_TO_K1(HPC1MISCSR) = HPC1MISC_RESET;
/*
    for (i = 0; (i < WAIT_COUNT) ; i++)
			DELAY(1000);
*/
	DELAY(25000);
    if (errcount)
			msg_printf(DBG,"Error in %s\n",bus_preempt_checking ? 
			"Pbus Interrupts - DMA - BPRE functionality" :
			"Pbus Interrupts - Write Slot - DSP functionality");

    return (errcount);
}
