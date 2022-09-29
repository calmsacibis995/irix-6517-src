/* 
   (C) Copyright Digital Instrumentation Technology, Inc., 1990 - 1993 
       All Rights Reserved
*/

/* DIT TransferPro  upper level functions for access to MAC diskette data  */



#include <sys/param.h>
#include "Device.h"
#include "macPort.h"
#include "smfd.h"
#include "dm.h"
#include "macLibrary.h"

static char *Module = "dm.c";

/*-- dm_format ------
 *  format a mac disk
 */
int dm_format( aDevice, diskname, Iflag )
    void *aDevice;
    char *diskname;
    int Iflag;   /* for future use to determine whether the driver support formatting*/
   {
    int retval = E_NONE;
    char *funcname = "mac_format";
    char *dname = (*diskname)? diskname : "Disk";
    int mediatype = (aDevice)? macVolumeTypeDevice(aDevice) : VOL_NONE;
	int sizeDisk = macLastLogicalBlockDevice( aDevice);
    struct m_VIB vibBuf;
    struct m_volume volume;

    volume.device = aDevice;
    volume.vib = &vibBuf;
    if (sizeDisk > 21 * 1024 * 2 /* 21 Mb */)
    {
	volume.vstartblk = 3;		/* 1 for block0, 2 for partition map */
	volume.vblksize  = sizeDisk + 1 - 3;
    }
    else
    {   volume.vstartblk = 0;
	volume.vblksize  = sizeDisk + 1;
    }

    if ( !aDevice )
       {
        /* DIT error handling -- device not exist */

        retval = set_error( E_NOTSUPPORTED, Module, funcname,
                                   "Device not initialized" ); 
       }
   
    if (retval != E_NONE)
	return retval;

    if (volume.vstartblk != 0)
    {
	/* write block 0 */
	if ((retval = mac_block0_init(aDevice)) != E_NONE)
	    return retval;

	/* write the partition table */
	if ((retval = mac_partition_init(aDevice)) != E_NONE)
	    return retval;
    }
    else
    {
	/* write the boot blocks */
	if ((retval = mac_boot_init(aDevice)) != E_NONE)
	    return retval;
    }

    /* write the VIB */
    if ((retval = mac_vib_init(&volume,dname,mediatype,Iflag)) != E_NONE)
	return retval;

    /* write the VBM */
    if ((retval = mac_vbm_init(&volume)) != E_NONE)
	return retval;

    /* write the extents tree */
    if ((retval = mac_extent_init(&volume)) != E_NONE)
	return retval;

    /* write the catalog tree */
    if ((retval = mac_catalog_init(&volume,dname)) != E_NONE)
	return retval;

    /* write the desktop file */
    if ((retval = mac_desktop_init(&volume)) != E_NONE)
	return retval;
    return E_NONE;
}
