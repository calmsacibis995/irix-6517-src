/*
 * FpDevice.c, the functions emulation file to DIT device.
 *
 * TODO:
 *
 */

#include <bstring.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/smfd.h>
#include <sys/dkio.h>
#include <sys/mkdev.h>
#include <unistd.h>

#include "mkfp.h"
#include "macSG.h"
#include "fpdevice.h"  /* this file replace the DsDevice.h */

extern int debug_flag;

#define    SECTOR_SIZE    (512)

#define E_NONE             0
#define E_READ         15003
#define E_WRITE        15004

int mac_volabel(FPINFO *, char *);

static char *Module = "FpDevice";

struct DsDevice * 
newDsDevice (FPINFO * fpinfop)
{
    struct DsDevice * fpdevicep;

    fpdevicep = (struct DsDevice *) safemalloc (sizeof (struct DsDevice));
    bzero(fpdevicep, sizeof *fpdevicep);

    strncpy(fpdevicep->diskdevice.device.deviceFile, fpinfop->dev, MAXPATHLEN);

    fpdevicep->diskdevice.device.deviceFd = fpinfop->devfd; 

    fpdevicep->diskdevice.logicalVolume = -1;

    fpdevicep->diskdevice.driveInfoData.sectorSize = 
					    fpinfop->vh.vh_dp.dp_secbytes;

    fpdevicep->diskdevice.capacityReplyData.cr_lastlba =
					    fpinfop->n_sectors - 1;

    fpdevicep->diskdevice.capacityReplyData.cr_blklen = 
					   fpinfop->vh.vh_dp.dp_secbytes;

    if (debug_flag > 1) {
	printf("newDsDevice: fpinfop/0x%x secsz/%d, lastlba+1/%u\n",
				fpinfop,
				fpinfop->vh.vh_dp.dp_secbytes,
				fpinfop->n_sectors);
    }

    return ((void *) fpdevicep);
}



BOOL 
volumeReadyDevice(void * fdp)
{
    /* don't understand so far */
    return ((BOOL)1);
}



int 
blockSizeDevice(void * devicep)
{
    struct DsDevice * fdp;

    fdp = (struct DsDevice *) devicep;

    return(fdp->diskdevice.driveInfoData.sectorSize);
}



struct capacity_info *
capacityReplyDiskDevice(void * devicep)
{
    struct DsDevice * fdp;

    fdp = (struct DsDevice *) devicep;

    return(&(fdp->diskdevice.capacityReplyData));
}



int
volumeTypeDevice(void * devicep)
{
    struct DsDevice * fdp;
    struct stat sb;
    int         devminor;

    fdp = (struct DsDevice *) devicep;
    if (fstat(fdp->diskdevice.device.deviceFd, &sb) < 0) {
        close(fdp->diskdevice.device.deviceFd);
        return(VOL_NONE); 
    } else {
        devminor = (int) minor(sb.st_rdev);
        return(devminor & SMFD_TYPE_MASK);
    }
}



int 
logicalVolumeDiskDevice(void * devicep)
{
    struct DsDevice * fdp;

    fdp = (struct DsDevice *) devicep;

    return(fdp->diskdevice.logicalVolume);
}



int
readBlockDevice(void * devicep, char * buf, int blkstart, int blkcount)
{
    int fd;
    int blksize;
    struct DsDevice * fdp;
    int retval = E_NONE;

    fdp = (struct DsDevice *) devicep;
    blksize = fdp->diskdevice.driveInfoData.sectorSize;
    if ((fd = fdp->diskdevice.device.deviceFd) == -1)
        return(E_READ);

    if (lseek64(fd, ((off64_t)blkstart*(off64_t)blksize), SEEK_SET) == -1)
	retval = set_error (E_READ, Module, "readBlockDevice",
			    "Can't seek");

    if (debug_flag > 1) {
	    off64_t w;

	    w = lseek64(fd, 0, SEEK_CUR);
	    printf("readBlockDevice: read @ %lld (sector %lld) size %d\n",
				    w, w / SECTOR_SIZE, (blkcount*blksize));
    }

    if (read(fd, (void *)buf, blkcount*blksize) != blkcount * blksize)
	retval = set_error (E_READ, Module, "readBlockDevice",
			    "Can't read device");

    return retval;
}



int
writeBlockDevice(void * devicep, char * buf, int blkstart, int blkcount)
{
    int fd;
    int blksize;
    struct DsDevice * fdp;
    int retval = E_NONE;
    int byteno, chunksize;

    fdp = (struct DsDevice *) devicep;
    blksize = fdp->diskdevice.driveInfoData.sectorSize;
    if ((fd = fdp->diskdevice.device.deviceFd) == -1)
        return(E_WRITE);

    lseek64(fd, ((off64_t)blkstart*(off64_t)blksize), SEEK_SET);
    for (byteno = 0; byteno < blkcount * blksize; byteno += chunksize)
    {   chunksize = blkcount * blksize - byteno;
	if (chunksize > 1024 * 1024)
	    chunksize = 1024 * 1024;	/* arbitrarily limit DMA size */

	if (debug_flag > 1) {
		off64_t w;

    		printf("writeBlockDevice: blkstart/%d lastlba/%d secsz/%d\n",
				blkstart,
				fdp->diskdevice.capacityReplyData.cr_lastlba,
				fdp->diskdevice.driveInfoData.sectorSize);

		w = lseek64(fd, 0, SEEK_CUR);
		printf("writeBlockDevice: write @ %lld (sector %lld) size %d\n",
						w, w / SECTOR_SIZE, chunksize);
	}

	if (write(fd, buf + byteno, chunksize) != chunksize)
	{
	    if (errno == EROFS)
	    {
		/* Write-protected */
		retval = set_error(E_PROTECTION, Module, "writeBlockDevice",
				   "Medium is write-protected");
	    }
	    else
		retval = set_error (E_WRITE, Module, "writeBlockDevice",
				    "Can't write to medium!");
	}
    }

    return retval;
}



void
setDeviceLocationDiskDevice(void * devicep, int mapextent, u_int PartBlkCnt)
{
    /* don't understand so far */
    return;
}



int
ejectDevice (void * devicep)
{
/* not implememted yet */

    return(E_NONE);
}



void
freeDsDevice (void ** devicepp)
{
    if (*devicepp != NULL)
        free(* devicepp);
}



int
mac_volabel(FPINFO * fpinfop, char * labelp)
{
    char * strp = labelp;
    int count = 0;

    while (* strp &&  count < MAX_MAC_VOL_NAME) {
        if (!isprint(*strp) || *strp=='\t' || *strp==':')
            fpinfop->volumelabel[count] = '-';
        else
            fpinfop->volumelabel[count] = *strp;

        strp++;
        count++;
    }
    return(E_NONE);
}
