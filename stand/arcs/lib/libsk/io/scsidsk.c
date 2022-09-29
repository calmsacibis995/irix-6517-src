/*
 * SN0 - IO6prom
 * scsidsk.c
 * 
 * Driver support routines for installing devices on qlogic SCSI 
 * Controllers in standalone. 
 * 'prom_install_devices' calls QLOGIC's 'install' routine viz 
 * 'kl_qlogic_install'. This probes the scsi Bus and adds the 
 * drives/devices to the PROMGRAPH.
 */

#include <sys/SN/klconfig.h>   /* config stuff */
#include <sys/PCI/bridge.h>
#include <sys/SN/addrs.h>
#include <sys/graph.h>            
#include <sys/hwgraph.h>          
#include <sys/SN/promcfg.h>
#include <arcs/cfgdata.h>
#include <sys/SN/SN0/klhwinit.h>
#include <libsc.h>
#include <libkl.h>
#include <promgraph.h>
#include <pgdrv.h>

#ifdef SN_PDI
#define PROM_EDGE_LBL_QL_TARGET "scsi_ctlr/0/target/"
#else
#define PROM_EDGE_LBL_QL_TARGET "scsi_ctlr/0/target"
#endif

void kl_qlogic_install_later(vertex_hdl_t, vertex_hdl_t) ;

void * malloc(size_t ) ;
extern void qlconfigure(COMPONENT *, char *, char *);
extern int dksc_strat(COMPONENT *self, IOBLOCK *iob);
extern int tpsc_strat(COMPONENT *self, IOBLOCK *iob);
extern int _ns16550_strat(COMPONENT *self, IOBLOCK *iob);
extern vertex_hdl_t hw_vertex_hdl ;
extern graph_hdl_t prom_graph_hdl ;

int num_qlogic ;

union scsicfg {
        struct scsidisk_data disk;
        struct floppy_data fd;
};

static COMPONENT scsiadaptmpl = {
    AdapterClass,               /* Class */
    SCSIAdapter,                /* Type */
    0,                          /* Flags */
    SGI_ARCS_VERS,              /* Version */
    SGI_ARCS_REV,               /* Revision */
    0,                          /* Key */
    0x01,                       /* Affinity */
    0,                          /* ConfigurationDataSize */
    10,                         /* IdentifierLength */
    "QLISP1040"                 /* Identifier */
};

#ifdef SN_PDI
void
kl_qlogic_install(vertex_hdl_t hw_vhdl, vertex_hdl_t devctlr_vhdl)
{
}
#else
void
kl_qlogic_install(vertex_hdl_t hw_vhdl, vertex_hdl_t devctlr_vhdl) 
{
	kl_register_install(hw_vhdl, devctlr_vhdl, kl_qlogic_install_later) ;
}

void
kl_qlogic_install_later(vertex_hdl_t hw_vhdl, vertex_hdl_t devctlr_vhdl) 
{
	int 		i = 0, ctlr_num ; 
	COMPONENT 	*arcs_compt ;
	graph_error_t	graph_err ;
	klinfo_t	*ctrkli_ptr ;
	__psunsigned_t 	hub_base ;
	unsigned char 	hubwid ;
        prom_dev_info_t *dev_info;
	char		hwgpath[64] ;
	char		sname[SLOTNUM_MAXLENGTH] ;
	char		*nicstr ;

	/* Make up a COMPONENT struct and pass it to qlconfigure. kludge */

	/* Fill up ARCS info and put it in klinfo_t of the device */

        arcs_compt = (COMPONENT *)malloc(sizeof(COMPONENT)) ;
        if (arcs_compt == NULL) {
                printf("qdsk inst : malloc failed \n") ;
                return ;
        }

	arcs_compt->Key          = num_qlogic++ ;
	arcs_compt->AffinityMask = (ULONG)devctlr_vhdl ;

        /* Call to register strategy routine for controller */

        graph_err = graph_info_get_LBL(prom_graph_hdl, devctlr_vhdl,
                                       INFO_LBL_DEV_INFO,NULL,
                                       (arbitrary_info_t *)&dev_info) ;
        if (graph_err != GRAPH_SUCCESS) {
          	printf("Qlogic install: cannot get dev_info\n");
          	return ;
        }

        kl_reg_drv_strat(dev_info, dksc_strat) ;
	get_slotname(dev_info->slotid, sname) ;
	sprintf(hwgpath, "/hw/module/%d/slot/%s", dev_info->modid, sname) ;

	ctrkli_ptr         = dev_info->kl_comp ;
	ctrkli_ptr->virtid = arcs_compt->Key ;

	arcs_compt->Key |= MK_SN0_KEY(ctrkli_ptr->nasid, 
			ctrkli_ptr->widid, ctrkli_ptr->physid) ;

	if (ctrkli_ptr->nasid)
		hub_base = (__psunsigned_t) REMOTE_HUB(ctrkli_ptr->nasid, 0) ;
	else
		hub_base = (__psunsigned_t) LOCAL_HUB(0) ;

	hubwid = GET_FIELD64((hub_base + IIO_WIDGET_CTRL), WIDGET_ID_MASK, 0) ;
	ADD_HUBWID_KEY(arcs_compt->Key, hubwid) ;
	ADD_HSTNASID_KEY(arcs_compt->Key, get_nasid()) ;

	/* ----------- NEW -------------- */

	/* create the graph segment "ql/target". This hangs off
           /pci/slot/<number>/
 	*/

	create_graph_path(devctlr_vhdl, PROM_EDGE_LBL_QL_TARGET, 1) ;

	/* Add info labelled "link" to the controller vertex handle.
	   This is later used by promgraph build to extend the graph.
	*/

	graph_err = graph_info_add_LBL(prom_graph_hdl, devctlr_vhdl, 
             				INFO_LBL_LINK, NULL,
					(arbitrary_info_t)
					PROM_EDGE_LBL_QL_TARGET) ;
	if (graph_err != GRAPH_SUCCESS) {
		printf("kl_ql_inst: addLBL error %d\n", graph_err) ;
		return ;
	}

	/* qlconfigure calls ql_init which uses the ->Key as the 
	   adapter number.
	*/

	nicstr=get_nicstr_from_devinfo(dev_info->lb) ;
	qlconfigure(arcs_compt, hwgpath, nicstr) ;
}

#endif

static klscdev_t *
make_dev_klcfg(pd_dev_info_t *pdev, vertex_hdl_t dv, char type, COMPONENT *ac)
{
        klscdev_t       *sd;
	char 		*ctmp ;

#ifdef SN_PDI
        sd = (klscdev_t *)sn_make_dev_klcfg(pdev, dv, type, ac) ;
#else
        sd = (klscdev_t *)init_device_graph(dv, type) ;
#endif
	if (sd == NULL)
		return sd ;

        bcopy(ac, sd->scdev_info.arcs_compt, sizeof(COMPONENT));
        sd->scdev_info.arcs_compt->Class = PeripheralClass ;
        sd->scdev_info.arcs_compt->Version = SGI_ARCS_VERS ;
        sd->scdev_info.arcs_compt->Revision = SGI_ARCS_REV ;

        ctmp = malloc(ac->IdentifierLength + 1) ;
        strncpy(ctmp, ac->Identifier, ac->IdentifierLength) ;
        ctmp[ac->IdentifierLength] = 0 ;
        sd->scdev_info.arcs_compt->Identifier = ctmp ;

        sd->scdev_info.physid = (ac->Key & 0xf) ; /* target */

	return sd ;
}

struct {
	char 	type ;
	char 	*name ;
	int     (*strat)() ;
} si ;

void
get_scsiinfo(int type)
{
	switch(type) {
                case CDROMController:
			si.name = "cdrom" ;
                        si.type = KLSTRUCT_CDROM ;
			si.strat = dksc_strat ;
                	break ;

                case TapePeripheral:
			si.name = "tape" ;
                        si.type = KLSTRUCT_TAPE ;
			si.strat = tpsc_strat ;
                	break ;

                default:
			si.name = "disk" ;
                        si.type = KLSTRUCT_DISK ;
			si.strat = dksc_strat ;
                	break ;
	}
}

#ifndef SN_PDI

kl_qlogic_AddChild(COMPONENT *arcs_compt, union scsicfg *cfgd)
{
	klscdev_t 	*sd;
	vertex_hdl_t	 devctlr_vhdl = arcs_compt->AffinityMask ;
	
	/* At this point we have found a disk. Need to init its klcfg */

	get_scsiinfo(arcs_compt->Type) ;

	if ((sd = make_dev_klcfg(NULL, devctlr_vhdl, si.type, arcs_compt)) == NULL)
		return 0 ;

	link_device_to_graph(hw_vertex_hdl, devctlr_vhdl,
				(klinfo_t *)sd, si.strat) ;

	return 1 ;
	
}
#endif /* !SN_PDI */

#ifdef SN_PDI

extern graph_hdl_t      pg_hdl ;

extern int	_symmon ;

extern moduleid_t      master_baseio_modid ;
extern slotid_t        master_baseio_slotid ;

void *
ql_make_inst_param(drv_inst_info_t *drvii, lboard_t *lb, klinfo_t *k)
{
	ULONG	Key ;

	drvii->drvii_ctrl_id 	= num_qlogic++ ;
	make_hwg_path(lb->brd_module, lb->brd_slot, drvii->drvii_hwg) ;
	Key 			= make_COMP_Key(k, drvii->drvii_ctrl_id) ;
	drvii->drvii_cfg_base	= GET_PCICFGBASE_FROM_KEY(Key);
	drvii->drvii_mem_base	= GET_PCIBASE_FROM_KEY(Key);
	drvii->drvii_dma_parm	= Key ;

        /* XXX this is a side effect and needs to be cleared */
        k->virtid       	= drvii->drvii_ctrl_id ;
	k->arcs_compt		= (COMPONENT *)malloc(sizeof(COMPONENT)) ;
	bcopy(&scsiadaptmpl, k->arcs_compt, sizeof(COMPONENT)) ;

	return (void *)drvii ;
}

int
ql_check_install(lboard_t *lb, klinfo_t *k)
{
	char                *p = NULL ;
	int                 mod, slot, ms ;

	mod=slot=0 ;

	if (_symmon)
		return 0 ;

	if (!(k->flags & KLINFO_ENABLE))
		return 0 ;

	if ((p=getenv("ProbeAllScsi")) && p && ((*p == 'y') || (*p == '1')))
		return 1 ;

	if ((lb->brd_module == master_baseio_modid) &&
            (lb->brd_slot   == master_baseio_slotid))
		return 1 ;

	if ((p=getenv("ProbeWhichScsi")) && p) {
		if ((ms = check_console_path(p)) > 0) {
            		mod  = ms >> 16 ;
            		slot = ms & 0xffff ;
        	}
                if ((lb->brd_module == mod) &&
                        ((SLOTNUM_GETSLOT(lb->brd_slot)) == slot))
			return 1 ;
	}

	return 0 ; 
}

void
ql_AddChild(pd_dev_info_t *pdev, vertex_hdl_t v, dev_inst_info_t *devii)
{
	char		path[128] ;
	COMPONENT 	*c = &devii->devii_c ;
	char		*dname ;
	vertex_hdl_t	v1, v2 ;
	klscdev_t	*sd ;

	/* XXX get this name / type filled up in ql.c */

	v1 = v ;

	/* get device type */
	*path = 0 ; 
 	get_scsiinfo(c->Type) ;
	dname = si.name ;

	/* get the hwgraph path */
	sprintf(path, PROM_EDGE_LBL_QL_TARGET "%d"
		      "/lun/0/", (c->Key & 0xf)) ;

	if (si.type == KLSTRUCT_CDROM) {
		path[strlen(path)-1] = 0 ;
		v2 = create_graph_path(v, path, 0) ;
		v1 = create_graph_path(v2, dname, 0) ;
		graph_edge_add(pg_hdl, v2, v1, "disk") ;
		*path = 0 ;
	} else
		strcat(path, dname) ;

	/* strcat(path, "/partition/0/block") ; */
	/* XXX fill up devii->lun and devii->partition 
	   if we are creating different parts */

	/* get device klcfg */
	sd = make_dev_klcfg(pdev, v, si.type, c) ;

	if (sd != NULL) {
		snAddChildPostOp(pdev, v1, path, (klinfo_t *)sd, devii) ;
	}
}
#endif


