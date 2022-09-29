#ident	"IP12diags/hpc1/hpc1.h:  $Revision: 1.5 $"

/* register offsets for hpc1 ethernet, scsi, and parallel 
 * control ports
 */

/* Each register to test has a mask associated with it.  The
 * mask defines which bits in the register are to be tested.
 * the rest of the bits are to be ignored.
 */

/* ethernet register masks */
#define	ENET_XCBP_MASK		0xffffffff	/* w */
#define	ENET_XNBDP_MASK		0xffffffff	/* w */
#define	ENET_XCBDP_MASK		0xffffffff	/* w */

#define	ENET_RCBP_MASK		0xffffffff	/* w */
#define	ENET_RNBDP_MASK		0xffffffff	/* w */
#define	ENET_RCBDP_MASK		0xffffffff	/* w */

#define	ENET_RX_CNTL_MASK	0xffffffff
#define	ENET_RX_PIOCFG_MASK	0x1fff
#define	ENET_RX_DMACFG_MASK	0xffff
#define	ENET_TX_CNTL_MASK	0xffffffff

/* scsi registers */
#define	SCSI_CBP		0x8c		/* w */
#define	SCSI_NBDP		0x90		/* w */

/* scsi register masks */
#define	SCSI_CBP_MASK		0x8fffffff
#define	SCSI_NBDP_MASK		0x0fffffff
#define SCSI_CNTL_MASK		0xf
#define SCSI_PIOCFG_MASK	0xffff
#define SCSI_DMACFG_MASK	0x3ffff

/* parallel register masks */
#define PAR_BC_MASK		0x01ff
#define PAR_CBP_MASK		0x8fffffff
#define PAR_NBDP_MASK		0x0fffffff

/* Debug registers to get to the HPC fifos
 */

#define MAXHPC3	2		/* maximum number of HPC's */

#define FIFOLEN	48		/* all fifos are 16 deep */

/* offsets to pointer and fifo and control registers
 */
#define ENET_X_PNTR	0x18	/* offset to transmit pointer reg */
#define ENET_X_FIFO	0x2e000	/* offset to transmit fifo reg */
#define ENET_R_PNTR	0x58	/* offset to receive pointer reg */
#define ENET_R_FIFO	0x2c000	/* offset to receive fifo reg */
#define S_FIFO0		0x28000	/* offset to scsi fifo-0 reg */
#define S_CTRL0		0x11004	/* offset to scsi control-0 reg */
#define S_FIFO1		0x2a000	/* offset to scsi fifo-0 reg */
#define S_CTRL1		0x13004	/* offset to scsi control-0 reg */
#define P_FIFO		0x20000	/* offset to pbus fifo reg */

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

#define	HPC3_ETHER_RX_CBP_OFFSET	0x14000 
#define	HPC3_ETHER_RX_NBDP_OFFSET     	0x14004
#define	HPC3_ETHER_RX_CNTL_OFFSET     	0x15004
#define	HPC3_ETHER_RX_DMACFG_OFFSET    	0x15018
#define	HPC3_ETHER_RX_PIOCFG_OFFSET   	0x1501c
#define	HPC3_ETHER_TX_CBP_OFFSET        0x16000 
#define	HPC3_ETHER_TX_NBDP_OFFSET	0x16004
#define	HPC3_ETHER_TX_CNTL_OFFSET     	0x17004

#define HPC3_SCSI_BUFFER_PTR0_OFFSET	0x10000	  	
#define HPC3_SCSI_BUFFER_NBDP0_OFFSET	0x10004	  	
#define HPC3_SCSI_CNTL0_OFFSET		0x11004	  	
#define HPC3_SCSI_DMA_CFG0_OFFSET	0x11010	  	
#define HPC3_SCSI_PIO_CFG0_OFFSET	0x11014	  	

#define HPC3_SCSI_BUFFER_PTR1_OFFSET	0x12000	  	
#define HPC3_SCSI_BUFFER_NBDP1_OFFSET	0x12004	  	
#define HPC3_SCSI_CNTL1_OFFSET		0x13004	  	
#define HPC3_SCSI_DMA_CFG1_OFFSET	0x13010	  	
#define HPC3_SCSI_PIO_CFG1_OFFSET	0x13014	  	
#define HPC31_INTRCFG_OFFSET		0x58000

unsigned int *hpc_base(int);
int hpc_probe(int);
int hpc3_fifo(int);
int hpc3_register(int);
