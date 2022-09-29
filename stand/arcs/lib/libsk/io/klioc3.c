#include <sys/SN/addrs.h>
#include <sys/SN/agent.h>      /* hub - network interface */
#include <sys/SN/klconfig.h>   /* config stuff */
#include <libsc.h>
#include <arcs/errno.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <sys/SN/promcfg.h>
#include <arcs/cfgdata.h>
#include <sys/SN/SN0/klhwinit.h>
#include <promgraph.h>
#include <pgdrv.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>

extern graph_hdl_t prom_graph_hdl ;
extern int ns16550_install(COMPONENT *) ;
extern int ef_install(COMPONENT *) ;
extern int pckbd_install(COMPONENT *) ;

extern int _ns16550_strat(COMPONENT *self, IOBLOCK *iob);
extern int _if_strat(COMPONENT *self, IOBLOCK *iob);
extern int _kbd_strat(COMPONENT *self, IOBLOCK *iob);
extern int _ioc3_strat(COMPONENT *self, IOBLOCK *iob) ;

extern vertex_hdl_t hw_vertex_hdl ;
int	_ioc3_strat() ;

static int kl_ioc3_AddChild(COMPONENT *, int , int(*)());

int num_ioc3 ;
int ioc3_installed ;

/*
 * This file contains routines to install and init the IOC3
 * controller in the IO6prom.
 * 
 * When the IOC3 is discovered, a klioc3_t structure is 
 * created and is attached to the lboard_t of the board of
 * which it is a part of.
 *
 * An IOC3 has the following components which have different
 * device drivers to handle them:
 *
 * . UART     - _ns16550_strat
 * . Ethernet - _if_strat
 * . keyboard - _kbd_strat
 * . mouse    - _ms_strat
 *
 */

/* 
 * These are the old style ARCS templates for the various
 * IOC3 components. The reason for having these here is that
 * the prom driver still understand and need these structures.
 * All this will go away when we have in the PROM the IO
 * infrastructure and drivers that understand IO infrastructure.
 */

static COMPONENT ttyptmpl = {
        PeripheralClass,                /* Class */
        LinePeripheral,                 /* Controller */
        (IDENTIFIERFLAG)(ConsoleIn|ConsoleOut|Input|Output), /* Flags */
        SGI_ARCS_VERS,                  /* Version */
        SGI_ARCS_REV,                   /* Revision */
        0,                              /* Key */
        0x01,                           /* Affinity */
        0,                              /* ConfigurationDataSize */
        0,                              /* IdentifierLength */
        NULL                            /* Identifier */
};

static COMPONENT netdev = {
        PeripheralClass,                /* Class */
        NetworkPeripheral,              /* Controller */
        (IDENTIFIERFLAG)(Input|Output), /* Flags */
        SGI_ARCS_VERS,                  /* Version */
        SGI_ARCS_REV,                   /* Revision */
        0,                              /* Key */
        0x01,                           /* Affinity */
        0,                              /* ConfigurationDataSize */
        0,                              /* IdentifierLength */
        NULL                            /* Identifier */
};

static COMPONENT kbdtempl = {
	PeripheralClass,		/* Class */
	KeyboardPeripheral,		/* Type */
	ReadOnly|ConsoleIn|Input,	/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	NULL				/* Identifier */
};

static COMPONENT sgims = {
        PeripheralClass,                /* Class */
        PointerPeripheral,              /* Peripheral */
        (IDENTIFIERFLAG)(ReadOnly|Input), /* Flags */
        SGI_ARCS_VERS,                  /* Version */
        SGI_ARCS_REV,                   /* Revision */
        0,                              /* Key */
        0x01,                           /* Affinity */
        0,                              /* ConfigurationDataSize */
        0,                              /* IdentifierLength */
        0                               /* Identifier */
};

/*
 * Called By: vvisit_install in promgraph.c. It is called when
 *            drivers for each device is being 'installed'.
 *            Installing a driver is putting the address of
 *            the driver _strat routine in a known place in
 *            the promgraph, which can be later easily
 *            retrieved. Also, any devices attached to the
 *            controller are discovered and the KLCONFIG is
 *            updated. hinv uses this info and the graph info.
 */

/*
 * Procedure: The procedure is not well defined for all kinds
 *            of devices. Each device may need their own
 *            special processing. The general flow is given below:
 * XXX
 */

#ifdef SN_PDI
int
kl_ioc3_install(vertex_hdl_t hw_vhdl, vertex_hdl_t devctlr_vhdl)
{
	return 1 ;
}
#else
int
kl_ioc3_install(vertex_hdl_t hw_vhdl, vertex_hdl_t devctlr_vhdl) 
{
	int 		i = 0, ctlr_num ; 
	COMPONENT 	*uart_compt, *net_compt, *kbd_compt, *ms_compt ;
	COMPONENT 	tmp_compt ;
	graph_error_t	graph_err ;
	klinfo_t	*ctrkli_ptr ;
	__psunsigned_t 	hub_base ;
	unsigned char 	hubwid ;
	__psunsigned_t	ioc3_register_base,
			ioc3_uart_base, ioc3_enet_base ;
	ULONG		Key ;
	prom_dev_info_t	*dev_info ;

	if (!ioc3_installed)
		ioc3_installed = 1 ;
	else {
		return 1 ;
	}

	/* Get the dev_info struct of the device controller. */

        graph_err = graph_info_get_LBL(prom_graph_hdl, devctlr_vhdl,
                                       INFO_LBL_DEV_INFO,NULL,
                                       (arbitrary_info_t *)&dev_info) ;
        if (graph_err != GRAPH_SUCCESS) {
          	printf("ioc3 install: cannot get dev_info\n");
          	return 0;
        }

        /* register strategy routine for controller */

        kl_reg_drv_strat(dev_info, _ioc3_strat) ;

	/* Update klinfo_t of the controller with its adapter number = virt id */

	ctrkli_ptr 	   = dev_info->kl_comp ;
	ctrkli_ptr->virtid = Key = num_ioc3++ ; /* number of ioc3's found */

	/* Build the rest of the Key for this controller. */

	Key |= MK_SN0_KEY(ctrkli_ptr->nasid, 
			ctrkli_ptr->widid, ctrkli_ptr->physid) ;

	hub_base = (__psunsigned_t) REMOTE_HUB(ctrkli_ptr->nasid, 0) ;

#if 0
	if (ctrkli_ptr->nasid)
	else
		hub_base = (__psunsigned_t) LOCAL_HUB(0) ;
#endif

	hubwid = GET_FIELD64((hub_base + IIO_WIDGET_CTRL), WIDGET_ID_MASK, 0) ;
	ADD_HUBWID_KEY(Key, hubwid) ;
	ADD_HSTNASID_KEY(Key, get_nasid()) ;

	/* Fill up ARCS info and put it in klinfo_t of the device */

        uart_compt = (COMPONENT *)malloc(sizeof(COMPONENT)) ;
        if (uart_compt == NULL) {
                printf("ioc3 uart inst : malloc failed \n") ;
                return 0 ;
        }

	bcopy(&ttyptmpl, uart_compt, sizeof(COMPONENT)) ;
        uart_compt->AffinityMask = (ULONG)devctlr_vhdl ;
	uart_compt->Key = Key ;

	tmp_compt.Key = ((__psunsigned_t) GET_PCIBASE_FROM_KEY(uart_compt->Key)) +
			IOC3_SIO_UA_BASE ;

	ns16550_install(&tmp_compt) ;
	kl_ioc3_AddChild(uart_compt, KLSTRUCT_IOC3UART, _ns16550_strat) ;

	/* Install second tty port */
	/* Indicate this by setting the physid to 1. */
	/* This is really a controller install. Detect this later in
	   promgraph. */

	uart_compt->Key |= 1 ;
	kl_ioc3_AddChild(uart_compt, KLSTRUCT_IOC3UART, _ns16550_strat) ;

	/* 
	 * Install Ethernet device 
	 */
        net_compt = (COMPONENT *)malloc(sizeof(COMPONENT)) ;
        if (net_compt == NULL) {
                printf("ethernet install : malloc failed \n") ;
                return 0 ;
        }

	bcopy(&netdev, net_compt, sizeof(COMPONENT));

	net_compt->Type = NetworkPeripheral ;
	net_compt->AffinityMask = (ULONG)devctlr_vhdl;
	net_compt->IdentifierLength = 0 ;
	net_compt->Key  = Key;

	ef_install(net_compt) ;
	kl_ioc3_AddChild(net_compt, KLSTRUCT_IOC3ENET, _if_strat) ;

	/* 
	 * Install Keyboard device 
	 */
        kbd_compt = (COMPONENT *)malloc(sizeof(COMPONENT)) ;
        if (kbd_compt == NULL) {
                printf("keyboard install : malloc failed \n") ;
                return 0 ;
        }

	bcopy(&kbdtempl, kbd_compt, sizeof(COMPONENT));

	kbd_compt->AffinityMask = (ULONG)devctlr_vhdl;
	kbd_compt->Key  = Key;

	PUT_INSTALL_STATUS(kbd_compt, 0) ;
	pckbd_install(kbd_compt) ;

	/* Install only if devices are found XXX check for mouse and/or kbd */

	if (GET_INSTALL_STATUS(kbd_compt) & 1)
		kl_ioc3_AddChild(kbd_compt, KLSTRUCT_IOC3PCKM, _kbd_strat) ;

        if (GET_INSTALL_STATUS(kbd_compt) & 2) {
                ms_compt = (COMPONENT *)malloc(sizeof(COMPONENT)) ;
                if (ms_compt == NULL) {
                        printf("mouse install : malloc failed \n") ;
                        return 0 ;
                }

                bcopy(&sgims, ms_compt, sizeof(COMPONENT));
                ms_compt->AffinityMask = (ULONG)devctlr_vhdl;
                ms_compt->Key  = Key;
                kl_ioc3_AddChild(ms_compt, KLSTRUCT_IOC3MS, _kbd_strat) ;
        }

#if 0
	else {
		printf("***Warning: IOC3 PC Keyboard not found.\n") ;
		printf("            Reverting back to tty keyboard\n") ;
	}
#endif

	return 1;
}

static int
kl_ioc3_AddChild(COMPONENT *arcs_compt, int type, int(*dev_strat)())
{
	klinfo_t 	*klinf_ptr ;
	vertex_hdl_t	 devctlr_vhdl = arcs_compt->AffinityMask ; /* Kludge */
	char 		*ctmp ;
	
	klinf_ptr = (klinfo_t *)init_device_graph(devctlr_vhdl, type) ;
        if (!klinf_ptr)
                panic("kl_ioc3_AddChild: Cannot allocate any more memory\n") ;

	if (arcs_compt->IdentifierLength) {
		ctmp = malloc(arcs_compt->IdentifierLength + 1) ;
		strncpy(ctmp, arcs_compt->Identifier, arcs_compt->IdentifierLength) ;
		arcs_compt->Identifier = ctmp ;
	}

	klinf_ptr->physid = (arcs_compt->Key & 0xf) ; /* target */
	/* 
	 * NOTE: There is some mixup between the model that the PROM
	 * uses and the model that the driver uses as far as the 
	 * second tty port on the ioc3 is concerned. The driver thinks
 	 * it is a second controller and uses the IOB->Controller
 	 * field to get to the 2nd port. The IOC3 sees it as a second
	 * device with a different set of addrs for the second port.
 	 */
        /* sneak in and change the virtid of this to 1 */
        if ((type == KLSTRUCT_IOC3UART) && (klinf_ptr->physid == 1))
                klinf_ptr->virtid = 1 ;
	
        klinf_ptr->arcs_compt = arcs_compt ;

	link_device_to_graph(hw_vertex_hdl, devctlr_vhdl,
				(klinfo_t *)klinf_ptr, dev_strat) ;

	return 1 ;
	
}

#endif

int
_ioc3_strat(COMPONENT *self, IOBLOCK *iob)
{
#ifndef SN_PDI
	switch (iob->FunctionCode) {

	case	FC_INITIALIZE:
		_ns16550_strat(self, iob) ;
	 	_if_strat(self, iob) ;
		_kbd_strat(self, iob) ;
		self->Type = PointerPeripheral ;
		_kbd_strat(self, iob) ;
                return iob->ErrorNumber ;

	default:
		return iob->ErrorNumber = EINVAL;
	}
#else
	return 0 ;
#endif
}

#ifdef SN_PDI

int
_sn_ioc3_strat(COMPONENT *self, IOBLOCK *iob)
{

        switch (iob->FunctionCode) {

        case    FC_INITIALIZE:
                _ns16550_strat(self, iob) ;
                _if_strat(self, iob) ;
                _kbd_strat(self, iob) ;
                self->Type = PointerPeripheral ;
                _kbd_strat(self, iob) ;
                return iob->ErrorNumber ;

        default:
                return iob->ErrorNumber = EINVAL;
        }
}

/* IOC3 is a composite device composed of ... */

static char *ioc3_dev_key[] = {
	"ioc3_tty"	,
	"ioc3_ei"	,
	"ioc3_pckm"	,
	NULL 
} ;

/* Install IOC3 driver on the master ioc3 only */

static int	sn_ioc3_inst ;
static int	sn_num_ioc3  ;

int
ioc3_check_install(lboard_t *lb, klinfo_t *k)
{
        if (lb->brd_type != KLTYPE_BASEIO)
		return 0 ;

	return(check_install_once(&sn_ioc3_inst)) ;
}

/*
 * Install for a composite controller. One controller
 * handles many sub-controllers. Need to to almost the
 * same job as pgCallInstall.
 */

void
composite_install(char **key_list, pd_inst_hdl_t *pdih)
{
	char 		**kl = key_list ;
        prom_drv_t      *pd ;
        int             dindx ;
        pd_info_t       *pdi ;

	if (kl == NULL)
		return ;

	while (*kl) {

        	/* Search for a prom devsw entry */
        	if ((pd = pgGetDrvTabPtrIndx(-1, -1, *kl, &dindx)) == NULL)
                	continue ;

		/* Make a pdi */
        	pdi = (pd_info_t *)pgCreatePd(sizeof(pd_info_t)) ;
        	if (pdi == NULL)
                	continue ;

		/* Init pdi */
        	pdi->pdi_devsw_p        = pd    ;
        	pdi->pdi_devsw_i        = dindx ;
        	pdi->pdi_flags          = 0     ;
        	pdi->pdi_k              = pdiK(pdihPdi(pdih))  ;
        	pdi->pdi_lb             = pdiLb(pdihPdi(pdih)) ;

		/* Do the Install for this sub-device */
		pgDoInstall(pdihCtrlV(pdih), pdi) ;
		kl++ ;
	}
}

int
sn_ioc3_install(pd_inst_hdl_t *pdih, pd_inst_parm_t *pdip)
{
	composite_install(ioc3_dev_key, pdih) ;
	pdipDevType(pdip) |= DEVICE_TYPE_PSEUDO ;
	snAddChild(pdih, pdip) ;
	return 1 ;
}

int
ioc3_AddChild(pd_dev_info_t *pdev, vertex_hdl_t v, dev_inst_info_t *devii)
{
        char            path[128] ;

	strcpy(path, "ioc3") ;
        snAddChildPostOp(pdev, v, path, (klinfo_t *)NULL, devii) ;
	return 1 ;
}

int
ioc3_tty_AddChild(pd_dev_info_t *pdev, vertex_hdl_t v, dev_inst_info_t *devii)
{
        char            path[128] ;
        klinfo_t        *k ;
	vertex_hdl_t	dev_v ;

        k = sn_make_dev_klcfg(pdev, v, KLSTRUCT_IOC3UART, &ttyptmpl) ;
	if (k) {
		k->physid = devii->devii_unit ;
		k->virtid = devii->devii_unit ;
	}
	sprintf(path, "tty/%d", devii->devii_unit + 1) ;
        dev_v = snAddChildPostOp(pdev, v, path, (klinfo_t *)k, devii) ;

	/* Add some aliases for backward compat */
	sprintf(path, "ioc3%d", devii->devii_unit) ;
	pg_edge_add_path("/dev/tty", dev_v, path) ;

	return 1 ;
}

int
ioc3_ei_AddChild(pd_dev_info_t *pdev, vertex_hdl_t v, dev_inst_info_t *devii)
{
        char            path[128] ;
	vertex_hdl_t	dev_v ;

	dev_v = snDevAddChild(pdev,v,devii,"ei",KLSTRUCT_IOC3ENET,&netdev) ;

        /* Add some aliases for backward compat */
        sprintf(path, "%d", devii->devii_unit) ;
        pg_edge_add_path("/dev/network", dev_v, path) ;

        return 1 ;
}

#define DEV_STAT_KBD_PRESENT	0x1
#define DEV_STAT_MS_PRESENT	0x2

int
ioc3_pckm_AddChild(pd_dev_info_t *pdev, vertex_hdl_t v, dev_inst_info_t *devii)
{
	char 		path[128] ;
	vertex_hdl_t	dev_v ;

	if (devii->devii_status & DEV_STAT_KBD_PRESENT) {
                dev_v = snDevAddChild(pdev, v, devii, 
                                "pckb", KLSTRUCT_IOC3PCKM, &kbdtempl) ;
        	/* Add some aliases for backward compat */
        	sprintf(path, "ioc3pckm%d", devii->devii_unit) ;
        	pg_edge_add_path("/dev/input", dev_v, path) ;

	}
	else if (devii->devii_status & DEV_STAT_MS_PRESENT) {
                dev_v = snDevAddChild(pdev, v, devii, 
                                "pcms", KLSTRUCT_IOC3MS, &sgims) ;
        	/* Add some aliases for backward compat */
        	sprintf(path, "ioc3ms%d", devii->devii_unit) ;
        	pg_edge_add_path("/dev/input", dev_v, path) ;
	}
	else
		return 1 ;

	return 1 ;
}
#endif

