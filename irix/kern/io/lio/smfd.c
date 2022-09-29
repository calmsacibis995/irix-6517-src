 /*************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "smfd.c: $Revision: 1.111 $"



/*
 	smfd.c -- Device driver for SCSI floppy (was Scientific
	Micro Systems Model 7000 SCSI floppy, hence the smfd).
	Also gets used for Syquest drives...

 	This driver talks to the scsi interface driver.
	Derived from dksc.c.  Many structures used by user programs
	have been left the same as the dksc driver, so that programs
	using them won't have to be changed.

	This driver no longer shares *ANYTHING* (but some of the structure)
	with dksc.c!  dksc.h used because that's where some ioctl structs
	are defined.  Olson, 10/95

	For first release, the following assumptions are made:
		1)  If sector size other than NPBSCTR set, may have problems
			with rest of kernel, (i.e., won't work as a file system,
			and sizes of < NBPSCTR will result in physio rejecting
			requests that are not a multiple of NBPSCTR).
		2)  support 9 and 15 sec/trk, double sided, 48 and 96 tpi
		    (PC and PC/AT)  (256 byte sectors should also work, but
			haven't been fully tested.).
		3)  assumptions 1 and 2 may be violated by the SMFDSETMODE
		    ioctl, but no minor # support for others is supported.
		    SMFDSETMODE may be done only if drive has only one
		    open since the previous 'last close'.  Furthermore, no
			further opens will be allowed after this ioctl until
			after last close.  No guarantees are made of usability
			after this ioctl, since parameters may not make sense.
		4)  no support for motor speed control
		5)  no support for detecting media change while drive
		    is in use.
		6)  May only be open as one 'type' at any given time
		7)  Opens fail if no media in drive as reported by
		    testunitready

      4/30/93
      randyh:   Issues involved to support Insite floptical.
      		1) Insite drives disable writes following powerup.
		   Drives can be write enabled by performing a mode sense of
		   SCSI mode page 0x2e.  This driver has been modified to
		   perform the mode sense whenever an Insite device is opened.
		
		2) Formatting of floptical media takes a long time. For
		   floptical media, the format timeout is increased.

		3) The floptical is basically a SCSI-II drive.  Previous
		   drives were not.  This means different code for ejecting
		   media and for formatting single tracks.

		4) The floptical fails a read-capacity request if unformatted
		   media is installed.  This would prevent the drive from
		   being opened.  Instead we return the capacity based on the 
		   current type.

		5) Recalibrate is so slow for the floptical it
		   takes 16 seconds to complete the first open and 16 seconds
		   for the eject command to eject a drive.  Therefore for
		   the floptical the smfd_recal call is skipped in
		   smfd_settype and a crude verify_state routine is called.

		6) The Insite drive supports yet another subset of select
		   parameters.  The setparams routine has been modified to
		   cope with the Insite drive.  The setparams should probably
		   be cleaned up to more clearly separate out the drive
		   specific handling.

     11/20/93
     randyh:   Added code to make the driver aware of SyQuest drives.  This
               is intended mostly to allow SyQuest drives to operate under
	       mount_hfs and mount_dos.  More general testing hasn't really
	       been done.

*/
# include <bstring.h>
# include <string.h>
# include <sys/types.h>
# include <sys/immu.h>
# include <sys/kabi.h>
# include <sys/kmem.h>
# include <sys/param.h>
# include <sys/systm.h>
# include <sys/sysmacros.h>
# include <sys/sbd.h>
# include <sys/pcb.h>
# include <sys/signal.h>
# include <sys/buf.h>
# include <sys/iobuf.h>
# include <sys/elog.h>
# include <sys/scsi.h>
# include <sys/major.h>
# include <sys/ioctl.h>
# include <sys/dkio.h>
# include <sys/debug.h>
# include <sys/uio.h>

#include <sys/dvh.h>
#include <sys/dksc.h>	/* for ioctl structs only */
#include <sys/smfd.h>
#include <ksys/vfile.h>	/* for FWRITE */
#include <sys/open.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include "sys/mload.h"

#include <sys/iograph.h>
#include <sys/invent.h>

#ifdef DEBUG_FLOPPY
static smdbg = 0;
#endif

int smfddevflag = D_MP;
char *smfdmversion = M_VERSION;


/*
 * The basic timeout for the floppy driver.  Note that it is long to handle
 * many units active at same time, with possible disconnects during commands
 */
#define SMFD_TIMEOUT	(30 * HZ)

/*	Maximum number of retrys on a failed request, high because floppies
	are so unreliable, and data often can be read in multiple
	tries. */
#define SMFD_MAX_RETRY	7

/* Size of a physical disk block */
#define SMFD_BLK_SZ	NBPSCTR

#define MAPLUN 0        /* for places we have to pass a lun */

#define	SMFD_SETUP 	0x2	/* flag for setup completed */
#define	SMFD_TEAC	0x100	/* TEAC drive with TEAC controller, not NCR */
#define	SMFD_NORETRY	0x200	/* no retry on reset during read/write */
#define	SMFD_SWAPPED	0x400	/* media changed */
#define SMFD_INSITE	0x800   /* Insite drive */
#define SMFD_READYING  0x1000   /* Drive is the process of becoming ready */
#define SMFD_SYQUEST45 0x2000   /* Drive is a Syquest 45MB drive */
#define SMFD_SYQUEST88 0x4000   /* Drive is a Syquest 88MB drive */
#define SMFD_NOMEDIA   0x8000   /* No media */

#define INSITE_UNLOCKSZ 42	/* Size of Insite unlock mode-sense return */

#define IS_TEAC   (smfd->s_flags & SMFD_TEAC)
#define IS_INSITE (smfd->s_flags & SMFD_INSITE)
#define IS_SYQUEST (smfd->s_flags & (SMFD_SYQUEST45|SMFD_SYQUEST88))

/*	Note that on select, you get an error if the SP (save param) bit is set.  */
const u_char smfd_read[SC_CLASS1_SZ] = {
	0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const u_char smfd_write[SC_CLASS1_SZ] = {
	0x2a, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const u_char smfd_readcapacity[SC_CLASS1_SZ]= {
	0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const u_char smfd_inquiry[SC_CLASS0_SZ] = {
	0x12, 0, 0, 0, 4, 0};
const u_char smfd_req_sense[SC_CLASS0_SZ] = {
	0x3, 0, 0, 0, 0x16, 0};
const u_char smfd_send_diag[SC_CLASS0_SZ] = {
	0x1d, 0, 0, 0, 0, 0};
const u_char smfd_mode_sense[SC_CLASS0_SZ] = {
	0x1a, 0, 0, 0, 0, 0};
      u_char smfd_mode_select[SC_CLASS0_SZ] = {
	0x15, 0, 0, 0, 0,	0};   /* 2nd byte can be set by DIOCSETFLAGS */
const u_char smfd_format[SC_CLASS0_SZ] = {
	0x4, 0, 0, 0, 0, 0};
const u_char smfd_add_defects[SC_CLASS0_SZ] = {
	0x7, 0, 0, 0, 0, 0};
const u_char smfd_rezero[SC_CLASS0_SZ] = {
	0x1, 0, 0, 0, 0, 0};
const u_char smfd_rdbuffer[SC_CLASS1_SZ] = {
	0x3c, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const u_char smfd_eject[SC_CLASS0_SZ] = {	/* SCSI-II */
	0x1B, 0, 0, 0, 2, 0};
const u_char smfd_eject_old[SC_CLASS0_SZ] = {	/* pre-SCSI II */
        0x10, 0, 0, 0, 0, 0};
const u_char smfd_tst_unit_rdy[SC_CLASS0_SZ] = {
	0x0, 0, 0, 0, 0, 0};
const u_char smfd_floptical_unlk[SC_CLASS0_SZ] = { /* Insite Floptical */
        0x1a, 0, 0x2e, 0, INSITE_UNLOCKSZ, 0};
const u_char smfd_control_removal[SC_CLASS0_SZ]={  /* Prevent/allow media removal*/
        0x1e, 0, 0, 0, 0, 0};

/*
 * Mappings from types to sectors per track (spta), sectors per cylinder (spc),
 *   cylinders (cyls), and media types (types).
 *
 *      0        1       2      3      4      5      6
 *    ---------------------------------------------------
 *    5.25"     5.25"   5.25"         3.5"   3.5"   3.5"
 *    360K      720K    1.2M          720K   1.44M  20M
 *    48tpi     96tpi   96tpi  
 *     9spt      9spt   15spt         9spt   18spt  27spt
 *                      PC-AT                       Flopt
 *    ---------------------------------------------------
 */
const u_char smfd_spta[SMFD_MAX_TYPES]  = {
	9,       9,     15,     0,     9,    18,    27};
const u_short smfd_spc[SMFD_MAX_TYPES]  = {
	18,     18,     30,     0,    18,    36,    54};
const u_short smfd_cyls[SMFD_MAX_TYPES] = {
	40,     80,     80,     0,    80,    80,   753};
const u_char smfd_types[SMFD_MAX_TYPES] =   {
	0x12, 0x16,   0x1A,     0,  0x1E,  0x1E,   0x20};

#define	SMFD_TEAC_TYPE_35	0x28
#define	SMFD_TEAC_TYPE_35LO	0x23
#define SMFD_INSITE_TYPE_35     0x1F

typedef struct smfd {
	scsi_request_t s_sr; /* struct scsi_request structure */
	lock_t	 s_lock;	/* Lock to protect structure fields */
	sv_t	 s_wait;	/* Synchronous use waiters */
	mutex_t	 s_sema;	/* controls access to s_done sema */
	sema_t	 s_done;	/* for synchronous I/O */
	int s_flags;	/* open w/o media, setup done, etc. */
	int s_dtype;	/* type of current open (OTYP*) */
	uint s_opens;	/* total opens of any type */
	uint s_opentypes[OTYPCNT]; /* count of opens for each
		possible open type. This rigamorale is because of OTYP_LYR
		opens that violate the close only called on very last close.
		LYR does gaurantee paired open/close. */
	struct iobuf s_queue;	/* wait queue */
	u_int s_capacity;	/* total sectors on drive */
	dev_t s_dev;	/* for error messages */
	ushort	 s_cyls;	/* # of cylinders on device */
	short s_secsize;	/* sector size. */
	short s_spt;	/* current # of sectors/trk.  needed for FMTTRK */
	short s_spc;	/* current # of sectors/cyl.  needed for FMTTRK */
	u_char s_lastsense;	/* first byte of last request sense */
	u_char s_lastcmd;	/* first byte of last cmd (other than reqsense,
		 etc.) */
	u_char s_curtype;	/* 0, if not open, otherwise 'type'
		drive is currently opened as; may change due to
		SMFDSETMODE ioctl */
	u_char s_retry;
	u_char s_selflags;	/* flags to use in modeselect */
} smfd_t;


/*
 * Forward function declartions
 */
static void smfdstart(struct buf *);
static void smfd_drive_acquire(smfd_t *);
void smfd_intr(scsi_request_t *);
void smfdstrategy(struct buf *);
int smfd_getinq(smfd_t *);
int smfd_docmd(smfd_t *, const u_char *, int, u_int, u_char *, size_t, int);
int smchkandsetup(smfd_t *, int);
int smfd_setparms(smfd_t *, struct smfd_setup *, u_char *);
int smfd_recal(smfd_t *);
int smfd_fmt_trk(smfd_t *, int);
int smfdwrite(dev_t, uio_t *);
int smfd_chkcond(scsi_request_t *, int *, int);
void smfd_intrend(smfd_t *, register struct buf *);
int smfd_interpret_sense(scsi_request_t *);
void smfd_retry(scsi_request_t *);
void
/*VARARGS2*/
smfd_rpt_err(dev_t, char *,...);
void smfd_drive_release(smfd_t *);

static void smfd_unlk_floptical(smfd_t *);
int smfd_verify_state(smfd_t *);

void smfd_removal(smfd_t*,int prevent_allow);
#define PREVENT 1
#define ALLOW   0


extern void disksort(struct iobuf *, struct buf *);


typedef struct smfd_local_info {
	smfd_t		*sli_smfd_sc;
} smfd_local_info_t;

/* some useful macros */
#define SLI_SMFD_SC(p)		(p->sli_smfd_sc)
#define SMFD_UNITINFO(dev)	(scsi_unit_info_get(hwgraph_connectpt_get(hwgraph_connectpt_get(dev))))
#define SMFD_SC(p)		(p ? ((smfd_local_info_t *)p)->sli_smfd_sc : NULL)

#define SUI_INFO(p)		(SUI_CTINFO(p))	
/*
 * allocate and initialize memory for the smfd driver local information
 */

smfd_local_info_t *
smfd_local_info_init(void)
{
	smfd_local_info_t	*local_info;

	local_info	= (smfd_local_info_t *)kern_calloc(1,sizeof(smfd_local_info_t));
	ASSERT(local_info);

	return local_info;
}

/*
 * put the local info pointer in the unit info structure
 */

void
smfd_local_info_put(vertex_hdl_t 	unit_vhdl,
		    smfd_t		*smfd_sc)

{
	scsi_unit_info_t	*unit_info;
	smfd_local_info_t	*local_info;
	
	unit_info = scsi_unit_info_get(unit_vhdl);
	/*
	 * add the local info only if is new
	 */
	if (!SUI_INFO(unit_info)) {
		local_info 		= smfd_local_info_init();
		SLI_SMFD_SC(local_info)	= smfd_sc;
		SUI_INFO(unit_info) 	= local_info;
	}
}	

typedef struct smfd_info {
	int	smfd_type;
} smfd_info_t;


/*
 *	suffixes that used to be added by the MAKEDEV script
 * 	for the floppy devices.
 */

extern char *smfd_suffix[];

#define SMFD_TYPE(dev)		(smfd_info_get(dev)->smfd_type)

/*
 * get the smfd specifics type info hanging off the 
 * specific mode floppy vertex
 */
smfd_info_t *
smfd_info_get(vertex_hdl_t	floppy_vhdl)
{
	smfd_info_t	*info ;

	info = (smfd_info_t *)hwgraph_fastinfo_get(floppy_vhdl);
	ASSERT(info);
	return info;
}
/*
 *  put the smfd specific type info when the approriate
 *  mode vertex is created
 */
void
smfd_info_put(vertex_hdl_t 	floppy_vhdl,
	      int		type)
{
	smfd_info_t	*info;

	
	if (hwgraph_fastinfo_get(floppy_vhdl))
		return;
	info = (smfd_info_t *)kern_calloc(1,sizeof(smfd_info_t));
	ASSERT(info);
	info->smfd_type = type;
	hwgraph_fastinfo_set(floppy_vhdl,(arbitrary_info_t)info);
	
}

/*
 * create the standard vertices for various modes
 * of opening the floppy disk
 */
void
smfd_namespace_add(vertex_hdl_t unit_vhdl)
{
	int 		type;
	vertex_hdl_t	floppy_vhdl_char,floppy_vhdl;
	/* REFERENCED */
	graph_error_t	rv;



	/* create a device vertex for each possible
	 * floppy device mode 
	 */
	for(type = 0 ; type < SMFD_MAX_TYPES ; type++) {

		rv = hwgraph_path_add(unit_vhdl, smfd_suffix[type], &floppy_vhdl);
		ASSERT(rv == GRAPH_SUCCESS);
		(void)hwgraph_char_device_add(floppy_vhdl, EDGE_LBL_CHAR, "smfd", &floppy_vhdl_char);
		hwgraph_chmod(floppy_vhdl_char, HWGRAPH_PERM_SMFD);
		
		ASSERT(floppy_vhdl != GRAPH_VERTEX_NONE);		
		ASSERT(floppy_vhdl_char != GRAPH_VERTEX_NONE);
		smfd_info_put(floppy_vhdl,type);
		smfd_info_put(floppy_vhdl_char,type);
	}


}
smfd_t *
smfd_alloc(vertex_hdl_t	 unit_vhdl)

{
	smfd_t *smfd;
	int i;
	scsi_unit_info_t	*unit_info = scsi_unit_info_get(unit_vhdl);

	smfd = (smfd_t *)kern_calloc(sizeof(*smfd), 1);

	smfd->s_sr.sr_sense = kmem_alloc(SCSI_SENSE_LEN, KM_CACHEALIGN);
	smfd->s_sr.sr_senselen = SCSI_SENSE_LEN;
	smfd->s_sr.sr_command = kmem_alloc(SC_CLASS1_SZ, 0);
	smfd->s_sr.sr_ctlr   = SUI_ADAP(unit_info);
	smfd->s_sr.sr_target = SUI_TARG(unit_info);
	smfd->s_sr.sr_lun    = SUI_LUN(unit_info);
	smfd->s_sr.sr_dev_vhdl = unit_vhdl;
	smfd->s_sr.sr_lun_vhdl = SUI_LUN_VHDL(unit_info);
	i = unit_vhdl;
	init_spinlock(&smfd->s_lock, "smflock",i);
	init_sv(&smfd->s_wait, SV_DEFAULT, "smfwait",i);
	init_mutex(&smfd->s_sema, MUTEX_DEFAULT, "smfsema", i);
	init_sema(&smfd->s_done, 0, "smfdone", i);
	
	smfd_local_info_put(unit_vhdl,smfd);
	return smfd;
}


void
smfd_unalloc(smfd_t *smfd)
{
	kmem_free(smfd->s_sr.sr_sense, smfd->s_sr.sr_senselen);
	kmem_free(smfd->s_sr.sr_command, SC_CLASS1_SZ);

	sv_destroy(&smfd->s_wait);
	spinlock_destroy(&smfd->s_lock);
	mutex_destroy(&smfd->s_sema);
	freesema(&smfd->s_done);
	SUI_INFO(SMFD_UNITINFO(smfd->s_dev)) = 0;
	kern_free(smfd);

}

void
smfdinit()
{
	static int smfdinit_done;

	if (smfdinit_done)
		return;
	smfdinit_done++;

}

int
smfdattach(vertex_hdl_t	floppy_vhdl)
{

#ifdef DEBUG_FLOPPY
	printf("		***Calling smfd attach %V\n",floppy_vhdl);
#endif

	smfd_namespace_add(floppy_vhdl);

	return 0;
}

void
smfddetach(vertex_hdl_t	floppy_vhdl)
{
	int			type;
	vertex_hdl_t		mode_vhdl;
	scsi_unit_info_t	*unit_info;	
	extern void		smfd_alias_remove(vertex_hdl_t);

	smfd_alias_remove(floppy_vhdl);
	for(type = 0 ; type < SMFD_MAX_TYPES ; type++) {

		hwgraph_edge_remove(floppy_vhdl,
				    smfd_suffix[type],
				    &mode_vhdl);
		hwgraph_vertex_destroy(mode_vhdl);
	}
	unit_info = scsi_unit_info_get(floppy_vhdl);
	mutex_destroy(&SUI_OPENSEMA(unit_info));
	kern_free(unit_info);
	scsi_unit_info_put(floppy_vhdl,0);


       
}

int
smfdreg()
{
#ifdef DEBUG_FLOPPY
	printf("registering smfd driver\n");
#endif
	scsi_driver_register(0,"smfd");
	return 0;
}

/* unregister the smfd driver */
int
smfdunreg()
{
	scsi_driver_unregister("smfd");
	return 0;
}

/* ARGSUSED */
int
smfdopen(dev_t *devp, int flag, int otyp, cred_t *crp)
{
	int	type;
	mutex_t	*opensem;
	smfd_t *smfd;
	int error = 0, alloced = 0;
	scsi_unit_info_t	*unit_info;

	if (!dev_is_vertex(*devp))
		return ENXIO;

	unit_info  = SMFD_UNITINFO(*devp);	
	type = SMFD_TYPE(*devp);

	/*	We don't allow opens for block and char dev at same time.
		BLK, MNT, SWP, are considered to be the same for our
		purposes, especially for close and for type check below */
	if(otyp == OTYP_MNT || otyp == OTYP_SWP)
		otyp = OTYP_BLK;

	/* Single thread opens to a device */
	opensem = &(SUI_OPENSEMA(unit_info));
	mutex_lock(opensem, PRIBIO);

	smfd = SMFD_SC(SUI_INFO(unit_info));
	/* Check if device already opened */
	if(smfd == NULL || smfd->s_opens == 0) { /* Not already open */
		if(smfd == NULL) {
			int err;
			smfd = smfd_alloc(SUI_UNIT_VHDL(unit_info));
			alloced = 1;

			smfd->s_dev = *devp;

			/* never do sync negotiations for floppies.  Some floppy
			* drives, particular the NCR ones, get really hosed when you
			* do.  None of our current floppies support it anyway.. */

			if ((err = SUI_ALLOC(unit_info)(SUI_LUN_VHDL(unit_info),
							SCSIALLOC_NOSYNC|1, NULL)) 
			    != SCSIALLOCOK) {
#if DEBUG_FLOPPY
				printf("scsi_alloc fails on smfd %d,%d,%d\n",SUI_ADAP(unit_info),
				       SUI_TARG(unit_info),MAPLUN);
#endif /* DEBUG_FLOPPY */
				smfd_unalloc(smfd);
				error = err ? err : ENODEV;
				goto openerr;
			}

		}

		smfd->s_curtype = type;
		smfd->s_flags = 0;

		/* allow an open to succeed if it is a EAGAIN 
		   error (no media in drive) */
		if ((error = smchkandsetup(smfd, flag)) == EAGAIN)
			error = 0;

	        if (!error && IS_SYQUEST)
		        /* Prevent the media from being manually removed.
			   We only attempt this with SyQuest drives as
		           this function hasn't been tested with
			   other drive types. */
		        smfd_removal(smfd,PREVENT);
	}
	else if(smfd->s_curtype != type ||
		(otyp != smfd->s_dtype && otyp != OTYP_LYR)) {
		error = EBUSY;	/* if open for different type, this is
			* a different adap/targ than what is already open,
			* or this is block and current is char (or vice-versa)
			* fail it with EBUSY */
	}
	if(!error) {
		if(!smfd->s_opens)
			smfd->s_dtype = otyp;
		smfd->s_opens++;
		smfd->s_opentypes[otyp]++;
	}
	else if(smfd->s_opens == 0) {	/* failure, and only open */
		if(alloced)
			smfd_unalloc(smfd);
		else
			kern_free(smfd);
	}

openerr:
	mutex_unlock(opensem);

	/* prevent any unexpected errors */
	return error;
}

/* unlike dkscclose, smfdclose is not a nop.  That is because it is
 * an exclusive open device still...
*/
/* ARGSUSED */
smfdclose(dev_t dev, int flag, int otyp, cred_t *crp)
{
	smfd_t *smfd;
	mutex_t	*opensem;
	int opens;
	scsi_unit_info_t *unit_info = SMFD_UNITINFO(dev);	
	
	/*	BLK, MNT, SWP, are considered to be the same for our
		purposes */
	if(otyp == OTYP_MNT || otyp == OTYP_SWP)
		otyp = OTYP_BLK;

	/* ensure no opens or other closes while closing */
	opensem = &(SUI_OPENSEMA(unit_info));
	mutex_lock(opensem, PRIBIO);

	smfd = SMFD_SC(SUI_INFO(unit_info));
	opens = smfd->s_opentypes[otyp];
	if(smfd->s_opentypes[otyp] == opens)  {
		/* LYR open/closes are always paired, so go by counter */
		if(otyp == OTYP_LYR && --smfd->s_opentypes[otyp] > 0)
			goto done;
		smfd->s_opentypes[otyp] = 0;
		for(opens=0; opens<OTYPCNT && !smfd->s_opentypes[opens]; opens++)
			;

		if(opens==OTYPCNT) {
			if (IS_SYQUEST)
			      /* Allow media removal */
			      smfd_removal(smfd,ALLOW);
			smfd_unalloc(smfd);
			SUI_FREE(unit_info)(SUI_LUN_VHDL(unit_info), NULL);
		}
	}
	/* else an open completed while we waited for the semaphore */
done:
	mutex_unlock(opensem);

	/* prevent any unexpected errors */
	return 0;
}

#if _MIPS_SIM == _ABI64
static void
irix5_dkioctl_to_dkioctl(struct irix5_dk_ioctl_data *i5dp, 
	struct dk_ioctl_data *dp)
{
	dp->i_addr = (caddr_t)(__psint_t)i5dp->i_addr;
	dp->i_len = i5dp->i_len;
	dp->i_page = i5dp->i_page;
}
#endif /* _ABI64 */

/*
 * Note that because the close routine cleans everything up, there
 * is no need for smfdunload() to do anything. It must exist in
 * order for the driver to be unloaded though.
 */
int
smfdunload(void)
{
	return 0;
}

/*	smfdioctl -- io controls.  Note that because of the
	ioctl_data stuff, there must be only one return from
	this routine (or all the goto's for errors need to be removed,
	and the 'sync' routines must all break instead of return).
*/

int
smfdioctl(dev_t dev, int cmd, void *arg, int flag)
{
	int	type;
	u_char *ioctl_data;
	smfd_t *smfd;
	int	media_status;
	int	error = 0;


	smfd = SMFD_SC(SUI_INFO(SMFD_UNITINFO(dev)));

	/* DIOCMKALIAS, DIOCDRIVETYE, SMFDMEDIA, and SMFDNORETRY always work */
	if (cmd != DIOCDRIVETYPE && cmd != SMFDMEDIA && cmd != DIOCMKALIAS &&
	    cmd != SMFDNORETRY && !(smfd->s_flags & SMFD_SETUP)) {
		type = smchkandsetup(smfd, (cmd==DIOCFMTTRK || cmd==DIOCFORMAT) ?
			FWRITE : FREAD);
		if(type)
			return type;
	}

	type = SMFD_TYPE(dev);
	switch (cmd) {
	case DIOCSENSE:
	case DIOCSELECT:
	case DIOCFMTTRK:
	case DIOCGETVH:
	case SMFDSETMODE:
		ioctl_data = kmem_alloc(MAX_IOCTL_DATA, KM_CACHEALIGN);
		break;
	default:
		ioctl_data = NULL;
		break;
	}

	switch (cmd) {
	case DIOCGETVH:	/* we fake this for programs like mkfs.  Note that
		we do NOT actually put a volume header on the disk, since we
		don't want to have to worry about partitions.  GETVH may 
		eventually be implemented to do the stuff SETMODE does,
		but not right now.  mkfs decides the partition # based
		on the minor #, but fd minor #'s don't fit that model.
		Therefor fill in the first SMFD_MAX_TYPES partitions to
		be identical.  'partition's are entire disk.
		mkfs doesn't check partition type, and will 'normally'
		be just data, so we use RAW.
		*/
		{
		struct volume_header *vh = (struct volume_header*)ioctl_data;
		register int i;

		bzero(vh, sizeof(*vh));	/* zero all don't care and 0 fields */
		vh->vh_magic = VHMAGIC;

		for(i=0; i< SMFD_MAX_TYPES; i++) {
			vh->vh_pt[i].pt_firstlbn = 0;
			vh->vh_pt[i].pt_nblks = smfd->s_capacity;
			vh->vh_pt[i].pt_type = PTYPE_RAW;
		}

		vh->vh_dp.dp_secbytes = smfd->s_secsize;
		vh->vh_dp.dp_drivecap = smfd->s_capacity;

		vh->vh_csum = -vh_checksum(vh);
		if(copyout(vh, arg, sizeof(*vh)))
		    error = EFAULT;
		break;
		}

	case SMFDSETMODE: /* to set spc, etc.  Drive must be open
		only once. This check can't work quite right, since if
		there have been multiple opens, and all but one are
		closed, we still think there are multiple opens...
		Better safe than sorry.  NOTE: if an SMFDSETMODE is
		done to non-standard before this, or done after this,
		or while using a mounted filesystem, then things would
		get pretty ugly.  This is take care of by the
		prohibition on having block and char devices open at
		the same time.  If that restriction is removed, this
		issue needs to be considered again! */
		{
			struct smfd_setup *setup = (struct smfd_setup *)ioctl_data;
			if(smfd->s_opens != 1)
				error = EBUSY;
			else if(copyin(arg, setup, sizeof(*setup)))
				error = EFAULT;
			else if(smfd_setparms(smfd, setup, (u_char *)&setup[1]))
				error = EIO;
			break;
		}

	case SMFDREQSENSE:	/* request extra sense: since it is cleared
		by subsequent SCSI commands, we give them the last one
		that we read, which MAY not correlate to last error
		we returned to user. as with other crud added for SoftPC,
		they wound up not using this either! */
		if(copyout(&smfd->s_lastsense, arg, 1))
			error = EFAULT;
		break;

	case SMFDRECAL:	/* do a recal on the drive */
		smfd_recal(smfd);
		break;

	case SMFDEJECT: /* eject the floppy disk */
		if(!(smfd->s_flags & (SMFD_TEAC|SMFD_INSITE))) {
			error = EINVAL;	/* not supported */
			break;
		}
		/* The TEAC drive ejects with the older SCSI-I command.
		   The Insite drive with the newer SCSI-II command. */
		(void)smfd_docmd(smfd,
                             (IS_TEAC) ? smfd_eject_old : smfd_eject,
			      SC_CLASS0_SZ, SMFD_TIMEOUT, (u_char *)0, 0, 0);
		error = 0;
		break;

	case SMFDMEDIA: /* media status */
		smfd->s_flags &= ~SMFD_SWAPPED;
		error = smfd_docmd(smfd, smfd_tst_unit_rdy, SC_CLASS0_SZ,
			SMFD_TIMEOUT, (u_char *)0, 0, 0);

		if (error) {
			if (error != ENODEV)	/* real error */
				break;
			media_status = 0;		/* no media in drive */
			error = 0;
		} else {				/* media in drive */
			u_char scmd[SC_CLASS0_SZ];
			u_char *sdata;

			media_status = SMFDMEDIA_READY;

			if (smfd->s_flags & SMFD_SWAPPED)
				media_status |= SMFDMEDIA_CHANGED;

			/* check whether the media in write protected or not */
			sdata = kmem_alloc(4, KM_CACHEALIGN);
			bcopy(smfd_mode_sense, scmd, sizeof(scmd));
			/*
			 * The way we tell if the media is writable is
			 * different for for floppies than for Syquest drives.
			 * For floppies, we read the floppy page (5). For
			 * Syquests we read the fixed geometry page. (Note
			 * that we don't really look at either the floppy
			 * page or the fixed page, we only look at the header.)
			 */
			scmd[2] = IS_SYQUEST ? 4 : 5; 
			scmd[4] = 4;	/* only need the 4 byte header */
			if(error = smfd_docmd(smfd, scmd, sizeof(scmd), SMFD_TIMEOUT,
				      sdata, 4, B_READ)) {
				error = EIO;
			} else if (sdata[0] > 2 && sdata[2] & 0x80) {
				media_status |= SMFDMEDIA_WRITE_PROT;
			}
			kern_free(sdata);
		}

		if (copyout(&media_status, arg, 4))
			error = EFAULT;
		break;

	case SMFDNORETRY:	/* no retry on reset during read/write */
		if ((__psunsigned_t)arg != 0)
			smfd->s_flags |= SMFD_NORETRY;
		else
			smfd->s_flags &= ~SMFD_NORETRY;
		break;

	case DIOCDRIVETYPE:
	    {
	    u_char inquiry_cmd[SC_CLASS0_SZ];

	    /* return drive name string */
	    flag = SCSI_DEVICE_NAME_SIZE;
	    /* +8 for the inquiry info header, plus room for cmd */
	    flag += 8;
	    ioctl_data = kmem_alloc(flag, KM_CACHEALIGN);
	    bzero(ioctl_data, flag);
	    bzero(inquiry_cmd, SC_CLASS0_SZ);

	    inquiry_cmd[0] = smfd_inquiry[0];
	    inquiry_cmd[4] = flag;
	    if(error = smfd_docmd(smfd, inquiry_cmd, SC_CLASS0_SZ, SMFD_TIMEOUT,
		    ioctl_data, flag, B_READ))
		    goto ioerr;

	    if(copyout(ioctl_data + 8, arg, flag-8))
		    error = EFAULT;
	    break;
	    }

	case DIOCTEST:
		{
		if(error = smfd_docmd(smfd, smfd_send_diag, SC_CLASS0_SZ, SMFD_TIMEOUT,
		    (u_char *)0, 0, 0))
ioerr:
			if(!error)
				error = EIO;
		}
		break;

	case DIOCFORMAT:
		if(smfd->s_opens != 1)	/* no formatting if multiple users.
				This check can't work quite right,
				since if there have been multiple opens, and
				all but one are closed, we still think there
				are multiple opens...  Better safe than sorry. */
			error = EBUSY;
			/*
			 * Formats according to current type (from open, DIOCSELECT or
			 * DIOCSETMODE).
			 * 
			 * Note that formatting a floptical takes ~ 22 minutes.
			 * Its difficult here to be sure if the media is a floptical or a floppy
			 * so we use a long timeout.  The worst that can happen is that if there is
			 * some sort of timeout error for a floppy, it takes as long to discover it
			 * as it would for a floptical.
			 */
		else if(error = smfd_docmd(smfd, smfd_format, SC_CLASS0_SZ,
					   (1800*HZ),
					   (u_char *)0, 0, 0))
			goto ioerr;
		break;

	case DIOCFMTTRK: /* format track */
		/* no longer support the hack to supply the data; the one
		 * customer that insisted that they had to have it never 
		 * actually used it.  I doubt anybody is using any of this,
		 * but we'll just do a track format for either case, just in case.
		*/
	{
		struct fmt_map_info {
			int	fmi_action;		/* action desired, see FMI_ above */
			unsigned short	fmi_cyl;
			unsigned char   fmi_trk;
		} *finfo;
		finfo = (struct fmt_map_info *)ioctl_data;
		if(copyin(arg, finfo, sizeof(*finfo)))
			error = EFAULT;
		else
			error = smfd_fmt_trk(smfd, finfo->fmi_trk);
		break;
	}

	case DIOCSENSE:
		{
			struct dk_ioctl_data sense_args;
			u_char sense_cmd[SC_CLASS0_SZ];
			u_int *sense_data = (u_int *)ioctl_data;
#if _MIPS_SIM == _ABI64
			if(!ABI_IS_IRIX5_64(get_current_abi())) {
			    struct irix5_dk_ioctl_data i5_sa;

			    if(copyin(arg, &i5_sa, sizeof(i5_sa))) {
				error = EFAULT;
				break;
			    }
			    else {
				irix5_dkioctl_to_dkioctl(&i5_sa, &sense_args);
			    }
			}
			else
#endif /* _ABI64 */
			if(copyin(arg, &sense_args, sizeof(struct dk_ioctl_data))) {
				error = EFAULT;
				break;
			}
			if(sense_args.i_len > MAX_IOCTL_DATA)
				goto invalerr;
			bcopy(smfd_mode_sense, sense_cmd, sizeof(sense_cmd));
			sense_cmd[2] = sense_args.i_page;
			sense_cmd[4] = sense_args.i_len;
			if(error = smfd_docmd(smfd, sense_cmd, SC_CLASS0_SZ, SMFD_TIMEOUT,
				(u_char *)sense_data, sense_args.i_len, B_READ))
				goto ioerr;
			if(copyout(sense_data, sense_args.i_addr, sense_args.i_len))
				error = EFAULT;
			break;
		}

	case DIOCSELFLAGS:
		if((__psunsigned_t)arg & 0xe0)	/* can't use to set LUN! */
			error = EINVAL;
		/* note that default is 0 in this driver, not 11 as in dksc */
		smfd->s_selflags = (u_char)((__psunsigned_t)arg & 0xff);
		break;
		
	case DIOCSELECT:
		{
			struct dk_ioctl_data select_args;
			u_char select_cmd[SC_CLASS0_SZ];
			u_int  *select_data = (u_int *)ioctl_data;

			if(smfd->s_opens != 1) {	/* must be only user.
				This check can't work quite right,
				since if there have been multiple opens, and
				all but one are closed, we still think there
				are multiple opens...  Better safe than sorry. */
				error = EBUSY;
				break;
			}
#if _MIPS_SIM == _ABI64
			if(!ABI_IS_IRIX5_64(get_current_abi())) {
			    struct irix5_dk_ioctl_data i5_sa;

			    if(copyin(arg, &i5_sa, sizeof(i5_sa))) {
				error = EFAULT;
				break;
			    }
			    else {
				irix5_dkioctl_to_dkioctl(&i5_sa, &select_args);
			    }
			}
			else
#endif /* _ABI64 */
			if(copyin(arg, &select_args, sizeof(struct dk_ioctl_data))) {
				error = EFAULT;
				break;
			}
			/* we don't let them change the LUN mapping */
			if(select_args.i_len > MAX_IOCTL_DATA || select_args.i_page == 22)
				goto invalerr;
			bcopy(smfd_mode_select, select_cmd, sizeof(select_cmd));
			select_cmd[1] = smfd->s_selflags;
			select_cmd[4] = select_args.i_len;
			if(copyin(select_args.i_addr, select_data, 
			    select_args.i_len)) {
				error = EFAULT;
				break;
			}
			if((error = smfd_docmd(smfd, select_cmd, SC_CLASS0_SZ, SMFD_TIMEOUT, 
			    (u_char *)select_data, select_args.i_len, B_WRITE)) ||
				(error = smfd_docmd(smfd, smfd_rezero, SC_CLASS0_SZ,
					SMFD_TIMEOUT,(u_char *)0, 0, B_READ)))
				goto ioerr;
			break;
		}

	case DIOCREADCAPACITY:
		if(copyout(&smfd->s_capacity, arg, sizeof(u_int)))
		    error = EFAULT;
		break;

	case DIOCMKALIAS:
	{
		extern void	smfd_alias_make(vertex_hdl_t);

#if DEBUG_FLOPPY
		printf("making smfd floppy mode aliases\n");
#endif		
		smfd_alias_make(dev);
		break;
	}
	default:
invalerr:
		error = EINVAL;
		break;
	}

	if(ioctl_data)
		kern_free(ioctl_data);

	return error;
}


/*
 * read from device
 */
int
smfdread(dev_t dev, uio_t *uiop)
{
	return uiophysio((int (*)())smfdstrategy, 0, dev, B_READ, uiop);
}

/*
 * write to device
 */
int
smfdwrite(dev_t dev, uio_t *uiop)
{
	return uiophysio((int (*)())smfdstrategy, 0, dev, B_WRITE, uiop);
}


/*
 * queue device request 
 */
void
smfdstrategy(register struct buf *bp)
{
	register struct buf *firstbp;
	register struct iobuf *list_head;
	register int sc;
	smfd_t *smfd;

	smfd = SMFD_SC(SUI_INFO(SMFD_UNITINFO(bp->b_edev)));

	/*
	 * dwong: swapped the next 2 conditional blocks such that we don't
	 * have to set up smfd->s_secsize in smandchksetup
	 */
	if(!(smfd->s_flags & SMFD_SETUP)) {
		sc = smchkandsetup(smfd, (bp->b_flags&B_READ)?FREAD:FWRITE);
		if(sc) {
			bp->b_error = sc;
			goto bad;
		}
	}
	if(bp->b_bcount % smfd->s_secsize) {
		bp->b_error = EIO;
		goto bad;
	}

	/*	we must re-calculate the block #, since our sector size
		may not match the hardwired NBPSCTR in the rest of the kernel.
		Better not plan on ever using floppy for filesytems, at
		least not with a blocksize != NBPSCTR!.  In particular,
		the btod() macro used in physio() will give the wrong
		result.  BUT, this is not true if we weren't called
		from physio!!! (i.e., being used as a block device).
	*/
	if(bp->b_flags & B_PHYS) {
		bp->b_blkno = (bp->b_blkno * NBPSCTR) / smfd->s_secsize;
	}

	if(bp->b_blkno < 0) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* if request goes past end, do as much as we can */
	sc = min(smfd->s_capacity - bp->b_blkno, bp->b_bcount/smfd->s_secsize);
	if(sc <= 0)  {
		if(!bp->b_bcount)
			goto transdone; /* Finish up on 0 length transfers */
		bp->b_resid = bp->b_bcount;
		if((bp->b_flags & B_READ) && sc == 0)
			goto transdone;	/* just return short */
		/* write past end, return err */
		bp->b_error = ENOSPC;
		goto bad;
	}

	/* set block number for disksort() */
	bp->b_sort = bp->b_blkno;
	bp->av_forw = NULL;

	/* update accounting for this buffer */

	bp->b_start = lbolt;

	sc = mutex_spinlock(&smfd->s_lock);
			
	list_head = &smfd->s_queue;

	/* queue request */
	disksort(&smfd->s_queue, bp);
	if(smfd->s_queue.io_state.active == 0)  {
		smfd->s_queue.io_state.active = 1;
		/* New request */
		smfd->s_retry = 0;
		firstbp = list_head->io_head;
		mutex_spinunlock(&smfd->s_lock, sc);
		smfdstart(firstbp);
	}
	else
		mutex_spinunlock(&smfd->s_lock, sc);
	return;
bad:
	bp->b_flags |= B_ERROR;
transdone:
	iodone(bp);
	return;
}
/*
 * print controller/unit number for a smfd device
 */
void
smfdprint(dev_t dev, char *str)
{
	char	devname[MAXDEVNAME];
	vertex_to_name(dev,devname,MAXDEVNAME);
	cmn_err(CE_CONT, "%s: %s\n", devname, str);
}



/*	return device size, in blocks (called via bdevsw, possibly direct
	from kernel code, probably not by user) */
int
smfdsize(dev_t dev)
{
	int capacity;
	smfd_t *smfd;

	smfd = SMFD_SC(SUI_INFO(SMFD_UNITINFO(dev)));
	if(smfd == NULL)  {
		if( smfdopen(&dev, 0, OTYP_LYR, (cred_t *)NULL) )
			capacity =  -1;
		else {
			smfd = SMFD_SC(SUI_INFO(SMFD_UNITINFO(dev)));
			capacity = smfd->s_spc * smfd->s_cyls;
			smfdclose(dev, 0, OTYP_LYR, (cred_t *)NULL);
		}
	}
	else /* already open */
		capacity = smfd->s_capacity;
	return capacity;
}


/*
 * setup a device operation
 */
static void
smfdstart(struct buf *bp)
{
	scsi_unit_info_t	*unit_info = SMFD_UNITINFO(bp->b_edev);
	smfd_t 			*smfd = SMFD_SC(SUI_INFO(unit_info));
	register scsi_request_t *sp = &smfd->s_sr;
	int sn, sc;

	bp->b_resid = bp->b_bcount;
	sn = bp->b_blkno;
	sc = bp->b_bcount/smfd->s_secsize;

	/* Let the scsi driver do the transfer */
	bcopy((bp->b_flags & B_READ) ? smfd_read : smfd_write, sp->sr_command, 
	    SC_CLASS1_SZ);

	/* set up logical block and length */
	sp->sr_command[2] = (unchar)(sn >> 24);
	sp->sr_command[3] = (unchar)(sn >> 16);
	sp->sr_command[4] = (unchar)(sn >> 8);
	sp->sr_command[5] = (unchar)sn;
	sp->sr_command[7] = (unchar)(sc >> 8);
	sp->sr_command[8] = (unchar)sc;

	smfd->s_lastcmd = sp->sr_command[0];
	sp->sr_cmdlen = SC_CLASS1_SZ;
	sp->sr_bp = (void *)bp;
	sp->sr_notify = smfd_intr;
	sp->sr_dev = (void *)smfd;
	/* ensure that no stale flags set (like SRF_MAPBP!), and that
	 * we don't fail if we had a unit attn (when we do get unit atn,
	 * we handle it immediately in the callback stuff or via
	 * an explict call to smfd_chkcond) */
	if (bp->b_flags & B_READ)
		sp->sr_flags |= SRF_DIR_IN|SRF_AEN_ACK;
	else
		sp->sr_flags = SRF_AEN_ACK;
	if(sp->sr_buflen = bp->b_bcount) {
		if (BP_ISMAPPED(bp)) {
			sp->sr_flags |= SRF_MAP;
			sp->sr_buffer = (u_char *)bp->b_dmaaddr;
			/* this should be unnecessary, since this should *only*
			 * be called with requests from smfd{read,write}, and
			 * physio should be flushing the cache for those.
			 * Nonetheless, I have operational proof that it is
			 * needed; and I don't have time to track it down...
			 * Old code always did a wbinval, which was wasteful
			 * Olson, 5/93 */
			if(sp->sr_flags & SRF_DIR_IN)
				dki_dcache_inval(sp->sr_buffer, sp->sr_buflen);
			else 
				dki_dcache_wb(sp->sr_buffer, sp->sr_buflen);
		}
		else
			sp->sr_flags |= SRF_MAPBP;
	}

	/* timeout of .5 sec per sector read/written, with a min of 30 secs */
	if(sc < 60)
		sp->sr_timeout = 30 * HZ;
	else
		sp->sr_timeout = (sc/2) * HZ;
	if(!smfd->s_retry)
		smfd->s_lastsense = 0;	/* at start of cmd, not on repeat! */
	
	SUI_COMMAND(unit_info)(sp);
}

/*
 * smfd interrupt routine, called via scsi_subrel().
 *
 */
void
smfd_intr(register scsi_request_t *sp)
{
	register struct buf *bp = (struct buf*)sp->sr_bp;
	int error = 0;

	/* check status */
	if(smfd_chkcond(sp,&error, 1))
		return;

	bp->b_resid = sp->sr_resid;

	bioerror(bp,error);

	smfd_intrend((smfd_t *)sp->sr_dev, bp);
}



/*
 * retry - called from smfd_chkcond only.
 */
int retry(scsi_request_t *sp, int *errp, int doretry)
{
	smfd_t *smfd = (smfd_t*)sp->sr_dev; 
	if (smfd->s_retry++ < SMFD_MAX_RETRY){
		if (!doretry){
			/* Delay and then let caller retry. */
			delay(HZ * smfd->s_retry);
			return 1; 
		}
		else{
			/* Delay and then reinitiate the command. */
			timeout(smfd_retry,sp,HZ * smfd->s_retry);
			return 1;
		}
	}
	/* Give up, busy for too lone */
	*errp = EIO;
	return 0;
}


/* check the return sense
 * return vals:
 *	if doretry set, and retries not exhausted, retry cmd.
 *	0: command shoudn't be/wasn't retried
 *      1: was/should be retried.
 */
int smfd_chkcond(scsi_request_t *sp, int *errp, int doretry)
{
	register smfd_t *smfd = (smfd_t *)sp->sr_dev;
	struct buf *bp;
	int cond_ret;
	
	bp = (struct buf *)sp->sr_bp;
	*errp = 0;
	if (sp->sr_status == SC_GOOD) {
		if (sp->sr_sensegotten > 0) {
			cond_ret = smfd_interpret_sense(sp);
			if (cond_ret == SC_WRITEFAULT || cond_ret == SC_READERR
			    || (cond_ret == SC_RESET && !(smfd->s_flags & SMFD_NORETRY))) {
			  *errp = EIO;

			  /*
			   * Insite drives do their own hardware retries so
			   * we don't retry their read/write errors.
			   */
			  if (!IS_INSITE || (cond_ret == SC_RESET))
			    /* retry the cmd (silently, since floppies
			     * are prone to errors...) */
			    if (smfd->s_retry++ < SMFD_MAX_RETRY) {
			      if(doretry)
				smfdstart(bp);
			      /* else caller retries */
			      return 1;
			    }
			  
			  smfd_rpt_err(smfd->s_dev,
				       "on block %d, status %d on disk status check\n",
				       bp ? bp->b_blkno : -1,
				       sp->sr_scsi_status);
			  return 0;
			  
			} else if (cond_ret) {
				switch (cond_ret) {
				      case SC_WRITEPROT:
					*errp = EROFS;
					break;
					
					/* no media, reset, media change, still comming ready*/
				      case SC_NOTREADY:
					
					/* SCSI-II allows a drive to return NOTREADY but with a
					   substatus indicating that it is becomming ready.
					   In this case, retry. */
					if (smfd->s_flags & SMFD_READYING)
					  return retry(sp,errp,doretry);
					   
				      case SC_RESET:
					*errp = ENODEV;
					break;
					   
				      default:
					*errp = EIO;
					break;
				}
			}
					   
		} else if (sp->sr_scsi_status == ST_BUSY){
			smfd_rpt_err(smfd->s_dev, "BUSY --\n");
			return retry(sp,errp,doretry);
					   
		} else if (sp->sr_scsi_status != ST_GOOD) {
			smfd_rpt_err(smfd->s_dev,
				     "%d, status %d on disk status check\n",
				     sp->sr_status, sp->sr_scsi_status);
			*errp = EIO;
		}
							
	} else {
		smfd_rpt_err(smfd->s_dev,
			     "%d, status %d on disk status check\n",
			     sp->sr_status, sp->sr_scsi_status);
		*errp = EIO;
	}
								     
	return 0;
}


/* smfd interrupt clean-up routine -- common wrapup code for
 *	smfd->s_intr and smfd->s_checkintr.
 */
void
smfd_intrend(smfd_t *smfd, register struct buf *bp)
{
	register struct iobuf *list_head;
	register struct buf *nextbp;
	int s;

	list_head = &smfd->s_queue;

	s = mutex_spinlock(&smfd->s_lock);

	list_head->io_head = bp->av_forw;

	/*
	 * As synchronous requests are pretty rare, give them first crack.
	 * Otherwise start up the next asynchronous request, if any.
	 */
	if (sv_signal(&smfd->s_wait) ||
		(nextbp = list_head->io_head) == NULL) {
		list_head->io_state.active = 0;
		mutex_spinunlock(&smfd->s_lock, s);
	}
	else {
		/* start next request */
		smfd->s_retry = 0;
		mutex_spinunlock(&smfd->s_lock, s);
		smfdstart(nextbp);
	}

	biodone(bp);
}


/*
 *  Called from timeout, used to retry a command when a drive returns busy.
 */
void smfd_retry(scsi_request_t *sp)
{
	smfdstart((struct buf *)sp->sr_bp);
}

void
smfd_endcmd(struct scsi_request *sp)
{
	smfd_t *smfd = (smfd_t*)sp->sr_dev; 
	vsema(&smfd->s_done);
}


/* do a "synchronous command; block until it completes */
static void
smfd_sendcmd(smfd_t *smfd, struct scsi_request *sp)
{

	mutex_lock(&smfd->s_sema, PRIBIO);
	sp->sr_notify = smfd_endcmd;
	sp->sr_dev = (void *)smfd;
	SUI_COMMAND(scsi_unit_info_get(sp->sr_dev_vhdl))(sp);
	psema(&smfd->s_done, PRIBIO);
	mutex_unlock(&smfd->s_sema);
}

#ifdef DEBUG_FLOPPY
static char cmdbyte;
#endif
/*	General synchronous disk command.
	Retries cmd on some errors
*/
smfd_docmd(smfd_t *smfd, const u_char *cmd_buffer, int cmd_size, u_int timeoutval,
	u_char *addr, size_t len, int rw)
{
	register scsi_request_t	*sp = &smfd->s_sr;
	int error;

#ifdef DEBUG_FLOPPY
	cmdbyte = *cmd_buffer;
#endif

	smfd->s_lastsense = 0;	/* at start of cmd, not on repeat! */
	smfd->s_retry = 1;	/* retry once if needed (on some errors) */

	do {
		smfd_drive_acquire(smfd);
		sp->sr_buffer = addr;
		sp->sr_buflen = len;
		bcopy(cmd_buffer, sp->sr_command, cmd_size);

		smfd->s_lastcmd = *cmd_buffer;
		sp->sr_cmdlen = cmd_size;

		sp->sr_timeout = timeoutval;

		if (rw&B_READ) {
			sp->sr_flags |= SRF_DIR_IN|SRF_MAP;
			if(len)
				dki_dcache_inval(addr, len);
		}
		else {
			sp->sr_flags &= ~SRF_DIR_IN;
			sp->sr_flags |= SRF_MAP;
			if(len && cachewrback)
				dki_dcache_wb(addr, len);
		}
			
		smfd_sendcmd(smfd,sp);

		smfd_drive_release(smfd);

		if(sp->sr_status != SC_GOOD) 
			return EIO;

	} while( smfd_chkcond(sp, &error, 0));

	return error;
}


char *NCRmsgs[] = {
	"Data field not found", /* 0x80 */
	"Data buffer parity error",	/* 0x81 */
	"Buffer not available",	/* 0x82 */
	"Controller cylinder error" /* 0x83 */
};


/*	We don't want to clutter console with 'common' floppy type errors,
	so this doesn't report a number of errors that the dksc driver does.
*/
int
smfd_interpret_sense(scsi_request_t *sp)
{
	smfd_t *smfd = (smfd_t *)sp->sr_dev;
	/* REFERENCED */
	register char *key_msg;
	/* REFERENCED */
	register char *addit_msg;
	register u_char sense_key, addit_sense, addit_senseq;
	u_char *sdata = sp->sr_sense;
	/* REFERENCED */
	int blkno;

	/* Always clear the readying flag */

	smfd->s_flags &= ~SMFD_READYING;

#ifdef DEBUG_FLOPPY
	if (smdbg)
		cmn_err(CE_CONT, "cmd:%x, key:%x, info:%x\n",
			cmdbyte, sdata[2] & 0xf, sdata[7] ? sdata[12] : 0xff);
#endif
	if( sp->sr_sensegotten == 0 )
		return 0;

	if((sense_key = sdata[2] & 0xf) == SC_NOSENSE) {
		return 0;	/* no sense */
	}
	/* if there isn't any additional sense, return ff if we
	 * can't figure it out from the sense key; there
	 * are cases where the NCR board returns no additional sense;
	 * all of them I have observed are where the floppy wasn't in
	 * the drive; we must NOT set addit_sense to 0, because that
	 * gets returned and indicates that there was no error! */
	addit_sense = sdata[7] ? sdata[12] : 0xff;
	addit_senseq = sdata[7]>1 ? sdata[13] : 0xff;

	if(sdata[0] & 0x80)
		blkno = (sdata[3] << 24) | (sdata[4] << 16) |
			(sdata[5] << 8)  | sdata[6];
	else
		blkno = -1;

	/* tried to write to a write-protected floppy.  */
	if(sense_key == SC_DATA_PROT ||
		(sense_key == SC_HARDW_ERR && addit_sense == SC_WRITEPROT))
		return SC_WRITEPROT;
	if(sense_key == SC_MEDIA_ERR) {
		/*	usually means formatted for different type,
		 * could also be a really bad floppy.  In any case,
		 * we don't print a message on this. */
		return addit_sense!=0xff ? addit_sense : SC_NOADDRID;
	}
	if(sense_key == SC_UNIT_ATTN && addit_sense == 0x28 &&
	   smfd->s_lastsense == SC_RESET) {
		/*	the problem here is that with the TEAC drive we have,
		 * and the NCR controller, that unitatn with media change
		 * is returned when no floppy is in the drive, and also
		 * after a parameter change;  notready doesn't get returned
		 * at any time.  This is because the Teac drive doesn't
		 * provide a ready pin.  We have no choice but to turn
		 * unit atn/media change into notready, but we only do it
		 * if we get it a second time in a row, since the first time
		 * could have been just a media or modesel change */
		sense_key = SC_NOT_READY;
		smfd->s_flags &= ~SMFD_SWAPPED;
	}
	if(sense_key == SC_UNIT_ATTN) {
		if (addit_sense == 0x28)	/* possible media changed */
			smfd->s_flags |= SMFD_SWAPPED;
		addit_sense = SC_RESET;  /* retry cmd */
		key_msg = NULL;
	}
	else if(sense_key == SC_NOT_READY){
		/*
		 * Generally this means no floppy in the drive or the door is open.
		 * This is common so we don't report it.
		 *
		 * However, SCSI-II allows a drive to return  SC_NOT_READY/SC_NOTREADY/1
		 * indicating that it is in the process of becomming ready.  The 
		 * Insite floptical is roughly SCSI-II so check this for it.  Because 
		 * we have already overloaded SC_NOTREADY to mean media isn't available,
		 * we set SMFD_READYING to qualify SC_NOTREADY to mean "not ready but
		 * will be soon".
		 */
		if ((IS_INSITE) && (addit_sense == SC_NOTREADY ) &&
		    (addit_senseq == 1))
			smfd->s_flags |= SMFD_READYING;

		addit_sense = SC_NOTREADY; 	/* also nulls key_msg below */
	      }
	else
	        key_msg = scsi_key_msgtab[sense_key];
	
	if(addit_sense < SC_NUMADDSENSE) {
		/* be quiet about a number of 'common' floppy errors; just return
		   error to program */
		switch(addit_sense) {
		case SC_RESET:
		case SC_NOTREADY:	/* see SC_NOT_READY above */
		case SC_WRITEPROT:
		case SC_NOINDEX:
		case SC_READERR:
			key_msg = NULL;
			break;
		default:
			addit_msg = scsi_addit_msgtab[addit_sense];
			break;
		}
	}
	else {
		if(addit_sense >= 0x80 && addit_sense <= 0x83)
			addit_msg = NCRmsgs[addit_sense-0x80];
		else
			addit_msg = NULL;
	}

#ifdef DEBUG_FLOPPY
	if(smdbg && key_msg != NULL)  {
		smfd_rpt_err(smfd->s_dev, "%s", key_msg);
		if(addit_msg != NULL) {
			cmn_err(CE_CONT, " %s", addit_msg);
			if(addit_sense == 0x20)	/* invalid/unsupported cmd */
				cmn_err(CE_CONT, " 0x%x", smfd->s_lastcmd);
		}
		if(blkno != -1)
			cmn_err(CE_CONT, ".  Block #%d\n", blkno);
		else 
			cmn_err(CE_CONT, ".\n");

		if (!(IS_TEAC)) {
			if(sdata[14])
				cmn_err(CE_CONT, "FRU = %d. ", sdata[14]);
			if(sdata[15] & 0x80)
				cmn_err(CE_CONT, "field ptr %d, ", 
					(((ushort)sdata[16]) << 8) | sdata[17]);
			if(sdata[18])
				cmn_err(CE_CONT, "recovery action %d, ",
					sdata[18]);
			if(sdata[19])
				cmn_err(CE_CONT, "%d retries. ", sdata[19]);
		}
	}
#endif /* DEBUG_FLOPPY */

	if (sense_key == SC_ERR_RECOVERED)
	  addit_sense = 0;

	smfd->s_lastsense = addit_sense;
	return(addit_sense);
}


smfd_getinq(smfd_t *smfd)
{
	u_char *inq;	/* used for results */
	int err = 0;
	u_char inquiry_cmd[SC_CLASS0_SZ];

#define INQSIZE 32
	inq = kmem_alloc(INQSIZE, KM_CACHEALIGN);
	bzero(inq, INQSIZE);
	bzero(inquiry_cmd, SC_CLASS0_SZ);

	inquiry_cmd[0] = smfd_inquiry[0];
	inquiry_cmd[4] = INQSIZE;
	if(smfd_docmd(smfd, inquiry_cmd, SC_CLASS0_SZ, SMFD_TIMEOUT,
	    inq, INQSIZE, B_READ) != 0) {
		smfd_rpt_err(smfd->s_dev, "inquiry cmd failed\n");
		err =  ENODEV;
	}
	else if(inq[0] || !(inq[1]&0x80)) {
		/*	should be direct access, removable media.
			NCR returns info in bits 0-6, so only look at RMB bit */
		smfd_rpt_err(smfd->s_dev,
			"inquiry reports wrong media type 0x%x %x\n",
		    inq[0], inq[1]);
		err =  EINVAL;
	}

	/* differentiate between NCR/TEAC and TEAC/TEAC combos */
	if(((smfd->s_curtype == FD_FLOP_35LO) ||
		(smfd->s_curtype == FD_FLOP_35)) &&
		(strncmp((const char *)&inq[8], "TEAC", 4) == 0)) {
		smfd->s_flags |= SMFD_TEAC;
	}
	/* check for Insite floptical drive, in which case we
	   set the SMFD_INSITE flag and unlock the drive. :randyh */
	if (((smfd->s_curtype == FD_FLOP_35_800K)  ||
	     (smfd->s_curtype == FD_FLOP_35_20M) ||
	     (smfd->s_curtype == FD_FLOP_35)   ||
	     (smfd->s_curtype == FD_FLOP_35LO)) &&
	     (!strncmp((const char *)&inq[8], "INSITE", 6) ||
	     !strncmp((const char *)&inq[8], "IOMEGA  Io20S", 13))){
	  smfd->s_flags |= SMFD_INSITE;
	  smfd_unlk_floptical(smfd);
	}

	/* Check for Syquest drive, in which case we set the SMFD_SYQUESTxx
	   flag.  :randyh */
	if (strncmp((const char *)&inq[8],"SyQuest",7)==0)
	    smfd->s_flags |= (SMFD_SYQUEST45|SMFD_SYQUEST88);
	kern_free(inq);
	return err;
}


/*	Set type based on arg.  Called from open.  */
int
smfd_settype(smfd_t *smfd)
{
	u_int  status;
	struct smfd_setup *s;
	u_char *sdata;

	smfd->s_spc = smfd_spc[smfd->s_curtype];
	smfd->s_cyls = smfd_cyls[smfd->s_curtype];
	smfd->s_spt = smfd_spta[smfd->s_curtype];

	/* set pin 2 meaning, spt, media type, etc */
	sdata = (u_char *)
		kmem_alloc(sizeof(struct dev_fdgeometry)+8+4, KM_CACHEALIGN);
	bzero(sdata, sizeof(struct dev_fdgeometry)+8+4);
	s = (struct smfd_setup *) kmem_alloc(sizeof(*s), KM_CACHEALIGN);
	bzero(s, sizeof(*s));
	s->tpi = smfd->s_curtype == FD_FLOP ? 48 : 96;
	s->sectrk = smfd_spta[smfd->s_curtype];
	s->cyls = smfd_cyls[smfd->s_curtype];
	/* we set these explicitly because some values that might be left
		over from a SETMODE ioctl might be in-compatible... (NCR
		does more checking than SMS, e.g., you can't have 9 spt with
		256 byte sectors... */
	s->secsize = 512;
	s->heads = 2;
	if(IS_TEAC) {
		s->motorondelay = 0xa;
		s->motoroffdelay = 0x46;
		if(smfd->s_curtype == FD_FLOP_35)
			s->headsettle = 22;
		else
			s->headsettle = 28;
	}
	status = smfd_setparms(smfd, s, sdata);
	kern_free(s);
	kern_free(sdata);
	if(status)
		return -1;

	/*
	 * We need to check that the drive is actually ready. 
	 * The older floppies have some bugs which makes smfd_recal the
	 * safest way to verify that the drive is ready.  For SCSI-II drives
	 * such as the Insite floptical, smfd_verify_state is used.
	 */
	if (IS_INSITE)
	  return smfd_verify_state(smfd);
	else 
	  return smfd_recal(smfd);
}


#ifdef DEBUG_FLOPPY
/* dump the drive geometry structure */
void
dumpgeom(struct dev_fdgeometry *geom, char *which)
{
	if(!smdbg)
		return;
	printf("%s: trate %x %x, %d head, %d spt, %x %x secsiz\n",
		which, geom->g_trate[0], geom->g_trate[1], geom->g_nhead,
		geom->g_spt, geom->g_bytes_sec[0], geom->g_bytes_sec[1]);
	printf("%x %x cyls, %x %x precomp, %x %x wrcurr, %x %x steprate, %x %x headsettle\n",
		geom->g_ncyl[0], geom->g_ncyl[1], geom->g_wprcomp[0], geom->g_wprcomp[1],
		geom->g_wrcurr[0], geom->g_wrcurr[1], geom->g_steprate[0], geom->g_steprate[1],
		geom->g_headset[0], geom->g_headset[1]);
	printf("%x motondel, %x motoffdel, trdy %d, startsec %d, motonpin %d, %d steps/cyl\n",
		geom->g_moton, geom->g_motoff,
		geom->g_trdy, geom->g_ssn,
		geom->g_mo, geom->g_stpcyl);
	printf("pin 1 %x, pin 2 %x, pin 4 %x, pin34 %x\n",
		geom->g_pin1, geom->g_pin2, geom->g_pin4, geom->g_pin34);
	
}
#endif /* DEBUG_FLOPPY */


/*	do setup for 'unusual' disks that require a page 5 mode select.
	We do a SENSE first, then set just the fields from the setup
	struct.  called from SMFDSETMODE ioctl.
	calculations for headsettle, etc. assume 500Kbit transfer rate.
	Structure members whose value is 0 use current settings.
	NOTE: even though NCR doesn't currently support the block
	descriptor, I've cleaned up the code so that whatever len
	is returned on modesense will be used on select, in case
	they add it, or we switch to yet another vendor.  This
	code is executed rarely enough that it doesn't really 
	matter how slow it is.
*/

int
smfd_setparms(smfd_t *smfd, struct smfd_setup *setup, u_char *sdata)
{
	u_char cmd[SC_CLASS0_SZ];
	struct dev_fdgeometry *geom, insite_geom;
	static char default_NCR[] =  {
		0x00, 0x16, 0x00, 0x00, 0x05, 0x1e, 0x00, 0xfA,
		0x02, 0x08, 0x02, 0x00, 0x00, 0x50, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x08,
		0x32, 0xc0, 0x00, 0x0c, 0x00, 0x00, 0x13, 0x10,
		0x00, 0x00, 0x00, 0x00,
	};
	static char default_TEAC[] =  {
		0x00, 0x1a, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x02, 0x00, 0x05, 0x1e, 0x00, 0xfa,
		0x02, 0x08, 0x02, 0x00, 0x00, 0x50, 0x00, 0x50,
		0x00, 0x50, 0x00, 0x28, 0x00, 0x01, 0x18, 0x04,
		0x46, 0x60, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00,
		0x00, 0x00, 0x00, 0x00, 
	};
	static char default_INSITE[] = {
		0x23, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x02, 0x00, 0x05, 0x16, 0x04, 0xb0,
		0x02, 0x12, 0x02, 0x00, 0x00, 0x50, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28,
		0x00, 0x00, 0x00, 0x00,
	};
	u_char *s_data;
	
	bcopy(smfd_mode_sense, cmd, sizeof(cmd));
	cmd[2] = 0x5;		/* page 5 */
	/* allow for 4byte header, and 1 8 byte block desc */
	cmd[4] = sizeof(*geom)+8+4;
	
	/*
	 * if the previous open uses the wrong type, mode sense may return
	 * with check condition.  use hard coded default to get around this
	 * problem
	 */
	if (smfd_docmd(smfd, cmd, sizeof(cmd), SMFD_TIMEOUT, sdata, 
		       sizeof(*geom)+8+4, B_READ)) {
		if (IS_TEAC)
			bcopy(default_TEAC, sdata, sizeof(default_TEAC));
		else
			if (IS_INSITE)
				bcopy(default_INSITE, sdata, sizeof(default_INSITE));
			else
				bcopy(default_NCR, sdata, sizeof(default_NCR));
		
	}
	
	geom = (struct dev_fdgeometry *)&sdata[sdata[3]+4];
#ifdef DEBUG_FLOPPY
	dumpgeom(geom, "sense");
#endif
	
	/* set the media type */
	if(IS_TEAC) {
		/*
		  TEAC/TEAC does not return the correct page code during sense
		  */
		geom->g_pgcode = 0x5;
		
		if(smfd->s_curtype == FD_FLOP_35)
			sdata[1] = SMFD_TEAC_TYPE_35;
		else
			sdata[1] = SMFD_TEAC_TYPE_35LO;
	}
	else
	  /*
	   * Smfd_types[FD_FLOP_35] is wrong for the INSITE drive.
	   * Fix it here.
	   */
	  if ((IS_INSITE) && (smfd->s_curtype==FD_FLOP_35))
	    sdata[1]= SMFD_INSITE_TYPE_35;
	  else
	    sdata[1] = smfd_types[smfd->s_curtype];
	
	if (IS_INSITE){
		/* Insite doesn't support setting the trate, number of heads, spt, etc.
		   However, we later will need the the values returned by the
		   select command so we save them here.*/
		bcopy(geom,&insite_geom,sizeof insite_geom);
		geom->g_trate[0]=
		  geom->g_trate[1] = 
		    geom->g_nhead =
		      geom->g_spt =
			geom->g_bytes_sec[0] =
			  geom->g_bytes_sec[1] =
			    geom->g_ncyl[0] =
			      geom->g_ncyl[1] = 0;
	      }
	else {
		if(setup->heads)
			geom->g_nhead = setup->heads;
		if(setup->sectrk)
			geom->g_spt = setup->sectrk;
		if(setup->secsize)  {
			sdata[10] = geom->g_bytes_sec[0] = setup->secsize >> 8;
			sdata[11] = geom->g_bytes_sec[1] = (u_char)setup->secsize;
		}
		if(setup->cyls)  {
			geom->g_ncyl[0] = setup->cyls >> 8;
			geom->g_ncyl[1] = (u_char)setup->cyls;
		}
	}
	/*
	 * TEAC/TEAC does not return the default values for these fields during
	 * a mode sense command, so we have to set them
	 */
	if(IS_TEAC) {
		geom->g_wprcomp[0] = 0x0;
		geom->g_wprcomp[1] = 0x0;
		geom->g_wrcurr[0] = 0x0;
		geom->g_wrcurr[1] = 0x0;
		if(smfd->s_curtype == FD_FLOP_35) {
			geom->g_steprate[0] = 0x0;
			geom->g_steprate[1] = 0x20;
		}
		else {
			geom->g_steprate[0] = 0x0;
			geom->g_steprate[1] = 0x28;
		}
	}
	
	/* INSITE and NCR/TEAC do not support head settle time */
	if (IS_INSITE || !(IS_TEAC))
		setup->headsettle = 0;
	if(setup->headsettle)  {
		setup->headsettle *= 10;
		geom->g_headset[0] = setup->headsettle >> 8;
		geom->g_headset[1] = (u_char)setup->headsettle;
	}
	
	
	/* Insite does not support motor on delay and implements motor off
	   delay in the motor on delay position */
	
	if (IS_INSITE){
		if (setup->motoroffdelay)
			geom->g_moton = setup->motoroffdelay;
	}
	else{
		if(setup->motorondelay)
			geom->g_moton = setup->motorondelay;
		if(setup->motoroffdelay)
			geom->g_motoff = setup->motoroffdelay;
	}
	
	if(setup->tpi)
		geom->g_stpcyl = setup->tpi == 96 ? 0 : 1;
	
	/*	have to set pin2 to control speed if using st 48tpi; 5 to set
		density only works if doing FD_FLOP_AT, or 3.5in high density,
		3 for speed works for all 3 NCR types. */
	if(geom->g_stpcyl == 1)
		geom->g_pin2 = 3;

	if (!(IS_INSITE))
		if(smfd->s_curtype == FD_FLOP_AT ||
		   smfd->s_curtype == FD_FLOP_35) {
			geom->g_trate[0] = 1;
			geom->g_trate[1] = 0xF4;
		}
		else{
			geom->g_trate[0] = 0;
			geom->g_trate[1] = 0xFA;
		}
	
	if(IS_TEAC) {
		/* for TEAC/TEAC combo */
		geom->g_pin34 = 0x2;
		geom->g_pin4 = 0x2;
		geom->g_pin1 = 0x0;
		if(smfd->s_curtype == FD_FLOP_35) {
			geom->g_pin2 = 0xd;
		}
		else {
			geom->g_pin2 = 0x5;
		}
	}
	else
		/* For non-Insite drives, set the pin data. */
		if (!(IS_INSITE)){
			if(smfd->s_curtype == FD_FLOP_35 || smfd->s_curtype == FD_FLOP_35LO) {
				
				/* for TEAC/NCR combo; we are sort of screwed here,
				   because we would really like a ready value
				   (see smfd_interpret_sense) but we can't get it;
				   setting it to open is even worse as far as the sense
				   values returned go. */
				geom->g_pin2 = 5;
				geom->g_pin34 = 2;
			}
			
			else {			/* 5 1/4 inch drive */
				geom->g_pin2 = 3;
				geom->g_pin34 = 1;
			}
		}
	
	if (IS_TEAC) {
		sdata[4] = sdata[5] = sdata[6] = 0;
		sdata[7] = sdata[8] = sdata[9] = 0;
		
		if (setup->secsize) {
			sdata[10] = setup->secsize >> 8;
			sdata[11] = setup->secsize & 0xff;
		}
	}
	
	bcopy(smfd_mode_select, cmd, sizeof(cmd));
	/* NCR and TEAC mark these as reserved on select */
	sdata[0] = sdata[2] = 0;
	
	/* Insite marks this as reserved on select */
	if (IS_INSITE)
		sdata[1] = 0;
	
#ifdef DEBUG_FLOPPY
	dumpgeom(geom, "select");
#endif
	
	/*
	 * always set the cmd byte and the datalen the same, just in case
	 * we get short data back on the sense, or the firmware changes;
	 * otherwise can get UNEX_RDATA errs
	 * dwong: we want to force mode select here such that we can recover
	 *        from an earlier open with a wrong type
	 */
	cmd[4] = sdata[3]+4+2+sdata[sdata[3]+5];
	(void)smfd_docmd(smfd, cmd, SC_CLASS0_SZ, SMFD_TIMEOUT, sdata, cmd[4],
			 B_WRITE);
	
	/* Recall that insite drive didn't allow us to set anything
	   but the motor on delay.  Set all the smfd-> geometry data from
	   the saved insite geometry data.  Note that we do this by
	   redirecting geom to point to the saved Insite geometry page. */
	if (IS_INSITE)
		geom = &insite_geom;

	/* should probably be looking at the logical block size in the block
	 * descriptor, if any, and non-zero. */
	smfd->s_secsize = ((u_int)geom->g_bytes_sec[0]<<8) +
		geom->g_bytes_sec[1];

	/* We have seen errors where a 0 block size is returned. That will
	   produce a divide by zero exception elsewhere in the driver.  Check
	   for a zero sector size. */
	if (smfd->s_secsize==0)
	  return -1;

	smfd->s_cyls =  ((u_short)geom->g_ncyl[0]<<8) + geom->g_ncyl[1];
	smfd->s_spt = geom->g_spt;
	smfd->s_spc = smfd->s_spt * geom->g_nhead;
	
	s_data = kmem_alloc(8, KM_CACHEALIGN);
	if(smfd_docmd(smfd, smfd_readcapacity, SC_CLASS1_SZ, SMFD_TIMEOUT,
		      s_data, 8, B_READ) == 0) {
		smfd->s_capacity = (s_data[0]<<24) + (s_data[1]<<16)
			+ (s_data[2]<<8) + s_data[3] + 1;
		kern_free(s_data);
		return 0;
	} else {
		/* Check for Insite floptical device.  It fails read-capacity on
		   unformatted media.  Instead set the capacity to the capacity
		   appropriate to device. This allows programs to open
		   the drive to format it.  It would be better to actually
		   check the status code to verify why the readcapacity failed.
		   :randyh */
		
		if (IS_INSITE){
			smfd->s_capacity = smfd->s_spc * smfd->s_cyls;
			kern_free(s_data);
			return 0;
		}
		
		kern_free(s_data);

		/* just failed trying to readcapacity ...
		 * check to see if it was due to NOMEDIA in
		 * the drive ...
		 */
		smfd->s_flags &= ~SMFD_SWAPPED;
		if (smfd_docmd(smfd, smfd_tst_unit_rdy, SC_CLASS0_SZ,
			SMFD_TIMEOUT, (u_char *)0, 0, 0)) {
			smfd->s_flags |= SMFD_NOMEDIA;
		}
		return -1;
	}
}

/* do a recal */
int
smfd_recal(smfd_t *smfd)
{
	return smfd_docmd(smfd, smfd_rezero, SC_CLASS0_SZ,
		SMFD_TIMEOUT, (u_char *)0, 0, 0);
}


/* format the specified track on NCR floppies only */
int
smfd_fmt_trk(smfd_t *smfd, int trk)
{
	u_char fmttrk[SC_CLASS0_SZ];
	int error;

	trk *= smfd->s_spt;	/* track is specified by logical sector #... */

	fmttrk[0] = 6;	/* format trk cmd is cmd 6 (vendor specific) */
	fmttrk[1] = (u_char)(trk >> 16) & 0x1f;
	fmttrk[2] = (u_char)(trk >> 8);
	fmttrk[3] = (u_char)trk;	/* track is specified by logical sector #... */
	fmttrk[4] = 0;	/* interleave to use */
	fmttrk[5] = 0;	/* reserved */

	if (error = smfd_recal(smfd))
		return error;

	return smfd_docmd(smfd, fmttrk, sizeof(fmttrk), SMFD_TIMEOUT, NULL,
		0, B_WRITE);
}

/* separate routine so printed same way everywhere */
void
/*VARARGS2*/
smfd_rpt_err(dev_t dev, char *fmt, ...)
{
	va_list ap;
	char	devname[MAXDEVNAME];

	vertex_to_name(dev,devname,MAXDEVNAME);
	cmn_err(CE_CONT, "%s error:  ",devname);
	va_start(ap, fmt);
	icmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}


/*	check to see if floppy is ready; if so do any setup that is
	needed.  Called from first open, and also from strategy, ioctl if
	device was opened with the FXECL flag, and the floppy hasn't
	yet been set up.
*/
int
smchkandsetup(smfd_t *smfd, int flag)
{
	u_char	status;

	if ((status = smfd_getinq(smfd)) != 0)
		return status;		/* no floppy drive */

	/*
	 * we should set up the drive in a reasonable state before actually
	 * doing anything to it
	 */
	 if (smfd_settype(smfd)) {
		if (smfd->s_flags & SMFD_NOMEDIA)
			return EAGAIN;
 
		return EIO;
	 }


	/*
	 * check for write protect
	 * following the tape drivers lead, and to make things easier for
	 * programs like SoftPC, we allow opens for read/write even if write
	 * protected (but not if open was for write only)
	 */
	if((flag & (FWRITE|FREAD)) == FWRITE) {
		u_char cmd[SC_CLASS0_SZ];
		/* 4byte header, 8 byte block desc */
		u_char *sdata;
		sdata = kmem_alloc(4, KM_CACHEALIGN);
		bcopy(smfd_mode_sense, cmd, sizeof(cmd));
		cmd[2] = 5;	
		cmd[4] = 4;	/* only need the 4 byte header */
		if(smfd_docmd(smfd, cmd, sizeof(cmd), SMFD_TIMEOUT, sdata, 
			4, B_READ)) {
			kern_free(sdata);
			return EIO;
		}
		if(sdata[0] > 2 && (sdata[2] & 0x80)) {
			kern_free(sdata);
			return EROFS;	/* high bit of byte 2 indicates
			* write-protect; not yet documented; only check if
			* we did actually get that data byte */
		}
		kern_free(sdata);
	}

	smfd->s_flags |= SMFD_SETUP;
	return 0;
}

/*
 * Acquire use of the subchannel as to not collide with other SCSI requests
 *
 * If a transfer is currently in progress, simply go to sleep.
 * Otherwise, set io_state.active and the drive is ours!
 * Note that this is essentially done 'inline' in dkscstrategy;
 * The two routines must be kept in sync.
 */
static void
smfd_drive_acquire(smfd_t *smfd)
{
	register struct iobuf *list_head;
	int s;
	scsi_request_t	*sp = &smfd->s_sr;

	sp->sr_flags = SRF_AEN_ACK;
	list_head = &smfd->s_queue;
	s = mutex_spinlock(&smfd->s_lock);

	while (list_head->io_state.active) {
		sv_wait(&smfd->s_wait, PRIBIO, &smfd->s_lock, s);
		mutex_spinlock(&smfd->s_lock);
	}
	list_head->io_state.active = 1;
	mutex_spinunlock(&smfd->s_lock, s);
}

/*
 * Release the drive and start up someone waiting for it.
 * Don't do it if we are dumping though!
 */
void
smfd_drive_release(smfd_t *smfd)
{
	register struct buf *nextbp;
	register struct iobuf *list_head;
	int s;

	list_head = &smfd->s_queue;

	s = mutex_spinlock(&smfd->s_lock);
	ASSERT(list_head->io_state.active != 0);
	if (sv_signal(&smfd->s_wait) || !(nextbp = list_head->io_head)) {
		list_head->io_state.active = 0;
		mutex_spinunlock(&smfd->s_lock, s);
	}
	else {
		smfd->s_retry = 0;
		mutex_spinunlock(&smfd->s_lock, s);
		smfdstart(nextbp);	/* start next request */
	}
}

/* Synchronous routine to unlock floptical. */

static void
smfd_unlk_floptical(smfd_t *smfd)
{
	u_char *sdata = kmem_alloc(INSITE_UNLOCKSZ, KM_CACHEALIGN);
	smfd_docmd(smfd, smfd_floptical_unlk, SC_CLASS0_SZ, SMFD_TIMEOUT,
		   sdata, INSITE_UNLOCKSZ, B_READ);
	kern_free(sdata);
}


/* 
 * Verify State -- based on algorithm defined by the SCSI-II standard.
 *
 * Returns success if and only if the unit is ready after three test-unit-
 * ready/reqest-sense command pairs.  Note that we don't actually do the
 * request-sense's, they are done automatically if the command
 * returns a check condition.
 *
 */
int smfd_verify_state(smfd_t* smfd){
	int pass=3, sts;
	while(pass-- && 
	      (sts = smfd_docmd(smfd, smfd_tst_unit_rdy, SC_CLASS0_SZ,
				SMFD_TIMEOUT, (u_char *)0, 0, 0)));
	return sts;
}

/*
 * Control media removal.  For drives that honor this request, manual
 *                         media removal can be enabled/disabled.
 */
void smfd_removal(smfd_t *smfd,int prevent_allow){
  u_char cmd[SC_CLASS0_SZ];
  bcopy(smfd_control_removal,cmd,SC_CLASS0_SZ);
  cmd[4] |=prevent_allow;
  smfd_docmd(smfd,cmd,SC_CLASS0_SZ,SMFD_TIMEOUT,(u_char*)0,0,0);
}

