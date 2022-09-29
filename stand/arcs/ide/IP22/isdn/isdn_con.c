static char rcsid[] = "$Id: isdn_con.c,v 1.10 1995/01/12 23:45:58 pap Exp $";
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "setjmp.h"
#include "fault.h"
#include "saioctl.h"
#include "uif.h"
#include "isdn.h"
#include "libsk.h"
#include "libsc.h"

#undef DEBUG

#define INT_LOOP_COUNT 10

#ifdef DEBUG_INFO
extern int isdn_dbginfo();
#endif /* DEBUG_INFO */
static int elapsed=0;
/* 
 * isac_act: Test the ISAC chip by giving it the AR8 command and check
 * CIRO register for confirmation.
 */

int 
isdn_con(argc, argv)
int argc;
char **argv;
{
    jmp_buf faultbuf;
    unsigned char ch;
    int	dma_xfer;
    unsigned int cbp;
    k_machreg_t oldsr = get_SR();
    int lcount, max_lcount, timeout=2000;
    int error_count = 0;
    unsigned char ret;
    int got_tone=0;
    int intcount=0;

	/* PBUS registers */
    volatile unsigned int *pbus_cfgpio_4 = (unsigned int *)
	PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(4));

    volatile unsigned int *pbus_cfgpio_5 = (unsigned int *)
        PHYS_TO_K1(HPC3_PBUS_CFG_ADDR(5));

    volatile unsigned int *pbus_cfgdma = (unsigned int *)
	PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(4));
    volatile unsigned int *pbus_control = (unsigned int *)
	PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(4));
    volatile unsigned int *pubs_nbdp = (unsigned int *)
	PHYS_TO_K1(HPC3_PBUS_DP(4));
    volatile unsigned int *pbus_cbp = (unsigned int *)
	PHYS_TO_K1(HPC3_PBUS_BP(4));

    /* ISAC registers */
  /*    volatile unsigned char *dev_isac_fifo = (unsigned char *)
         HSCX_RECV_FIFO_BASE; */
        volatile unsigned char *dev_isac_fifo = (unsigned char *)
         ISAC_FIFO_BASE;
	volatile unsigned char *dev_isac_adf2 = (unsigned char *)
	 PHYS_TO_K1(0x1fbd94e7);
    volatile unsigned char *dev_isac_adf1 = (unsigned char *)
	 PHYS_TO_K1(0x1fbd94e3);
    volatile unsigned char *dev_isac_sqxr = (unsigned char *)
	 PHYS_TO_K1(0x1fbd94ef);
    volatile unsigned char *dev_isac_mode = (unsigned char *)
	 PHYS_TO_K1(0x1fbd948b);
    volatile unsigned char *dev_isac_cmdr = (unsigned char *)
	 PHYS_TO_K1(0x1fbd9487);
    volatile unsigned char *dev_isac_ista = (unsigned char *)
	 PHYS_TO_K1(0x1fbd9483);
    volatile unsigned char *dev_isac_cix0 = (unsigned char *)
	 PHYS_TO_K1(0x1fbd94c7);
    volatile unsigned char *dev_isac_stcr = (unsigned char *)
	 PHYS_TO_K1(0x1fbd94df);
    volatile unsigned char *dev_isac_spcr = (unsigned char *)
	 PHYS_TO_K1(0x1fbd94c3);
    volatile unsigned char *dev_ioc_reset = (unsigned char *)
	 PHYS_TO_K1(0x1fbd9873);
    volatile unsigned char *dev_ioc_gcsel = (unsigned char *)
	 PHYS_TO_K1(0x1fbd984b);
    volatile unsigned char *dev_ioc_gc = (unsigned char *)
	 PHYS_TO_K1(0x1fbd984f);

    msg_printf (VRB, "ISDN ISAC(PEB) Dial Tone Test\n");
    msg_printf (VRB, "The ISDN jack should be connected to the NT1\n");
    msg_printf (VRB,"----------------------------------------------\n");

	/* Set up the PBUS CFG registers */
	*pbus_cfgpio_4 = 0x4e2b;
	*pbus_cfgpio_5 = 0x4e2b;



    /* Set up the IOC (turn off reset, activate CON) */
    *dev_ioc_reset = 0xf;
    *dev_ioc_gcsel = IOC_GENSEL_SHAREDDMA;
    *dev_ioc_gc = 0;

    /* Set up the ISAC */

    *dev_isac_adf2 = 0x80;	/* IOM2 interface mode */
    *dev_isac_adf1 = 0x01;	/* Interframe fill is flags */
    *dev_isac_sqxr = 0x1f;	/* SQ Interrupt enabled, tx FA bits */
/*    *dev_isac_sqxr = 0x0f; */	/* SQ Interrupt disabled, tx FA bits */
    *dev_isac_stcr = 0x70;	/* TIC bus address 7 */
    *dev_isac_mode = 0xc9;	/* Transparent Mode 2, Receiver Active, DIM2-1 == 001 (see p. 66) */
    *dev_isac_cix0 = 0x43;	/* activate output clocks TIM */

    /* Start sending commands that will cause interrupts */
    /* 0x67 - Activate request, priority 10 */
    /* 0x63 - Activate request, priority 8
     * The 2B+D channels can only be transmitted to the S/T interface if
     * an activate request command is written to the CIX0 register. p. 138 */
    us_delay(MS10);
    msg_printf(DBG, "Activating, request priority 8\n");
    *dev_isac_cix0 = 0x63;
    us_delay(MS10);
    if (argc > 1)
	 max_lcount = atoi(*++argv);
    else
	 max_lcount = 3;
    for (lcount=0; lcount < max_lcount; lcount++) {
	 if (setjmp (faultbuf)) {
	      char l;
	      intcount++; 
		msg_printf(DBG,"Interrupt Count: %d\n",intcount);
	      l = *K1_INT3_L1_ISR_ADDR;
#ifdef debug
	      msg_printf(DBG, "int3 ISR = 0x%08x\n", l);
	      msg_printf(DBG, "exc_save = 0x%08x\n", _exc_save);
	      msg_printf(DBG, "cause_save = 0x%08x\n", _cause_save);
#endif /* debug */
	      /* interrupt muck xxx */
	      if ((_exc_save == EXCEPT_NORM) && 
		  ((_cause_save & CAUSE_EXCMASK) == EXC_INT) && 
		  (_cause_save & CAUSE_IP4) && 
		  (l & LIO_PEB) && /* active low */
		  (*dev_isac_ista & ISTA_CISQ)) {
/*		   msg_printf(DBG, "*** Received CISQ interrupt *** \n"); */
		   ret = *dev_isac_cix0;
/*		   msg_printf(DBG, "CIR0 = 0x%08x\n", ret); */
		   if (ret & CIR0_CIC0) {
			switch(ret & CIR0_CODR0_MASK) {
			case CIR0_RSY:
			     msg_printf(DBG, "CIR0 indicates F5 unsynchronized (RSY)\n");
			     break;
			case CIR0_AI8:
			     msg_printf(DBG, "CIR0 indicates F7 activated (AI8)\n");
/*			     *dev_isac_cix0 = 0x63; */
			     got_tone = 1;
			     lcount = max_lcount;
			     break;
			case CIR0_AI10:
			     /* got one of these, but the next CISQ was 0x02
			      * so doing another AR10 does not help */
			     /* *dev_isac_cix0 = 0x67; */
			     msg_printf(DBG, "CIR0 indicates F7 activated (AI10)\n");
			     got_tone = 1;
			     lcount = max_lcount;
			     break;
			case CIR0_ARD:
			     msg_printf(DBG, "CIR0 indicates F6 synchronized (ARD)\n");
			     break;
			case CIR0_EI:
			     msg_printf(DBG, "CIR0 -> Error Indication\n");
#ifdef DEBUG_INFO
			     isdn_dbginfo();
#endif /* DEBUG_INFO */
			     /* if this is the first time, deactivate and try 
			      * again. */
			     if (intcount == 1) {
				  *dev_isac_cix0 = 0x7f;
				  us_delay(S1);
				  *dev_isac_cix0 = 0x63;
/*				  error_count++;*/
			     } else {
				error_count++;
				  goto error_exit;
			     }
			     break;
			case CIR0_DID:
			     msg_printf(DBG, "CIR0 -> Deactivation Indication\n");
			     break;
			case CIR0_PU:
			     msg_printf(DBG, "CIR0 -> Power up\n");
			     break;

			case CIR0_DR:
			     msg_printf(DBG, "CIR0 indicates deactivate request\n");
			     break;
			     
			default:
			     msg_printf(DBG, "CIR0: unknown state. (0x%02x)\n", ret);
#ifdef DEBUG_INFO
			     isdn_dbginfo();
#endif /* DEBUG_INFO */
			     error_count++;
			     break;
			}
		   }

		   if (ret & CIR0_SQC) {
			ret = *dev_isac_sqxr;
			if (ret & SQRR_SYN)
			     msg_printf(DBG, "CIR0/SQRR indicates sync\n");
			else 
			     msg_printf(DBG, "CIR0/SQRR indicates NO/LOSS OF sync\n");
		   }
		   
	      } else {	/* abnormal interrupt */
		   error_count++;
		   msg_printf (ERR,
			       "Phantom interrupt during isdn write test\n");
		   show_fault ();
#ifdef DEBUG_INFO
		   isdn_dbginfo();
#endif /* DEBUG_INFO */
	      }   /* if */
	      
	      /* Interrupt muck xxx */
	      /* This is a read only register, can't clear by reading */
#ifdef wrong
	      *K1_INT3_L1_ISR_ADDR = LIO_ISDN; 	/* clear isdn interrupt */
#endif /* wrong */
	      
	 } else {	/* normal (no interrupt) case */
	      /* Interrupt muck xxx*/
	      nofault = faultbuf;
	      elapsed = 0;
	      if (lcount == 0)
		   msg_printf (DBG, "enabling INT3 L1 interrupt\n");
	      /* enable isdn interrupt */
	      *K1_INT3_L1_MASK_ADDR = LIO_PEB_MASK;
	      *K1_INT3_L0_MASK_ADDR = 0;
	      *K1_INT3_MM0R_MASK_ADDR = 0;
	      *K1_INT3_MM1R_MASK_ADDR = 0;
		   
	      if (lcount == 0)
		   msg_printf (DBG, "enabling CPU interrupt 4\n");
	      /* enable CPU interrupt level 4 and turn off all others*/
	      set_SR(SR_IBIT4 | SR_IEC | (oldsr & ~SR_IMASK));

	      /* Wait for XPR interrupt from HSCX */
	      while (elapsed < timeout) {
		   us_delay (MS1);
		   us_delay (MS1);
		   us_delay (MS1);
		if (!(elapsed%500)) {
			busy(1);
				}	
		   elapsed++;
	      }
	      msg_printf (ERR, "Time out waiting for ISDN interrupt\n");
	      error_count++;
#ifdef DEBUG_INFO
	      isdn_dbginfo();
#endif /* DEBUG_INFO */
	 }   /* if (non-interrupt case) */
	 msg_printf(VRB, "elapsed time %d ms\n", elapsed);
	 msg_printf(VRB, "\n\n");
    }  /* for lcount loop */
	busy(0);
    if (got_tone) {
	 for (lcount=0; lcount < 32; lcount++) {
	      *dev_isac_fifo = (unsigned char)lcount;
	 }
	 /* transmit transparent frame XTF, tx message end XME */
	 msg_printf(VRB, "ISAC fifo filled, transmitting\n");
	 *dev_isac_cmdr = 0x0a;
    } else {
	printf("No tone...\n");
	}	
    /* disable further interrupts */
    nofault = 0;
    set_SR(oldsr);
    *K1_INT3_L1_MASK_ADDR = 0x00;

error_exit:

    if (!error_count)
	{
	okydoky();
	} else
	{
	sum_error("isdn_con");
	}
    return (error_count);
} 


