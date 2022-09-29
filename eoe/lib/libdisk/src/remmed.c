/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1988, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* remmedinfo() */


#include <sys/types.h>
#include <sys/invent.h>
#include <diskinfo.h>
#include <invent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/scsi.h>
#include <dslib.h>
#include <sys/sysmacros.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/smfd.h>
#include <sys/conf.h>
#include <sys/iograph.h>

static int device_is_adtron(dsreq_t *);
int cardinfo(dsreq_t *dsp, char *info, char lun);
void check_status(dsreq_t *dsp, int *info);
int fd_to_scsi_name(int, char *);
#define CARD_INFO 0xC0
/*
 * Given a file descriptor for an open dksc node, this function.
 * will return information about the PC card media status.
 * The three bits of information returned are the same as the 
 * ones define in smfd.h: SMFDMEDIA_READY, SMFDMEDIA_CHANGED and 
 * SMFDMEDIA_WRITE_PROT. This function attempts to mimic the SMFDMEDIA 
 * ioctl for PC cards. Since card info is unique to the Adtron card 
 * reader, it is not guaranteed to work with other devices.
 */
int remmedinfo( int fd, uint *info  )
{
   char		devname[MAXDEVNAME];
   char		*tmpname;
   char		card_info[14];
   dsreq_t	*dsp;
   char		data[4];
   int		lun;

   /*
    * only valid for dksc files.
    */
   if (fd_to_scsi_name(fd, devname) < 0)
	   return(EBADF);
   dsp = dsopen (devname, O_RDWR );
   
   if (dsp == NULL)
       return(ENXIO);

   testunitready00(dsp);

   /* check_status will return status of media in info */
   check_status((dsreq_t *)dsp, (int *)info);
   tmpname = strstr(devname, EDGE_LBL_SCSI "/");
   if (!tmpname)
	   return (ENXIO);
   sscanf(tmpname, "%d", &lun);
   /* unique to Adtron */
   if (!device_is_adtron(dsp) || cardinfo(dsp, card_info, lun) != 0) {
       /* if unsupported ... */
       /* this is copied from smfd.c */ 
       modesense1a( dsp, data, 4, 0/*pg ctrl*/, 5 /*pg*/, 0 /*vu*/);
       check_status((dsreq_t *)dsp, (int *)info);
       
       if (data[0] > 2 && data[2] & 0x80)
          *info |= SMFDMEDIA_WRITE_PROT;
   } else {
       /* cardinfo worked, check the header */
       if( card_info[0] & 0x8 )
          *info |= SMFDMEDIA_READY;
       if( card_info[0] & 0x80 )
          *info |= SMFDMEDIA_WRITE_PROT;
       if( card_info[0] & 0x4 )
          *info |= SMFDMEDIA_CHANGED;
  }
  return(0);
}

/*
 *  Verify that given device is an Adtron PC card reader by sending an
 *  INQUIRY command and verifying that the returned product
 *  identification starts with "SDDS".  This is the same test as used
 *  in kern/io/wd93.c.
 */

static int device_is_adtron(dsreq_t *dsp)
{
    char inquiry[36];

    if (inquiry12(dsp, inquiry, sizeof inquiry, 0) != STA_GOOD)
	return 0;
    return strncmp(inquiry + 16, "SDDS", 4) == 0;
}

/* 
 * Given an open dsp, and a lun this function will send 
 * a CARD_INFO command and return the information in 
 * the buffer (14 bytes). 
 * Returns: 0 if successful or the ds return code from the dsp.
 */
int cardinfo(dsreq_t *dsp, char *card_info, char lun)
{
   fillg0cmd(dsp, (uchar_t *)CMDBUF(dsp), CARD_INFO, lun << 5,
                     0, 0, 14, 0 );
   filldsreq(dsp, (uchar_t *)card_info, 14, DSRQ_READ);
   doscsireq(getfd(dsp), dsp);
   if (STATUS(dsp) != STA_GOOD)
       return(dsp->ds_ret);
   return(0);
}


/*
 * Sets bits in info indicating the status of the media. 
 * The info returned is identical to that provided by the
 * SMFDMEDIA ioctl provided by the floppy driver.
 */
void check_status(dsreq_t *dsp, int *info)
{
   char key, asc, asq;

   *info = 0;
   if( STATUS (dsp) == STA_GOOD ) {
         *info = SMFDMEDIA_READY;
         return;
   }

   if ( STATUS(dsp) == STA_CHECK ) {
       if( dsp->ds_ret == DSRT_SENSE /* sense data returned */ ) {
           key = dsp->ds_sensebuf[2] & 0xf;
           asc = dsp->ds_sensebuf[12];
           asq = dsp->ds_sensebuf[13];
           if ( key == SC_UNIT_ATTN && ((asc == 0x28)
                || (asc == 0x3F && asq == 0x3)) )
                 *info |= SMFDMEDIA_CHANGED;
       }
   }
   return;
}
/*
 *  Given a dksc device's file descriptor, return the corresponding
 *  scsi device name.
 */

int
fd_to_scsi_name(int dkfd, char *sdev)
{
    char		devname[MAXDEVNAME];
    char		*tmpname;
    int		devnmlen = MAXDEVNAME;

    /*
    * only valid for dksc files.
    */
    if (fdes_to_drivername(dkfd, devname, &devnmlen)) {
	    if(strncmp(devname, "dksc", 4) != 0)
		    return -1;
	    devnmlen = MAXDEVNAME;
	    if (fdes_to_devname(dkfd, devname, &devnmlen)) {
		    tmpname = strstr(devname, EDGE_LBL_DISK);
		    strcpy(tmpname, EDGE_LBL_SCSI);
		    strcpy(sdev, devname);
		    return 0;
	    }
    } 
    return -1;
}


