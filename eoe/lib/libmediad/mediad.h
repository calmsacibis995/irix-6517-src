/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986,1988, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.20 $"

#ifndef __MEDIAD_H__
#define __MEDIAD_H__

#include <mntent.h> // for MNTMAXSTR
/*
 * media clsses are for return_remmeddev_class to say what kind of media
 * device it is based on the file name.  Pretty gross.  Could get rid
 * of it if I'm completely dependent on lego.
 */
#define RMEDCLASS_UNKNOWN	0
#define RMEDCLASS_CDROM		1
#define RMEDCLASS_FLOPPY	2
#define RMEDCLASS_TAPE		3
#define RMEDCLASS_ZIP		4
#define RMEDCLASS_JAZ		5
#define RMEDCLASS_OPTICAL	6
#define RMEDCLASS_PCCARD	7


#define MUSICTYPESTR		"music"
#define PHOTOCDTYPESTR		"photocd"
#define DOSTYPESTR		"dos"
#define EFSTYPESTR		"efs"
#define MACTYPESTR		"hfs"
#define HFSDIRTYTYPESTR		"hfs_dirty"
#define NFSTYPESTR		"nfs"
#define ISO9660TYPESTR		"iso9660"
#define ARCHIVETYPESTR		"archive"
#define	UNKNOWNTYPESTR		"unknown"
#define UNFORMATTED		"unformatted"
#define NOTREADYSTR		"notready"
#define INSTTYPESTR		"inst"

/*
 * Socket defs, message types, return codes, names of tables
 * FSDAUTO is a file that tells mediad on startup what to monitor
 * FSDTAB is a file that mediad updates as it monitors devices.  The socket
 * address is also there.
 */

#define SOCK_TEMPLATE		"/tmp/.mediadXXXXXX"
#define SOCK			"sock"
#define MEDIAD_SOCK_LEN		12

#define FSDAUTO			"/etc/fsd.auto"
#define FSDTAB			"/etc/fsd.tab"

#define EXOPT_MON		"mon"
#define EXOPT_INSCHK		"inschk"
#define EXOPT_RMVCHK		"rmvchk"


typedef struct emsg {
	long    mtype;
	int     scsi;
	int     ctrl;
	int	lun;
	char    filename[MNTMAXSTR];
	char    progname[MNTMAXSTR];
}   emsg_t;

typedef struct rmsg {
	long    mtype;
	int     error;
	char    mpoint[MNTMAXSTR];
}   rmsg_t;

#define MSG_EJECT   	0x000e9ec7L
#define MSG_RETURN  	0x0105e746L
#define MSG_TERM	0x0007e7f0L
#define MSG_DIE     	0x00091e00L
#define MSG_TEST    	0x0007e570L
#define MSG_TESTOK  	0x01000000L
#define MSG_SHOWMOUNT	0x02000000L
#define MSG_QUERY	0x04000000L
#define MSG_SUSPENDON	0x10000000L
#define MSG_SUSPENDOFF	0x20000000L
#define MSG_STARTENTRY  0x30000000L
#define MSG_STOPENTRY   0x40000000L
#define MSG_SETLOGLEVEL 0x80000000L


/*
 * Various timeout values for sending and receiving socket messages.
 * Timeout values for querying the objectserver.
 * Number of misses before I decide objectserver is down.
 * Polling interval of devices.
 */
#define SOCK_TIMEOUT		5
#define DUPMEDIAD_TIMEOUT	5
#define EJECT_TIMEOUT		45
#define KILL_TIMEOUT		45
#define MEDIAD_EXCLUSIVEUSE_TIMEOUT	30
#define OBJSERV_LONG_TIMEOUT	45
#define OBJSERV_SHORT_TIMEOUT	3
#define OBJSERV_UPDATE_TIMEOUT	1

#define OBJSERV_MISSEDCOUNT	25
#define DEFAULT_INSCHK_INTERVAL	3
#define DEFAULT_RMVCHK_INTERVAL 45
#define POSTEJECT_CHKS		15

#define FLEJECT_RETRY_COUNT	3
#define FLEJECT_RETRY_INTERVAL	2

#define RMED_NOERROR	  0
#define RMED_EUSAGE	  1
#define RMED_ECANTUMOUNT  2
#define RMED_ECANTEJECT   3
#define RMED_ENODISC      4
#define RMED_ENOSWEJECT   5
#define RMED_ENODEVICE	  6
#define RMED_ESYSERR	  7
#define RMED_EACCESS	  8
#define RMED_ENOMEDIAD	  9
#define RMED_TWOMEDIAD	  10
#define RMED_ENOUNIQUE	  11
#define RMED_ETERMSIG	  12
#define RMED_CONTEJECT	  13
#define RMED_NUMCODES	  14

/*
 * The controller, SCSI id, and the unit/type info for floppies are
 * encoded in the minor number as follows:
 * bit: 2 1 0 9 8 7 6 5 4 3 2 1 0
 * 0-3: unit/type (0=48, 1=96, 2=96hi, 3=5.8.800k, 4=3.5, 5=3.5hi, 6=3.5.20m)
 * 4-7: SCSI id
 * 8-11: controller
 * In other words, ctrl * 256, scsi * 16
 * For Tapes, the algorithm is ctrl * 512, scsi * 32
 * For CDROM, the algorithm is ctrl * 128, scsi * 1
 * see /dev/MAKEDEV
 */

#define FLOPPY_CTRL(dev) (((minor(dev) >> 11) * 10) | ((minor(dev) >> 8) & 0x7))
#define FLOPPY_SCSI(dev)  ((minor(dev) >> 4) & 07)
#define FLOPPY_UNIT(dev)  (minor(dev) & 0xf)
#define FLOPPY_LUN(dev)   (0)

#define TAPE_CTRL(dev) ((((minor(dev) >> 12) & 0xf) * 10) | \
			 ((minor(dev) >> 9) & 0x7))
#define TAPE_SCSI(dev)  ((minor(dev) >> 5) & 0xf)
#define TAPE_UNIT(dev)  (minor(dev) & 0x1f)
#define TAPE_LUN(dev)   (minor(dev) >> 16)

#define CDROM_CTRL(dev) (((minor(dev) >> 10) * 10) | ((minor(dev) >> 7) & 0x7))
#define CDROM_SCSI(dev)  (minor(dev) & 0xF)
#define CDROM_LUN(dev)	(minor(dev) >> 4 & 7)

#define DKSC_CTRL(dev)  ((((minor(dev) >> 11) & 0xF) * 10) | ((minor(dev) >> 8) & 0x7))
#define DKSC_SCSI(dev)  (minor(dev) >> 4 & 0xF)
#define DKSC_LUN(dev)	(minor(dev) >> 15 & 7)

/*
 * Function declarations.
 */
#ifdef __cplusplus
extern "C" {
#endif

int mediad_get_exclusiveuse(const char *spec_file, const char *prog_name);
void mediad_release_exclusiveuse(int exclusiveuse_id);
/*
 * mediad_last_error returns more information about the status of
 * the most recent call to mediad_get_exclusiveuse().
 *
 * If the most recent call successfully obtained exclusive use from
 * mediad, mediad_last_error returns 0.  If the most recent call has a
 * system error, mediad_last_error returns -1, and errno should be
 * consulted.  (The app must not overwrite errno in the meantime.)  If
 * the most recent call contacted mediad, but mediad refused exclusive
 * use, mediad_last error returns a positive value from the list of
 * RMED_E* values above.
 *
 * N.B.  If mediad is not running, mediad_get_exclusiveuse returns a
 * successful (nonnegative) value, mediad_last_error returns -1, and
 * errno == ECONNREFUSED.
 */
int mediad_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* !__MEDIAD_H__ */
