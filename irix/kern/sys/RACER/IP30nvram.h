#ident "include/sys/IP30nvram.h: $Revision: 1.10 $"

/* IP30nvram.h - IP30 NVRAM offsets, etc. */

#ifndef __SYS_IP30NVRAM_H__
#define	__SYS_IP30NVRAM_H__

#if EMULATION || SABLE
/* number of bytes in the 93cs56 */
#define	NVLEN_MAX		256

#else	/* EMULATION || SABLE */

/* number of bytes in the ds1687 [first byte is for diags] */
#define	NVLEN_MAX		242

/* offset of the first nvram location into the ds1687 part */
#define	NVRAM_REG_OFFSET	0x0e

/* The 50 nvram locations that are accessible through both banks, and
 * the first is reserved for clock diagnostics
 */
#define	NVRAM_SET_0_MIN		NVRAM_REG_OFFSET
#define	NVRAM_SET_0_MAX		(NVRAM_SET_0_MIN + 50 - 1)

/* the 64 nvram locations that are accessible through bank 0 only */
#define	NVRAM_SET_1_MIN		(NVRAM_SET_0_MAX + 1)
#define	NVRAM_SET_1_MAX		(NVRAM_SET_1_MIN + 64 - 1)

/* the 128 extended nvram locations (-1 for scratch location) */
#define	NVRAM_SET_2_MIN		(NVRAM_SET_1_MAX + 1)
#define	NVRAM_SET_2_MAX		(NVRAM_SET_2_MIN + 128 - 1)

#endif	/* EMULATION || SABLE */

/* first byte should be the checksum */
#define	NVOFF_CHECKSUM		1
#define	NVLEN_CHECKSUM		1

/* second byte is the NVRAM layout revision */
#define	NVOFF_REVISION		(NVOFF_CHECKSUM + NVLEN_CHECKSUM)
#define	NVLEN_REVISION		1

/* bump the CURRENT_REV number each time the nvram layout is changed */
#define	NV_CURRENT_REV		7

/* ARCS: console input and output devices are synthesized from "console" */
#define	NVOFF_CONSOLE		(NVOFF_REVISION + NVLEN_REVISION)
#define	NVLEN_CONSOLE		2

/* color for textport background */
#define	NVOFF_PGCOLOR		(NVOFF_CONSOLE + NVLEN_CONSOLE)
#define	NVLEN_PGCOLOR		6

/* lbaud/rbaud are the initial baud rates for the duart */
#define	NVOFF_LBAUD		(NVOFF_PGCOLOR + NVLEN_PGCOLOR)
#define	NVLEN_LBAUD		8

/* indicate whether we are running as a diskless station or not */
#define	NVOFF_DISKLESS		(NVOFF_LBAUD + NVLEN_LBAUD)
#define	NVLEN_DISKLESS		1

/* ARCS: autoload - 'Y' or 'N' */
#define	NVOFF_AUTOLOAD		(NVOFF_DISKLESS + NVLEN_DISKLESS)
#define	NVLEN_AUTOLOAD		1

/* diagmode controls diagnostic report level/ide actions.  */
#define	NVOFF_DIAGMODE		(NVOFF_AUTOLOAD + NVLEN_AUTOLOAD)
#define	NVLEN_DIAGMODE		8

/*
 * netaddr is used by network software to determine the internet
 * address.
 */
#define	NVOFF_NETADDR		(NVOFF_DIAGMODE + NVLEN_DIAGMODE)
#define	NVLEN_NETADDR		20

/* nokbd indicates if the system is allowed to boot without a keyboard */
#define	NVOFF_NOKBD		(NVOFF_NETADDR + NVLEN_NETADDR)
#define	NVLEN_NOKBD		1

/*
 * lang is the language desired (format: en_USA)
 * keybd overrides the key map for the returned keyboad
 */
#define	NVOFF_KEYBD		(NVOFF_NOKBD + NVLEN_NOKBD)
#define	NVLEN_KEYBD		5

#define	PASSWD_LEN		8
/*
 * password_key is an encrypted key for protecting manual mode
 * netpasswd_key ditto for network access
 */
#define	NVOFF_PASSWD_KEY	(NVOFF_KEYBD + NVLEN_KEYBD)
#define	NVLEN_PASSWD_KEY  	(2 * PASSWD_LEN + 1)

/*
 * volume is the default audio volume for the system and is also the
 * volume at which the boot tune is played.  If volume is set to zero,
 * then the boot tune is not played.  If set to 255, then the boot tune
 * is played at full volume.
*/
#define	NVOFF_VOLUME		(NVOFF_PASSWD_KEY + NVLEN_PASSWD_KEY)
#define	NVLEN_VOLUME		3

/*
 * sgilogo adds the SGI product logo to the screen (unset for OEMs).
 * nogui surpresses gui and texport is just like a tty
 */
#define	NVOFF_SGILOGO		(NVOFF_VOLUME + NVLEN_VOLUME)
#define	NVLEN_SGILOGO		1

/* debugger port baud rate */
#define	NVOFF_RBAUD		(NVOFF_SGILOGO + NVLEN_SGILOGO)
#define	NVLEN_RBAUD		8

/* controls recovery from AC fail */
#define	NVOFF_BOOTMASTER	(NVOFF_RBAUD + NVLEN_RBAUD)
#define	NVLEN_BOOTMASTER	1

/* overrides the monitor id */
#define	NVOFF_MONITOR		(NVOFF_BOOTMASTER + NVLEN_BOOTMASTER)
#define	NVLEN_MONITOR		8

/* controls reboot after panic */
#define	NVOFF_REBOUND		(NVOFF_MONITOR + NVLEN_MONITOR)
#define	NVLEN_REBOUND		1
#define	REBOUND_DEFAULT		""

/* selects from multiple boot tunes */
#define	NVOFF_BOOTTUNE		(NVOFF_REBOUND + NVLEN_REBOUND)
#define	NVLEN_BOOTTUNE		1

/* base ethernet mode */

#define NVOFF_EF0MODE		(NVOFF_BOOTTUNE + NVLEN_BOOTTUNE)
#define NVLEN_EF0MODE		6


/* which cpu to boot with */
#define NVOFF_CPUDISABLE	(NVOFF_EF0MODE + NVLEN_EF0MODE)
#define NVLEN_CPUDISABLE	2
#define CPU_DISABLE_MAGIC	0xB210
#define CPU_DISABLE_INVALID	0x0020
#define CPU_DISABLE_MAGIC_MASK	0xfff0
#define CPU_DISABLE_MASK	0x000f

/* does the supply turn on automatically after an AC fail or not? */
#define NVOFF_AUTOPOWER		(NVOFF_CPUDISABLE + NVLEN_CPUDISABLE)
#define NVLEN_AUTOPOWER		1

/*
 * following areas for NFLASHEDRP, NFLASHEDFP, NFLASHEDPDS,
 * UPTIME, and CPUDISABLE will be directly access and *not* through
 * env variables (i.e. will not be in env_table).
 */
/* number of flashes for 3 flash areas */
/* Rprom */
#define NVOFF_NFLASHEDRP	(NVOFF_AUTOPOWER + NVLEN_AUTOPOWER)
#define NVLEN_NFLASHEDRP	4
/* Fprom */
#define NVOFF_NFLASHEDFP	(NVOFF_NFLASHEDRP + NVLEN_NFLASHEDRP)
#define NVLEN_NFLASHEDFP	4
/* PDS */
#define NVOFF_NFLASHEDPDS	(NVOFF_NFLASHEDFP + NVLEN_NFLASHEDFP)
#define NVLEN_NFLASHEDPDS	4

/* System uptime counter */
#define NVOFF_UPTIME		(NVOFF_NFLASHEDPDS + NVLEN_NFLASHEDPDS)
#define NVLEN_UPTIME		4

/* Hook to run prom text uncached */
#define NVOFF_UNCACHEDPROM	(NVOFF_UPTIME + NVLEN_UPTIME)
#define NVLEN_UNCACHEDPROM	1

/* flags to pass in to RPROM */
#define NVOFF_RPROMFLAGS	(NVOFF_UNCACHEDPROM + NVLEN_UNCACHEDPROM)
#define NVLEN_RPROMFLAGS	2

#if EMULATION || SABLE 
/* ethernet physical address */
#define	NVLEN_ENET		6
#define	NVOFF_ENET		(NVLEN_MAX - NVLEN_ENET)
#define	NVOFF_LAST		(NVOFF_REBOUND + NVLEN_REBOUND + NVLEN_ENET)
#else
#define NVOFF_FREE		(NVOFF_RPROMFLAGS + NVLEN_RPROMFLAGS)
#define NVLEN_FREE		119
#define	NVOFF_LAST		(NVOFF_FREE + NVLEN_FREE)

/* compatability with standalone nvram interfaces */
#define NVOFF_ENET		0xB210
#define NVLEN_ENET		5
#endif	/* EMULATION || SABLE */

/* variables that are stored in flash */
#define NVOFF_PDSVAR		555		/* var is in flash not dallas */
#define	NVOFF_SYSPART		NVOFF_PDSVAR	/* ARCS: system partition */
#define	NVLEN_SYSPART		1
#define	NVOFF_OSLOADER		NVOFF_PDSVAR	/* ARCS: OS loader */
#define	NVLEN_OSLOADER		1
#define	NVOFF_OSFILE		NVOFF_PDSVAR	/* ARCS: OS load filename */
#define	NVLEN_OSFILE		1
#define	NVOFF_OSOPTS		NVOFF_PDSVAR	/* ARCS: OS load options */
#define	NVLEN_OSOPTS		1
#define	NVOFF_OSPART		NVOFF_PDSVAR	/* ARCS: OS load partition */
#define	NVLEN_OSPART		1
#define NVOFF_FASTFAN		NVOFF_PDSVAR	/* increase fan speed */
#define NVLEN_FASTFAN		1

#if NVOFF_LAST != NVLEN_MAX
#include "error -- non-volatile ram table overflow/underflow"
#endif

#ifdef LANGUAGE_C
#define	MAXNVNAMELEN	32
/* format used to store nvram table information */
struct nvram_entry {
	char	nt_name[MAXNVNAMELEN];	/* string name of entry */
	char	*nt_value;		/* PROM: string for default value */
					/* UNIX: current value of entry */
	int	nt_nvaddr;		/* offset to entry in nvram */
	int	nt_nvlen;		/* length of entry in nvram */
};
#endif /* LANGUAGE_C */
#endif /* __SYS_IP30NVRAM_H__ */
