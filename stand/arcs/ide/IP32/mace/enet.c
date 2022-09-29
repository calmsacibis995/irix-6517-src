/*
 * Power-on diagnostic test for PROM and IDE
 *
 * Perfoms the following tests:
 * 
 * o Test MACMODE, FIAD registers
 * 	Check stuck at one/zero and data path on latches
 * o Test DMA control and alias addresses
 *	Check stuck at one/zero and duplicate pio paths
 * o Test RAM based 64-bit registers
 *      Check custom data path folding muxes
 * o Test interrupt message generation
 *      Check interrupt data path
 * o Test MCLFIFO tri-state latch array
 *      Screening test for custom latch array, stuck and decode tests
 * o Test multicast select
 *	Run 128 patterns through multicast tri-state 64-1 mux
 * o Test probe for PHY device on motherboard
 *	Check that we can see a PHY device
 * o Test DMA: internal loopback at 10Mb/second
 *	Send one packet
 * o Test DMA: internal loopback at 100Mb/second
 *	Send one packet
 * o Test DMA: external PHY loopback at 10Mb/second
 *	Send one packet
 * o Test DMA: external PHY loopback at 100Mb/second
 *	Send one packet
 * o Test DMA: [DP83430] external twister loopback at 100Mb/second
 *	Send one packet through National Twister internal loopback port
 * o Stress test
 *	Continuous 100mb loopback DMA with PIO polling
 */

#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/mace.h>
#include <fault.h>
#include <setjmp.h>
#include <uif.h>
#include <libsc.h>
#include <libsk.h>
#include "mace_ec.h"
#include <sys/if_me.h>

#define ETHERNET        0xBF280000

/* TX fifo entry */
typedef union{
	unsigned long long	TXCmd;
	unsigned long long	TXStatus;
	unsigned long long	TXConcatPtr[4];
	unsigned long long	TXData[16];
	char             	buf[128];
} TXfifo;

/* MACE ethernet info */
static struct maceif {
	/* Hardware structures */
	volatile struct mac110  *mac;
	int			revision;

	/* Phy xcvr info */
	signed char             phyaddr;
	char                    phyrev;
	char                    phystatus;
	int                     phytype;

	/* Memory allocation */
	void			*tile;
	__uint64_t		*ptrp, *ptcb, *prba;
	TXfifo			*tx_fifo;

	/* Ring buffers */
	short 			tx_rptr, tx_wptr;
	short 			rx_rptr, rx_wptr;
} ether = {
	(struct mac110 *)ETHERNET,
};

static const long long txalign[16] = {0};
static const char txpkt[] = {
	0x08, 0x1C, 0x69, 0x06, 0xd1, 0x06,
	0x08, 0x1C, 0x69, 0x06, 0xd1, 0x06,
	0x90, 0x02,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC,
	0xDD, 0xEE, 0xFF,
};

static long long physaddr = 0x0000081C6906d106LL;

#define NUMPATTERNS     4
static unsigned spatterns[] = {
	0xFFFFFFFF, 0xAAAAAAAA, 0x55555555, 0x00000000,
};
static unsigned long long lpatterns[] = {
	0xFFFFFFFFFFFFFFFFLL, 0xAAAAAAAAAAAAAAAALL,
	0x5555555555555555LL, 0x0000000000000000LL,
};
#define MODEMASK	0x1FFFFFFE
#define DMACMASK	0x0000FFFF
#define TIMEMASK	0x0000003F
#define FIADMASK	0x000003FF

/*
 * DEC Poly routine
 */
static unsigned
gencrc(unsigned char *Bytes, int BytesLength)
{
	unsigned Crc = 0xFFFFFFFF;
	unsigned const Poly = 0x04c11db7;
#define CRCCHECK        0xC704DD7B
	unsigned Msb;
	unsigned char CurrentByte;
	int Bit;

	while (BytesLength-- > 0) {
		CurrentByte = *Bytes++;
		for (Bit = 0; Bit < 8; Bit++) {
			Msb = Crc >> 31;
			Crc <<= 1;
			if (Msb ^ (CurrentByte & 1)) {
				Crc ^= Poly;
				Crc |= 1;
			}
			CurrentByte >>= 1;
		}
	}

	return Crc;
}

static unsigned
in_cksum(unsigned short *ptr, int len)
{
	register unsigned int cksum = 0;

	/* Sum the bytes */
	while (len > 1) {
		cksum += *ptr++;
		cksum = (cksum >> 16) + (cksum & 0xffff);
		len -= 2;
	}

	/* Fold in last byte */
	if (len == 1) {
		cksum += *ptr & 0xFF00;
		cksum = (cksum >> 16) + (cksum & 0xffff);
	}

	return cksum;
}

static void
etherreset(volatile struct mac110 *ether)
{
	register unsigned value;

	/* Force reset */
	ether->mac_control = MAC_RESET;

	/* Reset interrupt request register */
	ether->irltv.sintr_request = 0xFFFF0000;

	/* Check reset condition */
	value = ether->mac_control;
	if ((value & (MODEMASK | MAC_RESET)) != MAC_RESET) {
		msg_printf(ERR, "ethernet reset failed (0x%08X)\n", value);
	}

	us_delay(100);
}

static int
regs(volatile struct mac110 *ether)
{
	register unsigned i, errcnt = 0, value;

	msg_printf(VRB, "REGISTER TEST STARTING\n");

	/* Test MACMODE */
	for (i = 0; i < NUMPATTERNS; ++i) {
		ether->mac_control = spatterns[i] & MODEMASK;
		value = ether->mac_control;
		if ((value & MODEMASK) != (spatterns[i] & MODEMASK)) {
			msg_printf(ERR, "regs mac failed (0x%08X)\n", value);
			errcnt++;
		}
	}

	/* Test FIAD */
	for (i = 0; i < NUMPATTERNS; ++i) {
		ether->phy_address = spatterns[i] & FIADMASK;
		if ((value = ether->phy_address) != (spatterns[i] & FIADMASK)) {
			msg_printf(ERR, "regs phy failed (0x%08X)\n", value);
			errcnt++;
		}
	}

	/* Test DMA control */
	for (i = 0; i < NUMPATTERNS; ++i) {
		ether->dma_control = spatterns[i] & DMACMASK;
		if ((value = ether->dma_control) != (spatterns[i] & DMACMASK)) {
			msg_printf(ERR, "regs dma failed (0x%08X)\n", value);
			errcnt++;
		}
	}

	/* Test delay TIMER */
	for (i = 0; i < NUMPATTERNS; ++i) {
		ether->timer = spatterns[i] & TIMEMASK;
		if ((value = ether->timer) != (spatterns[i] & TIMEMASK)) {
			msg_printf(ERR, "regs tmr failed (0x%08X)\n", value);
			errcnt++;
		}
	}

	msg_printf(VRB, "REGISTER TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

static int
ralias(volatile struct mac110 *ether)
{
	register int value, errcnt = 0;

	msg_printf(VRB, "RALIAS TEST STARTING\n");

	/* Clear RESET */
	ether->mac_control = 0;

	/* Check trasmit interrupt alias */
	ether->transmit_alias = 0xFFFF;
	if ((value = ether->dma_control) != ETHER_TX_INTR_ENABLE) {
		msg_printf(ERR, "dma_control 0x%04x != expected 0x%04x\n",
			value, ETHER_TX_INTR_ENABLE);
		++errcnt;
	}
	ether->transmit_alias = 0;
	if ((value = ether->dma_control) != 0) {
		msg_printf(ERR, "dma_control 0x%04x != expected 0\n", value);
		++errcnt;
	}

	/* Check receive interrupt alias */
	ether->receive_alias = 0xFFFF;
	if ((value = ether->dma_control) != (ETHER_RX_INTR_ENABLE|ETHER_RX_THRESHOLD)) {

		msg_printf(ERR, "dma_control 0x%04x != expected 0x%04x\n",
			value, ETHER_RX_INTR_ENABLE|ETHER_RX_THRESHOLD);
		++errcnt;
	}
	ether->receive_alias = 0;
	if ((value = ether->dma_control) != 0) {
		msg_printf(ERR, "dma_control 0x%04x != expected 0\n", value);
		++errcnt;
	}

	msg_printf(VRB, "RALIAS TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

static int
ramregs(volatile struct mac110 *ether)
{
	register unsigned r, i, errcnt = 0;
	register volatile long long *ebase = (long long *)ether;
	register long long value;

	msg_printf(VRB, "RAMBASED REGISTER TEST STARTING\n");

	/* Test all registers from offset A8 to F8 */
	for (r = 20; r < 32; ++r) {
	    for (i = 0; i < NUMPATTERNS; ++i) {
		ebase[r] = lpatterns[i];
		if ((value = ebase[r]) != lpatterns[i]) {
			msg_printf(ERR, "ramregs failed (0x%016llX)\n", value);
			errcnt++;
		}
	    }
	}

	msg_printf(VRB, "RAMBASED REGISTER TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

static int
userint(volatile struct mac110 *ether)
{
	return 0;
}

#define FIFOSZ		16
#define MCLMASK		0xFFFFF000
#define	MASKWORD(w)	((w) & MCLMASK)
#define PATTERN(i,r)	(spatterns[((i) + (r)) & 3] & MCLMASK)
#define INDEX(i)	((i) << 12)

#define PTR(n)		((n) & 0xf)

static int
fifo(volatile struct mac110 *ether, int rotate)
{
	register unsigned word, i, errcnt = 0;

	for (i = 0; i < FIFOSZ; ++i) {
		/* Check write pointer */
		if ((word = PTR(ether->rx_fifo_wptr)) != i) {
			msg_printf(ERR, "fifo wptr %d != expected %d\n",
				word, i);
			++errcnt;
		}

		/* Write value, alternate pattern */
		ether->rx_fifo = PATTERN(i, rotate);

		/* Check FIFO depth */
		if ((word = ether->rx_fifo_depth) != (i + 1)) {
			msg_printf(ERR, "fifo depth %d != expected %d\n",
				word, i + 1);
			++errcnt;
		}
	}
	for (i = 0; i < FIFOSZ; ++i) {
		/* Check write pointer */
		if ((word = PTR(ether->rx_fifo_rptr)) != i) {
			msg_printf(ERR, "fifo_rptr %d != expected %d\n",
				word, i);
			++errcnt;
		}

		/* Pop entries off the FIFO */
		if ((word = MASKWORD(ether->rx_fifo)) != PATTERN(i, rotate)) {
			msg_printf(ERR,
				"fifo entry %d: 0x%08X != expected 0x%08X\n",
				i, word, PATTERN(i, rotate));
			++errcnt;
		}

		/* Check FIFO depth */
		if ((word = ether->rx_fifo_depth) != ((FIFOSZ - 1) - i)) {
			msg_printf(ERR, "fifo depth 0x%X != expected 0x%X\n",
				word, i);
			++errcnt;
		}
	}

	return errcnt;
}

static int
mclfifo(volatile struct mac110 *ether)
{
	register long long value, value2;
	register unsigned word, i, errcnt = 0;

	msg_printf(VRB, "MCLFIFO TEST STARTING\n");

	/* Check POW of MACMODE register */
	if (((value = ether->mac_control) & 0x1FFFFFFF) != MAC_RESET) {
		msg_printf(ERR, "value of MACMODE register is incorrect\n");
		msg_printf(ERR, "\tvalue read = 0x%016llx\n", value);
		++errcnt;
	}

	/* Turn off reset */
	value2 = value & ~1;
	ether->mac_control = value2;

	/* Check MACMODE again */
	if ((value = ether->mac_control) != value2) {
		msg_printf(ERR, "ERROR: MACMODE reset disable test failed\n");
		msg_printf(ERR, "\tmacmode 0x%016llx != expected 0x%016llx\n",
			value, value2);
		++errcnt;
	}

	/* Check for MCL FIFO stuck at one/zero */
	msg_printf(VRB, "\tStuck at one/zero test starting\n");
	for (i = 0; i < NUMPATTERNS; ++i) {
		errcnt += fifo(ether, i);
	}

	/* Verify MCL FIFO order (address decoders) */
	msg_printf(VRB, "\tAddress decode test starting\n");
	for (i = 0; i < FIFOSZ; ++i) {
		/* Check write pointer */
		if ((word = PTR(ether->rx_fifo_wptr)) != i) {
			msg_printf(ERR, "fifo wptr %d != expected %d\n",
				word, i);
			++errcnt;
		}

		/* Write index value */
		ether->rx_fifo = INDEX(i);

		/* Check FIFO depth */
		if ((word = ether->rx_fifo_depth) != (i + 1)) {
			msg_printf(ERR, "fifo depth %d != expected %d\n",
				word, i + 1);
			++errcnt;
		}
	}
	for (i = 0; i < FIFOSZ; ++i) {
		/* Check write pointer */
		if ((word = PTR(ether->rx_fifo_rptr)) != i) {
			msg_printf(ERR, "fifo_rptr %d != expected %d\n",
				word, i);
			++errcnt;
		}

		/* Pop entries off the FIFO */
		if ((word = MASKWORD(ether->rx_fifo)) != INDEX(i)) {
			msg_printf(ERR, "fifo top %d != expected %d\n",
				word, INDEX(i));
			++errcnt;
		}

		/* Check FIFO depth */
		if ((word = ether->rx_fifo_depth) != ((FIFOSZ - 1) - i)) {
			msg_printf(ERR, "fifo depth %d != expected %d\n",
				word, i);
			++errcnt;
		}
	}

	msg_printf(VRB, "MCLFIFO TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return errcnt;
}

static int
multihash(volatile struct mac110 *ether)
{
	register unsigned i, status, errcnt = 0;

	msg_printf(VRB, "MULTICAST FILTER TEST STARTING\n");

	/* Turn off reset */
	ether->mac_control = 0;

	/* Test all 64 bit positions */
	for (i = 0; i < 64; ++i) {
		ether->dma_control = i << 9;
		ether->mlaf = 0x1LL << i;
		status = ether->interrupt_status;
		if (!(status & (1 << 30))) {
			msg_printf(ERR, "hash bit %d stuck at zero (%x)\n",
				i, status);
			++errcnt;
		}
		ether->mlaf = ~(0x1LL << i);
		status = ether->interrupt_status;
		if (status & (1 << 30)) {
			msg_printf(ERR, "hash bit %d stuck at one (%x)\n",
				i, status);
			++errcnt;
		}
	}

	msg_printf(VRB, "MULTICAST FILTER TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

/*
 * Read a phy register over the MDIO bus
 */
static int
mace_ether_mdio_rd(register struct maceif *mif, int fireg)
{
	volatile struct mac110 *mac = mif->mac;
	volatile int rval;

	mac->phy_address = (mif->phyaddr << 5) | (fireg & 0x1f);
	mac->phy_read_start = fireg;
	us_delay(100);
	while ((rval = mac->phy_dataio) & MDIO_BUSY) {
		us_delay(100);
	}

	return rval;
}

/*
 * Write a phy register over the MDIO bus
 */
static int
mace_ether_mdio_wr(register struct maceif *mif, int fireg, int val)
{
	volatile struct mac110 *mac = mif->mac;
	volatile int rval;

	mac->phy_address = (mif->phyaddr << 5) | (fireg & 0x1f);
	mac->phy_dataio = val;
	us_delay(100);
	while ((rval = mac->phy_dataio) & MDIO_BUSY) {
		us_delay(100);
	}

	return rval;
}

/*
 * Modify phy register using given mask and value
 */
static void
mace_ether_mdio_rmw(register struct maceif *mif, int fireg, int mask, int val)
{
	register int rval;

	rval = mace_ether_mdio_rd(mif, fireg);
	rval = (rval & ~mask) | (val & mask);
	mace_ether_mdio_wr(mif, fireg, rval);
}

static char *
mace_phystr(int phytype)
{
	register char *pname = "";

	switch(phytype) {
	    case PHY_QS6612X:
		pname = "QS6612";
		break;
	    case PHY_ICS1889:
		pname = "ICS1889";
		break;
	    case PHY_ICS1890:
		pname = "ICS1890";
		break;
	    case PHY_DP83840:
		pname = "DP83840";
		break;
	}

	return pname;
}

/*
 * Probe the management interface for PHYs
 */
static int
mace_ether_mdio_probe(register struct maceif *mif)
{
	register int i, val, p2, p3;

	/* already found the phy? */
	if ((mif->phyaddr >= 0) && (mif->phyaddr < 32))
		return mif->phytype;

	/* probe all 32 slots for a known phy */
	for (i = 0; i < 32; ++i) {
		mif->phyaddr = (char)i;
		p2 = mace_ether_mdio_rd(mif, 2);
		p3 = mace_ether_mdio_rd(mif, 3);
		val = (p2 << 12) | (p3 >> 4);
		switch (val) {
		    case PHY_QS6612X:
		    case PHY_ICS1889:
		    case PHY_ICS1890:
		    case PHY_DP83840:
			mif->phyrev = p3 & 0xf;
			mif->phytype = val;
			return val;
		}
	}
	mif->phyaddr = -1;

	return -1;
}

static void
phyreset(register struct maceif *mif)
{
	/* Take interface out of reset mode */
	mif->mac->mac_control = 0;

	/* Send PHY reset message */
	(void) mace_ether_mdio_wr(mif, 0, PHY_PCTL_RESET);

	msg_printf(DBG, "TRANSCEIVER SOFTWARE RESET MSG SENT\n");
}

static int
phyprobe(register struct maceif *mif)
{
	register unsigned errcnt = 0;

	msg_printf(VRB, "TRANSCEIVER PROBE CHECK STARTING\n");

	/* Turn off reset */
	mif->mac->mac_control = 0;

	/* Search for device */
	mif->phyaddr = -1;
	if (mace_ether_mdio_probe(mif) == -1) {
		msg_printf(ERR, "transceiver probe failed\n");
		errcnt++;
	} else {
		msg_printf(VRB, "\tFound: '%s' addr=%d rev=%d mid=0x%x\n",
			mace_phystr(mif->phytype), mif->phyaddr,
			mif->phyrev, mif->phytype);
	}

	msg_printf(VRB, "TRANSCEIVER PROBE CHECK TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

#if 0
static __uint32_t
kvtophys(void *vaddr)
{
	return K1_TO_PHYS(vaddr);
}
#endif

extern void *getTile(void);
#define NRXBUFS 8
#define NTXBUFS 64

#ifdef NOTYET
/*
 * setup for DMA operation of specified type
 */
static void
dmasetup(register struct maceif *mif, int fast, int loop)
{
	register unsigned i, mode;

	/* Allocated tile for DMA buffers */
	if (mif->tile == 0) {
		if ((mif->tile = getTile()) == 0) {
			msg_printf(ERR, "can't allocate tile for dma area");
			return;
		}
		msg_printf(DBG, "\ttile=0x%X\n", mif->tile);
	}
	bzero(mif->tile, 65536);
	mif->ptrp = (__uint64_t *)mif->tile;
	mif->tx_fifo = (TXfifo *)mif->ptrp;
	mif->ptcb = mif->ptrp + 1024;
	mif->prba = mif->ptcb + 512;

	/* Clear internal reset in MACMODE register */
	mode = MAC_PHYSICAL | MAC_FULL_DUPLEX | MAC_DEFAULT_IPG;
	if (fast)
		mode |= MAC_100MBIT;
	if (loop)
		mode |= MAC_LOOPBACK;
	mif->mac->mac_control = mode;
	msg_printf(DBG, "\tmode=0x%X\n", mode);

	/* Initialize address filter */
	mif->mac->physaddr = physaddr;
	mif->mac->secphysaddr = NULL;
	mif->mac->mlaf = NULL;

	/* Setup transmit ring base */
	mif->mac->tx_ring_base = kvtophys(mif->ptrp);

	/* Setup backoff random number value */
	mif->mac->backoff = 7;

	/* Write receive page address to receive MCL fifo */
	mif->rx_rptr = mif->rx_wptr = 0;
	for (i = 0; i < NRXBUFS; ++i) {
		mif->mac->rx_fifo = kvtophys(&mif->prba[i]);
	}

	/* Turn on receive DMA and set interrupt threshold */
	mif->mac->dma_control =
		ETHER_RX_DMA_ENABLE |
		ETHER_RX_RUNT_ENABLE |
		ETHER_RX_INTR_ENABLE |
		(1 << ETHER_RX_OFFSET_SHIFT) |
		(NRXBUFS << ETHER_RX_THRESH_SHIFT) |
		ETHER_TX_DMA_ENABLE;

	/* Start transmitter at front of ring */
	mif->tx_rptr = mif->tx_wptr = 0;
}

static void
dmarxpush(register struct maceif *mif)
{
	register int cwptr;

	cwptr = mif->rx_wptr;
	mif->rx_wptr = (cwptr + 1) & (NRXBUFS - 1);

	mif->mac->rx_fifo = kvtophys(&mif->prba[cwptr]);
}

static __uint64_t *
dmarxpop(register struct maceif *mif)
{
	register int crptr;

	crptr = mif->rx_rptr;
	mif->rx_rptr = (crptr + 1) & (NRXBUFS - 1);

	return &mif->prba[crptr];
}

static void
dmatxpush(register struct maceif *mif, int psize)
{
	register volatile TXfifo *f = &mif->tx_fifo[mif->tx_wptr];
	register int size, remain, offset;

	/* Put transmit packet into the ring */
	if (psize > 112) {
		size = 40;
		remain = (psize - size);
		offset = sizeof (TXfifo) - size;
		f->TXCmd = TX_CMD_CONCAT_1 | (offset << 16) | (psize - 1);
		f->TXConcatPtr[0] = ((__uint64_t)(remain - 1) << 32) |
				      kvtophys(mif->ptcb);
		bcopy((void *)&txpkt[size], (char *)mif->ptcb, remain);
	} else {
		size = psize;
		offset = sizeof (TXfifo) - size;
		f->TXCmd = (offset << 16) | (psize - 1);
	}
	bcopy(txpkt, (char *)f->buf + offset, size);

	/* Set transmit ring write pointer to 1 */
	mif->tx_wptr = (mif->tx_wptr + 1) & (NTXBUFS - 1);
	wbflush();
	mif->mac->tx_ring_wptr = mif->tx_wptr;
}
#endif

static __uint32_t
getisr(volatile struct mac110 *ether)
{
        register union me_isr esr;

again:
        esr.lsr = ether->isr.lsr;
        if (esr.s.__ispd != 0) {
                goto again;
        }

        return esr.s.sr;
}

/*
 * Perform one DMA operation of specified type
 */
static int
packetdma(register struct maceif *mif, int psize, int fast, int loop)
{
	register __uint64_t *ptrp, *ptcb, *prba;
	register int errcnt = 0, i, ck, rck, size, remain, offset;
	unsigned mode, status, rcrc;
	volatile struct mac110 *ether = mif->mac;

	/* Allocated tile for DMA buffers */
	if (mif->tile == 0) {
		if ((mif->tile = getTile()) == 0) {
			msg_printf(ERR, "can't allocate tile for dma area");
			return (1);
		}
		msg_printf(DBG, "\ttile=0x%X\n", mif->tile);
	}
	bzero(mif->tile, 65536);
	ptrp = (__uint64_t *)mif->tile;
	ptcb = ptrp + 1024;
	prba = ptcb + 1024;

	/* Clear internal reset in MACMODE register */
	mode = MAC_PHYSICAL | MAC_FULL_DUPLEX | MAC_DEFAULT_IPG;
	if (fast)
		mode |= MAC_100MBIT;
	if (loop)
		mode |= MAC_LOOPBACK;
	ether->mac_control = mode;
	msg_printf(DBG, "\tmode=0x%X\n", mode);

	/* Initialize address filter */
	ether->physaddr = physaddr;
	ether->secphysaddr = NULL;
	ether->mlaf = NULL;

	/* Setup transmit ring base */
	ether->tx_ring_base = kvtophys(ptrp);

	/* Setup backoff random number value */
	ether->backoff = 7;

	/* Put transmit packet into the ring */
	if (psize > 112) {
		size = 43;
		remain = psize - size;
		offset = sizeof (TXfifo) - size;
		ptrp[0] = TX_CMD_CONCAT_1 | (offset << 16) | (psize - 1);
		ptrp[1] = ((__uint64_t)(remain - 1) << 32) | kvtophys(ptcb);
		if (ptcb[0] == 0) {
			bcopy((void *)&txpkt[size], (char *)ptcb,
				sizeof txpkt - size);
		}
	} else {
		size = psize;
		offset = sizeof (TXfifo) - size;
		ptrp[0] = (offset << 16) | (psize - 1);
	}
	bcopy(txpkt, (char *)ptrp + offset, size);

	/* Set transmit ring write pointer to 1 */
	wbflush();
	ether->tx_ring_wptr = 1;

	/* Write receive page address to receive MCL fifo */
	prba[0] = 0LL;
	ether->rx_fifo = kvtophys(prba);

	/* Turn on receive DMA and set interrupt threshold */
	msg_printf(DBG, "\tdma starting\n");
	ether->dma_control =
		ETHER_RX_DMA_ENABLE |
		ETHER_RX_RUNT_ENABLE |
		ETHER_RX_INTR_ENABLE |
		(1 << ETHER_RX_OFFSET_SHIFT) |
		(1 << ETHER_RX_THRESH_SHIFT) |
		ETHER_TX_DMA_ENABLE;

	/* Wait max 4ms for packet to complete */
	i = 0;
	while (((status = getisr(ether)) & 0xFF) == 0) {
		us_delay(4);
		if (++i > 1000) {
			msg_printf(ERR, "ERROR: dma complete timeout!");
			break;
		}
	}
	msg_printf(DBG, "\tdma done, status=0x%X\n", status);
	msg_printf(DBG, "\ttpsv = 0x%016llX\n", ptrp[0]);
	msg_printf(DBG, "\trpsv = 0x%016llX\n", prba[0]);
	
	/* Verify packet data */
	if (bcmp(((char *)&prba[1]) + 2, txpkt, psize) != 0) {
		msg_printf(ERR, "\treceive data doesn't match transmit\n");
		msg_printf(ERR, "\ttpsv = 0x%016llX\n", ptrp[0]);
		msg_printf(ERR, "\trpsv = 0x%016llX\n", prba[0]);
		for (i = 0; i < ((psize + 17) / 8); ++i) {
			msg_printf(ERR, "\t0x%016llX\n", prba[i]);
		}
		++errcnt;
	}

	/* Report CRIME memory errors */
	if ((status & INTR_TX_MEMORY_ERROR) != 0) {
		msg_printf(ERR, "\tCRIME memory read error reported\n");
		++errcnt;
	}

	/* Report RX fifo overflow errors */
	if ((status & INTR_RX_FIFO_OVERFLOW) != 0) {
		msg_printf(ERR, "\tRX FIFO overflow error reported\n");
		++errcnt;
	}

	/* Check interrupt status */
	if ((status & INTR_RX_DMA_REQ) == 0) {
		msg_printf(ERR, "\tRX threshold interrupt flag not set\n");
		msg_printf(VRB, "\tstatus = 0x%08X\n", status);
		msg_printf(VRB, "\ttpsv = 0x%016llX\n", ptrp[0]);
		msg_printf(VRB, "\trpsv = 0x%016llX\n", prba[0]);
		++errcnt;
	}

	/* Check receive sequence number */
	if ((status >> 25) != 1) {
		msg_printf(ERR, "\treceive sequence number is not a '1'\n");
		++errcnt;
	}
	if (((prba[0] >> 27) & 0x1F) != 0) {
		msg_printf(ERR, "\trsv sequence number is not a '0'\n");
		++errcnt;
	}

	/* Check receive MCL FIFO read-pointer alias */
	if (((status >> 8) & 0xFF) != 1) {
		msg_printf(ERR, "\treceive MCL read ptr alias is not a '1'\n");
		++errcnt;
	}

	/* Check transmit read-pointer alias */
	if (((status >> 16) & 0x1FF) != 1) {
		msg_printf(ERR, "\ttransmit read pointer alias is not a '1'\n");
		++errcnt;
	}

	/* Verify contents of LTV register */
	if (ether->irltv.last_transmit_vector != ptrp[0]) {
		msg_printf(ERR, "\tlast transmit vector register != ptrp[0]\n");
		msg_printf(ERR, "\t\tPTRP[0] 0x%016llX != LTV 0x%016llX\n",
			ptrp[0], ether->irltv.last_transmit_vector);
		++errcnt;
	}

	/* Verify length */
	if ((prba[0] & 0xFFFF) != (ptrp[0] & 0xFFFF)) {
		msg_printf(ERR, "\ttransmit length != receive length\n");
		++errcnt;
	}

	/* Verify checksum */
	size = prba[0] & 0xFFFF;
	ck = (prba[0] >> 32) & 0xFFFF;
	rck = in_cksum((unsigned short *)&prba[1] + 1, size);
	if (rck != ck) {
		msg_printf(ERR, "\tcksum 0x%04x != expected 0x%04x\n", ck, rck);
		++errcnt;
	}

	/* Verify CRC */
	rcrc = gencrc(((unsigned char *)&prba[1]) + 2, size);
	if (rcrc != CRCCHECK) {
		msg_printf(ERR, "\tcrc 0x%08X != expected 0xC704DD7B\n", rcrc);
		++errcnt;
	}

	return (errcnt);
}

static int
dma10i(register struct maceif *mif)
{
	register unsigned errcnt = 0;

	msg_printf(VRB, "INTERNAL LOOPBACK DMA 10Mb TEST STARTING\n");

	/* Turn off reset */
	mif->mac->mac_control = 0;

	/* Tell PHY we want to isolate */
	(void) mace_ether_mdio_wr(mif, 0, PHY_PCTL_ISOLATE);

	/* Go on DMA */
	errcnt = packetdma(mif, 1500, 0, 1);

	msg_printf(VRB, "INTERNAL LOOPBACK DMA 10Mb TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

static int
dma100i(register struct maceif *mif)
{
	register unsigned errcnt = 0;

	msg_printf(VRB, "INTERNAL LOOPBACK DMA 100Mb TEST STARTING\n");

	/* Turn off reset */
	mif->mac->mac_control = 0;

	/* Tell PHY we want to isolate */
	(void) mace_ether_mdio_wr(mif, 0, PHY_PCTL_ISOLATE);

	/* Go on DMA */
	errcnt = packetdma(mif, 1500, 1, 1);

	msg_printf(VRB, "INTERNAL LOOPBACK DMA 100Mb TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

static int
dma10e(register struct maceif *mif)
{
	register unsigned errcnt = 0;

	msg_printf(VRB, "EXTERNAL LOOPBACK DMA 10Mb TEST STARTING\n");

	/* Turn off reset */
	mif->mac->mac_control = 0;

	/* Tell PHY we want to loopback in 10Mb mode */
	(void) mace_ether_mdio_wr(mif, 0, PHY_PCTL_LOOPBACK|PHY_PCTL_DUPLEX);

	/* Tell the National PHY we want to loop through the 10Mb endec */
	if (mif->phytype == PHY_DP83840) {
		(void) mace_ether_mdio_rmw(mif, 24, 0xB00, 0x800);
	}

	/* Let the PHY settle */
	us_delay(5000);

	/* Go on DMA */
	errcnt = packetdma(mif, 1500, 0, 0);

	msg_printf(VRB, "EXTERNAL LOOPBACK DMA 10Mb TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

static int
dma100e(register struct maceif *mif)
{
	register unsigned errcnt = 0;

	msg_printf(VRB, "EXTERNAL LOOPBACK DMA 100Mb TEST STARTING\n");

	/* Turn off reset */
	mif->mac->mac_control = 0;

	/* Tell PHY we want to loopback in 100Mb mode */
	(void) mace_ether_mdio_wr(mif, 0,
			PHY_PCTL_LOOPBACK|PHY_PCTL_DUPLEX|PHY_PCTL_RATE);

	/* Let the PHY settle */
	us_delay(5000);

	/* Go on DMA */
	errcnt = packetdma(mif, 1500, 1, 0);

	msg_printf(VRB, "EXTERNAL LOOPBACK DMA 100Mb TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

static int
dma100x(register struct maceif *mif)
{
	register unsigned errcnt = 0;

	msg_printf(VRB, "EXTERNAL TWISTER-LOOP DMA 100Mb TEST STARTING\n");

	/* Turn off reset */
	mif->mac->mac_control = 0;

	/* Tell PHY we want to run full duplex at 100Mb */
	(void) mace_ether_mdio_wr(mif, 0, PHY_PCTL_DUPLEX|PHY_PCTL_RATE);

	/* Tell the National PHY we want to loop through the twister */
	(void) mace_ether_mdio_rmw(mif, 24, 0xB00, 0x100);

	/* Let the PHY settle */
	us_delay(5000);

	/* Go on DMA */
	errcnt = packetdma(mif, 1500, 1, 0);

	msg_printf(VRB, "EXTERNAL TWISTER-LOOP DMA 100Mb TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

static int
dma100si(register struct maceif *mif)
{
	register unsigned i, len, errcnt = 0;

	msg_printf(VRB, "INTERNAL LOOPBACK DMA 100Mb STRESS-TEST STARTING\n");

	/* Turn off reset */
	mif->mac->mac_control = 0;

	/* Generate packets of variable length and type */
	for (i = 0; i < 4000; ++i) {
		len = 60 + ((i ^ 1317) % 1440);
		errcnt = packetdma(mif, len, 1, 1);
		etherreset(mif->mac);
		if (errcnt > 0)
			break;
		busy(0);
	}

	msg_printf(VRB, "INTERNAL LOOPBACK DMA 100Mb STRESS-TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

static int
dma100se(register struct maceif *mif)
{
	register unsigned i, len, errcnt = 0;

	msg_printf(VRB, "EXTERNAL LOOPBACK DMA 100Mb STRESS-TEST STARTING\n");

	/* Turn off reset */
	mif->mac->mac_control = 0;

	/* Tell the National PHY we want to loop through the twister */
	if (mif->phytype == PHY_DP83840) {
		msg_printf(VRB, "\tLooping through external DP83223 Twister\n");
		(void) mace_ether_mdio_wr(mif, 0,
			PHY_PCTL_DUPLEX|PHY_PCTL_RATE);
		(void) mace_ether_mdio_rmw(mif, 24, 0xB00, 0x100);
	} else {
		/* Tell PHY we want to loopback in 100Mb mode */
		(void) mace_ether_mdio_wr(mif, 0,
			PHY_PCTL_LOOPBACK|PHY_PCTL_DUPLEX|PHY_PCTL_RATE);
	}

	/* Let the PHY settle */
	us_delay(5000);

	/* Generate packets of variable length and type */
	for (i = 0; i < 4000; ++i) {
		len = 60 + ((i ^ 1317) % 1440);
		errcnt = packetdma(mif, len, 1, 0);
		etherreset(mif->mac);
		if (errcnt > 0)
			break;
		busy(0);
	}

	msg_printf(VRB, "EXTERNAL LOOPBACK DMA 100Mb STRESS-TEST COMPLETED, ");
	if (errcnt) {
		msg_printf(VRB, "ERRORS %d\n", errcnt);
	} else {
		msg_printf(VRB, "PASSED\n");
	}

	return (errcnt);
}

bool_t
maceec()
{
	register int errtot = 0;

	msg_printf(VRB, "ETHERNET SUBSYSTEM TEST STARTING\n");

	/* Get MACE rev ID */
	ether.revision = ether.mac->mac_control >> MAC_REV_SHIFT;
	msg_printf(VRB, "\tMace%d.0\n", ether.revision + 1);

	/* Test MACMODE and FIAD registers */
	errtot += regs(ether.mac);
	etherreset(ether.mac);

	/* Test DMA control aliases */
	errtot += ralias(ether.mac);
	etherreset(ether.mac);

	/* Test RAM based registers */
	errtot += ramregs(ether.mac);
	etherreset(ether.mac);

	/* Test interrupt data path */
	errtot += userint(ether.mac);
	etherreset(ether.mac);

	/* Test MCLFIFO latch array */
	errtot += mclfifo(ether.mac);
	etherreset(ether.mac);

	/* Test multicast datap path logic */
	if (ether.revision > 0) {
		errtot += multihash(ether.mac);
		etherreset(ether.mac);
	}

	/* Test if we have a PHY on the motherboard */
	errtot += phyprobe(&ether);
	etherreset(ether.mac);

	/* National PHY errata, turn off link disconnect */
	if ((ether.phytype == PHY_DP83840) && (ether.phyrev == 0)) {
		(void) mace_ether_mdio_rmw(&ether, 23, 0x20, 0x20);
	}

	/* Internal 10Mb DMA test */
	if (ether.revision > 0) {
		errtot += dma10i(&ether);
		etherreset(ether.mac);
	}

	/* Internal 100Mb DMA test */
	errtot += dma100i(&ether);
	etherreset(ether.mac);

	/* External 10Mb DMA test */
	if (ether.revision > 0) {
		errtot += dma10e(&ether);
		etherreset(ether.mac);
	}

	/* External 100Mb DMA test */
	errtot += dma100e(&ether);
	etherreset(ether.mac);

	/* External National PHY specific 100Mb DMA test */
	if (ether.phytype == PHY_DP83840) {
		errtot += dma100x(&ether);
		etherreset(ether.mac);
	}

	/* Reset the transceiver (prevents chatter) */
	phyreset(&ether);

	msg_printf(VRB, "ETHERNET SUBSYSTEM TEST COMPLETED, ");
	if (errtot) {
		msg_printf(VRB, "ERRORS %d\n", errtot);
	} else {
		msg_printf(VRB, "PASSED\n");
		okydoky();
	}

	return errtot;
}

bool_t
maceecs()
{
	register int errtot = 0;

	msg_printf(VRB, "ETHERNET SUBSYSTEM STRESS-TEST STARTING\n");

	/* Get MACE rev ID */
	ether.revision = ether.mac->mac_control >> MAC_REV_SHIFT;
	msg_printf(VRB, "\tMace%d.0\n", ether.revision + 1);

	/* Test if we have a PHY on the motherboard */
	errtot += phyprobe(&ether);
	etherreset(ether.mac);

	/* National PHY errata, turn off link disconnect */
	if ((ether.phytype == PHY_DP83840) && (ether.phyrev == 0)) {
		(void) mace_ether_mdio_rmw(&ether, 23, 0x20, 0x20);
	}

	/* internal DMA loop at 100Mb/second */
	errtot += dma100si(&ether);
	etherreset(ether.mac);

	/* external DMA loop at 100Mb/second */
	errtot += dma100se(&ether);
	etherreset(ether.mac);

	/* Reset the transceiver (prevents chatter) */
	phyreset(&ether);

	msg_printf(VRB, "ETHERNET SUBSYSTEM STRESS-TEST COMPLETED, ");
	if (errtot) {
		msg_printf(VRB, "ERRORS %d\n", errtot);
	} else {
		msg_printf(VRB, "PASSED\n");
		okydoky();
	}

	return errtot;
}
