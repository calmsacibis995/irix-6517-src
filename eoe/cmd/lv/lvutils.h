
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.12 $"
/* Structures for Logical Volume Utilities */

struct headerinfo	{
	struct volume_header 	vh;
	int			vfd;
	int			dev_bsize;
	int			firstfileblk;
	int			trksize;
	};

struct volmember	{
	struct volmember	*next;
	struct volmember	*prev;
	struct logvol		*lvol;
	unsigned		status;
	char			*pathname;
	dev_t			dev;
	int			devindex;	/* position of member in vol */
	int			ptype;
	struct headerinfo 	*vhinfo;
	struct lvlabel		*lvlab;
	int 			labsize;
	struct lvlabel		*newlvlab;
	int 			newlabsize;
	};

typedef struct volmember *vmp;

/* defines for volmember state */

#define BAD_NAME 		0x1
#define DUP_NAME 		0x2
#define CANT_ACCESS 		0x4
#define UNKNOWN_DEV     	0x8
#define ILLEGAL_PTYPE		0x10
#define WRONG_PTYPE		0x20
#define WRONG_PSIZE		0x40
#define WRONG_DEV		0x80
#define WRONG_DEVTYPE		0x100
#define GOT_HEADER 		0x1000
#define GOT_LABEL 		0x10000
#define BAD_LABEL 		0x20000
#define UNREADABLE_LABEL 	0x40000
#define WRONG_VOLUME 		0x80000
#define POSSIBLE_REPAIR		0x100000
#define RECLAIMED_ROGUE		0x200000

struct logvol	{
	struct logvol	*next;	
	unsigned	status;
	int		minor;
	struct lv	*lv;
	char		*name;
	char		*volid;
	unsigned	ndevs;
	unsigned	labels;	          /* # of labels found */
	unsigned	goodlabels;	  /* # of labels checked OK */
	unsigned	hilabel;   /* index of highest label found so far */
	struct lvlabel  *lvlab;   /* 'typical' label for the volume (based
				   * on majority verdict if there are
				   * inconsistencies). */
	unsigned	labsize;  /* size of this */
	int		ptype;	  /* raw or otherwise */
	struct lvtabent	*tabent;
	vmp		vrogue;   /* dup/bad members found during lvck that
				   * can't be assigned to a vmem slot */
	vmp 		vmem[1];  /* open array, allocated as req'd */
	};

/* defines for logvol state */

#define L_MISSDEV 	0x1
#define L_GOODPARTIAL 	0x4
#define L_BADPARTIAL 	0x8
#define L_BADPATHNAME	0x100
#define L_GOODTABENT   	0x1000
#define L_GOTALL_LABELS	0x2000
#define L_GOODLABELS   	0x4000
#define L_DEVSCHEKD    	0x8000
#define L_GOODEVAL	0x10000


/*****************************************************************************
 * defines for pathname manipulation. 
 * These are inevitably dependant on the SGI device naming conventions!
 *
 * We make the assumption that while new controller names (such as
 * 'ips', 'xyl', 'dks') may appear, the #d#s# format following will
 * NOT change, likewise that the /dev/dsk and /dev/rdsk locations of
 * block and character files for disk devices will not change.
 *
 * NOTE:
 *	The #d#s# format has been extended to include lun numbers.
 *	The old format will be preserved for devices with luns of 0,
 *	devices with nonzero luns will have the format #d#l#s#.
 *
 *****************************************************************************/

#define MAXCTLRNAMELEN	20

struct pathanalysis {

	char	ctlrname[MAXCTLRNAMELEN];
	int 	ctlr;
	int 	drive;
	int	lun;
	int 	partition;
	};

#define MAXDEVPATHLEN	30
#define BLOCKPATHPREF	"/dev/dsk/"
#define BLOCKPATHLEN	9		
#define RAWPATHPREF	"/dev/rdsk/"
#define RAWPATHLEN	10	

/* dev_t to ctlr/unit/partition macros. 
 * XXXdh this will change when & if jag & SCSI are changed to take
 * advantage of more minors in a long dev_t */

#define PARTOF(d)	(d & 0xf)

/* ESDI and SMD */

#define CTLROF(d)	((d >> 6) & 0x3)
#define UNITOF(d)	((d >> 4) & 0x3)

/* SCSI */

#define SCSI_CTLROF(d)	SCSI_EXT_CTLR(DKSC_CTLR(dev))
#define SCSI_LUNOF(d)	DKSC_LUN(dev)
#define OSCSI_UNITOF(d)	((d >> 5) & 0x7)
#define SCSI_UNITOF(d)	DKSC_UNIT(dev)

/* IPI */

#define IPI_CTLROF(d)	(major(d) & 0x3)
#define IPI_UNITOF(d)	((d >> 4) & 0xf)

/* jag SCSI */

#define JAG_CTLROF(d)	(major(d) & 0x7)
#define JAG_UNITOF(d)	((d >> 4) & 0xf)

/* defines for checks: check every device, OR accept a subset. (The latter
 * for use in the initial check of a volume which is being grown).
 */

#define COMPLETE_VOL 0
#define PARTIAL_VOL  1

#define MAXFDS		16
#define streq(x,y)	(strcmp(x,y)==0)

/* defines to control printing of bad volumes. */

#define EXISTING_VOLUME 0
#define NEW_VOLUME 	1
