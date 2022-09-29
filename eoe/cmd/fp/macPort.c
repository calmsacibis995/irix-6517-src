/* 
   (C) Copyright Digital Instrumentation Technology, Inc., 1990 - 1993 
       All Rights Reserved
*/
/*-- DIT TransferPro macPort.c ------------------
 *
 * Contains port-dependent calls for LibMac
 *
 */

#include <sys/param.h>
#include "macSG.h"
#include "dm.h"
#include "fpdevice.h"


/*-- macBlockSizeDevice ------------
 *
 * returns the block size of the device (supported types 512, 1024)
 *
 */

int macBlockSizeDevice( device )
    void *device;
   {
    return (blockSizeDevice (device));
   }

/*-- macLastLogicalBlockDevice ------------
 *
 * returns the last logical block of the device (zero-based)
 *
 */

int macLastLogicalBlockDevice( device )
    void *device;
   {
    return ((capacityReplyDiskDevice(device))->cr_lastlba);
   }

/*-- macVolumeTypeDevice ------------
 *
 * returns the volume type of the device
 *
 */

int macVolumeTypeDevice( device )
    void *device;
   {
    return (volumeTypeDevice (device));
   }

/*-- macLogicalVolumeDiskDevice -----------
 *
 * returns the logical volume for the current device. For devices
 * where the volume coincides with the device, the device is always -1.
 * for mac partitioned disks, it is 0-n.
 *
 */

int macLogicalVolumeDiskDevice( device )
    void *device;
   {
    return ( logicalVolumeDiskDevice(device));
   }

/*-- macReadBlockDevice ---------------------
 *
 */

int macReadBlockDevice( device, buffer, blockstart, blockcount )
    void *device;
    char *buffer;
    int blockstart;
    int blockcount;
   {
    int retval = E_NONE;

    retval = (readBlockDevice ( device, buffer, blockstart, blockcount));

    return retval;
   }

/*-- macWriteBlockDevice ---------------------
 *
 */

int macWriteBlockDevice( device, buffer, blockstart, blockcount )
    void *device;
    char *buffer;
    int blockstart;
    int blockcount;
   {
    int retval = E_NONE;

    retval = (writeBlockDevice (device, buffer, blockstart, blockcount));

    return retval;
   }
