/*
 * flash.c -- Pseudo-device driver for the Everest IO4 flash prom.
 * 	It may someday be interesting to issue ioctls to this device,
 *	so we've made it a cdev.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <ksys/ddmap.h>
#include <sys/immu.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/debug.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>

/* Make this into a "new" I/O driver.  This definition must exist
 * in order for lboot to do the right thing.
 */
int flash_devflag = 0;

#if 0
#define dprintf(x)	printf x
#else
#define dprintf(x)
#endif

#define FLASHPROM_SIZE	(1024 * 1024)
#define FLASH_NUMCLICKS btoc(FLASHPROM_SIZE)
#define SPLIT		(FLASHPROM_SIZE/2)

static unsigned int epc_adap[EV_MAX_SLOTS];
static char* flashmap[EV_MAX_SLOTS];

/*
 * get_slot -- Returns the slot number corresponding to the given
 *	device structure. 
 */
static int 
get_slot(dev_t dev)
{
    int slot = minor(dev);		/* Slot containing selected device */
    static int master_slot = -1;	/* slot # containing master IO4 */

    ASSERT(slot >= 0 && slot < EV_MAX_SLOTS);

    if (slot)
	return slot;
    else {
	if (master_slot != -1)
	    return master_slot;
	else     
	    /* If slot is zero, find the master IO4 and use it */
	    for (slot = EV_MAX_SLOTS; slot >= 0; slot--) {
		evbrdinfo_t *brd = BOARD(slot);

		if (EVCLASS(brd->eb_type) == EVCLASS_IO && brd->eb_enabled &&
		    brd->eb_io.eb_winnum == 1) {
		    master_slot = slot;
		    return slot;
		}
	    }
    }

    return -1;
} 


/*
 * is_io4 -- return 1 if the given slot contains a legitimate IO4 with
 * 	an EPC.  Return 0 otherwise.
 */
static int
is_io4(int slot)
{
    unsigned int  adap;			/* EPC adapter number */
    evbrdinfo_t *brd;			/* Dereferenced pointer to brd info for slot */

    ASSERT(slot >= 0 && slot < EV_MAX_SLOTS);
    /* If the appropriate entry is already set in the epc_adap
     * array, we know that this must be an IO4.
     */
    if (epc_adap[slot])
	return 1;

    brd = BOARD(slot);

    if ((EVCLASS(brd->eb_type) != EVCLASS_IO) || !brd->eb_enabled) {
	return 0;
    }

    for (adap = 1; adap < IO4_MAX_PADAPS; adap++) {
	evioacfg_t *ioa = &(brd->eb_ioarr[adap]);

	if (ioa->ioa_type == IO4_ADAP_EPC && ioa->ioa_enable) {
	    epc_adap[slot] = adap;
	    break;
	}
    }

    if (epc_adap[slot] == 0)
	return 0;
    else
	return 1;
}


/*
 * flash_open -- checks the dev_t and determines which IO4 board is
 * 	being programmed.  If the specified board is invalid it
 * 	returns an error.
 */
/* ARGSUSED */
int
flash_open(dev_t *dev, int flag, int otyp, cred_t *cred)
{
    int 	slot = get_slot(*dev);	/* Slot for the device */
    int		adap;			/* EPC adapter number */
    int 	i;			/* Index variable for iterating over pages */ 
    pfn_t	pfn;			/* Page frame number */ 
    pde_t	*pp;			/* Pointer to page descriptor information */

    /* Complain if there is no flash prom associated with the
     * given device.
     */
    ASSERT(slot > 0 && slot < EV_MAX_SLOTS);
    if (!is_io4(slot))
	return ENODEV;

    adap = epc_adap[slot];

    /* Grab the flashmap lock. */

    dprintf(("flash%d: Opening adap %d\n", slot, adap));
    if (flashmap[slot]) {
	/* A region for the flashprom has already been acquired */
	/* Release flashmap lock */
	return 0; 
    }

    /* Try grabbing some kernel virtual space for this device */
    flashmap[slot] = kvalloc(FLASH_NUMCLICKS, VM_UNCACHED, 0);
    ASSERT(flashmap[slot] != NULL);

    /* Release the flashmap lock. */

    /* Set up the actual kseg2 mappings */
    pp = kvtokptbl(flashmap[slot]); 
    pfn = LWIN_PFN(BOARD(slot)->eb_io.eb_winnum, adap)
	>> (PNUMSHFT - IO_PNUMSHFT);
    dprintf(("flash%d: flashaddr = 0x%x, pfn = 0x%x\n", slot, flashmap[slot],
	    pfn));
 
    for (i = 0; i < FLASH_NUMCLICKS; i++, pp++, pfn++) {

	/* Allow for discontinuity in page numbering */
	if (i == btoc(SPLIT))
	    pfn += btoc(SPLIT);

	pg_setpgi(pp, mkpde(PG_VR|PG_SV|PG_G, pfn));
	pg_setcache_phys(pp);
    }

    return 0; 
}


/*
 * flash_close -- Releases the kseg2 space allocated in flash_open.
 */
int
flash_close(dev_t dev)
{
    int slot = get_slot(dev);

    ASSERT(flashmap[slot]);

    /* Acquire flashmap lock */
    dprintf(("flash%d: closing...\n", slot));
    kvfree(flashmap[slot], FLASH_NUMCLICKS);	
    flashmap[slot] = NULL;

    /* Release flashmap lock */

    return 0;
}

/* 
 * flash_read -- Standard read entry point.  This is a very
 * 	low-speed interface and is really just intended to allow
 *	people to obtain a snapshot of the flash prom image.  It
 *	is kind of useful for testing too.  We need to use ureadc()
 *	since the flashprom only supports halfword- and byte-sized
 *	transactions.  
 */

int
flash_read(dev_t dev, uio_t *uio)
{
    int 	slot = get_slot(dev);	/* Slot of device */
    caddr_t 	vaddr;			/* Pointer to appropriate flash map */
    int		numbytes;		/* Number of bytes to actually xfer */
    int 	error;			/* Error retval */

    ASSERT(flashmap[slot]);
    dprintf(("flash%d: in read, off = 0x%x, resid = 0x%x\n", slot,
	    uio->uio_offset, uio->uio_resid));

    /* Make sure that user can't lseek off of the end of the array */
    if (uio->uio_offset > FLASHPROM_SIZE)
	return ENXIO;

    /* Mark the end of file */
    if (uio->uio_offset == FLASHPROM_SIZE)
	return 0;

    /* Otherwise, return as much as we can without exceeding the limits */
    if ((uio->uio_offset + uio->uio_resid) > FLASHPROM_SIZE)
	numbytes = FLASHPROM_SIZE - uio->uio_offset;
    else
	numbytes = uio->uio_resid;

    /* Transfer the data out of the flash prom.  Slow but compatible */
    vaddr = flashmap[slot] + uio->uio_offset; 
    while (numbytes--) {
	if (error = ureadc(*vaddr++, uio))
	    return error;
    }

    return 0;
}


/*
 * We need to go through a fairly elaborate song-and-dance in order to
 * write to the flash prom, so for simplicity we won't support write
 * requests.
 */
/* ARGSUSED */
int
flash_write(dev_t dev, uio_t *uio)
{
    return ENXIO; 
}


/*
 * flash_ioctl -- currently unused.
 */
int
flash_ioctl()
{
    return ENXIO;
}


/*
 * flash_map -- Produces a mapping between a set of user virtual pages
 * 	and the physical pages which actually map the flashprom.  For
 * 	our purposes, we consider the flash prom to be a 1-meg object.
 */
/* ARGSUSED */
int
flash_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot)
{
    long offset = (long) off;		/* Actual offset into flash prom */
    int slot = get_slot(dev);

    /* Don't allow requests which exceed the size of the flash prom */
    if ((offset + len) > FLASHPROM_SIZE) {
	dprintf(("flash error: Request exceeded flashprom size\n"));
	return ENOSPC;
    }
    
    /* Don't allow non page-aligned offsets */
    if ((offset % NBPC) != 0) {
	dprintf(("flash error: offset not page-aligned\n"));
	return EIO;
    }

    /* Only allow mapping of entire pages */
    if ((len % NBPC) != 0) {
	dprintf(("flash error: length not integral number of pages\n"));
	return EIO;
    }

    ASSERT(flashmap[slot]);
    return v_mapphys(vt, flashmap[slot] + offset, len);
}


/*
 * Stub to avoid calling nodev() out of cdevsw[].
 */
/* ARGSUSED */
int
flash_unmap(dev_t dev, vhandl_t *vt)
{
    return 0;
}


/*
 * flash_unload -- Checks to see if anyone has open references to
 *	the flashprom driver.  If not, release it.
 */
int
flash_unload()
{
    int i;

    /* If flash devices are open, there will be non-zero entries in 
     * flashmap. 
     */
    for (i = 0; i < EV_MAX_SLOTS; i++)
	if (flashmap[i])
	    return EBUSY;

    return 0;
}
