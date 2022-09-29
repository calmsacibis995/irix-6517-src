/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/elog.h	10.3"*/
#ident	"$Revision: 3.9 $"
/*
 * "True" major device numbers. These correspond
 * to standard positions in the configuration
 * table, but are used for error logging
 * purposes only.
 */

#define CNTL	1
#define SYS	2
#define CAC	3
#define PF	4

/*
 * IO statistics are kept for each physical unit of each
 * block device (within the driver). Primary purpose is
 * to establish a guesstimate of error rates during
 * error logging.
 */

struct iostat {
	unsigned int	io_ops;	/* number of read/writes */
	unsigned int	io_misc;/* number of "other" operations */
	unsigned int	io_qcnt;/* number of jobs assigned to drive */
	ushort io_unlog;	/* number of unlogged errors */
};

/*
 * structure for system accounting
 */
struct iotime {
	struct iostat ios;
	unsigned int	io_bcnt;/* total blocks transferred */
	time_t	io_resp;	/* total block response time */
	time_t	io_act;		/* total drive active time (cumulative utilization) */
	unsigned int io_wops;  /* number of writes */
	unsigned int io_wbcnt;  /* total blocks written */
};
#define	io_cnt	ios.io_ops
#define io_qc ios.io_qcnt
/* drive utilization times can be calculated by system software as follows */

/* Average drive utilization = (io_cact/io_elapt) */
/* Average drive utilization for last interval = (io_liact/io_intv) */
