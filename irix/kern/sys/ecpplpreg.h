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

#ident "sys/ecpplpreg.h: $Revision: 1.12 $"

/*
 * ecpplpreg.h - header for the IP30/IP27 parallel port driver
 */

#ifndef _ECPPLPREG_H_
#define _ECPPLPREG_H_

/* IOC3 Base Address */
#define IOC3_BASE               0x1f500000
/* PCI Conf. space base addr */
#define PCI_CONF_ADDR           0x1f4c0000

/* IOC3 Misc Registers */
/* These are in config space */
#define PCI_ADDR_OFFSET         0x10    /* IOC3 Base Address Register */
#define PCI_SCR_OFFSET          0x04    /* IOC3 Status/Command Register */

#define PCI_ADDR                (PCI_CONF_ADDR+PCI_ADDR_OFFSET)
#define PCI_SCR                 (PCI_CONF_ADDR+PCI_SCR_OFFSET)

#define SIO_CR                  (IOC3_BASE+IOC3_SIO_CR)

/* Reset bit in SIO_CR */
#define SIO_RESET               0x1

/* IOC3 PP DMA Register addresses */

#define PPCR_COUNT_MASK         0x1ffff /* Bits 0:16 contain the count in PPCR_A and PPCR_B*/

/* Bit positions in PPCR */
#define PPCR_EN_A               0x1             /* Read only copy of PPCR_A<EN> */
#define PPCR_LAST_A             (0x1 << 1)      /* Read only copy of PPCR_A<LAST> */
#define PPCR_BUSY_A             (0x1 << 2)      /* DMA engine transferring context A */
#define PPCR_EN_B               (0x1 << 3)      /* Read only copy of PPCR_B<EN> */
#define PPCR_LAST_B             (0x1 << 4)      /* Read only copy of PPCR_B<LAST> */
#define PPCR_BUSY_B             (0x1 << 5)      /* DMA engine transferring context B */
#define PPCR_DIR_FROM_PP        (0x1 << 29)     /* Direction of DMA. 0 -> write to PP,
                                                 * 1 -> read from PP */
#define PPCR_DMA_RST            (0x1 << 30)     /* reset DMA state m/c in IOC3 */
#define PPCR_DMA_EN             (0x1 << 31)     /* Enable DMA engine */

/* Bit positions in PPCR_<A:B> */
#define PPCR_EN                 (0x1 << 30)     /* Indicates that context is ready */
#define PPCR_LAST               (0x1 << 31)     /* Indicates that TC is to be asserted after
                                                 * this context */

/* Bit positions in DCR */
#define DCR_NSTB                0x1             /* Strobe */
#define DCR_NAFD                (0x1 << 1)      /* Auto feed */
#define DCR_NINIT               (0x1 << 2)      /* Init */
#define DCR_NSLCTIN             (0x1 << 3)      /* nSelectin */
#define DCR_DIR                 (0x1 << 5)      /* Direction: 1 -> Read from PP,
                                                 * 0-> write to PP */

/* Bit positions in ECR */
#define ECR_EMPTY               0x1             /* Fifo Empty bit */
#define ECR_FULL                (0x1 << 1)      /* FIFO full bit */
#define ECR_SVCINTB             (0x1 << 2)      /* Enable service interrupt */
#define ECR_DMA_EN              (0x1 << 3)      /* DMA enable */
#define ECR_ENFLTINTB           (0x1 << 4)      /* Enable nFault Interrupt */
#define ECR_MODE_MASK           0xe0            /* bits 5,6 & 7 of ECR */
#define ECR_MODE1               0x20            /* 001 in bits 7-5 */
#define ECR_MODE3               0x60            /* 011 in bits 7-5 */

/* Bit positions in DSR */
#define DSR_INTR		(0x1 << 2)	/* printer intr */
#define DSR_NFAULT              (0x1 << 3)      /* nFault */
#define DSR_SELECT              (0x1 << 4)      /* Select */
#define DSR_PERROR              (0x1 << 5)      /* PE */
#define DSR_NACK                (0x1 << 6)      /* NACK */
#define DSR_BUSY                (0x1 << 7)      /* Busy */

#define EXT_REQ_ECP             0x10            /* Extensibility value. Req. ECP Mode */
/* Bit positions */

#define ECR_TEST_PATTERN        0x34
#define ECR_CFG_MODE            0xf4

#define ECR_PIO_MODE            0x00  /* Parallel port non-FIFO mode */
#define ECR_COMP_MODE           0x40  /* Parallel port FIFO mode */
#define ECR_ECP_MODE            0x60  /* ECP mode */

#ifdef _KERNEL

typedef struct memd {
    union {
        caddr_t buf;                            /* buffer pointer */
        unsigned long long cbp;                 /* current byte pointer */
    } memun_w0;
    unsigned int bc;                            /* byte count */
    union {
        struct memd *forw;                      /* forward descriptor pointer */
        unsigned long long nbdp;                /* next buffer descriptor ptr */
    } memun_w2;
} memd_t;

#define memd_bc                 bc
#define memd_cbp                memun_w0.cbp
#define memd_buf                memun_w0.buf
#define memd_nbdp               memun_w2.nbdp
#define memd_forw               memun_w2.forw

/* DMA context */
typedef struct context {
    unsigned cbp_h;             /* Upper 32 bits of address */
    unsigned cbp_l;             /* Lower 32 bits of address */
    unsigned int bc;            /* byte count */
} context_t;

typedef struct ecp_s {
	vertex_hdl_t plp_conn_vhdl;	/* vertex handle of connection point */
	vertex_hdl_t vhdl;		/* my vertex handle */
        unsigned char state;            /* software state of port */
        unsigned int mode;              /* mode port is in - ECP, EPP, PPF */
        unsigned int phase;             /* phase port is in */
        ioc3_mem_t *ecpregs;            /* address of parallel port registers */
        sema_t dmasema;			/* dma channel sync */
        sema_t wlock, rlock, dlock;	/* write, read and dma locks */
	mutex_t rbufsmutex;		/* lock for rbufs list */
	sv_t rbufssv;			/* list sync */
        memd_t *rbufs;                  /* list of data buffers that have
                                         * been read on this channel.
                                         * Pointers are K0 addrs, buffers are K1
                                         */
        __uint64_t rdl,wdl;             /* descriptor list: PHYS addr */
        __uint64_t cwdp;                /* current write desc. pointer */
        int rto, wto;                   /* read  & write timeout in secs */
        int rtimeoutid;                 /* read timeoutid */
        int wtimeoutid;                 /* write timeoutid */
        struct buf *bp;                 /* buf pointer */
        context_t contexta, contextb;   /* DMA contexts */
#define CONTXT_A_INTR	1
#define CONTXT_B_INTR	2
	int intr_next;			/* next expected interrupt */
	int final;			/* final context started */
	mutex_t ecpmutex;		/* sleep lock for ecp access */
	mutex_t tmoutlock;		/* timeout lock */
	sv_t sv_tmout;			/* timeout sync var */
	int plpinitfail;		/* initialization failed */
	int ecp_mode_supp;		/* ECP mode supported */
	int timed_out;			/* timed out */
	int flags;			/* flags */
	void *ioc3_soft;		/* back pointer to ioc3 soft area */
	int ctlr_num;			/* for /hw/parallel/plp%d */
        } ecp_t;

#define ECPPLP_HWG_OK	1		/* hwgraph alias created for this device */

#define NPLP    32       /* maximum number of parallel ports */

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
#define NXMD            (NPLP * 6)      /* number of transmit memory descs */
#define NMD             (NRMD + NXMD)   /* number of memory descriptors */

/* size of space for memory descriptor nodes, add one in case one of the
 * descriptors crosses a page boundary and needs to be thrown away.
 */
#define MDMEMSIZE               ((NMD + 1) * sizeof(memd_t))
#define plpunit(dev)    (minor(dev)&PLPUNITMASK)
#define PLPUNITMASK     0x1f    /* First five bits are unit number */

/* Parallel Port Phases */

#define COMP_FWD_IDLE_PHASE	1	/* Compatibility fwd. idle mode */
#define COMP_FWD_BUSY_PHASE	2	/* Compatibility fwd. busy mode */
#define NEGOTIATION_PHASE	3	/* Negotiation to determine ECP compliance */
#define SETUP_PHASE		4	/* Intermediate bet. negotiation and fwd_idle */
#define ECP_FWD_IDLE_PHASE	5	/* ECP fwd idle mode */
#define ECP_FWD_BUSY_PHASE	6	/* ECP fwd busy mode */
#define ECP_FWD_TO_REV_PHASE	7	/* ECP fwd to rev mode */
#define ECP_REV_TO_FWD_PHASE	8	/* ECP rev to fwd mode */
#define ECP_REV_IDLE_PHASE	9	/* ECP rev idle mode */
#define ECP_REV_BUSY_PHASE	10	/* ECP rev busy mode */
#define TERMINATION_PHASE	11	/* Bring port back to known COMP_FWD_IDLE state */

#define PLP_RTO              (1 * HZ)
/* Never time out on a write */
#ifdef ECP_DEBUG
#define PLP_WTO		     (0  * HZ)
#else
#define PLP_WTO              (0 * HZ)
#endif /* ECP_DEBUG */

/* Parallel Port Modes (values used in master.d/ecpplp file) */

#define PLP_ECP_MODE		6
#define PLP_COMP_MODE		4
#define PLP_BIDIR_MODE		2
#define PLP_PIO_MODE		0

/* Parallel Port States */

#define ECPPLP_ALIVE            0x1     /* parallel port exists */
#define ECPPLP_WROPEN           0x2     /* open for writing */
#define ECPPLP_RDOPEN           0x4     /* open for reading */
#define ECPPLP_NBIO             0x8     /* set for non-blocking reads */
#define ECPPLP_INUSE            0x10    /* reference flag for unloads */
#define ECPPLP_OCCUPIED         0x20    /* parallel port is busy */

#endif /* _KERNEL */

#endif /* _ECPPLPREG_H_ */
