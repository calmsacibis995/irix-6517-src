#ident "$Revision: 1.59 $"

/*
 * EFS version of mkfs.
 */

#include "efs.h"
#include <sys/dkio.h>
#include <sys/stat.h>
#include <diskinfo.h>
#include <mountinfo.h>
#include <ustat.h>

#define GOOD_INODE_TOTAL(b) (b / 10 + (b > 20000 ? 1000 : b / 20))

char	*progname;		/* name of this program */

/* user supplied information */
char	*special;		/* name of special device */
char 	*rawpath;		/* equivalent raw device (may be the same) */
long	fs_blocks;		/* total blocks in filesystem */
long 	dev_blocks;		/* total blocks on device */
efs_ino_t fs_isize = 0;		/* total minimum inodes in partition */
short	fs_sectors;		/* # of sectors per track */
int	user_inodes = 0;

/* internally supplied information */
long	cg_align;		/* alignment for cylinder groups */
long	i_align;		/* alignment for inodes */

/* random variables */
long	firstcg;		/* block offset to first cg */
long	cg_fsize = 0;		/* data size for a cylinder group */
long	cg_isize;		/* inode size for a cylinder group */
char	is_ittybitty;		/* non-zero for itty bitty filesystems */
int	fs_fd;			/* fd to read/write on for special */
long	ncg;			/* number of cylinder groups */
struct	efs *fs;		/* pointer to sblock.fs */
char	*bitmap;		/* base of incore copy of bitmap */
long	bitmap_blocks;		/* number of bb's in bitmap */
long	bitmap_bytes;		/* number of bytes in bitmap (exact) */
char	*proto;			/* name of proto file */
char	*charp;
FILE	*fproto;		/* FILE proto is open on during reading */
char	string[1000];		/* temp buffer for parsing proto */
int 	end_blocks;		/* blocks used by replsb at end */
int 	start_blocks;		/* blocks used by boot, superblock & bitmap 
				 * (rounded up to a cg_align boundary) */
int	cyl_align = 0;		/* in auto mode, align inodes and data to
				 * cylinder boundaries */

/*
 * This union declares a single BB, assuming that the minimum for raw i/o
 * is at least a BB.
 */
union {
	struct	efs fs;
	char	block[BBTOB(BTOBB(sizeof(struct efs)))];
} sblock;

int quiet = 0;
int interact = 0;
int use_writeb = 0;
int short_form = 0;
int recover_option = 0;

static void ask_confirm(void);
static void be_verbose(void);
static void blather(void);
static void buildfs(void);
static void busyout(efs_daddr_t bn, int len);
static int c_writeb(int fd, char *buf, int block, int nblocks);
static void cgsizepick(int blocks, int iblocks, int align, long *addr_ncg,
	long *addr_cg_fsize, long *addr_cg_isize);
static void clear_bootblock(void);
static void clear_inodes(void);
static void clear_other_sb(void);
static void efs_newregfile(efs_ino_t ino, char *name);
static void efs_putdev(dev_t dev, union di_addr *di);
       void error(void);
static struct extent *findex(efs_daddr_t block, int nblocks);
static int getch(void);
static void getdevinfo(void);
static long getnum(void);
static void getstr(void);
static int good_cg_size(int blocks, int iblocks, int align);
static void init_bitmap(void);
static void init_sb(void);
static int ismounted(char *name);
static void parseproto(efs_ino_t parent, efs_ino_t child);
static void userr(void);
static void zap_sb(char *name);

static int
good_cg_size(int blocks, int iblocks, int align)
{
	int	size;

	size = (blocks / 100) + 20000;
	if (size < ((blocks + iblocks - 1) / iblocks) * align)
		size = ((blocks + iblocks - 1) / iblocks) * align;
	return size;
}

/* Given an (extended) dev_t, put it into the on-disk
 * union. If its components are within the range understood by earlier
 * kernels, use the old format; if not, put -1 in the old field &
 * store it in the new one. Duplicates logic in fs/efs/efs_inode.c
 *
 * XXXdh mkfs should really use the efs_putdev() in efs/cmd/nlib/libefs.a,
 * but that is missing several functions needed by the proto file stuff,
 * so until those are moved into nlib & tested, we have to use the old 
 * lib/libefs.a
 */

static void
efs_putdev(dev_t dev, union di_addr *di)
{
	major_t maj;
	minor_t min;

	if ( ((maj = (dev >> L_BITSMINOR)) > O_MAXMAJ) ||
	     ((min = (dev & L_MAXMIN)) > O_MAXMIN)) {
		di->di_dev.odev = -1;
		di->di_dev.ndev = dev;
	}
	else
		di->di_dev.odev = (o_dev_t)((maj << O_BITSMINOR) | min);
}

int
main(int argc, char **argv)
{
	int cgblocks;
	int iblocks;
	int i, nargs;
	char **ap;
	long long ldevsize;

	progname = argv[0];

	/*	The (-q) option is not really intended to be documented or used
		by users.  It is here for use of programs like the
		vadmin disk tool.  Olson, 10/88
		Another change (daveh 10/26/88): the default is now to just
		go ahead. If you want confirmation, give the -i flag.
		(Note: ALL - flags MUST come before any other arguments,
		for backward compatibility with any scripts which use the
		long forms).
	*/

	nargs = (argc > 4) ? 4 : argc;
	ap = argv;
	for (i = 1; i < nargs; i++)
	{
		if (ap[i][0] != '-') break;
		switch (ap[i][1])
		{
		case 'q' : quiet++;
			   break;

		case 'i' : interact++;
			   break;

		case 'r' : recover_option++;
			   break;

		case 'a' : cyl_align++;
			   break;

		case 'n' : i++;
			   user_inodes = atoi(ap[i]);
			   argc--;
			   argv++;
			   break;

		default  : userr();
		}

		argc--;
		argv++;
	}

	/* If interactive use is specified, clobber 'quiet' so fs params
	   get printed for the user. (-q -i is pretty stupid, but just
	   in case...)
	   */

	if (interact) quiet = 0;

	switch (argc) {
	  case 3:			/* short form with proto */
			proto = argv[2];
		/* FALL THROUGH */
	  case 2:			/* short form without proto */
		special = argv[1];
		short_form = 1;
		getdevinfo();
		break;

	/* Now long forms. Note that the 'heads' parameter
	 * is in fact never used. However, it is left in the argument
	 * format as a dummy for compatibility with scripts which use
	 * the long specifications.
	 */

	  case 10:			/* long form with proto */
		proto = argv[9];
		/* FALL THROUGH */
	  case 9:			/* long form without proto. */ 
		special = argv[1];
		fs_blocks = atoi(argv[2]);
		fs_isize = atoi(argv[3]);
						/* argv[4] not used */
		fs_sectors = atoi(argv[5]);
		cg_fsize = atoi(argv[6]);
		cg_align = atoi(argv[7]);
		i_align = atoi(argv[8]);

	/* protect user against himself. sigh... */
	
		if ((fs_blocks == 0)  ||
	    	    (fs_isize == 0)   ||
	    	    (fs_sectors == 0) ||
	    	    (cg_fsize == 0)   ||
	    	    (cg_align == 0)   ||
	    	    (i_align == 0)) 
		{
			fprintf(stderr,"%s: Illegal zero numeric parameter.\n",
				progname);
			exit(1);
		}
		break;
	  default: userr();
	}

	/* Initial check: make sure the specified special file does not
	 * contain a mounted filesystem or that this partition overlaps
	 * a mounted partition.
	 */

	if (ismounted(special))
	    exit(1);

	/* If not short_form, try to find the real device size; sanity
	 * check the given fs_blocks parameter against it. */

	if (!short_form)
	{
		if ((ldevsize = findsize(special)) < 0LL)
			dev_blocks = 0L;
		else if (ldevsize > 0x7fffffffLL)
			dev_blocks = 0x7fffffffL;
		else
			dev_blocks = (long)ldevsize;
		if (dev_blocks && (fs_blocks > dev_blocks))
		{
			fprintf(stderr,"%s: %s does not contain enough space for %d blocks.\n",
			progname, special, fs_blocks);
			exit(1);
		}
	}


	/*
	 * Setup filesystem construction stuff.  If a proto file is being
	 * used, and the filesystem has no label, then use info from proto
	 * file.  Otherwise, label info takes precedence (for robustness)
	 * If an override is needed, the long form can always be used.
	 * NOTE: for security reasons, default to 755 on root, and
	 * 700 on lost+found.
	 */
	if (!proto)
		charp = "d--755 0 0\n lost+found d--700 0 0 $ $ ";
	else {
		fproto = fopen(proto, "r");
		if (!fproto) {
			fprintf(stderr, "%s: can't open proto file %s: %s\n",
					progname, proto, strerror(errno));
			exit(1);
		}
		/*
		 * XXX do we want a way to specify all the grot above from
		 * XXX the proto file?
		 */
		getstr();			/* ignore get boot image */
		(void) getnum();		/* skip blocks */
		(void) getnum();		/* skip inodes */
	}

	/* NOTE (daveh 12/13/89): if fs_blocks is greater than the 2 gig
	 * limit, we must use the new writeb system call.
	 * This works only on a char device, so we need in that case to find
	 * the equivalent char device even if invoked on block.
	 * Also, the structure of efs extent descriptors imposes, for the
	 * moment, an 8 gig absolute upper limit. (24 bit offsets).
	 */

	if (fs_blocks > 0xffffff)
	{
		fprintf(stderr,"%s: filesystems may not exceed 8 gigabytes!\n",
			progname);
		exit(1);
	}

	if (fs_blocks > (0x7fffffff / BBSIZE))
	{
		if ((rawpath = findrawpath(special)) == NULL)
		{
			fprintf(stderr,
				"%s: can't find equivalent raw device for %s\n",
				progname, special);
			exit(1);
		}
	
	
		if ((fs_fd = open(rawpath, O_RDWR)) < 0) 
		{
			fprintf(stderr, "%s: can't open %s: %s\n",
				progname, rawpath, strerror(errno));
			exit(1);
		}
		use_writeb = 1;
	}
	else
	{
		if ((fs_fd = open(special, O_RDWR)) < 0) 
		{
			fprintf(stderr, "%s: can't open %s: %s\n",
				progname, special, strerror(errno));
			exit(1);
		}
	}

	/*
	 * Compute the number of blocks used by the bootblock, superblock
	 * & bitmap, rounded up to a cg_align boundary. The first cylinder
	 * group starts there.
	 * The replicated superblock at the end effectively consumes cg_align 
	 * blocks.
	 */
	bitmap_bytes = (fs_blocks + BITSPERBYTE - 1) / BITSPERBYTE;
	bitmap_blocks = (long)BTOBB(bitmap_bytes);
	start_blocks = 2 + bitmap_blocks;	
	if (i = (start_blocks % cg_align))
		start_blocks += (cg_align - i);
	firstcg = start_blocks;
	end_blocks = cg_align;
	cgblocks = fs_blocks - (start_blocks + end_blocks);
	iblocks = (fs_isize + EFS_INOPBB - 1) / EFS_INOPBB;

	/*
	 * If file system is itty bitty, handle it specially
	 */
	if (short_form &&
	    fs_blocks < 2 * good_cg_size(cgblocks, iblocks, cg_align))
	{
		is_ittybitty = 1;
		ncg = 1;
		firstcg = start_blocks = 2 + bitmap_blocks;
		end_blocks = 1;
		cg_fsize = fs_blocks - (start_blocks + end_blocks);
		cg_align = i_align = 1;
		if (cg_fsize <= 0) 
		{
			fprintf(stderr,
				"%s: not enough space to make filesystem\n",
				progname);
			exit(1);
		}
		cg_isize = (long)((fs_isize + EFS_INOPBB - 1) / EFS_INOPBB);
		if (cg_isize >= cg_fsize) 
		{
			fprintf(stderr,
		"%s: invalid filesystem size - inodes larger than partition\n",
			        progname);
			exit(1);
		}
		buildfs();
		exit(0);
	}

	/*
	 * If size of cylinder groups not yet specified, compute a good
	 * size to efficiently fit the available space.
	 * Compute number of cylinder groups and blocks of inodes per cylinder 
	 * group.
	 */
	if (!cg_fsize)
	{
		if (cyl_align)
			cgsizepick(cgblocks, iblocks, cg_align,
				   &ncg, &cg_fsize, &cg_isize);
		else
		{
			cg_fsize = good_cg_size(cgblocks, iblocks, cg_align);
			ncg = cgblocks / cg_fsize;
			cg_fsize = cgblocks / ncg;
			cg_isize = cg_fsize / (cgblocks / iblocks);
		}
	}
	else
	{
		/* round cg_fsize down to a cg_align multiple. Sanity check:
		 * if given cg_fsize is < align, complain & exit */

		if (cg_fsize < cg_align)
		{
			fprintf(stderr,
		"%s: invalid cgsize - smaller than specified alignment.\n",
			progname);
			exit(1);
		}
		cg_fsize -= cg_fsize % cg_align; 
		if (cg_fsize > cgblocks)
		{
			fprintf(stderr,
		"%s: invalid cgsize - larger than available space.\n",
			progname);
			exit(1);
		}
		ncg = cgblocks / cg_fsize;

		/* Now the inode computations:
		 * Adjust cg_isize to be a multiple of i_align.  Round up or
		 * down, whichever is closest, unless this would make it zero!
		 */
		cg_isize = (iblocks + ncg - 1) / ncg;
		if (i = (cg_isize % i_align))
		{
			if (cg_isize < i_align)
				cg_isize = i_align;
			else if (i < (i_align / 2))
				cg_isize -= i;
			else
				cg_isize += (i_align - i);
		}
	}

	if (cg_isize >= cg_fsize)
	{
		fprintf(stderr,
	"%s: Ridiculous number of inodes; no space for data.\n",
		progname);
		exit(1);
	}
	fs_isize = cg_isize * ncg * EFS_INOPBB;
	buildfs();
	exit(0);
	/* NOTREACHED */
}


/*
 * Get info about device size & geometry.
 */
static void
getdevinfo(void)
{
	int	heads;
	int	add_inodes = 0;
	long long	ldevsize;

	if ((ldevsize = findsize(special)) <= 0LL)
	{
		fprintf(stderr,"Can't determine size of %s\n", special);
		blather();
		exit(1);
	}
	if (ldevsize > 0x7fffffffLL)
		dev_blocks = 0x7fffffffL;
	else
		dev_blocks = (long)ldevsize;
	fs_blocks = dev_blocks;    /* by default, use whole device */

	if (fs_blocks > 0xffffff)	/* bigger than 8gb? */
	{
		fprintf(stderr,"%s: warning: using only first 8GB of %s\n",
			progname, special);
		fs_blocks = 0xffffff;
	}

	fs_sectors = 128;	/* as good as anything else; it's a nice
		power of 2.  The whole idea of heads/cyls/sect_trk is silly
		with almost all current drives.  I want to remove that completely
		but for now we will just do this. */
	heads = 8;

	fs_isize = GOOD_INODE_TOTAL(fs_blocks); 

	/*
	 * On root partitions > 16MB, add 512 inodes for each additional MB.
	 */
	if (is_rootpart(special))
	{
		add_inodes = (fs_blocks - 0x8000) / 4;
		if (add_inodes < 0)
			add_inodes = 0;
		if (add_inodes > 4096)
			add_inodes = 4096;
	}
	fs_isize += add_inodes;

	if (user_inodes)
	{
		if (user_inodes < fs_isize)
			fprintf(stderr,"Warning: specified number of inodes %d is smaller than normal default %d\n", user_inodes, fs_isize);
		fs_isize = user_inodes;
	}
	if (cyl_align)
	{
		cg_align = fs_sectors * heads;
		i_align = fs_sectors * heads;
	}
	else
	{
		cg_align = 1;
		i_align = 1;
	}
}

static void
blather(void)
{
	fprintf(stderr,"Check that device exists and is writeable.\n");
	fprintf(stderr,"(Note that it is possible for a special file\n");
	fprintf(stderr,"to exist and have writeable permissions, but\n");
	fprintf(stderr,"for there to be no real device corresponding\n");
	fprintf(stderr,"to it).\n");
}

/*
 * cgsizepick() chooses cylinder group size, inodes per cylinder group,
 * and number of cylinder groups for a filesystem.  It holds inodes per
 * cylinder group to a constant and varies the data blocks per cylinder
 * group by up to 20 percent either way, looking for the least amount
 * of space wastage.  This algorithm is optimized for large align values.
 */
static void
cgsizepick(int blocks, int iblocks, int align,
	long *addr_ncg, long *addr_cg_fsize, long *addr_cg_isize)
{
	int cgs, cguess, i, waste;
	int savecgs, savecgsize, savecgisize;
	int prevwaste = blocks;
	int mincgsize, mult;

	mult = (blocks + iblocks - 1) / iblocks;

	/* initial guess for cgsize, in blocks */
	mincgsize = mult * align;
	cguess = (good_cg_size(blocks, iblocks, align) / mincgsize) * mincgsize;
	if (cguess == 0)
		cguess = mincgsize;
	savecgisize = align * (cguess / mincgsize);

	for (i = 0; i < cguess/5; i += align)
	{
		cgs = blocks / (cguess + i);
		waste = blocks % (cguess + i);
		if (waste < prevwaste)
		{
			prevwaste = waste;
			savecgs = cgs;
			savecgsize = cguess + i;
		}

		cgs = blocks / (cguess - i);
		waste = blocks % (cguess - i);
		if (waste < prevwaste)
		{
			prevwaste = waste;
			savecgs = cgs;
			savecgsize = cguess - i;
		}
	}
	*addr_ncg = savecgs;
	*addr_cg_fsize = savecgsize;
	*addr_cg_isize = savecgisize;
}

/*
 * Initialize the superblock
 */
static void
init_sb(void)
{
	fs = &sblock.fs;
	bzero(fs, sizeof(*fs));
	fs->fs_size = start_blocks + (ncg * cg_fsize);
	fs->fs_firstcg = firstcg;
	fs->fs_cgfsize = cg_fsize;
	fs->fs_cgisize = (short)cg_isize;
	fs->fs_sectors = fs_sectors;
	fs->fs_ncg = (short)ncg;
	time((time_t *)&fs->fs_time);
	fs->fs_magic = EFS_MAGIC;
	fs->fs_bmblock = 0;	/* to force 3.2 defaults... */
	fs->fs_heads = 10;   /* BOGUS: but 3.2 mount expects nonzero... */
	/*
	 * If short form we might have cut the size to 8gb.
	 * If long form invocation, put in a replicate superblock only if
	 * the given size parameter is "close" to the actual device size.
	 * "Close" is arbitrarily defined as less than cgfsize... 
	 * Of course, if we're working on a file not a device (eg miniroot
	 * construction) we don't have a dev size so never put in a replsb!
	 */
	if (!dev_blocks || ((dev_blocks - fs_blocks) >= cg_fsize)) 
		fs->fs_replsb = 0;
	else
		fs->fs_replsb = dev_blocks - 1L;
	fs->fs_bmsize = (fs->fs_size + BITSPERBYTE - 1) / BITSPERBYTE;
	fs->fs_tfree = ncg * (fs->fs_cgfsize - fs->fs_cgisize);
	strncpy(fs->fs_fpack, "nopack", sizeof(fs->fs_fpack));
	strncpy(fs->fs_fname, "noname", sizeof(fs->fs_fname));

	/* subtract 2 for inodes 0 & 1 which are never used */
	fs->fs_tinode = ncg * fs->fs_cgisize * EFS_INOPBB - 2;

	/* If this is recovery-option, mark the superblock dirty to
	 * force an fsck before it can be mounted.
	 */

	if (recover_option)
		fs->fs_dirty = EFS_DIRTY;
	EFS_SETUP_SUPERB(fs);
}

/*
 * Clear out the inodes
 */
static void
clear_inodes(void)
{
	struct efs_dinode *di;
	char *cp;
	int ilist_bytes;
	int i;
	time_t igen;

	ilist_bytes = BBTOB(cg_isize);
	cp = calloc(1, ilist_bytes);
	if (!cp) {
		fprintf(stderr,
			"%s: can't initialize inodes - out of memory\n",
			progname);
		exit(1);
	}
	/*
	 * Construct generation number based on current time to avoid
	 * generation number collisions when a server recreates an
	 * exported file system.
	 */
	time(&igen);
	di = (struct efs_dinode *)cp;
	for (i = EFS_INOPBB * cg_isize; i > 0; i-- ) {
		di->di_gen = igen;
		di++;
	}
	for (i = 0; i < ncg; i++) {
		int result;
		result = c_writeb(fs_fd, cp, EFS_CGIMIN(fs, i), cg_isize);
		if(result == -1) {
			fprintf(stderr, "%s: inode write failed at block %d: %s\n",
				progname, EFS_CGIMIN(fs, i), strerror(errno));
			exit(1);
		}
		if(result != cg_isize) {
			fprintf(stderr, "%s: inode write short; asked for %d, wrote %d at block %d\n",
				progname, cg_isize, result, EFS_CGIMIN(fs, i));
			exit(1);
		}
	}
	free(cp);
}

/*
 * Fill bootblock with zeros
 */
static void
clear_bootblock(void)
{
	long buf[BBSIZE / sizeof(long)];
	int result;

	bzero(buf, sizeof(buf));
	if((result=c_writeb(fs_fd, (char *)buf, 0, 1)) != 1) 
	{
		fprintf(stderr, "%s: boot block write failed: %s\n", progname,
			result==-1?strerror(errno):"short write");
		exit(1);
	}
}

/*
 * Initialize the bitmap
 */
static void
init_bitmap(void)
{
	int i, j;
	efs_daddr_t bn;

	bitmap = malloc(BBTOB(bitmap_blocks));
	if (!bitmap) 
	{
		fprintf(stderr, "%s: can't initialize bitmap - out of memory\n",
				progname);
		exit(1);
	}

	/*
	 * Start the bitmap out as entirely used.  Then clear out
	 * the regions that are allocatable.
	 */
	bzero(bitmap, BBTOB(bitmap_blocks));
	for (i = 0; i < ncg; i++) {
		bn = EFS_CGIMIN(fs, i) + fs->fs_cgisize;
		for (j = fs->fs_cgisize; j < fs->fs_cgfsize; j++) {
			bset(bitmap, bn);
			bn++;
		}
	}
}

void
error(void)
{
	int old_errno;

	old_errno = errno;
	fprintf(stderr, "%s: i/o error\n", progname);
	errno = old_errno;
	perror(progname);
	exit(1);
	/* NOTREACHED */
}

/*
 * Get a character from the canned string or from the input file
 */
static int
getch(void)
{

	if (charp)
		return (*charp++);
	return (getc(fproto));
}

/*
 * Get a string from the input file.  Gleefully stolen from system V mkfs.
 */
static void
getstr(void)
{
	int i, c;

loop:
	switch(c = getch()) {
	  case ' ':
	  case '\t':
	  case '\n':
		goto loop;
	  case '\0':
		fprintf(stderr, "%s: premature EOF on %s\n", progname, proto);
		exit(1);
	  case ':':
		while (getch() != '\n')
			;
		goto loop;
	}

	i = 0;
	do {
		string[i++] = c;
		c = getch();
	} 
	while((c != ' ') && (c != '\t') && (c != '\n') && (c != '\0'))
		;
	string[i] = '\0';
}

/*
 * Convert a string into a decimal number.
 */
static long 
getnum(void)
{
	int i, c;
	long n;

	getstr();
	n = 0;
	for (i = 0; c = string[i]; i++) {
		if ((c < '0') || (c > '9')) {
			fprintf(stderr, "%s: %s is a bad number\n",
					progname, string);
			exit(1);
		}
		n = n * 10 + (c - '0');
	}
	return n;
}

/*
 * Parse the proto file
 */
static void
parseproto(efs_ino_t parent, efs_ino_t child)
{
	struct efs_dinode *di;
	int val;
	int i;
	int majdev, mindev;
	int fmt, mode, uid, gid;
	efs_ino_t newino;
	int islostfound;

	/* get first word */
	getstr();

	/* decode mode spec */
	switch (string[0]) {
	  case '-': fmt = IFREG; break;
	  case 'b': fmt = IFBLK; break;
	  case 'c': fmt = IFCHR; break;
	  case 'd': fmt = IFDIR; break;
	  case 'p': fmt = IFIFO; break;
	  case 'l': fmt = IFLNK; break;
	}
	mode = 0;
	if (string[1] == 'u')
		mode |= ISUID;
	if (string[2] == 'g')
		mode |= ISGID;
	val = 0;
	for (i = 3; i < 6; i++) {
		if ((string[i] < '0') || (string[i] > '7')) {
			fprintf(stderr,
				"%s: %c/%s: bad octal mode digit\n",
				progname, string[i], string);
			exit(1);
		}
		val = (val * 8) + (string[i] - '0');
	}
	mode |= val;

	/* decode uid & gid */
	uid = getnum();
	gid = getnum();

	/* initialize an inode */
	efs_mknod(child, mode | fmt, uid, gid);

	/* now create the requested file */
	switch (fmt) {
	  case IFREG:			/* regular file */
		getstr();
		efs_newregfile(child, string);
		break;

	  case IFBLK:			/* block device special */
	  case IFCHR:			/* character device special */
		majdev = getnum() & L_MAXMAJ;
		mindev = getnum() & L_MAXMIN;

		di = efs_iget(child);
		efs_putdev(makedev(majdev, mindev), &di->di_u);
		efs_iput(di, child);
		break;

	  case IFIFO:			/* fifo */
		break;

	  case IFLNK:			/* symbolic link */
		/*
		 * Get contents of link from proto and write it to the
		 * link file.
		 */
		getstr();
		efs_write(child, string, strlen(string));
		break;

	  case IFDIR:			/* directory */
		if (child == EFS_ROOTINO) {
			/*
			 * For the root inode, we just need to set its
			 * link count to two to get things going.
			 */
			di = efs_iget(parent);
			di->di_nlink = 2;
			efs_iput(di, parent);
		} else {
			/* increment link count of parent inode */
			di = efs_iget(parent);
			di->di_nlink++;
			efs_iput(di, parent);

			/* increment link count of new directory */
			di = efs_iget(child);
			di->di_nlink++;
			efs_iput(di, child);
		}

		/* put "." and ".." entries into new directory */
		efs_enter(child, child, ".");
		efs_enter(child, parent, "..");

		/* now read in the directories contents and install them */
		for (;;) {
			getstr();
			if ((string[0] == '$') && (string[1] == '\0')) {
				break;
			}
			/*
			 * Allocate a new inode for the file in the current
			 * directory.  Then parse its attributes by recursing.
			 */
			newino = efs_allocino();
			efs_enter(child, newino, string);
			islostfound = strcmp(string, "lost+found") == 0;
			parseproto(child, newino);
			/*
			 * If inode is the lost+found inode, make it a big
			 * file so that fsck will work well.  We do this
			 * after recursing to allow for the lost+found "."
			 * and ".." directories to be entered first.
			 */
			if (islostfound) {
				efs_mklostandfound(newino);
			}
		}
		break;
	}
}

static void
be_verbose(void)
{
	printf( "Warning: read/write support for EFS filesystems will be\n"
		"removed from the next all-platform IRIX release.\n");
	printf("%s: %s: blocks=%d inodes=%d\n",
		    progname, special, fs_blocks, ncg * cg_isize * EFS_INOPBB);
	printf("%s: %s: sectors=%d cgfsize=%d\n",
		    progname, special, fs_sectors, cg_fsize);
	printf("%s: %s: cgalign=%d ialign=%d ncg=%d\n",
		    progname, special, cg_align, i_align, ncg);
	printf("%s: %s: firstcg=%d cgisize=%d\n",
		    progname, special, firstcg, cg_isize);
	printf("%s: %s: bitmap blocks=%d\n",
		    progname, special, bitmap_blocks);
}

static void
buildfs(void)
{
	char *realpath;
	int result;

	if (rawpath) 
		realpath = rawpath;
	else
		realpath = special;

	if(interact || !quiet)
		be_verbose();

	if (interact) ask_confirm();

	/* Added 2/14/90: the "recover" option just puts the superblock
	 * on disk without touching the rest of the space. This allows
	 * a last-ditch attempt to salvage a filesystem with fsck.
	 */

	if (!recover_option)
		clear_other_sb();

	init_sb();
	/* allocate root inode */
	fs->fs_tinode--;

	if (!recover_option)
	{
		clear_inodes();
		clear_bootblock();
		init_bitmap();

		/* finally, parse proto file and build fs */
		parseproto(EFS_ROOTINO, EFS_ROOTINO);

		/* sync to disk: updated superblock, its replica, & the bitmap. */

		if((result=c_writeb(fs_fd, bitmap, EFS_BITMAPBB, bitmap_blocks)) !=
		 	bitmap_blocks) 
		{
			fprintf(stderr,"%s: Can't write bitmap to %s: %s\n", progname, realpath,
				result==-1?strerror(errno):"short write");
			exit(1);
		}
	}

	/* keep track of the last inode in the fs */
	fs->fs_lastialloc = efs_lastallocino();
	efs_checksum();		/* libefs relies on global fs, yucko!! */

	if((result=c_writeb(fs_fd, sblock.block, EFS_SUPERBB , 1)) != 1)
	{
		fprintf(stderr,"%s: Can't write superblock to %s: %s\n", progname, realpath,
			result==-1?strerror(errno):"short write");
		exit(1);
	}

	/* We write the replicated superblock only if its pointer is
	 * present in the superblock.
	 * Also not written if it's recover_option.
	 */

	if (!recover_option && fs->fs_replsb &&
	    ((result=c_writeb(fs_fd, sblock.block, fs->fs_replsb, 1)) != 1))
	{
		fprintf(stderr,"%s: Can't write 2nd superblock to %s: %s\n", progname, realpath,
			result==-1?strerror(errno):"short write");
		exit(1);
	}
}

static void
ask_confirm(void)
{
	char rb[100];

	if (!isatty(0)) return;  /* don't hang if stdin isn't a tty! */
	printf("%s: is %s correct? (y/n?) ", progname, special);
	fflush(stdout);
	gets(rb);
	if ((*rb != 'y') && (*rb != 'Y')) exit(0);
}

/* ismounted()
 *
 * Checks to see if named partition is already mounted, part of
 * a logical volume, or reserved via a raw entry in fstab.
 * Uses the mnt_check routines in libdisk.
 */
static int
ismounted(char *name)
{
	mnt_check_state_t *check_state;
	int ret;

	if (mnt_check_init(&check_state) == -1) {
	    fprintf(stderr,"%s: unable to call mnt_check_init() for %s, mount checks disabled.\n",
		    progname, name);
	    return 0;
	}

	ret = mnt_find_mount_conflicts(check_state, name);

	if (ret > 0) {
	    if (mnt_causes_test(check_state, MNT_CAUSE_MOUNTED)) {
		fprintf(stderr, "%s: %s is already in use.\n",
			progname, name);
	    } else if (mnt_causes_test(check_state, MNT_CAUSE_OVERLAP)) {
		fprintf(stderr, "%s: %s overlaps partition already in use.\n",
			progname, name);
	    } else {
		mnt_causes_show(check_state, stderr, progname);
	    }
	    (void) fprintf(stderr, "\n");
	    mnt_plist_show(check_state, stderr, progname);
	    (void) fprintf(stderr, "\n");
	    (void) fflush(stderr);
	}

	mnt_check_end(check_state);

	if (ret < 0)
	        return 0;

	return ret;
}

static void
userr(void)
{
        fprintf(stderr,
		"Usage: %s [-q] [-a] [-i] [-r] [-n inodes] special [proto]\n",
		progname);
	fprintf(stderr,
		"       %s [-q] [-i] [-r] special blocks inodes heads "
		"sectors cgfsize cgalign ialign [proto]\n", progname);
	exit(1);
}


/*	clear out any potential superblocks in partitions which have their
	superblock overlapped by the one we are making.
	This is primarily for the benefit of the
	disk tools, so that we don't tell the user one of the overlapped
	partitions is a valid mount point.  It's only an issue when the
	other partition had a filesystem, and none of the info we write
	happens to clobber it.
	If we can't do it for some reason, be silent about it, since this
	is just icing on the cake...
	Also, only try on regular disks, not floppies & logical volumes.
*/
static void
clear_other_sb(void)
{
	struct stat sb;
	unsigned token;
	struct ptinfo *pt;
	struct ustat ustatarea;

	/* libdisk.a code needs to have name starting with /dev/rdsk */
	if(stat(special, &sb) == -1)
		return;

	if (!rawpath)
		rawpath = findrawpath(special);

	if (!rawpath) return;	/* too bad... */
		
	if((token=setdiskinfo(rawpath, "/etc/fstab", 0)) == 0)
		return;
	while(pt = getpartition(token)) {
		struct stat sb2;
		if(stat(partname(rawpath, pt->ptnum), &sb2) == 0 &&
			sb2.st_rdev == sb.st_rdev) {
			struct ptinfo **pover;
			for(pover=pt->overlaps; pover && *pover; pover++) {
				char *pn;
				if(stat((pn=partname(rawpath, (*pover)->ptnum)), &sb2) == 0) {
					if(ustat(sb2.st_rdev,&ustatarea) == 0) {
						fprintf(stderr,
						  "%s overlaps a mounted filesystem on partition %d\n",
						  special, (*pover)->ptnum);
						exit(1);
					}
					if(((*pover)->pstartblk+EFS_SUPERBOFF) >= pt->pstartblk &&
						((*pover)->pstartblk+EFS_SUPERBOFF) <= (pt->pstartblk+pt->psize))
						zap_sb(pn);
				}
			}
			enddiskinfo(token);
			return;
		}
	}
	enddiskinfo(token);
}

/*	zap a superblock on an overlapped partition; see comments at
	clear_other_sb().
*/
static void
zap_sb(char *name)
{
	struct efs e;
	int fd;

	bzero(&e, sizeof(e));
	fd = open(name, O_WRONLY);
	if(fd == -1) {	/* shouldn't happen, but perhaps done by non-super user */
		return;
	}

	/* it's conceivable that this could fail if a vh is bad, since the
		sb isn't necessarily inside the fs we are making */
	if(lseek(fd, (long) EFS_SUPERBOFF, 0) == EFS_SUPERBOFF)
		(void)write(fd, (char *) &e, BBTOB(BTOBB(sizeof(e))));
	/* Clear the block at 0, it might be an old xfs filesystem sb */
	if(lseek(fd, (long) 0, 0) == 0)
		(void)write(fd, (char *) &e, BBTOB(BTOBB(sizeof(e))));
	close(fd);
}

/* kept as a seperate function, because of things like mkfs proto to
 * a file, where we can't use the writeb call, since that only works
 * with devices.
 */
static int
c_writeb(int fd, char *buf, int block, int nblocks)
{
	int bytes, cnt;

	if (use_writeb)
		return writeb(fd, buf, block, nblocks);
	else
	{
		lseek(fd, block * BBSIZE, 0);
		bytes = nblocks * BBSIZE;
		cnt = write(fd, buf, bytes);
		if(cnt == -1)
			return cnt;
		if(cnt == bytes)
			return nblocks;
		/* this is somewhat questionable, but closest, if not exact */
		return bytes/BBSIZE;
	}
}

/* Now a new function efs_newregfile(). This creates a new regular file
 * of the given size; this file may have indirect extents.
 * This is done to avoid rehacking the
 * whole of libefs to know about indirect extents!
 * Basically to get round the size limitations when a mkfs proto specifies
 * a humongo file! Rather than successive extends, we allocate the space
 * all at once (since we're working from a regular file we already
 * know the size).
 *
 * Assumptions: 
 *
 *	efs_mknod has been called to allocate & initialize the dinode.
 *
 *	The bitmap is in core and is correctly representative of blocks
 *	used so far.
 */
static void
efs_newregfile(efs_ino_t ino, char *name)
{
	struct stat sb;
	int len;
	efs_daddr_t blocks, allocblocks;
	int fd;
	struct efs_dinode *di;
	struct extent *exbase = NULL;
	struct extent *ex, *foundex;
	int extents, exbufsize;
	int copysize, copyoffset, copied;
	char *copybuf = NULL;
	static efs_daddr_t curblock = 2;   /* starting place to search for 
					* free blocks */
	int largestextent;
	int i;
	int numindirs = 0;
	int indirblocks;

	if ((fd = open(name, O_RDONLY)) < 0)
	{
		fprintf(stderr,"%s: can't open %s: %s\n", progname, name,
			strerror(errno));
		exit(1);
	}
	if (fstat(fd, &sb) < 0)
	{
		fprintf(stderr,"%s: cannot stat %s: %s\n",progname, name,
			strerror(errno));
		exit(1);
	}
	if ((sb.st_mode & S_IFMT) != S_IFREG)
	{
		fprintf(stderr,"%s: %s is not a regular file\n",progname, name);
		exit(1);
	}
	len = sb.st_size; 
	blocks = (len + (BBSIZE - 1)) / BBSIZE;
	di = efs_iget(ino);

	/* Guess at number of extents & allocate space for them. We will
	 * realloc later if it turns out we need more; however since it's
	 * assumed we're creating in a virgin fs that is unlikely.
	 */

	exbufsize = blocks / 64; 
	exbase = (struct extent *)malloc(BBSIZE + exbufsize * sizeof (struct extent));

	/* now allocate extent space from the bitmap until we've got enough
	 * extents to hold the file.
	 */

	allocblocks = 0;
	extents = 0;
	largestextent = 0;

	while (allocblocks < blocks)
	{
		if ((foundex = findex(curblock, (blocks - allocblocks))) == NULL)
		{
			fprintf(stderr,"%s: cannot allocate space for file: %s\n", progname, name);
			exit(1);
		}
		if (foundex->ex_length > largestextent)
			largestextent = foundex->ex_length; 
		curblock = foundex->ex_bn + foundex->ex_length;
		ex = (exbase + extents);
		ex->ex_magic = 0;
		ex->ex_bn = foundex->ex_bn;
		ex->ex_length = foundex->ex_length;
		ex->ex_offset = allocblocks;
		allocblocks += foundex->ex_length;
		if (++extents == exbufsize)
		{
			exbufsize += 10;
			if ((exbase = (struct extent *)realloc((char *)exbase, (BBSIZE + exbufsize * sizeof (struct extent)))) == NULL)
			{
				fprintf(stderr,"%s: cannot allocate space for file: %s\n", progname, name);
				exit(1);
			}
		}
	}

	if (extents > EFS_DIRECTEXTENTS)
	{
		indirblocks = ((BBSIZE - 1 + (extents * sizeof(struct extent))) 				/ BBSIZE);
		allocblocks = 0;
		while (allocblocks < indirblocks)
		{
			if ((foundex = findex(curblock, (indirblocks - allocblocks))) == NULL)
			{
				fprintf(stderr,"%s: cannot allocate space for file: %s\n", progname, name);
				exit(1);
			}
			curblock = foundex->ex_bn + foundex->ex_length;
			ex = &di->di_u.di_extents[numindirs];
			ex->ex_magic = 0;
			ex->ex_bn = foundex->ex_bn;
			ex->ex_length = foundex->ex_length;
			allocblocks += foundex->ex_length;
			if (++numindirs == EFS_DIRECTEXTENTS)
			{
				fprintf(stderr,"%s: cannot allocate space for file: %s\n", progname, name);
				exit(1);
			}
		}
		di->di_u.di_extents[0].ex_offset = numindirs;
	}

	/* Hokay. Now we've allocated all the extents needed to hold the new 
	 * file's data (including indirect ones if any). Copy the data to the 
	 * appropriate places.
	 */

	if ((copybuf = malloc(largestextent * BBSIZE)) == NULL)
	{
		fprintf(stderr,"%s: can't get buffer memory for file copy\n",
			progname);
		exit(1);
	}

	for (i = 0, ex = exbase, copied = 0; i < extents; i++)
	{
		copysize = ex->ex_length * BBSIZE;
		copyoffset = ex->ex_bn * BBSIZE;
		bzero(copybuf, copysize);
		if ((len - copied) < copysize) /* partial last block */
			copysize = len - copied;
		if (read(fd, copybuf, copysize) != copysize)
		{
			fprintf(stderr, "%s: error reading %s: %s\n", 
				progname, name, strerror(errno));
			exit(1);
		}
		/* set copysize back to BBSIZE multiple: we may be working 
		 * on a raw device!
		 */
		copysize = ex->ex_length * BBSIZE;
		lseek(fs_fd, copyoffset, 0);
		if (write(fs_fd, copybuf, copysize) != copysize)
		{
			fprintf(stderr, "%s: error writing %s: %s\n", 
				progname, name, strerror(errno));
			exit(1);
		}
		copied += copysize;
		ex++;
	}
	free (copybuf);
	copybuf = NULL;

	/* Data copied. Now the extents: if < EFS_DIRECTEXTENTS, they go
	 * in the dinode. If greater, they must be written to the indirect
	 * extents allocated for them; pointers to these are already in 
	 * the dinode in that case.
	 */

	if (extents <= EFS_DIRECTEXTENTS)
	{
		for (i = 0, foundex = exbase, ex = di->di_u.di_extents; i < extents; i++, ex++, foundex++)
		{
			ex->ex_bn = foundex->ex_bn;
			ex->ex_length = foundex->ex_length;
			ex->ex_offset = foundex->ex_offset;
			ex->ex_magic = 0;
		}
	}
	else
	{
		copybuf = (char *)exbase;
		for (i = 0, ex = di->di_u.di_extents; i < numindirs; i++, ex++)
		{
			copysize = ex->ex_length * BBSIZE;
			copyoffset = ex->ex_bn * BBSIZE;
			lseek(fs_fd, copyoffset, 0);
			if (write(fs_fd, copybuf, copysize) != copysize)
			{
				fprintf(stderr, "%s: error writing %s: %s\n", 
					progname, name, strerror(errno));
				exit(1);
			}
			copybuf += copysize;		
		}
		copybuf = NULL;
	}

	
	/* Busy out the appropriate parts of the bitmap. */

	for (i = 0, ex = exbase; i < extents; i++, ex++)
		busyout(ex->ex_bn, ex->ex_length);

	if (numindirs)
		for (i = 0, ex = di->di_u.di_extents; i < numindirs; i++, ex++)
			busyout(ex->ex_bn, ex->ex_length);

	di->di_size = len;

	di->di_mtime = sb.st_mtime;
	di->di_atime = sb.st_atime;
	di->di_ctime = sb.st_ctime;

	di->di_numextents = extents;
	efs_iput(di, ino);
	close (fd);
	if (exbase)
		free ((char *)exbase);
	if (copybuf)
		free ((char *)copybuf);
}

static struct extent *
findex(efs_daddr_t block, int nblocks)
{
	static struct extent ex;
	efs_daddr_t nextblock = block;
	int foundblocks = 0;

	if (nblocks > EFS_MAXEXTENTLEN)
		nblocks = EFS_MAXEXTENTLEN;

	/* first skip any nonfree blocks */

	while (!btst(bitmap, nextblock))
	{
		nextblock++;	/* side effect warning: btst is a macro! */
		if (nextblock == (fs_blocks - 1))
			return (NULL);
	}
	block = nextblock;
		
	while ((foundblocks < nblocks)  && 
		(nextblock < fs_blocks) &&
		btst(bitmap, nextblock))
	{
		foundblocks++;
		nextblock++;
	}

	if (!foundblocks) 
		return (NULL);

	ex.ex_bn = block;
	ex.ex_length = foundblocks;

	return (&ex);
}

static void
busyout(efs_daddr_t bn, int len)
{
	while (len--) 
	{
		bclr(bitmap, bn);
		bn++;		/* can't do it inside bclr: side effects!! */
		fs->fs_tfree--;
	}
}
