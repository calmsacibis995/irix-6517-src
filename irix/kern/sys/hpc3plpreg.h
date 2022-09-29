/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "sys/hpc3plp.h: $Revision: 1.9 $"

/*
 * hpc3plp.h - header for the IP22/IP26 parallel port driver
 */

#include "sys/pi1.h"

/* memory descriptor for parallel port transfers 
 */
typedef struct memd {
    union {
        __uint32_t buf;
        __uint32_t cbp;
    } memun_w0;
    unsigned eox:1,pad1:1,xie:1,pad0:15,bc:14;
    union {
        __uint32_t forw;
        __uint32_t nbdp;
    } memun_w2;
    unsigned pad;
} memd_t;

#define memd_bc bc
#define memd_xie xie
#define memd_eox eox
#define memd_cbp memun_w0.cbp
#define memd_buf memun_w0.buf
#define memd_nbdp memun_w2.nbdp
#define memd_forw memun_w2.forw

/* structure of parallel port registers
 * c6=pbus channel six
 * c7=pbus channel seven
 */
typedef struct plp_regs_s {
    volatile __uint32_t cbp;		/* c7:current buffer pointer */
    volatile __uint32_t nbdp;		/* c7:next buffer descriptor pointer */
    unsigned char pad0[0xff8];		/* unused */
    volatile unsigned int ctrl;		/* c7:control register(pbus ctrl) */
    unsigned char pad1[0x10ffc];	/* unused */
    volatile unsigned int fifo;		/* c7:control register(pbus ctrl) */
    unsigned char pad2[0x397fc];	/* unused */
    pi1_t extreg;			/* c6:external status/remote(pbus6) */
    unsigned char pad3[0x4b];		/* unused */
    volatile unsigned char write1;	/* c6:pbus write1 reg */
    unsigned char pad4[0x3589];		/* unused */
    volatile unsigned int cfgdma;	/* c7:pbus dma config reg */
    unsigned char pad5[0x7fc];		/* unused */
    volatile unsigned int cfgpio;	/* c6:bus pio config reg */
} plp_regs_t;


/* HPC3 DMA configuration flag for PLP */
#define BURST_COUNT      0x20              /* HPC3 allows preeption of DMA
                                              every 32 bytes
                                           */
#define NPLP    1       /* maximum number of parallel ports */

#define EDGE_LBL_NODE_IO "node/io/gio/hpc"      /* connection point under /hw*/
#define EDGE_LBL_PARALLEL "parallel"            /* /hw/parallel (alias edge) */
#define EDGE_LBL_HPCPLP "hpc3plp"
#define EDGE_LBL_PLP "plp"
#define EDGE_LBL_PLPBI "plpbi"
#define NEW(x, t)       (x = (t)kmem_zalloc(sizeof(*(x)), KM_SLEEP))

#define PLP_CENT_MODE 0
#define	PLP_BI_MODE 1

/* struct defining state of parallel port
 *      - rlock and wlock protect r/w loops so data on a single r/w
 *              call is contiguous
 *      - dlock protects the dma channel (regs, dl, bp)
 */

/*
 * The thread model for this driver goes something like this:
 *      - a (breakable) semaphore per r/w/d channel
 *      - a single sleep lock per unit struct
 *      - a single sleep lock and sv per memd_t list
 *      - a single semaphore for dma event synchronization
 */
typedef struct plp_s {
    unsigned char state;        /* software state of port */
#define PLP_ALIVE       0x1     /* parallel port exists */
#define PLP_WROPEN      0x2     /* open for writing */
#define PLP_RDOPEN      0x4     /* open for reading */
#define PLP_NBIO        0x8     /* set for non-blocking reads */
#define PLP_RICOH       0x10    /* set for ricoh mode */
#define PLP_INUSE       0x20    /* reference flag for unloads */
#define PLP_OCCUPIED    0x40    /* parallel port is busy */

    unsigned int control;       /* value to use for strobe/control register */
    sema_t dmasema;             /* dma channel sync */
    sema_t rlock, wlock, dlock; /* read, write, and dma locks */

    plp_regs_t *regs;           /* address of HPC registers for port */
    __uint32_t dl;              /* descriptor list given to HPC: PHYS addr */
    struct buf *bp;             /* current buf for write */
    memd_t *rbufs;              /* list of buffers of data that have
                                 * been read on this channel.
                                 * pointers are K0 addrs, buffers are K1 */
    mutex_t rbufsmutex;         /* lock for rbufs list */
    sv_t rbufssv;               /* list sync */
    int rto, wto;               /* read/write timeout in secs */
    time_t last_dma_time;       /* value of lbolt when last DMA started */
    toid_t timeoutid;           /* read/write timeout */
    mutex_t plpmutex;           /* sleep lock for port access */
    vertex_hdl_t cvhdl;         /* connect vertex */
    vertex_hdl_t pvhdl;         /* parallel vertex */
 
} plp_t;

typedef struct plpif_s {
    plp_t               *plp;           /* pointer to plp struct */
    vertex_hdl_t        pvhdl;          /* hwgraph vertex */
    uchar_t             opmode;         /* operation mode */
} plpif_t;

/* modify these parameters for best performance
 */
#if NBPP == 4096
#define NRPPP           2               /* number of read pages per port */
#define RDSHIFT         2               /* 4 descriptors per page */
#elif NBPP == 16384
#define NRPPP           1               /* number of read pages per port */
#define RDSHIFT         2               /* 4 descriptors per page */
#else
#error "Unknown page size"
#endif

#define NRP             (NPLP * NRPPP)  /* number of initial read pages */
#define NRDPP           (1<<RDSHIFT)    /* read descriptors per page */
#define NBPRD           (NBPP>>RDSHIFT) /* bytes per read descriptor */
#define RDMASK          ~(NBPRD-1)      /* mask for read descriptor bytes */

#define NRMD            (NRP*NRDPP)     /* number of receive memory descs */
#define NXMD            6               /* number of transmit memory descs */
#define NMD             (NRMD + NXMD)   /* number of memory descriptors */

#define PLP_RTO         (1 * HZ)       	/* default: 1 sec timeout on a read */
#define PLP_WTO         0       	/* default: never timeout on a write */

#define PLP_VEC_NORM    VECTOR_PLP
#define PLP_VEC_XTRA    -1

/* pbus dma control fifo parameters */
#define PBUS_CTRL_HIWATER	0x100
#define PBUS_CTRL_FIFOBEG	0
#define PBUS_CTRL_FIFOEND	0
#define PBUS_CTRL_INTERRUPT	0x1

/* Next three additions allow same parallel port to be used as 
 * either output or bidirectional port.
 */
#define PLPBI           0x10    /* Minor number for bidirectional only */
#define PLP_DEV2PLP(x) \
        ((plp_t *)(((plpif_t *)device_info_get(dev_to_vhdl(x)))->plp))
