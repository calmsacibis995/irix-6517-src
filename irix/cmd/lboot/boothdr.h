/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 2.25 $"

#ifndef _BOOTHDR_H
#define _BOOTHDR_H

#define	Offset(ptr,base)	((offset) ((char*)(ptr) - (char*)(base)))
#define	POINTER(offset,base)	((char*)(base) + (offset))

#define	ROUNDUP(p)		(((long int)(p)+sizeof(int)-1) & ~(sizeof(int)-1))

typedef	unsigned short		offset;

#define	PARAMNMSZ	8	/* maximun size of a parameter name in /etc/master data base */
#define DONTCARE	0x1ff	/* lboot assigns external major number */
#define NMAJORS		512	/* number of majors */

/*
 * configuration information generated from object's master file
 */

struct	master {
	int		flag;		/* /etc/master flags */
	char		name[256];	/* master file name, '\0' terminated */
	char		prefix[15+1];	/* module prefix, '\0' terminated */
	unsigned char	sema;		/* driver semaphoring type */
	short		soft[DRV_MAXMAJORS];	/* sw module external major #s */
	short		nsoft;		/* number of major #s */
	unsigned char	ndev;		/* number of devices/controller */
	short		ndep;		/* number of dependent modules */
	short		nrtn;		/* number of routine names */
	short		naliases;	/* number of hwgrph aliases */
	short		nadmins;	/* number of admin directives */
	offset		o_depend;	/* ==> additional modules required */
	offset		o_routine;	/* ==> routines to be stubbed if module is not loaded */
	offset		o_alias;	/* ==> alias strings to match hwgrph */
	offset		o_admin;	/* ==> admin directives */
};

/*
 *	FLAG bits
 */
#define DLDREG	0x100000/* (D) load, unload, then register dyn modules */
#define NOUNLD	0x80000 /* (N) don't unload dynamically loadable modules */
#define ENET	0x40000 /* (e) ethernet device driver */
#define DYNSYM	0x20000	/* (T) dependency target (required if dynamic) */
#define DYNREG	0x10000	/* (R) pre-registered dynamic module */
#define	KOBJECT	0x8000	/* (k) this is a kernel object file */
#define	DYNAMIC	0x4000	/* (d) dynamically loadable module */
#define OLD	0x2000  /* (O) this is an Old-style driver */
#define STUB	0x1000	/* (u) this module contains stubs */
#define WBACK	0x0800	/* (w) write back cache before strategy routine */
#define FSTYP	0x0400	/* (j) file system type*/
#define	FUNDRV	0x0200	/* (f) framework/stream type device */
#define	FUNMOD	0x0100	/* (m) framework/stream module */
#define	ONCE	0x0080	/* (o) allow only one specification of device */
#define	REQADDR	0x0040	/* (a) xx_addr array must be generated */
#define	TTYS	0x0020	/* (t) cdevsw[].d_ttys == "prefix|_tty" */
#define	REQ	0x0010	/* (r) required device */
#define	BLOCK	0x0008	/* (b) block type device */
#define	CHAR	0x0004	/* (c) character type device */
#define	SOFT	0x0002	/* (s) software device driver */
#define	NOTADRV	0x0001	/* (x) not a driver; a configurable module */

#define S_NONE	0x1	/* (n) driver is semaphored, needs no locking */
#define S_MAJOR	0x2	/* (l) driver not semaphored, use major dev locking */
#define S_PROC	0x4	/* (p) driver not semaphored, shunt to master proc. */

/*
 * Dependencies: if the current module is loaded, then the following
 *               modules must also be loaded
 */
struct	depend {
	offset		name;		/* module name */
};

/*
 * Routines: if the current module is not loaded, then the following
 *           routines must be stubbed off
 */
struct	routine {
	char		id;		/*  routine type */
	offset		name;		/* ==> routine name */
};

/*
 * Aliases: strings used to match the hwgraph to determine whether
 *		a module should be loaded or registered.
 *
 * SARAH: See comments in mkboot.y. We currently parse the alias
 * strings from a master file, if they exist, and place the info
 * in the master structure, but don't use this information yet.
 *
 */
struct	alias {
	offset		str;		/* alias string */
};

/*
 *	Routine types
 */
#define RNOTHING	0	/* void rtn() { } */
#define RNULL		1		/* void rtn() { nulldev(); } */
#define RNOSYS		2		/* rtn() { return(nosys()); } */
#define RNODEV		3		/* rtn() { return(nodev()); } */
#define RTRUE		4		/* rtn() { return(1); } */
#define RFALSE		5		/* rtn() { return(0); } */
#define RFSNULL		6		/* rtn() { return(fsnull()); } */
#define RFSSTRAY	7	/* rtn() { return(fsstray()); } */
#define RNOPKG		8		/* void rtn() { return(nopkg()); } */
#define RNOREACH	9	/* void rtn() { noreach(); } */
#define RNODEVFLAG	10
#define RZERO		11

#define	XBUMP(p,what)	(union element *)((int)(p) + ((sizeof((p)->what)==ELENGTH)? \
							1+strlen((p)->what) : \
							sizeof((p)->what)))

/* device / driver administration support macros */

/*  
 *   	Format of the admin pair table entry .
 *	This table is built as the master files for each 
 *	driver are parsed.
 */
typedef struct admin_s {
	offset	admin_name;
	offset	admin_val;
} admin_t;

#define DAT_DEV_TAILSTR "\t{\"\",\"\",\"\"}};\n"			    	\
                        "\nint			dev_admin_table_size = "	\
                        "sizeof(dev_admin_table)/sizeof(dev_admin_info_t);\n\n"	
#define DAT_DEV_TAILLEN  (strlen(DAT_DEV_TAILSTR))
#define DAT_DRV_TAILSTR "\t{\"\",\"\",\"\"}};\n"			    	\
                        "\nint			drv_admin_table_size = "	\
                        "sizeof(drv_admin_table)/sizeof(dev_admin_info_t);\n\n"	
#define DAT_DRV_TAILLEN  (strlen(DAT_DEV_TAILSTR))

#endif /* !_BOOTHDR_H */
