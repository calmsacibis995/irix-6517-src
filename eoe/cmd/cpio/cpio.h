/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1990 MIPS Computer Systems, Inc.            |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 52.227-7013.   |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Drive                                |
 * |         Sunnyvale, CA 94086                               |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/eoe/cmd/cpio/RCS/cpio.h,v 1.7 1997/04/22 05:47:49 olson Exp $ */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Portions Copyright (c) 1988, Sun Microsystems, Inc.	*/
/*	All Rights Reserved.					*/


/* Option Character keys (OC#), where '#' is the option character specified. */

#define	OCa	0x1
#define	OCb	0x2
#define	OCc	0x4
#define	OCd	0x8
#define	OCf	0x10
#define	OCi	0x20
#define	OCk	0x40
#define	OCl	0x80
#define	OCm	0x100
#define	OCo	0x200
#define	OCp	0x400
#define	OCr	0x800
#define	OCs	0x1000
#define	OCt	0x2000
#define	OCu	0x4000
#define	OCv	0x8000
#define	OCA	0x10000
#define	OCB	0x20000
#define	OCC	0x40000
#define	OCE	0x80000
#define	OCH	0x100000
#define	OCI	0x200000
#define	OCL	0x400000
#define	OCM	0x800000
#define	OCO	0x1000000
#define	OCR	0x2000000
#define	OCS	0x4000000
#define	OCV	0x8000000
#define	OC6	0x10000000
#define	BSM	0x20000000

/* Invalid option masks for each action option (-i, -o or -p). */

#define INV_MSK4i	( OCo | OCp | OCA | OCL | OCO | OCa )

#define INV_MSK4o	( OCi | OCp | OCE | OCI | OCR | OCb \
			| OCd | OCf | OCk | OCl | OCm | OCr \
			| OCs | OCt | OCu | OCS | OC6 )

#define INV_MSK4p	( OCo | OCi | OCE | OCH | OCI | OCO \
			| OCb | OCc | OCf | OCk | OCo | OCr \
			| OCt | OCA | OCs | OCS | OC6 )

/* Header types */

#define NONE	0	/* No header value verified */
#define	BIN	1	/* Binary */
#define	CHR	2	/* ASCII character (-c) */
#define	ASC	3	/* ASCII with expanded maj/min numbers */
#define	CRC	4	/* CRC with expanded maj/min numbers */
#define	TARTYP	5	/* Tar or USTAR */
#define	SEC	6	/* Secure system */

/* Differentiate between TAR and USTAR */

#define TAR	7	/* Regular tar */
#define	USTAR	8	/* IEEE data interchange standard */

/* HDRSZ: header size minus filename field length */

#define HDRSZ (Hdr.h_name - (char *)&Hdr)

/*
 * IDENT: Determine if two stat() structures represent identical files.
 * Assumes that if the device and inode are the same the files are
 * identical (prevents the archive file from appearing in the archive).
 */

#define IDENT(a, b) ((a.st_ino == b.st_ino && a.st_dev == b.st_dev) ? 1 : 0)

/*
 * FLUSH: Determine if enough space remains in the buffer to hold
 * cnt bytes, if not, call bflush() to flush the buffer to the archive.
 */

#define FLUSH(cnt) if ((Buffr.b_end_p - Buffr.b_in_p) < cnt) bflush()

/*
 * FILL: Determine if enough bytes remain in the buffer to meet current needs,
 * if not, call rstbuf() to reset and refill the buffer from the archive.
 */

#define FILL(cnt) while (Buffr.b_cnt < cnt) rstbuf()

/*
 * VERBOSE: If x is non-zero, call verbose().
 */

#define VERBOSE(x, name) if (x) verbose(name)

/*
 * BLKORCHR: Used to check if the file type is a block or character device.
 */
#define BLKORCHR(x) ( ((x & S_IFMT) == S_IFBLK) || ((x & S_IFMT) == S_IFCHR) )

/*
 * DEV32TO16: Used to reduce a 32 bit device field into 16 bits - you can
 * lose data this way.
 */
#define DEV32TO16(x) (((x >> 10) & 0x7f00) | (x & 0x00ff))

/*
 * OVER_FLOW: Should be used before DEV32TO16 to check if the 32 bit field
 * will over flow a 16 bit field.
 */
#define OVER_FLOW(x) ( ((x & MAXMIN) > OMAXMIN) || ((x >> NBITSMINOR) > OMAXMAJ) )

/*
 * cpioM*: Used to covert major/minor numbers.
 */
#define	cpioMAJOR(x)	(int)(((unsigned) x >> 8) & 0x7F)
#define	cpioMINOR(x)	(int)(x & 0xFF)

/*
 * FORMAT: Date time formats
 * b - abbreviated month name
 * e - day of month (1 - 31)
 * H - hour (00 - 23)
 * M - minute (00 - 59)
 * Y - year as ccyy
 */

#define	FORMAT	"%b %e %H:%M %Y"
#define FORMATID ":31"

#define INPUT	0	/* -i mode (used for chgreel() */
#define OUTPUT	1	/* -o mode (used for chgreel() */
#define APATH	PATH_MAX /* maximum ASC or CRC header path length */
#define CPATH	256	/* maximum -c and binary path length */
#define BUFSZ	512	/* default buffer size for archive I/O */
#define CPIOBSZ	131072	/* buffer size for file system I/O */
#define LNK_INC	500	/* link allocation increment */
#define MX_BUFS	10	/* max. number of buffers to allocate */

#define F_SKIP	0	/* an object did not match the patterns */
#define F_LINK	1	/* linked file */
#define F_EXTR	2	/* extract non-linked object that matched patterns */

#define MX_SEEKS	10	/* max. number of lseek attempts after error */
#define	SEEK_ABS	0	/* lseek absolute */
#define SEEK_REL	1	/* lseek relative */

/*
 * xxx_CNT represents the number of sscanf items that will be matched
 * if the sscanf to read a header is successful.  If sscanf returns a number
 * that is not equal to this, an error occured (which indicates that this
 * is not a valid header of the type assumed.
 */

#define ASC_CNT	14	/* ASC and CRC headers */
#define CHR_CNT	11	/* CHR header */

/* These defines determine the severity of the message sent to the user. */

#define ERR	1	/* Error message (warning) - not fatal */
#define EXT	2	/* Error message - fatal, causes exit */
#define ERRN	3	/* Error message with errno (warning) - not fatal */
#define EXTN	4	/* Error message with errno - fatal, causes exit */
#define POST	5	/* Information message, not an error */
#define EPOST	6	/* Information message to stderr */
#define WARN    7       /* Warning to stderr */

#define SIXTH	060000	/* UNIX 6th edition files */

#define	P_SKIP	0	/* File should be skipped */
#define P_PROC	1	/* File should be processed */
#define P_CMP	2	/* File should be compared */

#define U_KEEP	0	/* Keep the existing version of a file (not -u) */
#define U_OVER	1	/* Overwrite the existing version of a file (-u) */

/*
 * _20K: Allocate the maximum of (20K or (MX_BUFS * Bufsize)) bytes 
 * for the main I/O buffer.  Therefore if a user specifies a small buffer
 * size, they still get decent performance due to the buffering strategy.
 */

#define _20K	20480	

#define HALFWD	1	/* Pad headers/data to halfword boundaries */
#define FULLWD	3	/* Pad headers/data to word boundaries */
#define FULLBK	511	/* Pad headers/data to 512 byte boundaries */

/*
 * Added for xfs: Block size for large file transfers
 */

#define BIGBLOCK ((long long)1<<((sizeof(long)*CHAR_BIT)-1))

/* block map structure for holey files when -W option is used.
 An array of these (one per struct returned by the F_GETBMAP fcntl on
 holey files+1) is written at the start of a file that is a holey
 file.  Holey files in the archive are marked by being as a regular
 file, but a non-zero st_rdev.  The count of map structs is in the count
 field of the first holemap struct; offset meaningless for the first struct.
 Use of the -W option will cause other
 versions of cpio to get errors when holey files are encountered.
 The values are in bytes (but will always be rounded to multipes of
 512 bytes because that's what F_GETBMAP returns).  holes are marked
 by count < 0, for -count bytes.
*/
struct holemap {
	off64_t offset;
	off64_t count;
};
