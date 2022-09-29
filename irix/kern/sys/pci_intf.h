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

#ident "$Revision: 1.7 $"

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

/* Macros for matching PCI ids.  Wildcard is defined by us, not by standard */
#define	REV_MATCH_ANY	-1
#define PCI_ID_WILDCARD 		-1
#define PCI_VENDOR_MATCHED(v1, v2) \
	(((v1)&0xffff) == 0xffff || ((v1)&0xffff) == ((v2)&0xffff))
#define PCI_DEVID_MATCHED(d1, d2) PCI_VENDOR_MATCHED(d1, d2)
#define PCI_REV_MATCHED(r1, r2) \
	(((r1)&0xff) == 0xff || ((r1)&0xff) == ((r2)&0xff))

struct pci_intr {
	__int32_t	(*pci_intr)(void *);
	void		*intr_arg;
};
typedef struct pci_intr pci_intr_t;

struct pci_record {
	__int32_t	(*pci_attach)(struct pci_record *, edt_t *);
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

#include <sys/graph.h>

/* pcitype iograph info, labelled by INFO_LBL_PCITYPE */
typedef struct v_pcitype_s {
	vertex_hdl_t	t_vertex;	/* back pointer to vertex */
	lock_t		t_lock;		/* spinlock for changing pci record */
	int		t_record_valid;	/* is pci record valid? */
	pci_record_t	t_record;
} v_pcitype_t;

#define V_TO_PCITYPE(v, pcitypeptr) hwgraph_info_get_LBL \
	(v, INFO_LBL_PCITYPE, (arbitrary_info_t*)&pcitypeptr)

/* prototypes */
extern int pci_register(pci_record_t *);
extern int pci_unregister(pci_record_t *, __int32_t reasons);
extern int pci_startup(void *bus_addr);
extern int pci_shutdown(void *bus_addr, __int32_t reasons);
extern int pci_setup_intr(edt_t *, pci_intr_t *);
extern int pci_remove_intr(edt_t *, pci_intr_t *);
extern void pci_dang_intr(void *, void (*)(), void *);
extern int pci_badaddr(void *, int, void *);
extern u_int32_t	pci_get_cfg(volatile unsigned char *, int);
extern int		pci_put_cfg(volatile unsigned char *, int, __int32_t);

#define	_PCIC_(x)	(('P'<<8) | x)

#define	PCI_ATTACH	_PCIC_(1)	/* attach a pci device */
#define	PCI_DETACH	_PCIC_(2)	/* detach a device */
#define	PCI_STARTUP	_PCIC_(3)	/* startup a bus */
#define	PCI_SHUTDOWN	_PCIC_(4)	/* shutdown a bus */
#define	PCI_UNREGISTER	_PCIC_(5)	/* Unregister a device driver */


#endif /* __SYS_PCI_INTF_H__ */
