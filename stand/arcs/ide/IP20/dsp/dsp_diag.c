/*
 * dsp_diag.c - test DSP functionalities
 */

#include "uif.h"
#include "sys/cpu.h"
#include "sys/param.h"
#include "sys/sbd.h"
#include "sys/types.h"

#include "dsp_diag.h"

#include <libsc.h>
#include <libsk.h>

#define	DSP_TEXT_SIZE	(sizeof(dsp_text) / sizeof(u_int))
#define	DSP_DATA_SIZE	(sizeof(dsp_ydata) / sizeof(u_int))



#define dbgprintf if(0)printf
#define	DSP_SRAM_DATA	(u_int *)PHYS_TO_K1(HPC1MEMORY + (0x4000 << 2))
#define	DSP_SRAM_TEXT	(u_int *)PHYS_TO_K1(HPC1MEMORY + (0x2000 << 2))
#define	DSP_SRAM	(u_int *)PHYS_TO_K1(HPC1MEMORY)
#define	WAIT_COUNT		1000
#define	BAD_DUART0		0x0001
#define	BAD_DUART1		0x0002
#define	BAD_DUART2		0x0004
#define	BAD_DUART3		0x0008
#define	BAD_RTC			0x0010
#define	BAD_HPC			0x0020
#define	BAD_DSP_WRITE_MIPS	0x0040
#define	BAD_DSP_READ_MIPS	0x0080
#define	BAD_DMA_DATA		0x0100
#define	BAD_SRAM_MAP		0x0200



/* DMA data */
u_int pattern[] = {
    ~0x000001, ~0x000002, ~0x000004, ~0x000008,
    ~0x000010, ~0x000020, ~0x000040, ~0x000080,
    ~0x000100, ~0x000200, ~0x000400, ~0x000800,
    ~0x001000, ~0x002000, ~0x004000, ~0x008000,
    ~0x010000, ~0x020000, ~0x040000, ~0x080000,
    ~0x100000, ~0x200000, ~0x400000, ~0x800000,
    0x000001, 0x000002, 0x000004, 0x000008,
    0x000010, 0x000020, 0x000040, 0x000080,
    0x000100, 0x000200, 0x000400, 0x000800,
    0x001000, 0x002000, 0x004000, 0x008000,
    0x010000, 0x020000, 0x040000, 0x080000,
    0x100000, 0x200000, 0x400000, 0x800000,
};
#define	DSP_DMA_COUNT	(sizeof(pattern) / sizeof(u_int))
u_int read_back[DSP_DMA_COUNT];



/*
   verify DSP functionalities
	. download DSP microcode to run diagnostics on the DSP
	. DSP passes diagnostics result to the MIPS
	. MIPS DMA to/from the DSP, verify the data
	. verify the handshake interrupt mechanism
*/
int
dsp_diag()
{
    int errcount = 0;
    volatile u_int *handrx = (u_int *)PHYS_TO_K1(HPC1HANDRX);
    volatile u_int *handtx = (u_int *)PHYS_TO_K1(HPC1HANDTX);
    volatile u_int *hpc1cintstat = (u_int *)PHYS_TO_K1(HPC1CINTSTAT);
    int	i;
    int j;
    int diag_result;
    u_int *p;
    volatile u_int *q;

    if(is_v50()) {
	return errcount;
    }
    msg_printf(VRB,"DSP functionality test\n");

/* put the DSP in reset state */

    *(volatile u_int *)PHYS_TO_K1(HPC1MISCSR) = HPC1MISC_RESET;

/* copy DSP program text and data sections into DSP static RAM */

    for (i = 0, p = dsp_text, q = DSP_SRAM_TEXT; i < DSP_TEXT_SIZE; i++)
	*q++ = *p++;
    for (i = 0, p = dsp_ydata, q = DSP_SRAM_DATA; i < DSP_DATA_SIZE; i++)
	*q++ = *p++;

/*  Clear the HPC1CINTSTAT Interrupt Status bits */

    *hpc1cintstat=0;

/*  Make sure that they actually cleared */

    dbgprintf("Clearing the HPC1CINTSTAT register\n");
    i = *hpc1cintstat;
    if ((i & 0x7) != 0x0) {
	errcount++;
	msg_printf(ERR,
	    "Stuck bit(s) in HPC1CINTSTAT register, 0x%x\n", i);
	goto dsp_diag_done;
    } else {
	dbgprintf("    Succeeded\n"); 
    }

/* Initialize the DSP Startup Flag */

    q = DSP_SRAM;
    *q = 0xdead;

/* unreset the DSP, mask out all interrupts, sign extension on */

    *(volatile u_int *)PHYS_TO_K1(HPC1CINTMASK) = 0x0;
    *(volatile u_int *)PHYS_TO_K1(HPC1MISCSR) = 0x48;

/* Wait for the DSP to set the startup flag. */

    dbgprintf("Waiting for DSP startup\n");
    for(i=0;i<WAIT_COUNT;i++) {
	j = *q;
	if(j != 0xdead)break;
	DELAY(10);
    }
    if(j == 0xdead) {
        msg_printf(ERR,
	    "DSP microcode failed to clear the startup flag\n");
    } else {
        dbgprintf("    Succeeded 0x%x\n",j);
    }
    dbgprintf("DSP startup complete\n");

/* wait for diagnostics result */

    dbgprintf("Waiting for the DSP to complete its test.\n");
    for (i = 0; (i < WAIT_COUNT) && !((j = *hpc1cintstat) & HPC1CINT_TX); i++)
	DELAY(100);
    if(!(j&HPC1CINT_TX)) {
        msg_printf(ERR,"DSP microcode failed to complete its tests\n");
    } else {
        dbgprintf("    Succeeded\n",j);

	*hpc1cintstat &= ~HPC1CINT_TX;
	
	diag_result = *handtx;
	if (diag_result & BAD_DUART0) {
	    errcount++;
	    msg_printf(ERR,"DSP read/write DUART0\n");
	}
	if (diag_result & BAD_DUART1) {
	    errcount++;
	    msg_printf(ERR,"DSP read/write DUART1\n");
	}
	if (diag_result & BAD_DUART2) {
	    errcount++;
	    msg_printf(ERR,"DSP read/write DUART2\n");
	}
	if (diag_result & BAD_DUART3) {
	    errcount++;
	    msg_printf(ERR,"DSP read/write DUART3\n");
	}
	if (diag_result & BAD_RTC) {
	    errcount++;
	    msg_printf(ERR,"DSP read/write RTC\n");
	}
	if (diag_result & BAD_HPC) {
	    errcount++;
	    msg_printf(ERR,"DSP read/write HPC registers\n");
	}
	if (diag_result & BAD_DSP_WRITE_MIPS) {
	    errcount++;
	    msg_printf(ERR,"DSP DMA write to the MIPS\n");
	}
	if (diag_result & BAD_DSP_READ_MIPS) {
	    errcount++;
	    msg_printf(ERR,"DSP DMA read from the MIPS\n");
	}
	if (diag_result & BAD_DMA_DATA) {
	    errcount++;
	    msg_printf(ERR,"DSP DMA data mismatch\n");
	}
	if (diag_result & BAD_SRAM_MAP) {
	    errcount++;
	    msg_printf(ERR,"DSP static RAM map\n");
	}
    }

    /* DMA write, from MIPS to DSP, started by MIPS */

    *hpc1cintstat &= ~HPC1CINT_DMA;
    *(volatile u_int *)PHYS_TO_K1(HPC1DMAWDCNT) = DSP_DMA_COUNT;
    *(volatile u_int *)PHYS_TO_K1(HPC1GIOADDL)=(u_int)pattern & 0xffff;
    *(volatile u_int *)PHYS_TO_K1(HPC1GIOADDM)=((u_int)pattern >> 16) & 0xfff;
    *(volatile u_int *)PHYS_TO_K1(HPC1PBUSADD)=0xc000+DSP_DMA_COUNT;
    *(volatile u_int *)PHYS_TO_K1(HPC1DMACTRL)=HPC1DMA_GO;

    for (i = 0; (i < WAIT_COUNT) && !(*hpc1cintstat & HPC1CINT_DMA); i++)
	DELAY(10);

    if (!(*hpc1cintstat & HPC1CINT_DMA)) {
	errcount++;
	msg_printf(ERR, "Timeout waiting for DMA from the MIPS to the DSP\n");
    }
    else {
	*hpc1cintstat &= ~HPC1CINT_DMA;

    	/* DMA read, from DSP to MIPS, started by MIPS */

        *(volatile u_int *)PHYS_TO_K1(HPC1DMAWDCNT) = DSP_DMA_COUNT;
        *(volatile u_int *)PHYS_TO_K1(HPC1GIOADDL) =
	    (u_int)read_back & 0xffff;
        *(volatile u_int *)PHYS_TO_K1(HPC1GIOADDM) =
	    ((u_int)read_back >> 16) & 0xfff;
        *(volatile u_int *)PHYS_TO_K1(HPC1PBUSADD) = 0xc000+DSP_DMA_COUNT;
        *(volatile u_int *)PHYS_TO_K1(HPC1DMACTRL) = HPC1DMA_GO | HPC1DMA_DIR;

        for (i = 0; (i < WAIT_COUNT) && !(*hpc1cintstat & HPC1CINT_DMA); i++)
	    DELAY(10);

	/* verify data */
        if (!(*hpc1cintstat & HPC1CINT_DMA)) {
	    errcount++;
	    msg_printf(ERR, "Timeout waiting for DMA from the DSP to the MIPS\n");
        }
	else {
	    int	data_error = 0;

	    *hpc1cintstat &= ~HPC1CINT_DMA;

	    for (i = 0; i < DSP_DMA_COUNT; i++) {
		if ((read_back[i] & 0xffffff) != (pattern[i] & 0xffffff))
		    data_error++;
	    }	
	    errcount += data_error;
	    if (data_error) {
		msg_printf(ERR, "MIPS DMA data mismatch\n");
		for(i=0; i<DSP_DMA_COUNT; i++) {
		    dbgprintf("%x ",read_back[i]);
		    if((i%4)==3)dbgprintf("\n");
		}
	    }
	}
    }

    /* check handshake interrupt mechanism by polling */

    for (i = 0; i < 2; i++) {
	for (j = 0; j < 16; j++) {

    	    u_short handshake_expected;
    	    u_short handshake_read;
    	    u_short handshake_wrote;
	    int k;

	    handshake_wrote = i ? pattern[j] : ~pattern[j];
	    handshake_expected = ~handshake_wrote;

	    /* write HANDRX handshake register */
	    *handrx = handshake_wrote;

	    /* wait for HANDRX acknowledge interrupt */
	    for (k = 0; (k < WAIT_COUNT) && !(*hpc1cintstat & HPC1CINT_RX); k++)
	    	DELAY(10);

	    if (!(*hpc1cintstat & HPC1CINT_RX)) {
	        errcount++;
	        msg_printf(ERR,
		    "Timeout waiting for HANDRX acknowledge interrupt\n");
	        goto dsp_diag_done;
	    }
	    else
	    	*hpc1cintstat &= ~HPC1CINT_RX;

	    /* wait for HANDTX interrupt */
	    for (k = 0; (k < WAIT_COUNT) && !(*hpc1cintstat & HPC1CINT_TX); k++)
	    	DELAY(10);

	    if (!(*hpc1cintstat & HPC1CINT_TX)) {
	        errcount++;
	        msg_printf(ERR,"Timeout waiting for HANDTX interrupt\n");
	        goto dsp_diag_done;
	    }
	    else {
	    	*hpc1cintstat &= ~HPC1CINT_TX;

	    	handshake_read = *handtx;
	    	if (handshake_read != handshake_expected) {
	    	    errcount++;
	    	    msg_printf(ERR,
			"Handshake value, expected:0x%04x, read:0x%04x\n",
			handshake_expected, handshake_read);
	    	    goto dsp_diag_done;
	    	}
	    }
	}
    }

    /* request DSP to send HANDTX acknowledge interrupt count */
    *handrx = 0x0;

    /* wait for HANDRX acknowledge interrupt */
    for (i = 0; (i < WAIT_COUNT) && !(*hpc1cintstat & HPC1CINT_RX); i++)
	DELAY(10);

    if (!(*hpc1cintstat & HPC1CINT_RX)) {
	errcount++;
	msg_printf(ERR,"Timeout waiting for HANDRX acknowledge interrupt\n");
	goto dsp_diag_done;
    }
    else
	*hpc1cintstat &= ~HPC1CINT_RX;

    /* wait for HANDTX interrupt */
    for (i = 0; (i < WAIT_COUNT) && !(*hpc1cintstat & HPC1CINT_TX); i++)
	DELAY(10);

    if (!(*hpc1cintstat & HPC1CINT_TX)) {
	errcount++;
	msg_printf(ERR,"Timeout waiting for HANDTX interrupt\n");
	goto dsp_diag_done;
    }
    else {
	*hpc1cintstat &= ~HPC1CINT_TX;

	/*
	   the DSP should get 33 HANDTX acknowledge interrupts, one after
	   it sends the diagnostics result, one after each handshake
	   test
	*/
        i = *handtx;
	if (i != 33) {
	    errcount++;
	    msg_printf(ERR,
		"DSP HANDTX acknowledge interrupt, expected:33, got:%d\n",i);
	}
    }

dsp_diag_done:
    *(volatile u_int *)PHYS_TO_K1(HPC1MISCSR) = HPC1MISC_RESET;

    if (errcount)
	sum_error("DSP functionality");
    else
	okydoky();

    return (errcount);
}
