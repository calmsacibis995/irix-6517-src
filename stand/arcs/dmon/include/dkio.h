/* --------------------------------------------------- */
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/dmon/include/RCS/dkio.h,v 1.2 1998/05/14 15:01:34 jtk Exp $ */

#ifndef	_SYS_DKIO_
#define	_SYS_DKIO_	1


/*	dkio.h	6.1	83/07/29	*/
/*
 * Structures and definitions for disk io control commands
 */


/* disk io control commands */
#define _DIOC_(x)	(('d'<<8) | x)
#define DIOCFMTTRK	_DIOC_(1)	/* Format and Map */
#define DIOCVFYSEC	_DIOC_(2)	/* Verify sectors */
#define DIOCGETCTLR	_DIOC_(3)	/* Get ctlr info */
#define DIOCDIAG	_DIOC_(4)	/* Perform diag */
#define DIOCSETDP	_DIOC_(5)	/* Set devparams */
#define DIOCGETVH	_DIOC_(6)	/* Get volume header */

/* BSD compatibility for now */
#define DIOCFMTMAP	_DIOC_(1)	/* Format and Map */
#define DIOCNOECC	_DIOC_(20)	/* Disable/Enable ECC */
#define DIOCRDEFECTS	_DIOC_(21)	/* Read meadia defects */
#define DIOCINITVH	_DIOC_(22)	/* Initialize volume header */

#define DIOCRECONFIG	_DIOC_(23)	/* SETVH, *and* reconfigure drive */
#define DIOCSOFTCNT     _DIOC_(24)      /* get/set soft error count */
#define DIOCSEEK        _DIOC_(25)      /* seek disk */
#define DIOCWRTVFY      _DIOC_(26)      /* turn on/off write verify flag */
#define DIOCREMOVE      _DIOC_(27)      /* setup for disk removal */
#define DIOCDISKCACHE   _DIOC_(28)      /* enable/disable disk cache */
#define DIOCDISKGEOM    _DIOC_(29)      /* get disk geometry */

/* vdisk io control commands */
#define DIOCGETDKTAB	_DIOC_(30)	/* Get dktab */
#define DIOCADDDKTAB	_DIOC_(31)	/* Add dktab */
#define DIOCDELDKTAB	_DIOC_(32)	/* Del dktab */
#define DIOCSETATTR	_DIOC_(33)	/* Set virtual disk attributes */

#define DIOCTRKID	_DIOC_(34)	/* Read track id	*/

/* Reserved for SGI. */
#define DIOCSETVH	_DIOC_(7)	/* SGI - SETVH */
					/* Can be or'd with DIOCFORCE */
/* #define DIOCRESERVED	_DIOC_(8)	/* SGI - DRIVETYPE */
#define DIOCTEST	_DIOC_(9)	/* SGI - TEST  */
/* #define DIOCRESERVED	_DIOC_(10)	/* SGI - FORMAT */
/* #define DIOCRESERVED	_DIOC_(11)	/* SGI - SENSE */
/* #define DIOCRESERVED	_DIOC_(12)	/* SGI - SELECT */
#define DIOCRDCAP	_DIOC_(13)	/* SGI - READCAPACITY */
/* #define DIOCRESERVED	_DIOC_(14)	/* SGI - RDEFECTS */
/* #define DIOCRESERVED	_DIOC_(15)	/* SGI - unused */
/* #define DIOCRESERVED	_DIOC_(16)	/* SGI - unused */
/* #define DIOCRESERVED	_DIOC_(17)	/* SGI - unused */
/* #define DIOCRESERVED	_DIOC_(18)	/* SGI - unused */
/* #define DIOCRESERVED	_DIOC_(19)	/* SGI - unused */

#define DIOCFORCE	_DIOC_(128)	/* "FORCE" is a flag that can be or'd
					 * in with DIOCSETVH to bypass the
					 * specfs "in-use" test that would
					 * fail the SETVH with an EBUSY if
					 * a partition was already "in-use".
					 * Note: Using this bit as a "flag"
					 *	 limits the # of DIOC ioctl's
					 *	 to 127 in number.
					 */
/*
 * Added from System 5.3:
 *	Ioctls to disk drivers will pass the address to this
 *	structure which the driver handle as appropriate.
 *
 */
struct io_arg {
	int retval;
	unsigned long sectst;
	unsigned long memaddr;
	unsigned long datasz;
};

/*
 * return values returned in retval when EIO returned
 */
#define DIOC_BADSIZE	1
#define DIOC_NOTVOLHDR	2
#define DIOC_DISKBUSY	3
#define DIOC_OPERR	4
#define DIOC_EFAULT	5
#define DIOC_EINVAL	6

/*
 * driver ioctl() commands not supported
 */
#define VIOC			('V'<<8)
#define V_PREAD			(VIOC|1)	/* Physical Read */
#define V_PWRITE		(VIOC|2)	/* Physical Write */
#define V_PDREAD		(VIOC|3)	/* Read of Physical 
						 * Description Area */
#define V_PDWRITE		(VIOC|4)	/* Write of Physical 
						 * Description Area */
#define V_GETSSZ		(VIOC|5)	/* Get the sector size of media */

struct  devtable {
        int     nmajr;          /* Number of major numbers for this device */
        int     ncpermjr;       /* number of controllers per major number */
        int     ndrives;        /* number of drives per controller */
        int     *mult;          /* Array of multiple major numbers */
};

struct bootdevtbl {
        char    *name;          /* name that is typed in        */
	int	(*has_func)();	/* function to determine if module is there */
	
};

/* defines for SCSI read defects DIOCRDEFECTS */

typedef struct defect_header {
        unsigned short reserved;
        unsigned short list_size;
} DEFECT_HEADER;

typedef struct defect_entry {
        unsigned long   def_cyl         : 24;
        unsigned long   def_head        : 8;
        unsigned long   def_sect;
} DEFECT_ENTRY;

#define HEADER_DEFECTS          0
#define GROWTH_DEFECTS          1
#define PRIMARY_DEFECTS         2
#define ALL_DEFECTS             3

/* DIOCDISKGEOM structure */

typedef struct geometry_info {
        unsigned long   geom_cyl;	/* nr. of cylinders */
        unsigned short   geom_head;	/* nr. of heads */
        unsigned short   geom_spt;	/* nr. of sectors per track */
        unsigned short   geom_bps;	/* nr. of bytes per sector */
        unsigned short   geom_tpz;	/* nr. of tracks per zone */
        unsigned short   geom_aspz;	/* nr. of alternate sectors per zone */
        unsigned short   geom_atpz;	/* nr. of alternate tracks per zone */
        unsigned short   geom_atpv;	/* nr. of alternate tracks per volume */
        unsigned short   geom_ilv;	/* interleave */
        unsigned short   geom_tsf;	/* track skew factor */
        unsigned short   geom_csf;	/* cylinder skew factor */
} GEOMETRY_INFO;

#endif	_SYS_DKIO_
