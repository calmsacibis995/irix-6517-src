/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * nvram.h -- Offset and length information for variables stored
 *	in the Everest NVRAM.
 */

#ifndef __SYS_EVEREST_NVRAM_H__
#define __SYS_EVEREST_NVRAM_H__

#ident "$Revision: 1.17 $"


/*
 * Total amount of storage on the Everest NVRAM (in bytes) 
 */
#define NVLEN_MAX		4096

/*
 * For backwards compatibility with older IP19 proms, we need to
 * keep this value up-to-date, even though it is broken.  Bummer.
 */
#define NVOFF_OLD_CHECKSUM		0
#define	NVLEN_OLD_CHECKSUM		1

/*
 * Second byte is the NVRAM layout revision
 */
#define NVOFF_REVISION		(NVOFF_OLD_CHECKSUM + NVLEN_OLD_CHECKSUM)
#define NVLEN_REVISION		1

/*
 * bump the CURRENT_REV number each time the nvram layout is changed
 * BE CAREFUL CHANGING THIS: The IP19 prom uses these offsets to determine
 * the baud rate it should use for the EPC UART.  Note that since offsets
 * depend on each other, you should not change anything before any offset
 * used by the IP19 prom.
 */
#define NV_CURRENT_REV	 	10	
#define NV_BROKEN_REV		9	/* Rev with broken nvchecksum code */

/*
 * ARCS: console input and output devices are synthesized from "console".
 */
#define NVOFF_CONSOLE		(NVOFF_REVISION + NVLEN_REVISION)
#define NVLEN_CONSOLE		2

/* 
 * ARCS: system partition
 */
#define NVOFF_SYSPART		(NVOFF_CONSOLE + NVLEN_CONSOLE)
#define NVLEN_SYSPART		48

/* 
 * ARCS: OS loader
 */
#define NVOFF_OSLOADER		(NVOFF_SYSPART + NVLEN_SYSPART)
#define NVLEN_OSLOADER		18

/* 
 * ARCS: OS load filename
 */
#define NVOFF_OSFILE		(NVOFF_OSLOADER + NVLEN_OSLOADER)
#define NVLEN_OSFILE		28

/* 
 * ARCS: OS load options
 */
#define NVOFF_OSOPTS		(NVOFF_OSFILE + NVLEN_OSFILE)
#define NVLEN_OSOPTS		12

/*
 * color for textport background
 */
#define NVOFF_PGCOLOR		(NVOFF_OSOPTS + NVLEN_OSOPTS)
#define NVLEN_PGCOLOR		6

/*
 * lbaud/rbaud are the initial baud rates for the duart
 * This is used by the IP19 PROM.  DO NOT CHANGE THIS OFFSETS.
 */
#define NVOFF_LBAUD		(NVOFF_PGCOLOR + NVLEN_PGCOLOR)
#define NVLEN_LBAUD		5

/*
 * dbgtty contains the name of the serial device being used for
 * debugging.
 */
#define NVOFF_DBGTTY		(NVOFF_LBAUD + NVLEN_LBAUD)
#define NVLEN_DBGTTY		20

/*
 * indicate whether we are running as a diskless station or not
 */
#define NVOFF_DISKLESS		(NVOFF_DBGTTY + NVLEN_DBGTTY)
#define	NVLEN_DISKLESS		1

/* 
 * ARCS: timezone - number of hours behind GMT
 *	XXX - This really needs 56 bytes for full POSIX compatibility
 *		but the nvram doesn't have enought space for it.
 */
#define NVOFF_TZ		(NVOFF_DISKLESS + NVLEN_DISKLESS)
#define NVLEN_TZ		8

/* 
 * ARCS: OS load partition
 */
#define NVOFF_OSPART		(NVOFF_TZ + NVLEN_TZ)
#define NVLEN_OSPART		48

/* 
 * ARCS: autoload - 'Y' or 'N'
 */
#define NVOFF_AUTOLOAD		(NVOFF_OSPART + NVLEN_OSPART)
#define NVLEN_AUTOLOAD		1

/*
 * root contains the IRIX device name of the disk device
 * which actually contains the root partition (e.g. dks0d1s0).
 */
#define NVOFF_ROOT		(NVOFF_AUTOLOAD + NVLEN_AUTOLOAD)
#define NVLEN_ROOT		20

/*
 * diagmode controls diagnostic report level/ide actions.
 */
#define NVOFF_DIAGMODE		(NVOFF_ROOT + NVLEN_ROOT)
#define NVLEN_DIAGMODE		2

/*
 * netaddr is used by network software to determine the internet
 * address.  Format is 4 raw bytes, and is converted to . notation
 * in memory at prom initialization.
 */
#define NVOFF_NETADDR		(NVOFF_DIAGMODE + NVLEN_DIAGMODE)
#define NVLEN_NETADDR		4

/*
 * nokbd indicates if the system is allowed to boot without a keyboard
 */
#define NVOFF_NOKBD		(NVOFF_NETADDR + NVLEN_NETADDR)
#define NVLEN_NOKBD		1

/*
 * lang is the language desired (format: en_USA)
 * keybd overrides the key map for the returned keyboad
 */
#define NVOFF_KEYBD		(NVOFF_NOKBD + NVLEN_NOKBD)
#define NVLEN_KEYBD		5
#define NVOFF_LANG		(NVOFF_KEYBD + NVLEN_KEYBD)
#define NVLEN_LANG		6


/*
 * password_key is an encrypted key for protecting manual mode
 * netpasswd_key ditto for network access
 */
#define PASSWD_LEN		8
#define	NVOFF_PASSWD_KEY	(NVOFF_LANG+NVLEN_LANG)
#define	NVLEN_PASSWD_KEY  	(2*PASSWD_LEN+1)
#define	NVOFF_NETPASSWD_KEY	(NVOFF_PASSWD_KEY+NVLEN_PASSWD_KEY)
#define	NVLEN_NETPASSWD_KEY  	(2*PASSWD_LEN+1)

/*
 * scsiretries is number of times the scsi bus should be probed
 * looking for a disk before ultimately giving up.
 */
#define NVOFF_SCSIRT		(NVOFF_NETPASSWD_KEY+NVLEN_NETPASSWD_KEY)
#define NVLEN_SCSIRT		1

/*
 * scsihostid is the host adapter id for onboard scsi controllers.
 * Currently, if there is more than one controller, they will all 
 * be given the same id.
 */
#define NVOFF_SCSIHOSTID	(NVOFF_SCSIRT+NVLEN_SCSIRT)
#define NVLEN_SCSIHOSTID	1

/*
 * sgilogo adds the SGI product logo to the screen (unset for OEMs).
 * nogui surpresses gui and texport is just like a tty
 */
#define NVOFF_SGILOGO		(NVOFF_SCSIHOSTID+NVLEN_SCSIHOSTID)
#define NVLEN_SGILOGO		1
#define NVOFF_NOGUI		(NVOFF_SGILOGO+NVLEN_SGILOGO)
#define NVLEN_NOGUI		1

/*
 * Endianess specifies whether we want to run the PROM big or 
 * little endian.
 */
#define NVOFF_ENDIAN		(NVOFF_SGILOGO+NVLEN_SGILOGO)
#define NVLEN_ENDIAN		1

/*
 * Non-stop indicates that autoboot (if enabled) should continue
 * even if non-terminal hardware errors occur.
 */
#define NVOFF_NONSTOP		(NVOFF_ENDIAN+NVLEN_ENDIAN)
#define NVLEN_NONSTOP		1

/*
 * The hardware inventory stores information concerning what kind
 * of boards are installed in the Everest system, what there current
 * state is.  It also contains the mapping from virtual to physical
 * adapter ids.
 */
#define NVOFF_INVENT_VALID	(NVOFF_NONSTOP+NVLEN_NONSTOP)
#define NVLEN_INVENT_VALID	1
#define INVENT_VALID 		0x4a	

#define NVOFF_INVENTORY		(NVOFF_INVENT_VALID+NVLEN_INVENT_VALID)
#define NVLEN_INVENTORY		512

#define NVOFF_FASTMEM		(NVOFF_INVENTORY+NVLEN_INVENTORY)
#define NVLEN_FASTMEM		1

#define NVOFF_RBAUD		(NVOFF_FASTMEM+NVLEN_FASTMEM)
#define NVLEN_RBAUD		5

#define NVOFF_RESTART		(NVOFF_RBAUD + NVLEN_RBAUD)	
#define NVLEN_RESTART		8

#define NVOFF_ALTOSPATH		(NVOFF_RESTART+NVLEN_RESTART)
#define NVLEN_ALTOSPATH		50	

/* Used only in IP23 systems */
#define	NVRAM_SNUMVALID		0x5a
#define	NVOFF_SNUMVALID		(NVOFF_ALTOSPATH+NVLEN_ALTOSPATH)
#define	NVLEN_SNUMVALID		1

#define	NVOFF_SNUMSIZE		(NVOFF_SNUMVALID+NVLEN_SNUMVALID)
#define	NVLEN_SNUMSIZE		1

#define	NVOFF_SERIALNUM		(NVOFF_SNUMSIZE+NVLEN_SNUMSIZE)
#define	NVLEN_SERIALNUM		16

#define NVOFF_REBOUND		(NVOFF_SERIALNUM+NVLEN_SERIALNUM)
#define NVLEN_REBOUND		1
#define REBOUND_DEFAULT		""

/*
 * This checksum is more accurate than the old checksum and is
 * what should be used in general.
 */
#define NVOFF_NEW_CHECKSUM	(NVOFF_REBOUND+NVLEN_REBOUND)
#define NVLEN_NEW_CHECKSUM	1

#define NVOFF_PIGGYBACK		(NVOFF_NEW_CHECKSUM+NVLEN_NEW_CHECKSUM)
#define NVLEN_PIGGYBACK		1

#define	NVOFF_SPLOCK		(NVOFF_PIGGYBACK+NVLEN_PIGGYBACK)
#define	NVLEN_SPLOCK		20


/*
 * @@: 519323 - Need to log panic output to NVRAM on Challenge
 */
#ifdef EVEREST
#define NVOFF_FRUDATA           (NVOFF_SPLOCK+NVLEN_SPLOCK)
#define NVLEN_FRUDATA           2800 /* @@ Big buf for FRU output */
#define NVOFF_TOD_CYCLE		(NVOFF_FRUDATA+NVLEN_FRUDATA)
#define NVLEN_TOD_CYCLE		1
#define NVOFF_LOWFREE		(NVOFF_TOD_CYCLE+NVLEN_TOD_CYCLE)
#else
#define NVOFF_LOWFREE		(NVOFF_SPLOCK+NVLEN_SPLOCK)
#endif /* EVEREST */


/************************************************************************
 * The gap between NVOFF_LOWFREE and NVOFF_HIGHFREE is available for    *
 * future expansion.  We zero it, so it makes it easy to assume valid   *
 * default values.							*
 ************************************************************************/

#define NVOFF_HIGHFREE		(NVOFF_SERIALSPACE)

/************************************************************************
 * The following variables aren't included in the checksum checks.	*
 * This is useful for variables which need to be changed quickly, and 	*
 * for things which shouldn't necessarily be overwritten.		*
 ************************************************************************/

#define NVLEN_SERIALSPACE	32
#define NVOFF_SERIALSPACE	(NVOFF_PWRFAIL-NVLEN_SERIALSPACE)	
 
#define	NVLEN_PWRFAIL		1
#define	NVOFF_PWRFAIL		(NVOFF_PWRFAILTIME-NVLEN_PWRFAIL)

#define	NVLEN_PWRFAILTIME	4
#define	NVOFF_PWRFAILTIME	(NVOFF_LAST-NVLEN_PWRFAILTIME)

#define NVOFF_LAST		(NVLEN_MAX - 1)

/* total length of used read/write nv ram */
#define NVRW_TOTAL		(NVOFF_LAST)

/* Check maximum length */
#if NVOFF_LOWFREE >= NVOFF_HIGHFREE
	"ERROR! Exceeded NVRAM size!"
#endif

/* For compatible code with other platforms */
#define NVOFF_ENET	-1
#define NVLEN_ENET	-1
#define NVOFF_VOLUME	-1
#define NVLEN_VOLUME	-1


#ifdef LANGUAGE_C
/*
 * format used to store nvram table information
 */
#define MAXNVNAMELEN	32
struct nvram_entry {
    char nt_name[MAXNVNAMELEN];		/* string name of entry */
    char *nt_value;			/* PROM: string for default value */
					/* UNIX: current value of entry */
    int nt_nvaddr;			/* offset to entry in nvram */
    int nt_nvlen;			/* length of entry in nvram */
};

int nvram_enable_cpu_set(cpuid_t cpu_num, int enable);

#endif /* LANGUAGE_C */

#endif /* __SYS_EVEREST_NVRAM_H__ */

