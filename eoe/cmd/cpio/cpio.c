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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/cpio/RCS/cpio.c,v 1.65 1999/08/10 19:55:52 tee Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <memory.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <sys/mtio.h>
#include <sys/tpsc.h>
#include <sys/resource.h>
#include <sys/param.h>		/* for UID_NOBODY */
#include <utime.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <ctype.h>
#include <archives.h>
#include <limits.h>
#include <locale.h>
#include <malloc.h>
#include <alloca.h>
#include <pfmt.h>
#include "cpio.h"
#include "newstat.h"
#include "rmt.h"
#include "libgenIO.h"
#include <msgs/uxsgicore.h>

/* Here for now - until POSIX tar/cpio get sync'ed up */
#define NAMSPACE 101  /* The maximum length of the name field */
#define NAMSIZ        (NAMSPACE - 1)
#define PREFIXSPACE   156     /* The maximum length of the prefix */
#define PREFIXSIZ     (PREFIXSPACE - 1)

#define BIN_MAXUID   0xffff
#define CHR_MAXUID   0777777
#define USTAR_MAXUID 0777777
#define TAR_MAXUID   0777777


extern int g_read(int devtype, int fdes, char *buf, unsigned nbytes);
extern int g_write(int devtype, int fdes, char *buf, unsigned nbytes);
extern int g_init(int *devtype, int *fdes);


static void setup(int, char **);
static void file_in(void);
static void file_out(void);
static void file_pass(void);
static int gethdr(void);
static int getname(void);
static void scan4trail(void);
static void ioerror(int);
static int chgreel(int);
static void rstbuf(void);
static int matched(void);
static ulong cksum(char, int);
static void verbose(char *);
static void rstfiles(int);
static int missdir(char *);
static int creat_hdr(void);
static int creat_lnk(char *, char *);
static int creat_spec(void);
static int creat_tmp(char *);
static void swap(char *, int);
static void write_trail(void);
static void bflush(void);
static int bfill(void);
static int ckname(void);
static void data_in(int);
static void data_out(void);
static int openout(void);
static void data_pass(void);
static int read_hdr(int);
static ulong mklong(short []);
static void mkshort(short [], long);
static void setpasswd(char *);
static void set_tym(char *, time_t, time_t);
static void usage(void);
static void sigint(void);
static void msg(int, ...);
int isqic02(int);
static void write_hdr(void);
static void flush_lnks(void);
int chknomedia(int);
static char * align(int);
static void ckopts(long);
static void getpats(int, char **);
static void chkandfixaudio(int);

static 
struct passwd	*Curpw_p,	/* Current password entry for -t option */
		*Rpw_p,		/* Password entry for -R option */
		*dpasswd;

static
struct group	*Curgr_p,	/* Current group entry for -t option */
		*dgroup;

/* Data structure for buffered I/O. */

static
struct buf_info {
	char	*b_base_p,	/* Pointer to base of buffer */
		*b_out_p,	/* Position to take bytes from buffer at */
		*b_in_p,	/* Position to put bytes into buffer at */
		*b_end_p;	/* Pointer to end of buffer */
	long	b_cnt,		/* Count of unprocessed bytes */
		b_size;		/* Size of buffer in bytes */
} Buffr;


/* Generic header format */

static
struct gen_hdr {
	ino64_t	g_ino;		/* Inode number of file - expanded for XFS */
	ulong	g_magic,	/* Magic number field */
		g_mode,		/* Mode of file */
		g_uid,		/* Uid of file */
		g_gid,		/* Gid of file */
		g_nlink,	/* Number of links; also flags bad header for cpio  */
		g_mtime;	/* Modification time */
	off64_t	g_filesz;	/* Length of file  - expanded for XFS */
	off_t	g_filesz2;	/* Remainder of file modulo 2**32 - created for XFS */
	ulong	g_dev,		/* File system of file */
		g_rdev,		/* Major/minor numbers of special files */
		g_namesz,	/* Length of filename */
		g_cksum;	/* Checksum of file */
	char	g_gname[32],
		g_uname[32],
		g_version[2],
		g_tmagic[6],
		g_typeflag;
	char	*g_tname,
		*g_prefix,
		*g_nam_p;	/* Filename */
} Gen, *G_p;

/* Data structure for handling multiply-linked files */
static
char	prebuf[PREFIXSPACE],
	nambuf[PATH_MAX],
	fullnam[HNAMLEN];


#define NLNKHASH	8
#define HASHLNK(x)	((x) & (NLNKHASH-1))
static
struct Lnk {
	short	L_cnt,		/* Number of links encountered */
		L_data;		/* Data has been encountered if 1 */
	struct gen_hdr	L_gen;	/* gen_hdr information for this file */
	struct Lnk	*L_nxt_p,	/* Next file in list */
			*L_bck_p,	/* Previous file in list */
			*L_lnk_p;	/* Next link for this file */
} Lnk_hd[NLNKHASH];

static struct Lnk * add_lnk(struct Lnk **);
static void reclaim( struct Lnk *);
static struct Lnk *find_lnk(void);

static
struct hdr_cpio	Hdr;

static
struct stat	ArchSt;	/* stat(2) information of the archive */
struct stat64	SrcSt,	/* stat64(2) information of source file */
		DesSt;	/* stat64(2) of destination file */

/*
 * bin_mag: Used to validate a binary magic number,
 * by combining to bytes into an unsigned short.
 */

static
union bin_mag{
	unsigned char b_byte[2];
	ushort b_half;
} Binmag;

static
union tblock *Thdr_p;	/* TAR header pointer */

/*
 * swpbuf: Used in swap() to swap bytes within a halfword,
 * halfwords within a word, or to reverse the order of the 
 * bytes within a word.  Also used in mklong() and mkshort().
 */

static
union swpbuf {
	unsigned char	s_byte[4];
	ushort	s_half[2];
	ulong	s_word;
} *Swp_p;

static
char	Adir,			/* Flags object as a directory */
	Aspec,			/* Flags object as a special file */
	Time[50],		/* Array to hold date and time */
	Ttyname[] = "/dev/tty",	/* Controlling console */
	T_lname[PATH_MAX],	/* Array to hold links name for tar */
	*Buf_p,			/* Buffer for file system I/O */
	*Empty,			/* Empty block for TARTYP headers */
	*Full_p,		/* Pointer to full pathname */
	*Efil_p,		/* -E pattern file string */
	*Eom_p = "Change to part %d and press RETURN key. [q] ",
	*Eom_pid = ":32",
	*Fullnam_p,		/* Full pathname */
	*Hdr_p,			/* -H header type string */
	*IOfil_p,		/* -I/-O input/output archive string */
	*Lnkend_p,		/* Pointer to end of Lnknam_p */
	*Lnknam_p,		/* Buffer for linking files with -p option */
	*Nam_p,			/* Array to hold filename */
	*Own_p,			/* New owner login id string */
	*Renam_p,		/* Buffer for renaming files */
	*Symlnk_p,		/* Buffer for holding symbolic link name */
	*Over_p,		/* Holds temporary filename when overwriting */
	*cmpbuf,		/* buffer to read file into with Compareflag */
	**Pat_pp = 0;		/* Pattern strings */

static
int	Append = 0,	/* Flag set while searching to end of archive */
	Archive,	/* File descriptor of the archive */
	Buf_error = 0,	/* I/O error occured during buffer fill */
	Def_mode = 0777,/* Default file/directory protection modes */
	Device,		/* Device type being accessed (used with libgenIO) */
	Error_cnt = 0,	/* Cumulative count of I/O errors */
	Finished = 1,	/* Indicates that a file transfer has completed */
	Fix_mode = 0,	/* Fix the mode on a dir (wrong mode from missdir?) */
	Hdrsz = ASCSZ,	/* Fixed length portion of the header */
	Hdr_type,		/* Flag to indicate type of header selected */
	Ifile,		/* File des. of file being archived */
	Ofile,		/* File des. of file being extracted from archive */
	Onecopy = 0,	/* Flags old vs. new link handling */
	Pad_val = 0,	/* Indicates the number of bytes to pad (if any) */
	Volcnt = 1,	/* Number of archive volumes processed */
	Verbcnt = 0,	/* Count of number of dots '.' output */
	Eomflag = 0,
	Dflag = 0,	/* Special flag for libpkg - not documented */
	Kflag = 0,	/* Don't ignore big files (required for xfs support)*/
	Holeflag = 0, /* Deal with holey (xfs) files better by skipping hole */
	portwarn,	/* have we warned yet that the archive will be non-portable? */
	Compareflag = 0; /* Test for differences (compare archive against filesystem) */

mode_t Orig_umask;	/* Inherited umask */

static
gid_t	Lastgid = -1;	/* Used with -t & -v to record current gid */

static
uid_t	Lastuid = -1;	/* Used with -t & -v to record current uid */

static
rlim64_t	Max_filesz;	/* Maximum file size from getrlimit64(2) */

static
long	Args,		/* Mask of selected options */
	Max_namesz = APATH,	/* Maximum size of pathnames/filenames */
	SBlocks;	/* Cumulative char count for short reads */

long long	Blocks;		/* Number of full blocks transferred */
static 
int	Bufsize = BUFSZ;	/* Default block size */

static
FILE	*Ef_p,			/* File pointer of pattern input file */
	*Err_p = stderr,	/* File pointer for error reporting */
	*Out_p = stdout,	/* File pointer for non-archive output */
	*Rtty_p,		/* Input file pointer for interactive rename */
	*Wtty_p;		/* Output file pointer for interactive rename */



static
ushort	Ftype = S_IFMT;		/* File type mask */
static
uid_t	Uid;			/* Uid of invoker */

static const char
        mutex[] = "-%c and -%c are mutually exclusive.",
        mutexid[] = ":33",
        badaccess[] = "Cannot access \"%s\"",
        badaccessid[] = ":34",
        badaccarch[] = "Cannot access the archive",
        badaccarchid[] = ":35",
        badcreate[] = "Cannot create \"%s\"",
        badcreateid[] = ":36",
        badcreatdir[] = "Cannot create directory for \"%s\"",
        badcreatdirid[] = ":37",
        badfollow[] = "Cannot follow \"%s\"",
        badfollowid[] = ":38",
        badread[] = "Read error in \"%s\"",
        badreadid[] = ":39",
        badreadsym[] = "Cannot read symbolic link \"%s\"",
        badreadsymid[] = ":40",
        badreadtty[] = "Cannot read tty.",
        badreadttyid[] = ":41",
        badreminc[] = "Cannot remove incomplete \"%s\"",
        badremincid[] = ":42",
        badremtmp[] = "Cannot remove temp file \"%s\"",
        badremtmpid[] = ":43",
        badwrite[] = "Write error in \"%s\"",
        badwriteid[] = ":44",
        badinit[] = "Error during initialization",
        badinitid[] = ":45",
        sameage[] = "Existing \"%s\" same age or newer",
        sameageid[] = ":46",
        badcase[] = "Impossible case.",
        badcaseid[] = ":47",
        badhdr[] = "Impossible header type.",
        badhdrid[] = ":48",
        nomem[] = "Out of memory",
        nomemid[] = ":49",
        badappend[] = "Cannot append to this archive",
        badappendid[] = ":50",
        badchmod[] = "chmod() failed on \"%s\"",
        badchmodid[] = ":51",
        badchown[] = "chown() failed on \"%s\"",
        badchownid[] = ":52",
        badpasswd[] = "Cannot get passwd information for %s",
        badpasswdid[] = ":53",
        badgroup[] = "Cannot get group information for %s",
        badgroupid[] = ":54",
        badorig[] = "Cannot recover original \"%s\"",
        badorigid[] = ":55";



/*
 * main: Call setup() to process options and perform initializations,
 * and then select either copy in (-i), copy out (-o), or pass (-p) action.
 */

main(argc, argv)
char **argv;
int argc;
{
	int gotname=0, doarch = 0;
	int goodhdr = 0, oldgoodhdr = 0;

	(void)setlocale(LC_ALL, "");
        (void)setcat("uxcore.abi");
        (void)setlabel("UX:cpio");

	setup(argc, argv);
	if (signal(SIGINT, sigint) == SIG_IGN)
		(void)signal(SIGINT, SIG_IGN);
	switch (Args & (OCi | OCo | OCp)) {
	case OCi: /* COPY IN */
		Hdr_type = NONE;
		while ((goodhdr = gethdr()) != 0) {
			file_in();
			oldgoodhdr += goodhdr;
		}
		/* Do not count "extra" "read-ahead" buffered data */
		if (Buffr.b_cnt > Bufsize)
			Blocks -= (Buffr.b_cnt / Bufsize);
		break;
	case OCo: /* COPY OUT */
		if (Args & OCA)
			scan4trail();
		while ((gotname = getname()) != 0) {
			if (gotname == 1) {
				file_out();
				doarch++;
			}
		}
		if (doarch > 0)	
			write_trail();
		break;
	case OCp: /* PASS */
		while ((gotname = getname()) != 0)
			if (gotname == 1)
				file_pass();
		break;
	default:
		msg(EXT, ":56", "Impossible action.");
	}
	Blocks = (long long)(Blocks * (double)(Bufsize/BUFSZ));
	Blocks += (SBlocks + (BUFSZ-1)) / BUFSZ;
	if (oldgoodhdr == 0 && (Args & OCi))
		Blocks = 0;
        msg(EPOST, ":57", "%lld blocks", Blocks);
        if (Error_cnt == 1)
                msg(EPOST, ":58", "1 error");
        else if (Error_cnt > 1)
                msg(EPOST, ":59", "%d error(s)", Error_cnt);
	if (oldgoodhdr == 0 && (Args & OCi))
		Error_cnt++;
	return Error_cnt;
}

/*
 * add_lnk: Add a linked file's header to the linked file data structure.
 * Either adding it to the end of an existing sub-list or starting
 * a new sub-list.  Each sub-list saves the links to a given file.
 */

static struct Lnk *
add_lnk(struct Lnk **l_p)
{
	register struct Lnk *t1l_p, *t2l_p, *t3l_p;

	/*
	 * Look up the link.  If the return value is null, we are creating
	 * a new link.
	 */
	t2l_p = find_lnk();
	t1l_p = (struct Lnk *)malloc(sizeof(struct Lnk));	/* create new link t1l_p */
	if (t1l_p == (struct Lnk *)NULL)
		msg(EXT, nomemid, nomem);
	t1l_p->L_lnk_p = (struct Lnk *)NULL;
	t1l_p->L_gen = *G_p; 					/* structure copy */
	t1l_p->L_gen.g_nam_p = (char *)malloc((unsigned int)G_p->g_namesz);
	if (t1l_p->L_gen.g_nam_p == (char *)NULL)
		msg(EXT, nomemid, nomem);
	(void)strcpy(t1l_p->L_gen.g_nam_p, G_p->g_nam_p);
	if (t2l_p == (struct Lnk *)NULL) { 			/* start new sub-list */
		int hash = HASHLNK(G_p->g_ino);
		t1l_p->L_nxt_p = &Lnk_hd[hash];
		t1l_p->L_bck_p = Lnk_hd[hash].L_bck_p;
		Lnk_hd[hash].L_bck_p = t1l_p->L_bck_p->L_nxt_p = t1l_p;
		t1l_p->L_lnk_p = (struct Lnk *)NULL;
		t1l_p->L_cnt = 1;
		t1l_p->L_data = Onecopy ? 0 : 1;
		t2l_p = t1l_p;
	} else { 						/* add to existing sub-list */
		t2l_p->L_cnt++;
		t3l_p = t2l_p;
		while (t3l_p->L_lnk_p != (struct Lnk *)NULL) {
			t3l_p->L_gen.g_filesz = G_p->g_filesz;
			t3l_p = t3l_p->L_lnk_p;
		}
		t3l_p->L_gen.g_filesz = G_p->g_filesz;
		t3l_p->L_lnk_p = t1l_p;
	}
	*l_p = t2l_p;
	return(t1l_p);
}

/*
 * Return a pointer to a link structure if one exists corresponding to
 * the current generic header.
 */
static struct Lnk *
find_lnk(void)
{
	register struct Lnk *t2l_p;
	long maj, min;
	int hash = HASHLNK(G_p->g_ino);

	t2l_p = Lnk_hd[hash].L_nxt_p;
		
	while (t2l_p != &Lnk_hd[hash]) {

	/* First check if the inode number are the same ... Then check if 
	 * the major/minor number are the same - if true then hardlink */

		if (t2l_p->L_gen.g_ino == G_p->g_ino) {
			if (Hdr_type == BIN || Hdr_type == CHR ) {
				maj = cpioMAJOR(t2l_p->L_gen.g_dev);
				min = cpioMINOR(t2l_p->L_gen.g_dev);
				if (maj == major(DEV32TO16(G_p->g_dev)) &&
				    min == minor(DEV32TO16(G_p->g_dev)) )
					break; /* found */
			}
			else {
				maj = (long)major(t2l_p->L_gen.g_dev);
				min = (long)minor(t2l_p->L_gen.g_dev);
				if( maj == major(G_p->g_dev) &&
				    min == minor(G_p->g_dev) )
					break; /* found */
			}
		}
		t2l_p = t2l_p->L_nxt_p;
	}
	
	if (t2l_p != &Lnk_hd[hash])
		return t2l_p;
	else
		return (struct Lnk *)0;
}

/*
 * align: Align a section of memory of size bytes on a page boundary and 
 * return the location.  Used to increase I/O performance.
 */

static char *
align(int size)
{
	register int pad;
	int pagesize;

	pagesize = getpagesize();

	if ((pad = ((int)sbrk(0) & (pagesize-1))) > 0) {
		pad = pagesize - pad;
		if (sbrk(pad) ==  NULL)
			return((char *)NULL);
	}
	return((char *)sbrk(size));
}

static int
bfill(void)
{
 	register int i = 0, rv;
	static int eof = 0;

	if (!Dflag) {
		while ((Buffr.b_end_p - Buffr.b_in_p) >= Bufsize) {
			errno = 0;
			if ((rv = g_read(Device, Archive, Buffr.b_in_p, Bufsize)) < 0) {
				if (((Buffr.b_end_p - Buffr.b_in_p) >= Bufsize) && (Eomflag == 0)) {
					Eomflag = 1;
					return(1);
				}
				if (errno == ENOSPC) {
					(void)chgreel(INPUT);
					continue;
				} else if (Args & OCk) {
					if (i++ > MX_SEEKS)
						msg(EXT, ":60", "Cannot recover.");
					if (rmtlseek(Archive, Bufsize, SEEK_REL) < 0)
						msg(EXTN, ":61", "lseek() failed");
					Error_cnt++;
					Buf_error++;
					rv = 0;
					continue;
				} else
					ioerror(INPUT);
			} /* (rv = g_read(Device, Archive ... */
			Buffr.b_in_p += rv;
			Buffr.b_cnt += (long)rv;
			if (rv == Bufsize)
				Blocks++;
			else if (!rv) {
				if (!eof) {
					eof = 1;
					break;
				}
				return(-1);
			} else
				SBlocks += rv;
		} /* (Buffr.b_end_p - Buffr.b_in_p) <= Bufsize */
	}
	else {
                errno = 0;
                if ((rv = g_read(Device, Archive, Buffr.b_in_p, Bufsize)) < 0) {
                        return(-1);
                } /* (rv = g_read(Device, Archive ... */
                Buffr.b_in_p += rv;
                Buffr.b_cnt += (long)rv;
                if (rv == Bufsize)
                        Blocks++;
                else if (!rv) {
                        if (!eof) {
                                eof = 1;
                                return(rv);
                        }
                        return(-1);
                } else
                        SBlocks += rv;
        }
	return(rv);
}

/*
 * bflush: Move wr_cnt bytes from data_p into the I/O buffer.  When the
 * I/O buffer is full, Flushbuf is set and the buffer is written out.
 */

static void
bflush(void)
{
	register int rv;

	while (Buffr.b_cnt >= Bufsize) {
		errno = 0;
		if ((rv = g_write(Device, Archive, Buffr.b_out_p, Bufsize)) < 0) {
			if (errno == ENOSPC && !Dflag && Device != G_FILE )
				rv = chgreel(OUTPUT);
			else
				ioerror(OUTPUT);
		}
		Buffr.b_out_p += rv;
		Buffr.b_cnt -= (long)rv;
		if (rv == Bufsize)
			Blocks++;
		else if (rv > 0)
			SBlocks += rv;
	}
	rstbuf();
}

/*
 * chgreel: Determine if end-of-medium has been reached.  If it has,
 * close the current medium and prompt the user for the next medium.
 */

static int
chgreel(int dir)
{
	register int lastchar, tryagain, askagain, rv;
	int tmpdev;
	char str[APATH];

        if (dir)
                msg(EPOST, ":62", "\007End of medium on output.");
        else
                msg(EPOST, ":63", "\007End of medium on input.");

	(void)rmtclose(Archive);

	Volcnt++;
	for (;;) {
		if (Rtty_p == (FILE *)NULL)
			if(!(Rtty_p = fopen(Ttyname, "r")))
				msg(EXT, badreadttyid, badreadtty); /* tough; give up */
		do { /* tryagain */
			if (IOfil_p) {
				do {
					msg(EPOST, Eom_pid, Eom_p, Volcnt);
					if (fgets(str, sizeof(str), Rtty_p) == (char *)NULL)
						msg(EXT, badreadttyid, badreadtty);
					askagain = 0;
					switch (*str) {
					case '\n':
						(void)strcpy(str, IOfil_p);
						break;
					case 'q':
						exit(Error_cnt);
					default:
						askagain = 1;
					}
				} while (askagain);
			} else {
				msg(EPOST, ":64", "To continue, type device/file name when ready.");
				if (fgets(str, sizeof(str), Rtty_p) == (char *)NULL)
					msg(EXT, badreadttyid, badreadtty);
				lastchar = strlen(str) - 1;
				if (*(str + lastchar) == '\n') /* remove '\n' */
					*(str + lastchar) = '\0';
				if (!*str)
					exit(Error_cnt);
			}
			tryagain = 0;

			if ((Archive = rmtopen(str, dir)) < 0) {
				msg(ERRN, ":65", "Cannot open \"%s\"", str);
				tryagain = 1;
			}
		} while (tryagain);
		(void)g_init(&tmpdev, &Archive);
		if (tmpdev != Device)
			msg(EXT, ":66", "Cannot change media types in mid-stream.");
		chkandfixaudio(Archive);
		if (dir == INPUT)
			break;
		else { /* dir == OUTPUT */
			errno = 0;
			if ((rv = g_write(Device, Archive, Buffr.b_out_p, Bufsize)) == Bufsize)
				break;
			else
				msg(ERR, ":67", "Cannot write on this medium, try another.");
		}
	} /* ;; */
	Eomflag = 0;
	return(rv);
}

/*
 * ckname: Check filenames against user specified patterns,
 * and/or ask the user for new name when -r is used.
 */

static int
ckname(void)
{
	register int lastchar;

	if (G_p->g_namesz > Max_namesz) {
		msg(ERR, ":68", "Name exceeds maximum length - skipped.");
		return(F_SKIP);
	}
	if (Pat_pp && !matched())
		return(F_SKIP);
	if ((Args & OCr) && !Adir) { /* rename interactively */
		(void)pfmt(Wtty_p, MM_NOSTD, ":69:Rename \"%s\"? ", G_p->g_nam_p);
		(void)fflush(Wtty_p);
		if (fgets(Renam_p, Max_namesz, Rtty_p) == (char *)NULL)
			msg(EXT, badreadttyid, badreadtty);
		if (feof(Rtty_p))
			exit(Error_cnt);
		lastchar = strlen(Renam_p) - 1;
		if (*(Renam_p + lastchar) == '\n') /* remove trailing '\n' */
			*(Renam_p + lastchar) = '\0';
		if (*Renam_p == '\0') {
			msg(POST, ":70", "%s Skipped.", G_p->g_nam_p);
			*G_p->g_nam_p = '\0';
			return(F_SKIP);
		} else if (strcmp(Renam_p, ".")) {
			G_p->g_nam_p = Renam_p;
		}
	}
	VERBOSE((Args & OCt) && !Compareflag && (G_p->g_filesz >= 0), G_p->g_nam_p);
	if((Args & OCt) && !Compareflag)
		return(F_SKIP);
	return(F_EXTR);
}

/*
 * ckopts: Check the validity of all command line options.
 */

static void
ckopts(long mask)
{
	register int oflag;
	register char *t_p;
	register long errmsk;

	if (mask & OCi)
		errmsk = mask & INV_MSK4i;
	else if (mask & OCo)
		errmsk = mask & INV_MSK4o;
	else if (mask & OCp)
		errmsk = mask & INV_MSK4p;
	else {
		msg(ERR, ":71", "One of -i, -o or -p must be specified.");
		errmsk = 0;
	}
	if (errmsk) /* if non-zero, invalid options were specified */
		Error_cnt++;
	if ((mask & OCa) && (mask & OCm))
		msg(ERR, mutexid, mutex, 'a', 'm');
	if (Holeflag) {
		if(mask & OCc)
			msg(ERR, mutexid, mutex, 'c', 'W');
		if(mask & OCH)
			msg(ERR, mutexid, mutex, 'H', 'W');
	}
	if ((mask & OCc) && (mask & OCH) &&
		((strcmp("odc", Hdr_p) == 0) ||
		 (strcmp("tar", Hdr_p) == 0) || (strcmp("TAR", Hdr_p) == 0) ||
		 (strcmp("ustar", Hdr_p) == 0) || (strcmp("USTAR", Hdr_p) == 0)))
			msg(ERR, mutexid, mutex, 'c', 'H');
	if ((mask & OCc) && (mask & OC6))
		msg(ERR, mutexid, mutex, 'c', '6');
	if ((mask & OCv) && (mask & OCV))
		msg(ERR, mutexid, mutex, 'v', 'V');
	if ((mask & OCB) && (mask & OCC))
		msg(ERR, mutexid, mutex, 'B', 'C');
	if ((mask & OCH) && (mask & OC6))
		msg(ERR, mutexid, mutex, 'H', '6');
	if ((mask & OCM) && !((mask & OCI) || (mask & OCO)))
		msg(ERR, ":72", "-M not meaningful without -O or -I.");
	if ((mask & OCA) && !(mask & OCO))
		msg(ERR, ":73", "-A requires the -O option.");
	if (Kflag && !(mask & OCo))
		msg(ERR, ":142", "-K used only with -o option.");
	if (Kflag && ((mask & OCc) || (mask & OCH)))
		msg(ERR, ":143", "-K used only in binary mode.");
	if (Bufsize <= 0)
		msg(ERR, ":74", "Illegal size given for -C option.");
	if (mask & OCH) {
		t_p = Hdr_p;
		while (*t_p != NULL) {
			if (isupper(*t_p))
				*t_p = 'a' + (*t_p - 'A');
			t_p++;
		}
		if (!strcmp("odc", Hdr_p)) {
			Hdr_type = CHR;
			Max_namesz = CPATH;
			Onecopy = 0;
		} else if (!strcmp("crc", Hdr_p)) {
			Hdr_type = CRC;
			Max_namesz = APATH;
			Onecopy = 1;
		} else if (!strcmp("tar", Hdr_p)) {
			if(Args & OCo) {
				Hdr_type = USTAR;
				Max_namesz = HNAMLEN;
			} else {
				Hdr_type = TAR;
				Max_namesz = TNAMLEN - 1;
			}
			Onecopy = 0;
		} else if (!strcmp("ustar", Hdr_p)) {
			Hdr_type = USTAR;
			Max_namesz = HNAMLEN;
			Onecopy = 0;
		} else
			msg(ERR, ":75", "Invalid header \"%s\" specified", Hdr_p);
	}
	if (mask & OCr) {
		Rtty_p = fopen(Ttyname, "r");
		Wtty_p = fopen(Ttyname, "w");
		if (Rtty_p == (FILE *)NULL || Wtty_p == (FILE *)NULL)
			msg(ERR, ":76", "Cannot rename, \"%s\" missing", Ttyname);
	}
	if ((mask & OCE) && (Ef_p = fopen(Efil_p, "r")) == (FILE *)NULL)
		msg(ERR, ":77", "Cannot open \"%s\" to read patterns", Efil_p);
	if ((mask & OCI) && (Archive = rmtopen(IOfil_p, O_RDONLY)) < 0)
		msg(ERR, ":78", "Cannot open \"%s\" for input", IOfil_p);
	if (mask & OCO) {
		if (mask & OCA) {
			if ((Archive = rmtopen(IOfil_p, O_RDWR)) < 0)
				msg(ERR, ":79", "Cannot open \"%s\" for append", IOfil_p);
		} else {
			oflag = (O_WRONLY | O_CREAT | O_TRUNC);
			if ((Archive = rmtopen(IOfil_p, oflag, 0666)) < 0)
				msg(ERR, ":80", "Cannot open \"%s\" for output", IOfil_p);
		}
	}
	chkandfixaudio(Archive);
	if (mask & OCR) {
		if (Uid != 0)
			msg(ERR, ":81", "R option only valid for super-user.");
		if ((Rpw_p = getpwnam(Own_p)) == (struct passwd *)NULL)
			msg(ERR, ":82", "Unknown user id: %s", Own_p);
	}
	if ((mask & OCo) && !(mask & OCO))
		Out_p = stderr;
}

/*
 * cksum: Calculate the simple checksum of a file (CRC) or header
 * (TARTYP (TAR and USTAR)).  For -o and the CRC header, the file is opened and 
 * the checksum is calculated.  For -i and the CRC header, the checksum
 * is calculated as each block is transferred from the archive I/O buffer
 * to the file system I/O buffer.  The TARTYP (TAR and USTAR) headers calculate 
 * the simple checksum of the header (with the checksum field of the 
 * header initialized to all spaces (\040)).
 */

static ulong 
cksum(char hdr, int byt_cnt)
{
	register char *crc_p, *end_p;
	register int cnt;
	register off64_t checksum = 0LL, lcnt, have;

	switch (hdr) {
	case CRC:
		if (Args & OCi) { /* do running checksum */
			end_p = Buffr.b_out_p + byt_cnt;
			for (crc_p = Buffr.b_out_p; crc_p < end_p; crc_p++)
				checksum += (long)*crc_p;
			break;
		}
		/* OCo - do checksum of file */
		lcnt = G_p->g_filesz;
		while (lcnt > 0) {
			have = (lcnt < CPIOBSZ) ? lcnt : CPIOBSZ;
			errno = 0;
			if (read(Ifile, Buf_p, have) != have) {
				msg(ERR, ":83", "Error computing checksum.");
				checksum = -1L;
				break;
			}
			end_p = Buf_p + have;
			for (crc_p = Buf_p; crc_p < end_p; crc_p++)
				checksum += (long)*crc_p;
			lcnt -= have;
		}
		if (lseek(Ifile, 0, SEEK_ABS) < 0)
			msg(ERRN, ":84", "Cannot reset file after checksum");
		break;
	case TARTYP: /* TAR and USTAR */
		crc_p = Thdr_p->tbuf.t_cksum;
		for (cnt = 0; cnt < TCRCLEN; cnt++) {
			*crc_p = '\040';
			crc_p++;
		}
		crc_p = (char *)Thdr_p;
		for (cnt = 0; cnt < TARSZ; cnt++) {
			checksum += (long)*crc_p;
			crc_p++;
		}
		break;
	default:
		msg(EXT, badhdrid, badhdr);
	} /* hdr */
	return (ulong)checksum;
}

/*
 * creat_hdr: Fill in the generic header structure with the specific
 * header information based on the value of Hdr_type.
 */

static int
creat_hdr(void)
{
	register ushort ftype;
	static char filname[NAMSPACE];
	static char prefix[PREFIXSPACE];
	char *cp, *wholename;
	int len, Alink;

	ftype = (ushort)SrcSt.st_mode & Ftype;
	Adir = (ftype == S_IFDIR);
	Aspec = (ftype == S_IFBLK || ftype == S_IFCHR || ftype == S_IFIFO);
	Alink = (ftype == S_IFLNK);

	/* We want to check here if the file is really readable --
	   If we wait till later it's to late and cpio will have to
	   pad out the file to the expected length, with null contents.
	   Complain about types (like UDS that we can't backup/restore.
	*/
	if (!Adir && !Aspec && !Alink) {
		if (access(Gen.g_nam_p, R_OK) < 0) {
			msg(ERR, ":65", "Cannot open \"%s\"", Gen.g_nam_p);
			return(0);
		}
		if(ftype != S_IFREG) {
			msg(ERR, ":1018", "Cannot backup sockets or unknown file types: \"%s\"", Gen.g_nam_p);
			return(0);
		}
	}

	switch (Hdr_type) {
	case BIN:
		Gen.g_magic = CMN_BIN;
		break;
	case CHR:
		Gen.g_magic = CMN_BIN;
		break;
	case ASC:
		Gen.g_magic = CMN_ASC;
		break;
	case CRC:
		Gen.g_magic = CMN_CRC;
		break;
	case USTAR:
		/* If the length of the fullname is greater than 256,
	   	print out a message and return.
		*/

		len = strlen(Gen.g_nam_p);
		if (len > sizeof(fullnam)) {
			msg(ERR, ":85", "%s: file name too long", Gen.g_nam_p);
			return(0);
		}
		memset(prefix,0,sizeof(prefix));
		if (len > NAMSIZ) {
			memset(filname,0,sizeof(filname));
			cp = wholename = Gen.g_nam_p;
			cp += (PREFIXSIZ > len) ? len : PREFIXSIZ;
smore:
			while ( cp > wholename && *cp != '/' ) /* find a '/' */
				cp--;
			if ( cp == wholename ) { /* didn't find '/' in prefix side */
				msg(WARN, _SGI_MMX_cpio_prefix, "%s: Cannot find \"/\" in prefix string.",Gen.g_nam_p);
				return (0);
			}
			/* Name will fit in 100 char array - Don't count "/" */
			if ( strlen(cp + 1) > NAMSIZ ) {
				msg(WARN, ":86", "%s: filename is greater than %d",(cp + 1),NAMSIZ);
				return (0);
			}
			if ( *(cp+1) == '\0' ) {	/* BSD tar dirs end with '/' */
				--cp;
				goto smore;
			}
			strncpy(prefix,wholename,cp - wholename);
			strncpy(filname,cp+1, len - (cp - wholename) - 1);
			Gen.g_tname=filname;
		} else {
			Gen.g_tname = Gen.g_nam_p;
		}
		Gen.g_prefix=prefix;

		(void)strcpy(Gen.g_tmagic, "ustar");
		(void)strcpy(Gen.g_version, "00");
		dpasswd = getpwuid(SrcSt.st_uid);
		if (dpasswd == (struct passwd *) NULL)
			msg(WARN, badpasswdid, badpasswd, Gen.g_nam_p);
		else {
			(void)strncpy(&Gen.g_uname[0], dpasswd->pw_name, 32);
			Gen.g_uname[31] = 0;
		}
		dgroup = getgrgid(SrcSt.st_gid);
		if (dgroup == (struct group *) NULL) {
			msg(WARN, badgroupid, badgroup, Gen.g_nam_p);
			Gen.g_gname[0] = 0;
		}
		else {
			(void)strncpy(&Gen.g_gname[0], dgroup->gr_name, 32);
			Gen.g_gname[31] = 0;
		}
		switch(ftype) {
			case S_IFDIR:
				Gen.g_typeflag = '5';
				break;
			case S_IFREG:
				if (SrcSt.st_nlink > 1)
					Gen.g_typeflag = '1';
				else
				Gen.g_typeflag = '0';
				break;
			case S_IFLNK:
				Gen.g_typeflag = '2';
				break;
			case S_IFBLK:
				Gen.g_typeflag = '4';
				break;
			case S_IFCHR:
				Gen.g_typeflag = '3';
				break;
			case S_IFIFO:
				Gen.g_typeflag = '6';
				break;
		}
	/* FALLTHROUGH*/
	case TAR:
		T_lname[0] = '\0';
		break;
	default:
		msg(EXT, badhdrid, badhdr);
	}

	Gen.g_namesz = strlen(Gen.g_nam_p) + 1;
	Gen.g_uid = SrcSt.st_uid;
	Gen.g_gid = SrcSt.st_gid;
	Gen.g_dev = SrcSt.st_dev;
	Gen.g_ino = SrcSt.st_ino;
	Gen.g_mode = SrcSt.st_mode;
	Gen.g_mtime = SrcSt.st_mtime;
	Gen.g_nlink = SrcSt.st_nlink;
	/* Added for xfs support */
	if (SrcSt.st_size >= BIGBLOCK && !Kflag && (Args & OCo)) {
		msg(POST, ":141", "Skipping file -> %s\nMust use -K option to archive this file", Gen.g_nam_p);
		return(0);
	}
	if (SrcSt.st_size >= BIGBLOCK && Kflag && (Args & OCo) && !portwarn) {
		portwarn = 1;
		msg(EPOST, ":144", "Warning: Inclusion of file -> %s will create a non-portable archive", Gen.g_nam_p);
	}
	if (ftype == S_IFREG || ftype == S_IFLNK) {
		if (SrcSt.st_size >= BIGBLOCK) {
			Gen.g_filesz = -(SrcSt.st_size/ BIGBLOCK);
			Gen.g_filesz2 = SrcSt.st_size% BIGBLOCK;
		}
		else	{
			Gen.g_filesz = SrcSt.st_size;
			Gen.g_filesz2 = 0L;
		}
	} else	{
		/* large dev_t handling with -K in binary mode; see comments
		 * in read_hdr() and getname() */
		if(!Kflag || Hdr_type != BIN || SrcSt.st_rdev != 0xffff)
			Gen.g_filesz = 0LL;
		Gen.g_filesz2 = 0L;
	}
	Gen.g_rdev = SrcSt.st_rdev;
	return(1);
}

/*
 * creat_lnk: Create a link from the existing name1_p to name2_p.
 */

static int
creat_lnk(char *name1_p, char *name2_p)
{
	register int cnt = 0;

	do {
		errno = 0;
		if (!link(name1_p, name2_p)) {
			cnt = 0;
			break;
		} else if (errno == EEXIST) {
			if (!(Args & OCu) && G_p->g_mtime <= DesSt.st_mtime && (cnt == 0)) {
				msg(POST, sameageid, sameage, name2_p);
				return(0);
			} else if (unlink(name2_p) < 0)
				msg(ERRN, ":88", "Cannot unlink \"%s\"", name2_p);
		}
		cnt++;
	} while ((cnt < 2) && !missdir(name2_p));
	if (!cnt) {
		if (Args & OCv)
			(void)pfmt(Err_p, MM_NOSTD, ":89:%s linked to %s\n", name1_p, name2_p);
		VERBOSE((Args & (OCv | OCV)), name2_p);
	} else if (cnt == 1)
		msg(ERRN, badcreatdirid, badcreatdir, name2_p);
	else if (cnt == 2)
		msg(ERRN, ":90", "Cannot link \"%s\" and \"%s\"", name1_p, name2_p);
	return(cnt);
}

/*
 * creat_spec:
 */

static int
creat_spec(void)
{
	register char *nam_p;
	register int cnt, result, rv = 0;
	char *curdir;

	if (Args & OCp)
		nam_p = Fullnam_p;
	else
		nam_p = G_p->g_nam_p;
	result = stat64(nam_p, &DesSt);
	if (Adir) {
		curdir = strrchr(nam_p, '.');
		if (curdir != NULL && curdir[1] == NULL)
			return(1);
		else {
			if (!result && (Args & OCd)) {
				/* this dir already exists.  if we've just
				 * created it with missdir(), the permissions
				 * may not be right.  now that we know what
				 * they should be, fix them.
				 */
				Fix_mode = 1;
				rstfiles(U_KEEP);
				Fix_mode = 0;
				return(1);
			}
			if (!result || !(Args & OCd) || !strcmp(nam_p, ".") || !strcmp(nam_p, ".."))
				return(1);
		}
	} else if (!result && creat_tmp(nam_p) < 0)
		return(0);
	cnt = 0;
	do {
		(void)umask(Orig_umask);
		if (Adir)
			result = mkdir(nam_p, (int)G_p->g_mode);
		else if (Aspec)
			result = mknod(nam_p, (int)G_p->g_mode, (int)G_p->g_rdev);
		if (result >= 0) {
			cnt = 0;
			break;
		}
		cnt++;
	} while (cnt < 2 && !missdir(nam_p));
	(void)umask(0);
	switch (cnt) {
	case 0:
		rv = 1;
		rstfiles(U_OVER);
		break;
	case 1:
		msg(ERRN, badcreatdirid, badcreatdir, nam_p);
		if (*Over_p == '\0')
			rstfiles(U_KEEP);
		break;
	case 2:
		if (Adir)
			msg(ERRN, ":91", "Cannot create directory \"%s\"", nam_p);
		else if (Aspec)
			msg(ERRN, ":92", "mknod() failed for \"%s\"", nam_p);
		if (*Over_p == '\0')
			rstfiles(U_KEEP);
		break;
	default:
		msg(EXT, badcaseid, badcase);
	}
	return(rv);
}



/* returns count of extents if file is holey, else 0 */
getbmap(int fd)
{
	int cnt = 0, hole = 0, i;
	struct getbmap	bm[512];	/* enough for more than all but the largest
		files in a single syscall */

	bm[0].bmv_offset = 0;
	bm[0].bmv_length = -1;
	bm[0].bmv_count = sizeof(bm)/sizeof(bm[0]);

	for (;;) {
		if (fcntl(fd, F_GETBMAP, bm) < 0) {
			if(errno != EINVAL)
				perror("getbmap");
			/* else filesystem doesn't support holes/fcntl (like EFS),
			 * don't complain for that case... */
			break;
		}
		cnt += bm[0].bmv_entries;
		for (i= 1;bm[0].bmv_entries-- > 0; i++) {
			if (bm[i].bmv_block == -1)
					hole++;
		}
		if(i < sizeof(bm)/sizeof(bm[0]))
			break;
	}
	return hole ? cnt : 0;
}


/*
 * creat_tmp:
 */

static int
creat_tmp(char *nam_p)
{
	register char *t_p;

	if ((Args & OCp) && G_p->g_ino == DesSt.st_ino && G_p->g_dev == DesSt.st_dev) {
		msg(ERR, ":93", "Attempt to pass a file to itself.");
		return(-1);
	}
	if (G_p->g_mtime <= DesSt.st_mtime && !(Args & OCu)) {
		msg(POST, sameageid, sameage, nam_p);
		return(-1);
	}
	if (Uid && Aspec) {
		msg(ERR, ":94", "Cannot overwrite \"%s\"", nam_p);
		return(-1);
	}
	(void)strcpy(Over_p, nam_p);
	t_p = Over_p + strlen(Over_p);
	while (t_p != Over_p) {
		if (*(t_p - 1) == '/')
			break;
		t_p--;
	}
	(void)strcpy(t_p, "XXXXXX");
	(void)mktemp(Over_p);
	if (*Over_p == '\0') {
		msg(ERR, ":95", "Cannot get temporary file name.");
		return(-1);
	}
	if (rename(nam_p, Over_p) < 0) {
		msg(ERRN, ":96", "Cannot create temporary file");
		return(-1);
	}
	return(1);
}

/*
 * data_in:  If proc_mode == P_PROC, bread() the file's data from the archive 
 * and write(2) it to the open fdes gotten from openout().  If proc_mode ==
 * P_SKIP, or becomes P_SKIP (due to errors etc), bread(2) the file's data
 * and ignore it.  If the user specified any of the "swap" options (b, s or S),
 * and the length of the file is not appropriate for that action, do not
 * perform the "swap", otherwise perform the action on a buffer by buffer basis.
 * If the CRC header was selected, calculate a running checksum as each buffer
 * is processed.
 */

static void
data_in(int proc_mode)
{
	register char *nam_p;
	register int cnt, pad;
	register off64_t filesz;
	register long  cksumval = 0L;
	register int rv, swapfile = 0;
	int	bigflag = 0;
	off64_t tempsz;
	int cfile=-1, same=1, samex=1, islnk=0;
	struct stat st;
	char *lnkbuf;
	int chunks = 0, chunki;
	struct holemap *hm = 0;
	off64_t offs;

	nam_p = G_p->g_nam_p;
	if ((G_p->g_mode & Ftype) == S_IFLNK && proc_mode != P_SKIP) {
		if(Compareflag) {
			if(lstat(nam_p, &st) || (st.st_mode&Ftype) != S_IFLNK)
				same = 0;
			islnk = 1;
			FILL(Gen.g_filesz);
			lnkbuf = Buffr.b_out_p;
		}
		else {
			proc_mode = P_SKIP;
			VERBOSE((Args & (OCv | OCV)), nam_p);
		}
	}
	if (Args & (OCb | OCs | OCS)) { /* verfify that swapping is possible */
		swapfile = 1;
		if (Args & (OCs | OCb) && G_p->g_filesz % 2) {
			msg(ERR, ":98", "Cannot swap bytes of \"%s\", odd number of bytes", nam_p);
			swapfile = 0;
		}
		if (Args & (OCS | OCb) && G_p->g_filesz % 4) {
			msg(ERR, ":99", "Cannot swap halfwords of \"%s\", odd number of halfwords", nam_p);
			swapfile = 0;
		}
	}
	if(S_ISREG(G_p->g_mode) && G_p->g_rdev==1) { /* see bug 511930
		* for why we explictly check for 1, rather than non-zero */
		/* it's a holey file */
		struct holemap h;
		FILL(sizeof(*hm));	/* read first struct get chunk count */
		memcpy(&h, Buffr.b_out_p, sizeof(h));
		if (!h.offset && h.count) { /* if this is REALLY a holey file,
			* h.count will be the number of holemap structs (chunks)
			* and h.offset will be zero.  This check in necessary
			* because unfortunately some implementations of cpio
			* leave garbage in their data structures instead of
			* zeroing them.  See bugs 511930 and 525101. */
			chunks = h.count;
			Buffr.b_out_p += sizeof(h);
			Buffr.b_cnt -= (long)sizeof(h);
			hm = alloca(sizeof(*hm)*chunks);
			FILL(sizeof(*hm)*chunks);
			memcpy(hm, Buffr.b_out_p, sizeof(*hm)*chunks);
			Buffr.b_out_p += sizeof(*hm)*chunks;
			Buffr.b_cnt -= (long)sizeof(*hm)*chunks;
			chunki = 0;
			offs = 0;
		}
	}
	filesz = G_p->g_filesz;
	if ((int)filesz < 0) {
		filesz = tempsz = BIGBLOCK * -(int)filesz;
		bigflag++;
	}
	if(proc_mode == P_CMP && !islnk) {
		if((cfile = open(nam_p, O_RDONLY)) == -1)
			same = 0;
		else if(!bigflag && (fstat(cfile, &st) || st.st_size != filesz))
			same = 0;
	}

	while (filesz > 0) {
		if(hm && chunki<chunks) {
			if(offs >= hm[chunki].offset && hm[chunki].count < 0) {
				if(proc_mode == P_PROC) {
					/* hole, seek over it */
					if((offs=lseek(Ofile, -hm[chunki].count, SEEK_CUR)) == -1)
						msg(ERRN, ":1015", "\"%s\": lseek error skipping over hole",
							nam_p);
				}
				else if(proc_mode == P_CMP) {
					/* have to seek on compare file; or should we
					 * compare block maps?  Maybe someday */
					if(same && !islnk && 
						lseek(cfile, -hm[chunki].count, SEEK_CUR) == -1)
						same = 0;
					offs -= hm[chunki].count;
				}
				else
					offs -= hm[chunki].count;
				filesz += hm[chunki].count;
				chunki++;
				continue;
			}
			cnt = (unsigned)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
			if(cnt > hm[chunki].count && hm[chunki].count>0LL)
				cnt = hm[chunki].count;
		}
		else
			cnt = (int)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
		FILL(cnt);
		if (proc_mode != P_SKIP) {
			if (Hdr_type == CRC)
				cksumval += cksum(CRC, cnt);
			if (swapfile)
				swap(Buffr.b_out_p, cnt);
			errno = 0;
			if(proc_mode == P_CMP) {
				/* compare contents */
				if(same && !islnk) {
					if(read(cfile, cmpbuf, cnt) != cnt)
						same = 0;
					else if(memcmp(cmpbuf, Buffr.b_out_p, cnt))
						same = 0;
				}
			}
			else {
				rv = write(Ofile, Buffr.b_out_p, cnt);
				if (rv < cnt) {
					if (rv < 0)
						msg(ERRN, badwriteid, badwrite, nam_p);
					else
						msg(EXTN, badwriteid, badwrite, nam_p);
					proc_mode = P_SKIP;
					(void)close(Ofile);
					rstfiles(U_KEEP);
				}
			}
		}
		Buffr.b_out_p += cnt;
		Buffr.b_cnt -= (long)cnt;
		filesz -= (long)cnt;
		if(hm) {
			if(hm[chunki].count>0LL && (!(hm[chunki].count -= (off64_t)cnt)))
				chunki++;	/* next chunk */
			offs += (off64_t)cnt;
		}
	} /* filesz */
	pad = (Pad_val + 1 - (G_p->g_filesz & Pad_val)) & Pad_val;
	if (pad != 0) {
		FILL(pad);
		Buffr.b_out_p += pad;
		Buffr.b_cnt -= pad;
	}
	if (bigflag) {
		gethdr();
		G_p = &Gen;
		/* += because with holey files, filesz may not be zero at this point */
		filesz += G_p->g_filesz;
		/* have to do the fstat here for very large files... */
		if(fstat(cfile, &st) || st.st_size != (G_p->g_filesz+tempsz))
			same = 0;
		G_p->g_filesz += tempsz;
		VERBOSE((Args & OCt) && !Compareflag, G_p->g_nam_p);
		while (filesz > 0) {
			if(hm && chunki<chunks) {
				if(offs >= hm[chunki].offset && hm[chunki].count < 0) {
					/* hole, seek over it */
					if(proc_mode == P_PROC) {
						if((offs=lseek(Ofile, -hm[chunki].count, SEEK_CUR)) == -1)
							msg(ERRN, ":1015", "\"%s\": lseek error skipping over hole",
								nam_p);
					}
					else if(proc_mode == P_CMP) {
						/* have to seek on compare file; or should we
						 * compare block maps?  Maybe someday */
						if(same && !islnk && 
							lseek(cfile, -hm[chunki].count, SEEK_CUR) == -1)
							same = 0;
						offs -= hm[chunki].count;
					}
					else
						offs -= hm[chunki].count;
					filesz += hm[chunki].count;
					chunki++;
					continue;
				}
				cnt = (unsigned)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
				if(cnt > hm[chunki].count && hm[chunki].count>0LL)
					cnt = hm[chunki].count;
			}
			else
				cnt = (int)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
			FILL(cnt);
			if (proc_mode != P_SKIP) {
				if (Hdr_type == CRC)
					cksumval += cksum(CRC, cnt);
				if (swapfile)
					swap(Buffr.b_out_p, cnt);
				errno = 0;
				if(proc_mode == P_CMP) {
					/* compare contents */
					if(same) {
						if(read(cfile, cmpbuf, cnt) != cnt)
							same = 0;
						else if(memcmp(cmpbuf, Buffr.b_out_p, cnt))
							same = 0;
					}
				}
				else {
					rv = write(Ofile, Buffr.b_out_p, cnt);
					if (rv < cnt) {
						if (rv < 0)
							msg(ERRN, badwriteid, badwrite, nam_p);
						else
							msg(EXTN, badwriteid, badwrite, nam_p);
						proc_mode = P_SKIP;
						(void)close(Ofile);
						rstfiles(U_KEEP);
					}
				}
			}
			Buffr.b_out_p += cnt;
			Buffr.b_cnt -= (long)cnt;
			filesz -= (long)cnt;
			if(hm) {
				if(hm[chunki].count>0LL && (!(hm[chunki].count -= (off64_t)cnt)))
					chunki++;	/* next chunk */
				offs += (off64_t)cnt;
			}
		} /* filesz */
		pad = (Pad_val + 1 - (G_p->g_filesz & Pad_val)) & Pad_val;
		if (pad != 0) {
			FILL(pad);
			Buffr.b_out_p += pad;
			Buffr.b_cnt -= pad;
		}
	}
	if (proc_mode == P_CMP) {
		char *sptr;
		if(cfile != -1)
			(void)close(cfile);
		if(Compareflag > 1) {
			if(Gen.g_mode != st.st_mode ||
				Gen.g_uid != st.st_uid || Gen.g_gid != st.st_gid)
				samex = 0;
			if(islnk) {
				size_t l;
				if(Hdr_type == USTAR || Hdr_type == TAR) {
					lnkbuf = Thdr_p->tbuf.t_linkname;
					l = strlen(Thdr_p->tbuf.t_linkname);
					if(l > sizeof(Thdr_p->tbuf.t_linkname))
						l = sizeof(Thdr_p->tbuf.t_linkname); /* no null */
				}
				else
					l = G_p->g_filesz;
				if(readlink(nam_p, cmpbuf, PATH_MAX) != l)
					samex = 0;
				else if(strncmp(cmpbuf, lnkbuf, l))
					samex = 0;
			}
		}
		printf("%c%s", same ? '=' : '!', Compareflag>1 ? (samex?"= ":"! ") : " ");
		sptr = Buffr.b_out_p;
		Buffr.b_out_p = lnkbuf;	/* need this for islnk case for link targ */
		verbose(nam_p);
		Buffr.b_out_p = sptr;
	}
	else if (proc_mode == P_PROC) {
		(void)close(Ofile);
		if (Hdr_type == CRC && G_p->g_cksum != cksumval) {
			msg(ERR, ":100", "\"%s\" - checksum error", nam_p);
			rstfiles(U_KEEP);
		} else
			rstfiles(U_OVER);
		VERBOSE(Args&(OCv|OCV), nam_p);
	}
	Finished = 1;
}

/*
 * data_out:  open(2) the file to be archived, compute the checksum
 * of it's data if the CRC header was specified and write the header.
 * read(2) each block of data and bwrite() it to the archive.  For TARTYP (TAR
 * and USTAR) archives, pad the data with NULLs to the next 512 byte boundary.
 */

static void
data_out(void)
{
	register char *nam_p;
	register int cnt, rv, pad;
	register off64_t filesz, offs;
	int lnksz, bigflag = 0, firsterr=1, chunks = 0, chunki;
	struct holemap *hm = 0;

	nam_p = G_p->g_nam_p;
	if (Aspec) {
		write_hdr();
		rstfiles(U_KEEP);
		VERBOSE((Args & (OCv | OCV)), nam_p);
		return;
	}
	if ((G_p->g_mode & Ftype) == S_IFLNK && (Hdr_type != USTAR && Hdr_type != TAR)) { /* symbolic link */
		write_hdr();
		FLUSH(G_p->g_filesz+1);
		errno = 0;
		if (readlink(nam_p, Buffr.b_in_p, G_p->g_filesz) < 0) {
			msg(ERRN, badreadsymid, badreadsym, nam_p);
			return;
		}
		Buffr.b_in_p[G_p->g_filesz] = '\0';	 /* be sure... */
		Buffr.b_in_p += G_p->g_filesz;
		Buffr.b_cnt += G_p->g_filesz;
		pad = (Pad_val + 1 - (G_p->g_filesz & Pad_val)) & Pad_val;
		if (pad != 0) {
			FLUSH(pad);
			(void)memcpy(Buffr.b_in_p, Empty, pad);
			Buffr.b_in_p += pad;
			Buffr.b_cnt += pad;
		}
		VERBOSE((Args & (OCv | OCV)), nam_p);
		return;
	} else if ((G_p->g_mode & Ftype) == S_IFLNK && (Hdr_type == USTAR || Hdr_type == TAR)) {
		if ((lnksz=readlink(nam_p, T_lname, G_p->g_filesz)) < 0 || lnksz > NAMSIZ) {
			msg(ERRN, ":86", "%s: filename is greater than %d", T_lname, NAMSIZ);
			return;
		}
		T_lname[G_p->g_filesz] = '\0';
		G_p->g_filesz = 0LL;
		write_hdr();
		VERBOSE((Args & (OCv | OCV)), nam_p);
		return;
	}
	/* OLSON: this should be reconsidered for a possible new fcntl or
	 * use of a pad field in stat in the future; this is somewhat of a
	 * hack that can't be counted on "forever"; 4/97 */
	/* We open for writing as a mechanism for detecting swap files.  If
	 * the open returns EBUSY, and it's a regular file, then this is a swap
	 * file.  Opening it for writing is safe, since we won't actually write to
	 * it, but does mean we have some small additional risk of a bug in
	 * cpio trashing a file... Very IRIX specific... */
	if ((Ifile = open(nam_p, O_RDWR)) < 0) {
		struct stat st;
		if(errno == EBUSY && !stat(nam_p, &st) && S_ISREG(st.st_mode)) {
			msg(ERR, ":1013", "\"%s\" skipped because it appears to be a swapfile", nam_p);
			return;
		}
		else if ((Ifile = open(nam_p, O_RDONLY)) < 0) {
			msg(ERR, ":101", "\"%s\" ?", nam_p);
			return;
		}
	}
	if (Hdr_type == CRC && ((G_p->g_cksum = cksum(CRC, 0)) == (ulong)-1)) {
		msg(POST, ":102", "\"%s\" skipped", nam_p);
		(void)close(Ifile);
		return;
	}

	if(Holeflag) {
		struct getbmap *bm;
		chunks = getbmap(Ifile);
		if(chunks) {	/* has holes */
			if((sizeof(*bm)*(chunks+1)) > Buffr.b_size) {
				(void)close(Ifile);
				msg(ERR, ":1014", "\"%s\" skipped because holey file map too"
				 " large. Use at least -C %u", nam_p, sizeof(*bm)*(chunks+1));
				/*NOTREACHED*/
			}
			if(!portwarn) {
				portwarn = 1;
				msg(EPOST, ":144", "Warning: Inclusion of file -> %s will create a non-portable archive", Gen.g_nam_p);
			}
			bm = alloca(sizeof(*bm)*(chunks+1));
			bm[0].bmv_offset = 0;
			bm[0].bmv_length = -1;
			bm[0].bmv_count = sizeof(*bm)*(chunks+1);
			if (fcntl(Ifile, F_GETBMAP, bm) < 0) {
				msg(ERR, ":1012", "\"%s\" skipped due to error in processing information about holes", nam_p);
				/* this shouldn't ever happen */
				(void)close(Ifile);
				return;
			}

			/* this is the way we flag a holey file; it's a regular file
			 * and has a non-zero g_rdev/h_rdev; see check in data_in() */
			G_p->g_rdev = 1;
			hm = alloca(sizeof(*hm)*(chunks+1));
			hm[0].offset = 0;	/* unused */
			hm[0].count = chunks;	/* set number of holemap structs */
			for(chunki=1; chunki<=chunks; chunki++) {
				hm[chunki].offset = (++bm)->bmv_offset * 512;
				hm[chunki].count = bm->bmv_length * ((bm->bmv_block==-1)
					? -512 : 512);
			}
		}
	}

	write_hdr();
	filesz = G_p->g_filesz;
	if (filesz < 0) {
		filesz = BIGBLOCK * -filesz;
		bigflag++;
	}

	if(hm) {
		FLUSH(sizeof(*hm)*(chunks+1));
		memcpy(Buffr.b_in_p, hm, sizeof(*hm)*(chunks+1));
		Buffr.b_in_p += sizeof(*hm)*(chunks+1);
		Buffr.b_cnt += (long)sizeof(*hm)*(chunks+1);
		chunki = 1;
		offs = 0;
	}

	while (filesz > 0) {
		if(hm && chunki < chunks) {
			if(offs >= hm[chunki].offset && hm[chunki].count < 0) {
				if((filesz + hm[chunki].count) < 0)
					break;	/* get the rest in the bigflag chunk */
				/* hole, seek over it */
				if((offs=lseek(Ifile, -hm[chunki].count, SEEK_CUR)) == -1)
					msg(ERRN, ":1015", "\"%s\": lseek error skipping over hole",
						nam_p);
				filesz += hm[chunki].count;
				chunki++;
				continue;
			}
			cnt = (unsigned)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
			if(cnt > hm[chunki].count)
				cnt = hm[chunki].count;
		}
		else 
			cnt = (unsigned)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
		FLUSH(cnt);
		errno = 0;
		if ((rv = read(Ifile, Buffr.b_in_p, (unsigned)cnt)) < 0) {
			if(firsterr) {
				msg(ERRN, badreadid, badread, nam_p);
				firsterr=0;
			}
			/* need to pad to original size, same as tar, or following
			 * files in archive will be "lost"; this was handled wrong
			 * prior to irix 6.5; zero so it doesn't have random earlier
			 * data in it; this can happen when trying to backup a swap
			 * file, for example (before the swap file changes).  */
			memset(Buffr.b_in_p, 0, (size_t)cnt);
			rv = cnt;
		}
		Buffr.b_in_p += rv;
		Buffr.b_cnt += (long)rv;
		filesz -= (long)rv;

		if(hm) {
			if(!(hm[chunki].count -= (off64_t)rv))
				chunki++;
			offs += (off64_t)rv;
		}
	}
	pad = (Pad_val + 1 - (G_p->g_filesz & Pad_val)) & Pad_val;
	if (pad != 0) {
		FLUSH(pad);
		(void)memcpy(Buffr.b_in_p, Empty, pad);
		Buffr.b_in_p += pad;
		Buffr.b_cnt += pad;
	}
	if (bigflag) {
		G_p->g_filesz = G_p->g_filesz2;
		G_p->g_filesz2 = 0LL;
		filesz += G_p->g_filesz;
		write_hdr();
		while (filesz > 0) {
			if(hm && chunki < chunks) {
				if(offs >= hm[chunki].offset && hm[chunki].count < 0) {
					/* hole, seek over it */
					if((offs=lseek(Ifile, -hm[chunki].count, SEEK_CUR)) == -1)
						msg(ERRN, ":1015", "\"%s\": lseek error skipping over hole",
							nam_p);
					filesz += hm[chunki].count;
					chunki++;
					continue;
				}
				cnt = (unsigned)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
				if(cnt > hm[chunki].count)
					cnt = hm[chunki].count;
			}
			else 
				cnt = (unsigned)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
			FLUSH(cnt);
			errno = 0;
			if ((rv = read(Ifile, Buffr.b_in_p, (unsigned)cnt)) < 0) {
				msg(ERRN, badreadid, badread, nam_p);
				break;
			}
			Buffr.b_in_p += rv;
			Buffr.b_cnt += (long)rv;
			filesz -= (long)rv;
			if(hm) {
				if(!(hm[chunki].count -= (off64_t)rv))
					chunki++;
				offs += (off64_t)rv;
			}
		}
		pad = (Pad_val + 1 - (G_p->g_filesz & Pad_val)) & Pad_val;
		if (pad != 0 ) {
			FLUSH(pad);
			(void)memcpy(Buffr.b_in_p, Empty, pad);
			Buffr.b_in_p += pad;
			Buffr.b_cnt += pad;
		}
	}
	(void)close(Ifile);
	rstfiles(U_KEEP);
	VERBOSE((Args & (OCv | OCV)), nam_p);
}

/*
 * data_pass:  If not a special file (Aspec), open(2) the file to be 
 * transferred, read(2) each block of data and write(2) it to the output file
 * Ofile, which was opened in file_pass().
 */

static void
data_pass(void)
{
	register int cnt, done = 1;
	register off64_t filesz;

	if (Aspec) {
		(void)close(Ofile);
		rstfiles(U_KEEP);
		VERBOSE((Args & (OCv | OCV)), Nam_p);
		return;
	}
	if ((Ifile = open(Nam_p, 0)) < 0) {
		msg(ERRN, ":103", "Cannot open \"%s\", skipped", Nam_p);
		(void)close(Ofile);
		rstfiles(U_KEEP);
		return;
	}
	filesz = G_p->g_filesz;
	if (filesz < 0)
		filesz = BIGBLOCK * -filesz + G_p->g_filesz2;
	while (filesz > 0) {
		cnt = (unsigned)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
		errno = 0;
		if (read(Ifile, Buf_p, (unsigned)cnt) < 0) {
			msg(ERRN, badreadid, badread, Nam_p);
			done = 0;
			break;
		}
		errno = 0;
		if (write(Ofile, Buf_p, (unsigned)cnt) < 0) {
			msg(ERRN, badwriteid, badwrite, Fullnam_p);
			done = 0;
			break;
		}
		Blocks += ((cnt + (BUFSZ - 1)) / BUFSZ);
		filesz -= (long)cnt;
	}
	(void)close(Ifile);
	(void)close(Ofile);
	if (done)
		rstfiles(U_OVER);
	else
		rstfiles(U_KEEP);
	VERBOSE((Args & (OCv | OCV)), Fullnam_p);
	Finished = 1;
}


/* compare filetype.  Maybe with Compareflag>1 also check owner and perms
 * at some point
*/
void
compare_spec(void)
{
	struct stat st;
	int same = 1, samex = 1;;

	if(stat(G_p->g_nam_p, &st) == -1)
		same = 0;
	else if((Gen.g_mode & Ftype) != (st.st_mode & Ftype))
		same = 0;
	else if(Compareflag > 1 && (Gen.g_mode != st.st_mode ||
		Gen.g_uid != st.st_uid || Gen.g_gid != st.st_gid))
		samex = 0;
	printf("%c%s", same ? '=' : '!', Compareflag>1 ? (samex?"= ":"! ") : " ");
	verbose(G_p->g_nam_p);
}


/*
 * file_in:  Process an object from the archive.  If a TARTYP (TAR or USTAR)
 * archive and g_nlink == 1, link this file to the file name in t_linkname 
 * and return.  Handle linked files in one of two ways.  If Onecopy == 0, this
 * is an old style (binary or -c) archive, create and extract the data for the 
 * first link found, link all subsequent links to this file and skip their data.
 * If Oncecopy == 1, save links until all have been processed, and then 
 * process the links first to last checking their names against the patterns
 * and/or asking the user to rename them.  The first link that is accepted 
 * for xtraction is created and the data is read from the archive.
 * All subsequent links that are accepted are linked to this file.
 */

static void
file_in(void)
{
	register struct Lnk *l_p, *tl_p;
	int lnkem = 0, cleanup = 0;
	int proc_file;
	struct Lnk *ttl_p;

	G_p = &Gen;

	/* TAR and USTAR */
	if ((Hdr_type == USTAR || Hdr_type == TAR) && G_p->g_nlink == 1 && !(Args & OCt)) {
		(void)creat_lnk(Thdr_p->tbuf.t_linkname, G_p->g_nam_p);
		return;
	}

	if (Adir) {
		if (ckname() != F_SKIP) {
			if(Compareflag)
				compare_spec();
			else if(creat_spec() > 0)
				VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
		}
		return;
	}
	if (G_p->g_nlink == 1 || (Hdr_type == TAR || Hdr_type == USTAR)) {
		if (Aspec) {
			if (ckname() != F_SKIP) {
				if(Compareflag)
					compare_spec();
				else if(creat_spec() > 0)
					VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
			}
		} else {
			if (ckname() == F_SKIP)
				data_in(P_SKIP);
			else if(Compareflag)
				data_in(P_CMP);
			else
				data_in((Ofile = openout()) < 0 ? P_SKIP : P_PROC);
		}
		return;
	}
	tl_p = add_lnk(&ttl_p);
	l_p = ttl_p;
	if (l_p->L_cnt == l_p->L_gen.g_nlink)
		cleanup = 1;
	if (!Onecopy) {
		lnkem = (tl_p != l_p) ? 1 : 0;
		G_p = &tl_p->L_gen;
		if (ckname() == F_SKIP) {
			data_in(P_SKIP);
		}
		else {
			if (!lnkem) {
				if (Aspec) {
					if(Compareflag)
						compare_spec();
					else if (creat_spec() > 0)
						VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
				} else if(Compareflag)
					data_in(P_CMP);
				else if ((Ofile = openout()) < 0) {
					data_in(P_SKIP);
					reclaim(l_p);
					return;
				} else
					data_in(P_PROC);
			} else {
				(void)strcpy(Lnkend_p, l_p->L_gen.g_nam_p);
				(void)strcpy(Full_p, tl_p->L_gen.g_nam_p);
				if(!Compareflag) {
					(void)creat_lnk(Lnkend_p, Full_p); 
					data_in(P_SKIP);
				} else if(Aspec) {
					compare_spec();
					data_in(P_SKIP);
				} else
					data_in(P_CMP);
				l_p->L_lnk_p = (struct Lnk *)NULL;
				free(tl_p->L_gen.g_nam_p);
				free(tl_p);
			}
		}
	} else { /* Onecopy */
		if (tl_p->L_gen.g_filesz)
			cleanup = 1;
		if (!cleanup)
			return; /* don't do anything yet */
		tl_p = l_p;
		while (tl_p != (struct Lnk *)NULL) {
			G_p = &tl_p->L_gen;
			if ((proc_file = ckname()) != F_SKIP) {
				if (l_p->L_data) {
					(void)creat_lnk(l_p->L_gen.g_nam_p, G_p->g_nam_p);
				} else if (Aspec) {
					if(Compareflag)
						compare_spec();
					else
						(void)creat_spec();
					l_p->L_data = 1;
					VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
				} else if(Compareflag)
					data_in(P_CMP);
				else if ((Ofile = openout()) < 0) {
					proc_file = F_SKIP;
				} else {
					data_in(P_PROC);
					l_p->L_data = 1;
				}
			} /* (proc_file = ckname()) != F_SKIP */
			tl_p = tl_p->L_lnk_p;
			if (proc_file == F_SKIP && !cleanup) {
				tl_p->L_nxt_p = l_p->L_nxt_p;
				tl_p->L_bck_p = l_p->L_bck_p;
				l_p->L_bck_p->L_nxt_p = tl_p;
				l_p->L_nxt_p->L_bck_p = tl_p;
				free(l_p->L_gen.g_nam_p);
				free(l_p);
			}
		} /* tl_p->L_lnk_p != (struct Lnk *)NULL */
		if (l_p->L_data == 0) {
			data_in(P_SKIP);
		}
	}
	if (cleanup)
		reclaim(l_p);
}

/*
 * file_out:  If the current file is not a special file (!Aspec) and it
 * is identical to the archive, skip it (do not archive the archive if it
 * is a regular file).  If creating a TARTYP (TAR or USTAR) archive, the first
 * time a link to a file is encountered, write the header and file out normally.
 * Subsequent links to this file put this file name in their t_linkname field.
 * Otherwise, links are handled in one of two ways, for the old headers
 * (i.e. binary and -c), linked files are written out as they are encountered.
 * For the new headers (ASC and CRC), links are saved up until all the links
 * to each file are found.  For a file with n links, write n - 1 headers with
 * g_filesz set to 0, write the final (nth) header with the correct g_filesz
 * value and write the data for the file to the archive.
 */

static void
file_out(void)
{
	register struct Lnk *l_p, *tl_p;
	register int cleanup = 0;
	struct Lnk *ttl_p;

	G_p = &Gen;
	if (!Aspec && !ArchSt.st_rdev && IDENT(SrcSt, ArchSt)) {
		/* only skip if archive isn't a device; this extra check
		 * keeps us from skipping files that happen to have a
		 * a faked inode/dev from new_stat64() that match the real
		 * device we are archiving to; that happens a lot more with the
		 * hwgraph, because the major number is 0.  Also *report* when
		 * we skip something; it should never be silent!  Bug #461672 */
		msg(POST, ":102", "\"%s\" skipped",G_p->g_nam_p);
		return; /* do not archive the archive if it's a regular file */
	}

	if ((Hdr_type == USTAR) || (Hdr_type == TAR)) { /* TAR and USTAR */
		if (Adir) {
			write_hdr();
			VERBOSE((Args & (OCv|OCV)), G_p->g_nam_p);
			return;
		}
		if (G_p->g_nlink == 1) {
			data_out();
			return;
		}
		tl_p = add_lnk(&ttl_p);
		l_p = ttl_p;
		if (tl_p == l_p) { /* first link to this file encountered */
			data_out();
			return;
		}
		(void)strncpy(T_lname, l_p->L_gen.g_nam_p, l_p->L_gen.g_namesz);
		write_hdr();
		VERBOSE((Args & (OCv | OCV)), tl_p->L_gen.g_nam_p);
		free(tl_p->L_gen.g_nam_p);
		free(tl_p);
		return;
	}
	if (Adir) {
		write_hdr();
		VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
		return;
	}
	if (G_p->g_nlink == 1) {	/* symlink */
		data_out();
		return;
	} else {			/* other hardlink, reg. file, ... */
		tl_p = add_lnk(&ttl_p);
		l_p = ttl_p;
		if (l_p->L_cnt == l_p->L_gen.g_nlink)
			cleanup = 1;
		else if (Onecopy)
			return; /* don't process data yet */
	}
	if (Onecopy) {
		tl_p = l_p;
		while (tl_p->L_lnk_p != (struct Lnk *)NULL) {
			G_p = &tl_p->L_gen;
        		if (Hdr_type == CRC){
		        	if ((Ifile = open(G_p->g_nam_p, O_RDONLY)) < 0) {
                			msg(ERR, ":101", "\"%s\" ?", G_p->g_nam_p);
                			return;
        			}
				if ((G_p->g_cksum = cksum(CRC, 0)) == (ulong)-1) {
                			msg(POST, ":102", "\"%s\" skipped",G_p->g_nam_p);
                			(void)close(Ifile);
                			return;
				}	
                		(void)close(Ifile);
        		}
			G_p->g_filesz = 0LL;
			write_hdr();
			VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
			tl_p = tl_p->L_lnk_p;
		}
		G_p = &tl_p->L_gen;
	}
	data_out();
	if (cleanup)
		reclaim(l_p);
}

/*
 * file_pass:  If the -l option is set (link files when possible), and the 
 * source and destination file systems are the same, link the source file 
 * (G_p->g_nam_p) to the destination file (Fullnam) and return.  If not a 
 * linked file, transfer the data.  Otherwise, the first link to a file 
 * encountered is transferred normally and subsequent links are linked to it.
 * It should be noted that when using the "pass" option that an internal file
 * structure is used to pass the device number (structure "Gen", device number
 * types are ulongs) thus, this is the reason "pass" will do the "right"
 * thing when passing major/minor numbers for block or character devices.
 */

static void
file_pass(void)
{
	register struct Lnk *l_p, *tl_p;
	struct Lnk *ttl_p;
	char *save_name;

	G_p = &Gen;
	if (Adir && !(Args & OCd)) {
		msg(ERR, ":104", "Use -d option to copy \"%s\"", G_p->g_nam_p);
		return;
	}
	save_name = G_p->g_nam_p;
	while (*(G_p->g_nam_p) == '/')
		G_p->g_nam_p++;
	(void)strcpy(Full_p, G_p->g_nam_p);
	if ((Args & OCl) && !Adir && creat_lnk(G_p->g_nam_p, Fullnam_p) == 0)
		return;
	if ((G_p->g_mode & Ftype) == S_IFLNK && !(Args & OCL)) {
		errno = 0;
		if (readlink(save_name, Symlnk_p, G_p->g_filesz) < 0) {
			msg(ERRN, badreadsymid, badreadsym, save_name);
			return;
		}
		errno = 0;
		(void)missdir(Fullnam_p);
		*(Symlnk_p + G_p->g_filesz) = '\0';
		if (symlink(Symlnk_p, Fullnam_p) < 0) {
			if (errno == EEXIST) {
				if (openout() < 0) {
					return;
				}
			} else {
				msg(ERRN, badcreateid, badcreate, Fullnam_p);
				return;
			}
		}
		rstfiles(U_OVER);
		VERBOSE((Args & (OCv | OCV)), Fullnam_p);
		return;
	}
	if (!Adir && G_p->g_nlink > 1) {
		tl_p = add_lnk(&ttl_p);
		l_p = ttl_p;
		if (tl_p == l_p) /* was not found */
			G_p = &tl_p->L_gen;
		else { /* found */
			(void)strcpy(Lnkend_p, l_p->L_gen.g_nam_p);
			(void)strcpy(Full_p, tl_p->L_gen.g_nam_p);
			(void)creat_lnk(Lnknam_p, Fullnam_p);
			l_p->L_lnk_p = (struct Lnk *)NULL;
			free(tl_p->L_gen.g_nam_p);
			free(tl_p);
			if (l_p->L_cnt == G_p->g_nlink) 
				reclaim(l_p);
			return;
		}
	}
	if (Adir || Aspec) {
		if (creat_spec() > 0)
			VERBOSE((Args & (OCv | OCV)), Fullnam_p);
	} else if ((Ofile = openout()) > 0)
		data_pass();
}

/*
 * flush_lnks: With new linked file handling, linked files are not archived
 * until all links have been collected.  When the end of the list of filenames
 * to archive has been reached, all files that did not encounter all their links
 * are written out with actual (encountered) link counts.  A file with n links 
 * (that are archived) will be represented by n headers (one for each link (the
 * first n - 1 have g_filesz set to 0)) followed by the data for the file.
 */

static void
flush_lnks(void)
{
	register struct Lnk *l_p, *tl_p;
	off64_t tfsize;
	int i;
	
  for (i = 0; i < NLNKHASH; i++) {
	l_p = Lnk_hd[i].L_nxt_p;
	while (l_p != &Lnk_hd[i]) {
		(void)strcpy(Gen.g_nam_p, l_p->L_gen.g_nam_p);
		if (stat64(Gen.g_nam_p, &SrcSt) == 0) { /* check if file exists */
			tl_p = l_p;
			(void)creat_hdr();
			Gen.g_nlink = l_p->L_cnt; /* "actual" link count */
			tfsize = Gen.g_filesz;
			Gen.g_filesz = 0LL;
			G_p = &Gen;
			while (tl_p != (struct Lnk *)NULL) {
				Gen.g_nam_p = tl_p->L_gen.g_nam_p;
				Gen.g_namesz = tl_p->L_gen.g_namesz;
				if (tl_p->L_lnk_p == (struct Lnk *)NULL) {
					Gen.g_filesz = tfsize;
					data_out();
					break;
				}
				write_hdr(); /* archive header only */
				VERBOSE((Args & (OCv | OCV)), Gen.g_nam_p);
				tl_p = tl_p->L_lnk_p;
			}
			Gen.g_nam_p = Nam_p;
		} else /* stat64(Gen.g_nam_p, &SrcSt) == 0 */
			msg(ERR, ":105", "\"%s\" has disapeared", Gen.g_nam_p);
		tl_p = l_p;
		l_p = l_p->L_nxt_p;
		reclaim(tl_p);
	} /* l_p != &Lnk_hd */
    }
}

/*
 * gethdr: Get a header from the archive, validate it and check for the trailer.
 * Any user specified Hdr_type is ignored (set to NONE in main).  Hdr_type is 
 * set appropriately after a valid header is found.  Unless the -k option is 
 * set a corrupted header causes an exit with an error.  I/O errors during 
 * examination of any part of the header cause gethdr to throw away any current
 * data and start over.  Other errors during examination of any part of the 
 * header cause gethdr to advance one byte and continue the examination.
 */

static int
gethdr(void)
{
	register ushort ftype;
	register int hit = NONE, cnt = 0;
	int goodhdr, hsize, offset;
	int bswap = 0;
	char *preptr;

	Gen.g_nam_p = Nam_p;
	do { /* hit == NONE && (Args & OCk) && Buffr.b_cnt > 0 */
		FILL(Hdrsz);
		switch (Hdr_type) {
		case NONE:
		case BIN:
			Binmag.b_byte[0] = Buffr.b_out_p[0];
			Binmag.b_byte[1] = Buffr.b_out_p[1];
			if ((Binmag.b_half == CMN_BIN) || 
			   (Binmag.b_half == CMN_BBS)) {
				hit = read_hdr(BIN);
				if (Hdr_type == NONE)
					bswap = 1;
				hsize = (int)(HDRSZ + Gen.g_namesz);
				break;
			}
			if (Hdr_type != NONE)
				break;
			/*FALLTHROUGH*/
		case CHR:
			if (!strncmp(Buffr.b_out_p, CMS_CHR, CMS_LEN)) {
				hit = read_hdr(CHR);
				hsize = (int)(CHRSZ + Gen.g_namesz);
				break;
			}
			if (Hdr_type != NONE)
				break;
			/*FALLTHROUGH*/
		case ASC:
			if (!strncmp(Buffr.b_out_p, CMS_ASC, CMS_LEN)) {
				hit = read_hdr(ASC);
				hsize = (int)(ASCSZ + Gen.g_namesz);
				break;
			}
			if (Hdr_type != NONE)
				break;
			/*FALLTHROUGH*/
		case CRC:
			if (!strncmp(Buffr.b_out_p, CMS_CRC, CMS_LEN)) {
				hit = read_hdr(CRC);
				hsize = (int)(ASCSZ + Gen.g_namesz);
				break;
			}
			if (Hdr_type != NONE)
				break;
			/*FALLTHROUGH*/
		case USTAR:
			Hdrsz = TARSZ;
			FILL(Hdrsz);
			if ((hit = read_hdr(USTAR)) == NONE) {
				Hdrsz = ASCSZ;
				break;
			}
			hit = USTAR;
			hsize = TARSZ;
			break;
			/*FALLTHROUGH*/
		case TAR:
			Hdrsz = TARSZ;
			FILL(Hdrsz);
			if ((hit = read_hdr(TAR)) == NONE) {
				Hdrsz = ASCSZ;
				break;
			}
			hit = TAR;
			hsize = TARSZ;
			break;
		default:
			msg(EXT, badhdrid, badhdr);
		} /* Hdr_type */
		Gen.g_nam_p = &nambuf[0];
		if (hit != NONE) {
			FILL(hsize);
			goodhdr = 1;
			if (Gen.g_namesz < 1)
				goodhdr = 0;
			if ((hit == USTAR) || (hit == TAR)) { /* TAR and USTAR */
				if (*Gen.g_nam_p == '\0') { /* tar trailer */
					goodhdr = 1;
				}
				else {
				
					G_p = &Gen;
					if (G_p->g_cksum != cksum(TARTYP, 0)) {
						goodhdr = 0;
						if (!strncmp(Buffr.b_out_p,"7070",4))
							msg(EXT, _SGI_MMX_cpio_bswap, "Byte swapped data - re-try with correct device");
						msg(ERR, ":106", "Bad header - checksum error.");
					}
				}
			} else { /* binary, -c, ASC and CRC */
				if (Gen.g_nlink <= (ulong)0)
					goodhdr = 0;
				if (*(Buffr.b_out_p + hsize - 1) != '\0')
					goodhdr = 0;
			}
			if (!goodhdr) {
				hit = NONE;
				if (!(Args & OCk))
					break;
				msg(ERR, ":107", "Corrupt header, file(s) may be lost.");
			} else {
				FILL(hsize);
			}
		} /* hit != NONE */
		if (hit == NONE) {
			Buffr.b_out_p++;
			Buffr.b_cnt--;
			if (!(Args & OCk))
				break;
			if (!cnt++)
				msg(ERR, ":108", "Searching for magic number/header.");
		}
	} while (hit == NONE);
	if (hit == NONE) {
		if (Hdr_type == NONE)
			msg(EXT, ":109", "Not a cpio file, bad header.");
		else
			msg(EXT, ":110", "Bad magic number/header.");
	} else if (cnt > 0) {
		if(Args & OCv)
			fprintf(stderr, "(%d blocks) ", cnt);
		msg(EPOST, ":111", "Re-synchronized on magic number/header.");
	}
	if (Hdr_type == NONE) {
		Hdr_type = hit;
		switch (Hdr_type) {
		case BIN:
			if(bswap)
				Args |= BSM;
			Hdrsz = HDRSZ;
			Max_namesz = CPATH;
			Pad_val = HALFWD;
			Onecopy = 0;
			break;
		case CHR:
			Hdrsz = CHRSZ;
			Max_namesz = CPATH;
			Pad_val = 0;
			Onecopy = 0;
			break;
		case ASC:
		case CRC:
			Hdrsz = ASCSZ;
			Max_namesz = APATH;
			Pad_val = FULLWD;
			Onecopy = 1;
			break;
		case USTAR:
			Hdrsz = TARSZ;
			Max_namesz = HNAMLEN;
			Pad_val = FULLBK;
			Onecopy = 0;
			break;
		case TAR:
			Hdrsz = TARSZ;
			Max_namesz = TNAMLEN - 1;
			Pad_val = FULLBK;
			Onecopy = 0;
			break;
		default:
			msg(EXT, badhdrid, badhdr);
		} /* Hdr_type */
	} /* Hdr_type == NONE */
	if ((Hdr_type == USTAR) || (Hdr_type == TAR)) { /* TAR and USTAR */
		Gen.g_namesz = 0;
		if (Gen.g_nam_p[0] == '\0')
			return(0);
		else {
			preptr = &prebuf[0];
			if (*preptr != (char) NULL) {
				(void)strcpy(&fullnam[0], &prebuf[0]);
				strcat(&fullnam[0],"/");
				strcat(&fullnam[0],&nambuf[0]);
				Gen.g_nam_p = &fullnam[0];
			} else
				Gen.g_nam_p = &nambuf[0];
		}
	} else {
		(void)memcpy(Gen.g_nam_p, Buffr.b_out_p + Hdrsz, Gen.g_namesz);
		if (!strcmp(Gen.g_nam_p, "TRAILER!!!"))
			return(0);
	}
	offset = ((hsize + Pad_val) & ~Pad_val);
	Buffr.b_out_p += offset;
	Buffr.b_cnt -= (long)offset;
	ftype = (ushort)Gen.g_mode & Ftype;
	Adir = (ftype == S_IFDIR);
	Aspec = (ftype == S_IFBLK || ftype == S_IFCHR || ftype == S_IFIFO);
	return(1);
}

/*
 * getname: Get file names for inclusion in the archive.  When end of file
 * on the input stream of file names is reached, flush the link buffer out.
 * For each filename, remove leading "./"s and multiple "/"s, and remove
 * any trailing newline "\n".  Finally, verify the existance of the file,
 * and call creat_hdr() to fill in the gen_hdr structure.
 */

static int
getname(void)
{
	register int goodfile = 0, lastchar, namelen;

	while (!goodfile) {
		Gen.g_nam_p = Nam_p;
		if (fgets(Gen.g_nam_p, PATH_MAX, stdin) == (char *)NULL) {
			if (Onecopy && !(Args &OCp))
				flush_lnks();
			return(0);
		}
		namelen = strlen(Gen.g_nam_p);
		if (namelen > Max_namesz) {
			*(Gen.g_nam_p + namelen - 1) = '\0';
			msg(ERR, _SGI_MMX_cpio_maxlen, "Name %s exceeds maximum length (%d) - skipped.",Gen.g_nam_p, Max_namesz);
			return(2);
		}

		while (*Gen.g_nam_p == '.' && Gen.g_nam_p[1] == '/') {
			Gen.g_nam_p += 2;
			while (*Gen.g_nam_p == '/')
				Gen.g_nam_p++;
		}
		lastchar = strlen(Gen.g_nam_p) - 1;
		if (*(Gen.g_nam_p + lastchar) == '\n')
			*(Gen.g_nam_p + lastchar) = '\0';

		/* 
			Since we cannot return to the old version of stat (16 bit 
			dev and rdev field) - lets make these look like the old version
			in a 32 bit field iff the options being used are the binary
			(default) or the -Hodc is being used.

			If the format is binary or -Hodc change the dev, and rdev fields
			to reflect 16 bits instead of the new 32 bits.  Difference between
			old major/minor (8/8 bits) and new major/minor (14/18 bits).
			If it OVER_FLOW's then make it max bits for old major/minor.
		*/
		if (!lstat64(Gen.g_nam_p, &SrcSt)) {
			goodfile = 1;
			if ((SrcSt.st_mode & Ftype) == S_IFLNK && (Args & OCL)) {
				errno = 0;
				if (stat64(Gen.g_nam_p, &SrcSt) < 0) {
					msg(ERRN, badfollowid, badfollow, Gen.g_nam_p);
					goodfile = 0;
				}
			}

			if ( (Hdr_type == BIN || Hdr_type == CHR) && !(Args & OCp) ) {
				if (new_stat64(&SrcSt) < 0)
					msg(EXT, nomemid, nomem);
		
				if ( BLKORCHR(SrcSt.st_mode) )
					if ( OVER_FLOW(SrcSt.st_rdev) ) {
						if(Hdr_type == BIN && Kflag) {
						/* only for rdev; Olson, 4/97
						 * this makes us able to backup all
						 * the possible dev_t's with -K, which
						 * uses binary mode, no expanded rdev */
							if(!portwarn) {
								portwarn = 1;
								msg(EPOST, ":144", "Warning: Inclusion of file -> %s will create a non-portable archive", Gen.g_nam_p);
							}
							Gen.g_filesz = SrcSt.st_rdev;
							SrcSt.st_rdev = 0xffff;	/* flag it */
						}
						else {
							msg(WARN, _SGI_MMX_cpio_expdev, "Old format cannot support expanded device types on \"%s\" (device %d,%d)", Gen.g_nam_p, SrcSt.st_rdev >> NBITSMINOR, SrcSt.st_rdev & MAXMIN);
							SrcSt.st_rdev &= 0x00007fff;
						}
					}
					else {
						SrcSt.st_rdev = DEV32TO16(SrcSt.st_rdev);
					}

			}

		} else {
			msg(ERRN, badaccessid, badaccess, Gen.g_nam_p);
		}

	}
	if (creat_hdr())
		return(1);
	return(2);
}

/*
 * getpats: Save any filenames/patterns specified as arguments.
 * Read additional filenames/patterns from the file specified by the
 * user.  The filenames/patterns must occur one per line.
 */

static void
getpats(int largc, char **largv)
{
	register char **t_pp;
	register int len;
	register unsigned numpat = largc, maxpat = largc + 2;
	
	if ((Pat_pp = (char **)malloc(maxpat * sizeof(char *))) == (char **)NULL)
		msg(EXT, nomemid, nomem);
	t_pp = Pat_pp;
	while (*largv) {
		if ((*t_pp = (char *)malloc((unsigned int)strlen(*largv) + 1)) == (char *)NULL)
			msg(EXT, nomemid, nomem);
		(void)strcpy(*t_pp, *largv);
		t_pp++;
		largv++;
	}
	while (fgets(Nam_p, Max_namesz, Ef_p) != (char *)NULL) {
		if (numpat == maxpat - 1) {
			maxpat += 10;
			if ((Pat_pp = (char **)realloc((char *)Pat_pp, maxpat * sizeof(char *))) == (char **)NULL)
				msg(EXT, nomemid, nomem);
			t_pp = Pat_pp + numpat;
		}
		len = strlen(Nam_p); /* includes the \n */
		*(Nam_p + len - 1) = '\0'; /* remove the \n */
		*t_pp = (char *)malloc((unsigned int)len);
		if(*t_pp == (char *) NULL)
			msg(EXT, nomemid, nomem);
		(void)strcpy(*t_pp, Nam_p);
		t_pp++;
		numpat++;
	}
	*t_pp = (char *)NULL;
}

static void
ioerror(int dir)
{
	register int t_errno;
	register int archtype;

	t_errno = errno;
	errno = 0;

	if (rmtfstat(Archive, &ArchSt) < 0)
		msg(EXTN, badaccarchid, badaccarch);
	errno = t_errno;
	archtype = (int)ArchSt.st_mode & Ftype;
	if ((archtype != S_IFCHR) && (archtype != S_IFBLK)) {
		if (dir) {
			if (errno == EFBIG)
				msg(EXT, ":113", "ulimit reached for output file.");
			else if (errno == ENOSPC)
				msg(EXT, ":114", "No space left for output file.");
			else
				msg(EXTN, ":115", "I/O error - cannot continue");
		} else
                        msg(EXT, ":116", "Unexpected end-of-file encountered.");        
	} else {

		if (chknomedia(Archive)) {
			msg(EXT, _SGI_MMX_cpio_dooropen, "\007No tape in drive or drive door open");
		}

		if (dir)
                	msg(EXTN, ":117", "\007I/O error on output");
        	else
                	msg(EXTN, ":118", "\007I/O error on input");
	}
}

/*
 * matched: Determine if a filename matches the specified pattern(s).  If the
 * pattern is matched (the first return), return 0 if -f was specified, else
 * return 1.  If the pattern is not matched (the second return), return 0 if
 * -f was not specified, else return 1.
 */

static int
matched(void)
{
	register char *str_p = G_p->g_nam_p;
	register char **pat_pp = Pat_pp;

	while (*pat_pp) {
		if ((**pat_pp == '!' && !gmatch(str_p, *pat_pp + 1)) || gmatch(str_p, *pat_pp))
			return(!(Args & OCf)); /* matched */
		pat_pp++;
	}
	return(Args & OCf); /* not matched */
}

/*
 * missdir: Create missing directories for files.
 * (Possible future performance enhancement, if missdir is called, we know
 * that at least the very last directory of the path does not exist, therefore,
 * scan the path from the end 
 */

static int
missdir(char *nam_p)
{
	register char *c_p;
	register int cnt = 2;

	if (*(c_p = nam_p) == '/') /* skip over 'root slash' */
		c_p++;
	for (; *c_p; ++c_p) {
		if (*c_p == '/') {
			*c_p = '\0';
			if (stat64(nam_p, &DesSt) < 0) {
				if (Args & OCd) {
					(void)umask(Orig_umask);
					cnt = mkdir(nam_p, Def_mode);
					(void)umask(0);
					if (cnt != 0) {
						*c_p = '/';
						return(cnt);
					}
				} else {
					msg(ERR, ":119", "Missing -d option.");
					*c_p = '/';
					return(-1);
				}
			}
			*c_p = '/';
		}
	}
	if (cnt == 2) /* the file already exists */
		cnt = 0;
	return(cnt);
}

/*
 * mklong: Convert two shorts into one long.  For VAX, Interdata ...
 */

static ulong
mklong(short v[])
{
	
	union swpbuf swp_b;

	swp_b.s_word = 1;
	if (swp_b.s_byte[0]) {
		swp_b.s_half[0] = v[1];
		swp_b.s_half[1] = v[0];
	} else {
		swp_b.s_half[0] = v[0];
		swp_b.s_half[1] = v[1];
	}
	return swp_b.s_word;
}

/*
 * mkshort: Convert a long into 2 shorts, for VAX, Interdata ...
 */

static void
mkshort(short sval[], long v)
{
	union swpbuf *swp_p, swp_b;

	swp_p = (union swpbuf *)sval;
	swp_b.s_word = 1;
	if (swp_b.s_byte[0]) {
		swp_b.s_word = v;
		swp_p->s_half[0] = swp_b.s_half[1];
		swp_p->s_half[1] = swp_b.s_half[0];
	} else {
		swp_b.s_word = v;
		swp_p->s_half[0] = swp_b.s_half[0];
		swp_p->s_half[1] = swp_b.s_half[1];
	}
}

/*
 * msg: Print either a message (no error) (POST), an error message with or 
 * without the errno (ERRN or ERR), or print an error message with or without
 * the errno and exit (EXTN or EXT).
 */

static void
msg(int severity,...)
{
	register char *fmt_p, *fmt_pid;
	register FILE *file_p;
	va_list v_Args;
	int Errno = errno;

	if ((Args & OCV) && Verbcnt) { /* clear current line of dots */
		(void)fputc('\n', Out_p);
		Verbcnt = 0;
	}
	va_start(v_Args,severity);
	if (severity == POST)
		file_p = Out_p;
	else
		if (severity == EPOST)
			file_p = Err_p;
		else {
			file_p = Err_p;
			Error_cnt++;
		}
	fmt_pid = va_arg(v_Args, char *);
	fmt_p = va_arg(v_Args, char *);
	(void)fflush(Out_p);
	(void)fflush(Err_p);
	if ((severity != POST) && (severity != EPOST))
                (void)pfmt(file_p, severity == WARN?MM_WARNING:MM_ERROR, NULL);
        (void)vfprintf(file_p, fmt_pid ? gettxt(fmt_pid, fmt_p) : fmt_p, v_Args);
	if (severity == ERRN || severity == EXTN) {
                (void)pfmt(file_p, MM_NOSTD, ":120:, Errno %d, %s\n", Errno,
                        strerror(Errno));
	} else
		(void)fprintf(file_p, "\n");
	(void)fflush(file_p);
	va_end(v_Args);
        if (severity == EXT || severity == EXTN) {
                if (Error_cnt == 1)
                        (void)pfmt(file_p, MM_NOSTD, ":121:1 error\n");
                else
                        (void)pfmt(file_p, MM_NOSTD, ":122:%d errors\n", Error_cnt);
		exit(Error_cnt);
	}
}

/*
 * openout: Open files for output and set all necessary information.
 * If the u option is set (unconditionally overwrite existing files),
 * and the current file exists, get a temporary file name from mktemp(3C),
 * link the temporary file to the existing file, and remove the existing file.
 * Finally either creat(2), mkdir(2) or mknod(2) as appropriate.
 * 
 */

static int
openout(void)
{
	register char *nam_p;
	register int cnt, result;

	if (Args & OCp)
		nam_p = Fullnam_p;
	else
		nam_p = G_p->g_nam_p;
	if (Max_filesz < G_p->g_filesz && G_p->g_filesz >= 0) { 
		msg(ERR, ":123", "Skipping \"%s\": exceeds rlimit by %lld bytes ",
			nam_p, G_p->g_filesz - Max_filesz);
		return(-1);
	}
	if (!lstat64(nam_p, &DesSt) && creat_tmp(nam_p) < 0)
		return(-1);
	cnt = 0;
	do {
		(void)umask(Orig_umask);
		errno = 0;
		if ((G_p->g_mode & Ftype) == S_IFLNK) {
			if ((!(Args & OCp)) && !(Hdr_type == USTAR)) {
				FILL(Gen.g_filesz);
				(void)strncpy(Symlnk_p, Buffr.b_out_p, G_p->g_filesz);
				*(Symlnk_p + G_p->g_filesz) = '\0';
			} else if ((!(Args & OCp)) && (Hdr_type == USTAR)) {
				(void)strcpy(Symlnk_p, &Thdr_p->tbuf.t_linkname[0]);
			}
			if ((result = symlink(Symlnk_p, nam_p)) >= 0) {
				cnt = 0;
				if (Over_p != NULL) {
					(void)unlink(Over_p);
					*Over_p = '\0';
				}
				break;
			}
		} else if ((result = creat(nam_p, (int)G_p->g_mode)) >= 0) {
			cnt = 0;
			break;
		}
		cnt++;
	} while (cnt < 2 && !missdir(nam_p));
	(void)umask(0);
	switch (cnt) {
	case 0:
		if ((Args & OCi) && (Hdr_type == USTAR))
			setpasswd(nam_p);
		if ((G_p->g_mode & Ftype) == S_IFLNK) {
			if (!Uid) {
				if (Args & OCR) {
					if (lchown(nam_p, Rpw_p->pw_uid, Rpw_p->pw_gid) < 0)
						msg(ERRN, badchownid, badchown, nam_p);
				} else {
					if (lchown(nam_p, (int)G_p->g_uid, (int)G_p->g_gid) < 0)
						msg(ERRN, badchownid, badchown, nam_p);
				}
			}
			break;
		}
		if (!Uid && chown(nam_p, (int)G_p->g_uid, (int)G_p->g_gid) < 0)
			msg(ERRN, badchownid, badchown, nam_p);
		break;
	case 1:
		msg(ERRN, badcreatdirid, badcreatdir, nam_p);
		break;
	case 2:
		msg(ERRN, badcreateid, badcreate, nam_p);
		break;
	default:
		msg(EXT, badcaseid, badcase);
	}
	Finished = 0;
	return(result);
}

/*
 * read_hdr: Transfer headers from the selected format 
 * in the archive I/O buffer to the generic structure.
 */

static int
read_hdr(int hdr)
{
	register int rv = NONE;
	major_t maj, rmaj;
	minor_t min, rmin;
	char tmpnull;

	if (Buffr.b_end_p != (Buffr.b_out_p + Hdrsz)) {
        	tmpnull = *(Buffr.b_out_p + Hdrsz);
        	*(Buffr.b_out_p + Hdrsz) = '\0';
	}

	switch (hdr) {
	case BIN:
		(void)memcpy(&Hdr, Buffr.b_out_p, HDRSZ);

		if ((Hdr.h_magic == (short)CMN_BBS) && !(Args & OCs))
			msg(EXT, _SGI_MMX_cpio_bswap, "Byte swapped data - re-try with correct device");

		if (Hdr.h_magic == (short)CMN_BBS)
                        swap((char *)&Hdr,HDRSZ);

		Gen.g_magic = Hdr.h_magic;
		Gen.g_mode = Hdr.h_mode;
		Gen.g_uid = Hdr.h_uid;
		Gen.g_gid = Hdr.h_gid;
		Gen.g_nlink = Hdr.h_nlink;
		Gen.g_mtime = mklong(Hdr.h_mtime);
		Gen.g_ino = Hdr.h_ino;
		if (Hdr.h_dev <= 0x7fff) {
			maj = cpioMAJOR(Hdr.h_dev);
			min = cpioMINOR(Hdr.h_dev);
			Gen.g_dev = makedev(maj, min);

			/* only for rdev, not dev; Olson, 4/97; this makes us able to
			 * backup all the possible dev_t's with -K, which requires 
			 * binary mode, so no expanded rdev; this is supposed to be
			 * an illegal value for binary mode rdev, so we shouldn't see it
			 * in any non-irix archives... */
			if((ushort)Hdr.h_rdev == 0xffff)
				Gen.g_rdev = mklong(Hdr.h_filesize);
			else {
				rmaj = cpioMAJOR(Hdr.h_rdev);
				rmin = cpioMINOR(Hdr.h_rdev);
				Gen.g_rdev = makedev(rmaj,rmin);
			}
		}
		else {
			Gen.g_dev = Hdr.h_dev;
			if((ushort)Hdr.h_rdev == 0xffff) /* see comments above */
				Gen.g_rdev = mklong(Hdr.h_filesize);
			else
				Gen.g_rdev = Hdr.h_rdev;
		}
		Gen.g_cksum = 0L;
		Gen.g_filesz = mklong(Hdr.h_filesize);
		Gen.g_namesz = Hdr.h_namesize;
		rv = BIN;
		break;
	case CHR:
		if (sscanf(Buffr.b_out_p, "%6lo%6lo%6llo%6lo%6lo%6lo%6lo%6lo%11lo%6o%11llo",
		&Gen.g_magic, &Gen.g_dev, &Gen.g_ino, &Gen.g_mode, &Gen.g_uid, &Gen.g_gid,
		&Gen.g_nlink, &Gen.g_rdev, &Gen.g_mtime, &Gen.g_namesz, &Gen.g_filesz) == CHR_CNT) {
			rv = CHR;

			maj = cpioMAJOR(Gen.g_dev);
			rmaj = cpioMAJOR(Gen.g_rdev);
			min = cpioMINOR(Gen.g_dev);
			rmin = cpioMINOR(Gen.g_rdev);
			Gen.g_dev = makedev(maj, min);
			Gen.g_rdev = makedev(rmaj,rmin);
		}
		break;
	case ASC:
	case CRC:
		if (sscanf(Buffr.b_out_p, "%6lx%8llx%8lx%8lx%8lx%8lx%8lx%8llx%8x%8x%8x%8x%8x%8lx",
		&Gen.g_magic, &Gen.g_ino, &Gen.g_mode, &Gen.g_uid, &Gen.g_gid, &Gen.g_nlink, &Gen.g_mtime,
		&Gen.g_filesz, &maj, &min, &rmaj, &rmin, &Gen.g_namesz, &Gen.g_cksum) == ASC_CNT) {
			Gen.g_dev = makedev(maj, min);
			Gen.g_rdev = makedev(rmaj, rmin);
			rv = hdr;
		}
		break;
	case USTAR: /* TAR and USTAR */
		if (*Buffr.b_out_p == '\0') {
			*Gen.g_nam_p = '\0';
			nambuf[0] = '\0';
		 } else {
			Thdr_p = (union tblock *)Buffr.b_out_p;
			prebuf[0] = Gen.g_nam_p[0] = '\0';
			(void)sscanf(Thdr_p->tbuf.t_name, "%100s", nambuf);
			(void)sscanf(Thdr_p->tbuf.t_mode, "%8lo", &Gen.g_mode);
			(void)sscanf(Thdr_p->tbuf.t_uid, "%8lo", &Gen.g_uid);
			(void)sscanf(Thdr_p->tbuf.t_gid, "%8lo", &Gen.g_gid);
			(void)sscanf(Thdr_p->tbuf.t_size, "%12llo", &Gen.g_filesz);
			(void)sscanf(Thdr_p->tbuf.t_mtime, "%12lo", &Gen.g_mtime);
			(void)sscanf(Thdr_p->tbuf.t_cksum, "%8lo", &Gen.g_cksum);
			if (Thdr_p->tbuf.t_linkname[0] != (char)NULL)
				Gen.g_nlink = 1;
			else
				Gen.g_nlink = 0;

			switch (Thdr_p->tbuf.t_typeflag) {
				case '0' :	
				case '1' :		/* hard link */
					Gen.g_mode |= S_IFREG;
					Aspec = 0;
					Adir = 0;
					break;
				case '2' :		/* symlink */
					Gen.g_nlink = 2;
					Gen.g_mode |= S_IFLNK;
					Aspec = 0;
					Adir = 0;
					break;
				case '3' :
					Gen.g_mode |= S_IFCHR;
					Aspec = 1;
					break;
				case '4' :
					Gen.g_mode |= S_IFBLK;
					Aspec = 1;
					break;
				case '5' :
					Gen.g_mode |= S_IFDIR;
					Adir = 1;
					break;
				case '6' :
					Gen.g_mode |= S_IFIFO;
					Aspec = 1;
					break;
			}

			/*
			 * Do not need to be filled here used only to write out
			 * not read - also cause a core dump (#198374) because
			 * it is not properly aligned (char vs. long).
			 *
			 * (void)sscanf(Thdr_p->tbuf.t_magic, "%8lo", Gen.g_tmagic);
			 * (void)sscanf(Thdr_p->tbuf.t_version, "%8lo", Gen.g_version); 
			 */
			(void)sscanf(Thdr_p->tbuf.t_uname, "%31s", Gen.g_uname);
			(void)sscanf(Thdr_p->tbuf.t_gname, "%31s", Gen.g_gname);
			(void)sscanf(Thdr_p->tbuf.t_devmajor, "%8lo", &Gen.g_dev);
			(void)sscanf(Thdr_p->tbuf.t_devminor, "%8lo", &Gen.g_rdev);
			(void)sscanf(Thdr_p->tbuf.t_prefix, "%155s", prebuf);
			Gen.g_namesz = strlen(Gen.g_nam_p) + 1;
			Gen.g_rdev = makedev(Gen.g_dev, Gen.g_rdev);

			/* 
			 * I don't know why this is here???  Since no st_dev information
			 * is passed to any tar structure and "maj" and "min" are not 
			 * filled during the sscanf.
			 */
			Gen.g_dev = makedev(maj, min);
		}
		rv = USTAR;
		break;
	case TAR:
		if (*Buffr.b_out_p == '\0')
			*Gen.g_nam_p = '\0';
		else {
			Thdr_p = (union tblock *)Buffr.b_out_p;
			Gen.g_nam_p[0] = '\0';
			(void)sscanf(Thdr_p->tbuf.t_mode, "%lo", &Gen.g_mode);
			(void)sscanf(Thdr_p->tbuf.t_uid, "%lo", &Gen.g_uid);
			(void)sscanf(Thdr_p->tbuf.t_gid, "%lo", &Gen.g_gid);
			(void)sscanf(Thdr_p->tbuf.t_size, "%llo", &Gen.g_filesz);
			(void)sscanf(Thdr_p->tbuf.t_mtime, "%lo", &Gen.g_mtime);
			(void)sscanf(Thdr_p->tbuf.t_cksum, "%lo", &Gen.g_cksum);
			if (Thdr_p->tbuf.t_typeflag == '1')
				Gen.g_nlink = 1;
			else
				Gen.g_nlink = 0;
			(void)sscanf(Thdr_p->tbuf.t_name, "%s", Gen.g_nam_p);
			Gen.g_namesz = strlen(Gen.g_nam_p) + 1;
		}
		rv = TAR;
		break;
	default:
		msg(EXT, badhdrid, badhdr);
	}
	if (Buffr.b_end_p != (Buffr.b_out_p + Hdrsz))
		*(Buffr.b_out_p + Hdrsz) = tmpnull;
	return(rv);
}

/*
 * reclaim: Reclaim linked file structure storage.
 */

static void
reclaim( struct Lnk *l_p)
{
	register struct Lnk *tl_p;
	
	l_p->L_bck_p->L_nxt_p = l_p->L_nxt_p;
	l_p->L_nxt_p->L_bck_p = l_p->L_bck_p;
	while (l_p != (struct Lnk *)NULL) {
		tl_p = l_p->L_lnk_p;
		free(l_p->L_gen.g_nam_p);
		free(l_p);
		l_p = tl_p;
	}
}

/*
 * rstbuf: Reset the I/O buffer, move incomplete potential headers to
 * the front of the buffer and force bread() to refill the buffer.  The
 * return value from bread() is returned (to identify I/O errors).  On the
 * 3B2, reads must begin on a word boundary, therefore, with the -i option,
 * any remaining bytes in the buffer must be moved to the base of the buffer
 * in such a way that the destination locations of subsequent reads are
 * word aligned.
 */

static void
rstbuf(void)
{
 	register int pad;

	if ((Args & OCi) || Append) {
        	if (Buffr.b_out_p != Buffr.b_base_p) {
			pad = ((Buffr.b_cnt + FULLWD) & ~FULLWD);
			Buffr.b_in_p = Buffr.b_base_p + pad;
			pad -= Buffr.b_cnt;
			(void)memcpy(Buffr.b_base_p + pad, Buffr.b_out_p, (int)Buffr.b_cnt);
      			Buffr.b_out_p = Buffr.b_base_p + pad;
		}
		if (bfill() < 0)
			msg(EXT, ":124", "Unexpected end-of-archive encountered.");
	} else { /* OCo */
		(void)memcpy(Buffr.b_base_p, Buffr.b_out_p, (int)Buffr.b_cnt);
		Buffr.b_out_p = Buffr.b_base_p;
		Buffr.b_in_p = Buffr.b_base_p + Buffr.b_cnt;
	}
}

static void
setpasswd(char *nam)
{

	if ((dpasswd = getpwnam(&Gen.g_uname[0])) == (struct passwd *)NULL) {
		msg(WARN, badpasswdid, badpasswd, &Gen.g_uname[0]);
                msg(WARN, ":125", "%s: owner not changed", nam);
	} else
		Gen.g_uid = dpasswd->pw_uid;
	if ((dgroup = getgrnam(&Gen.g_gname[0])) == (struct group *)NULL) {
		msg(WARN, badgroupid, badgroup, &Gen.g_gname[0]);
                msg(WARN, ":126", "%s: group not changed", nam);
	} else
		Gen.g_gid = dgroup->gr_gid;
	G_p = &Gen;
}

/*
 * rstfiles:  Perform final changes to the file.  If the -u option is set,
 * and overwrite == U_OVER, remove the temporary file, else if overwrite
 * == U_KEEP, unlink the current file, and restore the existing version
 * of the file.  In addition, where appropriate, set the access or modification
 * times, change the owner and change the modes of the file.
 */

static void
rstfiles(int over)
{
	register char *inam_p, *onam_p, *nam_p;

	if (Args & OCp)
		nam_p = Fullnam_p;
	else
		if (Gen.g_nlink > (ulong)0) 
			nam_p = G_p->g_nam_p;
		else
			nam_p = Gen.g_nam_p;

	if ((Args & OCi) && (Hdr_type == USTAR))
		setpasswd(nam_p);
	if (over == U_KEEP && *Over_p != '\0') {
		msg(POST, ":127", "Restoring existing \"%s\"", nam_p);
		(void)unlink(nam_p);
		if (rename(Over_p, nam_p) < 0)
			msg(EXTN, badorigid, badorig, nam_p);
		*Over_p = '\0';
		return;
	} else if (over == U_OVER && *Over_p != '\0') {
		if (unlink(Over_p) < 0)
			msg(ERRN, badremtmpid, badremtmp, Over_p);
		*Over_p = '\0';
	}
	if (Args & OCp) {
		inam_p = Nam_p;
		onam_p = Fullnam_p;
	} else /* OCi only uses onam_p, OCo only uses inam_p */
		inam_p = onam_p = G_p->g_nam_p;
	if ((Args & OCm) && !S_ISLNK(G_p->g_mode))
		set_tym(onam_p, (time_t)G_p->g_mtime, (time_t)G_p->g_mtime);
	if (Uid == 0) {
		if (!(Args & OCo)) {
			if (!S_ISLNK(G_p->g_mode) && chmod(onam_p, (int)G_p->g_mode) < 0)
				msg(ERRN, badchmodid, badchmod, onam_p);
		}
	}
#ifdef sgi
	else if (Adir && Fix_mode) {
		if (!S_ISLNK(G_p->g_mode) && chmod(onam_p, (int)(G_p->g_mode&(~Orig_umask))) < 0)
			msg(ERRN, badchmodid, badchmod, onam_p);
	}
#endif
	if (Args & OCa)
		set_tym(inam_p, SrcSt.st_atime, SrcSt.st_mtime);
	if ((Args & OCR) && chown(onam_p, Rpw_p->pw_uid, Rpw_p->pw_gid) < 0)
			msg(ERRN, badchownid, badchown, onam_p);
	if ((Args & OCp) && !(Args & OCR)) {
		if (!Uid) {
			if (!S_ISLNK(G_p->g_mode) && chown(onam_p, (uid_t)G_p->g_uid, (uid_t)G_p->g_gid) < 0)
				msg(ERRN, badchownid, badchown, onam_p);
			else if (lchown(onam_p, (uid_t)G_p->g_uid, (uid_t)G_p->g_gid) < 0)
				msg(ERRN, badchownid, badchown, onam_p);
		}	
	} else { /* OCi only uses onam_p, OCo only uses inam_p */
		if (!(Args & OCR)) {
			if ((Args & OCi) && !Uid && (chown(inam_p, (uid_t)G_p->g_uid, (uid_t)G_p->g_gid) < 0))
				msg(ERRN, badchownid, badchown, onam_p);
		}
	}
}

/*
 * scan4trail: Scan the archive looking for the trailer.
 * When found, back the archive up over the trailer and overwrite 
 * the trailer with the files to be added to the archive.
 */

static void
scan4trail(void)
{
	register int rv;
	register long off1, off2;

	Append = 1;
	Hdr_type = NONE;
	G_p = (struct gen_hdr *)NULL;
	while (gethdr()) {
		G_p = &Gen;
		data_in(P_SKIP);
	}
	off1 = Buffr.b_cnt;
	off2 = Bufsize - (Buffr.b_cnt % Bufsize);
	Buffr.b_out_p = Buffr.b_in_p = Buffr.b_base_p;
	Buffr.b_cnt = 0L;

	if (rmtlseek(Archive, -(off1 + off2), SEEK_REL) < 0)
		msg(EXTN, badappendid, badappend);
	if ((rv = g_read(Device, Archive, Buffr.b_in_p, Bufsize)) < 0)
		msg(EXTN, badappendid, badappend);
	if (rmtlseek(Archive, -rv, SEEK_REL) < 0)
		msg(EXTN, badappendid, badappend);

	Buffr.b_cnt = off2;
	Buffr.b_in_p = Buffr.b_base_p + Buffr.b_cnt;
	Append = 0;
}

/*
 * setup:  Perform setup and initialization functions.  Parse the options
 * using getopt(3C), call ckopts to check the options and initialize various
 * structures and pointers.  Specifically, for the -i option, save any
 * patterns, for the -o option, check (via stat64(2)) the archive, and for
 * the -p option, validate the destination directory.
 */

static void
setup(int largc, char **largv)
{
	extern int optind;
	extern char *optarg;
	register char	*opts_p = "abcdfiklmoprstuvABC:DE:H:I:KTWLM:O:R:SV6",
                        *dupl_p = "Only one occurrence of -%c allowed",
                        *dupl_pid = ":128";
	register int option;
	int blk_cnt;
	int i;
	struct rlimit64 rl;

	Uid = getuid();
	Orig_umask = umask(0);
	Hdr_type = BIN;
	Efil_p = Hdr_p = Own_p = IOfil_p = NULL;

	if((largv[1] != NULL) && (*largv[1] != '-'))
			usage();

	while ((option = getopt(largc, largv, opts_p)) != EOF) {
		switch (option) {
		case 'a':	/* reset access time */
			Args |= OCa;
			break;
		case 'b':	/* swap bytes and halfwords */
			Args |= OCb;
			break;
		case 'c':	/* select character header */
			Args |= OCc;
			Hdr_type = ASC;
			Onecopy = 1;
			break;
		case 'd':	/* create directories as needed */
			Args |= OCd;
			break;
		case 'f':	/* select files not in patterns */
			Args |= OCf;
			break;
		case 'i':	/* "copy in" */
			Args |= OCi;
			Archive = 0;
			break;
		case 'k':	/* retry after I/O errors */
			Args |= OCk;
			break;
		case 'l':	/* link files when possible */
			Args |= OCl;
			break;
		case 'm':	/* retain modification time */
			Args |= OCm;
			break;
		case 'o':	/* "copy out" */
			Args |= OCo;
			Archive = 1;
			break;
		case 'p':	/* "pass" */
			Args |= OCp;
			Hdr_type = ASC;	/* for expanded dev_t stuff */
			break;
		case 'r':	/* rename files interactively */
			Args |= OCr;
			break;
		case 's':	/* swap bytes */
			Args |= OCs;
			break;
		case 't':	/* table of contents */
			Args |= OCt;
			break;
		case 'u':	/* copy unconditionally */
			Args |= OCu;
			break;
		case 'v':	/* verbose - print file names */
			Args |= OCv;
			break;
		case 'A':	/* append to existing archive */
			Args |= OCA;
			break;
		case 'B':	/* set block size to 5120 bytes */
			Args |= OCB;
			Bufsize = 5120;
			break;
		case 'C':	/* set arbitrary block size */
			if (Args & OCC)
				msg(ERR, dupl_pid, dupl_p, 'C');
			else {
				Args |= OCC;
				Bufsize = atoi(optarg);
			}
			break;
		case 'D':	/* undocumented flag for libpkg */
			Dflag = 1;
			break;
		case 'E':	/* alternate file for pattern input */
			if (Args & OCE)
				msg(ERR, dupl_pid, dupl_p, 'E');
			else {
				Args |= OCE;
				Efil_p = optarg;
			}
			break;
		case 'H':	/* select header type */
			if (Args & OCH)
				msg(ERR, dupl_pid, dupl_p, 'H');
			else {
				Args |= OCH;
				Hdr_p = optarg;
			}
			break;
		case 'I':	/* alternate file for archive input */
			if (Args & OCI)
				msg(ERR, dupl_pid, dupl_p, 'I');
			else {
				Args |= OCI;
				IOfil_p = optarg;
			}
			break;
		case 'K':	/* Don't ignore big files (used by xfs) */
			Kflag = 1;
			break;
		case 'L':	/* follow symbolic links */
			Args |= OCL;
			break;
		case 'M':	/* specify new end-of-media message */
			if (Args & OCM)
				msg(ERR, dupl_pid, dupl_p, 'M');
			else {
				Args |= OCM;
				Eom_p = optarg;
				Eom_pid = (char *)NULL;
			}
			break;
		case 'O':	/* alternate file for archive output */
			if (Args & OCO)
				msg(ERR, dupl_pid, dupl_p, 'O');
			else {
				Args |= OCO;
				IOfil_p = optarg;
			}
			break;
		case 'R':	/* change owner/group of files */
			if (Args & OCR)
				msg(ERR, dupl_pid, dupl_p, 'R');
			else {
				Args |= OCR;
				Own_p = optarg;
			}
			break;
		case 'S':	/* swap halfwords */
			Args |= OCS;
			break;
		case 'V':	/* print a dot '.' for each file */
			Args |= OCV;
			break;
		case 'T':	/* Test for differences (compare archive against filesystem) */
			Compareflag++;
			Args |= OCi|OCt;	/* implies -it */
			break;
		case 'W':	/* Deal with holey (xfs) files better by skipping hole */
			Holeflag = 1;
			break;
		case '6':	/* for old, sixth-edition files */
			Args |= OC6;
			Ftype = SIXTH;
			break;
		default:
			Error_cnt++;
		} /* option */
	} /* (option = getopt(largc, largv, opts_p)) != EOF */
	largc -= optind;
	largv += optind;
	ckopts(Args);
	if (!Error_cnt) {
		if ((Buf_p = align(CPIOBSZ)) == (char *)-1)
			msg(EXTN, nomemid, nomem);
		if ((Empty = align(TARSZ)) == (char *)-1)
			msg(EXTN, nomemid, nomem);
		if ((Args & OCr) && (Renam_p = (char *)malloc(APATH)) == (char *)NULL)
			msg(EXTN, nomemid, nomem);
		if ((Symlnk_p = (char *)malloc(APATH)) == (char *)NULL)
			msg(EXTN, nomemid, nomem);
		if ((Over_p = (char *)malloc(APATH)) == (char *)NULL)
			msg(EXTN, nomemid, nomem);
		if ((Nam_p = (char *)malloc(APATH)) == (char *)NULL)
			msg(EXTN, nomemid, nomem);
		if ((Fullnam_p = (char *)malloc(APATH)) == (char *)NULL)
			msg(EXTN, nomemid, nomem);
		if ((Lnknam_p = (char *)malloc(APATH)) == (char *)NULL)
			msg(EXTN, nomemid, nomem);
		Gen.g_nam_p = Nam_p;
		if (Args & OCi) {
			if (largc > 0) /* save patterns for -i option, if any */
				Pat_pp = largv;
			if (Args & OCE)
				getpats(largc, largv);
		} else if (Args & OCo) {
			if (largc != 0) /* error if arguments left with -o */
				Error_cnt++;
			else if (rmtfstat(Archive, &ArchSt) < 0)
				msg(ERRN, badaccarchid, badaccarch);
			switch (Hdr_type) {
			case BIN:
				Hdrsz = HDRSZ;
				Pad_val = HALFWD;
				break;
			case CHR:
				Hdrsz = CHRSZ;
				Pad_val = 0;
				break;
			case ASC:
			case CRC:
				Hdrsz = ASCSZ;
				Pad_val = FULLWD;
				break;
			case TAR:
			/* FALLTHROUGH */
			case USTAR: /* TAR and USTAR */
				Hdrsz = TARSZ;
				Pad_val = FULLBK;
				break;
			default:
				msg(EXT, badhdrid, badhdr);
			}
		} else { /* directory must be specified */
			if (largc != 1)
				Error_cnt++;
			else if (access(*largv, 2) < 0)
				msg(ERRN, badaccessid, badaccess, *largv);
		}
	}
	if (Error_cnt)
		usage(); /* exits! */
	if (Args & (OCi | OCo)) {
		long bsize;
		if (!Dflag) {
			if (Args & (OCB | OCC)) {
				if (g_init(&Device, &Archive) < 0)
					msg(EXTN, badinitid, badinit);
			} else {
				if ((Bufsize = g_init(&Device, &Archive)) < 0)
					msg(EXTN, badinitid, badinit);
			}
		}
		blk_cnt = _20K / Bufsize;
		blk_cnt = (blk_cnt >= MX_BUFS) ? blk_cnt : MX_BUFS;
		while (blk_cnt > 1) {
			bsize = Bufsize * blk_cnt;
			if(bsize < (2*CPIOBSZ))
				bsize = 2*CPIOBSZ;
			if ((Buffr.b_base_p = align(bsize)) != (char *)-1) {
				Buffr.b_out_p = Buffr.b_in_p = Buffr.b_base_p;
				Buffr.b_cnt = 0L;
				Buffr.b_size = bsize;
				Buffr.b_end_p = Buffr.b_base_p + Buffr.b_size;
				break;
			}
			blk_cnt--;
		}
		if (blk_cnt < 2)
			msg(EXT, nomemid, nomem);
		if(Compareflag) {
			cmpbuf = align(bsize);
			if(cmpbuf == (char *)-1)
				msg(EXT, nomemid, nomem);
		}
	}
	if (Args & OCp) { /* get destination directory */
		(void)strcpy(Fullnam_p, *largv);
		if (stat64(Fullnam_p, &DesSt) < 0)
			msg(EXTN, badaccessid, badaccess, Fullnam_p);
		if ((DesSt.st_mode & Ftype) != S_IFDIR)
			msg(EXT, ":130", "\"%s\" is not a directory", Fullnam_p);
	}
	Full_p = Fullnam_p + strlen(Fullnam_p) - 1;
	if (*Full_p != '/') {
		Full_p++;
		*Full_p = '/';
	}
	Full_p++; 
	*Full_p = '\0';
	(void)strcpy(Lnknam_p, Fullnam_p);
	Lnkend_p = Lnknam_p + strlen(Lnknam_p);
	getrlimit64(RLIMIT_FSIZE, &rl);
	Max_filesz = rl.rlim_cur;
	for (i = 0; i < NLNKHASH; i++) {
		Lnk_hd[i].L_nxt_p = Lnk_hd[i].L_bck_p = &Lnk_hd[i];
		Lnk_hd[i].L_lnk_p = (struct Lnk *)NULL;
	}
}

/*
 * set_tym: Set the access and/or modification times for a file.
 */

static void
set_tym(char *nam_p, time_t atime, time_t mtime)
{
	struct utimbuf timev;

	timev.actime = atime;
	timev.modtime = mtime;
	if (utime(nam_p, &timev) < 0) {
		if (Args & OCa)
			if (errno == ENOSYS)
				msg(WARN, _SGI_MMX_cpio_settym, "Cannot reset time on \"%s\" : Operation not supported", nam_p);
			else
				msg(WARN, ":131", "Cannot reset access time for \"%s\"", nam_p);
		else
			msg(WARN, ":132", "Cannot reset modification time for \"%s\"", nam_p);
	}
}

/*
 * sigint:  Catch interrupts.  If an interrupt occurs during the extraction
 * of a file from the archive with the -u option set, and the filename did
 * exist, remove the current file and restore the original file.  Then exit.
 */

static void
sigint(void)
{
	register char *nam_p;

	(void)signal(SIGINT, SIG_IGN); /* block further signals */
	if (!Finished) {
		if (Args & OCi)
			nam_p = G_p->g_nam_p;
		else /* OCp */
			nam_p = Fullnam_p;
		if (*Over_p != '\0') { /* There is a temp file */
			if (unlink(nam_p))
				msg(ERRN, badremincid, badreminc, nam_p);
			if (rename(Over_p, nam_p) < 0)
				msg(ERRN, badorigid, badorig, nam_p);
		}  else if (unlink(nam_p))
			msg(ERRN, badremincid, badreminc, nam_p);
	}
	exit(Error_cnt);
}

/*
 * swap: Swap bytes (-s), halfwords (-S) or or both halfwords and bytes (-b).
 */

static void
swap(char *buf_p, int cnt)
{
	register unsigned char tbyte;
	register int tcnt;
	register int rcnt;
	register ushort thalf;

        rcnt = cnt % 4;
	cnt /= 4;
	if (Args & (OCb | OCs | BSM)) {
		tcnt = cnt;
		Swp_p = (union swpbuf *)buf_p;
		while (tcnt-- > 0) {
			tbyte = Swp_p->s_byte[0];
			Swp_p->s_byte[0] = Swp_p->s_byte[1];
			Swp_p->s_byte[1] = tbyte;
			tbyte = Swp_p->s_byte[2];
			Swp_p->s_byte[2] = Swp_p->s_byte[3];
			Swp_p->s_byte[3] = tbyte;
			Swp_p++;
		}
                if (rcnt >= 2) {
                        tbyte = Swp_p->s_byte[0];
                        Swp_p->s_byte[0] = Swp_p->s_byte[1];
                        Swp_p->s_byte[1] = tbyte;
                        tbyte = Swp_p->s_byte[2];
		}
	}
	if (Args & (OCb | OCS)) {
		tcnt = cnt;
		Swp_p = (union swpbuf *)buf_p;
		while (tcnt-- > 0) {
			thalf = Swp_p->s_half[0];
			Swp_p->s_half[0] = Swp_p->s_half[1];
			Swp_p->s_half[1] = thalf;
			Swp_p++;
		}
	}
}

/*
 * usage: Print the usage message on stderr and exit.
 */

static void
usage(void)
{

	(void)fflush(stdout);
	(void)pfmt(stderr, MM_ACTION, ":133:Usage:\n");
        (void)pfmt(stderr, MM_NOSTD, ":134:\tcpio -i[bcdfkmrstuvBSVT6] [-C size]");
        (void)pfmt(stderr, MM_NOSTD, ":135:[-E file] [-H hdr] [[-I file] [-M msg]] ");
        (void)pfmt(stderr, MM_NOSTD, ":136:[-R id] [patterns]\n");
        (void)pfmt(stderr, MM_NOSTD, ":137:\tcpio -o[acvABKLVW] [-C size] ");
        (void)pfmt(stderr, MM_NOSTD, ":138:[-H hdr] [[-M msg] [-O file]]\n");
        (void)pfmt(stderr, MM_NOSTD, ":139:\tcpio -p[adlmuvLV] [-R id] directory\n");
	(void)fflush(stderr);
	exit(Error_cnt);
}

/*
 * verbose: For each file, print either the filename (-v) or a dot (-V).
 * If the -t option (table of contents) is set, print either the filename,
 * or if the -v option is also set, print an "ls -l"-like listing.
 */

static void
verbose(char *nam_p)
{
	register int i, j, temp;
	mode_t mode;
	char modestr[11];

	for (i = 0; i < 10; i++)
		modestr[i] = '-';
	modestr[i] = '\0';

	if ((Args & OCt) && (Args & OCv)) {
		mode = (mode_t)Gen.g_mode;
		for (i = 0; i < 3; i++) {
			temp = (int)(mode >> (6 - (i * 3)));
			j = (i * 3) + 1;
			if (S_IROTH & temp)
				modestr[j] = 'r';
			if (S_IWOTH & temp)
				modestr[j + 1] = 'w';
			if (S_IXOTH & temp)
				modestr[j + 2] = 'x';
		}
		temp = (int)Gen.g_mode & Ftype;
		switch (temp) {
		case (S_IFIFO):
			modestr[0] = 'p';
			break;
		case (S_IFCHR):
			modestr[0] = 'c';
			break;
		case (S_IFDIR):
			modestr[0] = 'd';
			break;
		case (S_IFBLK):
			modestr[0] = 'b';
			break;
		case (S_IFREG): /* was initialized to '-' */
			if(Gen.g_rdev==1)
				modestr[0] = 'H';	/* holey file */
			break;
		case (S_IFLNK):
			modestr[0] = 'l';
			break;
		default:
			msg(ERR, ":140", "Impossible file type");
		}
		if ((S_ISUID & Gen.g_mode) == S_ISUID)
			modestr[3] = 's';
		if ((S_ISVTX & Gen.g_mode) == S_ISVTX)
			modestr[9] = 't';
		if ((S_ISGID & G_p->g_mode) == S_ISGID && modestr[6] == 'x')
			modestr[6] = 's';
		else if ((S_ENFMT & Gen.g_mode) == S_ENFMT && modestr[6] != 'x')
			modestr[6] = 'l';
		if ((Hdr_type == USTAR || Hdr_type == TAR) && Gen.g_nlink == 0)
			(void)printf("%s%4d ", modestr, Gen.g_nlink+1);
		else
			(void)printf("%s%4d ", modestr, Gen.g_nlink);
		if (Lastuid == (int)Gen.g_uid)
			(void)printf("%-9s", Curpw_p->pw_name);
		else {
			setpwent();
			if (Curpw_p = getpwuid((int)Gen.g_uid)) {
				(void)printf("%-9s", Curpw_p->pw_name);
				Lastuid = (int)Gen.g_uid;
			} else {
				(void)printf("%-9d", Gen.g_uid);
				Lastuid = -1;
			}
		}
		if (Lastgid == (int)Gen.g_gid)
			(void)printf("%-9s", Curgr_p->gr_name);
		else {
			setgrent();
			if (Curgr_p = getgrgid((int)Gen.g_gid)) {
				(void)printf("%-9s", Curgr_p->gr_name);
				Lastgid = (int)Gen.g_gid;
			} else {
				(void)printf("%-9d", Gen.g_gid);
				Lastgid = -1;
			}
		}
		if (!Aspec || ((Gen.g_mode & Ftype) == S_IFIFO))
			(void)printf("%13lld ", Gen.g_filesz);
		else
			(void)printf("%3d,%3d ", major(Gen.g_rdev), minor(Gen.g_rdev));
		(void)cftime(Time, gettxt(FORMATID, FORMAT), (time_t *)&Gen.g_mtime);
		(void)printf("%s %s", Time, nam_p);
		if ((Gen.g_mode & Ftype) == S_IFLNK) {
			if (Hdr_type == USTAR || Hdr_type == TAR)
				(void)strcpy(Symlnk_p, Thdr_p->tbuf.t_linkname);
			else {
				if(!Compareflag)	/* else Buffr.b_out_p setup right */
					FILL(Gen.g_filesz);
				(void)strncpy(Symlnk_p, Buffr.b_out_p, Gen.g_filesz);
				*(Symlnk_p + Gen.g_filesz) = '\0';
			}
			(void)printf(" -> %s", Symlnk_p);
		}
		(void)printf("\n");
	} else if ((Args & OCt) || (Args & OCv)) {
		(void)fputs(nam_p, Out_p);
		(void)fputc('\n', Out_p);
	} else { /* OCV */
		(void)fputc('.', Out_p);
		if (Verbcnt++ >= 49) { /* start a new line of dots */
			Verbcnt = 0;
			(void)fputc('\n', Out_p);
		}
	}
	(void)fflush(Out_p);
}

/*
 * write_hdr: Transfer header information for the generic structure
 * into the format for the selected header and bwrite() the header.
 */

static void
write_hdr(void)
{
        static int uid_overflow_warning = 0;
	register int cnt, pad;

	switch (Hdr_type) {
	case BIN:
	case CHR:
	case ASC:
	case CRC:
		cnt = Hdrsz + (int)G_p->g_namesz;
		break;
	case TAR:
	/*FALLTHROUGH*/
	case USTAR: /* TAR and USTAR */
		cnt = TARSZ;
		break;
	default:
		msg(EXT, badhdrid, badhdr);
	}
	FLUSH(cnt);
	switch (Hdr_type) {
	case BIN:
	    	if ((G_p->g_uid > BIN_MAXUID || G_p->g_gid > BIN_MAXUID) &&
		    !uid_overflow_warning) {
		        uid_overflow_warning = BIN_MAXUID;
		}

		Hdr.h_magic = (short)G_p->g_magic;
		Hdr.h_dev = (short)G_p->g_dev;
		Hdr.h_ino = (ushort)G_p->g_ino;
		Hdr.h_mode = (ushort)G_p->g_mode;
		Hdr.h_uid = (ushort)(G_p->g_uid > BIN_MAXUID ? UID_NOBODY : G_p->g_uid);
		Hdr.h_gid = (ushort)(G_p->g_gid > BIN_MAXUID ? GID_NOBODY : G_p->g_gid);
		Hdr.h_nlink = (short)G_p->g_nlink;
		Hdr.h_rdev = (short)G_p->g_rdev;
		mkshort(Hdr.h_mtime, (long)G_p->g_mtime);
		Hdr.h_namesize = (short)G_p->g_namesz;
		mkshort(Hdr.h_filesize, (long)G_p->g_filesz);
		(void)strcpy(Hdr.h_name, G_p->g_nam_p);
		(void)memcpy(Buffr.b_in_p, &Hdr, cnt);
		break;
	case CHR:
	    	if ((G_p->g_uid > CHR_MAXUID || G_p->g_gid > CHR_MAXUID) &&
		    !uid_overflow_warning) {
		        uid_overflow_warning = CHR_MAXUID;
		}

		(void)sprintf(Buffr.b_in_p, "%.6lo%.6lo%.6llo%.6lo%.6lo%.6lo%.6lo%.6lo%.11lo%.6lo%.11llo%s",
			G_p->g_magic, G_p->g_dev, G_p->g_ino, G_p->g_mode, 
			(G_p->g_uid > CHR_MAXUID ? UID_NOBODY : G_p->g_uid),
			(G_p->g_gid > CHR_MAXUID ? GID_NOBODY : G_p->g_gid),
			G_p->g_nlink, G_p->g_rdev, G_p->g_mtime, G_p->g_namesz, G_p->g_filesz, G_p->g_nam_p);
		break;
	case ASC:
	case CRC:
		(void)sprintf(Buffr.b_in_p, "%.6lx%.8llx%.8lx%.8lx%.8lx%.8lx%.8lx%.8llx%.8lx%.8lx%.8lx%.8lx%.8lx%.8lx%s",
			G_p->g_magic, G_p->g_ino, G_p->g_mode, G_p->g_uid, G_p->g_gid, G_p->g_nlink, G_p->g_mtime,
			G_p->g_filesz, major(G_p->g_dev), minor(G_p->g_dev), major(G_p->g_rdev), minor(G_p->g_rdev),
			G_p->g_namesz, G_p->g_cksum, G_p->g_nam_p);
		break;
	case USTAR: /* USTAR */
		/* N.B. We don't print a warning when the uid/gid overflows.
		 * Since we're saving the uname/gname too, chances are that we'll 
		 * still be able to do the right thing at extraction time.
		 */
		Thdr_p = (union tblock *)Buffr.b_in_p;
		(void)memcpy(Thdr_p, Empty, TARSZ);
		(void)strncpy(Thdr_p->tbuf.t_name, G_p->g_tname, strlen(G_p->g_tname));
		(void)sprintf(Thdr_p->tbuf.t_mode, "%07o", (G_p->g_mode & 0xfffL));
		(void)sprintf(Thdr_p->tbuf.t_uid, "%07o", 
			(G_p->g_uid > USTAR_MAXUID ? UID_NOBODY : G_p->g_uid));
		(void)sprintf(Thdr_p->tbuf.t_gid, "%07o", 
			(G_p->g_gid > USTAR_MAXUID ? GID_NOBODY : G_p->g_gid));
#ifndef sgi
		(void)sprintf(Thdr_p->tbuf.t_size, "%011lo", G_p->g_filesz);
#endif
		(void)sprintf(Thdr_p->tbuf.t_mtime, "%011lo", G_p->g_mtime);
		Thdr_p->tbuf.t_typeflag = G_p->g_typeflag;
#ifdef sgi
		if (T_lname[0] != '\0') {
			if (strlen(T_lname) > NAMSIZ) {
				msg(EPOST, ":86",
					"%s: filename is greater than %d",
					T_lname, NAMSIZ);
				/*
				 * Ensure that we don't write a record.
				 */
				G_p->g_filesz = 0LL;
				return;
			}
			(void)sprintf(Thdr_p->tbuf.t_size, "%011o",
							0L);
			Thdr_p->tbuf.t_typeflag = S_ISLNK(G_p->g_mode) ? '2' : '1';
		} else {
			if (Thdr_p->tbuf.t_typeflag == '1')
				Thdr_p->tbuf.t_typeflag = '0';
			(void)sprintf(Thdr_p->tbuf.t_size, "%011llo",
							G_p->g_filesz);
			
		}
#endif
		if (T_lname[0] != '\0') {
			int n = strlen(T_lname)+1;	/* including \0 ! */
			if(n > sizeof(Thdr_p->tbuf.t_linkname))
				n = sizeof(Thdr_p->tbuf.t_linkname);
			(void)strncpy(Thdr_p->tbuf.t_linkname, T_lname, n);
		}
		(void)sprintf(Thdr_p->tbuf.t_magic, "%s", TMAGIC);
		(void)sprintf(Thdr_p->tbuf.t_version, "%2s", TVERSION);
		(void)sprintf(Thdr_p->tbuf.t_uname, "%s",  G_p->g_uname);
		(void)sprintf(Thdr_p->tbuf.t_gname, "%s", G_p->g_gname);
		if (Aspec) {
			(void)sprintf(Thdr_p->tbuf.t_devmajor, "%07o", major(G_p->g_rdev));
			(void)sprintf(Thdr_p->tbuf.t_devminor, "%07o", minor(G_p->g_rdev));
		} else {
			(void)sprintf(Thdr_p->tbuf.t_devmajor, "%07o", major(G_p->g_dev));
			(void)sprintf(Thdr_p->tbuf.t_devminor, "%07o", minor(G_p->g_dev));
		}
		if (Gen.g_prefix != (char *) NULL)
			(void)sprintf(Thdr_p->tbuf.t_prefix, "%s", Gen.g_prefix);
		(void)sprintf(Thdr_p->tbuf.t_cksum, "%07o", (int)cksum(TARTYP, 0));
		break;
	case TAR:
	    	if ((G_p->g_uid > TAR_MAXUID || G_p->g_gid > TAR_MAXUID) &&
		    !uid_overflow_warning) {
		        uid_overflow_warning = TAR_MAXUID;
		}

		Thdr_p = (union tblock *)Buffr.b_in_p;
		(void)memcpy(Thdr_p, Empty, TARSZ);
		(void)strncpy(Thdr_p->tbuf.t_name, G_p->g_nam_p, G_p->g_namesz);
		(void)sprintf(Thdr_p->tbuf.t_mode, "%07o ", G_p->g_mode);
		(void)sprintf(Thdr_p->tbuf.t_uid, "%07o ", 
			(G_p->g_uid > TAR_MAXUID ? UID_NOBODY : G_p->g_uid));
		(void)sprintf(Thdr_p->tbuf.t_gid, "%07o ", 
			(G_p->g_gid > TAR_MAXUID ? UID_NOBODY : G_p->g_gid));
		(void)sprintf(Thdr_p->tbuf.t_size, "%011llo ", G_p->g_filesz);
		(void)sprintf(Thdr_p->tbuf.t_mtime, "%011o ", G_p->g_mtime);
		if (T_lname[0] != '\0')
			Thdr_p->tbuf.t_typeflag = S_ISLNK(G_p->g_mode) ? '2' : '1';
		else
			Thdr_p->tbuf.t_typeflag = '\0';
		(void)strncpy(Thdr_p->tbuf.t_linkname, T_lname, strlen(T_lname));
		break;
	default:
		msg(EXT, ":48", "Impossible header type.");
	} /* Hdr_type */

	if (uid_overflow_warning > 0) {
		msg(WARN, _SGI_MMX_cpio_uid_overflow,
		    "uid or gid > %d, using \"nobody\" instead", uid_overflow_warning);
	        /* only print this warning once */
		uid_overflow_warning = -1;
	}

	Buffr.b_in_p += cnt;
	Buffr.b_cnt += cnt;
	pad = ((cnt + Pad_val) & ~Pad_val) - cnt;
	if (pad != 0) {
		FLUSH(pad);
		(void)memcpy(Buffr.b_in_p, Empty, pad);
		Buffr.b_in_p += pad;
		Buffr.b_cnt += pad;
	}
}

/*
 * write_trail: Create the appropriate trailer for the selected header type
 * and bwrite the trailer.  Pad the buffer with nulls out to the next Bufsize
 * boundary, and force a write.  If the write completes, or if the trailer is
 * completely written (but not all of the padding nulls (as can happen on end
 * of medium)) return.  Otherwise, the trailer was not completely written out,
 * so re-pad the buffer with nulls and try again.
 */

static void
write_trail(void)
{
	register int cnt, need;

	switch (Hdr_type) {
	case BIN:
	case CHR:
	case ASC:
	case CRC:
		Gen.g_mode = Gen.g_uid = Gen.g_gid = 0;
		Gen.g_nlink = 1;
		Gen.g_mtime = Gen.g_dev = 0;
		Gen.g_rdev = Gen.g_cksum = 0;
		Gen.g_filesz = Gen.g_ino = 0LL;
		Gen.g_namesz = strlen("TRAILER!!!") + 1;
		(void)strcpy(Gen.g_nam_p, "TRAILER!!!");
		G_p = &Gen;
		write_hdr();
		break;
	case TAR:
	/*FALLTHROUGH*/
	case USTAR: /* TAR and USTAR */
		for (cnt = 0; cnt < 3; cnt++) {
			FLUSH(TARSZ);
			(void)memcpy(Buffr.b_in_p, Empty, TARSZ);
			Buffr.b_in_p += TARSZ;
			Buffr.b_cnt += TARSZ;
		}
		break;
	default:
		msg(EXT, badhdrid, badhdr);
	}
	need = Bufsize - (Buffr.b_cnt % Bufsize);
	if(need == Bufsize)
		need = 0;
	
	while (Buffr.b_cnt > 0) {
		while (need > 0) {
			cnt = (need < TARSZ) ? need : TARSZ;
			need -= cnt;
			FLUSH(cnt);
			(void)memcpy(Buffr.b_in_p, Empty, cnt);
			Buffr.b_in_p += cnt;
			Buffr.b_cnt += cnt;
		}
		bflush();
	}
}

/* check to see if it's a drive, and is in audio mode; if it is, then
 * try to fix it, and also notify them.  Otherwise we can write in
 * audio mode, but they won't be able to get the data back
*/
static void
chkandfixaudio(int mt)
{
	static struct mtop mtop = {MTAUD, 0};
	struct mtget mtget;
	unsigned status;

	if(rmtioctl(mt, MTIOCGET, &mtget))
		return;
	status = mtget.mt_dsreg | ((unsigned)mtget.mt_erreg<<16);
	if(status & CT_AUDIO) {
		msg(EPOST, ":1016", "Warning: drive was in audio mode, turning audio mode off");
		if(rmtioctl(mt, MTIOCTOP, &mtop) < 0)
			msg(EPOST, ":1017", "Warning: unable to disable audio mode.  Tape may not be readable");
	}
}


/*
 *	determine if we there is a tape in the drive, called after an i/o error. 
 */
int
chknomedia(int fd)
{
    struct mtget mt_status;
	int saverr = errno;

    if(rmtioctl(fd, MTIOCGET, (char *)&mt_status) == 0 &&
		!(mt_status.mt_dposn & MT_ONL))
		return 1;
	errno = saverr;	/* for perror to follow */
    return 0;
}
