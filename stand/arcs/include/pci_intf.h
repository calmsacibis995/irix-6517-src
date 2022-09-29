/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * pci_intf.h - PCI interface routines and defines
 */

#ident "$Revision: 1.1 $"

#ifndef __SYS_PCI_INTF_H__
#define  __SYS_PCI_INTF_H__

/*
 * structure for matching bus slot info to registration
 *     vendor_id and device_id are exact matches only
 *     revision_id is an exact match (>=0) or wildcard (-1)
 */
struct pci_ident {
	u_int32_t	vendor_id;		/* Vendor ID */
	u_int32_t	device_id;		/* Device ID */
	__int32_t	revision_id;		/* Revision ID */
};
typedef struct pci_ident pci_ident_t;

#define	REV_MATCH_ANY	-1

struct pci_intr {
	__int32_t	cpu_target;
	__int32_t	(*pci_intr)(void *);
	void		*intr_arg;
};
typedef struct pci_intr pci_intr_t;

struct pci_record {
	__int32_t	(*pci_attach)(struct pci_record *, edt_t *, COMPONENT *);
	__int32_t	(*pci_detach)(struct pci_record *, edt_t *, __int32_t);
	__int32_t	(*pci_error)(struct pci_record *, edt_t *, __int32_t);
	pci_ident_t	pci_id;
	u_int16_t	pci_flags;
	__int32_t	io_footprint;	/* size of io area required (bytes) */
	__int32_t	mem_footprint;	/* size of mem area required (bytes) */
};
typedef struct pci_record	pci_record_t;

/* pci_flags bits */
#define PCI_NO_AUTO_ATTACH      0x80

/* reasons bits */
#define	PCI_FORCE_DETACH	0x0001	/* force a detach */

/* prototypes */
extern pci_register(pci_record_t *);
extern pci_unregister(pci_record_t *, __int32_t reasons);
extern pci_starutp(void *);
extern pic_shutdown(void *, __int32_t);
extern pci_setup_intr(edt_t *, pci_intr_t *);
extern pci_remove_intr(edt_t *, pci_intr_t *);
extern u_int32_t	pci_get_cfg(volatile unsigned char *, int);
extern int		pci_put_cfg(volatile unsigned char *, int, __int32_t);


#endif /* __SYS_PCI_INTF_H__ */
