#ident	"IP12diags/hpc1/hpc1.h:  $Revision: 1.1 $"

/* register offsets for hpc1 ethernet, scsi, and parallel 
 * control ports
 */

/* Each register to test has a mask associated with it.  The
 * mask defines which bits in the register are to be tested.
 * the rest of the bits are to be ignored.
 */

/* ethernet registers */
#define	ETHERNET_CXBP		0x0c		/* w */
#define	ETHERNET_NXBDP		0x10		/* w */
#define	ETHERNET_XBC		0x14		/* w */
#define	ETHERNET_CXBDP		0x20		/* w */
#define	ETHERNET_CPFXBDP	0x24		/* w */
#define	ETHERNET_PPFXBDP	0x28		/* w */
#define	ETHERNET_TIMER		0x2c		/* w */
#define	ETHERNET_RBC		0x48		/* w */
#define	ETHERNET_CRBP		0x4c		/* w */
#define	ETHERNET_NRBDP		0x50		/* w */
#define	ETHERNET_CRBDP		0x54		/* w */
#define	ETHERNET_RESET		0x3c		/* w */

/* ethernet external seeq registers */
#define SEEQ_ENET_RX_ADDR	0x5b
#define SEEQ_ENET_TX_ADDR	0x5f

/* ethernet register masks */
#define	ENET_CXBP_MASK		0x8fffffff	/* w */
#define	ENET_NXBDP_MASK		0x0fffffff	/* w */
#define	ENET_XBC_MASK		0x80009fff	/* w */
#define	ENET_CXBDP_MASK		0x0fffffff	/* w */
#define	ENET_CPFXBDP_MASK	0x0fffffff	/* w */
#define	ENET_PPFXBDP_MASK	0x0fffffff	/* w */

#define	ENET_RBC_MASK		0x000001ff	/* w */
#define	ENET_CRBP_MASK		0x8fffffff	/* w */
#define	ENET_NRBDP_MASK		0x0fffffff	/* w */
#define	ENET_CRBDP_MASK		0x0fffffff	/* w */

#define	ENET_RESET_BIT		0x00000001
#define	ENET_TIMER_START	0x00ffffff

/* scsi registers */
#define	SCSI_BC			0x88		/* w */
#define	SCSI_CBP		0x8c		/* w */
#define	SCSI_NBDP		0x90		/* w */

/* scsi register masks */
#define	SCSI_BC_MASK		0x01ff
#define	SCSI_CBP_MASK		0x8fffffff
#define	SCSI_NBDP_MASK		0x0fffffff

/* parallel registers */
#define PARALLEL_BC		0xa8		/* w */
#define PARALLEL_CBP		0xac		/* w */
#define PARALLEL_NBDP		0xb0		/* w */
#define PARALLEL_CTRL		0xb4		/* w */

/* parallel register masks */
#define PAR_BC_MASK		0x01ff
#define PAR_CBP_MASK		0x8fffffff
#define PAR_NBDP_MASK		0x0fffffff
#define PAR_RESET_BIT		0x01

/* Debug registers to get to the HPC fifos
 */

#define MAXHPCS	4		/* maximum number of HPC's */

#define FIFOLEN	16		/* all fifos are 16 deep */

/* offsets to pointer and fifo and control registers
 */
#define ENET_X_PNTR	0x18	/* offset to transmit pointer reg */
#define ENET_X_FIFO	0x1C	/* offset to transmit fifo reg */
#define ENET_R_PNTR	0x58	/* offset to receive pointer reg */
#define ENET_R_FIFO	0x5C	/* offset to receive fifo reg */
#define S_PNTR		0x98	/* offset to scsi pointer reg */
#define S_FIFO		0x9C	/* offset to scsi fifo reg */
#define S_CTRL		0x94	/* offset to scsi control reg */
#define P_PNTR		0xB8	/* offset to parallel pointer reg */
#define P_FIFO		0xBC	/* offset to parallel fifo reg */
#define P_CTRL		0xB4	/* offset to parallel control reg */

/* control field info
 */
#define X_FLAG_SHIFT	24	/* shift to flag field */
#define X_TWP_SHIFT	2	/* shift to xmit write pointer field */
#define X_TRP_SHIFT	10	/* shift to xmit read pointer field */

#define R_RDIR_MP	0x8000	/* RDIR_MP bit */
#define R_RWP_SHIFT	10	/* shift to recv write pointer field */
#define R_RRP_SHIFT	2	/* shift to recv read pointer field */

#define FLAG_SHIFT	28	/* shift to scsi/parallel flag field */
#define MEMP_SHIFT	2	/* shift to scsi/parallel memory ptr field */
#define CHANP_SHIFT	10	/* shift to scsi/parallel channel ptr field */

