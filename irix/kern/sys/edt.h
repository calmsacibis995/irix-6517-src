/* "Equiped Device Table and VME interrupts 
 *
 * Copyright (c) 1984 AT&T. All Rights Reserved
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 */

#ifndef _SYS_EDT_H_
#define _SYS_EDT_H_

#ident	"$Revision: 3.34 $"

#if !_LANGUAGE_ASSEMBLY

#define NBASE 3

typedef struct iospace {
	unchar		ios_type;	/* io space type on the adapter */
	iopaddr_t	ios_iopaddr;	/* io space base address */
	ulong		ios_size;	
	caddr_t		ios_vaddr;	/* kernel virtual address */
} iospace_t;

#ifdef _KERNEL

typedef struct irix5_iospace {
	unchar		ios_type;
	app32_ulong_t	ios_iopaddr;
	app32_ulong_t	ios_size;
	app32_ptr_t	ios_vaddr;
} irix5_iospace_t;

#endif	/*_KERNEL */

typedef struct edt edt_t;
#include <sys/iobus.h>

struct edt {
	uint_t		e_bus_type;	/* vme, scsi, eisa, pci... */
	unchar		v_cpuintr;	/* cpu to send intr to */
	unchar		v_setcpuintr;	/* cpu field is valid */
	uint_t		e_adap;		/* adapter */
	uint_t          e_ctlr;         /* controller identifier */
	void*		e_bus_info;	/* bus dependent info */
	int		(*e_init)(struct edt *);/* device init/run-time probe */
	iospace_t	e_space[NBASE];
        void*           e_platform_private;/* platform-dependent private info */

	vertex_hdl_t	e_connectpt;	/* "parent" vertex for this device */
					/* may be physical rather than architectural */
					/* used to form canonical name for dev */

	vertex_hdl_t	e_master;	/* "master" vertex for this device. */
					/* Usually points to controller or adapter ASIC. */
					/* used to determine node that controls dev. */

	char            e_master_name[512]; 
	device_desc_t	e_device_desc;	/* administrative description of a device */
					/* desired bandwidth allocation, interrupt */
					/* target, etc */
};

#define e_base		e_space[0].ios_vaddr
#define e_base2		e_space[1].ios_vaddr
#define e_base3		e_space[2].ios_vaddr
#define e_iobase	e_space[0].ios_iopaddr
#define e_iobase2	e_space[1].ios_iopaddr
#define e_iobase3	e_space[2].ios_iopaddr

/* e_bus_type */
#define ADAP_NULL	0
#define ADAP_VME	1
#define ADAP_GFX	2
#define ADAP_SCSI	3
#define ADAP_LOCAL	4
#define ADAP_GIO	5
#define ADAP_EISA	6
#define ADAP_IBUS	7
#define ADAP_EPC	8
#define ADAP_DANG	9
#define ADAP_PCI	10

extern void *		edt_bus_info_get(edt_t *edt);
extern vertex_hdl_t	edt_connectpt_get(edt_t *edt);
extern vertex_hdl_t	edt_master_get(edt_t *edt);
extern device_desc_t	edt_device_desc_get(edt_t *edt);

/* IBUS-specific data structures and constants */
#define IBUS_SLOTSHFT	3
#define IBUS_IOASHFT	0
#define IBUS_IOAMASK	0x7	

#define EPC_SERIAL	0x1
#define EPC_ETHER	0x2

typedef struct {
	unsigned	ibus_module;	/* Name of module */
	unsigned	ibus_unit;	/* Unit number for this particular dev */

	unsigned	ibus_adap;	/* Adapter encoding for slot & ioa */
	unsigned	ibus_ctlr;	/* Mezz sub-controller number */

	int		ibus_proc;	/* logical ID for cpu-directed intrs */
} ibus_t;

extern ibus_t 	ibus_adapters[];
extern int 	ibus_numadaps;

#ifdef	_KERNEL
extern struct edt edt[];
extern int nedt;
#endif

#endif /* !_LANGUAGE_ASSEMBLY */

#endif
