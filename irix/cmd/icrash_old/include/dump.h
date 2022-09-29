
#ifndef __SYS_DUMP_H__
#define __SYS_DUMP_H__
/*
 * Constants and structures for kernel crash dumping stuff
 * There's a complete description of the how this works in
 * cmd/savecore/newdump.txt.
 *
 * $Revision: 1.1 $
 */

#ifdef	_KERNEL
#define	DUMP_OPEN	1		/* initialize device */
#define	DUMP_WRITE	2		/* write some data */
#define	DUMP_CLOSE	3		/* close the device */
#endif

/*
 * Values for led's during a crash
 */
#define	LED_PATTERN_BADVA	2	/* kernel bad virtual address */
#define	LED_PATTERN_FAULT	3	/* random kernel fault */
#define	LED_PATTERN_BOTCH	4	/* nofault botch */
#define	LED_PATTERN_PKG		5	/* bad package linkage */

#define DUMP_STACK_SIZE		_PAGESZ	/* Size of special dump stack */

/*
 * The following #defines cannot be changed without incrementing the
 * DUMP_VERSION and modifying all programns that manipulate compressed
 * vmcore dumps.
 */
#define DUMP_PANIC_LEN		80
#define DUMP_PUTBUF_LEN		2048

#if _LANGUAGE_C

/*
 * Selective/compressed dumps consist of a header (defined below) followed
 * by some number of pairs of directory entries and data blocks.
 * The directory entries are of type "dump_dir_ent_t" and the amount
 * of data to follow depends on the "length" field of the directory entry.
 */


/*
 * Header for selective/compressed VM core dump-
 * This contains both information about the dump, and about the machine
 * that created it.
 */
typedef struct dump_hdr_s {
	/*
	 * A magic number indicating a valid dump.
	 */
	unsigned long long	dmp_magic;
	/*
	 * Version of this dump header.  Future headers may add fields.
	 * We can tell which fileds are valid by this version number.
	 */
	unsigned int		dmp_version;
	/*
	 * Page size of the kernel that created the dump.
	 */
	unsigned int		dmp_pg_sz;
	/*
	 * The size of physical memory on the machine that created the
	 * dump (in megabytes).
	 */
	unsigned int		dmp_physmem_sz;
	/*
	 * The time at which the crash occurred.
	 */
	time_t			dmp_crash_time;
	/* The space available for dumping.  (Size of the dumpdev minus
	 *				      dumplo)
	 */
	unsigned int		dmp_size;
	/*
	 * The space actually occupied by this dump.  This is rewritten
	 * after the dump is complete (in pages).
	 */
	unsigned int		dmp_pages;
	/*
	 * The size of this header.  We can seek to this offset to get the start
	 * of data even if we don't understand this version of the header
	 * structure.
	 */
	unsigned int		dmp_hdr_sz;

	/* Offset == 36 */
	/* The uname offset is used by "file." See /etc/magic. */
	/*
	 * This field is used to store the full "uname" of the kernel
	 * creating this dump.
	 */
	char			dmp_uname[257 * 5];
	/*
	 * dmp_panic_str is the panic string printed by the crashing kernel.
	 */
	char			dmp_panic_str[DUMP_PANIC_LEN];
	/*
	 * This array is the kernel's "putbuf" which contains its last
	 * console output.
	 */
	char			dmp_putbuf[DUMP_PUTBUF_LEN];
	/*
	 * dmp_putbufndx is equivalent to the kernel's putbufndx.  It's the
	 * index of the next character asvailble in the putbuf.
	 */
	unsigned int		dmp_putbufndx;
	/*
	 * putbufsz is the size of the _kernel's_ putbuf.  Our array is
	 * always DUMP_PUTBUF_LEN.
	 */
	int			dmp_putbufsz;
	/*
	 * dmp_physmem_start is the address of the beginning of physical
	 * memory on the machine that created the dump.  IP20/IP22/IP26
	 * have physical memory that starts at 80M.  We don't want to
	 * create 80M of zeroes in an uncompressed file so the start of
	 * an expanded vmcore is this address.
	 */
	/* NOTE - This field is only valid in header version 1 and later.
	 */
	unsigned int		dmp_physmem_start;
	/*
	 * icrash needs to know the master node ID so it can bootstrap
	 * itself and find the icrashdefs structure in mapped kernels.
	 */
	nasid_t			dmp_master_nasid;
	/* Offset to beginning of the putbuf */
	unsigned int		dmp_putbuf_offset;
	/* Size of the putbuf */
	unsigned int		dmp_putbuf_size;
	/* Offset to beginning of the errbuf */
	unsigned int		dmp_errbuf_offset;
	/* Size of errbuf */
	unsigned int		dmp_errbuf_size;
} dump_hdr_t;

/*
 * Format of directory entry at the beginning of each block
 */
typedef struct dump_dir_ent_s {
	/* The high word of the block's starting physical address.
	 */
	uint addr_hi;
	/* The low word of the block's starting physical address.
	 */
	uint addr_lo;
	/* The compressed length of the block.
	 */
	int  length;
} dump_dir_ent_t;

#ifdef _KERNEL
extern void dumpsys(void);
#endif

#endif /* _LANGUAGE_C */

/*
 * Magic number at the beginning of such a file
 */
#define DUMP_MAGIC	0x4372736844756d70ULL

/*
 * Current dump file format version number
 */
#define DUMP_VERSION	3
/***************************************************************************
 * Version 1 adds the start of physical memory to the header.
 *
 ***************************************************************************/

/*
 * Flags to be ored into the address which is always page-aligned
 */

/*
 *  Compression types:
 */
#define DUMP_RAW		0x000	/* Uncompressed block. */
#define DUMP_RLE		0x001	/* Block compressed with RLE. */
#define DUMP_COMPMASK		0x003	/* Mask for compression type. */

/*
 * Block types.
 */
#define DUMP_DIRECT		0x080	/* Direct mapped block. */
#define DUMP_SELECTED		0x040	/* Block marked as important to dump */
#define DUMP_INUSE		0x020	/* Block marked as in-use. */
#define DUMP_FREE		0x010	/* Block marked as free. */
#define DUMP_TYPEMASK		0x0f0	/* Mask for block type. */
#define DUMP_TYPESHIFT		4	/* Amount to shift block type
					 * to create an array index.
					 */

/*
 * Miscelleneous flags.
 */
#define DUMP_END		0x800	/* This is the end of the dump. */

#define DUMP_BLOCKSIZE	NBPP		/* Current DUMP BLOCKSIZE. */

/* Flags needed by dbx to determine how the kernel was compiled.
 * 	These are stored in the kernel's initialized data in a
 *	variable called "dumpkflags"
 */

/* How kernel code is running: */
#define	DUMP_K32	0
#define DUMP_K64	1

/* How user code is running: */
#define	DUMP_U32	0
#define DUMP_U64	2

#endif	/* __SYS_DUMP_H__ */
