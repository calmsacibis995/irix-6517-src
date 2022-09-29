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

/*
 * File: klconfdev.c
 * klconfdev.c - Routines to init the KLCONFIG struct for each
 *              type of board.
 *
 *
 * Runs in MP mode, on all processors. These routines write in their
 * local memory areas.
 */

#include <sys/types.h>
#include <sys/mips_addrspace.h>
#include <sys/graph.h>
#include <sys/nic.h>
#include <sys/SN/addrs.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/error.h>
#include <sys/SN/xbow.h>
#include <sys/SN/nvram.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/kldiag.h>
#include <sys/PCI/bridge.h>
#include <arcs/cfgdata.h>
#include <libsc.h>
#include <libkl.h>
#include <libsk.h>
#include <libsc.h>
#include <report.h>

#ifndef SABLE
#define LDEBUG		0		/* Set for 1 for debug compilation */
#endif


#if defined(LD)
#undef LD
#endif

#define	LD(x)		(*(volatile __int64_t  *) (x))
#define	LW(x)		(*(volatile __int32_t  *) (x))

/* These are defined in different places in the IO6prom and IP27prom. */
extern int get_cpu_irr(void);
extern int get_fpc_irr(void);

#define TBD XXX

extern int kl_sable ;
extern off_t kl_config_info_alloc(nasid_t, int, unsigned int);
extern void init_config_malloc(nasid_t);
/*
 * Local function prototype declarations.
 */
int klconfig_port_update(void);
lboard_t *kl_config_alloc_board(nasid_t);
klinfo_t *kl_config_alloc_component(nasid_t, lboard_t *);
lboard_t *kl_config_add_board(nasid_t, lboard_t *);

int setup_hub_port(pcfg_hub_t *, pcfg_router_t *);
int setup_router_port_to_hub(pcfg_router_t *, pcfg_hub_t *, int);
int setup_router_port_to_router(pcfg_router_t *, pcfg_router_t *, int);

extern int get_hub_nic_info(nic_data_t, char *) ;

#define GET_PCI_VID(_id)	(_id & 0xffff)
#define GET_PCI_DID(_id)	((_id >> 16) & 0xffff)


klscdev_t *
init_klcfg_scsi_drive(klscsi_t *scsip, int type)
{
	klscdev_t *scdevptr ;
	off_t dev_offset;
	int c ;

	c = scsip->scsi_numdevs;
	scsip->scsi_numdevs += 1;
	dev_offset = kl_config_info_alloc(scsip->scsi_info.nasid,
				  DEVICE_STRUCT, sizeof(klcomp_t));
	if (dev_offset == -1) {
		return NULL;
	}

	scsip->scsi_devinfo[c] = dev_offset;
	scdevptr = (klscdev_t *)NODE_OFFSET_TO_K1(scsip->scsi_info.nasid,
						  dev_offset);

	scdevptr->scdev_info.struct_type = type ;
	scdevptr->scdev_info.flags = KLINFO_ENABLE|KLINFO_DEVICE ;
	scdevptr->scdev_info.virtid = scsip->scsi_info.virtid ;
	scdevptr->scdev_info.nasid = scsip->scsi_info.nasid ;

	return(scdevptr) ;
}


klttydev_t *
init_klcfg_tty(klioc3_t *ioc3p)
{
	klttydev_t *ttydevptr ;
	off_t dev_offset;
	int c ;

	dev_offset = kl_config_info_alloc(ioc3p->ioc3_info.nasid,
				  DEVICE_STRUCT, sizeof(klcomp_t));
	if (dev_offset == -1) {
		printf("init_klcfg_scsi_drive:Cannot allocate component\n");
		return NULL;
	}

	ttydevptr = (klttydev_t *)NODE_OFFSET_TO_K1(ioc3p->ioc3_info.nasid,
						  dev_offset);

	if (ioc3p->ioc3_info.struct_type == KLSTRUCT_HUB_UART) {
		ttydevptr->ttydev_info.struct_type = KLSTRUCT_HUB_TTY ;
		ttydevptr->ttydev_info.virtid = 0 ;
	}
	else {
		ttydevptr->ttydev_info.struct_type = KLSTRUCT_IOC3_TTY ;
		ioc3p->ioc3_tty_off = dev_offset;
		ttydevptr->ttydev_info.virtid = 0 ;
#if 0
		ttydevptr->ttydev_info.virtid =
				ioc3p->ioc3_info.virtid * 16 +
				ttydevptr->ttydev_info.physid ;
#endif
	}

	ttydevptr->ttydev_info.flags = KLINFO_ENABLE|KLINFO_DEVICE ;
	ttydevptr->ttydev_info.physid = 0 ;
	ttydevptr->ttydev_info.nasid = ioc3p->ioc3_info.nasid ;

	return(ttydevptr) ;
}

klenetdev_t *
init_klcfg_enet(klioc3_t *ioc3p)
{
	klenetdev_t *enetdevptr ;
	off_t dev_offset;
	int c ;

	dev_offset = kl_config_info_alloc(ioc3p->ioc3_info.nasid,
				  DEVICE_STRUCT, sizeof(klcomp_t));
	if (dev_offset == -1) {
		printf("init_klcfg_scsi_drive:Cannot allocate component\n");
		return NULL;
	}

	ioc3p->ioc3_enet_off = dev_offset;
	enetdevptr = (klenetdev_t *)NODE_OFFSET_TO_K1(ioc3p->ioc3_info.nasid,
						  dev_offset);

	enetdevptr->enetdev_info.struct_type = KLSTRUCT_IOC3ENET ;

	enetdevptr->enetdev_info.flags = KLINFO_ENABLE|KLINFO_DEVICE ;
	enetdevptr->enetdev_info.physid = 0 ; /* enet id */
#if 0
	enetdevptr->enetdev_info.virtid = ioc3p->ioc3_info.virtid * 16
			+ enetdevptr->enetdev_info.physid ;
#endif
	enetdevptr->enetdev_info.nasid = ioc3p->ioc3_info.nasid ;

	return(enetdevptr) ;
}

klkbddev_t *
init_klcfg_kbd(klioc3_t *ioc3p, int type)
{
	klkbddev_t 	*kbddevptr ;
	off_t 		dev_offset;
	int 		c ;

	dev_offset = kl_config_info_alloc(ioc3p->ioc3_info.nasid,
				  DEVICE_STRUCT, sizeof(klcomp_t));
	if (dev_offset == -1) {
		printf("init_klcfg_scsi_drive:Cannot allocate component\n");
		return NULL;
	}

	if (type == KLSTRUCT_IOC3PCKM)
		ioc3p->ioc3_kbd_off = dev_offset;

	kbddevptr = (klkbddev_t *)NODE_OFFSET_TO_K1(ioc3p->ioc3_info.nasid,
						  dev_offset);

	kbddevptr->kbddev_info.struct_type = type ;

	kbddevptr->kbddev_info.flags = KLINFO_ENABLE|KLINFO_DEVICE ;
	kbddevptr->kbddev_info.physid = 0 ; /* enet id */
	kbddevptr->kbddev_info.nasid = ioc3p->ioc3_info.nasid ;

	return(kbddevptr) ;
}

klmsdev_t *
init_klcfg_ms(klioc3_t *ioc3p)
{
        klmsdev_t *msdevptr ;
        off_t dev_offset;
        int c ;

        dev_offset = kl_config_info_alloc(ioc3p->ioc3_info.nasid,
                                  DEVICE_STRUCT, sizeof(klcomp_t));
        if (dev_offset == -1) {
                printf("init_klcfg_scsi_drive:Cannot allocate component\n");
                return NULL;
        }

#if 0
        ioc3p->ioc3_kbd_off = dev_offset;
#endif
        msdevptr = (klmsdev_t *)NODE_OFFSET_TO_K1(ioc3p->ioc3_info.nasid,
                                                  dev_offset);

        msdevptr->msdev_info.struct_type = KLSTRUCT_IOC3MS ;

        msdevptr->msdev_info.flags = KLINFO_ENABLE|KLINFO_DEVICE ;
        msdevptr->msdev_info.physid = 0 ; /* enet id */
        msdevptr->msdev_info.nasid = ioc3p->ioc3_info.nasid ;

        return(msdevptr) ;
}

