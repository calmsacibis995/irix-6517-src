/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990-1993, Silicon Graphics, Inc           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * This structure is used for holding on to per registration info
 */
struct pci_reg {
	struct pci_reg	*preg_next;	/* maintain the list */
	pci_record_t	*preg_record;	/* info provided to us */
};
typedef	struct pci_reg pci_reg_t;

extern pci_reg_t	*pci_reg_first;

#define	PCI_SLOT_ANY	(-1)

extern pci_attach_devs(pci_reg_t *, int);
extern pci_detach_devs(pci_reg_t *, int, int);
extern pci_reg_t *pci_find_reg(pci_ident_t *ip);
extern pci_unregister(pci_record_t *, int);
