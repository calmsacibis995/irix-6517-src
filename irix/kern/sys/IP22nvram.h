#ident "include/sys/IP22nvram.h: $Revision: 1.17 $"

/* 
 * IP22nvram.h - IP22/IP24/IP26 NVRAM offsets, etc. 
 */
#ifndef __SYS_IP22NVRAM_H__
#define __SYS_IP22NVRAM_H__

/*
 * Number of bytes on the 93cs56
 */
#define NVLEN_MAX		256

/*
 * First byte should be the checksum
 */
#define NVOFF_CHECKSUM		0
#define	NVLEN_CHECKSUM		1

/*
 * Second byte is the NVRAM layout revision
 */
#define NVOFF_REVISION		(NVOFF_CHECKSUM + NVLEN_CHECKSUM)
#define NVLEN_REVISION		1

/*
 * bump the CURRENT_REV number each time the nvram layout is changed
 */
#if IP26 || IP28
#define NV_CURRENT_REV		9
#else
#define NV_CURRENT_REV		8
#endif

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
 */
#define NVOFF_LBAUD		(NVOFF_PGCOLOR + NVLEN_PGCOLOR)
#define NVLEN_LBAUD		5

/*
 * indicate whether we are running as a diskless station or not
 */
#define NVOFF_DISKLESS		(NVOFF_LBAUD + NVLEN_LBAUD)
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
 * diagmode controls diagnostic report level/ide actions.
 */
#define NVOFF_DIAGMODE		(NVOFF_AUTOLOAD + NVLEN_AUTOLOAD)
#define NVLEN_DIAGMODE		2

/*
 * netaddr is used by network software to determine the internet
 * address.  Format is 4 raw bytes, and is converted to . notation
 * in memory at prom initilization.
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

/* NVOFF_LANG replaced with DLIF as lang was never used, and will always
 * be set to NULL so do not need a revision roll of the layout
 */
#define NVOFF_DLIF		(NVOFF_KEYBD + NVLEN_KEYBD)
#define NVLEN_DLIF		6


#define PASSWD_LEN		8
/*
 * password_key is an encrypted key for protecting manual mode
 * netpasswd_key ditto for network access
 */
#define	NVOFF_PASSWD_KEY	(NVOFF_DLIF+NVLEN_DLIF)
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
 * volume is the default audio volume for the system and is also the
 * volume at which the boot tune is played.  If volume is set to zero,
 * then the boot tune is not played.  If set to 255, then the boot tune
 * is played at full volume.
*/
#define NVOFF_VOLUME		(NVOFF_SCSIRT+NVLEN_SCSIRT)
#define NVLEN_VOLUME		3

/*
 * scsihostid is the host adapter id for onboard scsi controllers.
 * Currently, if there is more than one controller, they will all 
 * be given the same id.
 */
#define NVOFF_SCSIHOSTID	(NVOFF_VOLUME+NVLEN_VOLUME)
#define NVLEN_SCSIHOSTID	1

/*
 * sgilogo adds the SGI product logo to the screen (unset for OEMs).
 * nogui surpresses gui and texport is just like a tty
 */
#define NVOFF_SGILOGO		(NVOFF_SCSIHOSTID+NVLEN_SCSIHOSTID)
#define NVLEN_SGILOGO		1
#define NVOFF_NOGUI		(NVOFF_SGILOGO+NVLEN_SGILOGO)
#define NVLEN_NOGUI		1

/* encode rbaud as x*1200 */
#define NVOFF_RBAUD		(NVOFF_NOGUI+NVLEN_NOGUI)
#define NVLEN_RBAUD		1

/* controls recovery from AC fail */
#define NVOFF_AUTOPOWER		(NVOFF_RBAUD+NVLEN_RBAUD)
#define NVLEN_AUTOPOWER		1

/* overrides the monitor id */
#define NVOFF_MONITOR		(NVOFF_AUTOPOWER+NVLEN_AUTOPOWER)
#define NVLEN_MONITOR		1

/* controls reboot after panic */
#define NVOFF_REBOUND		(NVOFF_MONITOR+NVLEN_MONITOR)
#define NVLEN_REBOUND		1
#define REBOUND_DEFAULT		""

#if IP26 || IP28
/* selects from multiple boot tunes on power indigo2 */
#define NVOFF_BOOTTUNE		(NVOFF_REBOUND+NVLEN_REBOUND)
#define NVLEN_BOOTTUNE		1
#endif

/* add more read/write variables here ^ */
#if IP26 || IP28
#define NVOFF_FREE	(NVOFF_BOOTTUNE+NVLEN_BOOTTUNE)
#define NVLEN_FREE	7
#else
#define NVOFF_FREE	(NVOFF_REBOUND+NVLEN_REBOUND)
#define NVLEN_FREE	8
#endif

#define NVOFF_LAST	(NVOFF_FREE+NVLEN_FREE)

/* total length of used read/write nv ram */
#define NVRW_TOTAL		(NVOFF_LAST)

/*
 * EVERYTHING PAST THIS POINT WILL BE WRITE PROTECTED
 *
 * The write protected nvram is allocated in reverse order starting
 * at the end of nvram.
 */

/*
 * ethernet physical address
 */
#define	NVLEN_ENET		6
#define NVOFF_ENET		(NVLEN_MAX-NVLEN_ENET)

/* add more write protected variables here ^ */

/* low address of write protected nvram */
#define NVFUSE_START		NVOFF_ENET

#if (NVOFF_LAST != NVFUSE_START)
# include "error -- non-volatile ram table overflow/underflow"
#endif

/* Control opcode for nonvolatile ram on IP22  */
#define SER_READ	0xc000		/* serial memory read */
#define SER_WEN		0x9800		/* write enable before prog modes */
#define SER_WRITE	0xa000		/* serial memory write */
#define SER_WRALL	0x8800		/* write all registers */
#define SER_WDS		0x8000		/* disable all programming */
#define	SER_PRREAD	0xc000		/* read protect register */
#define	SER_PREN	0x9800		/* enable protect register mode */
#define	SER_PRCLEAR	0xffff		/* clear protect register */
#define	SER_PRWRITE	0xa000		/* write protect register */
#define	SER_PRDS	0x8000		/* disable protect register, forever */

#ifdef LANGUAGE_C
#define MAXNVNAMELEN	32
/* format used to store nvram table information
 */
struct nvram_entry {
    char nt_name[MAXNVNAMELEN];		/* string name of entry */
    char *nt_value;			/* PROM: string for default value */
					/* UNIX: current value of entry */
    int nt_nvaddr;			/* offset to entry in nvram */
    int nt_nvlen;			/* length of entry in nvram */
};
#if _K64PROM32
struct nvram_entry32 {
    char nt_name[MAXNVNAMELEN];		/* string name of entry */
    int nt_value;			/* PROM: string for default value */
					/* UNIX: current value of entry */
    int nt_nvaddr;			/* offset to entry in nvram */
    int nt_nvlen;			/* length of entry in nvram */
};
#endif /* _K64PROM32 */
#endif /* LANGUAGE_C */
#endif /* __SYS_IP22NVRAM_H__ */
