#ifndef __PATHNAMES_H__
#define __PATHNAMES_H__

/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.2 $"

/*****************************************************************************
 * defines for pathname manipulation. 
 * These are inevitably dependant on the SGI device naming conventions!
 *
 * We make the assumption that while new controller names (such as
 * 'ips', 'xyl', 'dks') may appear, the #d#s# format following will
 * NOT change, likewise that the /dev/dsk and /dev/rdsk locations of
 * block and character files for disk devices will not change.
 *
 *****************************************************************************/

struct pathanalysis {

	char	ctlrname[10];
	int 	ctlr;
	int 	drive;
	int 	lun;
	int 	partition;
	};

#define BLOCKPATHPREF	"/dev/dsk/"
#define BLOCKPATHLEN	9		
#define RAWPATHPREF	"/dev/rdsk/"
#define RAWPATHLEN	10	

/* dev_t to ctlr/unit/partition macros. 
 * XXXdh this will change when & if jag & SCSI are changed to take
 * advantage of more minors in a long dev_t */

#define PART_MASK	(0xf)
#define PARTOF(d)	(d & PART_MASK)
#define CTLR_UNITOF(d)	(d & ~PART_MASK)

/* ESDI and SMD */

#define CTLROF(d)	((d >> 6) & 0x3)
#define UNITOF(d)	((d >> 4) & 0x3)

/* SCSI */

#define SCSI_CTLROF(d)	SCSI_EXT_CTLR(DKSC_CTLR(dev))
#define OSCSI_UNITOF(d)	((d >> 5) & 0x7)
#define SCSI_LUNOF(d)	DKSC_LUN(dev)
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


/* Exported entrypoints */
extern int checkformat(char *p);
extern int pathtopart(char *p);
extern char *pathtovhpath(char *p);
extern char *devtopath(dev_t dev, int confirm);
extern char *devtorawpath(dev_t dev, int confirm);
extern void printpath(dev_t dev, int error, int confirm);

#endif	/* __PATHNAMES_H__ */
