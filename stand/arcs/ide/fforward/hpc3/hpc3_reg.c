#ident	"IP12diags/hpc3/hpc3_reg.c:  $Revision: 1.11 $"

/*
 * hpc3_reg.c - write/readback of HPC3 registers
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>
#include "hpctest.h"

int membinarybit(void *, __psunsigned_t);

/* these make things a little easier to look at
 */
#define REGTEST(a,c,s,m) \
		addr_range.ra_base = (__psint_t)PHYS_TO_K1(base + a);	\
		addr_range.ra_count = c;				\
		addr_range.ra_size = s;					\
		reg_sav = *((uint *)PHYS_TO_K1(base + a));		\
	        error_count += membinarybit(&addr_range,m);		\
		*((uint *) PHYS_TO_K1(base + a)) = reg_sav

/*
 * run both the hpc3 register and fifo tests
 */
int
hpc3(int argc, char** argv)
{
    int hpc = MAXHPC3; /* default - test all */
    if ( argc > 2 ) 
	printf("Usage: hpc3 [0|1|2|3|4]\n");
	
	
    if ( argc == 2 )
	   atob(argv[1], &hpc);
    else hpc = MAXHPC3; /* set to test all */
    return(hpc3_register(hpc) + hpc3_fifo(hpc));

}

/*
 * test the HPC3 peripheral controller chip registers 
 */
int
hpc3_register (int hpc_select)
{
    struct range addr_range;
    int	error_count = 0, last_error_count = 0;
    int	hpc;
    __psunsigned_t base;
    unsigned int reg_sav;

    msg_printf (VRB, "HPC3 registers test\n");

    for (hpc = 0; hpc < MAXHPC3; hpc++) {
	if ((hpc_select != MAXHPC3) && (hpc != hpc_select))
		continue;
	last_error_count = error_count;

	/* probe for hpc before running tests
	 */
	if (!hpc_probe(hpc)) {
	    continue;
	}
	base = (__psunsigned_t)hpc_base(hpc);

	/*
	 * ethernet registers
	 */
	REGTEST(HPC3_ETHER_RX_NBDP_OFFSET,1, 4, ENET_RNBDP_MASK);
	if (hpc != 1)  /* does work for hpc3 #1 */
	REGTEST(HPC3_ETHER_RX_DMACFG_OFFSET,1, 4, ENET_RX_DMACFG_MASK);
	REGTEST(HPC3_ETHER_RX_PIOCFG_OFFSET,1, 4, ENET_RX_PIOCFG_MASK);
	REGTEST(HPC3_ETHER_TX_NBDP_OFFSET,1, 4, ENET_XNBDP_MASK);
	/*
	 * SCSI registers
	 *
	 * The scsi interface zero is reset by the prom
	 */

	REGTEST(HPC3_SCSI_BUFFER_NBDP0_OFFSET,1,4,SCSI_NBDP_MASK);
	if (hpc != 1)
	REGTEST(HPC3_SCSI_DMA_CFG0_OFFSET,1,4,SCSI_DMACFG_MASK);
	REGTEST(HPC3_SCSI_PIO_CFG0_OFFSET,1,4,SCSI_PIOCFG_MASK);

	/*
	 * The scsi interface one is reset by the prom
	 */

	REGTEST(HPC3_SCSI_BUFFER_NBDP1_OFFSET,1,4,SCSI_NBDP_MASK);
	if (hpc != 1)
	REGTEST(HPC3_SCSI_DMA_CFG1_OFFSET,1,4,SCSI_DMACFG_MASK);
	REGTEST(HPC3_SCSI_PIO_CFG1_OFFSET,1,4,SCSI_PIOCFG_MASK);

	if (last_error_count != error_count)
	    msg_printf (VRB, "HPC3 number %d failed register test.\n", hpc);
	else
	    msg_printf (VRB, "HPC3 number %d passed register test.\n", hpc);

    }

    if (error_count)
	sum_error ("HPC3 Peripheral controller registers");
    else
	okydoky ();

    return (error_count);
}   /* hpc3_register */

/*
 * hpc_probe - return 1 if HPC number hpc exists, 0 otherwise
 */
#define HPC3_NOT_HPC1   PHYS_TO_K1(0x1FB02000)
int
hpc_probe(int hpc)
{
    int *testaddr;
    __psunsigned_t base;

    /* Double check the presence of the HPC3, since the GIO does 
       not always give a bus error if the chip is not there.  Writing to
       it again will give a reliable bus error if it's not actually there.
     */
    base = (__psunsigned_t)hpc_base(hpc);

    testaddr = (int *)PHYS_TO_K1(base);
    if (badaddr (testaddr, sizeof(int)) || badaddr (testaddr, sizeof(int)))
	return 0;

#ifdef IP22	/* only important on Indy */
    /*
     * Test to see if we really have an HPC3, not and HPC1.  This is only
     * necessary for the second HPC3.
     */
    if (hpc != 0)
	if (badaddr((int *) HPC3_NOT_HPC1, sizeof(int)))
		return 0;
#endif

    return 1;
}   /* hpc_probe */

unsigned int *
hpc_base(int hpc)
{
    switch (hpc) {
		case 0: return (unsigned int *)PHYS_TO_K1(HPC_0_ID_ADDR);
		case 1: return (unsigned int *)PHYS_TO_K1(HPC_1_ID_ADDR);
        	default: return 0;
	}
}
