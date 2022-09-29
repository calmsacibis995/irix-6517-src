/*	%Q%	%I%	%M%	*/

/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

/*
 * tpd.h -- definitions for format of boot tape directories
 *
 * NOTES:
 *	- all binary values are 2's complement, big-endian regardless
 *	of target machine
 *	- checksum is calculated by first zeroing the td_cksum field, then
 *	2's complement summing all 32 bit words in the struct tp_dir and
 *	and then assigning the 2's complement of the cksum to td_cksum.
 *	Thus the checksum is verified by resumming the header and verifing
 *	the sum to be zero.
 *	- each tape record is TP_BLKSIZE bytes long, trailing bytes in
 *	in a tape block (after the end of the directory or an end of file)
 *	are unspecified (although they should be zero).
 *	- all files start on a tape block boundry, the start of a particular 
 *	file may be found by skipping backward to the beginning of the
 *	file containing the tape directory and then skipping forward
 *	tp_lbn records.
 */
#define	TP_NAMESIZE	16
#define	TP_NENTRIES	20
#define	TP_BLKSIZE	512
#define	TP_MAGIC	0xaced1234
#define TP_SMAGIC       0x3412edac


#ifdef LANGUAGE_C
/*
 * tape directory entry
 */
struct tp_entry {
	char		te_name[TP_NAMESIZE];	/* file name */
	unsigned	te_lbn;		/* tp record number, 0 is tp_dir */
	unsigned	te_nbytes;	/* file byte count */
};

/*
 * boot tape directory block
 * WARNING: must not be larger than 512 bytes!
 */
struct tp_dir {
	unsigned	td_magic;	/* identifying magic number */
	unsigned	td_cksum;	/* 32 bits 2's comp csum of tp_dir */
	unsigned	td_spare1;
	unsigned	td_spare2;
	unsigned	td_spare3;
	unsigned	td_spare4;
	unsigned	td_spare5;
	unsigned	td_spare6;
	struct tp_entry td_entry[TP_NENTRIES];	/* directory */
};

union tp_header {
	char	th_block[TP_BLKSIZE];
	struct	tp_dir th_td;
};
#endif /* LANGUAGE_C */
