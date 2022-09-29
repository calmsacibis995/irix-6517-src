#include "options.h"

#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>		/* For varargs */
#include <stdlib.h>		/* For string conversion routines */
#include <string.h>
#include <bstring.h>
#include <errno.h>
#include <fcntl.h>		/* for I/O functions */
#include <unistd.h>
#include <sys/stat.h>

#include <sys/sysmacros.h>
#include <sys/major.h>
#include <sys/scsi.h>

/*
 * DEVSCSI definitions
 */
#if !HWG_AVAIL
#define DS_UNIT_MASK    0xF
#define DS_UNIT_SHIFT   0
#define DS_LUN_MASK     0x7
#define DS_LUN_SHIFT    4
#define DS_CTLR_MASK    0x7F
#define DS_CTLR_SHIFT   7
#define DSCSI_MAJOR	195
#endif

#ifndef SCEXT
#define SCEXT(_ctlr) (((_ctlr) >> 3) * 10 + ((_ctlr) & 7))
#endif
#ifndef SCINT
#define SCINT(_ctlr) (((_ctlr) % 10) + (((_ctlr) / 10) << 3))
#endif

#include "dslib.h"
#include "debug.h"
#include "scsi.h"

/*
 ******************************************************************************
 * make_devscsi_path()
 ******************************************************************************
 */
char *make_devscsi_path(int cid, int tid, int lun, char *filename)
{
  char dir_name[128];
  struct stat buf;
  dev_t dev, min, maj;

#if HWG_AVAIL
  sprintf(dir_name, "/dev/scsi");
#else
  sprintf(dir_name, "/tmp/%d", getpid());
#endif

  sprintf(filename, "%s/sc%dd%dl%d", dir_name, cid, tid, lun);

#if !HWG_AVAIL
  if (stat(dir_name, &buf) == -1)
    mkdir(dir_name, S_IRWXU);
  maj = DSCSI_MAJOR;
  min = (((dev_t)SCINT(cid) & DS_CTLR_MASK) << DS_CTLR_SHIFT) |
        (((dev_t)tid & DS_UNIT_MASK) << DS_UNIT_SHIFT) |
        (((dev_t)lun & DS_LUN_MASK) << DS_LUN_SHIFT);
  dev = makedev(maj, min);
  if (mknod(filename, S_IFCHR | S_IRWXU, dev)) {
    fatal("mknod(%s) failed : %s\n", filename, strerror(errno));
  }
#endif

  return(filename);
}

/*
 ******************************************************************************
 * make_scsiha_path()
 ******************************************************************************
 */
char *make_scsiha_path(int cid, char *filename)
{
  char dir_name[128];
  struct stat buf;
  dev_t dev, min, maj;

#if HWG_AVAIL
  sprintf(dir_name, "/hw/scsi_ctlr");
#else
  sprintf(dir_name, "/tmp/%d", getpid());
#endif

  sprintf(filename, "%s/%d", dir_name, cid);

#if !HWG_AVAIL
  if (stat(dir_name, &buf) == -1)
    mkdir(dir_name, S_IRWXU);
  maj = SCSIBUS_MAJOR;
  min = cid;
  dev = makedev(maj, min);
  if (mknod(filename, S_IFCHR | S_IRWXU, dev)) {
    fatal("mknod(%s) failed : %s\n", filename, strerror(errno));
  }
#endif

  return(filename);
}

/*
 ******************************************************************************
 * inquiry()
 ******************************************************************************
 */
int inquiry(char *devname, inq_struct_t *inqdata)
{
  struct dsreq *dsp = NULL;
  int rc;
  int retry = 3;

  if ((dsp = dsopen(devname, O_RDONLY)) == NULL) {
    return(-1);
  }

  fillg0cmd(dsp, CMDBUF(dsp), 
	    G0_INQU, 
	    0, 
	    0, 
	    0, 
	    B1(sizeof(inq_struct_t)), 
	    B1(0));
  filldsreq(dsp, (u_char *)inqdata, sizeof(inq_struct_t), DSRQ_READ|DSRQ_SENSE);
  while (retry--) {
    rc = doscsireq(getfd(dsp), dsp);
    if (rc == 0)
      break;
  }
  dsclose(dsp);

  return(rc);
}

/*
 ******************************************************************************
 * recv_diagnostics()
 ******************************************************************************
 */
int recv_diagnostics(char *devname, u_char pagecode, u_char *buf, u_int *buflen)
{
  struct dsreq *dsp = NULL;
  int rc;
  int retry = 3;

  if ((dsp = dsopen(devname, O_RDONLY)) == NULL) {
    printf("dsopen(%s) failed : %s\n", devname, strerror(errno));
    return(-1);
  }

  fillg0cmd(dsp, CMDBUF(dsp),
	    G0_RDR,
#if PCV_BIT_REQUIRED
	    1,
#else
	    0, 
#endif
	    B1(pagecode), 
	    B2(*buflen), 
	    B1(0));
  filldsreq(dsp, buf, *buflen, DSRQ_READ|DSRQ_SENSE);
  while (retry--) {
    rc = doscsireq(getfd(dsp), dsp);
    if (rc == 0) {
      *buflen = DATASENT(dsp);
      break;
    }
  }
  dsclose(dsp);

  return(rc);
}

/*
 ******************************************************************************
 * send_diagnostics()
 ******************************************************************************
 */
int send_diagnostics(char *devname, u_char *buf, u_int buflen)
{
  struct dsreq *dsp = NULL;
  int rc;
  int retry = 3;

  if ((dsp = dsopen(devname, O_RDONLY)) == NULL) {
    printf("dsopen(%s) failed : %s\n", devname, strerror(errno));
    return(-1);
  }

  fillg0cmd(dsp, CMDBUF(dsp),
	    G0_SD,
	    0x10,
	    0,
	    B2(buflen), 
	    B1(0));
  filldsreq(dsp, buf, buflen, DSRQ_WRITE|DSRQ_SENSE);
  while (retry--) {
    rc = doscsireq(getfd(dsp), dsp);
    if (rc == 0)
      break;
  }
  dsclose(dsp);

  return(rc);
}

int startstop_drive(char *devname, char start)
{
  struct dsreq *dsp = NULL;
  int rc;
  int retry = 3;

  if ((dsp = dsopen(devname, O_RDONLY)) == NULL) {
    printf("dsopen(%s) failed : %s\n", devname, strerror(errno));
    return(-1);
  }

  fillg0cmd(dsp, 
	    CMDBUF(dsp), 
	    G0_STOP,
	    1,
	    0, 
	    0, 
	    start,
	    0);
  filldsreq(dsp, NULL, 0, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time = 1000 * 90; /* 90 seconds */
  while (retry--) {
    rc = doscsireq(getfd(dsp), dsp);
    if (rc == 0)
      break;
  }
  dsclose(dsp);

  return(rc);
}

/*
 ******************************************************************************
 * enable_pbc()
 ******************************************************************************
 */
int enable_pbc(char *devname, u_int tid)
{
#if SCSIHA_AVAIL
  struct scsi_ha_op ha;
  int fd;

  if ((fd = open(devname, O_RDONLY)) == -1) {
    printf("open(%s) failed: %s\n", devname, strerror(errno));
    return(-1);
  }
  ha.sb_opt = tid;
  return(ioctl(fd, SOP_ENABLE_PBC, &ha));
#else
  printf("SCSIHA Enable Bypass not implemented yet.\n");
  return(0);
#endif
}

/*
 ******************************************************************************
 * disable_pbc()
 ******************************************************************************
 */
int disable_pbc(char *devname, u_int tid)
{
#if SCSIHA_AVAIL
  struct scsi_ha_op ha;
  int fd;

  if ((fd = open(devname, O_RDONLY)) == -1) {
    printf("open(%s) failed: %s\n", devname, strerror(errno));
    return(-1);
  }
  ha.sb_opt = tid;
  return(ioctl(fd, SOP_DISABLE_PBC, &ha));
#else
  printf("SCSIHA Disable Bypass not implemented yet.\n");
  return(0);
#endif
}


