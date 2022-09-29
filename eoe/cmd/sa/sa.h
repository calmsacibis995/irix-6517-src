/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)sa:sa.h	1.16" */
#ident	"$Revision: 1.41 $"
/*	sa.h 1.16 of 5/23/85 */
/*	sa.h contains struct sa and defines variable used 
		in sadc.c and sar.c.
*/
 
#define NDEVS 96	/* Allow for 16 SCSI and 8 IPI buses w/4 drives/bus */

/* devnm is not used now since device names are embedded in the record */

/* We will define a new disk stats structure that allows the device name
 * to be embedded in the record. This means that sar doesn't need to know
 * about disk names, and fixes the bug where it got drive numbers wrong if
 * drives were assigned discontiguously (eg a drive 0 & 2 but no drive 1)
 *
 * In 6.5.5, the neodisk routines and record types were added.  The new
 * sgidio is complatible with those.  oldsgidio is identical to sgidio
 * from 6.5 to 6.5.4.
 */

#include <sys/elog.h>

#define SDIO_NAMESIZE 80
struct sgidio {
	char 		sdio_lname[SDIO_NAMESIZE];
	struct iotime	sdio_iotim;
	float		save_busy;
};
struct oldsgidio {
	char 		sdio_dname[12];
	struct iotime	sdio_iotim;
	float		save_busy;
};


#define SGIDIO_OLD_DNAME(s)	((s).sdio_dname)
#define SGIDIO_DNAME(s)		((s).sdio_lname)
#define SGIDIO_COUNT(s) 	((s).sdio_iotim.io_cnt)
#define SGIDIO_BCNT(s)		((s).sdio_iotim.io_bcnt)
#define SGIDIO_WCOUNT(s)	((s).sdio_iotim.io_wops)
#define SGIDIO_WBCNT(s)		((s).sdio_iotim.io_wbcnt)
#define SGIDIO_ACT(s)		((s).sdio_iotim.io_act)
#define SGIDIO_RESP(s)		((s).sdio_iotim.io_resp)
#define SGIDIO_QCNT(s)		((s).sdio_iotim.ios.io_qcnt)
#define SGIDIO_QCUM(s)		((s).sdio_iotim.ios.io_misc)

#define SAR_MAGIC	"Sarmagic"
#define SAR_MAGIC_LEN	8

/*
 * The following define the identifiers that sar can use when seeking
 * for each record type in the log.  They can be a maximum of SAR_MAGIC_LEN
 *
 * NEODISK was added in 6.5.5.
 */

#define SA_MAGIC        "SA"
#define SINFO_MAGIC     "SINFO"
#define MINFO_MAGIC	"MINFO"
#define DINFO_MAGIC	"DINFO"
#define CPU_MAGIC       "CPU"
#define INFO_MAGIC      "INFO"
#define NEODISK_MAGIC   "NEODISK"
#define DISK_MAGIC      "DISK"
#define GFX_MAGIC	"GFX"
#define RMINFO_MAGIC	"RMINFO"

struct rec_header {
        char    magic[SAR_MAGIC_LEN];   /* Identifies it as a sar record */
        char    id[SAR_MAGIC_LEN];      /* Identifer defined above */
	uint	recsize;		/* Size of record structure */
	uint	nrec;			/* Number of records of recsize */
};


/*	structure sa defines the data structure of system activity data file
*/
 
struct sa {
	char	magic[SAR_MAGIC_LEN];	/* To find start of record */
	int	minserve;
	int	maxserve;
	int	szinode;	/* current size of inode table  */
	int	szfile;		/* current size of file table  */
	int	szproc;		/* current size of proc table  */
	int	szlckr;		/* current size of file record lock table */
	int	mszinode;	/* maximum size of inode table  */
	int	mszfile;	/* maximum size of file table  */
	int	mszproc;	/* maximum size of proc table  */
	int	mszlckr;	/* maximum size of file record lock table */
	long	inodeovf;	/* cumulative overflows of inode table
					since boot  */
	long	fileovf;	/* cumulative overflows of file table
					since boot  */
	long	procovf;	/* cumulative overflows of proc table
					since boot  */
	long	lckrovf;	/* cumulative overflows of record lock table
					since boot  */
	time_t	ts;		/* time stamp  */

	int apstate;		/* Number of active processors */

#define	IO_OPS	0  /* number of I /O requests since boot  */
#define	IO_BCNT	1  /* number of blocks transferred since boot */
#define	IO_ACT	2  /* cumulative time in ticks when drive is active  */
#define	IO_RESP	3  /* cumulative I/O response time in ticks since boot  */
};

extern struct sa sa;

/* define for the dummy restart record */

#define CPUDUMMYTIME -300

/* define struct of initial header record */
struct infomap {
        long    bootrt;                 /* Time of system boot */
        struct  utsname uname;          /* machine/unix ID */
        int     clk_tck;                /* system hertz value */
	int	long_sz;		/* data 'long' size */
};

/* define struct overlayed on sa record for unix restart */

struct revname {
	char release[32];
	char version[32];
};

void perrexit(const char *);
