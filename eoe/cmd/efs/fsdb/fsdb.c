#ident "$Revision: 1.18 $"

/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Computer Consoles Inc.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* "@(#) Copyright (c) 1988 Regents of the University of California.\n */
/* All rights reserved.\n" */
/* "@(#)fsdb.c	5.3 (Berkeley) 6/18/88" */

/*
 *  fsdb - file system debugger
 *
 *  usage: fsdb [options] special
 *  options:
 *	-?		display usage
 *	-o		override some error conditions
 *	-p"string"	set prompt to string
 *	-w		open for write
 */

#include <sys/param.h>
#include <sys/immu.h>
#include <sys/mkdev.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/wait.h>
#include <dirent.h>
#include <diskinfo.h>
#include <fcntl.h>
#include <mntent.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/fs/efs.h>

/*
 * Never changing defines.
 */
#define	OCTAL		8		/* octal base */
#define	DECIMAL		10		/* decimal base */
#define	HEX		16		/* hexadecimal base */

/*
 * Adjustable defines.
 */
#define	NBUF		10		/* number of cache buffers */
#define PROMPTSIZE	80		/* size of user definable prompt */
#define	MAXFILES	40000		/* max number of files ls can handle */
#define FIRST_DEPTH	10		/* default depth for find and ls */
#define SECOND_DEPTH	100		/* second try at depth (maximum) */
#define INPUTBUFFER	1040		/* size of input buffer */
#define BYTESPERLINE	16		/* bytes per line of /dxo output */
#define	NREG		36		/* number of save registers */

/*
 * Values dependent on sizes of structs and such.
 */
#define NUMB		3			/* these three are arbitrary, */
#define	BLOCK		5			/* but must be different from */
#define	FRAGMENT	7			/* the rest (hence odd).      */
#define	BITSPERCHAR	8			/* couldn't find it anywhere  */
#define	CHAR		(sizeof (char))
#define	SHORT		(sizeof (short))
#define	LONG		(sizeof (long))
#define	INODE		(sizeof (struct dinode))
#define	DIRECTORY	(sizeof (struct efs_dent) + 1)
#define	CGRP		(sizeof (struct cg))
#define SB		(sizeof (struct efs))
#define EXTENT		(sizeof(struct extent))

/*
 * Checks
 */
#define	chk_dirmagic(db) \
	if ((long)(db) & 1) \
		printf("Bogus directory address 0x%x\n", (long)(db)); \
	else if (db->magic != EFS_DIRBLK_MAGIC) \
		printf("Bogus directory magic number 0x%x\n", db->magic)

/*
 * Messy macros that would otherwise clutter up such glamorous code.
 */
#define min(x, y)	((x) < (y) ? (x) : (y))
#define	letter(c)	((((c) >= 'a')&&((c) <= 'z')) ||\
				(((c) >= 'A')&&((c) <= 'Z')))
#define	digit(c)	(((c) >= '0') && ((c) <= '9'))
#define HEXLETTER(c)	(((c) >= 'A') && ((c) <= 'F'))
#define hexletter(c)	(((c) >= 'a') && ((c) <= 'f'))
#define octaldigit(c)	(((c) >= '0') && ((c) <= '7'))
#define uppertolower(c)	((c) - 'A' + 'a')
#define hextodigit(c)	((c) - 'a' + 10)
#define	numtodigit(c)	((c) - '0')
#define loword(X)	(((ushort *)&X)[1])
#define lobyte(X)	(((unsigned char *)&X)[1])

/*
 * Fix up for long long byte addresses.
 */
typedef long long baddr_t;	/* byte address */
#undef BBTOB
#undef BTOBB
#undef BTOBBT
#define BBTOB(bbs)	((baddr_t)((baddr_t)(bbs) << BBSHIFT))
#define	BTOBB(bytes)	((unsigned long)(((baddr_t)(bytes) + (baddr_t)BBSIZE - 1LL) >> BBSHIFT))
#define	BTOBBT(bytes)	((unsigned long)((baddr_t)(bytes) >> BBSHIFT))

/*
 * Converting Berkeleyism's to SGIisms.
 */
#define	BLKSIZE			BBSIZE
#define	FRGSIZE			BBSIZE
#define	BLKSHIFT		BBSHIFT
#define	FRGSHIFT		BBSHIFT
#define SBLOCK			EFS_SUPERBB
#define lblkno(fs, address)	BTOBBT(address)
#define fragstoblks(fs, frags)	((frags) >> (FRGSHIFT - BLKSHIFT))
#define blkstofrags(fs, blks)	((blks) << (FRGSHIFT - BLKSHIFT))
#define cg_chkmagic(cg)		(1)
#define blkoff(fs, loc)		((loc) & (baddr_t)BBMASK)
#define dinode			efs_dinode
#define cgtod(fs, cgrp)		EFS_CGIMIN((fs), (cgrp))
#define blkroundup(fs, size)	(((size) + BBSIZE - 1) & ~BBMASK)
#define STRINGSIZE(d)		((long)((u_long)(d)->d_namelen))
#define d_ino			ud_inum.l
#define NDADDR			EFS_DIRECTEXTENTS
#define NIADDR			EFS_DIRECTEXTENTS
#define di_db			di_u.di_extents
#define itob(i)			((BBTOB(EFS_ITOBB(fs, i))) + \
				 (EFS_ITOO(fs, (i)) << EFS_EFSINOSHIFT))
#define fragoff(fs, loc)	((loc) & (baddr_t)BBMASK)
#define cgimin(fs, cg)		(EFS_CGIMIN((fs), (cg)))
#define INOPB(fs)		EFS_INOPBB
#define isset(a, i)		((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define NINDIR(fs)		(BBSIZE/sizeof(struct extent))

/*
 * buffer cache structure.
 */
struct buf {
	struct	buf  *fwd;
	struct	buf  *back;
	char	*blkaddr;
	short	valid;
	long	blkno;
} buf[NBUF], bhdr;

/*
 * used to hold save registers (see '<' and '>').
 */
struct	save_registers {
	baddr_t	sv_addr;
	long	sv_value;
	short	sv_objsz;
} regs[NREG];

/*
 * cd, find, and ls use this to hold filenames.  Each filename is broken
 * up by a slash.  In other words, /usr/src/adm would have a len field
 * of 2 (starting from 0), and filenames->fname[0-2] would hold usr,
 * src, and adm components of the pathname.
 */
struct filenames {
	long	ino;		/* inode */
	long	len;		/* number of components */
	char	flag;		/* flag if using SECOND_DEPTH allocator */
	char	find;		/* flag if found by find */
	char	**fname;	/* hold components of pathname */
} *filenames, *top;

struct efs filesystem, *fs;	/* super block */

/*
 * Global data.
 */
char		*input_path[MAXPATHLEN];
char		*stack_path[MAXPATHLEN];
char		*current_path[MAXPATHLEN];
char		input_buffer[INPUTBUFFER];
char		*prompt;
char		*buffers;
char		scratch[64];
char		BASE[] = "o u     x";
char		PROMPT[PROMPTSIZE] = "> ";
char		laststyle = '/';
char		lastpo = 'x';
short		input_pointer;
short		current_pathp;
short		stack_pathp;
short		input_pathp;
short		cmp_level;
short		nfiles;
short		type = NUMB;
short		dirslot;
short		eslotno;
short		fd;
short		c_count;
short		error;
short		paren;
short		trapped;
short		doing_cd;
short		doing_find;
short		find_by_name;
short		find_by_inode;
short		long_list;
short		recursive;
short		objsz = SHORT;
short		override = 0;
short		wrtflag;
short		base = HEX;
short		acting_on_inode;
short		acting_on_directory;
short		should_print = 1;
short		clear;
short		star;
baddr_t		addr;
baddr_t		bod_addr;
long		value;
baddr_t		erraddr;
long		errcur_bytes;
baddr_t		errino;
long		errinum;
long		cur_cgrp;
long		cur_sblock;
baddr_t		cur_ino;
long		cur_inum;
baddr_t		cur_dir;
long		cur_block;
long		cur_bytes;
long		find_ino;
long		filesize;
long		blocksize;
long		stringsize;
long		count = 1;
long		commands;
long		read_requests;
long		actual_disk_reads;
long		debug;
jmp_buf		env;
int		gtemp;

static struct mntent	*allocfsent(struct mntent *);
static int		bcomp(baddr_t);
static long		bmap(long);
static baddr_t		cgrp_check(long);
static int		check_addr(short, short *, short *, short);
static int		compare(char *, char *, short);
static void		compute_cginfo(struct efs **);
static void		compute_efs(struct efs *);
static int		devcheck(short);
static void		eat_spaces(void);
static int		efs_addrtoinum(baddr_t);
static int		emajor(struct edevs *);
static int		eminor(struct edevs *);
static void		err(void);
static baddr_t		expr(void);
static int		fcmp(const void *, const void *);
static int		ffcmp(const void *, const void *);
static void		fill(void);
static void		find(void);
static char		*fmtentry(struct filenames *);
static void		follow_path(long, long);
static void		formatf(struct filenames *, struct filenames *);
static void		fprnt(char, char);
static void		freemem(struct filenames *, int);
static struct mntent	*fstabsearch(char *);
static long		get(short);
static char		getachar(void);
static char		*getblk(baddr_t);
static baddr_t		getdirslot(short);
static void		getfstab(void);
static void		getname(void);
static void		getnextinput(void);
static baddr_t		getnumb(void);
static int		icheck(baddr_t);
static struct extent	*inext(struct extent *, int, int);
static void		insert(struct buf *);
static void		ls(struct filenames *, struct filenames *, short);
static int		match(char *, int);
static void		myindex(int);
static int		nullblk(long);
static void		parse(void);
static void		print(baddr_t, int, int, int);
static unsigned long	*print_check(unsigned long *, long *, short, int);
static void		print_path(char *[], short);
static void		printcg(struct cg *);
static void		printextent(struct extent *, int);
static void		printsb(struct efs *);
static int		put(long, short);
static int		puta(void);
static void		putf(char);
static char		*rawname(struct mntent *);
static void		restore_inode(long);
static baddr_t		term(void);
static void		ungetachar(char);
static int		valid_addr(void);

/*
 * main - lines are read up to the unprotected ('\') newline and
 *	held in an input buffer.  Characters may be read from the
 *	input buffer using getachar() and unread using ungetachar().
 *	Reading the whole line ahead allows the use of debuggers
 *	which would otherwise be impossible since the debugger
 *	and fsdb could not share stdin.
 */
int
main(int argc, char **argv)
{
	char		c, *cptr;
	short		i;
	struct efs_dent	*dep;
	struct buf	*bp;
	char		*progname;
	short		colon, mode;
	short		tbase;
	long		temp;
	struct stat	statb;
	struct mntent	*dt;
	char		*disk, *odisk;
	int		tryagain = 1;

	setbuf(stdin, NULL);

	progname = argv[0];
	prompt = &PROMPT[0];
	/*
	 * Parse options.
	 */
	while (argc>1 && argv[1][0] == '-') {
		if (strcmp("-?", argv[1]) == 0)
			goto usage;
		if (strcmp("-d", argv[1]) == 0) {
			debug = 1;
			argc--; argv++;
			continue;
		}
		if (strcmp("-o", argv[1]) == 0) {
			override = 1;
			argc--; argv++;
			continue;
		}
		if (strncmp("-p", argv[1],2) == 0) {
			prompt = &argv[1][2];
			argc--; argv++;
			continue;
		}
		if (strcmp("-w", argv[1]) == 0) {
			/* suitable for open */ 
			wrtflag = 2;
			argc--; argv++;
			continue;
		}
		goto usage;
	}
	if (argc!=2) {
usage:
		printf("usage:   %s [options] special\n", progname);
		printf("\tspecial		raw device file name\n");
		printf("options:\n");
		printf("\t-?		display usage\n");
		printf("\t-o		override some error conditions\n");
		printf("\t-p\"string\"	set prompt to string\n");
		printf("\t-w		open for write\n");
		exit(1);
	}

	disk = argv[1];
	getfstab();
	if ((dt = fstabsearch(disk)) != NULL) {
		disk = rawname(dt);
	}
	odisk = disk;

	/*
	 * Attempt to open the special file.
	 */
rawopen:
	if (((fd = open(disk,wrtflag)) < 0) || (stat(disk, &statb) < 0)) {
		perror(disk);
		exit(1);
	}
	if (!S_ISCHR(statb.st_mode)) {
		/*
		 * Try last ditch effor to find character device
		 */
		(void) close(fd);
		if (tryagain && ((disk = findrawpath(disk)) != NULL)) {
			tryagain = 0;
			goto rawopen;
		}
		fprintf(stderr, "%s is not a character device\n", odisk);
		exit(1);
	}
	/*
	 * Read in the super block and validate (not too picky).
	 */
	if (syssgi(SGI_READB, fd, &filesystem, EFS_SUPERBB, 1) != 1) {
		fprintf(stderr, "%s: cannot read superblock\n", disk);
		exit(1);
	}
	fs = &filesystem;
	if (!IS_EFS_MAGIC(fs->fs_magic)) {
		fprintf(stderr, "%s: Bad magic number in file system\n", disk);
		if (!override)
			exit(1);
	}
	(void) compute_efs(fs);
	(void) compute_cginfo(&fs);
	printf("fsdb of %s %s\n", disk,
		wrtflag ? "(Opened for write)" : "(Read only)");
	if (debug)
		printf("debug on\n");
	if (override)
		printf("error checking off\n");
	/*
	 * Malloc buffers and set up cache.
	 */
	buffers = malloc(NBUF * BLKSIZE);
	bhdr.fwd = bhdr.back = &bhdr;
	for (i=0; i<NBUF; i++) {
		bp = &buf[i];
		bp->blkaddr = buffers + (i * BLKSIZE);
		bp->valid = 0;
		insert(bp);
	}
	/*
	 * Malloc filenames structure.  The space for the actual filenames
	 * is allocated as it needs it.
	 */
	filenames = (struct filenames *)calloc(MAXFILES,
						sizeof (struct filenames));
	if (filenames == NULL) {
		printf("out of memory\n");
		exit(1);
	}

	restore_inode(2);
	/*
	 * Malloc a few filenames (needed by pwd for example).
	 */
	for (i = 0; i < MAXPATHLEN; i++) {
		input_path[i] = calloc(1, MAXNAMLEN);
		stack_path[i] = calloc(1, MAXNAMLEN);
		current_path[i] = calloc(1, MAXNAMLEN);
		if (current_path[i] == NULL) {
			printf("out of memory\n");
			exit(1);
		}
	}
	current_pathp = -1;

	(void) signal(SIGINT, err);
	setjmp(env);

	getnextinput();
	/*
	 * Main loop and case statement.  If an error condition occurs
	 * initialization and recovery is attempted.
	 */
	for (;;) {
		if (error) {
			freemem(filenames, nfiles);
			nfiles = 0;
			c_count = 0;
			count = 1;
			star = 0;
			error = 0;
			paren = 0;
			acting_on_inode = 0;
			acting_on_directory = 0;
			should_print = 1;
			addr = erraddr;
			cur_ino = errino;
			cur_inum = errinum;
			cur_bytes = errcur_bytes;
			printf("?\n");
			getnextinput();
			if (error)
				continue;
		}
		c_count++;

		switch (c = getachar()) {

		case '\n': /* command end */
			freemem(filenames, nfiles);
			nfiles = 0;
			if (should_print && laststyle == '=') {
				ungetachar(c);
				goto calc;
			}
			if (c_count == 1) {
				clear = 0;
				should_print = 1;
				erraddr = addr;
				errino = cur_ino;
				errinum = cur_inum;
				errcur_bytes = cur_bytes;
				switch (objsz) {
				case DIRECTORY:
					if ((addr =
					     getdirslot(dirslot+1)) == 0)
						should_print = 0;
					if (error) {
						ungetachar(c);
						continue;
					}
					break;
				case INODE:
					cur_inum++;
					addr = itob(cur_inum);
					if (!icheck(addr)) {
						/* always? -XXX */
						if (!override)
							cur_inum--;
						should_print = 0;
					}
					break;
				case CGRP:
					cur_cgrp++;
					if ((addr=cgrp_check(cur_cgrp)) == 0) {
					     cur_cgrp--;
					     continue;
					}
					break;
				case SB:
					printf("step thru super block ???\n");
					error++;
					continue;
				case EXTENT:
					eslotno++;
				default:
					addr += objsz;
					cur_bytes += objsz;
					if (valid_addr() == 0)
						continue;
					break;
				}
			}
			if (type == NUMB)
				trapped = 0;
			if (should_print)
				switch (objsz) {
				case DIRECTORY:
					fprnt('?', 'd');
					break;
				case INODE:
					fprnt('?', 'i');
					if (!error)
						cur_ino = addr;
					break;
				case CGRP:
					fprnt('?', 'c');
					break;
				case SB:
					fprnt('?', 's');
					break;
				case CHAR:
				case SHORT:
				case LONG:
				case EXTENT:
					fprnt(laststyle, lastpo);
					break;
				}
			if (error) {
				ungetachar(c);
				continue;
			}
			c_count = colon = acting_on_inode = 0;
			acting_on_directory = 0;
			should_print = 1;
			getnextinput();
			if (error)
				continue;
			erraddr = addr;
			errino = cur_ino;
			errinum = cur_inum;
			errcur_bytes = cur_bytes;
			continue;

		case '(': /* numeric expression or unknown command */
		default:
			colon = 0;
			if (digit(c) || c == '(') {
				ungetachar(c);
				addr = expr();
				type = NUMB;
				value = (long)addr;
				continue;
			}
			printf("unknown command or bad syntax\n");
			error++;
			continue;

		case '?': /* general print facilities */
		case '/':
			eslotno = 0;
			/* XXX dirslot = 0 */
			fprnt(c, getachar());
			continue;

		case ';': /* command separator and . */
		case '\t':
		case ' ':
		case '.':
			continue;

		case ':': /* command indicator */
			colon++;
			commands++;
			should_print = 0;
			stringsize = 0;
			trapped = 0;
			continue;

		case ',': /* count indicator */
			colon = star = 0;
			if ((c = getachar()) == '*') {
				star = 1;
				count = BLKSIZE;
			} else {
				ungetachar(c);
				count = (long)expr();
				if (error)
					continue;
				if (!count)
					count = 1;
			}
			clear = 0;
			continue;

		case '+': /* address addition */
			colon = 0;
			c = getachar();
			ungetachar(c);
			if (c == '\n')
				temp = 1;
			else {
				temp = (long)expr();
				if (error)
					continue;
			}
			erraddr = addr;
			errcur_bytes = cur_bytes;
			switch (objsz) {
			case DIRECTORY:
				addr = getdirslot(dirslot + (short)temp);
				if (error)
					continue;
				break;
			case INODE:
				cur_inum += temp;
				addr = itob(cur_inum);
				if (!icheck(addr)) {
					if (!override)
						cur_inum -= temp;
					continue;
				}
				break;
			case CGRP:
				cur_cgrp += temp;
				if ((addr = cgrp_check(cur_cgrp)) == 0) {
					cur_cgrp -= temp;
					continue;
				}
				break;
			case SB:
				printf("increment super block ???\n");
				error++;
				continue;
			case EXTENT:
			default:
				if (objsz == EXTENT)
					eslotno += temp;
				else
					laststyle = '/';
				addr += temp * objsz;
				cur_bytes += temp * objsz;
				if (valid_addr() == 0)
					continue;
				break;
			}
			value = get(objsz);
			continue;

		case '-': /* address subtraction */
			colon = 0;
			c = getachar();
			ungetachar(c);
			if (c == '\n')
				temp = 1;
			else {
				temp = (long)expr();
				if (error)
					continue;
			}
			erraddr = addr;
			errcur_bytes = cur_bytes;
			switch (objsz) {
			case DIRECTORY:
				addr = getdirslot(dirslot - (short)temp);
				if (error)
					continue;
				break;
			case INODE:
				cur_inum -= temp;
				addr = itob(cur_inum);
				if (!icheck(addr)) {
					if (!override)
						cur_inum += temp;
					continue;
				}
				break;
			case CGRP:
				cur_cgrp -= temp;
				if ((addr = cgrp_check(cur_cgrp)) == 0) {
					cur_cgrp += temp;
					continue;
				}
				break;
			case SB:
				printf("decrement super block ???\n");
				error++;
				continue;
			case EXTENT:
			default:
				if (objsz == EXTENT)
					eslotno -= temp;
				else
					laststyle = '/';
				addr -= temp * objsz;
				cur_bytes -= temp * objsz;
				if (valid_addr() == 0)
					continue;
				break;
			}
			value = get(objsz);
			continue;

		case '*': /* address multiplication */
			colon = 0;
			temp = (long)expr();
			if (error)
				continue;
			if (objsz != INODE && objsz != DIRECTORY &&
				objsz != EXTENT)
				laststyle = '/';
			addr *= temp;
			value = get(objsz);
			continue;

		case '%': /* address division */
			colon = 0;
			temp = (long)expr();
			if (error)
				continue;
			if (!temp) {
				printf("divide by zero\n");
				error++;
				continue;
			}
			if (objsz != INODE && objsz != DIRECTORY &&
				objsz != EXTENT)
				laststyle = '/';
			addr /= temp;
			value = get(objsz);
			continue;

		case '=':  /* assignment operation */
			tbase = base;
calc:
			c = getachar();
			if (c == '\n') {
				ungetachar(c);
				c = lastpo;
				if (acting_on_inode == 1) {
					if (c != 'o' && c != 'd' && c != 'x' &&
					    c != 'O' && c != 'D' && c != 'X') {
						switch (objsz) {
						case LONG:
							c = lastpo = 'X';
							break;
						case SHORT:
							c = lastpo = 'x';
							break;
						case CHAR:
							c = lastpo = 'c';
							break;
						}
					}
				} else {
					if (acting_on_inode == 2)
						c = lastpo = 't';
				}
			} else if (acting_on_inode)
				lastpo = c;
			should_print = star = 0;
			count = 1;
			erraddr = addr;
			errcur_bytes = cur_bytes;
			switch (c) {
			case '"': /* character string */
				if (type == NUMB) {
					blocksize = BLKSIZE;
					filesize = BLKSIZE * 2;
					cur_bytes = (long)blkoff(fs, addr);
					if (objsz==DIRECTORY || objsz==INODE &&
						objsz == EXTENT)
						lastpo = 'X';
				}
				puta();
				continue;
			case '+': /* =+ operator */
				temp = (long)expr();
				value = get(objsz);
				if (!error)
					put(value+temp,objsz);
				continue;
			case '-': /* =- operator */
				temp = (long)expr();
				value = get(objsz);
				if (!error)
					put(value-temp,objsz);
				continue;
			case 'b':
			case 'c':
				if (objsz == CGRP)
					fprnt('?', c);
				else
					fprnt('/', c);
				continue;
			case 'e':
				if (objsz == EXTENT)
					fprnt('?', 'e');
				continue;
			case 'i':
				addr = cur_ino;
				fprnt('?', 'i');
				continue; 
			case 's':
				fprnt('?', 's');
				continue;
			case 't':
			case 'T':
				laststyle = '=';
				printf("\t\t");
				printf("%s", ctime((time_t *)&value));
				continue;
			case 'o':
				base = OCTAL;
				goto otx;
			case 'd':
				if (objsz == DIRECTORY) {
					addr = cur_dir;
					fprnt('?', 'd');
					continue;
				}
				base = DECIMAL;
				goto otx;
			case 'x':
				base = HEX;
otx:
				laststyle = '=';
				printf("\t\t");
				if (acting_on_inode)
					print(value & 0177777L, 12, -8, 0);
				else
					print(addr & 0177777L, 12, -8, 0);
				printf("\n");
				base = tbase;
				continue;
			case 'O':
				base = OCTAL;
				goto OTX;
			case 'D':
				base = DECIMAL;
				goto OTX;
			case 'X':
				base = HEX;
OTX:
				laststyle = '=';
			 	printf("\t\t");
				if (acting_on_inode)
					print(value, 12, -8, 0);
				else
					print(addr, 12, -8, 0);
				printf("\n");
				base = tbase;
				continue;
			default: /* regular assignment */
				ungetachar(c);
				value = (long)expr();
				if (error)
					printf("syntax error\n");
				else
					put(value,objsz);
				continue;
			}

		case '>': /* save current address */
			colon = 0;
			should_print = 0;
			c = getachar();
			if (!letter(c) && !digit(c)) {
				printf("invalid register specification, ");
				printf("must be letter or digit\n");
				error++;
				continue;
			}
			if (letter(c)) {
				if (c < 'a')
					c = uppertolower(c);
				c = hextodigit(c);
			} else
				c = numtodigit(c);
			regs[c].sv_addr = addr;
			regs[c].sv_value = value;
			regs[c].sv_objsz = objsz;
			continue;

		case '<': /* restore saved address */
			colon = 0;
			should_print = 0;
			c = getachar();
			if (!letter(c) && !digit(c)) {
				printf("invalid register specification, ");
				printf("must be letter or digit\n");
				error++;
				continue;
			}
			if (letter(c)) {
				if (c < 'a')
					c = uppertolower(c);
				c = hextodigit(c);
			} else
				c = numtodigit(c);
			addr = regs[c].sv_addr;
			value = regs[c].sv_value;
			objsz = regs[c].sv_objsz;
			continue;

		case 'a':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			if (match("at", 2)) { 		/* access time */
				acting_on_inode = 2;
				should_print = 1;
				addr = (baddr_t)
					&((struct dinode *)cur_ino)->di_atime;
				value = get(LONG);
				type = NULL;
				continue;
			}
			goto bad_syntax;

		case 'b':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			if (match("block", 2)) { 	/* block conversion */
				if (type == NUMB) {
					value = (long)addr;
					cur_bytes = 0;
					blocksize = BLKSIZE;
					filesize = BLKSIZE * 2;
				}
				addr = (baddr_t)value << FRGSHIFT;
				bod_addr = addr;
				value = get(LONG);
				type = BLOCK;
				dirslot = 0;
				eslotno = 0;
				trapped++;
				continue;
			}
			if (match("bs", 2)) {		/* block size */
				printf("bs: command not valid for EFS\n");
				goto bad_command;
#ifdef notdef
				acting_on_inode = 1;
				should_print = 1;
				if (icheck(cur_ino) == 0)
					continue;
				addr = (baddr_t)&((struct efs_dinode *)
					cur_ino)->di_size;
				value = get(LONG);
				type = NULL;
				continue;
#endif /* notdef */
			}
			if (match("base", 2)) {		/* change/show base */
showbase:
				if ((c = getachar()) == '\n') {
					ungetachar(c);
					printf("base =\t\t");
					switch (base) {
					case OCTAL:
						printf("OCTAL\n");
						continue;
					case DECIMAL:
						printf("DECIMAL\n");
						continue;
					case HEX:
						printf("HEX\n");
						continue;
					}
				}
				if (c != '=') {
					printf("missing '='\n");
					error++;
					continue;
				}
				value = (long)expr();
				switch (value) {
				default:
					printf("invalid base\n");
					error++;
					break;
				case OCTAL:
				case DECIMAL:
				case HEX:
					base = (short)value;
					break;
				}
				goto showbase;
			}
			goto bad_syntax;

		case 'c':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			if (match("cd", 2)) {		/* change directory */
				top = filenames - 1;
				eat_spaces();
				if ((c = getachar()) == '\n') {
					ungetachar(c);
					current_pathp = -1;
					restore_inode(2);
					continue;
				}
				ungetachar(c);
				temp = cur_inum;
				doing_cd = 1;
				parse();
				doing_cd = 0;
				if (nfiles != 1) {
					restore_inode(temp);
					if (!error) {
						print_path(input_path,
								input_pathp);
						if (nfiles == 0)
							printf(" not found\n");
						else
							printf(" ambiguous\n");
						error++;
					}
					continue;
				}
				restore_inode(filenames->ino);
				if ((mode = icheck(addr)) == 0)
					continue;
				if ((mode & IFMT) != IFDIR) {
					restore_inode(temp);
					print_path(input_path, input_pathp);
					printf(" not a directory\n");
					error++;
					continue;
				}
				for (i = 0; i <= top->len; i++)
					strcpy(current_path[i],
						top->fname[i]);
				current_pathp = (short)top->len;
				continue;
			}
			if (match("cg", 2)) {		/* cylinder group */
				if (type == NUMB)
					value = (long)addr;
				if (value > fs->fs_ncg - 1) {
					printf("highest cylinder group number is ");
					print(fs->fs_ncg - 1, 8, -8, 0);
					printf("\n");
					error++;
					continue;
				}
				type = objsz = CGRP;
				cur_cgrp = value;
				addr = (baddr_t)cgtod(fs, cur_cgrp) << FRGSHIFT;
				continue;
			}
			if (match("ct", 2)) {		/* creation time */
				acting_on_inode = 2;
				should_print = 1;
				addr = (baddr_t)
					&((struct dinode *)cur_ino)->di_ctime;
				value = get(LONG);
				type = NULL;
				continue;
			}
			goto bad_syntax;

		case 'd':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			if (match("directory", 2)) { 	/* directory offsets */
				if (type == NUMB)
					value = (long)addr;
				objsz = DIRECTORY;
				type = DIRECTORY;
				addr = getdirslot((short)value);
				continue;
			}
			if (match("db", 2)) {		/* direct block */
				acting_on_inode = 1;
				should_print = 1;
				if (type == NUMB)
					value = (long)addr;
				if ((value >= NDADDR) || (value < 0)) {
					printf("direct blocks are 0 to ");
					print(NDADDR - 1, 0, 0, 0);
					printf("\n");
					error++;
					continue;
				}
				addr = cur_ino;
				if (!icheck(addr))
					continue;
				addr = (baddr_t) &((struct dinode *)cur_ino)->di_db[value];
				addr += 1;	/* ex_bn offset */
				bod_addr = addr;
				cur_bytes = (value) * BLKSIZE;
				cur_block = value;
				type = BLOCK;
				dirslot = 0;
				eslotno = 0;
				value = get(NUMB);
				if (!value && !override) {
					printf("non existent block\n");
					error++;
				}
				continue;
			}
			goto bad_syntax;

		case 'f':
			if (colon) 
				colon = 0;
			else
				goto no_colon;
			if (match("find", 3)) {		/* find command */
				find();
				continue;
			}
			if (match("fragment", 2)) {	/* fragment conv. */
				printf("fragment: command not valid for EFS\n");
				goto bad_command;
#ifdef notdef
				if (type == NUMB) {
					value = addr;
					cur_bytes = 0;
					blocksize = FRGSIZE;
					filesize = FRGSIZE * 2;
				}
				if (min(blocksize, filesize) - cur_bytes >
							FRGSIZE) {
					blocksize = cur_bytes + FRGSIZE;
					filesize = blocksize * 2;
				}
				addr = (baddr_t)value << FRGSHIFT;
				bod_addr = addr;
				value = get(LONG);
				type = FRAGMENT;
				dirslot = 0;
				eslotno = 0;
				trapped++;
				continue;
#endif /* notdef */
			}
			if (match("file", 4)) {		/* access as file */
				acting_on_inode = 1;
				should_print = 1;
				if (type == NUMB)
					value = (long)addr;
				addr = cur_ino;
				if ((mode = icheck(addr)) == 0)
					continue;
				if (((mode & IFMT) != IFREG) && !override) {
					printf("special device\n");
					error++;
					continue;
				}
				if ((addr = ((baddr_t)bmap(value) << FRGSHIFT)) == 0)
					continue;
				cur_block = value;
				bod_addr = addr;
				type = BLOCK;
				dirslot = 0;
				eslotno = 0;
				continue;
			}
			if (match("fill", 4)) {		/* fill */
				if (getachar() != '=') {
					printf("missing '='\n");
					error++;
					continue;
				}
				if (objsz == INODE || objsz == DIRECTORY ||
					objsz == EXTENT) {
				      printf("can't fill inode or directory or extent\n");
				      error++;
				      continue;
				}
				fill();
				continue;
			}
			goto bad_syntax;

		case 'g':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			if (match("gen", 3)) {		/* generation number */
				acting_on_inode = 1;
				should_print = 1;
				addr = (baddr_t)
					&((struct dinode *)cur_ino)->di_gen;
				value = get(LONG);
				type = NULL;
				continue;
			}
			if (match("gid", 1)) {		/* group id */
				acting_on_inode = 1;
				should_print = 1;
				addr = (baddr_t)
					&((struct dinode *)cur_ino)->di_gid;
				value = get(SHORT);
				type = NULL;
				continue;
			}
			goto bad_syntax;

		case 'i':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			if (match("inode", 2)) { /* i# to inode conversion */
				if (c_count == 2) {
					addr = cur_ino;
					value = get(INODE);
					type = NULL;
					laststyle = '=';
					lastpo = 'i';
					should_print = 1;
					continue;
				}
				if (type == NUMB)
					value = (long)addr;
				addr = itob(value);
				if (!icheck(addr)) {
					if (override)
						cur_inum = value;
					continue;
				}
				cur_ino = addr;
				cur_inum = value;
				value = get(INODE);
				type = NULL;
				continue;
			}
			if (match("ib", 2)) {	/* indirect block/extent */
				printf("ib: command not valid for EFS\n");
				goto bad_command;
#ifdef notdef
				acting_on_inode = 1;
				should_print = 1;
				if (type == NUMB)
					value = addr;
				if (value >= NIADDR) {
					printf("indirect blocks are 0 to ");
					print(NIADDR - 1, 0, 0, 0);
					printf("\n");
					error++;
					continue;
				}
				/*
				 * Some error checking for nextents, whether
				 * there are any indirect extents etc
				 */
				addr = cur_ino;
				if (!icheck(addr))
					continue;
				addr = (baddr_t)
				      &((struct dinode *)cur_ino)->
				      di_u.di_extents[value];
				addr += 1;	/* ex_bn offset */
				bod_addr = addr;
				cur_bytes = 0;
				cur_block = 0;
				type = BLOCK;
				dirslot = 0;
				eslotno = 0;
				value = get(NUMB);
				if (!value && !override) {
					printf("non existent block\n");
					error++;
				}
				continue;
#endif /* notdef */
			}
			goto bad_syntax;

		case 'l':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			if (match("len", 3)) {		/* length of extent */
				acting_on_inode = 1;
				should_print = 1;
				if (type == NUMB)
					value = (long)addr;
				if ((value >= NDADDR) || (value < 0)) {
					printf("index for direct extents are 0 to ");
					print(NDADDR - 1, 0, 0, 0);
					printf("\n");
					error++;
					continue;
				}
				addr = cur_ino;
				if (!icheck(addr))
					continue;
				addr = (baddr_t) &((struct dinode *)cur_ino)->di_db[value];
				addr += 4;		/* ex_length offset */
				value = get(CHAR);
				type = NULL;
				continue;
			}
			if (match("ls", 2)) {		/* ls command */
				temp = cur_inum;
				recursive = long_list = 0;
				top = filenames - 1;
				for (;;) {
					eat_spaces();
					if ((c = getachar()) == '-') {
						if ((c = getachar()) == 'R') {
						  recursive = 1;
						  continue;
						} else if (c == 'l') {
						  long_list = 1;
						} else {
						  printf("unknown option ");
						  printf("'%c'\n", c);
						  error++;
						  break;
						}
					} else
						ungetachar(c);
					if ((c = getachar()) == '\n') {
						if (c_count != 2) {
							ungetachar(c);
							break;
						}
					}
					c_count++;
					ungetachar(c);
					parse();
					restore_inode(temp);
					if (error)
						break;
				}
				recursive = 0;
				if (error || nfiles == 0) {
					if (!error) {
						print_path(input_path,
								input_pathp);
						printf(" not found\n");
					}
					continue;
				}
				if (nfiles) {
				    cmp_level = 0;
				    qsort(filenames, nfiles,
					sizeof (struct filenames), ffcmp);
				    ls(filenames, filenames + (nfiles - 1), 0);
				} else {
				    printf("no match\n");
				    error++;
				}
				restore_inode(temp);
				continue;
			}
			if (match("ln", 2)) {		/* link count */
				acting_on_inode = 1;
				should_print = 1;
				addr = (baddr_t)
					&((struct dinode *)cur_ino)->di_nlink;
				value = get(SHORT);
				type = NULL;
				continue;
			}
			goto bad_syntax;

		case 'm':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			addr = cur_ino;
			if ((mode = icheck(addr)) == 0)
				continue;
			if (match("mt", 2)) { 		/* modification time */
				acting_on_inode = 2;
				should_print = 1;
				addr = (baddr_t)
					&((struct dinode *)cur_ino)->di_mtime;
				value = get(LONG);
				type = NULL;
				continue;
			}
			if (match("md", 2)) {		/* mode */
				acting_on_inode = 1;
				should_print = 1;
				addr = (baddr_t)
					&((struct dinode *)cur_ino)->di_mode;
				value = get(SHORT);
				type = NULL;
				continue;
			}
			if (match("maj", 2)) {	/* major device number */
				acting_on_inode = 1;
				should_print = 1;
				if (devcheck(mode))
					continue;
				addr = (baddr_t) &((struct dinode *)cur_ino)->di_db[0];
				value = get(CHAR);
				type = NULL;
				continue;
			}
			if (match("min", 2)) {	/* minor device number */
				acting_on_inode = 1;
				should_print = 1;
				if (devcheck(mode))
					continue;
				/*
				 * The minor number stored in the second byte
				 * of the first extent.
				 */
				addr = (baddr_t) &((struct dinode *)cur_ino)->di_db[0];
				addr += 1;	/* ex_bn offset */
				value = get(CHAR);
				type = NULL;
				continue;
			}
			goto bad_syntax;

		case 'n':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			if (match("nex", 3)) {		/* number of extents */
				acting_on_inode = 1;
				should_print = 1;
				addr = (baddr_t)
					&((struct dinode *)cur_ino)->di_numextents;
				value = get(SHORT);
				type = NULL;
				continue;
			}
			if (match("nm", 1)) {		/* directory name */
				struct efs_dirblk *db;

				objsz = DIRECTORY;
				acting_on_directory = 1;
				cur_dir = addr;
				if ((cptr = getblk(addr)) == 0)
					continue;
				db = (struct efs_dirblk *)(cptr +
						blkoff(fs, addr));

				/*
				 * EFS_SLOTAT could return zero or some bogus
				 * value - it should be in the range of
				 * 3-511
				 */
				chk_dirmagic(db);
				if (((gtemp = EFS_SLOTAT(db, dirslot)) == 0) ||
					(gtemp < 3) || (gtemp > 511)) {
					printf("Bad directory slot\n");
					continue;
				}
				dep = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db, dirslot));
				if (EFS_INUM_ISZERO(dep)) {
					printf("Inode number is zero\n");
					continue;
				}
				stringsize = dep ? STRINGSIZE(dep) : 0;
				addr += (baddr_t) ((caddr_t)&dep->d_name[0] -
					(caddr_t)db);
				type = NULL;
				continue;
			}
			goto bad_syntax;

		case 'o':
			if (colon) 
				colon = 0;
			else
				goto no_colon;
			if (match("off", 3)) {		/* logical extent off */
				acting_on_inode = 1;
				should_print = 1;
				if (type == NUMB)
					value = (long)addr;
				if ((value >= NDADDR) || (value < 0)) {
					printf("index for direct extents are 0 to ");
					print(NDADDR - 1, 0, 0, 0);
					printf("\n");
					error++;
					continue;
				}
				addr = cur_ino;
				if (!icheck(addr))
					continue;
				addr = (baddr_t) &((struct dinode *)cur_ino)->di_db[value];
				addr += 5;		/* ex_offset offset */
				value = get(NUMB);
				type = NULL;
				continue;
			}
			if (match("override", 1)) {	/* override flip flop */
				if (override = !override)
					printf("error checking off\n");
				else
					printf("error checking on\n");
				continue;
			}
			goto bad_syntax;

		case 'p':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			if (match("pwd", 2)) {		/* print working dir */
				print_path(current_path, current_pathp);
				printf("\n");
				continue;
			}
			if (match("prompt", 2)) {	/* change prompt */
				if ((c = getachar()) != '=') {
					printf("missing '='\n");
					error++;
					continue;
				}
				if ((c = getachar()) != '"') {
					printf("missing '\"'\n");
					error++;
					continue;
				}
				i = 0;
				prompt = &prompt[0];
				while ((c = getachar()) != '"' &&
				       c != '\n') {
					prompt[i++] = c;
					if (i >= PROMPTSIZE) {
						printf("string too long\n");
						error++;
						break;
					}
				}
				prompt[i] = '\0';
				continue;
			}
			goto bad_syntax;
				
		case 'q':
			if (!colon)
				goto no_colon;
			if (match("quit", 1)) {		/* quit */
				if ((c = getachar()) != '\n') {
					error++;
					continue;
				}
				exit(0);
			}
			goto bad_syntax;

		case 's':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			if (match("sb", 2)) {		/* super block */
				if (c_count == 2) {
					cur_sblock = 0;
					type = objsz = SB;
					laststyle = '=';
					lastpo = 's';
					should_print = 1;
					continue;
				}
				if (type == NUMB)
					value = (long)addr;
#ifdef notdef
				if (value > fs->fs_size - 1)
#endif /* notdef */
				if ((value > fs->fs_size - 1) && !(override)) {
					printf("super block beyond maxsize ");
					print(fs->fs_size, 8, -8, 0);
					printf("\n");
					error++;
					continue;
				}
				type = objsz = SB;
				cur_sblock = value;
				addr = (baddr_t)cur_sblock << FRGSHIFT;
				continue;
			}
			if (match("sz", 2)) {		/* file size */
				acting_on_inode = 1;
				should_print = 1;
				addr = (baddr_t)
					&((struct dinode *)cur_ino)->di_size;
				value = get(LONG);
				type = NULL;
				continue;
			}
			goto bad_syntax;

		case 'u':
			if (colon)
				colon = 0;
			else
				goto no_colon;
			if (match("uid", 1)) {		/* user id */
				acting_on_inode = 1;
				should_print = 1;
				addr = (baddr_t)
					&((struct dinode *)cur_ino)->di_uid;
				value = get(SHORT);
				type = NULL;
				continue;
			}
			goto bad_syntax;

		case 'F': /* buffer status (internal use only) */
			if (colon) 
				colon = 0;
			else
				goto no_colon;
			for (bp = bhdr.fwd; bp != &bhdr; bp = bp->fwd)
				printf("%8x %d\n",bp->blkno,bp->valid);
			printf("\n");
			printf("# commands\t\t%d\n", commands);
			printf("# read requests\t\t%d\n", read_requests);
			printf("# actual disk reads\t%d\n", actual_disk_reads);
			continue;
no_colon:
		printf("a colon should precede a command\n");
		error++;
		continue;
bad_syntax:
		printf("more letters needed to distinguish command\n");
		error++;
		continue;
bad_command:
		error++;
		continue;
		}
	}
}

/*
 * getachar - get next character from input buffer.
 */
static char
getachar(void)
{
	return(input_buffer[input_pointer++]);
}

/*
 * ungetachar - return character to input buffer.
 */
static void
ungetachar(char c)
{
	if (input_pointer == 0) {
		printf("internal problem maintaining input buffer\n");
		error++;
		return;
	}
	input_buffer[--input_pointer] = c;
}

/*
 * getnextinput - display the prompt and read an input line.
 *	An input line is up to 128 characters terminated by the newline
 *	character.  Handle overflow, shell escape, and eof.
 */
static void
getnextinput(void)
{
	int	i;
	char	c;
	pid_t	pid, rpid;
	int	retcode;

newline:
	i = 0;
	printf("%s", prompt);
ignore_eol:
	while ((c = getc(stdin)) != '\n' && !(c == '!' && i == 0) &&
	       !feof(stdin) && i <= INPUTBUFFER - 2)
		input_buffer[i++] = c;
	if (input_buffer[i - 1] == '\\') {
		input_buffer[i++] = c;
		goto ignore_eol;
	}
	if (feof(stdin)) {
		printf("\n");
		exit(0);
	}
	if (c == '!') {
		if ((pid = fork()) == 0) {
			execl("/bin/sh", "sh", "-t", 0);
			error++;
			return;
		}
		while ((rpid = wait(&retcode)) != pid && rpid != -1)
			;
		printf("!\n");
		goto newline;
	}
	if (c != '\n')
		printf("input truncated to 128 characters\n");
	input_buffer[i] = '\n';
	input_pointer = 0;
}

/*
 * eat_spaces - read extraneous spaces.
 */
static void
eat_spaces(void)
{
	char	c;

	while ((c = getachar()) == ' ')
		;
	ungetachar(c);
}

/*
 * restore_inode - set up all inode indicators so inum is now
 *	the current inode.
 */
static void
restore_inode(long inum)
{
	errinum = cur_inum = inum;
	addr = errino = cur_ino = itob(inum);
}

/*
 * match - return false if the input does not match string up to
 *	upto letters.   Then proceed to chew up extraneous letters.
 */
static int
match(char *string, int upto)
{
	int	i, length = strlen(string) - 1;
	char	c;
	int	save_upto = upto;

	while (--upto) {
		string++;
		if ((c = getachar()) != *string) {
			for (i = save_upto - upto; i; i--) {
				ungetachar(c);
				c = *--string;
			}
			return(0);
		}
		length--;
	}
	while (length--) {
		string++;
		if ((c = getachar()) != *string) {
			ungetachar(c);
			return(1);
		}
	}
	return(1);
}

/*
 * expr - expression evaluator.  Will evaluate expressions from
 *	left to right with no operator precedence.  Parentheses may
 *	be used.
 */
static baddr_t
expr(void)
{
	baddr_t	numb = 0, temp;
	char	c;

	numb = term();
	for (;;) {
		if (error)
			return(0);
		c = getachar();
		switch (c) {

		case '+':
			numb += term();
			continue;

		case '-':
			numb -= term();
			continue;

		case '*':
			numb *= term();
			continue;

		case '%':
			temp = term();
			if (!temp) {
				printf("divide by zero\n");
				error++;
				return(0);
			}
			numb /= temp;
			continue;

		case ')':
			paren--;
			return(numb);

		default:
			ungetachar(c);
			if (paren && !error) {
				printf("missing ')'\n");
				error++;
			}
			return(numb);
		}
	}
}

/*
 * term - used by expression evaluator to get an operand.
 */
static baddr_t
term(void)
{
	char	c;

	switch (c = getachar()) {

	default:
		ungetachar(c);

	case '+':
		return(getnumb());

	case '-':
		return(-getnumb());

	case '(':
		paren++;
		return(expr());
	}
}

/*
 * getnumb - read a number from the input stream.  A leading
 *	zero signifies octal interpretation, a leading '0x'
 *	signifies hexadecimal, and a leading '0t' signifies
 *	decimal.  If the first character is a character,
 *	return an error.
 */
static baddr_t
getnumb(void)
{
	char	c, savec;
	baddr_t	number = 0;
	long	tbase, num;

	c = getachar();
	if (!digit(c)) {
		error++;
		ungetachar(c);
		return(-1);
	}
	if (c == '0') {
		tbase = OCTAL;
		if ((c = getachar()) == 'x')
			tbase = HEX;
		else if (c == 't')
			tbase = DECIMAL;
		else ungetachar(c);
	} else {
		tbase = base;
		ungetachar(c);
	}
	for (;;) {
		num = tbase;
		c = savec = getachar();
		if (HEXLETTER(c))
			c = uppertolower(c);
		switch (tbase) {
		case HEX:
			if (hexletter(c)) {
				num = hextodigit(c);
				break;
			}
		case DECIMAL:
			if (digit(c))
				num = numtodigit(c);
			break;
		case OCTAL:
			if (octaldigit(c))
				num = numtodigit(c);
			break;
		}
		if (num == tbase)
			break;
		number = number * tbase + num;
	}
	ungetachar(savec);
	return(number);
}

/*
 * find - the syntax is almost identical to the unix command.
 *		find dir [-name pattern] [-inum number]
 *	Note:  only one of -name or -inum may be used at a time.
 *	       Also, the -print is not needed (implied).
 */
static void
find(void)
{
	struct filenames	*fn;
	char			c;
	long			temp;
	short			mode;

	eat_spaces();
	temp = cur_inum;
	top = filenames - 1;
	doing_cd = 1;
	parse();
	doing_cd = 0;
	if (nfiles != 1) {
		restore_inode(temp);
		if (!error) {
			print_path(input_path, input_pathp);
			if (nfiles == 0)
				printf(" not found\n");
			else
				printf(" ambiguous\n");
			error++;
			return;
		}
	}
	restore_inode(filenames->ino);
	freemem(filenames, nfiles);
	nfiles = 0;
	top = filenames - 1;
	if ((mode = icheck(addr)) == 0)
		return;
	if ((mode & IFMT) != IFDIR) {
		print_path(input_path, input_pathp);
		printf(" not a directory\n");
		error++;
		return;
	}
	eat_spaces();
	if ((c = getachar()) != '-') {
		printf("missing '-'\n");
		error++;
		return;
	}
	find_by_name = find_by_inode = 0;
	c = getachar();
	if (match("name", 4)) {
		eat_spaces();
		find_by_name = 1;
	} else if (match("inum", 4)) {
		eat_spaces();
		find_ino = (long)expr();
		if (error)
			return;
		while ((c = getachar()) != '\n')
			;
		ungetachar(c);
		find_by_inode = 1;
	} else {
		printf("use -name or -inum with find\n");
		error++;
		return;
	}
	doing_find = 1;
	parse();
	doing_find = 0;
	if (error) {
		restore_inode(temp);
		return;
	}
	for (fn = filenames; fn <= top; fn++) {
		if (fn->find == 0)
			continue;
		printf("i#: ");
		print(fn->ino, 12, -8, 0);
		print_path(fn->fname, (short)fn->len);
		printf("\n");
	}
	restore_inode(temp);
}

/*
 * ls - do an ls.  Should behave exactly as ls(1).
 *	Only -R and -l is supported and -l gives different results.
 */
static void
ls(struct filenames *fn0, struct filenames *fnlast, short level)
{
	struct filenames	*fn, *fnn;

	fn = fn0;
	for (;;) {
		fn0 = fn;
		if (fn0->len) {
			cmp_level = level;
			qsort(fn0, fnlast - fn0 + 1,
				sizeof (struct filenames), fcmp);
		}
		for (fnn = fn, fn++; fn <= fnlast; fnn = fn, fn++) {
			if (fnn->len != fn->len && level == fnn->len - 1)
				break;
			if (fnn->len == 0)
				continue;
			if (strcmp(fn->fname[level], fnn->fname[level]))
				break;
		}
		if (fn0->len && level != fn0->len - 1)
			ls(fn0, fnn, level + 1);
		else {
			if (fn0 != filenames)
				printf("\n");
			print_path(fn0->fname, (short)(fn0->len - 1));
			printf(":\n");
			if (fn0->len == 0)
				cmp_level = level;
			else
				cmp_level = level + 1;
			qsort(fn0, fnn - fn0 + 1,
				sizeof (struct filenames), fcmp);
			formatf(fn0, fnn);
			nfiles -= fnn - fn0 + 1;
		}
		if (fn > fnlast)
			return;
	}
}

/*
 * formatf - code lifted from ls.
 */
static void
formatf(struct filenames *fn0, struct filenames *fnlast)
{
	struct filenames	*fn;
	int			width = 0, w, nentry = fnlast - fn0 + 1;
	int			i, j, columns, lines;
	char			*cp;

	if (long_list) {
		columns = 1;
	} else {
		for (fn = fn0; fn <= fnlast; fn++) {
			int len = strlen(fn->fname[cmp_level]) + 2;

			if (len > width)
				width = len;
		}
		width = (width + 8) &~ 7;
		columns = 80 / width;
		if (columns == 0)
			columns = 1;
	}
	lines = (nentry + columns - 1) / columns;
	for (i = 0; i < lines; i++) {
		for (j = 0; j < columns; j++) {
			fn = fn0 + j * lines + i;
			if (long_list) {
				printf("i#: ");
				print(fn->ino, 12, -8, 0);
			}
			cp = fmtentry(fn);
			printf("%s", cp);
			if (fn + lines > fnlast) {
				printf("\n");
				break;
			}
			w = strlen(cp);
			while (w < width) {
				w = (w + 8) &~ 7;
				putchar('\t');
			}
		}
	}
}

/*
 * fmtentry - code lifted from ls.
 */
static char *
fmtentry(struct filenames *fn)
{
	static char	fmtres[BUFSIZ];
	struct dinode	*ip;
	char		*cptr, *cp, *dp;

	dp = &fmtres[0];
	for (cp = fn->fname[cmp_level]; *cp; cp++) {
		if (*cp < ' ' || *cp >= 0177)
			*dp++ = '?';
		else
			*dp++ = *cp;
	}
	addr = itob(fn->ino);
	if ((cptr = getblk(addr)) == 0)
		return(NULL);
	cptr += blkoff(fs, addr);
	ip = (struct dinode *)cptr;
	switch (ip->di_mode & IFMT) {
	case IFDIR:
		*dp++ = '/';
		break;
	case IFLNK:
		*dp++ = '@';
		break;
	case IFSOCK:
		*dp++ = '=';
		break;
	case IFIFO:
		*dp++ = 'f';
		break;
	case IFCHR:
	case IFBLK:
	case IFREG:
		if (ip->di_mode & 0111)
			*dp++ = '*';
		else
			*dp++ = ' ';
		break;
	default:
		*dp++ = '?';
		break;
	}
	*dp++ = 0;
	return (fmtres);
}

/*
 * fcmp - routine used by qsort.  Will sort first by name, then
 *	then by pathname length if names are equal.  Uses global
 *	cmp_level to tell what component of the path name we are comparing.
 */
static int
fcmp(const void *af1, const void *af2)
{
	struct filenames	*f1 = (struct filenames *)af1;
	struct filenames	*f2 = (struct filenames *)af2;
	int 			fvalue;

	if ((fvalue = strcmp(f1->fname[cmp_level], f2->fname[cmp_level])))
		return(fvalue);
	return (f1->len - f2->len);
}

/*
 * ffcmp - routine used by qsort.  Sort only by pathname length.
 */
static int
ffcmp(const void *af1, const void *af2)
{
	struct filenames	*f1 = (struct filenames *)af1;
	struct filenames	*f2 = (struct filenames *)af2;

	return (f1->len - f2->len);
}

/*
 * parse - set up the call to follow_path.
 */
static void
parse(void)
{
	int	i;
	char	c;

	stack_pathp = input_pathp = -1;
	if ((c = getachar()) == '/') {
		while ((c = getachar()) == '/')
			;
		ungetachar(c);
		cur_inum = 2;
		if ((c = getachar()) == '\n') {
			ungetachar('\n');
			if (doing_cd) {
				top++;
				top->ino = 2;
				top->len = -1;
				nfiles = 1;
				return;
			}
		} else
			ungetachar(c);
	} else {
		ungetachar(c);
		stack_pathp = current_pathp;
		if (!doing_find)
			input_pathp = current_pathp;
		for (i = 0; i <= current_pathp; i++) {
			if (!doing_find)
				strcpy(input_path[i], current_path[i]);
			strcpy(stack_path[i], current_path[i]);
		}
	}
	getname();
	follow_path(stack_pathp + 1, cur_inum);
}

/*
 * follow_path - called by cd, find, and ls.
 *	input_path holds the name typed by the user.
 *	stack_path holds the name at the current depth.
 */
static void
follow_path(long level, long inum)
{
	struct efs_dent		*dep;
	char			**ccptr, *cptr;
	int			i;
	struct filenames	*tos, *bos, *fn, *fnn, *fnnn;
	long			block;
	short			mode;
	struct efs_dirblk	*db;
	int			slotno = 0;
	int			nextblk = 0;

	tos = top + 1;
	restore_inode(inum);
	if ((mode = icheck(addr)) == 0)
		return;
	if ((mode & IFMT) != IFDIR)
	    return;
	block = cur_bytes = 0;
	while (cur_bytes < filesize) {
	    if (block == 0 || bcomp(addr) || nextblk) {
		nextblk = 0;
		error = 0;
		if ((addr = ((baddr_t)bmap(block++) << FRGSHIFT)) == 0)
		    break;
		if ((cptr = getblk(addr)) == 0)
		    break;
		cptr += blkoff(fs, addr);
		slotno = 0;
		db = (struct efs_dirblk*)cptr;
		chk_dirmagic(db);
	    }
	    db = (struct efs_dirblk*)cptr;
	    if (((gtemp = EFS_SLOTAT(db, slotno)) == 0) ||
		    (gtemp < 3) || (gtemp > 511))
		    goto duplicate;
	    dep = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db, slotno));
	    if (EFS_GET_INUM(dep)) {
		char buffer[MAXPATHLEN];

		bcopy(&dep->d_name[0], buffer, dep->d_namelen);
		buffer[dep->d_namelen] = '\0';
		if (level > input_pathp || doing_find ||
			compare(input_path[level], buffer, 1)) {
		    if (++top - filenames >= MAXFILES) {
			printf("too many files\n");
			error++;
			return;
		    }
		    top->fname = (char **)calloc(FIRST_DEPTH, sizeof (char **));
		    top->flag = 0;
		    if (top->fname == 0) {
			printf("out of memory\n");
			error++;
			return;
		    }
		    nfiles++;
		    top->ino = EFS_GET_INUM(dep);
		    top->len = stack_pathp;
		    top->find = 0;
		    if (doing_find) {
			if (find_by_name) {
			    if (compare(input_path[0], buffer, 1))
				top->find = 1;
			} else if (find_by_inode)
			    if (find_ino == EFS_GET_INUM(dep))
				top->find = 1;
		    }
		    if (top->len + 1 >= FIRST_DEPTH && top->flag == 0) {
			ccptr = (char **)calloc(SECOND_DEPTH, sizeof (char **));
			if (ccptr == 0) {
			    printf("out of memory\n");
			    error++;
			    return;
			}
			for (i = 0; i < FIRST_DEPTH; i++)
				ccptr[i] = top->fname[i];
			free((char *)top->fname);
			top->fname = ccptr;
			top->flag = 1;
		    }
		    if (top->len >= SECOND_DEPTH) {
			printf("maximum depth exceeded, try to cd lower\n");
			error++;
			return;
		    }
		    /*
		     * Copy current depth.
		     */
		    for (i = 0; i <= stack_pathp; i++) {
			top->fname[i]=calloc(1, strlen(stack_path[i])+1);
			if (top->fname[i] == 0) {
			    printf("out of memory\n");
			    error++;
			    return;
			}
			strcpy(top->fname[i], stack_path[i]);
		    }
		    /*
		     * Check for '.' or '..' typed.
		     */
		    if ((level <= input_pathp) &&
				       (strcmp(input_path[level], ".") == 0 ||
					strcmp(input_path[level], "..") == 0)) {
			if (strcmp(input_path[level],"..") == 0 &&
							 top->len >= 0) {
			    free(top->fname[top->len]);
			    top->len -= 1;
			}
		    } else {
			/*
			 * Check for duplicates.
			 */

			if (!doing_cd && !doing_find) {
			    for (fn = filenames; fn < top; fn++) {
				if (fn->ino == EFS_GET_INUM(dep) &&
					    fn->len == stack_pathp + 1) {
				    for (i = 0; i < fn->len; i++)
					if (strcmp(fn->fname[i], stack_path[i]))
					    break;
				    bcopy(&dep->d_name[0], buffer,
						dep->d_namelen);
				    buffer[dep->d_namelen] = '\0';
				    if (i != fn->len ||
					    strcmp(fn->fname[i], buffer))
					continue;
				    freemem(top, 1);
				    if (top == filenames)
					top = NULL;
				    else
					top--;
					nfiles--;
					goto duplicate;
				}
			    }
			}
			top->len += 1;
			top->fname[top->len] = calloc(1, dep->d_namelen+1);
			if (top->fname[top->len] == 0) {
			    printf("out of memory\n");
			    error++;
			    return;
			}
			bcopy(&dep->d_name[0], top->fname[top->len],
				dep->d_namelen);
			top->fname[top->len][dep->d_namelen] = '\0';
		    }
		}
	    }
duplicate:
	    slotno++;
	    if (slotno >= db->slots) {
		nextblk++;
		cur_bytes += BBSIZE;
		addr += BBSIZE;
		cptr += BBSIZE;
	    }
	}
	if (top < filenames)
	    return;
	if ((doing_cd && level == input_pathp) ||
		(!recursive && !doing_find && level > input_pathp))
	    return;
	bos = top;
	/*
	 * Check newly added entries to determine if further expansion
	 * is required.
	 */
	for (fn = tos; fn <= bos; fn++) {
	    /*
	     * Avoid '.' and '..' if beyond input.
	     */
	    if ((recursive || doing_find) && (level > input_pathp) &&
		(strcmp(fn->fname[fn->len], ".") == 0 ||
		 strcmp(fn->fname[fn->len], "..") == 0))
		 continue;
	    restore_inode(fn->ino);
	    if ((mode = icheck(cur_ino)) == 0)
		return;
	    if ((mode & IFMT) == IFDIR || level < input_pathp) {
		/*
		 * Set up current depth, remove current entry and
		 * continue recursion.
		 */
		for (i = 0; i <= fn->len; i++)
		    strcpy(stack_path[i], fn->fname[i]);
		stack_pathp = (short)fn->len;
		if (!doing_find &&
			(!recursive || (recursive && level <= input_pathp))) {
		    /*
		     * Remove current entry by moving others up.
		     */
		    freemem(fn, 1);
		    fnn = fn;
		    for (fnnn = fnn, fnn++; fnn <= top; fnnn = fnn, fnn++) {
			fnnn->ino = fnn->ino;
			fnnn->len = fnn->len;
			if (fnnn->len + 1 < FIRST_DEPTH) {
			    fnnn->fname = (char **)calloc(FIRST_DEPTH,
							sizeof (char **));
			    fnnn->flag = 0;
			} else if (fnnn->len < SECOND_DEPTH) {
			    fnnn->fname = (char **)calloc(SECOND_DEPTH,
							sizeof (char **));
			    fnnn->flag = 1;
			} else {
			    printf("maximum depth exceeded, ");
			    printf("try to cd lower\n");
			    error++;
			    return;
			}
			for (i = 0; i <= fnn->len; i++)
			    fnnn->fname[i] = fnn->fname[i];
		    }
		    if (fn == tos)
			fn--;
		    top--;
		    bos--;
		    nfiles--;
		}
		follow_path(level + 1, cur_inum);
		if (error)
			return;
	    }
	}
}

/*
 * getname - break up the pathname entered by the user into components.
 */
static void
getname(void)
{
	int	i;
	char	c;

	if ((c = getachar()) == '\n') {
	    ungetachar(c);
	    return;
	}
	ungetachar(c);
	input_pathp++;
gclear:
	for (i = 0; i < MAXNAMLEN; i++)
	    input_path[input_pathp][i] = '\0';
	for (;;) {
	    c = getachar();
	    if (c == '\\') {
		if (strlen(input_path[input_pathp]) + 1 >= MAXNAMLEN) {
		    printf("maximum name length exceeded, ");
		    printf("truncating\n");
		    return;
		}
		input_path[input_pathp][strlen(input_path[input_pathp])] = c;
		input_path[input_pathp][strlen(input_path[input_pathp])] =
						getachar();
		continue;
	    }
	    if (c == ' ' || c == '\n') {
		ungetachar(c);
		return;
	    }
	    if (!doing_find && c == '/') {
		if (++input_pathp >= MAXPATHLEN) {
		    printf("maximum path length exceeded, ");
		    printf("truncating\n");
		    input_pathp--;
		    return;
		}
		goto gclear;
	    }
	    if (strlen(input_path[input_pathp]) >= MAXNAMLEN) {
		printf("maximum name length exceeded, truncating\n");
		return;
	    }
	    input_path[input_pathp][strlen(input_path[input_pathp])] = c;
	}
}

/*
 * compare - check if a filename matches the pattern entered by the user.
 *	Handles '*', '?', and '[]'.
 */
static int
compare(char *s1, char *s2, short at_start)
{
	char	c, *s;

	s = s2;
	while (c = *s1) {
		if (c == '*') {
			if (at_start && s == s2 && !letter(*s2) && !digit(*s2))
				return(0);
			if (*++s1 == 0)
				return(1);
			while (*s2) {
				if (compare(s1, s2, 0))
					return(1);
				if (error)
					return(0);
				s2++;
			}
		}
		if (*s2 == 0)
			return(0);
		if (c == '\\') {
			s1++;
			goto compare_chars;
		}
		if (c == '?') {
			if (at_start && s == s2 && !letter(*s2) && !digit(*s2))
				return(0);
			s1++;
			s2++;
			continue;
		}
		if (c == '[') {
			s1++;
			if (*s2 >= *s1++) {
				if (*s1++ != '-') {
					printf("missing '-'\n");
					error++;
					return(0);
				}
				if (*s2 <= *s1++) {
					if (*s1++ != ']') {
						printf("missing ']'");
						error++;
						return(0);
					}
					s2++;
					continue;
				}
			}
		}
compare_chars:
		if (*s1++ == *s2++)
			continue;
		else
			return(0);
	}
	if (*s1 == *s2)
		return(1);
	return(0);
}

/*
 * freemem - free the memory allocated to the filenames structure.
 */
static void
freemem(struct filenames *p, int numb)
{
	int	i, j;

	if (numb == 0)
		return;
	for (i = 0; i < numb; i++, p++) {
		for (j = 0; j <= p->len; j++)
			free(p->fname[j]);
		free((char *)p->fname);
	}
}

/*
 * print_path - print the pathname held in p.
 */
static void
print_path(char *p[], short pntr)
{
	int	i;

	printf("/");
	if (pntr >= 0) {
		for (i = 0; i < pntr; i++)
			printf("%s/", p[i]);
		printf("%s", p[pntr]);
	}
}

/*
 * fill - fill a section with a value or string.
 *	addr,count:fill=[value, "string"].
 */
static void
fill(void)
{
	char	*cptr;
	int	i;
	short	eof_flag, end = 0, eof = 0;
	long	temp, tcount;
	baddr_t	taddr;

	if (!wrtflag) {
		printf("not opened for write '-w'\n");
		error++;
		return;
	}
	temp = (long)expr();
	if (error)
		return;
	if ((cptr = getblk(addr)) == 0)
		return;
	if (type == NUMB)
		eof_flag = 0;
	else
		eof_flag = 1;
	taddr = addr;
	switch (objsz) {
	case LONG:
		addr &= ~(LONG - 1LL);
		break;
	case SHORT:
		addr &= ~(SHORT - 1LL);
		temp &= 0177777L;
		break;
	case CHAR:
		temp &= 0377;
		break;
	}
	cur_bytes -= taddr - addr;
	cptr += blkoff(fs, addr);
	tcount = check_addr(eof_flag, &end, &eof, 0);
	for (i = 0; i < tcount; i++) {
		switch (objsz) {
		case LONG:
			*(long *)cptr = temp;
			break;
		case SHORT:
			*(short *)cptr = (short)temp;
			break;
		case CHAR:
			*cptr = (char)temp;
			break;
		}
		cptr += objsz;
	}
	addr += (tcount - 1) * objsz;
	cur_bytes += (tcount - 1) * objsz;
	put(temp, objsz);
	if (eof) {
		printf("end of file\n");
		error++;
	} else if (end) {
		printf("end of block\n");
		error++;
	}
}
	
/*
 * get - read a byte, short or long from the file system.
 *	The entire block containing the desired item is read
 *	and the appropriate data is extracted and returned. 
 */
static long
get(short lngth)
{

	char	*bptr;
	baddr_t	temp = addr;
	u_char	*tptr;

	objsz = lngth;
	if (objsz == INODE || objsz == SHORT)
		temp &= ~(SHORT - 1LL);
	else if (objsz == DIRECTORY || objsz == LONG)
		temp &= ~(LONG - 1LL);
	else if (objsz == EXTENT)
		temp &= ~(EXTENT - 1LL);
	if ((bptr = getblk(temp)) == 0)
		return(-1);
	bptr += blkoff(fs, temp);
	switch (objsz) {
	case CHAR:
		return((long)*bptr);
	case SHORT:
	case INODE:
		return((long)(*(short *)bptr));
	case LONG:
	case DIRECTORY:
	case EXTENT:
		return(*(long *)bptr);
	case NUMB:
		tptr = (u_char *)bptr;
		return((*tptr << 16) + (*(tptr+1) << 8) + *(tptr+2));
	default:
		printf("Unknown size %d\n", objsz);
		error++;
		break;
	}
	return(0);
}

/*
 * cgrp_check - make sure that we don't bump the cylinder group
 *	beyond the total number of cylinder groups or before the start.
 */
static baddr_t
cgrp_check(long cgrp)
{
	if (cgrp < 0) {
	    printf("beginning of cylinder groups\n");
	    error++;
	    return(0);
	}
	if (cgrp >= fs->fs_ncg) {
	    printf("end of cylinder groups\n");
	    error++;
	    return(0);
	}
	if (objsz == CGRP)
	    return((baddr_t)cgtod(fs, cgrp) << FRGSHIFT);
	return(0);
}

/*
 * icheck -  make sure we can read the block containing the inode
 *	and determine the filesize (0 if inode not allocated).  Return
 *	0 if error otherwise return the mode.
 * Currently this semantic of icheck results in the ability to step through
 * inodes use '\n', '+' or '-' till we hit an unallocated disk inode. These
 * unallocated disk inodes can only be viewed if the -o option is used.
 */
static int
icheck(baddr_t address)
{
	char		*cptr;
	struct dinode	*ip;

	if (address & (INODE - 1LL)) {
		printf("illegal inode address - not aligned\n");
		error++;
		return(0);
	}
	if ((cptr = getblk(address)) == 0)
		return(0);
	cptr += blkoff(fs, address);
	ip = (struct dinode *)cptr;
	if ((ip->di_mode & IFMT) == 0) {
		printf("i#: ");
		print(efs_addrtoinum(address),12,-8,0);
		printf("is not allocated on disk\n");
		if (!override) {
			error++;
			return(0);
		}
		blocksize = filesize = 0;
	} else {
		trapped++;
		filesize = ip->di_size;
		blocksize = filesize * 2;
	}
	return(ip->di_mode);
}

/*
 * getdirslot - get the address of the directory slot desired.
 */
static baddr_t
getdirslot(short slot)
{
	char			*cptr;
	struct efs_dent		*dep = NULL;
	short			i;
	char			*string = &scratch[0];
	short			bod = 0, mode, temp;
	struct efs_dirblk	*db = NULL;
	int			slotno;

	if (slot < 0) {
		slot = 0;
		bod++;
	}
	if (type != DIRECTORY) {
		/*
		 * XXX - use bmap and block no as in follow_path for dir blks?
		 */
		if (type == BLOCK)
			string = "block";
		else
			string = "fragment";
		addr = bod_addr;
		if ((cptr = getblk(addr)) == 0)
			return(0);
		cptr += blkoff(fs, addr);
		cur_bytes = 0;
		db = (struct efs_dirblk*)cptr;
		slotno = 0;
		chk_dirmagic(db);
		if (((gtemp = EFS_SLOTAT(db, slotno)) == 0) ||
			(gtemp < 3) || (gtemp > 511))
			dep = NULL;
		else
			dep = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db, slotno));
		for (dirslot = 0; (dirslot < slot) && (slotno < db->slots); 
				dirslot++) {
			if (((gtemp = EFS_SLOTAT(db, slotno)) == 0) ||
				(gtemp < 3) || (gtemp > 511)) {
				slotno++;
				dep = NULL;
				goto down;
			}
			dep = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db, slotno));
			slotno++;
			if (blocksize > filesize) {
				if ((slotno > db->slots) &&
				    (cur_bytes + BBSIZE >= filesize)) {
					printf("end of file\n");
					erraddr = addr;
					errcur_bytes = cur_bytes;
					stringsize = dep ? STRINGSIZE(dep) : 0;
					error++;
					return(addr);
				}
			} else {
				if ((slotno > db->slots) &&
				    (cur_bytes + BBSIZE >= blocksize)) {
					printf("end of %s\n", string);
					erraddr = addr;
					errcur_bytes = cur_bytes;
					stringsize = dep ? STRINGSIZE(dep) : 0;
					error++;
					return(addr);
				}
			}
down:
			cptr = (char *)dep;
			if (slotno >= db->slots) {
			    cur_bytes += BBSIZE;
			    addr += BBSIZE;
			}
		}
		if (bod) {
			if (blocksize > filesize)
				printf("beginning of file\n");
			else
				printf("beginning of %s\n", string);
			erraddr = addr;
			errcur_bytes = cur_bytes;
			error++;
		}
		stringsize = dep ? STRINGSIZE(dep) : 0;
		return(addr);
	} else {
		addr = cur_ino;
		if ((mode = icheck(addr)) == 0)
			return(0);
		if (!override && ((mode & IFMT) != IFDIR)) {
			printf("inode is not a directory\n");
			error++;
			return(0);
		}
		slotno = 0;
		temp = slot;
		i = 0;
		cur_bytes = 0;
		for (;;) {
			if (cur_bytes >= filesize) {
				addr = 0;
				break;
			}
			if (i == 0 || bcomp(addr) || 
			    (slotno >= (ulong)db->slots)) {
				error = 0;
				if ((addr=((baddr_t)bmap(i++) << FRGSHIFT)) == 0)
					break;
				if ((cptr = getblk(addr)) == 0)
					break;
				cptr += blkoff(fs, addr);
				{
				    /*
				     * make sure that this dir block is okay
				     */
				    db = (struct efs_dirblk *)cptr;
				    slotno = 0;
				    if ((db->magic != EFS_DIRBLK_MAGIC) ||
					(db->slots > EFS_MAXENTS) ||
					(EFS_DIR_FREEOFF(db->firstused) <
					 (EFS_DIRBLK_HEADERSIZE + db->slots))) {
					printf("bad directory block\n");
					stringsize = dep ? STRINGSIZE(dep):0;
					goto efs_direrr;
				    }
				}
			}
			if (((gtemp = EFS_SLOTAT(db, slotno)) == 0) ||
				(gtemp < 3) || (gtemp > 511)) {
				slotno++;
				dep = NULL;
				goto down2;
			}
			dep = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db, slotno));
			slotno++;
			if ((value = EFS_GET_INUM(dep)) == 0)
				goto efs_direrr;
			if (!temp--)
				break;
down2:
			if ((slotno >= db->slots) && 
			    (cur_bytes + BBSIZE >= filesize)) {
				printf("end of file\n");
				stringsize = dep ? STRINGSIZE(dep) : 0;
efs_direrr:
				dirslot = slot - temp - 1;
				objsz = DIRECTORY;
				erraddr = addr;
				errcur_bytes = cur_bytes;
				error++;
				return(addr);
			}
			cptr = (char *)dep;
			if (slotno >= db->slots) {
			    cur_bytes += BBSIZE;
			    addr += BBSIZE;
			}
		}
		dirslot = slot;
		objsz = DIRECTORY;
		if (bod) {
			printf("beginning of file\n");
			erraddr = addr;
			errcur_bytes = cur_bytes;
			error++;
		}
		stringsize = dep ? STRINGSIZE(dep) : 0;
		return(addr);
	}
}

/*
 * putf - print a byte as an ascii character if possible.
 *	The exceptions are tabs, newlines, backslashes
 *	and nulls which are printed as the standard C
 *	language escapes. Characters which are not
 *	recognized are printed as \?.
 */
static void
putf(char c)
{

	if (c<=037 || c>=0177 || c=='\\') {
		printf("\\");
		switch (c) {
		case '\\':
			printf("\\");
			break;
		case '\t':
			printf("t");
			break;
		case '\n':
			printf("n");
			break;
		case '\0':
			printf("0");
			break;
		default:
			printf("?");
			break;
		}
	}
	else {
		printf("%c", c);
		printf(" ");
	}
}

/*
 * put - write an item into the buffer for the current address
 *	block.  The value is checked to make sure that it will
 *	fit in the size given without truncation.  If successful,
 *	the entire block is written back to the file system.
 */
static int
put(long item, short lngth)
{

	char	*bptr, *sbptr;
	long	olditem;

	if (!wrtflag) {
		printf("not opened for write '-w'\n");
		error++;
		return(0);
	}
	objsz = lngth;
	if ((sbptr = getblk(addr)) == 0)
		return(0);
	bptr = sbptr + blkoff(fs, addr);
	/*
	 * Align the address on a proper boundary.
	 */
	switch (objsz) {
	case LONG:
	case EXTENT:
	case DIRECTORY:
		olditem = *(long *)bptr;
		*(long *)bptr = item;
		break;
	case SHORT:
	case INODE:
		olditem = (long)*(short *)bptr;
		item &= 0177777L;
		*(short *)bptr = (short)item;
		break;
	case CHAR:
		olditem = (long)*bptr;
		item &= 0377;
		*bptr = lobyte(loword(item));
		break;
	case NUMB:
		olditem = *(long *)((long)bptr & ~0x3);
		item = (long)((olditem & 0xff000000) | (item & 0x00ffffff));
		*(long *)((long)bptr & ~0x3) = item;
		break;
	default:
		printf("Unknown size %d\n", objsz);
		error++;
		return(0);
	}
	if (syssgi(SGI_WRITEB, fd, sbptr, lblkno(fs, addr), 1) != 1) {
		error++;
		fflush(stdout);
		perror("write error");
		fprintf(stderr, "           : block  = %x\n",lblkno(fs, addr));
		fprintf(stderr, "           : nbytes = %x\n", BBSIZE);
		fflush(stderr);
		return(0);
	}
	if (!acting_on_inode && objsz != INODE && objsz != DIRECTORY &&
		objsz != EXTENT) {
		myindex(base);
		print(olditem, 8, -8, 0);
		printf("\t=\t");
		print(item, 8, -8, 0);
		printf("\n");
	} else {
		if (objsz == DIRECTORY) {
			addr = cur_dir;
			fprnt('?', 'd');
		} else if (objsz == EXTENT) {
			fprnt('?', 'e');
		} else {
			addr = cur_ino;
			objsz = INODE;
			fprnt('?', 'i');
		}
	}
	return(0);
}

/*
 * getblk - check if the desired block is in the file system.
 *	Search the incore buffers to see if the block is already
 *	available. If successful, unlink the buffer control block
 *	from its position in the buffer list and re-insert it at
 *	the head of the list.  If failure, use the last buffer
 *	in the list for the desired block. Again, this control
 *	block is placed at the head of the list. This process
 *	will leave commonly requested blocks in the in-core buffers.
 *	Finally, a pointer to the buffer is returned.
 */
static char *
getblk(baddr_t address)
{

	struct buf		*bp;
	unsigned long		block;

	read_requests++;
	block = lblkno(fs, address);
	if ((block >= fragstoblks(fs, fs->fs_size)) && !(override)) {
		printf("block exceeds maximum block in file system\n");
		error++;
		return(0);
	}
	for (bp=bhdr.fwd; bp!= &bhdr; bp=bp->fwd)
		if (bp->valid && bp->blkno==block)
			goto xit;
	actual_disk_reads++;
	bp = bhdr.back;
	bp->blkno = (long)block;
	bp->valid = 0;
	if (syssgi(SGI_READB, fd, bp->blkaddr, block, 1) != 1) {
		error++;
		fflush(stdout);
		perror("read error ");
		fprintf(stderr, "           : block  = 0x%x\n", block);
		fprintf(stderr, "           : nbytes = 0x%x\n", BBSIZE);
		fflush(stderr);
		return(0);
	}
	bp->valid++;
xit:	bp->back->fwd = bp->fwd;
	bp->fwd->back = bp->back;
	insert(bp);
	return(bp->blkaddr);
}

/*
 * insert - place the designated buffer control block
 *	at the head of the linked list of buffers.
 */
static void
insert(struct buf *bp)
{

	bp->back = &bhdr;
	bp->fwd = bhdr.fwd;
	bhdr.fwd->back = bp;
	bhdr.fwd = bp;
}

/*
 * err - called on interrupts.  Set the current address
 *	back to the last address stored in erraddr. Reset all
 *	appropriate flags.  A reset call is made to return
 *	to the main loop;
 */
static void
err(void)
{
	freemem(filenames, nfiles);
	nfiles = 0;
	signal(SIGINT, err);
	addr = erraddr;
	cur_ino = errino;
	cur_inum = errinum;
	cur_bytes = errcur_bytes;
	error = 0;
	c_count = 0;
	printf("\n?\n");
	fseek(stdin, 0L, SEEK_END);
	longjmp(env,0);
}

/*
 * devcheck - check that the given mode represents a 
 *	special device. The IFCHR bit is on for both
 *	character and block devices.
 */
static int
devcheck(short md)
{
	if (override)
		return(0);
	if (((md & IFMT) == IFCHR) || ((md & IFMT) == IFBLK))
		return(0);
	printf("not character or block device\n");
	error++;
	return(1);
}

/*
 * nullblk - return error if address is zero.  This is done
 *	to prevent block 0 from being used as an indirect block
 *	for a large file or as a data block for a small file.
 */
static int
nullblk(long bn)
{
	if (bn != 0)
		return(0);
	printf("non existent block\n");
	error++;
	return(1);
}

/*
 * puta - put ascii characters into a buffer.  The string
 *	terminates with a quote or newline.  The leading quote,
 *	which is optional for directory names, was stripped off
 *	by the assignment case in the main loop.
 */
static int
puta(void)
{
	char		*cptr, c;
	int		i;
	char		*sbptr;
	short		terror = 0;
	long		maxchars, temp;
	baddr_t		taddr = addr;
	long		tcount = 0, item, olditem = 0;

	if (!wrtflag) {
		printf("not opened for write '-w'\n");
		error++;
		return(0);
	}
	if ((sbptr = getblk(addr)) == 0)
		return(0);
	cptr = sbptr + blkoff(fs, addr);
	if (objsz == DIRECTORY) {
		if (acting_on_directory)
			maxchars = stringsize;
		else
			maxchars = LONG;
	} else if (objsz == INODE)
		maxchars = (long)(objsz - (addr - cur_ino));
	else
		maxchars = min(blocksize - cur_bytes, filesize - cur_bytes);
	while ((c = getachar()) != '"') {
		if (tcount >= maxchars) {
			printf("string too long\n");
			if (objsz == DIRECTORY)
				addr = cur_dir;
			else if (acting_on_inode || objsz == INODE)
				addr = cur_ino;
			else
				addr = taddr;
			erraddr = addr;
			errcur_bytes = cur_bytes;
			terror++;
			break;
		}
		tcount++;
		if (c == '\n') {
			ungetachar(c);
			break;
		}
		temp = (long)*cptr;
		olditem <<= BITSPERCHAR;
		olditem += temp & 0xff;
		if (c == '\\') {
			switch (c = getachar()) {
			case 't':
				*cptr++ = '\t';
				break;
			case 'n':
				*cptr++ = '\n';
				break;
			case '0':
				*cptr++ = '\0';
				break;
			default:
				*cptr++ = c;
				break;
			}
		}
		else
			*cptr++ = c;
	}
	if (objsz == DIRECTORY && acting_on_directory)
		for (i = tcount; i < maxchars; i++)
			*cptr++ = '#';
	if (syssgi(SGI_WRITEB, fd, sbptr, lblkno(fs, addr), 1) != 1) {
		error++;
		fflush(stdout);
		perror("write error");
		fprintf(stderr, "           : block  = %x\n",lblkno(fs, addr));
		fprintf(stderr, "           : nbytes = %x\n", BBSIZE);
		fflush(stderr);
		return(0);
	}
	if (!acting_on_inode && objsz != INODE && objsz != DIRECTORY &&
		objsz != EXTENT) {
		addr += tcount;
		cur_bytes += tcount;
		taddr = addr;
		if (objsz != CHAR) {
			addr &= ~(objsz - 1LL);
			cur_bytes -= taddr - addr;
		}
		if (addr == taddr) {
			addr -= objsz;
			taddr = addr;
		}
		tcount = (long)(LONG - (taddr - addr));
		myindex(base);
		if ((cptr = getblk(addr)) == 0)
			return(0);
		cptr += blkoff(fs, addr);
		switch (objsz) {
		case LONG:
			item = *(long *)cptr;
			if (tcount < LONG) {
				olditem <<= tcount * BITSPERCHAR;
				temp = 1;
				for (i = 0; i < (tcount*BITSPERCHAR); i++)
					temp <<= 1;
				olditem += item & (temp - 1);
			}
			break;
		case SHORT:
			item = (long)*(short *)cptr;
			if (tcount < SHORT) {
				olditem <<= tcount * BITSPERCHAR;
				temp = 1;
				for (i = 0; i < (tcount * BITSPERCHAR); i++)
					temp <<= 1;
				olditem += item & (temp - 1);
			}
			olditem &= 0177777L;
			break;
		case CHAR:
			item = (long)*cptr;
			olditem &= 0377;
			break;
		}
		print(olditem, 8, -8, 0);
		printf("\t=\t");
		print(item, 8, -8, 0);
		printf("\n");
	} else {
		if (objsz == DIRECTORY) {
			addr = cur_dir;
			fprnt('?', 'd');
		} else if (objsz == EXTENT) {
			fprnt('?', 'e');
		} else {
			addr = cur_ino;
			objsz = INODE;
			fprnt('?', 'i');
		}
	}
	if (terror)
		error++;
	return(0);
}

/*
 * fprnt - print data.  'count' elements are printed where '*' will
 *	print an entire blocks worth or up to the eof, whichever
 *	occurs first.  An error will occur if crossing a block boundary
 *	is attempted since consecutive blocks don't usually have
 *	meaning.  Current print types:
 *		/		b   - print as bytes (base sensitive)
 *				c   - print as characters
 *				o O - print as octal shorts (longs)
 *				d D - print as decimal shorts (longs)
 *				x X - print as hexadecimal shorts (longs)
 *		?		c   - print as cylinder groups
 *				d   - print as directories
 *				i   - print as inodes
 *				s   - print as super blocks
 *				e   - print as extents
 */
static void
fprnt(char style, char po)
{
	int			i;
	struct efs		*sb;
	struct cg		*cg;
	struct efs_dent		*dep = NULL;
	struct dinode		*ip;
	int			tbase;
	char			c, *cptr, *p;
	long			tinode, tcount, temp;
	baddr_t			taddr;
	short			offset, mode, end = 0, eof = 0, eof_flag;
	unsigned short		*sptr;
	unsigned long		*lptr;
	struct extent		*eptr;
	struct efs_dirblk	*db;
	int			slotno = 0;
	char			buffer[MAXPATHLEN];

	laststyle = style;
	lastpo = po;
	should_print = 0;
	if (count != 1) {
		if (clear) {
			count = 1;
			star = 0;
			clear = 0;
		} else
			clear = 1;
	}
	tcount = count;
	offset = blkoff(fs, addr);

	if (style == '/') {
		if (type == NUMB)
			eof_flag = 0;
		else
			eof_flag = 1;
		switch (po) {

		case 'c': /* print as characters */
		case 'b': /* or bytes */
			if ((cptr = getblk(addr)) == 0)
				return;
			cptr += offset;
			objsz = CHAR;
			tcount = check_addr(eof_flag, &end, &eof, 0);
			if (tcount) {
				for (i=0; tcount--; i++) {
					if (i % 16 == 0) {
						if (i)
							printf("\n");
						myindex(base);
					}
					if (po == 'c') {
						putf(*cptr++);
						if ((i + 1) % 16)
							printf("  ");
					} else {
						if ((i + 1) % 16 == 0)
							print(*cptr++ & 0377,
								2,-2,0);
						else
							print(*cptr++ & 0377,
								4,-2,0);
					}
					addr += CHAR;
					cur_bytes += CHAR;
				}
				printf("\n");
			}
			addr -= CHAR;
			erraddr = addr;
			cur_bytes -= CHAR;
			errcur_bytes = cur_bytes;
			if (eof) {
				printf("end of file\n");
				error++;
			} else if (end) {
				if (type == BLOCK)
					printf("end of block\n");
				else
					printf("end of fragment\n");
				error++;
			}
			return;

		case 'o': /* print as octal shorts */
			tbase = OCTAL;
			goto otx;
		case 'd': /* print as decimal shorts */
			tbase = DECIMAL;
			goto otx;
		case 'x': /* print as hex shorts */
			tbase = HEX;
otx:
			if ((cptr = getblk(addr)) == 0)
				return;
			taddr = addr;
			addr &= ~(SHORT - 1LL);
			cur_bytes -= taddr - addr;
			cptr += blkoff(fs, addr);
			sptr = (unsigned short *)cptr;
			objsz = SHORT;
			tcount = check_addr(eof_flag, &end, &eof, 0);
			if (tcount) {
				for (i=0; tcount--; i++) {
					sptr = (unsigned short *)
					   print_check((unsigned long *)sptr, &tcount, tbase, i);
					switch (po) {
					case 'o':
						printf("%06o ",*sptr++);
						break;
					case 'd':
						printf("%05d  ",*sptr++);
						break;
					case 'x':
						printf("%04x   ",*sptr++);
						break;
					}
					addr += SHORT;
					cur_bytes += SHORT;
				}
				printf("\n");
			}
			addr -= SHORT;
			erraddr = addr;
			cur_bytes -= SHORT;
			errcur_bytes = cur_bytes;
			if (eof) {
				printf("end of file\n");
				error++;
			} else if (end) {
				if (type == BLOCK)
					printf("end of block\n");
				else
					printf("end of fragment\n");
				error++;
			}
			return;

		case 'O': /* print as octal longs */
			tbase = OCTAL;
			goto OTX;
		case 'D': /* print as decimal longs */
			tbase = DECIMAL;
			goto OTX;
		case 'X': /* print as hex longs */
			tbase = HEX;
OTX:
			if ((cptr = getblk(addr)) == 0)
				return;
			taddr = addr;
			addr &= ~(LONG - 1LL);
			cur_bytes -= taddr - addr;
			cptr += blkoff(fs, addr);
			lptr = (unsigned long *)cptr;
			objsz = LONG;
			tcount = check_addr(eof_flag, &end, &eof, 0);
			if (tcount) {
				for (i=0; tcount--; i++) {
					lptr =
					   print_check(lptr, &tcount, tbase, i);
					switch (po) {
					case 'O':
						printf("%011o    ",*lptr++);
						break;
					case 'D':
						printf("%010u     ",*lptr++);
						break;
					case 'X':
						printf("%08x       ",*lptr++);
						break;
					}
					addr += LONG;
					cur_bytes += LONG;
				}
				printf("\n");
			}
			addr -= LONG;
			erraddr = addr;
			cur_bytes -= LONG;
			errcur_bytes = cur_bytes;
			if (eof) {
				printf("end of file\n");
				error++;
			} else if (end) {
				if (type == BLOCK)
					printf("end of block\n");
				else
					printf("end of fragment\n");
				error++;
			}
			return;

		default:
			error++;
			printf("no such print option\n");
			return;
		}
	} else
		switch (po) {

		case 'c': /* print as cylinder group */
			if (type != NUMB)
				if (cur_cgrp + count > fs->fs_ncg) {
					tcount = fs->fs_ncg - cur_cgrp;
					if (!star)
						end++;
				}
			addr &= ~(LONG - 1LL);
			/*
			 * There really is no information in the cylinder
			 * group itself about the cylinder group, since
			 * the size etc is stored in the super-block and the
			 * allocation stuff is in the bitmaps (which is not
			 * stored in the cg). So we just kid ourselves and
			 * go through with the rigmarole and finally go take it
			 * out of the compute_cginfo() stuff.
			 * Should be taken out sometime.
			 */
			for (; tcount--;) {
				erraddr = addr;
				errcur_bytes = cur_bytes;
				if (type != NUMB) {
					addr = (baddr_t)cgtod(fs, cur_cgrp)
						<< FRGSHIFT;
					cur_cgrp++;
				}
				/*
				 * Try and read first block of cylinder group,
				 * even though we have no use for it
				 */
				if ((cptr = getblk(addr)) == 0) {
					if (cur_cgrp)
						cur_cgrp--;
					return;
				}
				/*
				 * Get stuff precomputed in compute_cginfo()
				 */
				cg = (struct cg *)&(fs->fs_cgs
					[EFS_BBTOCG(fs, (addr >> BBSHIFT))]);
				if (type == NUMB) {
					cur_cgrp = EFS_BBTOCG(fs,
						cg->cg_firstbn);
					type = objsz = CGRP;
					if (cur_cgrp + count - 1 > fs->fs_ncg) {
						tcount = fs->fs_ncg - cur_cgrp;
						if (!star)
							end++;
					}
				}
				if (!override && !cg_chkmagic(cg)) {
					printf("invalid cylinder group ");
					printf("magic word\n");
					if (cur_cgrp)
						cur_cgrp--;
					error++;
					return;
				}
				printf("cg#: %d\n",
					EFS_BBTOCG(fs, (addr >> BBSHIFT)));
				printcg(cg);
				if (tcount)
					printf("\n");
			}
			cur_cgrp--;
			if (end) {
				printf("end of cylinder groups\n");
				error++;
			}
			return;

		case 'd': /* print as directories */
			if ((cptr = getblk(addr)) == 0)
				return;
			if (type == NUMB) {
				if (fragoff(fs, addr)) {
					printf("address must be at the ");
					printf("beginning of a fragment\n");
					error++;
					return;
				}
				bod_addr = addr;
				type = FRAGMENT;
				dirslot = 0;
				cur_bytes = 0;
				blocksize = FRGSIZE;
				filesize = FRGSIZE * 2;
			}
			cptr += offset;
			objsz = DIRECTORY;
			db = (struct efs_dirblk *)cptr;
			chk_dirmagic(db);
			slotno = dirslot;
			while (tcount-- && cur_bytes < filesize &&
			       (slotno < db->slots) &&
			       cur_bytes < blocksize && !bcomp(addr)) {
				if (((gtemp = EFS_SLOTAT(db, slotno)) == 0) ||
					(gtemp < 3) || (gtemp > 511)) {
					dep = NULL;
					printf("<empty> \n");
					goto down;
				}
				dep = EFS_SLOTOFF_TO_DEP(db,
				    EFS_SLOTAT(db, slotno));
				tinode = EFS_GET_INUM(dep);
				printf("i#: ");
				if (tinode == 0)
					printf("free\t");
				else
					print(tinode, 12, -8, 0);
				bcopy(&dep->d_name[0], buffer,
						dep->d_namelen);
				buffer[dep->d_namelen] = '\0';
				printf("%s\n", buffer);
				erraddr = addr;
				errcur_bytes = cur_bytes;
				addr = (baddr_t) dep;
				cptr = (char *) dep;
down:
				slotno++;
				if (slotno >= db->slots)
				    cur_bytes += BBSIZE;
				dirslot++;
			}
#ifdef notdef
			if (slotno >= db->slots)
				dirslot = 0;
#endif /* notdef */
			addr = erraddr;
			cur_dir = addr;
			cur_bytes = errcur_bytes;
			stringsize = dep ? STRINGSIZE(dep) : 0;
			dirslot--;
			if (tcount >= 0 && !star) {
				switch (type) {
				case FRAGMENT:
					printf("end of fragment\n");
					break;
				case BLOCK:
					printf("end of block\n");
					break;
				default:
					printf("end of directory\n");
					break;
				}
				error++;
			} else
				error = 0;
			return;

		case 'I': /* print direct extents even if not valid */
		case 'i': /* print as inodes */
			if ((ip = (struct dinode *)getblk(addr)) == 0)
				return;
			for (i=1; i < fs->fs_ncg; i++)
				if (addr < ((baddr_t)cgimin(fs,i) << FRGSHIFT))
					break;
			i--;
			offset /= INODE;
			temp = (long)((addr - ((baddr_t)cgimin(fs,i) << FRGSHIFT)) >> FRGSHIFT);
			temp = (i * EFS_COMPUTE_IPCG(fs)) +
				fragstoblks(fs,temp) * INOPB(fs) + offset;
			if (count + offset > INOPB(fs)) {
				tcount = INOPB(fs) - offset;
				if (!star)
					end++;
			}
			/*
			 * temp - inode number of one to be printed
			 * tcount - adjusted to print only those in the bblock
			 * offset - integral # of inodes into the bblock
			 */
			objsz = INODE;
			ip += offset;
			for (i=0; tcount--; ip++, temp++) {
				if ((mode = icheck(addr)) == 0)
					if (!override)
						continue;
				p = " ugtrwxrwxrwx";

				switch (mode & IFMT) {
				case IFDIR:
					c = 'd';
					break;
				case IFCHR:
					c = 'c';
					break;
				case IFBLK:
					c = 'b';
					break;
				case IFREG:
					c = '-';
					break;
				case IFLNK:
					c = 'l';
					break;
				case IFSOCK:
					c = 's';
					break;
				default:
					c = '?';
					if (!override)
						goto empty;
					break;
				}
				printf("i#: ");
				print(temp,12,-8,0);
				printf("   md: ");
				printf("%c", c);
				for (mode = mode << 4; *++p; mode = mode << 1) {
					if (mode & IFREG)
						printf("%c", *p);
					else
						printf("-");
				} 
				printf("  uid: ");
				print(ip->di_uid,8,-4,0);
				printf("      gid: ");
				print(ip->di_gid,8,-4,0);
				printf("\n");
				printf("ln: ");
				print(ip->di_nlink,8,-4,0);
				printf("       sz: ");
				print(ip->di_size,12,-8,0);
				printf("   nex: ");
				print(ip->di_numextents, 8, -4, 0);
				printf("      gen: ");
				print(ip->di_gen,12,-8,0);
				printf("\n");
				if (((ip->di_mode & IFMT) == IFCHR) ||
				    ((ip->di_mode & IFMT) == IFBLK)) {
					printf("maj: ");
					print((int)emajor(&ip->di_u.di_dev),7,-4,0);
					printf("      min: ");
					print((int)eminor(&ip->di_u.di_dev),7,-4,0);
					printf("\n");
				} else {
				    if (ip->di_numextents 
					    > EFS_DIRECTEXTENTS) {
					if (ip->di_numextents > 
						EFS_MAXEXTENTS) {
					    printf("Illegal number of extents\n");
					    error++;
					    return;
					}
					printf("%-7s%-12s%-7s%-12s",
					    "iex#", "db", "len", "off");
					if (ip->di_u.di_extents[0].ex_offset>1)
					    printf("%-7s%-12s%-7s%-12s",
						"iex#", "db", "len", "off");
					printf("\n");
					for (i=0; (i < ip->di_u.di_extents[0].
					    ex_offset);) {
					    print(i, 7, -2, 0);
					    print(ip->di_u.di_extents[i].ex_bn,
						12,-8,0);
					    print(ip->di_u.di_extents[i].
						ex_length, 7,-2,0);
					    print(ip->di_u.di_extents[i].
						ex_offset, 12,-8,0);
					    if (++i % 2 == 0)
						printf("\n");
					}
				    } else {
					if (po == 'I')
						printf("\nForcing the printing of all direct extents entries\n\n");
					if ((ip->di_numextents > 0) || (po == 'I'))
						printf("%-7s%-12s%-7s%-12s",
						    "ex#", "db", "len", "off");
					if ((ip->di_numextents > 1) || (po == 'I'))
					    printf("%-7s%-12s%-7s%-12s",
						"ex#", "db", "len", "off");
					if (po != 'I' &&
					    ip->di_numextents == 0 &&
					    (ip->di_mode & IFMT) == IFLNK &&
					    ip->di_size > 0 &&
					    ip->di_size <= EFS_MAX_INLINE) {
					    bcopy((char *)
						&ip->di_u.di_extents[0],
						buffer,
						ip->di_size);
					    buffer[ip->di_size] = 0;
					    printf("inline data: %s",
						buffer);
					}
					printf("\n");
					for (i=0; ((i < ip->di_numextents) && (po == 'i')) || ((po == 'I') && (i < EFS_DIRECTEXTENTS)); ) {
					    print(i, 7, -2, 0);
					    print(ip->di_u.di_extents[i].ex_bn,
						12,-8,0);
					    print(ip->di_u.di_extents[i].
						ex_length, 7,-2,0);
					    print(ip->di_u.di_extents[i].
						ex_offset, 12,-8,0);
					    if (++i % 2 == 0)
						    printf("\n");
					}
				    }
				    if (i & 0x1)
					printf("\n");
				}
				if (count == 1) {
					printf("\taccessed: %s",
						ctime((time_t *)&ip->di_atime));
					printf("\tmodified: %s",
						ctime((time_t *)&ip->di_mtime));
					printf("\tcreated : %s",
						ctime((time_t *)&ip->di_ctime));
				}
				if (tcount)
					printf("\n");
empty:
				if (c == '?' && !override) {
					printf("i#: ");
					print(temp, 12, -8, 0);
					printf("  is unallocated\n");
					if (count != 1)
						printf("\n");
				}
				cur_ino = erraddr = addr;
				errcur_bytes = cur_bytes;
				cur_inum++;
				addr = addr + INODE;
			}
			addr = erraddr;
			cur_bytes = errcur_bytes;
			cur_inum--;
			if (end) {
				printf("end of block\n");
				error++;
			}
			return;

		case 's': /* print as super block */
			if (cur_sblock == 0) {
				addr = SBLOCK * BBSIZE;
				type = NUMB;
			} else
				addr = (baddr_t)cur_sblock << FRGSHIFT;
			addr &= ~(LONG - 1LL);
			erraddr = addr;
			cur_bytes = errcur_bytes;
			if ((cptr = getblk(addr)) == 0)
				return;
			cptr += blkoff(fs, addr);
			sb = (struct efs *)cptr;
			if (!IS_EFS_MAGIC(sb->fs_magic)) {
				if (!override) {
					printf("invalid super block ");
					printf("magic word\n");
					error++;
					return;
				}
			}
			printf("super block in block ");
			print(BTOBBT(addr), 0, 0, 0);
			printf("\n");
			printsb(sb);
			/* eat up the "b" at the end of "?sb" */
			return;
		case 'e' :	/* print as extents */
			if ((cptr = getblk(addr)) == 0)
				return;
			taddr = addr;
			addr &= ~(EXTENT - 1LL);
			cur_bytes -= taddr - addr;
			cptr += blkoff(fs, addr);
			eptr = (struct extent *)cptr;
			objsz = EXTENT;
			tcount = check_addr(eof_flag, &end, &eof, 0);
			if (tcount) {
				printf("%-7s%-12s%-7s%-12s",
					"ex#", "db", "len", "off");
				if (tcount > 1)
					printf("%-7s%-12s%-7s%-12s",
						"ex#", "db", "len", "off");
				printf("\n");
				for (i=0; tcount--; i++) {
					/* XXX check for eslotno being too
					 * big, ex_magic etc
					 */
					printextent(eptr++, eslotno++);
					if ((i)%2)
						printf("\n");
					addr += EXTENT;
					cur_bytes += EXTENT;
				}
				printf("\n");
			}
			addr -= EXTENT;
			erraddr = addr;
			cur_bytes -= EXTENT;
			errcur_bytes = cur_bytes;
			eslotno--;
			if (eof) {
				printf("end of file\n");
				error++;
			} else if (end) {
				if (type == BLOCK)
					printf("end of block\n");
				else
					printf("end of fragment\n");
				error++;
				eslotno = 0;
			}
			return;
		default:
			error++;
			printf("no such print option\n");
			return;
		}
}

/*
 * valid_addr - call check_addr to validate the current address.
 */
static int
valid_addr(void)
{
	short	end = 0, eof = 0;
	long	tcount = count;

	if (!trapped)
		return(1);
	if (cur_bytes < 0) {
		cur_bytes = 0;
		if (blocksize > filesize) {
			printf("beginning of file\n");
		} else {
			if (type == BLOCK)
				printf("beginning of block\n");
			else
				printf("beginning of fragment\n");
		}
		error++;
		return(0);
	}
	count = 1;
	(void) check_addr(1, &end, &eof, (filesize < blocksize));
	count = tcount;
	if (eof) {
		printf("end of file\n");
		error++;
		return(0);
	}
	if (end == 2) {
		if (erraddr > addr) {
			if (type == BLOCK)
				printf("beginning of block\n");
			else
				printf("beginning of fragment\n");
			error++;
			return(0);
		}
	}
	if (end) {
		if (type == BLOCK)
			printf("end of block\n");
		else
			printf("end of fragment\n");
		error++;
		return(0);
	}
	return(1);
}

/*
 * check_addr - check if the address crosses the end of block or
 *	end of file.  Return the proper count.
 */
static int
check_addr(short eof_flag, short *end, short *eof, short keep_on)
{
	long	temp, tcount = count, tcur_bytes = cur_bytes;
	baddr_t	taddr = addr;

	if (bcomp(addr + count * objsz - 1) ||
	    (keep_on && taddr < ((baddr_t)bmap(cur_block) << FRGSHIFT))) {
		error = 0;
		addr = taddr;
		cur_bytes = tcur_bytes;
		if (keep_on) {
			if (addr < erraddr) {
				if (cur_bytes < 0) {
					(*end) = 2;
					return(0);
				}
				temp = cur_block - (long)lblkno(fs, cur_bytes);
				cur_block -= temp;
				if ((addr = (baddr_t)bmap(cur_block) << FRGSHIFT) == 0) {
					cur_block += temp;
					return(0);
				}
				temp = tcur_bytes - cur_bytes;
				addr += temp;
				cur_bytes += temp;
				return(0);
			} else {
				if (cur_bytes >= filesize) {
					(*eof)++;
					return(0);
				}
				temp = (long)lblkno(fs, cur_bytes) - cur_block;
				cur_block += temp;
				if ((addr = (baddr_t)bmap(cur_block) << FRGSHIFT) == 0) {
					cur_block -= temp;
					return(0);
				}
				temp = tcur_bytes - cur_bytes;
				addr += temp;
				cur_bytes += temp;
				return(0);
			}
		}
		tcount = (long)((blkroundup(fs, addr+1LL)-addr) / objsz);
		if (!star)
			(*end) = 2;
	}
	addr = taddr;
	cur_bytes = tcur_bytes;
	if (eof_flag) {
		if (blocksize > filesize) {
			if (cur_bytes >= filesize) {
				tcount = 0;
				(*eof)++;
			} else if (tcount > (filesize - cur_bytes) / objsz) {
				tcount = (filesize - cur_bytes) / objsz;
				if (!star || tcount == 0)
					(*eof)++;
			}
		} else {
			if (cur_bytes >= blocksize) {
				tcount = 0;
				(*end)++;
			} else if (tcount > (blocksize - cur_bytes) / objsz) {
				tcount = (blocksize - cur_bytes) / objsz;
				if (!star || tcount == 0)
					(*end)++;
			}
		}
	}
	return(tcount);
}

/*
 * print_check - check if the index needs to be printed and delete
 *	rows of zeros from the output.
 */
static unsigned long *
print_check(unsigned long *lptr, long *tcount, short tbase, int i)
{
	int		j, k, temp = BYTESPERLINE / objsz;
	short		first_time = 0;
	unsigned long	*tlptr;
	unsigned short	*tsptr, *sptr;

	sptr = (unsigned short *)lptr;
	if (i == 0)
		first_time = 1;
	if (i % temp == 0) {
		if (*tcount >= temp - 1) {
			if (objsz == SHORT)
				tsptr = sptr;
			else
				tlptr = lptr;
			k = *tcount - 1;
			for (j = i; k--; j++) {
				if (override)
					break;
				if (objsz == SHORT) {
					if (*tsptr++ != 0)
						break;
				} else {
					if (*tlptr++ != 0)
						break;
				}
			}
			if (j > (i + temp - 1)) {
				j = (j - i) / temp;
				while (j-- > 0) {
					if (objsz == SHORT)
						sptr += temp;
					else
						lptr += temp;
					*tcount -= temp;
					i += temp;
					addr += BYTESPERLINE; 
					cur_bytes += BYTESPERLINE;
				}
				if (first_time)
					printf("*");
				else
					printf("\n*");
			}
			if (i)
				printf("\n");
			myindex(tbase);
		} else {
			if (i)
				printf("\n");
			myindex(tbase);
		}
	}
	if(objsz == SHORT)
		return((unsigned long *)sptr);
	else
		return(lptr);
}

/*
 * myindex - print a byte index for the printout in base b
 *	with leading zeros.
 */
static void
myindex(int b)
{
	int	tbase = base;

	base = b;
	print(addr, 8, 8, 1);
	printf(":\t");
	base = tbase;
}

/*
 * print - print out the value to digits places with/without
 *	leading zeros and right/left justified in the current base.
 */
static void
print(baddr_t ivalue, int fieldsz, int digits, int lead)
{
	int	i, left = 0;
	char	mode = BASE[base - OCTAL];
	char	*string = &scratch[0];

	if (digits < 0) {
		left = 1;
		digits *= -1;
	}
	if (base != HEX)
		if (digits)
			digits = digits + (digits - 1)/((base >> 1) - 1) + 1;
		else
			digits = 1;
	if (lead) {
		if (left)
			sprintf(string, "%%%c%d%d.%d%s%c",
				'-', 0, digits, lead, "ll", mode);
		else
			sprintf(string, "%%%d%d.%d%s%c", 0, digits, lead, "ll", mode);
	} else {
		if (left)
			sprintf(string, "%%%c%d%s%c", '-', digits, "ll", mode);
		else
			sprintf(string, "%%%d%s%c", digits, "ll", mode);
	}
	printf(string, ivalue);
	for (i = 0; i < fieldsz - digits; i++)
		printf(" ");
}

/*
 * Print out the contents of a superblock.
 */
static void
printsb(struct efs *fsp)
{
	char tmpbuf[100];

	printf("fs_dirty      %5.5s     ", fsp->fs_dirty ? "DIRTY" : "CLEAN");

	strncpy(tmpbuf, fsp->fs_fname, sizeof(fsp->fs_fname));
	tmpbuf[sizeof(fsp->fs_fname)] = '\0';
	printf("fs_fname      %s     ", tmpbuf);

	strncpy(tmpbuf, fsp->fs_fpack, sizeof(fsp->fs_fpack));
	tmpbuf[sizeof(fsp->fs_fpack)] = '\0';
	printf("fs_fpack      %s\n\n", tmpbuf);

	printf("fs_magic      "); print(fsp->fs_magic, 10, -9, 0);
	printf("fs_time       %s", ctime((time_t *)&fsp->fs_time));

	printf("fs_size       "); print(fsp->fs_size, 10, -9, 0);
	printf("fs_ncg        "); print(fsp->fs_ncg,  10, -9, 0); printf("\n");

	printf("fs_cgfsize    "); print(fsp->fs_cgfsize, 10, -9, 0);
	printf("fs_cgisize    "); print(fsp->fs_cgisize, 10, -9, 0);
	printf("\n");

	printf("fs_sectors    "); print(fsp->fs_sectors, 10, -9, 0);
	printf("fs_heads      "); print(fsp->fs_heads, 10, -9, 0); printf("\n");

	printf("fs_tfree      "); print(fsp->fs_tfree, 10, -9, 0);
	printf("fs_tinode     "); print(fsp->fs_tinode, 10, -9, 0);
	printf("\n");

	printf("fs_firstcg    "); print(fsp->fs_firstcg, 10, -9, 0);
	printf("fs_replsb     "); print(fsp->fs_replsb, 10, -9, 0);
	printf("\n");

	printf("fs_bmsize     "); print(fsp->fs_bmsize, 10, -9, 0);
	printf("fs_bmblock    "); print(fsp->fs_bmblock, 10, -9, 0);
	printf("fs_lastialloc "); print(fsp->fs_lastialloc, 10, -9, 0);
	printf("\n");

	printf("fs_checksum   "); print((uint)fsp->fs_checksum, 10, -9, 0);
	/*
	 * These fields are reserved for in-core manipulations. When these
	 * fields are read in from disk, it is assumed that they contain
	 * trash.
	 */
	printf("fs_minfree    "); print(fsp->fs_minfree, 10, -9, 0);
	printf("fs_mindirfree "); print(fsp->fs_mindirfree, 10, -9, 0);
	printf("\n");

	printf("fs_inopchunk  "); print(fsp->fs_inopchunk, 10, -9, 0);
	printf("fs_inopchnkbb "); print(fsp->fs_inopchunkbb, 10, -9, 0);
	printf("fs_ipcg       "); print(fsp->fs_ipcg, 10, -9, 0);
	printf("\n");

	printf("fs_lastinum   "); print(fsp->fs_lastinum, 10, -9, 0);
	printf("fs_lbshift    "); print(fsp->fs_lbshift, 10, -9, 0);
	printf("fs_bmbbsize   "); print(fsp->fs_bmbbsize, 10, -9, 0);
	printf("\n");

	printf("fs_diskfull   "); print(fsp->fs_diskfull, 10, -9, 0);
	printf("fs_freedblock "); print(fsp->fs_freedblock, 10, -9, 0);
	printf("\n");
}

/*
 * Print out the contents of a cylinder group.
 */
static void
printcg(struct cg *cg)
{
	printf("cg_firstbn    "); print(cg->cg_firstbn, 10, -9, 0);
	printf("cg_firstdbn   "); print(cg->cg_firstdbn, 10, -9, 0);
	printf("cg_firsti     "); print(cg->cg_firsti, 10, -9, 0);
	printf("\n");

	printf("cg_lowi       "); print(cg->cg_lowi, 10, -9, 0);
	printf("cg_dfree      "); print(cg->cg_dfree, 10, -9, 0);
	printf("cg_firstdfree "); print(cg->cg_firstdfree, 10, -9, 0);
	printf("\n");
}

/*
 * bcomp - used to check for block over/under flows when stepping through
 *	a file system.
 */
static int
bcomp(baddr_t laddr)
{
	if (override)
		return(0);
	if (lblkno(fs, laddr) == (bhdr.fwd)->blkno)
		return(0);
	error++;
	return(1);
}

/*
 * bmap - maps the logical block number of a file into
 *	the corresponding physical block on the file
 *	system.
 */
static long
bmap(long bn)
{
	struct dinode	*ip;
	struct extent	*ep;
	char 		*cptr;
	int		i, j, exsrched, exnum;
	long		dblk;
	baddr_t		tmpaddr;
	short		tobjsz = objsz;

	if ((cptr = getblk(cur_ino)) == 0)
		return(0);
	cptr += blkoff(fs, cur_ino);
	ip = (struct dinode *)cptr;
	if ((bn << BLKSHIFT)+1 > ip->di_size)
		goto e2big;

	if (ip->di_numextents > EFS_MAXEXTENTS) {
		printf("number of extents exceeds EFS_MAXEXTENTS\n");
		error++;
		return(0L);
	}
	if (ip->di_numextents > EFS_DIRECTEXTENTS) {
		exsrched = 0;
		for (i = 0; i < ip->di_u.di_extents[0].ex_offset; i++) {
			/*
			 * We now search the indirect extent pointed to
			 * by index i
			 */
			for (j=0; j < ip->di_u.di_extents[i].ex_length ; j++) {
				exnum = min(ip->di_numextents - exsrched,
					BBSIZE/EXTENT);
				tmpaddr = BBTOB(ip->di_u.di_extents[i].ex_bn + j);
				if ((cptr = getblk(tmpaddr)) == NULL)
					return(0);
				if ((ep = inext((struct extent *)cptr, exnum,
					bn)) != NULL)
					goto gotit;
				exsrched += exnum;
			}
		}
	} else {
		ep = &ip->di_u.di_extents[0];
		if ((ep = inext(ep, ip->di_numextents, bn)) == NULL)
			goto e2big;
		goto gotit;
	}
e2big:
	printf("Block number exceeds file size !\n");
	error++;
	return(0L);
gotit:
	/*
	 * ex_bn numbers are physical disk block #s, not byte addresses
	 */
	dblk = (long)ep->ex_bn + (bn - (long)ep->ex_offset);
	if (debug) {
		printf("dblk#: ");
		printextent(ep, dblk);
		printf("\n");
	}
	addr = BBTOB(dblk);
	cur_bytes = bn * BLKSIZE;
	i = get(LONG);
	objsz = tobjsz;
	return(nullblk(i) ? 0L : dblk);
}

/*
 * These fields in the superblock are always computed. Do not trust what
 * is on disk.
 */
static void
compute_efs(struct efs *fs)
{
	int			pagesize, shift;
	struct pageconst	p;
	
	if (syssgi(SGI_CONST, SGICONST_PAGESZ, &p, sizeof(p), 0) == -1) {
		perror("syssgi: pageconst");
		exit(1);
	}
	pagesize = p.p_pagesz;
	shift = p.p_pnumshft;
	fs->fs_ipcg = EFS_INOPBB * fs->fs_cgisize;
	fs->fs_lastinum = (fs->fs_ipcg * fs->fs_ncg) - 1;
	fs->fs_lbshift = shift;
	fs->fs_inopchunk = pagesize / sizeof(struct efs_dinode);
	fs->fs_minfree = EFS_MINFREEBB;
	fs->fs_mindirfree = EFS_MINDIRFREEBB;
	/* make sure that fs->fs_inopchunk is on a bb boundary */
	fs->fs_inopchunk = ((fs->fs_inopchunk + EFS_INOPBB - 1) >>
				EFS_INOPBBSHIFT) << EFS_INOPBBSHIFT;
	fs->fs_inopchunkbb = (fs->fs_inopchunk + EFS_INOPBB - 1) >>
				EFS_INOPBBSHIFT;
}

static void
compute_cginfo(struct efs **fspp)
{
	struct efs *newfsp;
	int inum, bn, i;
	struct cg *cg;

	if ((newfsp = (struct efs *)calloc(1, sizeof(struct efs) + 
		((*fspp)->fs_ncg - 1) * sizeof(struct cg))) == NULL) {
	    fprintf(stderr, "Couldn't allocated memory for cylinder groups\n");
	    exit(1);
	}
	bcopy((caddr_t)*fspp, (caddr_t)newfsp, 
		sizeof(struct efs) - sizeof(struct cg));
	inum = 0;
	bn = fs->fs_firstcg;
	cg = &newfsp->fs_cgs[0];
	for (i = fs->fs_ncg; --i >= 0; cg++) {
	    cg->cg_firsti = inum;
	    cg->cg_lowi = cg->cg_firsti;
	    cg->cg_firstbn = bn;
	    cg->cg_firstdbn = bn + fs->fs_cgisize;
	    inum += fs->fs_ipcg;
	    bn += fs->fs_cgfsize;
	}
	/* What about efs_buildsum(fs, cg); */
	*fspp = newfsp;
}

static int
efs_addrtoinum(baddr_t byte_addr)
{
	int cg, offset, temp = 0;

	for (cg=1; cg < fs->fs_ncg; cg++)
		if (byte_addr < ((baddr_t)cgimin(fs,cg) << FRGSHIFT))
			break;
	cg--;
	/* cg - cylinder group the inode belongs to */
	offset = blkoff(fs, byte_addr);
	offset /= INODE;
	temp = (byte_addr - ((baddr_t)EFS_CGIMIN(fs,cg) << FRGSHIFT)) >> FRGSHIFT;
	temp = (cg * EFS_COMPUTE_IPCG(fs)) +
		fragstoblks(fs,temp) * INOPB(fs) + offset;
	return(temp);
}

static void
printextent(struct extent *ep, int num)
{
	print(num, 7, -2, 0);
	print(ep->ex_bn, 12,-8,0);
	print(ep->ex_length, 7,-2,0);
	print(ep->ex_offset, 12,-8,0);
}

static struct extent *
inext(struct extent *ep, int nex, int bn)
{
	struct extent *tep;
	int i;

	for (tep = ep, i=0; i < nex; i++, tep++) {
		if (tep->ex_magic != 0) {
			printf("Bad magic number in file extent\n");
			return(0);
		}
		if ((tep->ex_offset <= bn) &&
			(tep->ex_offset + tep->ex_length > bn))
			return(tep);
	}
	return(0);
}

struct	pfstab {
	struct	pfstab *pf_next;
	struct	mntent *pf_fstab;
};

struct pfstab *table = NULL;

static struct mntent *
allocfsent(struct mntent *fs)
{
	struct mntent	*new;
	char		*cp;

	new = (struct mntent *)malloc(sizeof (*fs));
	cp = malloc(strlen(fs->mnt_dir) + 1);
	strcpy(cp, fs->mnt_dir);
	new->mnt_dir = cp;
	cp = malloc(strlen(fs->mnt_type) + 1);
	strcpy(cp, fs->mnt_type);
	new->mnt_type = cp;
	cp = malloc(strlen(fs->mnt_fsname) + 1);
	strcpy(cp, fs->mnt_fsname);
	new->mnt_fsname = cp;
	cp = malloc(strlen(fs->mnt_opts) + 1);
	strcpy(cp, fs->mnt_opts);
	new->mnt_opts = cp;
	new->mnt_passno = fs->mnt_passno;
	new->mnt_freq = fs->mnt_freq;
	return (new);
}

static void
getfstab(void)
{
	struct mntent	*fs;
	struct pfstab	*pf;
	FILE		*fp;

	fp = setmntent(MNTTAB, "r");
        if (fp == NULL) {
		fprintf(stderr,
			"Can't open %s for raw device information.\n", MNTTAB);
		return;
	}
	while (fs = getmntent(fp)) {
		if (strcmp(fs->mnt_type, MNTTYPE_EFS) != 0)
			continue;
		fs = allocfsent(fs);
		pf = (struct pfstab *)malloc(sizeof (*pf));
		pf->pf_fstab = fs;
		pf->pf_next = table;
		table = pf;
	}
	endmntent(fp);
}

/*
 * Search in the fstab for a file name.
 * This file name can be either the special or the path file name.
 *
 * The entries in the fstab are the BLOCK special names, not the
 * character special names.
 * The caller of fstabsearch assures that the character device
 * is dumped (that is much faster)
 *
 * The file name can omit the leading '/'.
 */
static struct mntent *
fstabsearch(char *key)
{
	struct pfstab	*pf;
	struct mntent	*fs;

	if (table == NULL)
		return 0;
	for (pf = table; pf; pf = pf->pf_next) {
		fs = pf->pf_fstab;
		if (strcmp(fs->mnt_dir, key) == 0)
			return (fs);
		if (strcmp(fs->mnt_fsname, key) == 0)
			return (fs);
		if (strcmp(rawname(fs), key) == 0)
			return (fs);
		if (key[0] != '/'){
			if (*fs->mnt_fsname == '/' &&
			    strcmp(fs->mnt_fsname + 1, key) == 0)
				return (fs);
			if (*fs->mnt_dir == '/' &&
			    strcmp(fs->mnt_dir + 1, key) == 0)
				return (fs);
		}
	}
	return (0);
}

static char *
rawname(struct mntent *mnt)
{
	char *cp;
	static char buf[128];
	char *sep = NULL;

	if (mnt->mnt_opts != NULL && (cp = hasmntopt(mnt, MNTOPT_RAW)) != NULL) {
		cp += strlen(MNTOPT_RAW)+1;
		if ((sep = strchr(cp, ',')) != NULL)
			*sep = '\0';
	} else
		cp = mnt->mnt_fsname;
	strcpy(buf, cp);
	if (sep)
		*sep = ',';
	return buf;
}

static int
emajor(struct edevs *ep)
{
	if (ep->odev == -1)
		return((int)(ep->ndev >> NBITSMINOR));
	else
		return((int)(ep->odev >> ONBITSMINOR));
}

static int
eminor(struct edevs *ep)
{
	if (ep->odev == -1)
		return((int)(ep->ndev & MAXMIN));
	else
		return((int)(ep->ndev & OMAXMIN));
}
