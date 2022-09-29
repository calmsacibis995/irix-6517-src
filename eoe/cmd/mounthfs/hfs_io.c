/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.17 $"

#include <errno.h>
#include <fcntl.h>
#include <sys/dkio.h>
#include <sys/dvh.h>
#include <sys/param.h>
#include <sys/smfd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/conf.h>
#include <sys/iograph.h>

#include "hfs.h"


/*********
 * io_open	Open a file or device for use by the file system.
 */
int io_open(char *pathname, hfs_t *hfs){
  struct stat sb;

  u_int  status = 0;
  u_int	 capacity;

  struct volume_header vh;
  struct partition_table *pt;

  char    *str;
  char    *pattern = EDGE_LBL_VOLUME"/"EDGE_LBL_CHAR;
  char    devname[MAXDEVNAME];
  int     part, len1, len2;


  len1 = strlen(pattern);
  len2 = MAXDEVNAME;
  /*
   * Stat the path to check if it points to a /dev/scsi device.
   * If so, call cd_open to open the device.
   */
  if (stat(pathname,&sb) < 0)
	  return errno;
  if (S_ISCHR(sb.st_mode)) {
	  if (dev_to_drivername(sb.st_rdev, devname, &len2)) 
		  if(strncmp(devname, "ds", 2) == 0) {
			  
			  int error;
			  if (error=cd_open(pathname,32,&hfs->fs_cd))
				  return error;
			  if (error=cd_set_blksize(hfs->fs_cd,BBSIZE))
				  return error;
			  if (FS_ISCLR(hfs,RDONLY))
				  return EROFS;
			  FS_SET(hfs,CDROM);
			  
			  /*
			   * Note: Should pick the physical
			   * block size up from the driver descriptor
			   * in physical block 0.
			   */
			  hfs->fs_pbsize = 512;
			  return 0;
		  }
	  /*
	   * Check for floppy - it's removable media and needs special checks.
	   */
	  if(strncmp(devname, "smfd", 4) == 0)
		  FS_SET(hfs,FLOPPY);
  }

  /*
   * Do not allow access to write protected media under RDWR mode.
   */
  if ((hfs->fs_fd =
       open(pathname,
	    FS_ISSET(hfs,RDONLY) ? O_RDONLY|O_EXCL : O_RDWR|O_EXCL) ) < 0 )
    return errno;
  if (!(hfs->fs_flags & FS_RDONLY) && FS_ISSET(hfs,FLOPPY)){
    if (ioctl(hfs->fs_fd,SMFDMEDIA, &status)!=0)
      goto err;
    if (status & SMFDMEDIA_WRITE_PROT){
      close(hfs->fs_fd);
      return EROFS;
    }
  }
  hfs->fs_pbsize = BBSIZE;

  /*
   *  Check for invalid volume header and install valid one.  dkXX
   *  drivers support DIOCGETVH and DIOCSETVH.  smfd driver supports
   *  DIOCGETVH only.  We're very surprised if HFS disks have valid SGI
   *  volume headers, but if they do, don't muck with them.
   */

  /* Warning: valid_vh() returns nonzero if header is NOT valid. */

  if (fstat(hfs->fs_fd, &sb) < 0)
	goto err;

  if (!FS_ISSET(hfs, FLOPPY) && ioctl(hfs->fs_fd, DIOCGETVH, &vh) == 0)
  {
      /* only allow access to the whole-volume device. */

      len1 = strlen(pattern);
      len2 = MAXDEVNAME;

      dev_to_devname(sb.st_rdev, devname, &len2);
      len2 = strlen(devname);

      str = devname + (len2-len1);
      if (!strcmp(pattern, str)){
              part = 10;
              pt = &vh.vh_pt[part];
      }
      else  part = -1;

      if (part != 10)
      {	  close(hfs->fs_fd);
	  hfs->fs_fd = -1;
	  return ENODEV;
      }

      if (ioctl(hfs->fs_fd, DIOCREADCAPACITY, &capacity) != 0)
	  goto err;

      bzero(&vh, sizeof vh);
      vh.vh_magic          = VHMAGIC;
      vh.vh_dp.dp_drivecap = capacity;
      pt->pt_nblks         = capacity;
      pt->pt_firstlbn      = 0;
      pt->pt_type          = PTYPE_RAW;
      vh.vh_csum           = -vhchksum(&vh);

      (void) ioctl(hfs->fs_fd, DIOCSETVH, &vh);	/* fails on smfd */
  }
  return 0;


 err:
  close(hfs->fs_fd);
  return errno;
}


/**********
 * io_close	Close a file in use by the file system.
 */
int io_close(hfs_t *hfs){
  if (FS_ISSET(hfs,CDROM))
    cd_close(hfs->fs_cd);
  else
    close(hfs->fs_fd);
  return 0;
}


/*********
 * io_isfd      Check for matching file descriptor.
 */
int io_isfd(hfs_t *hfs, int fd){
  if (FS_ISSET(hfs,CDROM))
    return cd_is_dsp_fd(hfs->fs_cd,fd);
  return hfs->fs_fd == fd;
}


/*************
 * io_capacity	Return device capacity.
 */
int io_capacity(hfs_t *hfs, u_int *capacity){
  struct stat sb;
  mode_t md;

  if (FS_ISSET(hfs,CDROM)){
    int error;
    if (error=cd_stat(hfs->fs_cd,&sb))
      return error;
    *capacity = sb.st_blocks;
    return 0;
  }

  if (fstat(hfs->fs_fd,&sb))
    return errno;
  md = sb.st_mode & S_IFMT;

  if (md==S_IFREG)
    *capacity = sb.st_blocks;
  else
    if (md==S_IFCHR){
      if (ioctl(hfs->fs_fd,DIOCREADCAPACITY,capacity))
	return errno;
    }
    else
      return EIO;
  return 0;
}


/**********
 * io_pread	Physical block read.
 */
int io_pread(struct hfs *hfs, u_int blk, u_int count, void *buf){
  if (FS_ISSET(hfs,CDROM)){
    int error;

    /*
     * NOTE: The CDROM driver contains its own logical to physical mapping
     *       code so we map the block from physical back to logical so that
     *       everything works out.  This is a hack to avoid working on the
     *       cdrom driver.  Note also that this hack hasn't really been
     *       tested well as the only testing has involved drives that
     *       can operate in 512 byte sector mode.
     */
    blk = PTOL(hfs,blk);
    if (error=cd_read(hfs->fs_cd,BBTOB(blk),buf,count,1))
      return error;
    return 0;
  }

  lseek(hfs->fs_fd, BBTOB(blk), 0);
  if ((read(hfs->fs_fd, buf, count)) != count){
    syslog(LOG_CRIT,"read error: %d, block: %d",errno,blk);
    return errno;
  }
  return 0;
}


/**********
 * io_lread	Logical block read.
 */
int io_lread(hfs_t * hfs, u_int blk, u_int count, void *buf){
  if (LOGICALRANGECHECK(hfs,blk,count))
    return ENXIO;
  return io_pread(hfs, LTOP(hfs, blk), count, buf);
}


/**********
 * io_aread	Allocation block read.
 */
int io_aread(hfs_t * hfs, u_int blk, u_int count, void *buf){
  return io_lread(hfs, ATOL(hfs, blk), count, buf);
}


/***********
 * io_pwrite	Physical block write.
 */
int io_pwrite(hfs_t * hfs, u_int blk, u_int count, void *buf){

  /* Fail if it's a read only file system */

  if (FS_ISSET(hfs,RDONLY)||FS_ISSET(hfs,CORRUPTED))
    return EROFS;

  /*
   * Force out the mdb if it's marked as unmounted.
   */
  __chk(hfs,fs_push_mdb(hfs),io_pwrite);


  /* Write the data */

  lseek(hfs->fs_fd, BBTOB(blk), 0);
  if (write(hfs->fs_fd, buf, count) != count)
  { syslog(LOG_CRIT,"write error: %d, block: %d", errno, blk);
    return errno;
  }

  /*
   * Increment the mdb write count.
   * Don't set the MDB as dirty, that would result in the MDB being written
   *  out simply to track the write count.  We assume that eventually the
   *  mdb will have more significant attributes changes and that will force
   *  out the write count.
   */
  hfs->fs_mdb.m_WrCnt++;
  return 0;
}


/***********
 * io_lwrite	Logical block write.
 */
int io_lwrite(hfs_t * hfs, u_int blk, u_int count, void *buf){
  if (LOGICALRANGECHECK(hfs,blk,count))
    return ENXIO;
  return io_pwrite(hfs, LTOP(hfs, blk), count, buf);
}


/***********
 * io_awrite	Allocation block write.
 */
int io_awrite(hfs_t * hfs, u_int blk, u_int count, void *buf){
  return io_lwrite(hfs, ATOL(hfs, blk), count, buf);
}

/*
 *-----------------------------------------------------------------------------
 * io_awrite_partial()
 * This routine is used to write out certain portions of a allocation block.
 * When a high level write is to only a part of an allocation block, I think it
 * makes more sense to write out only those logical blocks inside it that are
 * actually being modified. Now, if only a part of a logical block is being mod-
 * ified, then we need to read it in first before altering it.
 *
 * Parameters:
 *      alb_num = Allocation block number
 *      alb_off = Byte offset within allocation block
 *      count   = Number of bytes to be written out to disk
 *      data    = Starting buffer address to write out to disk
 *-----------------------------------------------------------------------------
 */
int
io_awrite_partial(hfs_t *hfs, u_int alb_num, u_int alb_off,
                  int count, char *data)
{
        int     error;
        char    *buf, *src, *dst;
        char    *alb_buf = hfs->fs_buf;

        u_int   albsize;
        u_int   lbn_albstart;
        u_int   lbnum, lbsize, num;
        u_int   first_lblk, numbr_lblk;

        albsize = hfs->fs_mdb.m_AlBlkSiz;
        hfs->fs_alb = alb_num;
        bzero(alb_buf, albsize);

        lbn_albstart = ATOL(hfs, alb_num);
        lbsize = BBSIZE;
        lbnum  = lbn_albstart + alb_off/lbsize;

        first_lblk = lbnum;                      /* First logical blk */
        numbr_lblk = 0;                          /* Numbr logical blk */
        buf = alb_buf + (alb_off/lbsize)*lbsize; /* First logical blk pointer */

        if (alb_off % lbsize){
                /*
                 * The very first logical block being written to
                 * is written to starting at a non-logical block
                 * aligned offset. We therefore have to read it.
                 */
                error = io_lread(hfs,  lbnum, lbsize, buf);
                if (error)
                        return (error);

                src = data;
                dst = alb_buf+alb_off;
                num = lbsize - alb_off % lbsize;
                bcopy(src, dst, num);

                /*
                 * Update the following variables:
                 * src, dst, count, lbnum, numbr_lblk;
                 */
                src += num;
                dst += num;
                count -= num;
                numbr_lblk++;
                lbnum++;
        }
        else {
                /*
                 * The very first logical block being written
                 * is going to be overwritten starting at the
                 * beginning of the logical block inside it.
                 */
                src = data;
                dst = &alb_buf[alb_off];
        }
        /*
         * We look at all the logical blocks inside this
         * allocation block that need to be written to disk.
         * We're about to write to the beginning of a logical blk.
         */
        while (count){
                /*
                 * Check to see if this logical block is being
                 * written to only partially. If so, read it in
                 */
                if (count < lbsize){
                        /* Read in logical block */
                        /* Modify required portion */

                        error = io_lread(hfs, lbnum, lbsize, dst);
                        if (error)
                                return (error);

                        num = count;
                        bcopy(src, dst, num);

                        /* Update: src, dst, count, lbnum, numbr_lblk */

                        src += num;
                        dst += num;
                        count -= num;
                        numbr_lblk++;
                        lbnum++;
                        break;
                }
                /* Nope, we're writting to an entire logical block */
                /* We therefore need not perform a read before writing */
                num = lbsize;
                bcopy(src, dst, num);

                /* Update: src, dst, count, lbnum, numbr_lblk */

                src += num;
                dst += num;
                count -= num;
                numbr_lblk++;
                lbnum++;
        }
        /* We've refrained from writing until we've viewed all */
        /* Logical blocks that are part of this allocation block */
        /* We now proceed with the actual logical block write */

        error = io_lwrite(hfs, first_lblk, numbr_lblk*lbsize, buf);
        return (error);
}

/*
 * HFS structures are stored on disk as "packed" structures.  Pack and
 * unpack are used to convert structures back and forth between incore
 * and disk format.  Both are called with the address of a source and
 * a destination buffer as well as the address of a structure layout table.
 *
 * The structure layout table is a sequence of halfwords:
 *
 *   	sizeof structure member 0,   offset of structure member 0
 *   	sizeof structure member 1,   offset of structure member 1
 *   	sizeof structure member 2,   offset of structure member 2
 *	           :			        :
 *                 0                            0
 * Data is copied from the source buffer to the destination buffer,
 * realigning as necessary.
 */

/********
 * unpack
 */
void unpack(void* src, void* dst, u_short *mv){
  register u_char *f = (u_char*)src;
  register u_char *t = (u_char*)dst;
  register int fo=0,to;
  int size;

  while (size = *mv++){
    to = *mv++;
    switch(size){
    case 4:
      t[to++] = f[fo++];
      t[to++] = f[fo++];
    case 2:
      t[to++] = f[fo++];
    case 1:
      t[to++] = f[fo++];
      break;
    default:
      bcopy(&f[fo],&t[to],size);
      fo +=size;
    }
  }
}


/******
 * pack	Pack data into a buffer.
 */
u_int pack(void* src, void* dst, u_short *mv){
  register u_char *f = (u_char*)src;
  register u_char *t = (u_char*)dst;
  register int fo,to=0;
  register int totalsize=0;
  register int size;

  while (size = *mv++){
    totalsize += size;
    fo = *mv++;
    switch(size){
    case 4:
      t[to++] = f[fo++];
      t[to++] = f[fo++];
    case 2:
      t[to++] = f[fo++];
    case 1:
      t[to++] = f[fo++];
      break;
    default:
      bcopy(&f[fo],&t[to],size);
      to +=size;
    }
  }
  return totalsize;
}
