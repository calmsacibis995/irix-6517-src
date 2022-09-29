/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_NVRAM_H__
#define __SYS_SN_NVRAM_H__

#ident "$Revision: 1.36 $"
/*
 * nvram.h -- Offset and length information for variables stored
 *      in the SN0 NVRAM. The Layout of the NVRAM in SN0 is:

        +------------------+
        |                  |
        |                  |     Dallas TOD registers
        |                  |
        +------------------+ <-- NVOFF_REVISION
        |                  |
        |                  |
        |                  |     NVRAM variables, kept in Sync
        |                  |
        |                  |
        +------------------+ <-- NVOFF_LOWFREE
        |                  |
        |                  |
        +------------------+ <-- NVOFF_SN0_HIGHFREE
        |                  |
        |                  |     Partition related info, Not in Sync
        |                  |
        +------------------+ <-- NVOFF_HIGHFREE
        |                  |
        |                  |
        |                  |
        +------------------+ <-- NVOFF_LAST
        |                  |
        |                  |     SGS TOD registers
        |                  |
        +------------------+
 */




#include <sys/SN/agent.h>       /* nic_t definition */

/*
 * Total amount of storage on the SN0 NVRAM (in bytes) 
 */
#define NVLEN_MAX		0x7ff7 

/*
 * The Dallas DS1386 part has the clock at the top of the nvram
 */
#define NVOFF_DALLAS_CLOCK	0
#define NVLEN_DALLAS_CLOCK	0xe

/*
 * Second byte is the NVRAM layout revision
 * We do not start at 0 as on the Dallas nvram part the clock
 * uses addresses 0x0 - 0xd
 */
#define NVOFF_REVISION		(NVOFF_DALLAS_CLOCK + NVLEN_DALLAS_CLOCK)
#define NVLEN_REVISION		1

/*
 * bump the CURRENT_REV number each time the nvram layout is changed
 * BE CAREFUL CHANGING THIS: The IP27 prom uses these offsets to determine
 * the name and other parameters like baud rate for the system console 
 * during early init. The IP27prom just uses the offset value for a variable
 * without doing any sanity checks. And since this prom is different 
 * from the IO6prom which does bulk of the nvram operations they both
 * will not be in sync. For this reason and also, since offsets
 * depend on each other, you should not change anything before any offset
 * used by the IP27 prom.
 *
 * The offsets used by the IP27prom are:
 * - NVOFF_CONSOLE
 */
#define NV_CURRENT_REV	 	4

/*
 * ARCS: console input and output devices are synthesized from "console".
 */
#define NVOFF_CONSOLE		(NVOFF_REVISION + NVLEN_REVISION)
#define NVLEN_CONSOLE		2

/*
 * SN0 - This needs a console hwgraph path in the prom to determine which
 *       console to use. It is of the form /hw/module/?/slot/io??
 */
#define NVOFF_CONSOLE_PATH      (NVOFF_CONSOLE + NVLEN_CONSOLE)
#define NVLEN_CONSOLE_PATH      128
#define CONSOLE_PATH_DEFAULT    "default"

/* 
 * ARCS: system partition
 */
#define NVOFF_SYSPART		(NVOFF_CONSOLE_PATH + NVLEN_CONSOLE_PATH)
#define NVLEN_SYSPART		128

/* 
 * ARCS: OS loader
 */
#define NVOFF_OSLOADER		(NVOFF_SYSPART + NVLEN_SYSPART)
#define NVLEN_OSLOADER		128

/* 
 * ARCS: OS load filename
 */
#define NVOFF_OSFILE		(NVOFF_OSLOADER + NVLEN_OSLOADER)
#define NVLEN_OSFILE		128

/* 
 * ARCS: OS load options
 */
#define NVOFF_OSOPTS		(NVOFF_OSFILE + NVLEN_OSFILE)
#define NVLEN_OSOPTS		128

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
#define NVLEN_DBGTTY		128
#define DBGTTY_PATH_DEFAULT	"/dev/tty/ioc30"

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
#define NVLEN_OSPART		128

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
#define NVLEN_ROOT		128

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
 * volume is the default audio volume for the system and is also the
 * volume at which the boot tune is played.  If volume is set to zero,
 * then the boot tune is not played.  If set to 255, then the boot tune
 * is played at full volume.
 */
#define NVOFF_VOLUME            (NVOFF_SCSIRT + NVLEN_SCSIRT)
#define NVLEN_VOLUME            3

/* base ethernet mode */
#define NVOFF_EF0MODE           (NVOFF_VOLUME + NVLEN_VOLUME)
#define NVLEN_EF0MODE           6

/*
 * scsihostid is the host adapter id for onboard scsi controllers.
 * Currently, if there is more than one controller, they will all 
 * be given the same id.
 */
#define NVOFF_SCSIHOSTID	(NVOFF_EF0MODE+NVLEN_EF0MODE)
#define NVLEN_SCSIHOSTID	2

/*
 * sgilogo adds the SGI product logo to the screen (unset for OEMs).
 * nogui surpresses gui and texport is just like a tty
 */
#define NVOFF_SGILOGO           (NVOFF_SCSIHOSTID+NVLEN_SCSIHOSTID)
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
 * SN0 needs more space to store the inventory. So the whole
 * inventory addresses have been moved further down.
 *
 * The hardware inventory stores information concerning what kind
 * of boards are installed in the SN0 system, what their current
 * state is.

#define NVOFF_INVENT_VALID	(NVOFF_NONSTOP+NVLEN_NONSTOP)
#define NVLEN_INVENT_VALID	1
#define INVENT_VALID 		0x4a	

#define NVOFF_INVENTORY		(NVOFF_INVENT_VALID+NVLEN_INVENT_VALID)
#define NVLEN_INVENTORY		512
(NVOFF_INVENTORY+NVLEN_INVENTORY)
*/

#define NVOFF_FASTMEM		(NVOFF_NONSTOP+NVLEN_NONSTOP)
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

#define NVOFF_CHECKSUM		(NVOFF_REBOUND+NVLEN_REBOUND)
#define NVLEN_CHECKSUM		1

/* ------------------- New SN0 specific stuff -------------------- */

/* Inventory table */

#define NVOFF_INVENT_VALID	(NVOFF_CHECKSUM+NVLEN_CHECKSUM)
#define NVLEN_INVENT_VALID	1
#define INVENT_VALID 		0x4a	

#define NVOFF_INVENTORY		(NVOFF_INVENT_VALID+NVLEN_INVENT_VALID)
#define NVLEN_INVENTORY		4096   /* XXX Is it OK */

/* Persistent Name Table */

#define NVOFF_PNT_VALID		(NVOFF_INVENTORY + NVLEN_INVENTORY)
#define NVLEN_PNT_VALID		1
#define PNT_VALID		0xaa

#define NVOFF_PNT		(NVOFF_PNT_VALID+NVLEN_PNT_VALID)
#define NVLEN_PNT		4096
#define VALID_PNT_NVOFF(pnt)		\
	((pnt >= NVOFF_PNT) && (pnt < (NVOFF_PNT + NVLEN_PNT)))

/* Error and FRU info */

#define NVOFF_ERROR_VALID	(NVOFF_PNT + NVLEN_PNT)
#define NVLEN_ERROR_VALID	1
#define ERROR_VALID		0xba

#define NVOFF_ERROR		(NVOFF_ERROR_VALID+NVLEN_ERROR_VALID)
#define NVLEN_ERROR		4096

#define NVOFF_G_CONSOLE_IN      (NVOFF_ERROR + NVLEN_ERROR)
#define NVLEN_G_CONSOLE_IN      128
#define G_CONSOLE_IN_DEFAULT    "default"

#define NVOFF_G_CONSOLE_OUT      (NVOFF_G_CONSOLE_IN + NVLEN_G_CONSOLE_IN)
#define NVLEN_G_CONSOLE_OUT      128
#define G_CONSOLE_OUT_DEFAULT    "default"

#define NVOFF_SCSI_TO_WAR	(NVOFF_G_CONSOLE_OUT+NVLEN_G_CONSOLE_OUT)
#define NVLEN_SCSI_TO_WAR	1          

#define NVOFF_PROBEALLSCSI      (NVOFF_SCSI_TO_WAR+NVLEN_SCSI_TO_WAR)
#define NVLEN_PROBEALLSCSI      1
#define PROBEALLSCSI_DEFAULT    "n"

#define NVOFF_PROBEWHICHSCSI    (NVOFF_PROBEALLSCSI+NVLEN_PROBEALLSCSI)
#define NVLEN_PROBEWHICHSCSI    128
/* offset 0x35c2 */

#define NVOFF_OLD_CHECKSUM      (NVOFF_PROBEWHICHSCSI+NVLEN_PROBEWHICHSCSI)
#define NVLEN_OLD_CHECKSUM      1

#define NVOFF_SAV_CONSOLE_PATH  (NVOFF_OLD_CHECKSUM+NVLEN_OLD_CHECKSUM)
#define NVLEN_SAV_CONSOLE_PATH  NVLEN_CONSOLE_PATH
#define SAV_CONSOLE_PATH_DFLT   ""

/* Reserve space in the nvram for storing the info related to the 
 * disabled io components.
 */
#define NVOFF_DISABLEDIODEVS	(NVOFF_SAV_CONSOLE_PATH+NVLEN_SAV_CONSOLE_PATH)
#define NVLEN_DISABLEDIODEVS	1+1+48+3/* Last 3 bytes indicate the end */

#define NVOFF_SCSI_TO_LL	(NVOFF_DISABLEDIODEVS+NVLEN_DISABLEDIODEVS)
#define NVLEN_SCSI_TO_LL	1

#define NVOFF_DBGNAME		(NVOFF_SCSI_TO_LL+NVLEN_SCSI_TO_LL)
#define NVLEN_DBGNAME   	128

/* Version number of the master baseio prom */
#define NVOFF_VERSION		(NVOFF_DBGNAME+NVLEN_DBGNAME)
#define NVLEN_VERSION		2

#define NVOFF_PREV_PART_ID	(NVOFF_VERSION + NVLEN_VERSION)
#define NVLEN_PREV_PART_ID	1

#define NVOFF_SWAP		(NVOFF_PREV_PART_ID + NVLEN_PREV_PART_ID)
#define NVLEN_SWAP		NVLEN_ROOT

#define NVOFF_RESTORE_ENV	(NVOFF_SWAP + NVLEN_SWAP)
#define NVLEN_RESTORE_ENV	1

#define	NVOFF_EA_CNT		(NVOFF_RESTORE_ENV + NVLEN_RESTORE_ENV)
#define	NVLEN_EA_CNT		1

#define	NVOFF_MAX_BURST		(NVOFF_EA_CNT + NVLEN_EA_CNT)
#define	NVLEN_MAX_BURST		3

#define NVOFF_STORE_PARTID	(NVOFF_MAX_BURST + NVLEN_MAX_BURST)
#define	NVLEN_STORE_PARTID	1

#define NVOFF_STORE_CONSOLE_PATH (NVOFF_STORE_PARTID + NVLEN_STORE_PARTID)
#define NVLEN_STORE_CONSOLE_PATH	128

#define NVOFF_PART_MAGIC	(NVOFF_STORE_CONSOLE_PATH + NVLEN_STORE_CONSOLE_PATH)
#define NVLEN_PART_MAGIC	1
#define PART_MAGIC		0x5a

#define NVOFF_TOD_CYCLE		(NVOFF_PART_MAGIC + NVLEN_PART_MAGIC)
#define NVLEN_TOD_CYCLE		1

/* 
 * The appropriate behavior for the prom is that if banks 0 and 1 are
 * swapped, the other pairs of banks must also be swapped.  Older versions
 * of the prom only swap banks 0 and 1, while newer versions swap all
 * banks.  This nvram variable lets the kernel know whether it is dealing
 * with old or new behavior.  See PV 669589 and translate_hub_mcreg()
 *
 * The variable will be set to 1 only if all pairs of banks are actually
 * swapped (i.e. if bank 0 is disabled).  The default value is 0.
 */ 
#define NVOFF_SWAP_ALL_BANKS    (NVOFF_TOD_CYCLE + NVLEN_TOD_CYCLE)
#define NVLEN_SWAP_ALL_BANKS    1

#define NVOFF_LOWFREE           (NVOFF_SWAP_ALL_BANKS + NVLEN_SWAP_ALL_BANKS)

/* The following is an overflow area for the PNT. It's at located at the
 * same offset as NVOFF_LOWFREE for other PROMs and should not conflict
 * with them.
 */

#define NVOFF_PNT_OVERFLOW	NVOFF_LOWFREE
/* The length is determined by the offset of NVOFF_SN0_HIGHFREE. */

/*******************************************************************
 * This is another 'boundary' in the NVRAM layout of SN0.          *
 * The space between NVOFF_SN0_HIGHFREE and NVOFF_HIGHFREE is      *
 * used to store any partitioning related info. The space          *
 * between NVOFF_LOWFREE and NVOFF_HIGHFREE is not kept in sync    *
 * with other NVRAMs in the system.                                *
 *******************************************************************/

#define NVOFF_SN0_HIGHFREE      (NVOFF_PREV_NETADDR)

#define NVLEN_PREV_NETADDR      NVLEN_NETADDR
#define NVOFF_PREV_NETADDR      (NVOFF_PREV_ROOT-NVLEN_PREV_NETADDR)

#define NVLEN_PREV_ROOT         NVLEN_ROOT
#define NVOFF_PREV_ROOT         (NVOFF_PREV_OSPART-NVLEN_PREV_ROOT)

#define NVLEN_PREV_OSPART       NVLEN_OSPART
#define NVOFF_PREV_OSPART       (NVOFF_PREV_DSKPART-NVLEN_PREV_OSPART)

#define NVLEN_PREV_DSKPART      NVLEN_SYSPART
#define NVOFF_PREV_DSKPART      (NVOFF_STORED_PART_ID-NVLEN_PREV_DSKPART)

#define NVLEN_STORED_PART_ID    1
#define NVOFF_STORED_PART_ID    (NVOFF_NVRAM_STATE-NVLEN_STORED_PART_ID)

#define NVLEN_NVRAM_STATE       4
#define NVOFF_NVRAM_STATE       (NVOFF_KEEPINSYNC-NVLEN_NVRAM_STATE)

#define NVLEN_KEEPINSYNC        1
#define NVOFF_KEEPINSYNC        (NVOFF_HIGHFREE-NVLEN_KEEPINSYNC)
#define KEEPINSYNC_DEFAULT      1

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
/*
#if NVOFF_LOWFREE >= NVOFF_HIGHFREE
*/

#if NVOFF_LOWFREE >= NVOFF_SN0_HIGHFREE
	"ERROR! Exceeded NVRAM size!"
#endif

/* For compatible code with other platforms */
#define NVOFF_ENET	-1
#define NVLEN_ENET	-1

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

/* -------------------- Persistent Name Table (PNT) ------------- */

#define NVPNTOFF_MODID	0
#define NVPNTSIZE_MODID	4
#define NVPNTOFF_NIC  	(NVPNTOFF_MODID + NVPNTSIZE_MODID)
#define NVPNTSIZE_NIC  	8
#define NVPNTOFF_PART  	(NVPNTOFF_NIC + NVPNTSIZE_NIC)
#define NVPNTSIZE_PART  	4
#define NVPNT_ENTRY_SIZE  (NVPNTSIZE_MODID + NVPNTSIZE_NIC + NVPNTSIZE_PART)

typedef struct pnt_string_s {
	char 	*s_modid ; /* Module id - 4 digits */
	char 	*s_nic   ; /* NIC string */
	char	*s_part  ; /* partition number - 4 digits */
} pnt_string_t ;

typedef struct pnt_s {
	nic_t 		nic   ; /* NIC */
	moduleid_t 	modid ; /* Module id */
	partid_t	part  ; /* partition number */
} pnt_t ;

#define NVRAM_GETNEXT_PNT(pnt)	(pnt ? (pnt + NVPNT_ENTRY_SIZE) : NVOFF_PNT) 
#define NVRAM_GET_PNT(n)	(n * NVPNT_ENTRY_SIZE + NVOFF_PNT) 

/* --------------------------------- */

/*
 * SCSI TIMEOUT war requires additional info to be
 * stored in the NVRAM along with the number of times
 * it has been reset.
 */

#ifdef NVOFF_SCSI_TO_LL
#define SCSI_TO_LL_MAGIC_SHFT	5
#define SCSI_TO_LL_MAGIC	(0x5 << SCSI_TO_LL_MAGIC_SHFT)
#define SCSI_TO_LL_REBOOT	0x1
#endif

/* ----------------------- Master/Slave NVRAM stuff --------------- */

/*
 * For SN0, we will assume that the IO6prom will keep 
 * upto a certain number of NVRAMs in sync.
 */

#define MAX_NVRAM_IN_SYNC	64
#define SLVNV_PRESENT	(nslv_nvram)	/* More than 1 nvram present */

/* mstnv_state - flag values */

#define SLVNV_OK	0x1		/* slave is consistent with master */
#define SLVNV_INSYNC    (mstnv_state & SLVNV_OK)

/* ---------------------------------------------------------------- */

/* Order in which Inventory bytes will be stored

struct invent_entry {
	unsigned char 	type ;
	unsigned char 	rev ;
	unsigned short 	diagval ;
	unsigned char	enabled ;
	unsigned char	module ;
	unsigned char	slot ;
	nic_t		nic ;
} ; */

extern void nvram_baseinit(void);

/*----------------- Disabled io component table (DICT) ---------------------*/

#define MAX_DISABLEDIO_COMPS	16	/* limit on the  # of io components
					 * that can be disabled.
					 */
/* Structure to store the <mod,slot,comp> triple for each disabled io
 * component
 */
typedef struct dict_entry_s {
	unchar		module;		/* module id of the io board */
	unchar		slot;		/* slot of the io board */
	unchar		comp;		/* component indexo the disabled 
					 * component on the io board 
					 */
} dict_entry_t;


/* Convenient interface to access the size , module , slot & comp info from the
 * nvram disable io component table 
 */

#define NVRAM_DICT_MAGIC_OFFSET	NVOFF_DISABLEDIODEVS
#define NVRAM_DICT_SIZE_OFFSET	NVRAM_DICT_MAGIC_OFFSET+1
#define NVRAM_DICT_BASE_OFFSET	NVRAM_DICT_SIZE_OFFSET+1		

#define NVRAM_DICT_MAGIC	0xda

#define nvram_dict_magic_get()						\
	get_nvreg((uint)NVRAM_DICT_MAGIC_OFFSET)

#define nvram_dict_magic_set()						\
	set_nvreg((uint)NVRAM_DICT_MAGIC_OFFSET,NVRAM_DICT_MAGIC)

#define nvram_dict_magic_check()					\
	(nvram_dict_magic_get() == NVRAM_DICT_MAGIC)

#define nvram_dict_size_get()						\
	get_nvreg(NVRAM_DICT_SIZE_OFFSET)

#define nvram_dict_size_set(size)					\
	set_nvreg(NVRAM_DICT_SIZE_OFFSET,size)	

#define nvram_dict_module_get(index)					\
	get_nvreg(NVRAM_DICT_BASE_OFFSET + index * 3)

#define nvram_dict_slot_get(index)					\
	get_nvreg(NVRAM_DICT_BASE_OFFSET + index * 3 + 1)

#define nvram_dict_comp_get(index)					\
	get_nvreg(NVRAM_DICT_BASE_OFFSET + index * 3 + 2)

#define nvram_dict_module_set(index,module)				\
	set_nvreg(NVRAM_DICT_BASE_OFFSET + index * 3, (unchar)module)

#define nvram_dict_slot_set(index,slot)					\
	set_nvreg(NVRAM_DICT_BASE_OFFSET + index * 3 + 1, (unchar)slot)

#define nvram_dict_comp_set(index,comp)					\
	set_nvreg(NVRAM_DICT_BASE_OFFSET + index * 3 + 2, (unchar)comp)


/* Store the <mod,slot,comp> triple into the nvram */
#define nvram_disable_io_component(index,mod,slot,comp)			\
	{								\
		nvram_dict_module_set(index,mod);			\
		nvram_dict_slot_set(index,slot);			\
		nvram_dict_comp_set(index,comp);			\
	}


#define	nvram_dict_invalid_value	0xff

/* Check that 0 <= index < MAX_DISABLEDIO_COMPS */			
#define valid_index(index)		(index < MAX_DISABLEDIO_COMPS)

/* Check if this is a free index */	
#define free_index_check(index)						\
	((nvram_dict_module_get(index) == nvram_dict_invalid_value) &&	\
	 (nvram_dict_slot_get(index) == nvram_dict_invalid_value)   &&	\
	 (nvram_dict_comp_get(index) == nvram_dict_invalid_value))

/* Mark this index as free */
#define free_index_set(index)						\
	{								\
		nvram_dict_module_set(index,nvram_dict_invalid_value);	\
		nvram_dict_slot_set(index,nvram_dict_invalid_value);	\
		nvram_dict_comp_set(index,nvram_dict_invalid_value);	\
	}


/* Interface to the disable io component table maintained in the nvram */
int	nvram_dict_index_get(dict_entry_t *,unchar);
int	nvram_dict_index_set(dict_entry_t *,unchar);
void	nvram_dict_index_remove(unchar index);
void	nvram_dict_init(void);
unchar	nvram_dict_size(void);
unchar 	nvram_dict_first_free_index_get(void);
unchar	nvram_dict_next_index_get(unchar);

/* Interface to read & write the prom verision for the master baseio */
void	nvram_prom_version_set(unsigned char,unsigned char);
void	nvram_prom_version_get(unsigned char *,unsigned char *);

/* Interface to read & write the ip27log for disabling cpus on reboot */
#define DISABLE_CPU_IP27LOG 		"disabled by testfpu cron job"
int nvram_enable_cpu_set(cpuid_t cpu_num, int enable);


#endif /* LANGUAGE_C */


#endif /* __SYS_SN_NVRAM_H__ */
