/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef __INCLUDE_PGDRV_H__
#define __INCLUDE_PGDRV_H__

#ident	"$Revision: 1.8 $"

#if 0
#define SN_PDI			1	/* Prom Driver Infrastructure */
#define SN_PIG			SN_PDI	/* Prom Infrastructure Graph */
#endif

/*
 * PROM GRAPH Device Driver related structures
 * 
 * XXX - Some of the fields in the structs defined
 * in this file below, are not needed in the final
 * code. They are here because all sections of the
 * prom like drivers, hinv could not be changed at
 * once and for temp backward compatibility.
 */

/*
 * prom_drv_t
 *
 * Prom device driver interface.
 * These are the callback functions that can be
 * provided by a prom driver. If there is no 
 * routine provided for a particular driver, the
 * prom uses the default.
 * Defaults are in libsk/ml/pgdrv.c
 */

typedef struct prom_drv_s {
	int		key1 ;
	int		key2 ;
	char		*strkey ;
	unsigned int	flag ;
	void		*(*init_dev_sncfg)() ;	/* for devices found */
	int		(*reserved_cb)() ;
	void		*(*make_inst_parm)() ;	/* install parameter */
	void		(*install)() ;		/* install driver */
	int		(*check_install)() ;	/* Can we install? */
	int		(*_strat)() ;		/* Strategy */
	void 		(*sn_add_child)() ;	/* Adds to prom graph */
	int		(*get_dma_addr)() ;	/* dma map routine */
	void 		*link_out_ptrs ;	/* generate stubs ? */
	void 		*link_in_ptrs ;	
	void		*reserved ;
} prom_drv_t ;

/*
 * pgGetDrvTabPtrIndx
 *
 * Search for a prom devsw entry based on the keys given.
 */

prom_drv_t	*pgGetDrvTabPtrIndx(int, int, char *, int *) ;

/*
 * pgDrvTabInit
 *
 * Driver table is initted from the routines in
 * libsk/ml/pgdrvinst.c
 */

void		pgDrvTabInit(void) ;

/*
 * pd_info_t
 *
 * This is created for every controller discovered
 * whose key matches, a driver in the prom drv table.
 * Hangs off of the controller vertex as INFO_LBL_DRIVER
 * /hw/module/X/slot/YY/<name>/pci/slot/ZZ
 */

typedef struct pd_info_s {
	prom_drv_t	*pdi_devsw_p ;		/* ptr to driver table entry */
	int		pdi_devsw_i  ;		/* indx to driver table entry */
	int		pdi_flags    ;		/* INSTALL, CONTROLLER ... */
#define PDI_INSTALLED	0x1
#define PDI_PSEUDO_DEV	0x2
	lboard_t	*pdi_lb      ;		/* klcfg entry for the board */
	klinfo_t	*pdi_k       ;		/* klcfg for the component */
	int		pdi_ctrl_id  ;		/* ctrl number */
	COMPONENT	pdi_ctrl_ac  ; 		/* ARCS COMPONENT for ctrl */
} pd_info_t ;

#define IS_PD_INSTALLED(pdi)	(pdi->pdi_flags & PDI_INSTALLED)

#define pdiDevSwP(_p)		(_p->pdi_devsw_p)
#define pdiLb(_p)		(_p->pdi_lb)
#define pdiK(_p)		(_p->pdi_k)
#define pdiCtrlId(_p)		(_p->pdi_ctrl_id)
#define pdiArcsComp(_p)		(_p->pdi_ctrl_ac)

#define pdiInitDevSncfg(_p) 	(_p->pdi_devsw_p->init_dev_sncfg)
#define pdiMakeInstParm(_p) 	(_p->pdi_devsw_p->make_inst_parm)
#define pdiInstall(_p) 		(_p->pdi_devsw_p->install)
#define pdiStrat(_p) 		(_p->pdi_devsw_p->_strat)
#define pdiCheckInstall(_p) 	(_p->pdi_devsw_p->check_install)
#define pdiSnAddChild(_p) 	(_p->pdi_devsw_p->sn_add_child)
#define pdiGetDmaAddr(_p) 	(_p->pdi_devsw_p->get_dma_addr)

/*
 * pd_inst_hdl_t
 *
 * A handle passed to the driver install routine by the PROM.
 * It has to be used by the driver in the snAddChild 
 * call back.
 */

typedef struct pd_inst_hdl_s {
	int		pdih_magic   ;		/* valid handle ? */
#define PDIH_MAGIC	0x747370
	vertex_hdl_t	pdih_ctrl_v  ;		/* attach device hwg path */
	pd_info_t	*pdih_pdi    ;		/* ctrl's pdi */
} pd_inst_hdl_t ;

#define pdihCtrlV(_p)	(_p->pdih_ctrl_v)
#define pdihPdi(_p)	(_p->pdih_pdi)

/*
 * dev_inst_info_t
 *
 * device install info. passed by driver install routine to
 * snAddChild. The arcs struct COMPONENT has enough info
 * to describe a device. The other values are numbers assigned
 * by the driver. This can be driver specific based on 
 * COMPONENT type field.
 */
 
typedef struct dev_inst_info_s {
	COMPONENT	devii_c ; 		/* dev info - type ntracks */
	uchar		devii_type ;		/* device type */
#define	DEVICE_TYPE_BLOCK	0x10
#define	DEVICE_TYPE_CHAR 	0x20
#define	DEVICE_TYPE_PSEUDO 	0x40
	int		devii_status ;		/* dev probe status */
	int		devii_unit ;		/* unit/target number */
	int		devii_lun ;		/* scsi specific lun */
	int		devii_partition ;	/* block dev specific part. */
	int		devii_gfxid ;		/* gfx specific */
} dev_inst_info_t ;

#define GET_DEV_CLASS(_t)	DEVICE_TYPE_BLOCK

/*
 * drv_inst_info_t
 *
 * Driver specific info to be passed to the install routine.
 * dev and drv info make up the install parameter. 
 */

#define MAX_HWG_LEN		128
#define MAX_HW_MOD_SLOT_LEN  	MAX_HWG_LEN

typedef struct drv_inst_info_s {
        char            drvii_hwg[MAX_HWG_LEN] ; /* backward compat */
        int             drvii_ctrl_id  ;         /* future use */
        __psunsigned_t  drvii_cfg_base ;         /* pci cfg space */
        __psunsigned_t  drvii_mem_base ;         /* pci mem space */
        ULONG           drvii_dma_parm ;         /* COMPONENT->Key */
} drv_inst_info_t ;

/*
 * pd_inst_parm_t
 *
 * This is passed as a parameter to the install routine.
 * Created by the driver specific make Install parm routine.
 * By default the prom creates the struct below.
 */

typedef struct pd_inst_parm_s {
	drv_inst_info_t pdip_drvii    ;		/* Driver info */
	dev_inst_info_t pdip_devii    ;		/* Device */
} pd_inst_parm_t ;

#define pdipHwg(_p)		(_p->pdip_drvii.drvii_hwg)
#define pdipCtrlId(_p)		(_p->pdip_drvii.drvii_ctrl_id)
#define pdipCfgBase(_p)		(_p->pdip_drvii.drvii_cfg_base)
#define pdipMemBase(_p)		(_p->pdip_drvii.drvii_mem_base)
#define pdipDmaParm(_p)		(_p->pdip_drvii.drvii_dma_parm)

#define pdipDevC(_p)		(_p->pdip_devii.devii_c)
#define pdipDevType(_p)		(_p->pdip_devii.devii_type)
#define pdipDevStatus(_p)	(_p->pdip_devii.devii_status)
#define pdipDevUnit(_p)		(_p->pdip_devii.devii_unit)
#define pdipDevLun(_p)		(_p->pdip_devii.devii_lun)
#define pdipDevDskPart(_p)	(_p->pdip_devii.devii_partition)
#define pdipDevGfxId(_p)	(_p->pdip_devii.devii_gfxid)

#define IS_DEVICE_PSEUDO(_p)	(pdipDevType(_p) & DEVICE_TYPE_PSEUDO)

/*
 * pd_drv_parm_t
 *
 * Driver parameters. Needed by the prom to invoke
 * a device driver. It is filled into the eiob struct.
 */

typedef struct prom_drv_parm_s {
	uchar		pdp_type ;
	int		pdp_ctrl ; 	/* controller number */
	int		pdp_unit ;	/* Unit/target */
	int		pdp_lun ; 	/* logical unit number - scsi only */
	int		pdp_part ;	/* partition disk devices only */
	int		pdp_gfx ; 	/* gfx related info */
} pd_drv_parm_t ;

/*
 * pd_dev_info_t 
 *
 * This contains all info about a device.
 * Hangs as INFO_LBL_DEVICE off of the endpoint 
 * that the driver specific AddChild routine 
 * returns. From the hwg path given in the 
 * command line, the parse routine looks up this
 * vertex, reads this label and passes the values from
 * this struct to various prom fs routines.
 */ 

#define INFO_LBL_DEVICE	"_device"
typedef struct pd_dev_info_s {
	pd_info_t	pdev_pdi ;
	pd_drv_parm_t 	pdev_pdp ;
	dev_inst_info_t pdev_devii ;
} pd_dev_info_t ;

#define pdevPdiInitSncfg(_p)	(pdiInitDevSncfg((pd_info_t *)(&_p->pdev_pdi)))
/*
 * PROM Driver Support
 *
 * The following set of routines are available to
 * the prom driver. Some of them have to be called
 * and some are generic interfaces that can be used
 * if the driver does not want to do anything special.
 */

/*
 * snAddChild
 *
 * This routine is called by the prom device driver
 * for every device found, with the handle passed to
 * it, and a ptr to the device info found.
 */

void 		snAddChild(pd_inst_hdl_t *, pd_inst_parm_t *) ;

/*
 * snAddChildPostOp
 *
 * This routine has to be called by the device
 * specific AddChild routine for every vertex
 * it wants to create. For example, a disk driver
 * may want to create 1 vertex for partition/0, 1, ...
 */

vertex_hdl_t 	snAddChildPostOp(	pd_dev_info_t         *pdev,
                 			vertex_hdl_t           dev_v,
                 			char                   *path,  
                 			klinfo_t               *k,
                 			dev_inst_info_t        *dii) ;

/*
 * Make a COMPONENT->Key field. Previous drivers would pass
 * various info to the driver install routine through this
 * field. Ref SN/SN0/addrs.h for details.
 */

ULONG 		make_COMP_Key(klinfo_t *, int) ;

/* Create a buf of size and zero it. Like calloc. */

void* 		pgCreatePd(int size) ;

/* 
 * Make drv handle, check install, call install. Needed by
 * composite install routines like ioc3.
 */

void * 		pgDoInstall(vertex_hdl_t, pd_info_t *) ;

/* Generic AddChild routine */

vertex_hdl_t	snDevAddChild(	pd_dev_info_t   *pdev   ,
                        	vertex_hdl_t    v       ,
                        	dev_inst_info_t *devii  ,
                        	char            *path   ,
                        	int             type    ,
                        	COMPONENT       *c) ;

/* 
 * Perform the 'install only the first instance of a driver' thing.
 * Can be a generic check install.
 */

int		check_install_once(int *) ;

/* Generic make klconfig for device. */

klinfo_t* 	sn_make_dev_klcfg(	pd_dev_info_t 	*pdev	,
					vertex_hdl_t	v	,
					int 		type	,
					COMPONENT 	*c	) ;

#if 0
/* static in pgdrv.c */
/* init device graph, basically init klcfg for a device */

klinfo_t* 	sn_init_device_graph(	pd_dev_info_t 	*pdev	,
					vertex_hdl_t 	ctlr_v	,
					int 		dev_type) ;
#endif

#endif /* __INCLUDE_PGDRV_H__ */
