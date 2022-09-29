/*-- DiskDevice.h -------
 *   
 (C) Copyright Digital Instrumentation Technology, Inc. 1990 - 1993
       All Rights Reserved

 * Abstract superclass for disk devices.
*/

#ifndef DISKDEVICE_H
#define DISKDEVICE_H

#include "Device.h"

#include "macLibrary.h"

#define MAXDDNAMELEN 63
#ifndef MAX_TRANSFER
#define MAX_TRANSFER 65536
#endif

struct capacity_info 
{
    unsigned int cr_lastlba;
    unsigned int cr_blklen;
};

struct disk_device_info 
{
    char name[MAXDDNAMELEN+1];
    int labelLocation[4];
    int sectorSize;
    int maxTransferSize;  /* in bytes */
};

struct DiskDevice
{
    struct Device device;                    /* DiskDevice is a "subclass" of 
                                                Device */
    char deviceMountPoint[ MAXPATHLEN+1 ];   /* Instance variables */
    struct disk_device_info driveInfoData;
    struct capacity_info capacityReplyData;
    int logicalVolume;
    int deviceBlockStart;
    int deviceBlockCount;

    int (*fsTypeFcn)();                  /* methods */
    int (*ejectFcn)();
    char *(*mountPointFcn)();
    int (*setMountPointFcn)();
    int (*logicalVolumeFcn)();
    int (*setLogicalVolumeFcn)();
    struct disk_device_info *(*driveInfoFcn)();
    struct capacity_info *(*capacityReplyFcn)();
    int (*resetDriverFcn)();
    void (*setDeviceLocationFcn)();
};


struct DiskDevice *newDiskDevice();
int freeDiskDevice();
int initDiskDevice();

BOOL volumeReadyDiskDevice();
int volumeTypeDiskDevice();
int fsTypeDiskDevice();
char *mountPointDiskDevice();
int setMountPointDiskDevice();

/* the logical volume is used to indicate which logical volume is currently
   in use on this UNIX device. If the logical volume corresponds exactly to
   the device, logical volume can be 0 or -1. Subclassed code is responsible
   for subclassing the seek and capacity commands. the -format method
   always resets the logical volume to -1.
*/
int logicalVolumeDiskDevice(void *);
int setLogicalVolumeDiskDevice();

int capacityDiskDevice();
int blockSizeDiskDevice();
struct disk_device_info *driveInfoDiskDevice();
struct capacity_info *capacityReplyDiskDevice(void *);
int resetDriverDiskDevice();
extern void setDeviceLocationDiskDevice(void *, int, u_int);
int seekDiskDevice();
#endif
