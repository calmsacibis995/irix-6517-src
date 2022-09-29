/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	__SLOT_DRVR_H__
#define	__SLOT_DRVR_H__

/*
 * SLOTCONFIG provides card/slot id based matching in standalone, integrated
 * with the older driver install based probing methods.  Bus providers call
 * slot_register() to register a slot and the config code will match this
 * against the data provided by standcfg.
 *
 * This code is an slightly better integrated version of Greg Limes'
 * slot_drvr, which is derived from the original pci_intf.
 *
 * .cf ORDERING: Some of this code requires the .cf file to give the
 * propper driver parent dependancies.
 *
 * CALL ORDERING: If multiple drivers are interested in a device, the
 * various drivers' install routines will be called with that device
 * in the order in which the drivers are in the .cf file.
 */
typedef struct slotdrvr_id_s	slotdrvr_id_t;
typedef struct slotinfo_s	slotinfo_t;
typedef struct drvrinfo_s	drvrinfo_t;

/*
 * slot_register: register a device found.
 */
void slot_register(struct component *,slotinfo_t *);

/*
 * slotdrvr_id: common substructure representing the information that
 * we might acqiure from a device in a bus-dependent but
 * device-independent manner, plus a notation of what kind of bus we
 * acquired the data from. In order for a driver and a device to
 * rendevous, all fields must "match" (where match means that the
 * values are the same, or the driver used an _ANY wildcard, or in the
 * case of "rev" the device's value must be greater than or equal to the
 * driver's value).
 */
struct slotdrvr_id_s {
    unsigned int bus;
    unsigned int mfgr;
    unsigned int part;
    unsigned int rev;
};

/*
 * slotinfo_t: information provided by the bus scanner.
 *
 * Must include enough ID to limit the set of drivers that might be
 * interested in looking at these devices; should include the base
 * address of any "configuration" space (mostly for PCI) and an
 * address (and size) representing the virtual addresses that will get
 * to the actual device. Additionally a COMPONENT pointer is made
 * available so the device driver can correctly position the resulting
 * AddChild within the COMPONENT tree.
 *
 * bus_base is a pointer to the regsiters for the bus controller,
 * useful when a device needs to do something to its controller (like
 * a PCI device whacking a Bridge Device(n) regster). bus_slot is the
 * logical slot number or id number; on XTalk this is the XTalk ID,
 * and on PCI this is the slot number.
 */
struct slotinfo_s {
    slotdrvr_id_t	id;
    struct component   *parent;
    volatile void      *cfg_base;
    volatile void      *mem_base;

    void	       *bus_base;
    int			bus_slot;
    int			drvr_index;
};

/*
 * drvrinfo_t: information provided by the device driver.
 *
 * The ID word is used to filter which devices are actually presented
 * to the install routine; in proper environments (PCI, Crosstalk)
 * this generally filters out all but the specific device that the
 * driver is interested in. The "name" field is optional but suggested
 * for debuggability.
 *
 * It is reasonable for a driver to register itself multiple times,
 * using different drvrinfo structures. When using this to cause
 * different rendevous routines to be used for different revisions of
 * the card, it is suggested that the registrations be done in
 * decreasing revision number order.
 */
struct drvrinfo_s {
    slotdrvr_id_t      *id;		/* null terminated array of ids */
    char	       *name;
};

#define BUS_NONE		(0)	/* terminator */
#define	BUS_ANY			(1)
#define	BUS_GIO			(2)
#define	BUS_IOC3		(3)
#define	BUS_XTALK		(4)
#define	BUS_PCI			(5)

/*
 * Wildcards. Using "all bits set" makes it least likely
 * that we will collide on a valid value.
 *
 * NB: PCI "invalid vendor" is 0xFFFF so we have precedent.
 */
#define	MFGR_ANY	(0xFFFFFFFFu)
#define	PART_ANY	(0xFFFFFFFFu)
#define	REV_ANY		(0)

#endif	/* __SLOT_DRVR_H__ */
