#ifndef __SYS_DSGLUE_H__
#define __SYS_DSGLUE_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * dsglue.h - devscsi glue layer
 *
 *	Copyright 1988, 1989, by
 *	Gene Dronek (Vulcan Laboratory) and
 *	Rich Morin  (Canta Forda Computer Laboratory).
 *	All Rights Reserved.
 *	Ported to SGI by Dave Olson 3/89
 *
 *	This file is normally used only by the kernel.  All system-
 *	dependent items should be here.  See also dsreq.h
 */
#ident "$Revision: 1.25 $"

/*
 *  The DEVSCSI driver uses the UNIX minor device number to determine
 *  SCSI bus (controller), target ID, and LUN.
 *  Up to (B_MAX) buses can be supported (see SCSI_MAXCTLR in sys/scsi.h).
 *  Each SCSI bus may theoretically support up to 32 (I_MAX) SCSI devices,
 *  including the host adaptor, though most will only support 8 or 16.  
 *
 *  Furthermore, DEVSCSI uses the UNIX minor device number to encode
 *  the device ID, LUN, and two (installation-dependent) control bits.
 *  The DEVSCSI driver supports 8 LUN's (L_MAX) per device, but some
 *  installations may not support more than one LUN.
 *
 *        minor bits
 *     17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 *     -----------------------------------------------------
 *      R  R  R  R  B  B  B  B  B  B  B  L  L  L  T  T  T  T
 *
 *  macros for converting  dev to bus, ctrl, lun and id
 *
 *  At SGI, the control bits are just used to indicate controller number.
 *  Different major numbers are used to indicate different types of
 *  host adaptor (I.e. VME-SCSI host adaptor vs. SGI SCSI host adaptor)
 *  A function must be called to get controller number.
 */
#define DS_UNIT_MASK   0xF
#define DS_UNIT_SHIFT    0
#define DS_LUN_MASK    0x7
#define DS_LUN_SHIFT     4

#define get_id(dev)	(SDEVI_TARG(scsi_dev_info_get(dev)))
#define get_lun(dev)	(SDEVI_LUN(scsi_dev_info_get(dev)))
#define get_bus(dev)	(SDEVI_ADAP(scsi_dev_info_get(dev)))
#define get_role(dev)	(0)

/* g_status definitions */
#define BUS_INIT	1	/* bus (and gluer) initialized and up */

#define SPLSCSI spl5 	/* high enough to disable scsi interrupts */

#ifdef __sgi
# define _CALLOC(ptr, x) { ptr = kern_calloc(x, 1); }
# define _CFREE(x) kern_free(x)
#else /* AUX */
# define _CALLOC(ptr, x) { ptr = kmem_alloc(x); if(ptr) bzero(ptr, x); }
# define _CFREE(x)  kmem_free(x)
 caddr_t kern_alloc();
#endif /* __sgi */

#ifdef DEBUG
extern int dsdebug;
# define DBG(s) {if(dsdebug) {s;} }
#else
# define DBG(s)
#endif

/* declared here because called from both dsreq.c and dsglue.c */
void dma_unlock(register struct dsiovec *, long, int);

/*
 *	Interface structures between generic portion of DEVSCSI driver
 *	and system-dependent glue layer.
 *	Each request is allocated a separate task descriptor.
 */

typedef struct task_desc  {
  uchar_t    td_bus, td_id;     /* bus number, device id 0-7	*/
  uchar_t    td_lun, td_role;   /* lun 0-7, and role 0-1	*/
  vertex_hdl_t td_lun_vhdl;	/* vertex handle of the 	
				 * particular lun that the scsi 
				 * device corresponds to 
				 */
  vertex_hdl_t td_dev_vhdl;	/* scsi dev vertex handle */
  uchar_t    td_cmdbuf[16];	/* cmd to be issued		*/
  uchar_t    *td_sensebuf;	/* place to receive sense data  */
  uchar_t    td_msgbuf[6];	/* place to receive msg info	*/
  uchar_t    td_senselen;	/* amount of sense data         */
  uchar_t    td_driver;		/* SCSI driver number           */
  dsiovec_t  td_iovbuf[V_MAX];	/* array of data dma vectors	*/
  dsiovec_t  td_iovdummy[1];	/* buffered data dma vector 	*/
	/* td_devtype is after a pointer so will be aligned for dma */
  uchar_t    td_devtype;	/* SCSI device type, from inquiry */
  uchar_t    td_cmdlen;		/* len of cmd in cmdbuf		*/
  uchar_t    td_msglen;		/* len of returned msg info	*/
  uchar_t    td_status;		/* device status byte return    */
  u_short    td_iovlen;		/* len of io vectors present    */
  u_long     td_datalen;	/* requested/returned length    */
  u_long     td_flags;		/* filtered dsrq request flags  */
  u_long     td_iflags;		/* flags internal to driver     */
  u_short    td_state;		/* task state			*/
  u_short    td_ret;		/* return code from glue layer  */
  u_short    td_retry;		/* retry count			*/
  u_long     td_time;		/* timeout in ms		*/
  caddr_t    td_dsreq;		/* ptr to devscsi data struct   */
  caddr_t    td_hsreq;		/* ptr to host scsi data struct */
  struct buf td_buf;		/* for use with r/w interface  	*/
  sema_t     td_done;		/* command completion semaphore */
  uint       td_refcount;	/* reserve/release count */
} task_desc_t;

/* size of buffer allocated for td_sensebuf (same size as secondary
 * cache line size, since we need to allocate that much anyway */
#define TDS_SENSELEN 128

/*
 *  definitions for task td_state
 */
#define TDS_START	0	/* not initialized */
#define TDS_INIT	1	/* space allocated */
#define TDS_TASK	2	/* task present	*/
#define TDS_RUN 	3	/* task running */
#define TDS_INTERIM 	4	/* host completed, need entero called */
#define TDS_WAITASYNC	5	/* async request in progress */
#define TDS_DONE	6	/* request completed */


/*  definitions for task td_iflags */
#define TASK_INTERNAL  0x1	/* internal call to driver */
#define TASK_HAVETYPE	0x2	/* used for read/write interface. If set,
	an inquiry has been done, so we know what form of read/write
	command to use. */
#define TASK_EXCLUSIVE	0x8 /* passed to glue if O_EXCL used on open; 
	results in SCSIALLOC_EXCLUSIVE being passed to scsi_alloc calls
	(see sys/scsi.h). */
#define TASK_ABORT	0x10 /* send an abort message; from DS_ABORT ioctl;
	only useful if no commands active on the target; as it otherwise
	delays until no commands are active.  Commands like rewind immediate
	and other commands with the immediate bit can be aborted in this
	way */

/*
 *      sgi gluer declarations
 *      called from both dsglue and dsreq, declared in dsglue
 */
int glue_reserve(struct task_desc *, dev_t);
int glue_release(struct task_desc *);
int glue_start(struct task_desc *, int);
void glue_return(struct task_desc *, int);
int glue_getopt(struct task_desc *, int);
extern uint glue_support;
extern uint glue_default;

#endif /* __SYS_DSGLUE_H__ */
