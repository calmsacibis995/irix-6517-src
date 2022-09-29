/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.11 $"

/* 
 * pgdrv.c - promgraph driver interface.
 */

#include <sys/types.h>

#include <sys/PCI/bridge.h>  	/* for the GET_PCI stuff ... */
#include <sys/SN/addrs.h>

#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <promgraph.h>
#include <sys/iograph.h>
#include <sys/SN/SN0/klhwinit.h>
#include <arcs/hinv.h>
#include <arcs/eiob.h>
#include <prom_msgs.h>

#include <libsc.h>
#include <pgdrv.h>

#ifdef SN_PDI

extern graph_hdl_t      pg_hdl ;
extern vertex_hdl_t	snpdi_rvhdl ;

#define REPORT_ERR	1

/*
 * promDrvTab
 *
 * This table contains info about all drivers
 * configured into the prom - like the unix
 * 'devsw' tables.
 */

#define 	MAX_PROM_DRIVERS	32
prom_drv_t      *promDrvTab[MAX_PROM_DRIVERS] ; 

/* Driver interface routines vector definitions. */

#include "pgdrvinst.c"

/*
 * This routine creates the 'devsw' tables for PROM.
 */

void
pgDrvTabInit(void)
{
	int 	i ;

	for (i=0;i<MAX_PROM_DRIVERS;i++)
		promDrvTab[i] = 0 ;

	/* NOTE : promDrvTab[SHOULD NOT EXCEED MAX_PROM_DRIVERS] */

	promDrvTab[0] = &ql_drv_info ;
	promDrvTab[1] = &ioc3_drv_info ;
	promDrvTab[2] = &ef_drv_info ;
	promDrvTab[3] = &tty_drv_info ;
	promDrvTab[4] = &pckm_drv_info ;
#if defined(SN0PROM) || defined(SN0XXLPROM)
	promDrvTab[5] = &kona_drv_info ;
	promDrvTab[6] = &mgras_drv_info ;
#endif
	/* Add new drivers here */
}

/*
 * Search the prom registered driver table for an
 * entry based on the keys.
 */

int
pgGetDrvTabIndex(int key1, int key2, char *strkey)
{
        prom_drv_t      **pdp ;
        int             indx = 0 ;
        int             kp1, kp2, kp3 ;         /* key present? */
        int             kc1, kc2, kc3 ;         /* key condition */

        kp1 = (key1 != -1) ;
        kp2 = (key2 != -1) ;
        kp3 = (strkey != NULL) ;

        if (!(kp1 || kp2 || kp3))
                return -1 ;

        pdp = promDrvTab ;

        while (*pdp) {
                prom_drv_t      *pd ;

                pd = *pdp ;

                if (kp1)
                        kc1 = (key1 == pd->key1) ;
                else
                        kc1 = 1 ;

                if (kp2)
                        kc2 = (key2 == pd->key2) ;
                else
                        kc2 = 1 ;

                if (kp3)
                        kc3 = ((pd->strkey) && !strcmp(strkey, pd->strkey)) ;
                else
                        kc3 = 1 ;

                if (kc1 && kc2 && kc3) {
                        return indx ;
                }

                pdp ++ ;
                indx ++ ;
        }
        return -1 ;
}

/*
 * Get both the prom driver entry pointer and index.
 */

prom_drv_t *
pgGetDrvTabPtrIndx(int key1, int key2, char *strkey, int *indxp)
{
	int 	indx ;

	if ((indx = pgGetDrvTabIndex(key1, key2, strkey)) < 0)
		return NULL ;

	*indxp = indx ;
	return(promDrvTab[indx]) ;
}

/*
 * make_COMP_Key
 * 
 * A Key is a ULONG that has info about a IO device 
 * encoded in it, like widget id, nasid, hub widget id.
 * Used to calculate a dma addr for a pci device.
 */

ULONG
make_COMP_Key(klinfo_t *k, int ctrl_id)
{
        __psunsigned_t  hb ;
        int             hw ;
	ULONG		Key ;

        Key          	= 0 ;
	Key		|= ctrl_id ;
        Key         	|= MK_SN0_KEY(k->nasid, k->widid, k->physid) ;

        hb = (__psunsigned_t) REMOTE_HUB(k->nasid, 0) ;

        hw = GET_FIELD64((hb + IIO_WIDGET_CTRL), WIDGET_ID_MASK, 0) ;
        ADD_HUBWID_KEY(Key, hw) ;
        ADD_HSTNASID_KEY(Key, get_nasid()) ;

	return Key ;
}

/*
 * pgCreatePd
 *
 * Create a prom driv struct and zero it.
 */

void *
pgCreatePd(int size)
{
	void 	*tmp ;

	tmp = (void *)malloc(size) ;
	if (tmp != NULL)
		bzero(tmp, size) ;
	return tmp ;
}

int sn_pass ;	/* Checks for multiple inits */

/*
 * pgDevInit
 *
 * Calls init routine of all drivers.
 */

int
pgDevInit(void *arg, vertex_hdl_t vhdl)
{
	pd_info_t		*pdi ;
        register struct eiob 	*io  ;
	graph_error_t		ge   ;

	pdi = (pd_info_t *)pg_get_lbl(vhdl, INFO_LBL_DRIVER) ;
	if (pdi == NULL) 
		return 0 ;

	if (!IS_PD_INSTALLED(pdi))
		return 0 ;

	if ((io = new_eiob()) == NULL)
		return 1 ;

        io->iob.Controller   = pdi->pdi_ctrl_id ;
        io->iob.FunctionCode = FC_INITIALIZE;
        io->iob.Count        = sn_pass ;

	if (pdiStrat(pdi))
		if ((*pdiStrat(pdi))(&pdiArcsComp(pdi), &io->iob) != 0)
			/* XXX check if ac->flag is set */ ;
	/* 
	 * The flag of the device needs to be set to fail if
	 * init failed. This is because, when we Open a device the
	 * INFO_LBL_DEVICE gets opened. But here we have the klinfo_t
	 * of the driver.
	 * This flag is checked in parse_path. Since on SN, the
 	 * driver inits are fairly simple and are mostly successful
 	 * we do not set this flag and let Open continue. It will
	 * encounter a problem somewhere else, if there is a real
	 * hw failure. Also, in critical cases like console, if init
	 * has a problem we have to panic.
 	 */

	/*
 	 * In the SN prom there is no place to call tpsc init and 
	 * tpsc uses the same ql driver, but different processing.
	 * So, call tpsc_strat here if we are dealing with dksc.
	 */
#if 0
	if (pdiStrat(pdi) == dksc_strat) {
		tpsc_strat(&pdiArcsComp(pdi), &io->iob) ;
	}
#endif

	free_eiob(io) ;

	return 0 ; /* continue for all nodes */
}

void
sn_init_devices(void)
{
        vertex_hdl_t    nv ;
        graph_error_t   ge ;

	printf("sn Initializing devices...\n") ;

	sn_pass ++ ;
        graph_vertex_visit(pg_hdl, pgDevInit, &snpdi_rvhdl,
                                &ge, &nv) ;
		
}

/*
 * pgInitPdih
 *
 */

void
pgInitPdih(pd_inst_hdl_t *pdih, vertex_hdl_t v, pd_info_t *pdi)
{
        pdih->pdih_magic 	= PDIH_MAGIC ;
        pdih->pdih_ctrl_v 	= v ;
        pdih->pdih_pdi    	= pdi ;
}

pd_inst_hdl_t *
pgMakeInstHdl(vertex_hdl_t v, pd_info_t *pdi)
{
	pd_inst_hdl_t	*pdih ;

        pdih = (pd_inst_hdl_t *)pgCreatePd(sizeof(pd_inst_hdl_t)) ;
	if (pdih)
		pgInitPdih(pdih, v, pdi) ;
	return pdih ;
}

void *
make_inst_parm(drv_inst_info_t *drvii, lboard_t *lb, klinfo_t *k)
{
        ULONG   Key ;

        drvii->drvii_ctrl_id    = 0 ;
        Key                     = make_COMP_Key(k, drvii->drvii_ctrl_id) ;
        drvii->drvii_cfg_base   = GET_PCICFGBASE_FROM_KEY(Key);
        drvii->drvii_mem_base   = GET_PCIBASE_FROM_KEY(Key);
        drvii->drvii_dma_parm   = Key ;

#if 0
        /* XXX this is a side effect and needs to be cleared */
        k->virtid               = drvii->drvii_ctrl_id ;
        k->arcs_compt           = (COMPONENT *)malloc(sizeof(COMPONENT)) ;
#endif

        return (void *)drvii ;
}

pd_inst_parm_t *
pgMakeInstParm(pd_info_t *pdi)
{
	pd_inst_parm_t	*pdip ;

        pdip = (pd_inst_parm_t *)pgCreatePd(sizeof(pd_inst_parm_t)) ;
	if (pdip == NULL)
		goto MIP_done ;

	/* assigns ctrl id */
	if (pdiMakeInstParm(pdi)) {
		(*pdiMakeInstParm(pdi))(&pdip->pdip_drvii, 
					pdiLb(pdi), pdiK(pdi)) ;
	}
	else {
		make_inst_parm(&pdip->pdip_drvii, pdiLb(pdi), pdiK(pdi)) ;
	}

	/* back propagate the ctrl_id and other stuff to the pdi */
	if (pdiMakeInstParm(pdi)) {
		pdiCtrlId(pdi) 		= pdipCtrlId(pdip) ;
		if (pdiK(pdi)->arcs_compt)
			bcopy(pdiK(pdi)->arcs_compt, 
				&pdiArcsComp(pdi), sizeof(COMPONENT)) ;
	}

MIP_done:
	return pdip ;
}

int
gen_inst_check(klinfo_t	*k)
{
	if (	(k->flags & (KLINFO_INSTALL|KLINFO_ENABLE)) ==
		(KLINFO_INSTALL|KLINFO_ENABLE))
		return 1 ;
	return 0 ;
}

/*
 * pgCallInstall
 *
 * Called to install a driver for a controller.
 * Called only if the check install callback 
 * returns OK.
 */

void *
pgDoInstall(vertex_hdl_t v, pd_info_t *pdi)
{
        pd_inst_hdl_t   *pdih   ;
        pd_inst_parm_t  *pdip   ;

	if (!gen_inst_check(pdiK(pdi)))
		return NULL ;

	/* 
	 * If the user has supplied a check_install routine use it.
	 * The default is to go ahead and install.
	 */

        if (pdiCheckInstall(pdi))
                if (!(*pdiCheckInstall(pdi))(pdiLb(pdi), pdiK(pdi)))
                        return NULL ;

        /* Make inst handle */
        pdih = pgMakeInstHdl(v, pdi) ;
        if (pdih == NULL) {
                return NULL ;
        }

        /* Make install parm */
        pdip = (pd_inst_parm_t *)pgMakeInstParm(pdi) ;
        if (pdip == NULL) {
                return NULL ;
        }

        (*pdiInstall(pdi))(pdih, pdip) ;

	free(pdih) ;
	free(pdip) ;

	return (void *)pdi ;
}

int
pgCallInstall(void *arg, vertex_hdl_t v)
{
	pd_info_t	*pdi 	;

	pdi = (pd_info_t *)pg_get_lbl(v, INFO_LBL_DRIVER) ;
	if (pdi == NULL)
		return 0 ;

	if (pgDoInstall(v, pdi) == NULL)
		return NULL ;

	/* Set status of the driver */

	pdi->pdi_flags |= PDI_INSTALLED ;

        return 0 ;
}

void
pgDriverRegister()
{
	vertex_hdl_t	nv ;
	graph_error_t	ge ;

	graph_vertex_visit(pg_hdl, pgCallInstall, 
			   &snpdi_rvhdl, &ge, &nv) ;
}

pd_dev_info_t *
pgMakePdev(pd_inst_hdl_t *pdih, pd_inst_parm_t *pdip)
{
        pd_dev_info_t   *pdev ;
	dev_inst_info_t	*devii = (dev_inst_info_t *)&pdip->pdip_devii ;

	/* alloc a dev info struct */

	pdev = (pd_dev_info_t *)pgCreatePd(sizeof(pd_dev_info_t)) ;
	if (!pdev)
		goto MPDEV_done ;

	/* Copy the pdi of the controller */

	bcopy(pdih->pdih_pdi, &pdev->pdev_pdi, sizeof(pd_info_t)) ;

	/* zero the pdi_k ptr. It was pointing to the ctrl klinfo */

	pdev->pdev_pdi.pdi_k = 0 ;

	/* controller id from inst handle */
	pdev->pdev_pdp.pdp_ctrl = pdih->pdih_pdi->pdi_ctrl_id ;

	/* unit from dev inst info */
	pdev->pdev_pdp.pdp_type = GET_DEV_CLASS(devii->devii_c.Type) ;
	pdev->pdev_pdp.pdp_unit = devii->devii_unit ;
	pdev->pdev_pdp.pdp_lun  = devii->devii_lun  ;
	pdev->pdev_pdp.pdp_part = 0 ;

	pdev->pdev_devii  	= *devii ;	/* struct assign */

MPDEV_done:
	return pdev ;
}

/*
 * snAddChild
 *
 * Called by the device driver install routine for every
 * device found. The driver add child needs to grow a
 * fancy path for the device from ctrl_v, using the info
 * returned by the driver install in devii. It also has to
 * make the required sncfg, using the pdih->pdih_pdi->init_sncfg 
 * routine.
 */

void
snAddChild(pd_inst_hdl_t *pdih, pd_inst_parm_t *pdip)
{
	pd_dev_info_t 	*pdev ;
	vertex_hdl_t	dev_v ;
	klinfo_t	*k = NULL ;
	dev_inst_info_t	*devii = (dev_inst_info_t *)&pdip->pdip_devii ;

	if (!IS_DEVICE_PSEUDO(pdip)) {
		pdev = pgMakePdev(pdih, pdip) ;
        	if (!pdev)
                	return ;
	}
	else
		pdev = NULL ;

	/* Call driver supplied add child routine to 
	   extend the graph, and represent the new device */

	if (pdiSnAddChild(pdih->pdih_pdi))
		(*pdiSnAddChild(pdih->pdih_pdi))(pdev,
			pdih->pdih_ctrl_v, (void *)devii) ;
}

vertex_hdl_t
snAddChildPostOp(pd_dev_info_t 		*pdev, 
		 vertex_hdl_t		ctrl_v,
		 char 			*path, 	
		 klinfo_t 		*k,
		 dev_inst_info_t 	*devii)
{
	vertex_hdl_t 	dev_v ;
#if 0
	COMPONENT	*c = &devii->devii_c ;
#endif

	/* Update some new stuff from dev specific add child */

	/* XXX If dev specific add child updates any info
	   in devii, it needs to be copied here. */

	if (pdev)
        	pdev->pdev_pdi.pdi_k = k ;

	/*
	 * create the rest of the path as required
	 * by the driver. If *path is null it means
	 * that the caller has created the full
	 * path and passed in the leaf vertex as ctrl_v.
	 */

	if (*path)
        	dev_v = create_graph_path(ctrl_v, path, 1) ;
	else
		dev_v = ctrl_v ;

	/* 
	 * Hang the 2 required prom labels off of the vertex
 	 * that represents the device.
	 */

	if (k)
        	pg_add_lbl(dev_v, INFO_LBL_KLCFG_INFO, (void *)k) ;

	if (pdev)
        	pg_add_lbl(dev_v, INFO_LBL_DEVICE,     (void *)pdev) ;

	return dev_v ;

}

int
check_install_once(int *once_flag)
{
        if (*once_flag) {
                return 0 ;
        }
        else {
                *once_flag = 1 ;
                return 1 ;
        }
}

static klinfo_t *
sn_init_device_graph(pd_dev_info_t *pdev, vertex_hdl_t ctlr_v, int dev_type)
{
        klinfo_t        *dev_k 		= NULL ;
	pd_info_t	*pdi 		= &pdev->pdev_pdi ;
	void		*(*fptr)() ;
	graph_error_t	ge ;

	/* XXX we need to get the ctlr_k here some how.
	   Ultimately, we need to eliminate the need for the 
	   ctlr k here.
 	 */

	klinfo_t	*ctlr_k = NULL ;

	ge = graph_info_get_LBL(pg_hdl, 
				ctlr_v, 
				INFO_LBL_KLCFG_INFO, NULL, 
				(arbitrary_info_t *)&ctlr_k) ;
	if (ge != GRAPH_SUCCESS)
		return NULL ;

	fptr = NULL ;
	if (pdev)
		if ((fptr = pdiInitDevSncfg(pdi)) == NULL)
			/* XXX need a generic routine here, input - k */
		 	;

	/* dev_type is ignored by most functions except scsi, kbd */
	if (fptr)
		dev_k = (klinfo_t *)(*fptr)(ctlr_k, dev_type) ;

        return(dev_k) ;
}

klinfo_t *
sn_make_dev_klcfg(pd_dev_info_t *pdev, vertex_hdl_t v, int type, COMPONENT *c)
{
        klinfo_t        *k ;
        COMPONENT       *ac ;

        k = (klinfo_t *)sn_init_device_graph(pdev, v, type) ;
        if (k == NULL)
                return NULL ;

        ac = (COMPONENT *)malloc(sizeof(COMPONENT)) ;
        if (ac == NULL)
                return NULL ;

        bzero(ac, sizeof(COMPONENT)) ;
        bcopy(c, ac, sizeof(COMPONENT)) ;

        k->arcs_compt = ac ;

        return k ;
}

/*
 * Typical AddChild for a device.
 * Use this unless a device has 
 * specific requirements.
 */

vertex_hdl_t
snDevAddChild(	pd_dev_info_t   *pdev   ,
		vertex_hdl_t    v       ,
		dev_inst_info_t *devii  ,
		char            *path   ,
		int             type    ,
		COMPONENT       *c)
{
        char            tpath[128] ;
        klinfo_t        *k ;
	pd_info_t	*pdi = &pdev->pdev_pdi ;

        strcpy(tpath, path) ;

        k = sn_make_dev_klcfg(pdev, v, type, c) ;

	if (k)
        	k->physid = devii->devii_unit ;

        return(snAddChildPostOp(pdev, v, path, (klinfo_t *)k, devii)) ;
}


#endif SN_PDI




