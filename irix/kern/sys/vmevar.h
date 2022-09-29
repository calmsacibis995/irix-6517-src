#ident	"$Revision: 3.3 $"
/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */
#ifndef __SYS_VMEVAR_H__
#define __SYS_VMEVAR_H__

/*
 * This file contains definitions related to the kernel structures
 * for dealing with vme bus devices.
 *
 * Each vme bus controller which is not a device has a vme_ctlr structure.
 * Each vme bus device has a vme_device structure.
 */
#include "sys/buf.h"

/*
 * Per-controller structure.
 * (E.g. one for each disk and tape controller, and other things
 * which use and release buffered data paths.)
 *
 * If a controller has devices attached, then there are
 * cross-referenced vme_drive structures.
 * The queue of devices waiting to transfer is attached here.
 */
struct vme_ctlr {
	struct	vme_driver *vm_driver;
	short	vm_ctlr;	/* controller index in driver */
	short	vm_alive;	/* controller exists */
	int	(**vm_intr)();	/* interrupt handler(s) */
	caddr_t	vm_addr;	/* address of device in K1 space */
	struct	buf vm_tab;	/* queue of devices for this controller */
};

/*
 * Per ``device'' structure.
 * A controller has devices.  Everything else is a ``device''.
 *
 * If a controller has many drives attached, then there will
 * be several vme_device structures associated with a single vme_ctlr
 * structure.
 *
 * This structure contains all the information necessary to run
 * a vme bus device without slaves.  It also contains information
 * for slaves of vme bus controllers as to which device on the slave
 * this is.  A flags field here can also be given in the system specification
 * and is used to tell device specific parameters.
 */
struct vme_device {
	struct	vme_driver *vi_driver;
	short	vi_unit;	/* unit number on the system */
	short	vi_ctlr;	/* mass ctlr number; -1 if none */
	short	vi_slave;	/* slave on controller */
	int	(**vi_intr)();	/* interrupt handler(s) */
	caddr_t	vi_addr;	/* address of device in K1 space */
	short	vi_dk;		/* if init 1 set to number for iostat */
	int	vi_flags;	/* parameter from system specification */
	short	vi_alive;	/* device exists */
	short	vi_type;	/* driver specific type information */
/* this is the forward link in a list of devices on a controller */
	struct	vme_device *vi_forw;
/* if the device is connected to a controller, this is the controller */
	struct	vme_ctlr *vi_mi;
};

/*
 * Per-driver structure.
 *
 * Each vme bus driver defines entries for a set of routines
 * as well as an array of types which are acceptable to it.
 * These are used at boot time by the configuration program.
 */
struct vme_driver {
	int	(*vd_probe)();		/* see if a driver is really there */
	int	(*vd_slave)();		/* see if a slave is there */
	int	(*vd_attach)();		/* setup driver for a slave */
	unsigned *vd_addr;		/* csr addresses in SA16 space */
	char	*vd_dname;		/* name of a device */
	struct	vme_device **vd_dinfo;	/* backpointers to ubdinit structs */
	char	*vd_mname;		/* name of a controller */
	struct	vme_ctlr **vd_minfo;	/* backpointers to ubminit structs */
};

#ifdef _KERNEL
/*
 * vmminit and vmdinit initialize the mass storage controller and
 * device tables specifying possible devices.
 */
extern	struct	vme_ctlr vmminit[];
extern	struct	vme_device vmdinit[];
#endif /* _KERNEL */

#endif /* __SYS_VMEVAR_H__ */
