#ident	"sys/hpcplpreg.h:  $Revision: 1.10 $"

/* 
 * hpcplpreg.h - header for the IP12/IP20 parallel port driver
 */

/* memory descriptor for parallel port transfers
 */
typedef struct memd {
    union {
	unsigned int word0;
	struct {
	    unsigned int pad0 :19;
	    unsigned int bc   :13;
	} mem_w0;
    } memun_w0;
    union {
	caddr_t buf;
	struct {
	    unsigned int eox  :1;
	    unsigned int pad1 :3;
	    unsigned int cbp  :28;
	} mem_w1;
    } memun_w1;
    union {
	struct memd *forw;
	struct {
	    unsigned int pad2  :4;
	    unsigned int nbdp  :28;
	} mem_w2;
    } memun_w2;
} memd_t;

#define memd_bc memun_w0.mem_w0.bc
#define memd_eox memun_w1.mem_w1.eox
#define memd_cbp memun_w1.mem_w1.cbp
#define memd_buf memun_w1.buf
#define memd_nbdp memun_w2.mem_w2.nbdp
#define memd_forw memun_w2.forw

/* structure of parallel port registers
 */
typedef struct plp_regs_s {
    volatile unsigned int bc;		/* byte count */
    volatile memd_t *cbp; 		/* current buffer pointer */
    volatile memd_t *nbdp; 		/* next buffer descriptor pointer */
    volatile unsigned int ctrl;		/* control register */
    unsigned int pntr;			/* unused */
    unsigned int fifo;			/* unused */
    unsigned char pad0[0x74];		/* unused */
    unsigned char pad1;			/* unused */
    volatile unsigned char extreg;	/* external status/remote */
    unsigned char pad2[2];		/* unused */
} plp_regs_t;

#define NPLP	1			/* maximum number of parallel ports */

/* modify these parameters for best performance
 */
#define NRPPP		2		/* number of read pages per port */
#define NRP		(NPLP * NRPPP)	/* number of initial read pages */
#define RDSHIFT		2		/* 4 descriptors per page */
#define NRDPP		(1<<RDSHIFT)	/* read descriptors per page */
#define NBPRD		(NBPP>>RDSHIFT)	/* bytes per read descriptor */
#define RDMASK		~(NBPRD-1)	/* mask for read descriptor bytes */

#define NRMD		(NRP*NRDPP)	/* number of receive memory descs */
#define NXMD		6		/* number of transmit memory descs */
#define NMD		(NRMD + NXMD)	/* number of memory descriptors */

#define PLP_RTO		(1 * HZ)	/* default: 1 sec timeout on a read */
#define PLP_WTO		0 		/* default: never timeout on a write */

#define PLP_VEC_NORM	VECTOR_PLP
#define PLP_VEC_XTRA	-1

#define PLP_CONTROL_DEF_STROBE 	0x7f702100
#define PLP_CONTROL_DEF		PLP_CONTROL_DEF_STROBE

#define PLP_EXT_PRT	2
#define PLP_EXT_RESET	1

/* Next three additions allow same parallel port to be used as 
 * either output or bidirectional port.
 */
#define PLPUNITMASK	0x0f	/* First four bits are unit number */
#define PLPBI           0x10    /* Minor number for bidirectional only */
#define plpunit(dev)	(geteminor(dev)&PLPUNITMASK)
