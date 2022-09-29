/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			   All Rights Reserved				*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	blocks.h    archive structure information file
 *
 *  SCCS
 *
 *	@(#)blocks.h	9.11	5/11/88
 *
 *  SYNOPSIS
 *
 *	#include "config.h"
 *	#include "blocks.h"
 *
 *  DESCRIPTION
 *
 *	Contains structures and unions which define the archive format.
 *	Each block in the block buffer can be one of the following:
 *
 *		1.	Archive header block.  Contains interesting
 *			but non-essential information about the
 *			archive itself.  Essential information about
 *			the archive is duplicated in the header of
 *			of every logical block.  Thus the archive header
 *			can be corrupted or unreadable with minimal
 *			impact on bru's ability to recover archived
 *			files.
 *
 *		2.	File header block.  Contains attribute
 *			information about the archived file.
 *			Bru can recover from corruption of the
 *			file header block however some non-essential
 *			information about the file may be lost.
 *
 *		3.	File data block.  Contains essential information
 *			about the file, along with some data from the
 *			file.  If the file is not a regular file or
 *			is empty, there are no data blocks.
 *
 *		4.	Archive end marker, a null block which
 *			marks the end of the archive.
 *
 */


/*
 *
 *	The actual block structure is somewhat complex, with several
 *	named structures and a couple unions.  Basically, a block
 *	is either structured or unstructured.  Structured blocks
 *	have named data elements, unstructured blocks are used
 *	when the data is simply treated as a byte stream.
 *	The unstructured block is simply an internal device for
 *	computing checksums and such, all external blocks are
 *	always structured.
 *
 *	A structured block has two components, a header and a
 *	data section.  The data section is different for
 *	the first three block types mentioned above.
 *
 *
 *		|<- blkhdr ->|<----------- blkdata ------------>|
 *
 *		+------------+----------------------------------+
 *		|	     |	ah   (archive header)		|
 *		|   hdr	     +----------------------------------+
 *		|  	     |	fh   (file header)		|
 *		|  (header)  +----------------------------------+
 *		|	     |	fd   (file data)		|
 *		+------------+----------------------------------+
 *		|		bytes  (unstructured bytes)	|
 *		+-----------------------------------------------+
 *		|		shorts  (unstructured shorts)	|
 *		+-----------------------------------------------+
 *		|		longs  (unstructured longs)	|
 *		+-----------------------------------------------+
 *
 *		|<--------------  BLKSIZE bytes --------------->|
 *
 *	Integer values in the buffer are stored in ascii, in
 *	base hex, 4 or 8 characters per value, regardless of the internal
 *	representation of the quantities they correspond to.  This
 *	greatly simplifies conversion to/from ascii and enhances
 *	portability of archives across machines.
 *
 *	Note that a header is written in every block.  This was
 *	a design consideration which was decided in favor of reliability
 *	instead of efficiency.  Other programs like tar and cpio use
 *	data blocks which are typically 512 or 1024 bytes, to match
 *	the raw physical device blocksize.  This maximizes I/O
 *	efficiency at the expense of reliability.  By having a header
 *	in each block, with a small amount of overhead to maintain the
 *	header, we know what the block is, where it goes in the archive,
 *	what file it corresponds to, what block of the file it is, etc.
 *
 *	Since the archive format is fixed (or at least should be
 *	by the time user's start saving stuff), very few manifest
 *	constants are used in describing the fields.  In general,
 *	space was allocated rather lavishly.
 *	
 */



#include "utsname.h"

/*
 *	Block Header
 *
 *	A header is recorded in every archive block.  It contains
 *	essential information about the archive, along with information
 *	to allow bru to verify consistency in it's internal processing.
 *
 *	The structure for external format is "blkhdr", the structure
 *	for internal format is "bhdr".  A blkhdr structure is exactly
 *	256 bytes long.
 *
 */

struct blkhdr {			/* Header recorded in every block */
    char h_name[NAMESIZE];	/* Name of file this block belongs to */
    char h_chk[8];		/* Checksum for the block */
    char h_alba[8];		/* Archive logical block address */
    char h_flba[8];		/* File logical block address */
    char h_time[8];		/* Time archive created (for unique ID) */
    char h_bufsize[8];		/* Archive read/write buffer size */
    char h_msize[8];		/* Media size */
    char h_magic[4];		/* Identifies type of the block */
    char h_vol[4];		/* Physical volume number */
    char h_release[4];		/* Major release number */
    char h_level[4];		/* Minor release level number */
    char h_variant[4];		/* Bru variant */
    char h_filler[60];		/* Adjust for 256 bytes total */
};


/*
 *	Archive header
 *
 *	The first block in any archive is a header block containing
 *	auxiliary information about the archive.  This block is only
 *	recorded on the first volume, at archive logical block zero.
 */

struct ar_hdr {			/* Block data is archive info */
    char a_uid[8];		/* User id of creating user */
    char a_gid[8];		/* Group id of creating user */
    char a_name[NAMESIZE];	/* Stream archive was written to */
    char a_id[IDSIZE];		/* ID of bru creating archive */
    char a_label[BLABELSIZE];	/* Archive label given by user */
    struct my_utsname a_host;	/* Host unix info, see uname(2), 5x9 bytes */
    char a_spare2[3];		/* Pad out from 5 x 9 to 48 bytes */
};    
    

/*
 *	File header
 *
 *	Each file in the archive contains at least one block, a file
 *	header block.  It contains information about the attributes of
 *	the file.  Note that if this block is corrupted, the attributes
 *	are lost but it is still theoretically possible to recover the
 *	file's contents.
 */

struct file_hdr {		/* Block data is file info */
    char f_lname[NAMESIZE];	/* Name of file linked to */
    char f_mode[8];		/* Protection mode */
    char f_ino[8];		/* Inode number */
    char f_dev[8];		/* ID of device */
    char f_rdev[8];		/* ID of device; c & b special only */
    char f_nlink[8];		/* Number of links */
    char f_uid[8];		/* User id */
    char f_gid[8];		/* Group id */
    char f_size[8];		/* Size (compressed size if compressed) */
    char f_atime[8];		/* Time of last access */
    char f_mtime[8];		/* Time of last data modification */
    char f_ctime[8];		/* Time of last file status change */
    char f_flags[8];		/* Copy of finfo fi_flags */
    char f_fibprot[8];		/* Copy of finfo fib_Protection */
    char f_fibcomm[116];	/* Copy of finfo fib_Comment */
    char f_xsize[8];		/* Original expanded (uncompressed) size */
};


/*
 *	Data section
 *
 *	The data section of each block contains one of the elements 
 *	from the union defined here.
 *
 */

#define DATASIZE ((BLKSIZE) - sizeof (struct blkhdr))
#define LONGSIZE (sizeof (long))
#define SHORTSIZE (sizeof (short))

union blkdata {			/* Block data section */
    struct ar_hdr ah;		/* Archive header */
    struct file_hdr fh;		/* File header */
    char fd[DATASIZE];		/* File data */
};

struct sblock {			/* A structured block */
    struct blkhdr hdr;		/* The block header */
    union blkdata data;		/* The block data */
};

union blk {
    struct sblock sb;			/* A structured block */
    char bytes[BLKSIZE];		/* An unstructured block */
    short shorts[BLKSIZE/SHORTSIZE];	/* A block of shorts */
    long longs[BLKSIZE/LONGSIZE];	/* A block of longs */
};


/*
 *	The following defines simplify references to data objects
 *	in the rather complicated block structure.  They make the
 *	code easier to read and allow changes to be made in
 *	the structure without massive editing sessions.
 */

#define HD sb.hdr	/* Reference to a structured block header element */
#define AH sb.data.ah	/* Reference to an archive header data element */
#define FH sb.data.fh	/* Reference to a file header data element */
#define FD sb.data.fd	/* Reference to the file data section */
