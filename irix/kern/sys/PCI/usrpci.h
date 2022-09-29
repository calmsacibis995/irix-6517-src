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
#ifndef __PCI_USRPCI_H__
#define __PCI_USRPCI_H__

#ident "$Revision: 1.3 $"

/*
 * usrpci.h -- User level PCI address space driver.
 */

#if LANGUAGE_C
#if _KERNEL
#include <sys/alenlist.h>
#include <sys/ioerror.h>

extern void usrpci_device_register(vertex_hdl_t, vertex_hdl_t, int);
extern int usrpci_error_handler(vertex_hdl_t, int, iopaddr_t);

#endif /* _KERNEL */

#define UPIOC ('u' << 16 | 'p' << 8)

#define UPIOCREGISTERULI	(UPIOC|0)	/* register ULI */

#endif /* _LANGUAGE_C */
#endif /* __PCI_PCIIO_H__ */
