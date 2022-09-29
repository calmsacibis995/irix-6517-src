#ident  "lib/libsk/net/if_ef.c:  $Revision: 1.82 $"

/*
 * IOC3 PCI 10/100 Mbit/s Fast Ethernet chip driver
 *
 * Copyright 1995, Silicon Graphics, Inc.  All rights reserved.
 */

/* generate stubs for all non IP22,IP27,IP30 platforms and 
   for IP30 emulation and sable simulation */

#if (!IP27 && !IP30) || (IP30 && EMULATION) || (IP30 && SABLE) || (SN0 && SABLE)

#include <libsc.h>

#define FIXME(foo)      printf("fixme: %s\n",foo); return 0;

int ef_install() { 
#ifdef SN0
FIXME("ef_install") ;
#endif

return 0 ;
}
int ef_init() { FIXME("ef_init"); }
int ef_probe() { FIXME("ef_probe"); }
int ef_open() { FIXME("ef_open"); }
int ef_close() { FIXME("ef_close"); }

#else /* only for IP27 (sn0), IP30 (speedracer) and IP22 (testing only) */

/* 
 * if_ef.c
 */

/**********************************************************************
 *
 * INCLUDES 
 *
 */

#include <setjmp.h>
#include <arcs/errno.h>
#include <arcs/hinv.h>		/* NBPP */
#include <arcs/time.h>
#include <sys/cpu.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/PCI/ioc3.h> 
#include <sys/PCI/PCI_defs.h>	
#include <net/in.h>
#include <net/socket.h>
#include <net/arp.h>
#include <net/ei.h>
#include <net/mbuf.h>
#include <saio.h>
#include <saioctl.h>
#include <libsk.h>
#include <libsc.h>

#ifdef SN0
#include <sys/PCI/bridge.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/addrs.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/error.h>
#include <diag_enet.h>
#include <pgdrv.h>
#else
#include <standcfg.h>
#endif

#include <net/if_ef.h> 

/**********************************************************************
 *
 * DEFINES 
 *
 */

#if IP30

/*
 * For now, IP30 prom supports enet on the baseio only.
 * To support multiple enets, we need to do:
 * 1. Enable discovering multiple IOC3s.
 * 2. Change the 'ei' and related variables
 *    to be arrays instead of a single struct/pointer to struct.
 * 3. An algorithm needed to assign adapter numbers for the enets
 * 4. Change command user interface (ef0mode).
 */
static int ef_installed;
#endif

#define cei(x)		((struct ether_info *)(x->DevPtr))
#define SGI_ETHER_ADDR	0x08		/* 1st byte of SGI enet addresses */

#define	EFMAXPHY	(3)		/* max phy to probe */

/*
 * Misc defines.
 */

/* used to display errors to the user */
#define err_printf(arg) { printf("ef%d: ",ei->ei_unit);printf arg ; }

/* output that gets turned on from prom via Debug env variable */
extern int Debug;
#define dbg_printf(x)	(Debug?printf x : 0)
#define Dbg_printf(x,n)	(Debug>=n?printf x : 0)

/* output that gets turned on from prom via Verbose env variable */
extern int Verbose;
#define	VRB_WARN	2	/* setting Verbose=2 prints out warnings */
#define	VRB_PHY		3	/* setting Verbose=3 prints out phy info */
#define	VRB_PKTS	4	/* setting Verbose=4 prints out packet info */
#define	Vrb_printf(x,n) (Verbose>=n?printf x : 0)


#define UNIT ei->ei_unit

/* for development work only */

/* gives malloc info on transmit/receive rings */
#ifdef VERBOSE_MALLOC
#define MPRINTF(arg) printf arg
#else
#define MPRINTF(arg)
#endif

/* prints what function code is in */
#ifdef VERBOSE_FUNC
#define FPRINTF(arg) printf arg
#else
#define FPRINTF(arg)
#endif

/*
 * Since the ioc3 compares the rx consume and produce indices
 * modulo-16, we increase number of receive buffers by 15.
 */
#define	NRBUFADJ(n)	(n + 15)
#define	NEXTTXD(i)	(((i)+1) & (ei->ei_ntxd - 1))
#define	NEXTRXD(i)	(((i)+1) & (NRXD - 1))
#define	ETHER_HDRLEN	(sizeof (struct ether_header))
#define	ETHER_CRCSZ	(4)			/* # bytes ethernet crc */

/*
 * Non-rx, non-tx, "other" interrupt conditions
 */
#define	EISR_OTHER \
	(EISR_RXOFLO | EISR_RXBUFOFLO | EISR_RXMEMERR | EISR_RXPARERR | \
	EISR_TXRTRY | EISR_TXEXDEF | EISR_TXLCOL | EISR_TXGIANT | \
	EISR_TXBUFUFLO | EISR_TXCOLLWRAP | EISR_TXDEFERWRAP | \
	EISR_TXMEMERR | EISR_TXPARERR)

#define	offsetof(s,m)	(size_t)(&(((s*)0)->m))

/* macros for reading/writing registers ... */
#define W_REG(reg,val) (reg) = (val)        /* write register */
#define R_REG(reg) (reg)                    /* read register */
#define AND_REG(reg,val) (reg) &= (val)     /* and register w/ value */
#define OR_REG(reg,val) (reg) |= (val)      /* or register w/ value */

#if SN0
#define	KVTOIOADDR(addr)	get_pci64_dma_addr((__psunsigned_t)(addr), dma_info)
#elif IP30
#define	KVTOIOADDR(addr)	kv_to_bridge32_dirmap(ef_bus_base,(__psunsigned_t)addr)
#else
#define	KVTOIOADDR(addr)	KDM_TO_PHYS(addr)
#endif	/* SN0 */

/*
 * We keep track of the EIO block used for networking, so that
 * while printing we can turn off the scanning.
 */
IOBLOCK *net_iob;

/**********************************************************************
 *
 * Declarations for variables in master.d/if_ef
 * Lots of tunables go here for now until we know what we're doing.
 */

/* # receive buffers in 10Mbit/s mode */
#define ef_nrbuf10 50

/* # receive buffers in 100Mbits/s mode */
#define ef_nrbuf100 50

/* # tx descriptors
 * Only the following values are valid:
 *     0 = default (autoconfigure)
 *   128 = force 128 tx descriptors
 *   512 = force 512 tx descriptors
 */
#define ef_ntxd 128

/* fullduplex/halfduplex modes interpacket gap programmables */
#define ef_fd_ipgt 21
#define ef_fd_ipgr1 21
#define ef_fd_ipgr2 21
#define ef_hd_ipgt 21
#define ef_hd_ipgr1 11
#define ef_hd_ipgr2 17

#define	MAXEF	8

/**********************************************************************
 *
 * STATICS 
 *
 */

static struct ef_info ef_info, *ei;
#ifdef SN0
static ULONG 	dma_info ;
extern __psunsigned_t get_pci64_dma_addr(__psunsigned_t, ULONG) ;
#endif

#ifdef IP30
static void *ef_bus_base;
#endif

extern int	nic_get_eaddr(__uint32_t *mcr, __uint32_t *gpcr_s, char *eaddr);

static struct efrxbuf ef_dummyrbuf;

static int ef_init_failed;


/**********************************************************************
 *
 * FUNCTION PROTOS 
 *
 */

/* called from outside */       
int ef_init(void);              
int ef_probe(int unit);         
int ef_open(IOBLOCK *iob);      
int ef_close(void);             

/* internal only functions */                                          
static int eioctl(IOBLOCK *);                                          
static int eoutput(struct ifnet *, struct mbuf *, struct sockaddr *);  
static void epoll(IOBLOCK *);                                          
static int eput(struct etheraddr *, struct etheraddr *, u_short,
		struct mbuf *m0);                                      
static void reclaim(void);                                             
static void ereset(void);                                              
static void getcdc(struct ef_info *ei);                                
static void ioc3reset(void);                                           
static int einit(void);                                                
static void phyunreset(void);
static int ioc3init(void);
#ifdef SN0
static int run_diags(void);
#endif /* SN0 */

static int phyprobe(void);
static int phymode(int *speed100,int *fullduplex);
static int physetmanual(void);
static int phyreset(void);
static int phyinit(void);                                              
static void phyerr(void);                                              
static int phyget(int);                                                
static void phyput(int, u_short);                                      
static void physet(int, u_short);                                      
static void phyclear(int, u_short);                                    

static void efill(void);                                               
static void eread(void);                                               
static void e_frwrd_pkt(struct mbuf *);                                

/* debug funcs */
static void dump_mbuf(struct mbuf*, int);
static void dump_mbuf_data(struct efrxbuf*, int);
static void ef_dump(void);
static void ef_dumpif(struct ifnet *ifp);
static void ef_dumpei(struct ef_info *ei);
static void ef_dumpregs(struct ef_info *ei);
static void ef_dumpphy(struct ef_info *ei);
static void ef_hexdump(char *msg, char *cp, int len);


/**********************************************************************
 *
 * us_delaybus: first make sure bus is clear, so delay is guaranteed
 *              to be relative to bus, not CPU
 */

void
us_delaybus(register uint us)
{
	flushbus();
	if (us > 0)
		us_delay(us-1);	/* sub off time for flushbus (approx) */
	return;
}

/**********************************************************************
 *
 *  ssram_discovery
 *
 */

#define	PATTERN	0x5555
static void
ssram_discovery(struct ef_info *ei)
{
	volatile __uint32_t *ssram_addr0 = &ei->ei_ssram[0];
	volatile __uint32_t *ssram_addr1 =
		&ei->ei_ssram[IOC3_SSRAM_LEN / (2 * sizeof(__uint32_t))];

	FPRINTF(("ssram_discovery()\n"));
	
	/*
	 * the ssram can be either 64kb or 128kb so we use
	 * a write-write-read sequence to find out which.
	 */

	/* set ssram size to 128 KB */
	OR_REG(ei->ei_regs->emcr, EMCR_BUFSIZ | EMCR_RAMPAR);

	/* first word in the second half of the larger size SSRAM */
	W_REG(*ssram_addr0, PATTERN);
	W_REG(*ssram_addr1, ~PATTERN & IOC3_SSRAM_DM);

	if ((R_REG(*ssram_addr0) & IOC3_SSRAM_DM) != PATTERN ||
	    (R_REG(*ssram_addr1) & IOC3_SSRAM_DM) != (~PATTERN & IOC3_SSRAM_DM)) {
   	        /* set ssram size to 64 KB */
		AND_REG(ei->ei_regs->emcr, ~EMCR_BUFSIZ);
		ei->ei_ssram_bits = EMCR_RAMPAR;
	} else
		ei->ei_ssram_bits = EMCR_BUFSIZ | EMCR_RAMPAR;
}

/**********************************************************************
 *
 *  check_eisr: check for error bits in eisr (ethernet interrupt
 *              status register)
 *
 */

static void
check_eisr(void)
{

	int	eisr;
	int	fatal;

	FPRINTF(("check_eisr()\n"));

	/* read and clear all interrupt bits */
	eisr = R_REG(ei->ei_regs->eisr);

	/* write back to clear */
	if (eisr != 0)
	  W_REG(ei->ei_regs->eisr, eisr);

        /* checks everything except RX_TIMER_INT, RX_THRESH_INT, TX_EMPTY,
	   TX_EXPLICIT */

	if ((eisr & EISR_OTHER) == 0) /* if nothing else interesting */
		return;

	fatal = 0;

	if (eisr & EISR_RXOFLO) {
		ei->ei_drop++;
		fatal = 1;
		dbg_printf(("ef%d: dropped packet\n", UNIT));
	}

	if (eisr & EISR_RXBUFOFLO) {
		ei->ei_ssramoflo++;
		fatal = 1;
		dbg_printf(("ef%d:  receive overflow\n", UNIT));
	}

	if (eisr & EISR_RXMEMERR) {
		ei->ei_memerr++;
		fatal = 1;
		err_printf(("ef%d:  receive PCI error\n", UNIT));
	}

	if (eisr & EISR_RXPARERR) {
		ei->ei_parityerr++;
		fatal = 1;
		err_printf(("ef%d:  receive SRAM parity error\n", UNIT));
	}

	if (eisr & EISR_TXRTRY) {
		ei->ei_rtry++;
		dbg_printf(("ef%d: excessive collisions\n",UNIT));
	}

	if (eisr & EISR_TXEXDEF) {
		ei->ei_exdefer++;
		dbg_printf(("ef%d: excessive defers\n",UNIT));
	}

	if (eisr & EISR_TXLCOL) {
		ei->ei_lcol++;
		dbg_printf(("ef%d: late collision\n",UNIT));
	}

	if (eisr & EISR_TXGIANT) {
		ei->ei_txgiant++;
		dbg_printf(("ef%d: transmit giant packet error\n",UNIT));
	}

	if (eisr & EISR_TXBUFUFLO) {
		ei->ei_ssramuflo++;
		fatal = 1;
		dbg_printf(("ef%d: transmit underflow\n", UNIT));
	}

	if (eisr & (EISR_TXCOLLWRAP | EISR_TXDEFERWRAP)) {
		getcdc(ei);
		dbg_printf(("ef%d: collision or defer count wrapped\n", UNIT));
	}

	if (eisr & EISR_TXMEMERR) {
		ei->ei_memerr++;
		fatal = 1;
		err_printf(("ef%d:  transmit PCI error\n", UNIT));
	}

	if (eisr & EISR_TXPARERR) {
		ei->ei_parityerr++;
		fatal = 1;
		err_printf(("ef%d: transmit SRAM parity error\n", UNIT));
	}

	if (fatal)
	        einit();

}

/**********************************************************************
 *
 * ef_einit -- cleanup globals
 *        
 */

int
ef_init(void)
{

	__uint32_t *ioc3_base;	
        __uint32_t ioc3_config_base;
	__uint32_t val;	
	int	size;
	struct ef_info	eftmp ;


        ei = &ef_info;
	eftmp.ei_regs = ei->ei_regs ;
	eftmp.ei_ssram = ei->ei_ssram ;
	eftmp.ei_ioc3regs = ei->ei_ioc3regs ;

	FPRINTF(("ef_init():\n"));

	/* set to 1 if we encounter a fatal error */
	ef_init_failed = 0;
	
        /* set the global ei pointer */
        ei = &ef_info;
        bzero(ei, sizeof(ef_info));

	/* set unit */
	ei->ei_unit = 0;

#define WBFLUSH wbflush

	ei->ei_regs = eftmp.ei_regs ;
	ei->ei_ssram = eftmp.ei_ssram ;
	ei->ei_ioc3regs = eftmp.ei_ioc3regs ;

	/* Set the ERBAR register, to enable the barrier bit in
	 * xtalk packets from bridge.  This really should be handled
	 * in generic routines since all DMA PCI devices will likely
	 * need this.  Caution: these bits are XOR'd with the 64 bit
	 * DMA address so do not set a bit in both places!
	 */
	ei->ei_erbar = 0;

	/* we have a choice of 128 or 512 transmit descriptors,
	 * For now, we use 128.
	 */
	ei->ei_ntxd = NTXDSML;
	if (ef_ntxd != 0)
	  ei->ei_ntxd = ef_ntxd;
	
	/* allocate memory */

	/* set up the array of ptrs to tx mbufs */
	if ((ei->ei_txm = (void *) malloc(ei->ei_ntxd*sizeof(struct mbuf *))) == NULL ) {
	  err_printf(("ef%d: out of memory - could not allocate array of transmit descriptor pointers\n", UNIT));
	  ef_init_failed = 1;
	  return(ENOMEM);
	}
	bzero(ei->ei_txm, ei->ei_ntxd*sizeof(struct mbuf *));
	MPRINTF(("ef_init: malloc(%X) ei_txm = %X\n",ei->ei_ntxd*sizeof(struct mbuf *),ei->ei_txm));

	/* allocate transmit descriptor ring, must be 16KB or 64KB aligned */
	size = ei->ei_ntxd * TXDSZ;
	ei->ei_txd = (struct eftxd*)align_malloc(size,MIN(size,ETBR_ALIGNMENT));
	if (ei->ei_txd == NULL) {
	  err_printf(("ef%d: out of memory - could not allocate transmit descriptors\n", UNIT));
	  free((void *)ei->ei_txm); ei->ei_txm = NULL;
	  ef_init_failed = 1;
	  return(ENOMEM);
	}
	bzero((void *)ei->ei_txd, size);
	MPRINTF(("ef_init: align malloc ei_txd = %X with size = 0x%X\n",ei->ei_txd,size));

	/* allocate receive descriptor ring, must be 4k aligned */
	size = NRXD * RXDSZ;
	ei->ei_rxd = (__uint64_t*)align_malloc(size,ERBR_ALIGNMENT);
	if (ei->ei_rxd == NULL) {
	  err_printf(("ef%d: out of memory - could not allocate receive descriptors\n", UNIT));
	  free((void *)ei->ei_txm); ei->ei_txm = NULL;
	  free((void *)ei->ei_txd); ei->ei_txd = NULL;
	  ef_init_failed = 1;
	  return(ENOMEM);
	}
	bzero((void *)ei->ei_rxd, size);	
	MPRINTF(("ef_init: align malloc ei_rxd = %X with size = 0x%X\n",ei->ei_rxd,size));

	/* set up the array of ptrs to rx mbufs */
	if ( (ei->ei_rxm = (void *) malloc(NRXD*sizeof(struct mbuf *))) == NULL ) {
	  err_printf(("ef%d: out of memory - could not allocate array of receive descriptor pointers\n", UNIT));
	  free((void *)ei->ei_txm); ei->ei_txm = NULL;
	  free((void *)ei->ei_txd); ei->ei_txd = NULL;
	  free((void *)ei->ei_rxd); ei->ei_txd = NULL;
	  ef_init_failed = 1;
	  return(ENOMEM);
	}
	bzero(ei->ei_rxm, NRXD*sizeof(struct mbuf *));
	MPRINTF(("ef_init: malloc(%d) ei_rxm = %X\n",NRXD*sizeof(struct mbuf *),ei->ei_rxm));

	/* set up the SSRAM to correct size */
	ssram_discovery(ei);

	/* phy not initialized yet */
	ei->ei_phyunit = -1;
        ei->ei_speed100 = -1;
        ei->ei_fullduplex = -1;

	return(ESUCCESS);
}

/**********************************************************************
 *
 * eprobe -- returns -1 on invalid unit number
 *
 */

int
ef_probe(int unit)
{

        FPRINTF(("ef_probe()\n"));

	/* make sure ef_init succeeded */
	if (ef_init_failed) {
	  err_printf(("error: could not probe due to prior initialization failure\n"));
	  return(ENOMEM);
	}

        /* returns true for probing unit 0 only */
	return (unit == 0 ? 1 : -1);
}

/**************************************************************************
 *
 * ef_install -- manages ARCS configuration for ef0
 *
 */


#ifdef	IP30

/* defs for ef_install below */

static COMPONENT eftmpl = {
	ControllerClass,		/* Class */
	NetworkController,		/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,	/* was 1 for net(1) */	/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	4,				/* IdentifierLength */
	"ef0"				/* Identifier */
};

static COMPONENT efptmpl = {
	PeripheralClass,		/* Class */
	NetworkPeripheral,		/* Type */
	Input|Output,			/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	29,				/* IdentiferLength */
	"ef0 ethernet (100/10 base-T)"	/* Identifier */
};

static slotdrvr_id_t efids[] = {
        {BUS_IOC3, IOC3_VENDOR_ID_NUM, IOC3_DEVICE_ID_NUM, REV_ANY},
        {BUS_NONE, 0, 0, 0}
};
drvrinfo_t ef_drvrinfo = { efids, "ef" };

int
ef_install(COMPONENT *p, slotinfo_t *slot)
{
        __psunsigned_t  ioc3_cfg_base = (__psunsigned_t)slot->cfg_base;
        __psunsigned_t  ioc3_dev_base = (__psunsigned_t)slot->mem_base;


        /*
         * currently, this code only works for
         * one interface -- the one on the onboard
         * ioc3. if we enter a second time, ignore
         * the additional interfaces.
         */
	if (ef_installed) {
#ifdef DEBUG	/* don't babble if IO6 like cards are installed */
                printf("only one ef ethernet allowed\n");
#endif
                return(EACCES);
	} else
                ef_installed = 1;

        p = AddChild(p, &eftmpl, NULL);
        if (p == (COMPONENT *)NULL) cpu_acpanic("ef0");
        p = AddChild(p, &efptmpl,(void *)0);
        if (p == (COMPONENT *)NULL) cpu_acpanic("ef0");

        ef_info.ei_regs  = (ioc3_eregs_t *)(ioc3_dev_base + IOC3_REG_OFF);
        ef_info.ei_ioc3regs = (ioc3_mem_t *)(ioc3_dev_base);
        ef_info.ei_ssram = (__uint32_t *)(ioc3_dev_base + IOC3_RAM_OFF);
	ef_bus_base = slot->bus_base;

        RegisterDriverStrategy(p,_if_strat);

        return (1);

}

#else	/* !IP30 */

#ifdef SN0

#ifdef SN_PDI
char *efmembase;

int
ef_install(pd_inst_hdl_t *pdih, pd_inst_parm_t *pdip)
{
        int             rc = 0;
        lboard_t        *brd_ptr;

        FPRINTF(("ef_install()\n"));

        /* make sure ef_init succeeded */
        if (ef_init_failed) {
          err_printf(("error: could not install due to prior initialization failure\n"));
          return(ENOMEM);
        }

        dma_info = pdipDmaParm(pdip) ;

        efmembase = (char*) GET_PCIBASE_FROM_KEY(dma_info);
        ef_info.ei_regs  = (ioc3_eregs_t *)(efmembase + IOC3_REG_OFF);
        ef_info.ei_ioc3regs = (ioc3_mem_t *)efmembase ;
        ef_info.ei_ssram = (__uint32_t *)(efmembase + IOC3_RAM_OFF);
        ei = &ef_info;

        phyunreset();

        /* Run diagnostics. */

        rc = run_diags();

        if (brd_ptr = pdiLb(pdihPdi(pdih))) {
                klioc3_t        *ioc3_ptr;

                if (ioc3_ptr = (klioc3_t *) find_first_component(brd_ptr,
                                        KLSTRUCT_IOC3))
                        ioc3_ptr->ioc3_enet.diagval = rc;
        }

	snAddChild(pdih, pdip) ;

        return(ESUCCESS);

}

#else

char *efmembase;

int
ef_install(COMPONENT *p)
{
	int		rc = 0;
	lboard_t	*brd_ptr;

        FPRINTF(("ef_install()\n"));

	/* make sure ef_init succeeded */
	if (ef_init_failed) {
	  err_printf(("error: could not install due to prior initialization failure\n"));
	  return(ENOMEM);
	}

	dma_info = p->Key ;
	
	efmembase = (char*) GET_PCIBASE_FROM_KEY(dma_info);
	ef_info.ei_regs  = (ioc3_eregs_t *)(efmembase + IOC3_REG_OFF);
	ef_info.ei_ioc3regs = (ioc3_mem_t *)efmembase ;
	ef_info.ei_ssram = (__uint32_t *)(efmembase + IOC3_RAM_OFF);
	ei = &ef_info;

	phyunreset();
	
	/* Run diagnostics. */

        rc = run_diags();

	if (brd_ptr = brd_from_key(p->Key)) {
		klioc3_t	*ioc3_ptr;

		if (ioc3_ptr = (klioc3_t *) find_first_component(brd_ptr, 
					KLSTRUCT_IOC3))
			ioc3_ptr->ioc3_enet.diagval = rc;
	}

	return(ESUCCESS);

}	
#endif /* SN_PDI */

#endif /* SN0 */

#endif  /* IP30 */

static void
phyunreset()
{
	/*
	 * IOC3 general purpose i/o pin 5 is ANDed with the system reset signal,
	 * inverted, and connected to the PHY chip reset input.
	 * To take the PHY out of reset mode, we have to write to
	 * the IOC3 gpcr register to configure pin 5 to be an output signal,
	 * then write a 1 (0=reset, 1=non-reset) into the gppr_5 data register.
	 */
	W_REG(ei->ei_ioc3regs->gpcr_s, GPCR_PHY_RESET);
	W_REG(ei->ei_ioc3regs->gppr_5, 1);
}

/**********************************************************************
 *
 * ef_open -- initialize network driver data structs
 *
 */

int
ef_open(IOBLOCK *iob)
{

        FPRINTF(("ef_open()\n"));

	/* make sure ef_init succeeded */
	if (ef_init_failed) {
	  err_printf(("error: could not open due to prior initialization failure\n"));
	  return(ENOMEM);
	}

	/* Keep track of our IOB to do MAGIC */
	if (!net_iob)
		net_iob = iob;

        /* make sure were not already open */
	if (ei->ei_if.if_flags & (IFF_UP|IFF_RUNNING|IFF_BROADCAST))
		panic("ef%d: trying to open the driver twice\n",UNIT);

	/* set port registry table: mark not bound yet */
	cei(iob)->ei_registry = -1;	

	/* get ethernet address */
#if SN0 || IP30
#if SN0
	if (nic_get_eaddr((__uint32_t*) (efmembase + IOC3_MCR),
		(__uint32_t*) (efmembase + IOC3_GPCR_S),
#elif IP30
	if (nic_get_eaddr((__uint32_t*) (IOC3_PCI_DEVIO_K1PTR + IOC3_MCR),
		(__uint32_t*) (IOC3_PCI_DEVIO_K1PTR + IOC3_GPCR_S),
#endif	/* SN0 */
		(char*) ei->ei_addr.ea_vec)) {
		err_printf(("ef%d: nic_get_eaddr failed: bad nic\n", UNIT));
		return(iob->ErrorNumber = EIO);
	}
#else
	cpu_get_eaddr(ei->ei_addr.ea_vec);        
#endif	/* SN0 || IP30 */
	bcopy(ei->ei_addr.ea_vec, ei->ei_ac.ac_enaddr, 6);

#ifdef SN0
	if (SN00){
		LOCAL_HUB_S(MD_LED0, 0x1);
		LOCAL_HUB_S(MD_LED0, 0x1);
	}
	DELAY(50000);
#endif
	/* check for valid ethernet address - 1st byte should match sgi address */
	if (ei->ei_addr.ea_vec[0] != SGI_ETHER_ADDR) {
		printf("ef0: bad ethernet address %s\n",
			(char *)ether_sprintf(ei->ei_addr.ea_vec));
		return(iob->ErrorNumber = EINVAL);
	}

	/* call einit to  set up DMA receive / transmit descriptors gcs*/
	if (einit() < 0)
		return(iob->ErrorNumber = EIO);

	/* Intializing stuff in stuct ifnet in net/arp.h gcs */
	ei->ei_if.if_unit = 0;         /* sub-unit for lower level driver */   
	ei->ei_if.if_mtu = ETHERMTU;   /* maximum transmission unit */
	ei->ei_if.if_output = eoutput; /* setting output routine */
	ei->ei_if.if_ioctl = eioctl;   /* setting ioctl routine */
	ei->ei_if.if_poll = epoll;     /* setting input poll routine */

	cei(iob)->ei_acp = &ei->ei_ac;	/* iob ptr to arpcom */

        /* set flags to open */
	ei->ei_if.if_flags = IFF_UP|IFF_RUNNING|IFF_BROADCAST;

#ifndef IP30_RPROM
	if (Verbose >= VRB_PHY)
		ef_dump();
#endif /* !IP30_RPROM */

	return(ESUCCESS);
}

/**********************************************************************
 *
 * ereset --
 *
 */

static void
ereset(void)
{
	int i;

        FPRINTF(("ereset()\n"));

	/* before resetting - get the collision and defer counts and
	   save them in ei */
	getcdc(ei);

	/* reset the ioc3 ethernet state */
	ioc3reset();

	/* free any pending transmit mbufs */
	for (i = 0; i < ei->ei_ntxd; i++) 
		if (ei->ei_txm[i]) {
			_m_freem(ei->ei_txm[i]); 
			ei->ei_txm[i] = NULL;
		}

	/* free the posted rbufs */
	for (i = 0; i < NRXD; i++)
		if (ei->ei_rxm[i]) {
			_m_freem(ei->ei_rxm[i]); 
			ei->ei_rxm[i] = NULL;
		}

	/* reset rx quick ptr */
	ei->ei_rb = &ef_dummyrbuf;

}

/**********************************************************************
 *
 * Read chip collision and defer counters and update ifnet stats.
 */
static void
getcdc(struct ef_info *ei)
{
	int etcdc;

        FPRINTF(("getcdc()\n"));

	/* collision counts are kept in the etcdc register in bits 15:0
	   deferral counts are kept  in the etcdc register in bits 31:16 */

	/* get the 32 bit register value */
	etcdc = R_REG(ei->ei_regs->etcdc);

	/* update w/ num of collisions */
	ei->ei_if.if_collisions += (etcdc & ETCDC_COLLCNT_MASK);

	/* update w/ num of deferrals */
	ei->ei_defers += (etcdc >> ETCDC_DEFERCNT_SHIFT);
}

/**********************************************************************
 *
 * Reset ethernet portions of IOC3 chip.
 */

static void
ioc3reset(void)
{
        FPRINTF(("ioc3reset()\n"));

	/*
	 * Errata from IOC3 4.4 spec,
	 * Write RESET bit, read it back to stall until the write buffer drains,
	 * spin until EMCR_ARB_DIAG_IDLE bit is set, then clear the RESET bit.
	 */
	W_REG(ei->ei_regs->emcr, EMCR_RST);
        R_REG(ei->ei_regs->emcr);
	while ((R_REG(ei->ei_regs->emcr) & EMCR_ARB_DIAG_IDLE) == 0)
		DELAY(1);
        W_REG(ei->ei_regs->emcr,0x0);

	/*
	 * Errata from IOC3 4.4 spec,
	 * must explicitly clear tx/rx consume and produce pointers.
	 */
	W_REG(ei->ei_regs->etcir, 0x0);
	W_REG(ei->ei_regs->ercir, 0x0);
	W_REG(ei->ei_regs->etpir, 0x0);
	W_REG(ei->ei_regs->erpir, 0x0);
}

/**********************************************************************
 *
 * eioctl -- device specific ioctls
 *
 */

static int
eioctl(IOBLOCK *iob)
{
        FPRINTF(("eioctl()\n"));

	switch ((__psint_t)(iob->IoctlCmd)) {
	case NIOCRESET:
		(void) ereset();
		break;

	default:
		printf("ef0: invalid ioctl (cmd %d)\n",
			iob->IoctlCmd);
		return iob->ErrorNumber = EINVAL;
	}
	return(ESUCCESS);
}

/**********************************************************************
 *
 * einit -- sets up DMA receive / transmit descriptors gcs
 *
 */

static int
einit(void)
{
	int rc;

        FPRINTF(("einit()\n"));

	/* reset the chip */
	ereset();

	/* clear transmit descriptor ring. */
	bzero((caddr_t) ei->ei_txd, ei->ei_ntxd * TXDSZ);
	ei->ei_txhead = ei->ei_txtail = 0;

	/* clear receive descriptor ring. */
	bzero((caddr_t) ei->ei_rxd, NRXD * RXDSZ);
	ei->ei_rxhead = ei->ei_rxtail = 0;

	/* IOC3 initialization */
	if (rc = ioc3init())
		return (rc);

	/* post receive buffers */
	efill();

	/* set quick pointer to head receive buffer */
	if (ei->ei_rxm[ei->ei_rxhead])
	        ei->ei_rb = mtod(ei->ei_rxm[ei->ei_rxhead], struct efrxbuf*);
	else
		ei->ei_rb = &ef_dummyrbuf;

	/* enable IOC3 */
	OR_REG(ei->ei_regs->emcr, (EMCR_TXEN | EMCR_TXDMAEN | EMCR_RXEN | EMCR_RXDMAEN));

        FPRINTF(("einit(): done\n"));

	return(ESUCCESS);
}

/**********************************************************************
 *
 * ioc3init
 *
 */

static int
ioc3init(void)
{
	ioc3_eregs_t	*regs = ei->ei_regs;
	__uint32_t	w;
	int     flags;
	int	rc;
	int off;

        FPRINTF(("ioc3init()\n"));

	/* get flags */
	flags = ei->ei_if.if_flags;

	/* phy initialization first */
	if (rc = phyinit())
		return (rc);

	DELAY(100000);

	/* check phy for errors */
	phyerr();

	/* determine number of rbufs to post */
	ei->ei_nrbuf = ei->ei_speed100? ef_nrbuf100: ef_nrbuf10;
	ei->ei_nrbuf = NRBUFADJ(ei->ei_nrbuf);

	/* set ipg values */
	if (ei->ei_fullduplex)
		w = (ef_fd_ipgr2 << ETCSR_IPGR2_SHIFT)
			| (ef_fd_ipgr1 << ETCSR_IPGR1_SHIFT)
			| ef_fd_ipgt;
	else
		w = (ef_hd_ipgr2 << ETCSR_IPGR2_SHIFT)
			| (ef_hd_ipgr1 << ETCSR_IPGR1_SHIFT)
			| ef_hd_ipgt;
	W_REG(regs->etcsr, w);

	/* set the Ethernet Random Seed Register ERSR register */ 
	/* with time stamp - add 1 to make sure its non-zero */
	W_REG(ei->ei_regs->ersr, GetTime()->Seconds + 1);	

	/* set receive barrier register */
	W_REG(regs->erbar, ei->ei_erbar);

	/* set our individual ethernet address */
	w = (ei->ei_addr.ea_vec[3] << 24) | (ei->ei_addr.ea_vec[2] << 16)
		| (ei->ei_addr.ea_vec[1] << 8) | ei->ei_addr.ea_vec[0];
	W_REG(regs->emar_l, w);
	w = (ei->ei_addr.ea_vec[5] << 8) | ei->ei_addr.ea_vec[4];
	W_REG(regs->emar_h, w);

	/* set the multicast filter - dont need in standalone mode */
	W_REG(regs->ehar_l, 0x0);
	W_REG(regs->ehar_h, 0x0);

	/* set the receive ring base address */
	W_REG(regs->erbr_l, (__uint32_t) (KVTOIOADDR(ei->ei_rxd) & 0xffffffff));
	W_REG(regs->erbr_h, (__uint32_t) ((KVTOIOADDR(ei->ei_rxd) & 0xffffffff00000000LL) >> 32));

	/* set the transmit ring base address */
	W_REG(regs->etbr_l, (__uint32_t) (KVTOIOADDR(ei->ei_txd) & 0xffffffff));
	W_REG(regs->etbr_h, (__uint32_t) ((KVTOIOADDR(ei->ei_txd) & 0xffffffff00000000LL) >> 32));

	/* set the transmit ring size */
	OR_REG(regs->etbr_l,
	       (ei->ei_ntxd == NTXDBIG)? ETBR_L_RINGSZ512 : ETBR_L_RINGSZ128);

	/* init the receive consume ptr register (ercir) */
	W_REG(ei->ei_regs->ercir, 0x0);	

	/* init the receive produce ptr register (erpir) */
	W_REG(ei->ei_regs->erpir, 0x0);	

	/* init the transmit consume ptr register (etcir) */
	W_REG(ei->ei_regs->etcir, 0x0);	
	
	/* init the transmit produce ptr register (etpir) */
	W_REG(ei->ei_regs->etpir, 0x0);	

	/*
	 * Initialize the MAC CSR register to reflect the
	 * phy autonegotiated values and previously-determined
	 * sram parity and sram size bits.
	 * Leave the tx and rx channel bits OFF for now.
	 */
	w = 0;
	if (flags & IFF_PROMISC)
		w |= EMCR_PROMISC;
	w |= EMCR_PADEN;
	w |= EMCR_RAMPAR;  

	if (ei->ei_fullduplex)
		w |= EMCR_DUPLEX;

	/* set rx offset in halfwords to ebh_header start */
	off = offsetof(struct efrxbuf, rb_ebh.ebh_ether) / 2;
	w |= off << EMCR_RXOFF_SHIFT;

	w |= ei->ei_ssram_bits;
	W_REG(regs->emcr, w);

	/* dont enable interupts in stand alone mode */
	W_REG(regs->eier, 0x0);

	return(ESUCCESS);
}

#ifdef SN0
#include <libkl.h>
#include <report.h>
/**********************************************************************
 *
 * run_diags
 *
 */
extern char  diag_name[128];

static int
run_diags(void)
{
  int           rc;
  jmp_buf       fault_buf;  
  void         *old_buf;
  
  /* Register diag exception handler. */
  if (setfault(fault_buf, &old_buf)) {
      printf("\n=====> %s diag took an exception. <=====\n", diag_name);
      diag_print_exception();
#ifdef DEBUG
      kl_log_hw_state();
      kl_error_show_log("Hardware Error State: (Forced error dump)\n",
			"END Hardware Error State (Forced error dump)\n");
#endif
      result_fail(diag_name, KLDIAG_ENET_EXCEPTION, "Took an exception"); 
      restorefault(old_buf);
      return(KLDIAG_ENET_EXCEPTION);
  }
    
  /* Call enet diags. */
  rc = diag_enet_all(diag_get_mode(),
		     SN0_WIDGET_BASE(GET_DEVNASID_FROM_KEY(dma_info),
				       GET_WIDID_FROM_KEY(dma_info)),
		     GET_PCIID_FROM_KEY(dma_info));

  /* Un-register diag exception handler. */
  restorefault(old_buf);

  return(rc);
}
#endif  /* SN0 */

/**********************************************************************
 *
 * phyprobe: probe for phy unit
 *
 */
static int
phyprobe(void)
{
        int i;
        int val;
        int r2, r3;

        if (ei->ei_phyunit != -1)
                return (ei->ei_phytype);

        for (i = 0; i < 32; i++) {
                ei->ei_phyunit = i;
                r2 = phyget(2);
                r3 = phyget(3);
                val = (r2 << 12) | (r3 >> 4);
		val &= 0xffffffc0;	/* don't look at model number */
                switch (val) {
                case PHY_QS6612X:
                case PHY_ICS1890:
                case PHY_ICS1892:
                case PHY_DP83840:
                        ei->ei_phyrev = r3 & 0xf;
                        ei->ei_phytype = val;
                        return (val);
                }
        }

        ei->ei_phyunit = -1;

        err_printf(("ef%d: PHY not found\n", ei->ei_unit));

        return (-1);

}


/**********************************************************************
 *
 * phymode: Determine the current phy speed and duplex.
 *          Return 0 on success, EBUSY if autonegotiation hasn't completed.
 */
static int
phymode(int *speed100,
	int *fullduplex)
{
	int r0, r1, r4, r5, r6;
	int v;

	r0 = phyget(0);
	r1 = phyget(1);
	r6 = phyget(6);

	/* if Phy does not support AutoNegotiation, then use
	   Phy's default values */
	if ((r0 & MII_R0_AUTOEN) == 0) {
		*speed100 = (r0 & MII_R0_SPEEDSEL)? 1: 0;
		*fullduplex = (r0 & MII_R0_DUPLEX)? 1: 0;
		return (0);
	}

	/* AutoNegotiation still in progress so return */
	if ((r1 & MII_R1_AUTODONE) == 0) {
                if (ei->ei_phytype == PHY_ICS1890) {
                /* ICS1890 WAR - refer to rev J version 3 release note */
                /* ICS1890 with legacy 100Base-TX partner(no AN capability)
                ** may prevent AN from completing. Corrected by disabling,
                ** then re-enabling AN. */
                        v = phyget(17);
                        v &= ICS1890_R17_PD_MASK;
                        if ( v == ICS1890_R17_PD_FAULT ) {
                                phyput(0, ~MII_R0_AUTOEN);
                                phyput(0, MII_R0_AUTOEN);
                        }
                }
		*speed100 = -1;
		*fullduplex = -1;
		return (EBUSY);
	}

	/* check if link partner is unable to do AutoNegotiation */
	if ((r6 & MII_R6_LPNWABLE) == 0) {
                switch (ei->ei_phytype) {
                case PHY_DP83840:
                        *speed100 = !(phyget(25) & DP83840_R25_SPEED_10);
                        *fullduplex = 0;
                        break;
                case PHY_ICS1890:
                case PHY_ICS1892:
                        v = phyget(17);
                        *speed100 = (v & ICS1890_R17_SPEED_100)? 1:0;
                        *fullduplex = (v & ICS1890_R17_FULL_DUPLEX)? 1:0;
                        break;
                case PHY_QS6612X:
                        v = phyget(31);
                        switch (v & QS6612X_R31_OPMODE_MASK) {
                        case QS6612X_R31_10:
                                *speed100 = 0;
                                *fullduplex = 0;
                                break;
                        case QS6612X_R31_100:
                                *speed100 = 1;
                                *fullduplex = 0;
                                break;
                        case QS6612X_R31_10FD:
                                *speed100 = 0;
                                *fullduplex = 1;
                                break;
                        case QS6612X_R31_100FD:
                                *speed100 = 1;
                                *fullduplex = 1;
                                break;
                        }
                }

                return (0);
	}

	/* AutoNegotiation completed so we choose speed & duplex
	   by looking at our AutoNegotiation advertisement register
	   (4) and the AutoNegotiation link partner ability register (5) */
	r4 = phyget(4);
	r5 = phyget(5);
	if ((r4 & MII_R4_TXFD) && (r5 & MII_R5_TXFD)) {
		*speed100 = 1;
		*fullduplex = 1;
	}
	else if ((r4 & MII_R4_TX) && (r5 & MII_R5_TX)) {
		*speed100 = 1;
		*fullduplex = 0;
	}
	else if ((r4 & MII_R4_10FD) && (r5 & MII_R5_10FD)) {
		*speed100 = 0;
		*fullduplex = 1;
	}
	else if ((r4 & MII_R4_10) && (r5 & MII_R5_10)) {
		*speed100 = 0;
		*fullduplex = 0;
	}

	return (0);
}

static void
phyrmw(int reg, int mask, int val)
{
	int rval, wval;
	
	rval = phyget(reg);
	wval = (rval & ~mask) | (val & mask);
	phyput(reg, (u_short)wval);
}

/**********************************************************************
 *
 * phyreset
 *
 */
static int
phyreset(void)
{
	volatile ioc3reg_t g;
	int	timeout;

	/*
	 * IOC3 gpio pin 5 is ANDed with the PCI reset signal,
	 * inverted, and connected to the PHY chip reset input.
	 * 0=reset, 1=non-reset
	 */
	W_REG(ei->ei_ioc3regs->gppr_5, 0);
	g = R_REG(ei->ei_ioc3regs->gppr_5);
	W_REG(ei->ei_ioc3regs->gppr_5, 1);
	g = R_REG(ei->ei_ioc3regs->gppr_5);

#ifdef SN0
	/*
	 * Speedo has a signal coming out of the Hub instead of
	 * IOC3 gpio pin 5 connected to the PHY reset input pin.
	 */
	if (SN00){
		LOCAL_HUB_S(MD_LED0, 0x1);
		LOCAL_HUB_S(MD_LED0, 0x1);
	}
#endif
	DELAY(500);

        if (phyprobe() == -1)
                return (-1);

/* Hmmm, the kernel driver doesn't do this */
#ifdef 0

        /* reset the PHY */
        phyput(0, MII_R0_RESET);
        timeout = 600;
        while (phyget(0) & MII_R0_RESET) {
                DELAY(5000);
                if (timeout-- <= 0) {
                        err_printf(("PHY did not complete reset\n"));
                        return (-1);
                }
        }

	/* initiate autonegotiation */
        phyput(0, MII_R0_AUTOEN | MII_R0_RESTARTAUTO);
#endif
/* This is the equivalent of kernel driver's ef_phyerrat() */
	switch(ei->ei_phytype) {
	case PHY_DP83840:
		phyrmw(23, 1<<5, 1<<5);
		break;
	case PHY_ICS1890:
	case PHY_ICS1892:
		phyrmw(18, 1<<5, 1<<5);
		break;
	default:
		break;
	}

	return (0);
}

/**********************************************************************
 *
 * physetmanual: get values from env variable, ef0mode                *
 *
 *********************************************************************/
#define phy_printf(arg) { printf("ef%d: ",ei->ei_unit);printf arg ; }
static int
physetmanual(void)
{
#if IP30

        char *mode;
        u_short r0 = 0x0;  /* value to write into phy reg 0 */
        u_short r4 = 0x0;
	int v, timeout;

        /* environment can force override mode (10, H10, F10, 100, H100, F100) */
        if ((mode = getenv("ef0mode")) != NULL) {

                phyput(0, ~MII_R0_AUTOEN);      /* disable autonegotiation */
                phyput(4, 0);                   /* clear reg. 4 - ICS1890 WAR */

		ei->ei_fullduplex = 0;
		ei->ei_speed100 = 0;

                if ((mode[0] == 'f') || (mode[0] == 'F')) {
                        /* full-duplex operating mode */
                        r0 |= MII_R0_DUPLEX;
                        ei->ei_fullduplex = 1;
		}
		
                if (((mode[1] == '1') && (mode[3] == '0')) ||
                        ((mode[0] == '1') && (mode[2] == '0'))) {
                        /* 100Mbit operating speed */
                        r0 |= MII_R0_SPEEDSEL;
                        ei->ei_speed100 = 1;
                }

                /* now program register 4 */
                if ( (ei->ei_speed100 == 0) && (ei->ei_fullduplex == 0) )
                        r4 |= MII_R4_10 | MII_R4_TXFD;
                if ( (ei->ei_speed100 == 0) && (ei->ei_fullduplex == 1) )
                        r4 |= MII_R4_10 | MII_R4_10FD | MII_R4_TXFD;
                if ( (ei->ei_speed100 == 1) && (ei->ei_fullduplex == 0) )
                        r4 |= MII_R4_TXFD;
                if ( (ei->ei_speed100 == 1) && (ei->ei_fullduplex == 1) )
                        r4 |= MII_R4_TXFD;

                /* debug output */
                if (ei->ei_speed100) {
                        dbg_printf(("setting to 100 Mbps\n")); }
                else {
                        dbg_printf(("setting to 10 Mbps\n")); }
                if (ei->ei_fullduplex) {
                        dbg_printf(("setting to full duplex\n")); }
                else {
                        dbg_printf(("setting to half duplex\n")); }

                /* write the phy register */
                phyput(0, r0);
                phyput(4, r4);

                timeout = 20000;
		v = phyget(1);
                while ( (v & MII_R1_LINKSTAT) == 0 ) {
                        DELAY(100);
                        if (timeout-- <= 0) {
				err_printf(("physetmanual: timeout waiting for link valid\n"));
				break;
			}
			v = phyget(1);
                }
		return(0);
        }
#endif
	return(-1);
}

/**********************************************************************
 *
 * phyinit
 *
 */
static int
phyinit(void)
{
	int timeout;
	int ospeed100, ofullduplex;

	FPRINTF(("phyinit()\n"));

	/* save original values */
	ospeed100 = ei->ei_speed100;
	ofullduplex = ei->ei_fullduplex;

	/* reset phy */
	if (phyreset() < 0)
		return (-1);

	if ( physetmanual() < 0 ) {
		/* Wait a while for autonegotiation to complete */
		timeout = 20000;
		while (phymode(&ei->ei_speed100, &ei->ei_fullduplex) == EBUSY) {
			DELAY(100);
			if (timeout-- <= 0)
				break;
		}

                /*
                * Eat transient link fail that happens about 500us after Autodone.
                */
                DELAY(1000);
                (void)phyget(1);

		/*
	 	 * If autonegotiation didn't complete then 
	 	 * assume 10 mbit half duplex.
		 */
		if ((ei->ei_speed100 == -1) || (ei->ei_fullduplex == -1)) {
   	       		dbg_printf(("auto-negotiation did not complete\n"));
			phyput(0, MII_R0_AUTOEN);
			ei->ei_speed100 = 0;
			ei->ei_fullduplex = 0;
		}
	}


	/* When the mode changes display the new mode */
	if ((ei->ei_speed100 != ospeed100) || (ei->ei_fullduplex != ofullduplex))
		dbg_printf(("ef%d: %d Mbits/s %sduplex\n",
			ei->ei_unit,
			ei->ei_speed100? 100: 10,
			ei->ei_fullduplex? "full": "half"));


	return(ESUCCESS);
}

/**********************************************************************
 *
 * Check PHY register 1 for errors.
 *
 * We maintain a last-known-value for RemoteFault, Jabber,
 * and LinkStatus and use this to detect a change in the state.
 * We don't want to check the link status *during* phy
 * initialization, only afterwards.
 */

static void
phyerr(void)
{
	int unit;
	int reg1;

        FPRINTF(("phyerr()\n"));

	unit = ei->ei_unit;

	/*
	 * REMFAULT, JABBER, and LINKSTAT are latching bits.
	 * For the prom, we probably only care if the
	 * condition is *currently* occurring (i.e. it's
	 * persistent) so we read R1 twice.
	 */
	reg1 = phyget(1);
	reg1 = phyget(1);

	if (reg1 & MII_R1_REMFAULT) {
		if (ei->ei_remfaulting == 0)
		        err_printf(("ef%d: remote hub fault\n", unit));
		ei->ei_remfaulting = 1;
	} else {
		if (ei->ei_remfaulting)
			dbg_printf(("ef%d: remote hub ok\n", unit));
		ei->ei_remfaulting = 0;
	}

	if ((reg1 & MII_R1_JABBER) && !ei->ei_speed100) {
		if (ei->ei_jabbering == 0)
			err_printf(("ef%d: jabbering\n", unit));
		ei->ei_jabbering = 1;
	} else {
		if (ei->ei_jabbering)
			dbg_printf(("ef%d: jabber ok\n", unit));
		ei->ei_jabbering = 0;
	}

	if (reg1 & MII_R1_LINKSTAT) {
		if (ei->ei_linkdown)
			dbg_printf(("ef%d: link ok\n", unit));
		ei->ei_linkdown = 0;
	} else {
		if (ei->ei_linkdown == 0)
			err_printf(("ef%d: link fail - check ethernet cable\n", unit));
		ei->ei_linkdown = 1;
	}
}

/**********************************************************************
 *
 * Read a PHY register.
 *
 * Several error bits in register 1 are read-clear.
 * We don't care about these bits until after phy
 * initialization is over, then we explicit test for
 * them.
 */

static int
phyget(int reg)
{
	ioc3_eregs_t	*regs = ei->ei_regs;

        FPRINTF(("phyget()\n"));

	while (regs->micr & MICR_BUSY) {
		DELAYBUS(1);
	}
        regs->micr = MICR_READTRIG | (ei->ei_phyunit << MICR_PHYADDR_SHIFT)
                | reg;
	do {
		DELAYBUS(1);
	} while (regs->micr & MICR_BUSY);

        return ((regs->midr_r & MIDR_DATA_MASK));
}

/**********************************************************************
 *
 * Write a PHY register.
 */

static void
phyput(int reg, u_short val)
{
	ioc3_eregs_t	*regs = ei->ei_regs;

        FPRINTF(("phyput()\n"));

	while (regs->micr & MICR_BUSY)
		DELAYBUS(1);

	regs->midr_w = val;
        regs->micr = (ei->ei_phyunit << MICR_PHYADDR_SHIFT) | reg;

	do {
		DELAYBUS(1);
	} while (regs->micr & MICR_BUSY);
}

/**********************************************************************
 *
 * Set a PHY register bit or bits.
 */

static void
physet(int reg, u_short bits)
{
	int	v;

        FPRINTF(("physet()\n"));

	v = phyget(reg);
	v |= bits;
	phyput(reg, (u_short)v);
}

/**********************************************************************
 *
 * Clear a PHY register bit or bits.
 */

static void
phyclear(int reg, u_short bits)
{
	int	v;

        FPRINTF(("phylclear()\n"));

	v = phyget(reg);
	v &= ~bits;
	phyput(reg, (u_short)v);
}

/**********************************************************************
 *
 * Allocate and post receive buffers.
 */

/*
 * dont initialize in_efill!! it goes to data section of prom
 * keep it un-initialized will put it at bss in main memory
 */
int in_efill;

static void
efill(void)
{
	volatile struct efrxbuf *rb;
	struct mbuf *m;
	int	head, tail;
	int	n;
	int	i;
#if HEART_COHERENCY_WAR
	int	old_tail;
#endif	/* HEART_COHERENCY_WAR */

        FPRINTF(("efill()\n"));

	if (in_efill) {
		/* Poor man's reentrancy check */
		/* We are here through some other path. Don't try again */
		return;
	}else {
		in_efill = 1;
	}


	/*
	 * Determine how many receive buffers we're lacking
	 * from the full complement, allocate, initialize,
	 * and post them, then update the chip rx produce index.
	 * Deal with m_vget() failure.
	 */

	head = ei->ei_rxhead; /* next rxd to read */
	tail = ei->ei_rxtail; /* next rxd to post */
#if HEART_COHERENCY_WAR
	old_tail = tail;
#endif	/* HEART_COHERENCY_WAR */

	if (head <= tail)
		n = ei->ei_nrbuf - (tail - head);
	else
		n = ei->ei_nrbuf - (NRXD - head) - tail;

	for (i = 0; i < n; i++) {
	        /* gets mbufs that are 128 bytes aligned and dont cross 16k boundaries */
		if ((m = _m_get(M_DONTWAIT, MT_DATA)) == NULL) {
		  err_printf(("ef%d: efill: out of memory\n", UNIT));
		  break;
		}

		ASSERT(M128ALIGN(m));

		while (1) {
		  if (((ulong)m->m_dat + m->m_off) % 128 == 0)
		    break;
		  m->m_off++;
		}

		ASSERT(M128ALIGN(m->m_dat + m->m_off));

		rb = mtod(m, struct efrxbuf*);

		rb->rb_rxb.w0 = 0;
		rb->rb_rxb.err = 0;
#if HEART_COHERENCY_WAR
		heart_write_dcache_wb_inval((void *)rb, sizeof(struct efrxbuf));
#endif	/* HEART_COHERENCY_WAR */
		ei->ei_rxm[tail] = m;

		/* needs to be a 64 bit ptr */
		ei->ei_rxd[tail] = (__uint64_t) KVTOIOADDR(rb);

		tail = NEXTRXD(tail);
	}

	ei->ei_rxtail = tail;
#if HEART_COHERENCY_WAR
	if (tail >= old_tail)
		heart_dcache_wb_inval((void *)&ei->ei_rxd[old_tail],
			(tail - old_tail) * RXDSZ);
	else {
		heart_dcache_wb_inval((void *)&ei->ei_rxd[old_tail],
			(NRXD - old_tail) * RXDSZ);
		heart_dcache_wb_inval((void *)&ei->ei_rxd[0], tail * RXDSZ);
	}
#endif	/* HEART_COHERENCY_WAR */

	W_REG(ei->ei_regs->erpir, (tail * RXDSZ) );

	in_efill = 0;
}

/**********************************************************************
 *
 * eoutput -- raw access to transmit a packet
 *
 */

static int
eoutput(struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst)
{
	ETHADDR edst;
	struct in_addr idst;
	struct ether_header *eh;
	int type;

        FPRINTF(("eoutput()\n"));

	/* set type and put destination into edst */
	switch (dst->sa_family) {

	case AF_INET:
	        /* address of destination - actual 4 bytes gcs */
		idst = ((struct sockaddr_in *)dst)->sin_addr;

		/* _arpresolve: 
		 * Resolve an IP address into an ethernet address.  If success, 
		 * desten is filled in and 1 is returned.  If there is no entry
		 * in arptab, set one up and broadcast a request 
		 * for the IP address;  return 0.
		 *  void _arpresolve(struct arpcom *ac, struct in_addr *destip, 
		 *                   u_char *desten)
		 * puts answer in edst */
		_arpresolve(&ei->ei_ac, &idst, (u_char *)&edst);
		type = ETHERTYPE_IP;
		break;

	case AF_UNSPEC:
		eh = (struct ether_header *)dst->sa_data;
		bcopy(eh->ether_dhost, &edst, sizeof(edst));
		type = eh->ether_type;
		break;

	default:
		/* free m_buf and return error */
		_m_freem(m0);
		dbg_printf(("ef%d: warning: no support for address family 0x%x\n",
			    UNIT,(int)dst->sa_family));
		return(1);
	}

	/*
	 * Queue message on interface
	 */
	return( eput( (struct etheraddr*)&edst,(struct etheraddr*)&ei->ei_addr,
		     type,m0) );
}

/**********************************************************************
 *
 * eput -- transmit a packet
 *
 */

static int
eput(	
        struct etheraddr *edst,
        struct etheraddr *esrc,
        u_short type,
        struct mbuf *m0)
{
        struct mbuf *m;
	volatile struct eftxd	*txd;
	struct ether_header *eh;
	int mlen;
	int total;

        FPRINTF(("eput()\n"));

	/*
	 * The basic performance idea is to keep a dense,
	 * linear fastpath and throw rare conditions out via "goto".
	 */
	if (ei->ei_linkdown)
		goto linkdown;

top:
	/* allocate tx descriptor */
	txd = &ei->ei_txd[ei->ei_txhead];
	if (NEXTTXD(ei->ei_txhead) == ei->ei_txtail)
		goto outoftxd;

	/* recaim every 5 mbufs */
#define TX_RECLAIM_NUM 5
	if (ei->ei_txhead % TX_RECLAIM_NUM == 0)
	  reclaim();

	/* Each 128byte IOC3 tx descriptor describes a single packet
	 * and contains up to 104 bytes of data locally, plus two
	 * pointers to noncontiguous data.  * The IOC3 tx has the
	 * following restrictions: - if data0 and buf1 are used, then
	 * data0 must be an even number of bytes in length.  - if
	 * used, buf1 must always start on an even byte address.  - if
	 * used, buf2 must start on a 128byte address and buf1 must be
	 * an even number of bytes in length - neither buf1 nor buf2
	 * may cross a page boundary */ 

	/* replacement code for above - only 1 mbuf ! */
	mlen = m0->m_len;

	if (mlen > ETHERMTU)
		goto toobig;

	/* create ether header */
	eh = (struct ether_header*) &txd->eh;
	*(struct etheraddr*)eh->ether_dhost = *edst;
	*(struct etheraddr*)eh->ether_shost = *esrc;
	eh->ether_type = type;

	total = mlen + sizeof (*eh);

	/* if small, copy it into txd */
	if (mlen <= TXDATASZ) {	
		bcopy(mtod(m0, char*), (caddr_t)txd->data, mlen);
		txd->cmd = ETXD_D0V | total;
		txd->bufcnt = mlen + sizeof(*eh);
	}
	/* point buf1 at it */
	else  {	
		txd->cmd = ETXD_D0V | ETXD_B1V | total;
		txd->bufcnt = (m0->m_len << ETXD_B1CNT_SHIFT) | sizeof (*eh);
		txd->p1 = (__uint64_t) KVTOIOADDR(mtod(m0, caddr_t));
#if HEART_COHERENCY_WAR /* IP30 only */
		heart_dcache_wb_inval(mtod(m0, void *), mlen);
#endif	/* HEART_COHERENCY_WAR */
        }
#if HEART_COHERENCY_WAR /* IP30 only */
		heart_dcache_wb_inval((void *)txd, TXDSZ);
#endif	/* HEART_COHERENCY_WAR */

	/* could fill in txd checksum fields for optimization here ... */

	/* save mbuf chain to free later */
	ASSERT(ei->ei_txm[ei->ei_txhead] == 0);
	ei->ei_txm[ei->ei_txhead] = m0;

	/* increment index of txd head */
	ei->ei_txhead = NEXTTXD(ei->ei_txhead);

	/* go - bump produce ptr */
	W_REG(ei->ei_regs->etpir, ei->ei_txhead * TXDSZ);

	/* check for error bits in EISR register */
	check_eisr();

	return (ESUCCESS);

toobig:
	_m_freem(m0); 
	dbg_printf(("ef%d: warning: received packet with invalid mbuf len %d\n",
		    UNIT, mlen));
	return (E2BIG);

outoftxd:
	reclaim();
	if (NEXTTXD(ei->ei_txhead) != ei->ei_txtail)
		goto top;
	_m_freem(m0); 
	return (ENOMEM);

outofmbufs:
	_m_freem(m0); 
	dbg_printf(("ef%d: out of mbufs\n", UNIT));
	return (ENOMEM);

linkdown:
	_m_freem(m0); 
	err_printf(("ef%d: link down\n", UNIT));
	phyerr();
	return (EIO);
}

/**********************************************************************
 *
 * Reclaim all the completed tx descriptors/mbufs.
 */

static void
reclaim(void)
{
	int	consume;

        FPRINTF(("reclaim()\n"));

	ASSERT(IFNET_ISLOCKED(&ei->ei_if));

	consume = (R_REG(ei->ei_regs->etcir) & ETCIR_TXCONSUME_MASK)/TXDSZ;
	ASSERT(consume < ei->ei_ntxd);

	for (; ei->ei_txtail != consume; ei->ei_txtail = NEXTTXD(ei->ei_txtail)) {
		_m_freem(ei->ei_txm[ei->ei_txtail]);
		ei->ei_txm[ei->ei_txtail] = NULL;
	}
}

/**********************************************************************
 *
 * eread -- Process received packets.
 * 
 */

static void
eread(void)
{
	struct efrxbuf *rb;
	struct mbuf *m;
	int w0;
	int err;
	int rlen;
	int i;

        FPRINTF(("eread()\n"));

	/*
	 * Process all the received packets.
	 */
	for (i = ei->ei_rxhead; i != ei->ei_rxtail; i = NEXTRXD(i)) {

	        ei->ei_rxhead = i;

		m = ei->ei_rxm[i];

	        /* make sure we got a valid ptr */
	        if (m == NULL) {
		  /* this should never happen ! */
		  dbg_printf(("ef%d: null mbuf\n", UNIT));
		  return;
		}

		rb = mtod(m, struct efrxbuf*); 

		w0 = rb->rb_rxb.w0;
		err = rb->rb_rxb.err; 

		if ((w0 & ERXBUF_V) == 0)  {
			break;
		}

		ei->ei_rxm[i] = NULL; 

		rlen = ((w0 & ERXBUF_BYTECNT_MASK) >> ERXBUF_BYTECNT_SHIFT) - ETHER_CRCSZ;

		m->m_off += sizeof (struct ioc3_erxbuf);

		ei->ei_if.if_ipackets++;

		if (!(err & ERXBUF_GOODPKT) || (rlen < ETHER_HDRLEN))
			goto error;

		m->m_len = (rlen - ETHER_HDRLEN) + sizeof (struct etherbufhead);

		e_frwrd_pkt(m);

		continue;

error:

		if (rlen + ETHER_CRCSZ < 64) {
			ei->ei_runt++;
			dbg_printf(("ef%d: warning: received runt packet\n", UNIT));
			rlen = ETHER_HDRLEN;
		}

		if (err & ERXBUF_CRCERR) {
			if (rlen + ETHER_CRCSZ == GIANT_PACKET_SIZE) {
				ei->ei_giant++;
				dbg_printf(("ef%d: warning: giant packet received\n", UNIT));
				rlen = ETHERMTU + ETHER_HDRLEN;
			}
			else {
				ei->ei_crc++;
				dbg_printf(("ef%d: warning: crc error\n", UNIT));
			}
		}
		else if (rlen + ETHER_CRCSZ > 1518) {
			ei->ei_giant++;
			dbg_printf(("ef%d: giant packet received\n", UNIT));
			rlen = ETHERMTU + ETHER_HDRLEN;
		}

		/*
		 * Skip tests for ERXBUF_FRAMERR and ERXBUF_INVPREAMB because
		 * there's no way that either of these can be the only error since,
		 * by themselves, they aren't considered "bad packet" conditions.
		 * If these bits -are- set in a bad packet, they are secondary effects
		 * of the real error, so it really just muddies the water to report
		 * them.
		 */

		if (err & ERXBUF_CODERR) {
			ei->ei_code++;
			dbg_printf(("ef%d: warning: framing error\n", UNIT));
		}

		m->m_len = (rlen - ETHER_HDRLEN) + sizeof (struct etherbufhead);

		e_frwrd_pkt(m);
	}

	/* update rxd head */
	ei->ei_rxhead = i;

	/* post any necessary receive buffers */
	efill();

	/* update quick rbuf pointer */
	if (ei->ei_rxm[i]) {
	        ei->ei_rb = mtod(ei->ei_rxm[i], struct efrxbuf*);
	}
	else {
		ei->ei_rb = &ef_dummyrbuf;
	}

}

/**********************************************************************
 *
 * e_frwrd_pkt -- forward packet to appropriate protocol handler
 *
 */

static void
e_frwrd_pkt(struct mbuf *m)
{
	struct ether_header *eh;
	uint len1;
	u_short type;
	u_char *cp;

        FPRINTF(("e_frwrd_pkt()\n"));

	/* cast mbuf to ether_header */
	eh = &mtod(m, struct etherbufhead *)->ebh_ether;

	/* cast to char* */
	cp = (u_char *)eh;

	/* set offset in mbuf to account for header */
	m->m_off += sizeof(struct etherbufhead); 

	/* set len in mbuf to account for header */
	m->m_len -= sizeof(struct etherbufhead); 
	/* move cp ptr */
	cp += sizeof(struct ether_header);
	/* save a copy of len */
	len1 = m->m_len;
	
	/* get type from ether header */
#ifdef	_MIPSEL
	type = nuxi_s((u_short)eh->ether_type);
#else	/*_MIPSEB*/
	type = eh->ether_type;
#endif	/*_MIPSEL*/

	/* take care of ETHERTYPE_TRAIL	case */
	if (type >= ETHERTYPE_TRAIL
	    && type < ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER) 
	  Vrb_printf(("ef%d: warning: received trailer packet ... discarding\n",UNIT), VRB_WARN);

	/* length must be > 0 to be meaningful */
	if(len1 == 0) {
		Vrb_printf(("ef%d: warning: discarding 0 length packet\n",UNIT), VRB_WARN);
		_m_freem(m);
		return;
	}

	/* now switch on type */
	switch (type) {
	case ETHERTYPE_IP:
    	        Vrb_printf(("ip "), VRB_PKTS);
		_ip_input(&ei->ei_if, m);
		break;

	case ETHERTYPE_ARP:
    	        Vrb_printf(("arp "), VRB_PKTS);
		_arpinput(&ei->ei_ac, m);
		break;

	default:
		Vrb_printf(("ef%d: discarding packet with unknown type 0x%x\n", 
			    UNIT,type),VRB_PKTS);
		_m_freem(m);
	}

	/* check for error bits in EISR register */
	check_eisr();

}

/**********************************************************************
 *
 * ef_close -- close
 * 
 */

int
ef_close(void)
{

        FPRINTF(("Calling _efclose\n"));

	/* make sure ef_init succeeded */
	if (ef_init_failed) {
	  err_printf(("error: could not close due to prior initialization failure\n"));
	  return(ENOMEM);
	}

	/* turn off RX_enable RX_dma enable , TX_enable, TX_dma_enable */
	AND_REG(ei->ei_regs->emcr, 
		(~(EMCR_TXEN | EMCR_TXDMAEN | EMCR_RXEN | EMCR_RXDMAEN)));

        /* set flags to close */
	ei->ei_if.if_flags &= ~(IFF_UP|IFF_RUNNING|IFF_BROADCAST);

	return(ESUCCESS);
}

/**********************************************************************
 *
 * epoll - checks to see if we received a packet by seeing 
 *         if the valid bit is on in buffer
 *
 */

static int in_epoll;

static void
epoll(IOBLOCK *io)
{
        int dif;

#if 0
	FPRINTF(("epoll()\n"));
#endif

	/* Poor man's recursion prevention */
	if (in_epoll) 
		return;

	in_epoll = 1;

#if 0
	/* check for error bits in EISR register */
	check_eisr();
#endif

	if (ei->ei_rb->rb_rxb.w0 & ERXBUF_V) {
	  eread();
	}

	in_epoll = 0;

}

#ifndef IP30_RPROM
/**********************************************************************
 *
 * DEBUG FUNCTIONS
 *
 */

static void 
dump_mbuf(struct mbuf* m, int hex_dump_only) {

  int i;

  printf("dump mbuf m = 0x%X\n",m);

  /* dont interpret fields */
  if (hex_dump_only) {
    for (i=0 ; i<sizeof(struct mbuf)/sizeof(uint) ; i++) {
      if (i%5 == 0)
	printf("0x%08X: ",(uint *)m+i);
      printf("%08X ",*((uint *)m+i));
      if (i%5 == 4)
	printf("\n");
    }
    printf("\n");  
    
    return;
  }

  printf("m_len = 0x%X\n",m->m_len);
  printf("m_inuse = 0x%X\n",m->m_inuse);
  printf("m_srcaddr.sa_family  = 0x%X\n",m->m_srcaddr.sa_family  );
  printf("m_srcaddr.sa_data  = %d.%d.%d.%d %d.%d.%d.%d\n",
	 m->m_srcaddr.sa_data[0],m->m_srcaddr.sa_data[1],
	 m->m_srcaddr.sa_data[2],m->m_srcaddr.sa_data[3],
	 m->m_srcaddr.sa_data[4],m->m_srcaddr.sa_data[5],
	 m->m_srcaddr.sa_data[6],m->m_srcaddr.sa_data[7]);
  printf("m_off = 0x%X\n",m->m_off);
  printf("m_dat = 0x%X\n",m->m_dat);
  printf("m_act = 0x%X\n",m->m_act);


}

static void 
dump_mbuf_data(struct efrxbuf* e, int hex_dump_only) {
  
  int i;

  /* dont interpret fields */
  if (hex_dump_only) {
    printf("data: MLEN = %d\n",MLEN);
    for (i=0 ; i<MLEN/sizeof(uint) ; i++) {
      if (i%5 == 0)
	printf("0x%08X: ",(uint *)e+i);
      printf("%08X ",*((uint *)e+i));
      if (i%5 == 4)
	printf("\n");
    }
    printf("\n");  
    
    return;
  }

  printf("Dumping mbuf data portion = 0x%X\n",e);
  
  printf("ioc3_erxbuf: w0 = 0x%X\n",e->rb_rxb.w0);
  printf("ioc3_erxbuf: err = 0x%X\n",e->rb_rxb.err);

  printf("etherbufhead: ifheader ebh_ifh - NOT USED, size = %d chars\n",
	 IFHDRPAD);
  for (i=0 ; i<IFHDRPAD/sizeof(uint) ; i++)
    printf("0x%X ",*(((uint *)e->rb_ebh.ebh_ifh)+i));
  printf("\n");

  printf("etherbufhead: snoopheader ebh_snoop - NOT USED, size = %d chars\n",
	 SNOOPPAD);
  for (i=0 ; i<SNOOPPAD/sizeof(uint) ; i++)
    printf("0x%X ",*(((uint *)e->rb_ebh.ebh_snoop)+i));
  printf("\n");

  printf("etherbufhead: pad NOT USED, size = %d chars\n",
	 ETHERHDRPAD);
  for (i=0 ; i<ETHERHDRPAD/sizeof(uint) ; i++)
    printf("0x%X ",*(((uint *)e->rb_ebh.ebh_pad)+i));
  printf("\n");

  printf("etherbufhead: ether_header ebh_ether\n");
  printf("etherbufhead: ether_header: ether_dhost = %x:%x:%x:%x:%x:%x\n",
	 e->rb_ebh.ebh_ether.ether_dhost[0],e->rb_ebh.ebh_ether.ether_dhost[1],
	 e->rb_ebh.ebh_ether.ether_dhost[2],e->rb_ebh.ebh_ether.ether_dhost[3],
  	 e->rb_ebh.ebh_ether.ether_dhost[4],e->rb_ebh.ebh_ether.ether_dhost[5]);
  printf("etherbufhead: ether_header: ether_shost = %x:%x:%x:%x:%x:%x\n",
	 e->rb_ebh.ebh_ether.ether_shost[0],e->rb_ebh.ebh_ether.ether_shost[1],
	 e->rb_ebh.ebh_ether.ether_shost[2],e->rb_ebh.ebh_ether.ether_shost[3],
  	 e->rb_ebh.ebh_ether.ether_shost[4],e->rb_ebh.ebh_ether.ether_shost[5]);
  printf("etherbufhead: ether_header: ether_type = 0x%X\n",
	 e->rb_ebh.ebh_ether.ether_type);

  printf("data: size = %d\n",EFRDATASZ);
  for (i=0 ; i<EFRDATASZ/sizeof(uint) ; i++) {
    if (i%5 == 0)
      printf("0x%08X: ",((uint *)e->rb_data)+i);
    printf("%08X ",*(((uint *)e->rb_data)+i));
    if (i%5 == 4)
      printf("\n");
  }
  printf("\n");  


}

static void
ef_dump(void)
{

        ef_dumpif(&ei->ei_if);
	printf("\n");
	ef_dumpei(ei);
	printf("\n");
	ef_dumpregs(ei);
	printf("\n");
	ef_dumpphy(ei);
}

static void
ef_dumpif(struct ifnet *ifp)
{
        printf("if_unit %d if_mtu %d if_flags 0x%x if_timer %d\n",
                ifp->if_unit, ifp->if_mtu, ifp->if_flags, ifp->if_timer);
        printf("if_ipackets %d if_ierrors %d if_opackets %d if_oerrors %d\n",
                ifp->if_ipackets, ifp->if_ierrors,
                ifp->if_opackets, ifp->if_oerrors);
        printf("if_collisions %d\n",
                ifp->if_collisions);
}

static void
ef_dumpei(struct ef_info *ei)
{
        printf("ac_enaddr %s\n", ether_sprintf(ei->ei_curaddr));
        printf("phyunit %d\n",
                ei->ei_phyunit);
        printf("ntxd %d txd 0x%x txhead %d txtail %d txm 0x%x linkdown %d\n",
                ei->ei_ntxd, ei->ei_txd, ei->ei_txhead, ei->ei_txtail,
                ei->ei_txm, ei->ei_linkdown);
        printf("nrbuf %d rxd 0x%x rxhead %d rxtail %d rb 0x%x rxm 0x%x\n",
                ei->ei_nrbuf, ei->ei_rxd, ei->ei_rxhead, ei->ei_rxtail,
                ei->ei_rb, ei->ei_rxm);
        printf("erbar 0x%x ssram_bits 0x%x\n",
                ei->ei_erbar, ei->ei_ssram_bits);
        printf("speed100 %d fullduplex %d remfaulting %d jabbering %d\n",
                ei->ei_speed100, ei->ei_fullduplex, ei->ei_remfaulting, ei->ei_jabbering);
        printf("defers %d runt %d crc %d giant %d code %d\n",
                ei->ei_defers, ei->ei_runt, ei->ei_crc, ei->ei_giant, ei->ei_code);
        printf("drop %d ssramoflo %d ssramuflo %d memerr %d\n",
                ei->ei_drop, ei->ei_ssramoflo,
                ei->ei_ssramuflo, ei->ei_memerr);
        printf("parityerr %d rtry %d exdefer %d lcol %d txgiant %d\n",
                ei->ei_parityerr, ei->ei_rtry, ei->ei_exdefer,
                ei->ei_lcol, ei->ei_txgiant);
}

static void
ef_dumpregs(struct ef_info *ei)
{
        ioc3_eregs_t *regs = ei->ei_regs;

        printf("ei 0x%x ", ei);
        printf("ei_regs 0x%x\n", regs);
        printf("emcr 0x%x eisr 0x%x eier 0x%x\n",
                regs->emcr, regs->eisr, regs->eier);
        printf("ercsr 0x%x erbr_l 0x%x erbr_h 0x%x erbar 0x%x\n",
                regs->ercsr, regs->erbr_l, regs->erbr_h, regs->erbar);
        printf("ercir 0x%x erpir 0x%x ertr 0x%x etcsr 0x%x etcdc 0x%x\n",
                regs->ercir, regs->erpir, regs->ertr,
                regs->etcsr, regs->etcdc);
        printf("etbr_l 0x%x etbr_h 0x%x etcir 0x%x etpir 0x%x\n",
                regs->etbr_l, regs->etbr_h, regs->etcir, regs->etpir);
        printf("ebir 0x%x emar_l 0x%x emar_h 0x%x ehar_l 0x%x ehar_h 0x%x\n",
                regs->ebir, regs->emar_l, regs->emar_h, regs->ehar_l, regs->ehar_h);
        printf("micr 0x%x\n", regs->micr);
}

static void
ef_dumpphy(struct ef_info *ei)
{
	int i;
	int nl = 0;

	for (i = 0; i < 32; i++) {
		printf("reg%d 0x%04x ", i, phyget(i));
		if (++nl >= 4) {
			printf("\n");
			nl = 0;
		}
	}
	if (nl != 0)
		printf("\n");
}

/*
 * print msg, then variable number of hex bytes.
 */
static void
ef_hexdump(char *msg, char *cp, int len)
{
	register int idx;
	register int qqq;
	char qstr[512];
	int maxi = 128;
	static char digits[] = "0123456789abcdef";

	if (len < maxi)
		maxi = len;
	for (idx = 0, qqq = 0; qqq<maxi; qqq++) {
		if (((qqq%16) == 0) && (qqq != 0))
			qstr[idx++] = '\n';
		qstr[idx++] = digits[cp[qqq] >> 4];
		qstr[idx++] = digits[cp[qqq] & 0xf];
		qstr[idx++] = ' ';
	}
	qstr[idx] = 0;
	printf("%s: %s\n", msg, qstr);
}
#endif /* !IP30_RPROM */

#endif /* not (SN0 || IP30's) */
