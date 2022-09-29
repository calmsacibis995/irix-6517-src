/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990-1996, Silicon Graphics, Inc           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

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
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/ksynch.h>
#include <sys/mload.h>
#include <sys/atomic_ops.h>
#include <sys/RACER/sflash.h>
#include <sys/RACER/IP30nvram.h>
#include "ml/RACER/RACERkern.h"			/* ip30_[gs]et_an_nvram */

#include <sys/hwgraph.h>
#include <sys/xtalk/xwidget.h>
#include <sys/PCI/bridge.h>

/*
 * srflash: device driver for speed racer system board (IP30) flash devices
 */
int srflash_devflag = D_MP;		/* multi-processor safe */
char * srflash_mversion = M_VERSION;	/* loadable driver */

/**********************************************************************
 * device driver internal state.  one structure allocated for each
 * instance of the flash device, hung off the flash device vertex
 */

#define	SRFLASH_OPEN_WRITE	0x00010000	/* was device open for write */
#define	SRFLASH_POWER_STATE	0x00020000	/* software power pre-state */

typedef struct srflash_state_s {
	mutex_t		lock;		/* mutex lock */
	widget_cfg_t  * bridge_cfg;	/* bridge config registers */
	volatile int16_t * flashaddr;	/* flash access address */
	vertex_hdl_t	conn;		/* hwgraph connection */
	int32_t		magic;		/* magic cookie (for sanity's sake) */
	int32_t		state;		/* driver/structure state */
	int32_t		devnopen;	/* ntimes this device is open */
} srflash_state_t;

static int srflash_nopen = 0;		/* ntimes this _driver_ is open */

/**********************************************************************
 * code
 */

/*
 * XXX - access to bridge control registers needs a lock of some kind!
 */
static void
srflash_bridge_wr_enable_on( widget_cfg_t * cfg )
{
	cfg->w_control |= BRIDGE_CTRL_FLASH_WR_EN;
	cfg->w_control;				/* flush write */
}

static void
srflash_bridge_wr_enable_off( widget_cfg_t * cfg )
{
	cfg->w_control &= ~BRIDGE_CTRL_FLASH_WR_EN;
	cfg->w_control;				/* flush write */
}

/*
 * return 1 if there is  flash memory, 0 if there isn't
 *
 * XXX flash memory check, this is temporary hack to prevent gio-bridge
 * hangs, we need a better check
 */
static int
srflash_flash_mem_check( widget_cfg_t * cfg)
{
	if (cfg->w_status & BRIDGE_STAT_PCI_GIO_N)
		return 1;	/* PCI mode */
	else
		return 0;	/* GIO mode */
}

/*
 **********************************************************************
 */

/*
 * don't know how to twiddle the softpower stuff yet ... but
 * encapsulate that here so that when I do figure it out.
 */
/*ARGSUSED*/
static void
srflash_softpower_disable( srflash_state_t * flash )
{
}

/*ARGSUSED*/
static void
srflash_softpower_enable( srflash_state_t * flash )
{
}

/*
 **********************************************************************
 */

/*
 * srflash_reg
 *
 * load the speed racer flash driver into the hardware graph framework
 */
int
srflash_reg(void)
{
	/*
	 * register the driver with the hardware graph, as someone
	 * who hangs around with bridges
	 */
	xwidget_driver_register( BRIDGE_WIDGET_PART_NUM,
				 BRIDGE_WIDGET_MFGR_NUM,
				 "srflash_", 0 );
	return (0);
}

/*
 **********************************************************************
 */

/*
 * srflash_unreg
 *
 */
void
srflash_unreg(void)
{
	/*
	 * unregister the driver
	 */
	xwidget_driver_unregister("srflash_");
}

/*
 **********************************************************************
 */

/*
 * srflash_attach
 *
 * called by xtalk infrastructure for each bridge node in the system.
 * we need to peek at the appropriate bridge address and see if we can
 * find (our particular brand of) flash prom there.
 */
int
srflash_attach( vertex_hdl_t conn )
{
	srflash_state_t * flash;	/* driver internal state */
	widget_cfg_t * cfg;		/* bridge widget config registers */
	volatile int16_t * probeaddr;	/* address to peek at for flash */
	short mfg;			/* first short of identify response */
	short dev;			/* second short of identify response */
	vertex_hdl_t vertex;		/* char vertex added on success */

	/*
	 * before we even probe around looking for a flash, check
	 * that a flash node doesn't already exist off of this bridge.
	 * if one does, that means this is a re-load of the driver, and
	 * just use the srflash_state_t hanging off that vertex
	 */
	if (hwgraph_traverse(conn, "flash", &vertex) == GRAPH_SUCCESS) {
		if ( device_info_get(vertex) != NULL ) {
			/* it's already there, we're a re-load */
			return 0;	/* success */
		}
	}


	/*
	 * fall through means this is a first time load, setup a vertex
	 */
	cfg = (widget_cfg_t *) xtalk_piotrans_addr( conn, 0,
						0, sizeof(*cfg), 0 );

	/*XXX check for presence of flash memory on bridge */
	if (srflash_flash_mem_check(cfg) == 0)
		return(0);
	/*
	 * what address do we probe around in to check for our flash?
	 */
	probeaddr = (volatile int16_t *) xtalk_piotrans_addr( conn, 0,
				BRIDGE_EXTERNAL_FLASH, sizeof(int64_t), 0 );

	/*
	 * we have a magic control sequence to send to the flash part
	 * that will return some other magic numbers (if it is our part),
	 * but we need to tell the flash to parse the magic ... when
	 * we turn on BRIDGE_CTRL_FLASH_WR_EN, then it will listen
	 */
	srflash_bridge_wr_enable_on( cfg );

	*probeaddr = SFLASH_CMD_ID_READ;
	mfg = probeaddr[0];
	dev = probeaddr[1];


	if ((mfg == SFLASH_MFG_ID) && (dev == SFLASH_DEV_ID)) {
		*probeaddr = SFLASH_CMD_READ;	/* revert flash to read mode */

		srflash_bridge_wr_enable_off( cfg );

		/*
		 * okay, we have a flash at this vertex ... add a flash
		 * vertex to the hardware graph hanging off this bridge,
		 * and then hang out driver structure off that vertex
		 */

		hwgraph_char_device_add( conn, "flash", "srflash_", &vertex );

		if (!vertex) {
			return -1;		/* vertex add failed */
		}
		else {
			flash = kmem_alloc(sizeof(srflash_state_t), KM_SLEEP);
			mutex_init( &(flash->lock), MUTEX_DEFAULT, 
							"srflash_mutex" );
			flash->state = 0;
			flash->magic = 0x00abcdee;
			flash->bridge_cfg = cfg;
			flash->flashaddr = probeaddr;
			flash->conn = conn;
			flash->devnopen = 0;

			device_info_set( vertex, flash );
		}
	}
	else {
		srflash_bridge_wr_enable_off( cfg );
	}

	return 0;			/* success */
}

/*
 **********************************************************************
 */

/*
 * srflash_open
 *
 * attempt to open the flash prom driver for a specified hwgraph vertex.
 * if we get here, well, we know that the flash exists (wouldn't have
 * had the vertex added in the first place)
 *
 * we allow multiple readers, or one writer.  any other combination
 * will return EBUSY.
 */

/* ARGSUSED */
int
srflash_open( dev_t *devp, int oflag, int otype, cred_t *crp )
{
	srflash_state_t * flash;

	flash = (srflash_state_t *)device_info_get( dev_to_vhdl(*devp) );
	if (!flash) {
printf("sflash: no data hanging off of vertex!!\n");
		return EIO;
	}

	mutex_lock( &(flash->lock), PZERO );

	/*
	 * if the device is already open, and this open wants to have
	 * write access, then fail.  if there already is someone with
	 * the device open for write, fail.
	 */
	if (((flash->devnopen > 0) && (oflag & FWRITE)) ||
				(flash->state & SRFLASH_OPEN_WRITE)) {
		mutex_unlock( &(flash->lock) );
		return EBUSY;
	}
	
	flash->devnopen++;
	if (oflag & FWRITE) {
		flash->state |= SRFLASH_OPEN_WRITE;
	}

	srflash_softpower_disable( flash );
	atomicAddInt( &srflash_nopen, 1 );		/* ++ global open cnt */

	mutex_unlock( &(flash->lock) );

	return 0;
}


/*
 **********************************************************************
 */

/*
 * srflash_close
 *
 * close the device.  clear the flag that says that we have the
 * device open, and re-enable the software power off.
 */

/* ARGSUSED */
int
srflash_close(dev_t dev, int oflag, int otype, cred_t *crp )
{
	srflash_state_t * flash;

	flash = (srflash_state_t *)device_info_get( dev_to_vhdl(dev) );
	if (!flash) {
printf("sflash: no data hanging off of vertex!!\n");
		return EIO;
	}

	mutex_lock( &(flash->lock), PZERO );

	/*
	 * if this is the last time this device is open, clear the 
	 * open for writing flag.  (redundant on open for read, but
	 * what the heck, it catches all cases)
	 */
	if (--flash->devnopen == 0) {
		flash->state &= ~SRFLASH_OPEN_WRITE;
	}

	srflash_softpower_enable( flash );
	atomicAddInt( &srflash_nopen, -1 );		/* -- global open cnt */

	mutex_unlock( &(flash->lock) );

	return 0;
}

/*
 **********************************************************************
 */

/*
 * srflash_read
 *
 * read the current contents of the flash, based on the starting address
 * and length that the caller provided
 */

/* ARGSUSED */
int
srflash_read(dev_t dev, uio_t *uio, cred_t *crp)
{
	int err;				/* tmp */
	caddr_t vaddr;				/* offset to read from */
	srflash_state_t * flash;		/* need to get base addr */
	int nbytes;				/* how much to transfer */

	flash = (srflash_state_t *)device_info_get( dev_to_vhdl(dev) );
	if (!flash) {
printf("sflash: no data hanging off of vertex!!\n");
		return EIO;
	}

	/*
	 * we want to return data up to the end of the region,
	 * but no more.  so we just return 0 and leave the resid
	 * >0, and I believe that's the "EOF" indication
	 */
	nbytes = SFLASH_MAX_SIZE - uio->uio_offset;
	err = 0;				/* if nbytes == 0, return 0 */

	if (nbytes) {
		vaddr = (char *) flash->flashaddr + uio->uio_offset;
		err = uiomove( vaddr, nbytes, UIO_READ, uio );
	}

	return err;				/* uiomove returns 0 on ok */
}

/*
 **********************************************************************
 */

/*
 * srflash_write
 *
 * UNSUPPORTED
 */

/* ARGSUSED */
int
srflash_write(dev_t dev, uio_t *uio)
{
	return ENXIO;
}

/*
 **********************************************************************
 */

/*
 * srflash_ioctl
 *
 * three ioctl operations are supported, all three of them
 * relate to getting, incrementing, and setting the number of
 * times that one of the three "areas" of the flash prom has
 * been erased.
 *
 * the return from this function is either 0 or an error,
 * the current (newly incremented) value is returned in the argument
 */

/*ARGSUSED*/
int 
srflash_ioctl(dev_t dev, uint cmd, caddr_t arg, int flags, cred_t *crp,
        int *rvalp)

{
	int nvoff;			/* which offset to access */
	int nvlen;			/* length of variable */
	int32_t nvvalue;		/* 4 bytes in dallas for each */
	char charbuffer[5];		/* 4 bytes plus a NULL */
	srflash_state_t * flash;	/* global device info */

	flash = (srflash_state_t *)device_info_get( dev_to_vhdl(dev) );
	if (!flash) {
		return EIO;
	}

	/*
	 * all these ioctls require that the flash device be open for write
	 */
	if (!(flash->state & SRFLASH_OPEN_WRITE)) {
		return EPERM;
	}

	switch(cmd) {
		case	SFL_INCGET_RPROM:
				nvoff = NVOFF_NFLASHEDRP;
				nvlen = NVLEN_NFLASHEDRP;
				break;
		case	SFL_INCGET_FPROM:
				nvoff = NVOFF_NFLASHEDFP;
				nvlen = NVLEN_NFLASHEDFP;
				break;
		case	SFL_INCGET_PDS:
				nvoff = NVOFF_NFLASHEDPDS;
				nvlen = NVLEN_NFLASHEDPDS;
				break;
		default:
				return ENXIO;
				/* NOTREACHED */
				break;
	}

	/* this size should never change, but just incase ... */
	if (nvlen != sizeof(nvvalue)) {
		ASSERT(nvlen == sizeof(nvvalue));
		return ENXIO;
	}

	if (ip30_get_an_nvram(nvoff, nvlen, charbuffer)) {
		return ENXIO;
	}
	bcopy(charbuffer, (char*)&nvvalue, sizeof(nvvalue));

	nvvalue++;

	bcopy((char*)&nvvalue, charbuffer, sizeof(nvvalue));
	if (ip30_set_an_nvram(nvoff, nvlen, charbuffer)) {
		return ENXIO;
	}

	*rvalp = nvvalue;
	return 0;
}

/*
 **********************************************************************
 */

/*
 * srflash_map
 *
 * this is the guts of the driver.  all the real work happens in the 
 * user level application, so all that we need to do is map the address
 * space of the flash into the address space of the process that will
 * do the real work.
 */

/* ARGSUSED */
int 
srflash_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot) 
{
	int rc;					/* tmp */
	caddr_t kaddr;				/* physical address to map */
	srflash_state_t * flash;		/* need to get base addr */

	flash = (srflash_state_t *)device_info_get( dev_to_vhdl(dev) );
	if (!flash) {
printf("sflash: no data hanging off of vertex!!\n");
		return EIO;
	}

	if (off+len > SFLASH_MAX_SIZE)
		return ENOSPC;
	if ((off % NBPC) || (len % NBPC))
		return EIO;

	/*
	 * get a mapping to a kernel physical address that we'll
	 * use to map the region
	 */
        kaddr = (caddr_t)xtalk_piotrans_addr(flash->conn, 0, 
					off + BRIDGE_EXTERNAL_FLASH, len, 0);

	if (IS_COMPATK1(kaddr)) {
printf("sflash: kaddr is compat k1 ... to normal k1 with you!\n");
		kaddr = (caddr_t)PHYS_TO_K1(COMPAT_TO_PHYS(kaddr));
	}



	rc = v_mapphys(vt, kaddr, len );

	/*
	 * move flash into the "magic" mode
	 */
	if (flash->state & SRFLASH_OPEN_WRITE) {
		srflash_bridge_wr_enable_on( flash->bridge_cfg );
	}

	return rc;
}

/*
 **********************************************************************
 */

/*
 * srflash_unmap
 *
 * user unmapped the flash address space, undo the magic prom mode
 */

/* ARGSUSED */
int
srflash_unmap(dev_t dev, vhandl_t *vt)
{
	srflash_state_t * flash;		/* need to get base addr */

	flash = (srflash_state_t *)device_info_get( dev_to_vhdl(dev) );
	if (!flash) {
printf("sflash: no data hanging off of vertex!!\n");
		return EIO;
	}

	if (flash->state & SRFLASH_OPEN_WRITE) {
		srflash_bridge_wr_enable_off( flash->bridge_cfg );
	}

	return 0;
}

/*
 **********************************************************************
 */

/*
 * srflash_unload
 *
 * unload the flash prom driver
 * I'm gonna assume that there is some higher level locking that 
 * prevents a loadable driver entry points from being called while
 * the unload routine is active ... it'd be hard to cleanup if you're
 * still worried about getting called ...
 */

int 
srflash_unload(void)
{
	int rv = 0;			/* function return value */

#ifdef DEBUG
	printf("srflash_unload, driver opencnt %d\n",srflash_nopen);
#endif

	if (srflash_nopen) rv = EBUSY;

	return rv;
}
