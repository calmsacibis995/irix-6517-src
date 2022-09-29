static char rcsid[] = "$Id: isdn_dis.c,v 1.16 1996/07/16 17:06:34 jeffs Exp $";
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "setjmp.h"
#include "fault.h"
#include "saioctl.h"
#include "libsc.h"
#include "libsk.h"
#include "uif.h"
#include "isdn.h"

#define DEBUG

#define INT_LOOP_COUNT 10

#ifdef DEBUG_INFO
extern int isdn_dbginfo();
#endif /* DEBUG_INFO */
static int test_dma(int hscx_channel);

/* 
 * isdn_dis: Test the ISDN chipset by
 * 1.  Read power up registers
 * 2.  Activate Test loop 3.  Check for interrupt and status change in CIR0.  (HSCX)
 * 
 */

int 
isdn_dis(int argc, char **argv)
{
	jmp_buf faultbuf;
	unsigned char ch;
	int	dma_xfer;
	unsigned int cbp;
	k_machreg_t oldsr = get_SR();
	int timeout=0;
	int lcount;
	int error_count = 0;
	int isac_err_count=0, hscx_err_count=0, hscx2_err_count=0;
	unsigned char ret;

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

	/* ISAC and HSCX registers */
	volatile unsigned char *dev_hscx_rfifo = (unsigned char *)
		HSCX_RECV_FIFO_BASE;
	volatile unsigned char *dev_isac_adf2 = (unsigned char *)
		PHYS_TO_K1(0x1fbd94e7);
	volatile unsigned char *dev_isac_sqxr = (unsigned char *)
		PHYS_TO_K1(0x1fbd94ef);
	volatile unsigned char *dev_isac_mode = (unsigned char *)
		PHYS_TO_K1(0x1fbd948b);
	volatile unsigned char *dev_isac_ista = (unsigned char *)
		PHYS_TO_K1(0x1fbd9483);
	volatile unsigned char *dev_isac_cix0 = (unsigned char *)
		PHYS_TO_K1(0x1fbd94c7);
	volatile unsigned char *dev_isac_star = (unsigned char *)
		PHYS_TO_K1(0x1fbd9487);
	volatile unsigned char *dev_isac_exir = (unsigned char *)
		PHYS_TO_K1(0x1fbd9493);
	volatile unsigned char *dev_isac_rbch = (unsigned char *)
		PHYS_TO_K1(0x1fbd94ab);

	volatile unsigned char *dev_ioc_reset = (unsigned char *)
		PHYS_TO_K1(0x1fbd9873);
	volatile unsigned char *dev_ioc_gcsel = (unsigned char *)
		PHYS_TO_K1(0x1fbd984b);
	volatile unsigned char *dev_ioc_gc = (unsigned char *)
		PHYS_TO_K1(0x1fbd984f);
	volatile unsigned char *dev_ioc_dmasel = (unsigned char *)
		PHYS_TO_K1(0x1fbd986b);

	volatile unsigned char *dev_hscx_mask = (unsigned char *)
		PHYS_TO_K1(MASK_ADR(1));
	volatile unsigned char *dev_hscx_mask2 = (unsigned char *)
		PHYS_TO_K1(MASK_ADR(2));
	volatile unsigned char *dev_hscx_timr = (unsigned char *)
		PHYS_TO_K1(TIMR_ADR(1));
	volatile unsigned char *dev_hscx_timr2 = (unsigned char *)
		PHYS_TO_K1(TIMR_ADR(2));
	volatile unsigned char *dev_hscx_xccr = (unsigned char *)
		PHYS_TO_K1(XCCR_ADR(1));
	volatile unsigned char *dev_hscx_rccr = (unsigned char *)
		PHYS_TO_K1(RCCR_ADR(1));
	volatile unsigned char *dev_hscx_xccr2 = (unsigned char *)
		PHYS_TO_K1(XCCR_ADR(2));
	volatile unsigned char *dev_hscx_rccr2 = (unsigned char *)
		PHYS_TO_K1(RCCR_ADR(2));
	volatile unsigned char *dev_hscx_ccr2 = (unsigned char *)
		PHYS_TO_K1(CCR2_ADR(1));
	volatile unsigned char *dev_hscx_ccr22 = (unsigned char *)
		PHYS_TO_K1(CCR2_ADR(2));
	volatile unsigned char *dev_hscx_ccr1 = (unsigned char *)
		PHYS_TO_K1(CCR1_ADR(1));
	volatile unsigned char *dev_hscx_ccr12 = (unsigned char *)
		PHYS_TO_K1(CCR1_ADR(2));
	volatile unsigned char *dev_hscx_mode = (unsigned char *)
		PHYS_TO_K1(MODE_ADR(1));
	volatile unsigned char *dev_hscx_mode2 = (unsigned char *)
		PHYS_TO_K1(MODE_ADR(2));
	volatile unsigned char *dev_hscx_tsax = (unsigned char *)
		PHYS_TO_K1(TSAX_ADR(1));
	volatile unsigned char *dev_hscx_tsar = (unsigned char *)
		PHYS_TO_K1(TSAR_ADR(1));
	volatile unsigned char *dev_hscx_cmdr = (unsigned char *)
		PHYS_TO_K1(CMDR_ADR(1));
	volatile unsigned char *dev_hscx_cmdr2 = (unsigned char *)
		PHYS_TO_K1(CMDR_ADR(2));
	volatile unsigned char *dev_hscx_xbch = (unsigned char *)
		PHYS_TO_K1(XBCH_ADR(1));
	volatile unsigned char *dev_hscx_xbcl = (unsigned char *)
		PHYS_TO_K1(XBCL_ADR(1));
	volatile unsigned char *dev_hscx_ista = (unsigned char *)
		PHYS_TO_K1(ISTA_ADR(1));
	volatile unsigned char *dev_hscx_ista2 = (unsigned char *)
		PHYS_TO_K1(ISTA_ADR(2));
	volatile unsigned char *dev_hscx_exir = (unsigned char *)
		PHYS_TO_K1(EXIR_ADR(1));
	volatile unsigned char *dev_hscx_exir2 = (unsigned char *)
		PHYS_TO_K1(EXIR_ADR(2));
	volatile unsigned char *dev_hscx_tsax2 = (unsigned char *)
		PHYS_TO_K1(TSAX_ADR(2));
	volatile unsigned char *dev_hscx_tsar2 = (unsigned char *)
		PHYS_TO_K1(TSAR_ADR(2));

	/*End of register declarations */

	msg_printf (VRB, "\nDisconnected ISDN test.\n");
	msg_printf (VRB, "*** Do not connect anything to the ISDN jack ***\n");

	*dev_ioc_reset = 0x7;		/* Resetting ISDN to clear HSCX registers */
	us_delay (MS1);
	*dev_isac_cix0 = 0x47;          /* Reset to clear ISAC registers */

	/* Set up the PIO config registers */
	*pbus_cfgpio_4 = 0x4e2b;
	*pbus_cfgpio_5 = 0x4e2b;



	/* Set up the IOC (turn off reset, Set the gen control register to out) */
	*dev_ioc_reset = 0xf;
	*dev_ioc_gcsel = IOC_GENSEL_SHAREDDMA;
	*dev_ioc_gc = 0;
	*dev_ioc_dmasel = 0xf;

	msg_printf (DBG, "Checking power up registers in ISAC(PEB 2086)....");
	ret = *dev_isac_adf2;
	if (ret != 0) {
		if (!isac_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "adf2 is 0x%02x, expected 0x0\n", ret);
		isac_err_count++;
	}
/*	ret = *dev_isac_sqxr;
	if (ret != 0) {
		if (!isac_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "sqxr is 0x%02x, expected 0x0\n", ret);
		isac_err_count++;
	}
*/
/*  Removed register test, Siemens chip analysis to follow... 8/9/93 */

	ret = *dev_isac_mode;
	if (ret != 0) {
		if (!isac_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "mode is 0x%02x, expected 0x0\n", ret);
		isac_err_count++;
	}
	ret = *dev_isac_ista;
	if (ret != 0) {
		if (!isac_err_count)
			msg_printf(VRB, "\n");
		if (ret & 0x8)
			msg_printf(VRB, "*** ERROR: ista TIN detected!\n");
		msg_printf(VRB, "ista is 0x%02x, expected 0x0\n", ret);
		isac_err_count++;
	}
	ret = *dev_isac_exir;
	if (ret != 0) {
		if (!isac_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "exir is 0x%02x, expected 0x0\n", ret);
		isac_err_count++;
	}
	ret = *dev_isac_cix0;
	if (ret != 0x7c) {
		if (!isac_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "cix0 is 0x%02x, expected 0x7c\n", ret);
		isac_err_count++;
	}

	/*    ret = *dev_isac_rbch;
    if ((ret & ISAC_VERS_MASK) != ISAC_VERS) {
	 if (!isac_err_count)
	      msg_printf(VRB, "\n");
	 msg_printf(VRB, "rbch is 0x%02x, expected at least 0x0\n", ISAC_VERS);
	 isac_err_count++;
    }  */

	ret = *dev_isac_star;
	if ((ret != 0x48) && (ret != 0x4a)) {
		if (!isac_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "star is 0x%02x, expected 0x48 or 0x4a\n", ret);
		isac_err_count++;
	} else if (ret == 0x4a) {
		msg_printf(DBG, "\nNote that star BVS is set. (error?)\n");
	}
	if (isac_err_count) {
		error_count++;
		msg_printf(VRB, " *** ISAC registers FAILED ***\n\n");
	} else {
		msg_printf(DBG, "ISAC registers O.K.\n\n");
	}


	msg_printf (DBG, "Checking power up registers in HSCX(SAB 82525) channel 1....");
	ret = *dev_hscx_ista;
	if (ret != 0x0){
		if (!hscx_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "ista is 0x%02x, expected 0x0\n", ret);
		hscx_err_count++;
	}
	ret = *dev_hscx_cmdr;
	if (ret != 0x48){
		if (!hscx_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "cmdr is 0x%02x, expected 0x48\n", ret);
		hscx_err_count++;
	}
	ret = *dev_hscx_mode;
	if (ret != 0x0){
		if (!hscx_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "mode is 0x%02x, expected 0x0\n", ret);
		hscx_err_count++;
	}
	ret = *dev_hscx_exir;
	if (ret != 0x0){
		if (!hscx_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "exir is 0x%02x, expected 0x0\n", ret);
		hscx_err_count++;
	}
	if (hscx_err_count) {
		error_count++;
		msg_printf(VRB, " *** HSCX Ch. 1 registers FAILED ***\n\n");
	} else {
		msg_printf(DBG, "HSCX Ch. 1 O.K.\n\n");
	}


	msg_printf (DBG, "Checking power up registers in HSCX(SAB 82525) channel 2....");
	ret = *dev_hscx_ista2;
	if (ret != 0x0){
		if (!hscx2_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "ista2 is 0x%02x, expected 0x0\n", ret);
		hscx2_err_count++;
	}
	ret = *dev_hscx_cmdr2;
	if (ret != 0x48){
		if (!hscx2_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "cmdr2 is 0x%02x, expected 0x48\n", ret);
		hscx2_err_count++;
	}
	ret = *dev_hscx_mode2;
	if (ret != 0x0){
		if (!hscx2_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "mode2 is 0x%02x, expected 0x0\n", ret);
		hscx2_err_count++;
	}
	ret = *dev_hscx_exir2;
	if (ret != 0x0){
		if (!hscx2_err_count)
			msg_printf(VRB, "\n");
		msg_printf(VRB, "exir2 is 0x%02x, expected 0x0\n", ret);
		hscx2_err_count++;
	}

	if (hscx2_err_count) {
		error_count++;
		msg_printf(VRB, " *** HSCX Ch. 2 registers FAILED ***\n\n");
	} else {
		msg_printf(DBG, "HSCX Ch. 2 O.K.\n\n");
	}



	msg_printf(DBG, "Activating Test Loop on ISAC, test interrupts....");
	/* Set up the ISAC */
	*dev_isac_adf2 = 0x80;	/* IOM2 interface mode */
	*dev_isac_sqxr = 0x0f;	/* SQ Interrupt disabled, tx FA bits */
	*dev_isac_mode = 0xc8;	/* Transparent Mode 2, Receiver Active */

	ret = *dev_isac_cix0;	/* read out old stuff */

	us_delay(MS1);
	us_delay(MS1);
	us_delay(MS1);
	us_delay(MS1);

	*dev_isac_cix0 = 0x43;	/* activate output clocks TIM */

	/* read a couple to get rid of stuff in between */
	ret = *dev_isac_cix0;
	us_delay(MS1);
	ret = *dev_isac_cix0;
	us_delay(MS1);
	ret = *dev_isac_cix0;

	/* Start sending commands that will cause interrupts */
	*dev_isac_cix0 = 0x6b;		/* Activate Request Loop */

	for (lcount=0; lcount < INT_LOOP_COUNT; lcount++) {
		if (setjmp (faultbuf)) {
			char l;

			l = *K1_INT3_L1_ISR_ADDR;
#ifdef debug
			msg_printf(VRB, "int3 ISR = 0x%08x\n", l);
			msg_printf(VRB, "exc_save = 0x%08x\n", _exc_save);
			msg_printf(VRB, "cause_save = 0x%08x\n", _cause_save);
#endif /* debug */
			/* interrupt muck xxx */
			if ((_exc_save == EXCEPT_NORM) && 
			    ((_cause_save & CAUSE_EXCMASK) == EXC_INT) && 
			    (_cause_save & CAUSE_IP4) && 
			    (l & LIO_PEB) && /* active low */
			((ret = *dev_isac_ista) & ISTA_CISQ)) {
				/*		   msg_printf(VRB, "*** Received CISQ interrupt *** \n"); */
				if (ret & ISTA_TIN) {
					msg_printf(VRB, "*** Received TIN interrupt *** \n");
					error_count++;
					goto error_exit;
				}
				ret = *dev_isac_cix0;
				/*		   msg_printf(VRB, "CIR0 = 0x%08x\n", ret); */
				if (ret & CIR0_CIC0) {
					switch(ret & CIR0_CODR0_MASK) {
					case CIR0_RSY:
						msg_printf(VRB, "CIR0 indicates F5 unsynchronized (RSY)\n");
						break;
					case CIR0_AI8:
						msg_printf(VRB, "CIR0 indicates F7 activated (AI8)\n");
						break;
					case CIR0_AI10:
						/* got one of these, but the next CISQ was 0x02
			      * so doing another AR10 does not help */
						/* *dev_isac_cix0 = 0x67; */
						msg_printf(VRB, "CIR0 indicates F7 activated (AI10)\n");
						break;
					case CIR0_ARD:
						msg_printf(VRB, "CIR0 indicates F6 synchronized (ARD)\n");
						break;
					case CIR0_EI:
						msg_printf(VRB, "CIR0 -> Error Indication\n");
						break;
					case CIR0_DID:
						msg_printf(VRB, "CIR0 -> Deactivation Indication\n");
						break;
					case CIR0_DR:
						msg_printf(VRB, "CIR0 indicates deactivate request\n");
						break;
					case CIR0_TI:
						msg_printf(DBG, "got interrupt, test loop O.K.\n\n");
						lcount = INT_LOOP_COUNT;	/* break out of loop */
						break;

					default:
						msg_printf(VRB, "CIR0: unknown state (0x%x)", ret);
#ifdef DEBUG_INFO
						isdn_dbginfo();
#endif /* DEBUG_INFO */
						error_count++;
						break;
					}
				}
#ifdef ignore		   
				if (ret & CIR0_SQC) {
					ret = *dev_isac_sqxr;
					if (ret & SQRR_SYN)
						msg_printf(VRB, "CIR0/SQRR indicates sync\n");
					else 
						msg_printf(VRB, "CIR0/SQRR indicates NO/LOSS OF sync\n");
				}
#endif /* ignore */

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

		} else {	/* normal (no interrupt) case */
			/* Interrupt muck xxx*/
			nofault = faultbuf;
			timeout = 0;
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

			/*  Waiting for interrupt  */

			while (timeout < 10) {
				us_delay (MS1);
				timeout++;
			}
			msg_printf (ERR, "Time out waiting for ISDN interrupt\n");
			error_count++;
#ifdef DEBUG_INFO
			isdn_dbginfo();
#endif /* DEBUG_INFO */
		}   /* if (non-interrupt case) */
		msg_printf(VRB, "\n\n");
	}  /* for lcount loop */

	/* disable further interrupts */
	nofault = 0;
	set_SR(oldsr);
	*K1_INT3_L1_MASK_ADDR = 0x00;

	/* Reset ISAC */
	*dev_isac_cix0 = 0x47;

	msg_printf(DBG, "\nTesting DMA for channel B1...\n");
	if (test_dma(1)) {
		msg_printf(VRB, "\nError in DMA for channel B1\n");
		error_count++;
	} else {
		msg_printf(DBG, "O.K.\n");
	}

	msg_printf(DBG, "\nTesting DMA for channel B2...\n");
	if (test_dma(2)) {
		msg_printf(VRB, "\nError in DMA for channel B2\n");
		error_count++;
	} else {
		msg_printf(DBG, "O.K.\n");
	}

error_exit:
	if (!error_count)
		{
			okydoky();
		} else
		{
			sum_error("isdn_dis");
		}	
	return (error_count);
}




/************************************************************************
 * 
 *  PART II: Test HSCX DMA
 *
 ************************************************************************/

/* 
 * test_dma - test the HSXC dma by transmitting and receiving
 * it in loopback.
 */

static int 
test_dma(int hscx_channel)
{
	jmp_buf faultbuf;
	unsigned char ch;
	int	dma_xfer;
	unsigned char *cbp;
	k_machreg_t oldsr = get_SR();
	int timeout, i;
	int bcount;
	unsigned char *tx_buf = 0;
	unsigned char *rx_buf = 0;
	struct md *rx_mdesc=0, *tx_mdesc=0, *head_desc=0,*link_desc;
	int error_count = 0;
	int got_rme=0, got_xpr=0;
	int counter;
	__psunsigned_t desc_addr[5];
	char ret, rsta_val;
	int  pbus_tx_ch = 4;
	int  pbus_rx_ch = (hscx_channel==1) ? 5:6;


	/* PBUS registers */
	volatile unsigned int *pbus_tx_cfgdma = (unsigned int *)
		PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(pbus_tx_ch));
	volatile unsigned int *pbus_tx_control = (unsigned int *)
		PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(pbus_tx_ch));
	volatile unsigned int *pbus_tx_dp = (unsigned int *)
		PHYS_TO_K1(HPC3_PBUS_DP(pbus_tx_ch));
	volatile unsigned int *pbus_tx_bp = (unsigned int *)
		PHYS_TO_K1(HPC3_PBUS_BP(pbus_tx_ch));
	volatile unsigned int *pbus_rx_cfgdma = (unsigned int *)
		PHYS_TO_K1(HPC3_PBUS_CFGDMA_ADDR(pbus_rx_ch));
	volatile unsigned int *pbus_rx_control = (unsigned int *)
		PHYS_TO_K1(HPC3_PBUS_CONTROL_ADDR(pbus_rx_ch));
	volatile unsigned int *pbus_rx_dp = (unsigned int *)
		PHYS_TO_K1(HPC3_PBUS_DP(pbus_rx_ch));
	volatile unsigned int *pbus_rx_bp = (unsigned int *)
		PHYS_TO_K1(HPC3_PBUS_BP(pbus_rx_ch));

	/* IOC registers */
	volatile unsigned char *dev_ioc_gc = (unsigned char *)
		PHYS_TO_K1(0x1fbd984f);


	/* ISAC and HSCX registers */
	volatile unsigned char *dev_hscx_rfifo = (unsigned char *)
		HSCX_RECV_FIFO_BASE;
	volatile unsigned char *dev_isac_adf2 = (unsigned char *)
		PHYS_TO_K1(0x1fbd94e7);
	volatile unsigned char *dev_isac_sqxr = (unsigned char *)
		PHYS_TO_K1(0x1fbd94ef);
	volatile unsigned char *dev_isac_mode = (unsigned char *)
		PHYS_TO_K1(0x1fbd9488);
	volatile unsigned char *dev_isac_cix0 = (unsigned char *)
		PHYS_TO_K1(0x1fbd94c7);
	volatile unsigned char *dev_hscx_mask = (unsigned char *)
		PHYS_TO_K1(MASK_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_timr = (unsigned char *)
		PHYS_TO_K1(TIMR_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_xccr = (unsigned char *)
		PHYS_TO_K1(XCCR_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_rccr = (unsigned char *)
		PHYS_TO_K1(RCCR_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_ccr2 = (unsigned char *)
		PHYS_TO_K1(CCR2_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_ccr1 = (unsigned char *)
		PHYS_TO_K1(CCR1_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_mode = (unsigned char *)
		PHYS_TO_K1(MODE_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_tsax = (unsigned char *)
		PHYS_TO_K1(TSAX_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_tsar = (unsigned char *)
		PHYS_TO_K1(TSAR_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_cmdr = (unsigned char *)
		PHYS_TO_K1(CMDR_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_xbch = (unsigned char *)
		PHYS_TO_K1(XBCH_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_xbcl = (unsigned char *)
		PHYS_TO_K1(XBCL_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_ista = (unsigned char *)
		PHYS_TO_K1(ISTA_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_exir = (unsigned char *)
		PHYS_TO_K1(EXIR_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_rbch = (unsigned char *)
		PHYS_TO_K1(RBCH_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_rbcl = (unsigned char *)
		PHYS_TO_K1(RBCL_ADR(hscx_channel));
	volatile unsigned char *dev_hscx_rsta = (unsigned char *)
		PHYS_TO_K1(RSTA_ADR(hscx_channel));


	/* Configure pbus DMA
     * Was 0x80108a4, which gives:
     * drq_line
     * D5 Write: 4	D4 write: 2	D3 write: 2
     * D5 Read : 5	D4 read : 3	D3 read : 2
     *
     * But I will use larger values
     * drq_live
     * D5 Write: 8	D4 write: 15	D3 write: 3
     * D5 Read : 8	D4 read : 15	D3 read : 3
     */
	*pbus_tx_cfgdma = 0x08023f1f;
	*pbus_rx_cfgdma = 0x08023f1f;

	*dev_ioc_gc &= (hscx_channel==1) ? ~IOC_GENCTL_SHAREDTXB2 :
					    IOC_GENCTL_SHAREDTXB2 ;

	/* Set up the ISAC */
	*dev_isac_adf2 = 0x80;	/* IOM2 interface mode */
	*dev_isac_sqxr = 0x0f;	/* SQ Interrupt disabled, tx FA bits */
	*dev_isac_mode = 0xc8;	/* Transparent Mode 2, Receiver Active */


	/* Set up the HSXC */
	timeout = 5;
	*dev_hscx_mode = MODE_MDS1 | MODE_RAC | MODE_TRS | MODE_TLP;
	*dev_hscx_timr = 0xff;
	*dev_hscx_xccr = 0x7;	/* 8 bit width */
	*dev_hscx_rccr = 0x7;	/* 8 bit width */
	*dev_hscx_ccr2 = CCR2_XCS0 | CCR2_RCS0;	/* 1 tx and rx clock shift */
	*dev_hscx_tsax = 0x2c;	/* slot 11 */
	*dev_hscx_tsar = 0x2c;	/* slot 11 */

	/* power up, flags, clock mode 5 */
	*dev_hscx_ccr1 = CCR1_PU | CCR1_ITF | CCR1_CM2 | CCR1_CM0;

	/* Receive Message complete, rx and tx Reset */
	*dev_hscx_cmdr = CMDR_RMC | CMDR_RHR | CMDR_XRES;

	us_delay(MS1);

	ret = *dev_hscx_ista;	/* clear old ISTA bits by reading */
	ret = *dev_hscx_exir;	/* clear old EXIR bits by reading */

	/* get buffers that lie within a page */
	tx_buf = (unsigned char *) align_malloc (NCHARS, 0x1000);
	if (tx_buf == NULL) {
		msg_printf (ERR, "\nFailed to allocate data buffer\n");
		return (1);
	}
	rx_buf = (unsigned char *) align_malloc (RECV_BUF_SIZE, 0x1000);
	if (rx_buf == NULL) {
		msg_printf (ERR, "\nFailed to allocate data buffer\n");
		return (1);
	}

	/* get DMA memory descriptors */
	for (counter=0;counter!=NUM_DESCRIPTORS;counter++) {
		tx_mdesc = (struct md *) align_malloc (sizeof(struct md), 0x1000);
		if (tx_mdesc == NULL) {
			if (tx_buf) {
				align_free (tx_buf);
				align_free (rx_buf);
			}
			msg_printf (ERR, "\nFailed to allocate memory descriptor\n");
			return (1);
		} else {
			if (!head_desc)
				head_desc = (struct md *)(tx_mdesc);
			else  
			link_desc->nbdp = (__psunsigned_t) tx_mdesc;

			link_desc = (struct md *)(tx_mdesc);
			desc_addr[counter]=(__psunsigned_t) tx_mdesc;
		}
	}
	tx_mdesc->nbdp = 0;
	tx_mdesc = head_desc;

	/* Put data in buffer */
	bcount = 0;
	while (bcount < NCHARS) {
		tx_buf[bcount++] = 'a' + (bcount % 26);
	}

	bcount = 0;
	/* Initialize receive buffer to a different value */
	while (bcount < NCHARS) {
		rx_buf[bcount++] = 'b' + (bcount % 26);
	}

	/* Check for early interrupts */
	ret = *dev_hscx_ista;
	if (ret) {
		msg_printf(VRB, "\nAlready an interrupt pending -> 0x%02x\n", ret);
		error_count++;
		goto error_exit;
	}

	/* Execute DMA transfer */
	if (setjmp (faultbuf)) {
		int cmatch=1;

		/* interrupt muck xxx */
		if ((_exc_save == EXCEPT_NORM) && 
		    ((_cause_save & CAUSE_EXCMASK) == EXC_INT) && 
		    (_cause_save & CAUSE_IP4) && 
		    (*K1_INT3_L1_ISR_ADDR & LIO_SAB) &&
		    ((ret = *dev_hscx_ista) & ISTA_XPR | ISTA_RME)) {

			if (ret & ISTA_RME) {
				for (i=0; i < NCHARS; i++) {
					if (rx_buf[i] != (i % 26) + 'a') {
						msg_printf(VRB, "\ncharacter mismatch at %d  ", bcount);
						msg_printf(VRB, "got 0x%02x\n", ret);
						cmatch = 0;
					}
				}
				if (!cmatch) {
					error_count++;
					goto error_exit;
				}

				rsta_val = *dev_hscx_rsta;
				if (rx_buf[NCHARS] != rsta_val) {
					msg_printf(VRB, "\nRSTA not in last byte in fifo\n");
					msg_printf(VRB, "RSTA 0x%02x, last byte 0x%02x\n", rsta_val, rx_buf[NCHARS]);
					error_count++;
				}
				if (*dev_hscx_rbcl != NCHARS + 1) {
					msg_printf(VRB, "\nReceived byte count mismatch\n");
					msg_printf(VRB, "RBCH = 0x%02x\n", *dev_hscx_rbch);
					msg_printf(VRB, "RBCL = 0x%02x\n", *dev_hscx_rbcl);
					msg_printf(VRB, "Should be 0x%02x\n", NCHARS + 1);
					error_count++;
				}

				got_rme = 1;
			} /* end of ret & ISTA_RME */

			if (ret & ISTA_XPR ) {
				/* check that the HPC3 cbp is pointing to the end of the 
		    * requested tx buffer */
				if (GETDESC(*pbus_tx_bp) != (tx_mdesc->cbp+(tx_mdesc->bc&0xff))) {
					int byte=GETDESC(*pbus_tx_bp)-tx_mdesc->cbp;
					error_count++;
					msg_printf (ERR, "--- Inconsistent DMA descriptors ---");
					msg_printf (ERR, "pbus_cbp = 0x%x\n", GETDESC(*pbus_tx_bp));
					msg_printf (ERR, "tx_mdesc->cbp = 0x%x\n", tx_mdesc->cbp);
					msg_printf (ERR, "tx_mdesc->bc  = %d\n", tx_mdesc->bc);
					error_count++;
				}
				got_xpr = 1;
			}

			if (!got_xpr && !got_rme) {
				msg_printf(VRB, "\n*** Error in interrupt logic *** \n");
				error_count++;
			}

		} else {	/* abnormal interrupt */
			error_count++;
			msg_printf (ERR,
			    "Phantom interrupt during isdn write test\n");
			show_fault ();
			goto error_exit;
		}   /* if */

		/* stop dma */
		if (got_xpr)
			*pbus_tx_control = (hscx_channel==1) ? B1_DMA_TX_STOP :
							       B2_DMA_TX_STOP ;
		if (got_rme)
			*pbus_rx_control = (hscx_channel==1) ? B1_DMA_RX_STOP :
							       B2_DMA_RX_STOP ;

	} else {	/* normal (no interrupt) case */
		/* Set longjump context, enable CPU interrupt level 4 */
		nofault = faultbuf;
		*K1_INT3_L0_MASK_ADDR = 0;
		*K1_INT3_MM0R_MASK_ADDR = 0;
		*K1_INT3_MM1R_MASK_ADDR = 0;
		*K1_INT3_L1_MASK_ADDR = LIO_SAB_MASK; 	/* enable HSCX intr */
		set_SR(SR_IBIT4 | SR_IEC | (oldsr & ~SR_IMASK));
		*dev_hscx_mask = MASK_RSC | MASK_TIN;

		/* set the tx DMA descriptor */
		cbp = tx_buf;
		bcount = NCHARS;
		tx_mdesc->cbp = KDM_TO_PHYS(cbp);
		tx_mdesc->bc = 0x80000000 | bcount; /* eox hi */
		tx_mdesc->nbdp = 0;

		/* set the rx DMA descriptor */
		cbp = rx_buf;
		bcount = RECV_BUF_SIZE;
		rx_mdesc = (struct md *) desc_addr[1];
		rx_mdesc->cbp = KDM_TO_PHYS(cbp);
		rx_mdesc->bc = 0x80000000 | bcount; /* eox hi */
		rx_mdesc->nbdp = 0;

		/* start the HSCX dma */
		*pbus_tx_bp = 0xbad00bad;	/* set to unreasonable value */
		*pbus_tx_dp = KDM_TO_PHYS(tx_mdesc);
		*pbus_rx_bp = 0xbad00bad;	/* set to unreasonable value */
		*pbus_rx_dp = KDM_TO_PHYS(rx_mdesc);

		if ((*dev_hscx_cmdr & STAR_XFW) == 0) {
			msg_printf(VRB, "\nXFW not set in star. = 0x%02x\n", *dev_hscx_cmdr);
			error_count++;
			goto error_exit;
		}

		*dev_hscx_xbch = 0x80 | ((NCHARS-1) >> 8);/* dma mode,tx byte high */
		*dev_hscx_xbcl = NCHARS - 1;	/* dma mode, tx byte lo */

		if (*dev_hscx_cmdr & STAR_CEC) {
			msg_printf(VRB, "\nCommand in progress at STAR but should not\n");
			error_count++;
		} else {
			*dev_hscx_cmdr = CMDR_XTF;
		}
		*pbus_rx_control = (hscx_channel==1) ? B1_DMA_RX_START :
						       B2_DMA_RX_START ;
		*pbus_tx_control = (hscx_channel==1) ? B1_DMA_TX_START :
						       B2_DMA_TX_START ;

		/* Wait for XPR interrupt from HSCX */
		for (i=0; i < timeout; i++) {
			us_delay (S1);
		}

		msg_printf (ERR, "Time out waiting for ISDN DMA to complete\n");
		error_count++;
		*K1_INT3_L1_MASK_ADDR = 0x00;
		*dev_hscx_cmdr = CMDR_RMC | CMDR_RHR | CMDR_XRES;
		*pbus_tx_control = (hscx_channel==1) ? B1_DMA_TX_STOP :
						       B2_DMA_TX_STOP ;
		*pbus_rx_control = (hscx_channel==1) ? B1_DMA_RX_STOP :
						       B2_DMA_RX_STOP ;

	}   /* if (non-interrupt case) */

	if (error_count | got_rme) {
		goto error_exit;
	}

	/* Enable interrupts again to get RME */
	if (setjmp (faultbuf)) {
		int cmatch=1;

		/* interrupt muck xxx */
		if ((_exc_save == EXCEPT_NORM) && 
		    ((_cause_save & CAUSE_EXCMASK) == EXC_INT) && 
		    (_cause_save & CAUSE_IP4) && 
		    ((*K1_INT3_L1_ISR_ADDR) & LIO_SAB) &&
		    ((ret = *dev_hscx_ista) & ISTA_RME)) {
			if (ret & ISTA_RME) {
				for (i=0; i < NCHARS; i++) {
					if (rx_buf[i] != (i % 26) + 'a') {
						msg_printf(VRB, "\ncharacter mismatch at %d  ", bcount);
						msg_printf(VRB, "got 0x%02x\n", ret);
						cmatch = 0;
					}
				}
				if (!cmatch) {
					error_count++;
					goto error_exit;
				}

				rsta_val = *dev_hscx_rsta;
				if (rx_buf[NCHARS] != rsta_val) {
					msg_printf(VRB, "\nRSTA not in last byte in fifo\n");
					msg_printf(VRB, "RSTA 0x%02x, last byte 0x%02x\n", rsta_val, rx_buf[NCHARS]);
					error_count++;
				}
				if (*dev_hscx_rbcl != NCHARS + 1) {
					msg_printf(VRB, "\nReceived byte count mismatch\n");
					msg_printf(VRB, "RBCH = 0x%02x\n", *dev_hscx_rbch);
					msg_printf(VRB, "RBCL = 0x%02x\n", *dev_hscx_rbcl);
					msg_printf(VRB, "Should be 0x%02x\n", NCHARS + 1);
					error_count++;
				}
			} else {
				msg_printf(VRB, "\nGot interrupt, ista = 0x%02x\n", ret);
			}
		}
	} else {
		nofault = faultbuf;
		*K1_INT3_L0_MASK_ADDR = 0;
		*K1_INT3_MM0R_MASK_ADDR = 0;
		*K1_INT3_MM1R_MASK_ADDR = 0;
		*K1_INT3_L1_MASK_ADDR = LIO_SAB_MASK; 	/* enable HSCX intr */
		set_SR(SR_IBIT4 | SR_IEC | (oldsr & ~SR_IMASK));

		msg_printf(VRB, "Setting an interrupt for RME on channel %d\n",hscx_channel);
		for (i=0; i < timeout; i++) {
			us_delay (S1);
		}
		msg_printf (ERR, "Time out waiting for RME\n");
		msg_printf(VRB, "tx: pbus_tx_bp=0x%x, pbus_tx_control=0x%x , pbus_tx_dp=0x%x\n",*pbus_tx_bp,*pbus_tx_control,
		    *pbus_tx_dp);
		msg_printf(VRB, "rx: pbus_rx_bp=0x%x, pbus_rx_control=0x%x , pbus_rx_dp=0x%x\n",*pbus_rx_bp,*pbus_rx_control,
		    *pbus_rx_dp);

		msg_printf(VRB, "RBCH = 0x%02x\n", *dev_hscx_rbch);
		msg_printf(VRB, "RBCL = 0x%02x\n", *dev_hscx_rbcl);

		error_count++;
	}

error_exit:

	/* disable further interrupts */
	nofault = 0;
	set_SR(oldsr);
	*K1_INT3_L1_MASK_ADDR = 0x00;
#ifdef DEBUG_INFO
	if (error_count)
		isdn_dbginfo();
#endif /* DEBUG_INFO */

	*pbus_tx_control = (hscx_channel==1) ? B1_DMA_TX_STOP:B2_DMA_TX_STOP;
	*pbus_rx_control = (hscx_channel==1) ? B1_DMA_RX_STOP:B2_DMA_RX_STOP;

	if (tx_buf) {
		align_free (tx_buf);
		align_free (rx_buf);
	}
	for (counter=0; counter != NUM_DESCRIPTORS; counter++)
		align_free ((void *)desc_addr[counter]);

	return (error_count);
}



#ifdef DEBUG_INFO

int
isdn_dbginfo()
{
	extern int hscx_channel;

	msg_printf (VRB, "- ISAC Registers - \n");
	msg_printf (VRB, "CIR0 0x%02x, ISTA 0x%02x\n", 
	    *(unsigned char *)PHYS_TO_K1(R_CIR0),
	    *(unsigned char *)PHYS_TO_K1(R_ISTA));
	msg_printf (VRB, "STAR 0x%02x, EXIR 0x%02x\n", 
	    *(unsigned char *)PHYS_TO_K1(R_STAR),
	    *(unsigned char *)PHYS_TO_K1(R_EXIR));

	msg_printf (VRB, "- HSXC (chan %d) Registers - \n", hscx_channel);
	msg_printf (VRB, "ISTA 0x%02x, EXIR 0x%02x\n", 
	    *(unsigned char *)PHYS_TO_K1(ISTA_ADR(hscx_channel)),
	    *(unsigned char *)PHYS_TO_K1(EXIR_ADR(hscx_channel)));
	msg_printf (VRB, "STAR 0x%02x\n", 
	    *(unsigned char *)PHYS_TO_K1(STAR_ADR(hscx_channel)));
}
#endif  /*DEBUG_INFO*/
