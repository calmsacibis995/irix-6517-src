/*-- Device.h ---------------
 
     (C) Copyright Digital Instrumentation Technology, Inc. 1990 - 1993
       All Rights Reserved

  This is an abstract superclass that is used to support any device accessible
  from a UNIX device file. There will probably be one subclass for each
  driver type, e.g. SdDevice FdDevice, OdDevice, etc., perhaps with
  intermediate classes to cover device groups, such as disk device, tape
  device, etc.
*/

#ifndef DEVICE_H
#define DEVICE_H

#include "macLibrary.h"
#include <sys/file.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/fcntl.h>

#define VOL_NONE  -1
#define VOL_720    0
#define VOL_1_44   1
#define VOL_2_88   2
#define VOL_HD     3
#define VOL_NFS    4
#define VOL_400    5
#define VOL_800    6
#define VOL_40MB   7
#define VOL_MAC_HD 8

#define DEFAULT_BLOCK_SIZE 512
typedef void *DITVOLUME;

struct Device
   {
    char deviceFile[ MAXPATHLEN+1 ];
    int  deviceFd;

    int (*freeFcn)();
    int (*openFcn)();
    int (*closeFcn)();
    int (*setFileFcn)();
    int (*fdFcn)();
    char *(*fileFcn)();
    int (*readFcn)();
    int (*writeFcn)( );
    int (*seekFcn)( );
    int (*readBlockFcn)( );
    int (*writeBlockFcn)( );
    int (*blockSizeFcn) ( );
    int (*willEjectFcn) ();
    int (*willFormatFcn) ();
    int (*ejectFcn) ();
    int (*formatFcn) ();
    char *(*volumeNameFcn) ( );
    int (*volumeTypeFcn) ( );
    int (*volumeChangedFcn) ( );
    int (*volumeReadyFcn) ( );
    int (*volumeProtectedFcn) ( );
    void *(*volumeInfoFcn) ( );
    int (*setVolumeNameFcn)( );
    int (*capacityFcn)( );
   };

extern struct Device *newDevice();
extern int initDevice();
extern int freeDevice();

extern int openDevice(void *, int);
extern int setFile();
extern int closeDevice();
extern int fdDevice();
extern char *fileDevice();
extern int readDevice();
extern int writeDevice( );
extern int seekDevice( );

/* The device and the calling code must agree on the size of a
   block. The size assumed in this class is DEFAULT_BLOCK_SIZE (512)
*/
extern int readBlockDevice(void *, char *, int, int);
extern int writeBlockDevice(void *, char *, int, int);
extern int blockSizeDevice (void *);

extern int ejectDevice (void *);
extern int formatDevice ();

char *volumeNameDevice ( );
int volumeTypeDevice (void *);
void *volumeInfoDevice ( );
int setVolumeNameDevice( );

int capacityDevice( );

#endif
