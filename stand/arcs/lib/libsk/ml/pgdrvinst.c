
/*
 * pgdrvinst.c
 * 
 * Driver install structs definition.
 * Will replace the old io6inst.c
 * Any new driver to be added to the IOprom should
 * have an entry here.
 * A driver needs a bunch of call back routines. 
 * If there is no call back routine for a particular
 * function, NULL should be used. The system then
 * uses a default function for that driver.
 *
 * The current call backs and their functions are :
 *
 * init_dev_klcfg - This will be called when the driver
 * discovers a device and needs to init a structure in
 * prom low mem - klconfig area.
 *
 * make_inst_param - Each driver can pass the parameters
 * it wants to be sent during installation. The prom sends
 * it as void *. In the driver install routine it can get
 * its value back. PROM uses 'make_inst_param' to get the
 * param info, else it passes a COMPONENT * like ARCS proms.
 *
 * install - this is needed and a default cannot be called.
 * This routine performs install of a prom controller.
 *
 * check_install - PROM calls this routine to verify if 
 * the install can be called for this controller. The 
 * default is to not call it.
 *
 * strategy - driver strategy routine.
 * 
 * pgAddChild - Driver calls snAddChild to add the devices
 * it found to the prom graph. snAddChild calls pgAddChild.
 * 
 * get_dma_addr - used by the driver to do any dma mappings.
 * default is the dma routine used for the bridge.
 *
 * [driver/prom] extern - Pointer to any routines that the 
 * [driver/prom] wishes to publish - for example do_scsi by 
 * dksc. If the prom does not want this driver, it can 
 * generate these stubs.
 */

#include <libsc.h>
#include <libsk.h>
#if 0
#include <libkl.h>
#endif
#include <sys/SN/SN0/klhwinit_gfx.h>

extern void qlconfigure() ;
extern void sn_ioc3_install() ;
extern void ef_install() ;
extern void ns16550_install() ;
extern void pckbd_install() ;
extern void kona_install() ;

extern void *ql_make_inst_param() ;
extern void ql_AddChild() ;
extern void ioc3_AddChild() ;
extern void ioc3_ei_AddChild() ;
extern void ioc3_tty_AddChild() ;
extern void ioc3_pckm_AddChild() ;

extern int  ql_check_install() ;
extern int  ioc3_check_install() ;

extern int dksc_strat() ;
extern int _sn_ioc3_strat() ;
extern int _if_strat() ;
extern int _ns16550_strat() ;
extern int _kbd_strat() ;
extern int _gfx_strat() ;

extern void *init_klcfg_scsi_drive() ;
extern void *init_klcfg_enet() ;
extern void *init_klcfg_tty() ;
extern void *init_klcfg_kbd() ;
extern void *init_klcfg_ms() ;
extern void *init_klcfg_gdev() ;

#define	init_sncfg_scsi_drive	NULL

prom_drv_t      ql_drv_info = {
0x1020 ,0x1077, NULL, 			/* key1, key2, strkey */
0,					/* flag */
init_klcfg_scsi_drive, 			/* init_dev_klcfg callback */
NULL, 					/* reserved callback */
ql_make_inst_param,			/* make_inst_param callback */
qlconfigure, 				/* install callback */
ql_check_install, 			/* check_install callback */
dksc_strat, 				/* strategy callback */
ql_AddChild,				/* pgAddChild callback */
NULL, 					/* get_dma_addr callback */
NULL, 					/* driver extern routines */
NULL, 					/* prom extern routines */
NULL					/* reserved */
} ;

#define	init_sncfg_ioc3	NULL

prom_drv_t      ioc3_drv_info = {
0x3 ,0x10a9, NULL, 			/* key1, key2, strkey */
0,					/* flag */
init_sncfg_ioc3, 			/* init_dev_klcfg callback */
NULL, 					/* reserved callback */
NULL,					/* make_inst_param callback */
sn_ioc3_install, 			/* install callback */
ioc3_check_install, 			/* check_install callback */
_sn_ioc3_strat, 			/* strategy callback */
ioc3_AddChild,				/* pgAddChild callback */
NULL, 					/* get_dma_addr callback */
NULL, 					/* driver extern routines */
NULL, 					/* prom extern routines */
NULL					/* reserved */
} ;

#define	init_sncfg_enet	NULL

prom_drv_t      ef_drv_info = {
0 ,0, "ioc3_ei", 			/* key1, key2, strkey */
0,					/* flag */
init_klcfg_enet, 			/* init_dev_klcfg callback */
NULL, 					/* reserved callback */
NULL,					/* make_inst_param callback */
ef_install, 				/* install callback */
NULL, 					/* check_install callback */
_if_strat, 				/* strategy callback */
ioc3_ei_AddChild,			/* pgAddChild callback */
NULL, 					/* get_dma_addr callback */
NULL, 					/* driver extern routines */
NULL, 					/* prom extern routines */
NULL					/* reserved */
} ;

#define	init_sncfg_tty	NULL

prom_drv_t      tty_drv_info = {
0,0, "ioc3_tty", 			/* key1, key2, strkey */
0,					/* flag */
init_klcfg_tty, 			/* init_dev_klcfg callback */
NULL, 					/* reserved callback */
NULL,					/* make_inst_param callback */
ns16550_install, 			/* install callback */
NULL, 					/* check_install callback */
_ns16550_strat, 			/* strategy callback */
ioc3_tty_AddChild,			/* pgAddChild callback */
NULL, 					/* get_dma_addr callback */
NULL, 					/* driver extern routines */
NULL, 					/* prom extern routines */
NULL					/* reserved */
} ;

#define	init_sncfg_pckm	NULL

prom_drv_t      pckm_drv_info = {
0,0, "ioc3_pckm", 			/* key1, key2, strkey */
0,					/* flag */
init_klcfg_kbd, 			/* init_dev_klcfg callback */
NULL, 					/* reserved callback */
NULL,					/* make_inst_param callback */
pckbd_install, 				/* install callback */
NULL, 					/* check_install callback */
_kbd_strat, 				/* strategy callback */
ioc3_pckm_AddChild,			/* pgAddChild callback */
NULL, 					/* get_dma_addr callback */
NULL, 					/* driver extern routines */
NULL, 					/* prom extern routines */
NULL					/* reserved */
} ;

#if defined(SN0PROM) || defined(SN0XXLPROM)

/*
 *  XXXmacko
 *  We probably need to revisit how graphics fits into
 *  the whole SN_PDI scheme before this will all work.
 */

prom_drv_t      kona_drv_info = {
XG_WIDGET_PART_NUM,0, NULL, 		/* key1, key2, strkey */
0,					/* flag */
NULL,					/* init_dev_klcfg callback */
NULL, 					/* reserved callback */
NULL,					/* make_inst_param callback */
kona_install, 				/* install callback */
NULL,		 			/* check_install callback */
_gfx_strat, 				/* strategy callback */
NULL,					/* pgAddChild callback */
NULL, 					/* get_dma_addr callback */
NULL, 					/* driver extern routines */
NULL, 					/* prom extern routines */
NULL					/* reserved */
} ;

#define mgras_install 		NULL

prom_drv_t      mgras_drv_info = {
HQ4_WIDGET_PART_NUM ,0, NULL, 		/* key1, key2, strkey */
0,					/* flag */
NULL,		 			/* init_dev_klcfg callback */
NULL, 					/* reserved callback */
NULL,					/* make_inst_param callback */
mgras_install, 				/* install callback */
NULL, 					/* check_install callback */
_gfx_strat, 				/* strategy callback */
NULL,					/* pgAddChild callback */
NULL, 					/* get_dma_addr callback */
NULL, 					/* driver extern routines */
NULL, 					/* prom extern routines */
NULL					/* reserved */
} ;

#endif


