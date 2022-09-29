#ident	"IP12diags/hpc1/hpc1_reg.c:  $Revision: 1.3 $"

/*
 * hpc1_reg.c - write/readback of HPC1 registers
 */

#include "sys/param.h"
#include "sys/sbd.h"
#include "sys/cpu.h"
#include "uif.h"
#include "hpc1.h"
#include <libsk.h>

/* these make things a little easier to look at
 */
#define REGTEST(a,c,s,m) \
		addr_range.ra_base = PHYS_TO_K1(a); \
		addr_range.ra_count = c; \
		addr_range.ra_size = s; \
	        error_count += membinarybit (&addr_range,m);

#define SETREG(t,a,b) *(volatile unsigned t *)(a) = (b);
#define GETREG(t,a) *(volatile unsigned t *)(a)

unsigned int *hpc_base (int);
int hpc_probe(int);
int hpc1_fifo(void);
int hpc1_register(void);

int membinarybit(struct range *, u_int);

/*
 * run both the hpc1 register and fifo tests
 */
int
hpc1()
{
    return hpc1_register() + hpc1_fifo();
}

/*
 * test the HPC1 peripheral controller chip registers 
 */
int
hpc1_register (void)
{
    struct range addr_range;
    int	error_count = 0, last_error_count = 0;
    int	hpc;
    unsigned int base;
    unsigned int a,b,c;

    msg_printf (VRB, "HPC1 registers test\n");

    for (hpc = 0; hpc < MAXHPCS; hpc++) {
	last_error_count = error_count;

	/* probe for hpc before running tests
	 */
	if (!hpc_probe(hpc)) {
	    if (hpc == 0)
		msg_printf (ERR, "HPC 0 not responding\n");
	    continue;
	}
	base = (unsigned int)hpc_base(hpc);

	/*
	 * ethernet registers
	 */
	SETREG(int,base+ETHERNET_RESET,ENET_RESET_BIT);
	DELAY (25000);
	SETREG(int,base+ETHERNET_RESET,0);
	REGTEST(base+ETHERNET_CXBP,1, 4, ENET_CXBP_MASK);
	REGTEST(base+ETHERNET_NXBDP,1, 4, ENET_NXBDP_MASK);
	REGTEST(base+ETHERNET_XBC,1, 4, ENET_XBC_MASK);
	REGTEST(base+ETHERNET_CXBDP,1, 4, ENET_CXBDP_MASK);
	REGTEST(base+ETHERNET_CPFXBDP,1, 4, ENET_CPFXBDP_MASK);
	REGTEST(base+ETHERNET_PPFXBDP,1, 4, ENET_PPFXBDP_MASK);
	REGTEST(base+ETHERNET_RBC,1, 4, ENET_RBC_MASK);
	REGTEST(base+ETHERNET_CRBP,1, 4, ENET_CRBP_MASK);
	REGTEST(base+ETHERNET_NRBDP,1, 4, ENET_NRBDP_MASK);
	REGTEST(base+ETHERNET_CRBDP,1, 4, ENET_CRBDP_MASK);
	SETREG(int,base+ETHERNET_TIMER,ENET_TIMER_START);
	a = GETREG(int,base+ETHERNET_TIMER);
	DELAY(10);
	b = GETREG(int,base+ETHERNET_TIMER);
	DELAY(10);
	c = GETREG(int,base+ETHERNET_TIMER);
	if ((c >= b) || (b >= a) || (a >= ENET_TIMER_START)) {
	    msg_printf (ERR, "HPC1 %d ethernet timer register failed.\n", hpc);
	}
	SETREG(int,base+ETHERNET_RESET,ENET_RESET_BIT);
	DELAY (25000);
	SETREG(int,base+ETHERNET_RESET,0);

	/*
	 * SCSI registers
	 *
	 * The scsi interface is reset by the prom
	 */
	REGTEST(base+SCSI_BC,1,4,SCSI_BC_MASK);
	REGTEST(base+SCSI_CBP,1,4,SCSI_CBP_MASK);
	REGTEST(base+SCSI_NBDP,1,4,SCSI_NBDP_MASK);

	/*
	 * Parallel port interface registers
	 */
	SETREG(int,base+PARALLEL_CTRL,PAR_RESET_BIT);
	DELAY (25000);
	SETREG(int,base+PARALLEL_CTRL,0);
	REGTEST(base+PARALLEL_BC,1,4,PAR_BC_MASK);
	REGTEST(base+PARALLEL_CBP,1,4,PAR_CBP_MASK);
	REGTEST(base+PARALLEL_NBDP,1,4,PAR_NBDP_MASK);
	SETREG(int,base+PARALLEL_CTRL,PAR_RESET_BIT);
	DELAY (25000);
	SETREG(int,base+PARALLEL_CTRL,0);

	if (last_error_count != error_count)
	    msg_printf (VRB, "HPC1 number %d failed registers test.\n", hpc);
    }

    if (error_count)
	sum_error ("HPC1 Peripheral controller registers");
    else
	okydoky ();

    return (error_count);
}   /* hpc1_register */

int
hpc_probe(int adap)
{
	if ( adap == 0 )
		return 1;
	return 0;
}
unsigned int *
hpc_base(int adap)
{
	if ( adap == 0 )
		return (unsigned int *)PHYS_TO_K1(HPC_0_ID_ADDR);
	
	return 0;
}
