static char rcs_ident[] = "Based on compress.c 4.0 85/07/30";

/* 
 * Compress - data compression program
 *
 * Derived from bsd/compress/compress.c.
 * Actual compression/uncompression code pulled out
 * into separate files: comp.h, comp.c, uncomp.h and uncomp.c.
 * Intention is to support applications linking with this
 * code instead of having to invoke compress/uncompress
 * separate executables.
 */

#include <limits.h>
#define _BSD_SIGNALS
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <utime.h>
#include <sys/mman.h>

#include "comp.h"
#include "uncomp.h"
#include <errno.h>

COMP_DATA comp_data;
UNCOMP_DATA uncomp_data;
COMP_OPTIONS comp_opt;
UNCOMP_OPTIONS uncomp_opt;

int suser = 0;
#define MAXBITS 16	    /* Upper limit on code size, in bits. */
int maxbits = MAXBITS;			/* user settable max # bits/code */
int perfile_stat = 0;			/* per-file status */
int perm_stat = 0;			/* permanent status */

int debug = 0;

int nomagic = 0;	/* Use a 3-byte magic number header, unless old file */
int zcat_flg = 0;	/* Write output on stdout, suppress messages */
int precious = 1;	/* Don't unlink output file on interrupt */
int close_ofd = 0;	/* if (close_ofd) then should close(ofd) */
int quiet = 1;		/* don't tell me about compression */
int block_compress = 1;	/* enable restarting dict if full and less compression */

char cmd_label[20];

int force = 0;
char ofname [PATH_MAX];	/* output filename */
int verbose = 0;

#ifdef sgi
void (*bgnd_flag)();
#else
int (*bgnd_flag)();
#endif

int compressing = 1;	    /* set if compress, clear if uncompress */
int try_mmap;		    /* flag used to control mmap usage */

static void writeerr(void);
static void copystat(const char *, char *);
static void version(void);
int foreground(void);

/*
 * Make available (mmap or read) some bytes from
 * file descriptor fd, starting at its current offset.
 * Update *pbuf to point to that place, and *plen to its length.
 * It's ok to assume that the file fd has at most fsize bytes.
 * Increase the current seek offset of fd by *plen bytes.
 * Return -1 on any inability to do this (EOF, error, evil spells).
 * Return 0 on success.
 */

int mmap_read (
    int fd, off64_t fsize, const char *filename,
    unsigned char **pbuf, size_t *plen)
{
    static void *last_addr;
    static size_t last_len;
    off64_t offset;
    static unsigned char *readbuf;
    static unsigned char tinybuf[64];	/* in case both mmap and malloc fail */
    static int sizereadbuf;
    int r;

    if (last_addr != 0) {
	munmap (last_addr, last_len);
	last_addr = 0;
	last_len = 0;
    }

    if (try_mmap) {
	offset = lseek64 (fd, 0, SEEK_CUR);
    
	if (offset >= 0) {
	    if (offset == fsize)
		return -1;			/* EOF */
	
	    if (offset < fsize) {


#		define min(a,b) ((a>b) ? b : a)
		*plen = min (fsize - offset, 128*1024);
		*pbuf = (unsigned char *) mmap64 (
				    0, *plen, PROT_READ, MAP_SHARED, fd, offset);
	
		if ((int)*pbuf != -1) {
		    /* mmap succeeded -- remember it, update seek, and return */
		    last_addr = *pbuf;
		    last_len = *plen;
		    if (lseek64 (fd, *plen, SEEK_CUR) < 0)
			return -1;
		    return 0;
		}
	    }
	}
    }

    /* mmap failed or untried -- lets try it the old fashioned way */

    try_mmap = 0;

    if (readbuf == 0) {
	sizereadbuf = 16*1024;
	readbuf = calloc (sizereadbuf, 1);
    }
    if (readbuf == 0) {
	sizereadbuf = sizeof (tinybuf);
	readbuf = tinybuf;
    }

    r = read (fd, readbuf, sizereadbuf);

    if (r == 0)
	return -1;			/* EOF */

    if (r > 0) {
	/* good - that worked */
	*pbuf = readbuf;
	*plen = r;
	return 0;
    }

    /* can't buy bytes around this place */
    fprintf (stderr, "Read (%d, 0x%x, %d) failed: ", fd, readbuf, sizereadbuf);
    perror (filename);
    return -1;
}

static ssize_t output (void *param, void *buf, size_t len)
{
    int output_ofd = * ((int *)(param));
    int ret;

    if (uncomp_opt.printcodes)
	return len;

    if ((ret = write (output_ofd, buf, len)) < 0)
	writeerr();

    return ret;
}

void compress_filedesc (int ifd, int ofd, off64_t fsize, const char *filename)
{
    unsigned char *buf;
    size_t buflen;
    int ret1, ret2;

    comp_opt.output_param = ((void *) &ofd);
    comp_begin (&comp_data, &comp_opt);
    try_mmap = 1;

    while (mmap_read (ifd, fsize, filename, &buf, &buflen) == 0)
	comp_compress (&comp_data, buf, buflen);

    ret1 = comp_end (&comp_data);
    ret2 = comp_geterrno (&comp_data);

    switch (ret2) {
	case -1:
	    /* should have already told user */
	    break;
	case 0:
	    /* success */
	    break;
	default:
	    fprintf (stderr, "%s: unknown internal compressor error\n",
		    filename);
	    break;
    }

    if (ret1 == -2)
	perfile_stat = 2;
}

void uncompress_filedesc (int ifd, int ofd, off64_t fsize, const char *filename)
{
    unsigned char *buf;
    size_t buflen;
    int ret;

    uncomp_opt.output_param = ((void *) &ofd);
    uncomp_begin (&uncomp_data, &uncomp_opt);
    try_mmap = 1;

    while (mmap_read (ifd, fsize, filename, &buf, &buflen) == 0 &&
	    uncomp_uncompress (&uncomp_data, buf, buflen) == 0
    ) {
	continue;
    }

    uncomp_end (&uncomp_data);
    ret = uncomp_geterrno (&uncomp_data);

    switch (ret) {
	case -1:
	    /* should have already told user */
	    break;
	case 0:
	    /* success */
	    break;
	case 1:
	    fprintf (stderr,
		    "%s: compressed with > %d bits, can only handle %d bits\n",
		    filename, MAXBITS, MAXBITS);
	    break;
	case 2:
	    fprintf (stderr, "%s: corrupt data, unable to decompress\n",
		    filename);
	    break;
	case 3:
	    fprintf (stderr, "%s: not in compressed format\n",
		    filename);
	    break;
	default:
	    fprintf (stderr, "%s: unknown internal decompressor error %d\n",
		    filename, ret);
	    break;
    }
    if (ret) {
	unlink (ofname);
	exit (1);
    }
}

void printcodes (int ifd, int ofd, off64_t fsize, const char *filename)
{
    uncomp_opt.printcodes = 1;
    uncompress_filedesc (ifd, ofd, fsize, filename);
    uncomp_opt.printcodes = 0;
}


/* Check for overwrite of existing file */

overwrite_check (const char *ofname)
{
    struct stat64 statbuf;	/* stat into here, to get fsize */
    char response[2];

    if (stat64 (ofname, &statbuf) < 0)
	return 0;

    response[0] = 'n';
    fprintf (stderr, "%s already exists;", ofname);
    if (!isatty(0)) {
	fprintf (stderr, "cannot overwrite.\n");
	exit(1);
    }
    if (foreground()) {
	fprintf (stderr,
	    " do you wish to overwrite %s (y or n)? ",
	    ofname);
	fflush (stderr);
	read (2, response, 2);
	while (response[1] != '\n') {
	    if (read (2, response+1, 1) < 0) {
		perror("stderr");
		break;
	    }
	}
    }
    if (response[0] != 'y') {
	fprintf(stderr, "\tnot overwritten\n");
	return -1;
    }

    return 0;
}

static void
errmsg(char *cmd, char c)
{
    errno = EINVAL;

    _sgi_nl_error (
	SGINL_NOSYSERR, cmd_label,
	gettxt(_SGI_DMMX_illoption, "illegal option -- %c"),
	c);

    if (strcmp (cmd, "uncompress") == 0)
	_sgi_nl_usage(SGINL_USAGE, cmd_label, 
	    gettxt(_SGI_DMMX_uncompress_usage, "uncompress [-fvcV][file]"));
    else if (strcmp (cmd, "zcat") == 0)
	_sgi_nl_usage(SGINL_USAGE, cmd_label, 
	    gettxt(_SGI_DMMX_zcat_usage, "zcat [file]"));
    else
	_sgi_nl_usage(SGINL_USAGE, cmd_label, 
	    gettxt(_SGI_DMMX_compress_usage, "compress [-fvcVd] [-b bits] [file]"));

    exit(1);
}

/*****************************************************************
 * TAG( main )
 *
 * Usage: compress [-dfvc] [-b bits] [file ...]
 * Inputs:
 *	-d:	    If given, decompression is done instead.
 *
 *      -c:         Write output on stdout, don't remove original.
 *
 *      -b:         Parameter limits the max number of bits/code.
 *
 *	-f:	    Forces output file to be generated, even if one already
 *		    exists, and even if no space is saved by compressing.
 *		    If -f is not used, the user will be prompted if stdin is
 *		    a tty, otherwise, the output file will not be overwritten.
 *
 *      -v:	    Write compression statistics
 *
 * 	file ...:   Files to be compressed.  If none specified, stdin
 *		    is used.
 * Outputs:
 *	file.Z:	    Compressed form of file with same mode, owner, and utimes
 * 	or stdout   (if stdin used as input)
 *
 * Assumptions:
 *	When filenames are given, replaces with the compressed version
 *	(.Z suffix) only if the file decreases in size.
 * Algorithm:
 * 	See the comp.c and uncomp.c files.
 */

main( argc, argv )
register int argc; char **argv;
{
    int overwrite = 0;	/* Do not overwrite unless given -f flag */
    char *simple_command_name;
    extern void onintr();
    int cmd_is_compress=0; 
    int c;

    (void)setlocale(LC_ALL, "");
    (void)setcat("uxsgicore");

    sprintf(cmd_label, "UX:%s", argv[0]);
    (void)setlabel(cmd_label);

    if (geteuid() == 0)
	suser++;
    if ( (bgnd_flag = signal ( SIGINT, SIG_IGN )) != SIG_IGN ) {
	signal ( SIGINT, onintr );
    }

    if((simple_command_name = strrchr (argv[0], '/')) != 0) {
	simple_command_name++;
    } else {
	simple_command_name = argv[0];
    }
    if (strcmp (simple_command_name, "uncompress") == 0) {
	compressing = 0;
    } else if (strcmp (simple_command_name, "zcat") == 0) {
	compressing = 0;
	zcat_flg = 1;
    } else {
	cmd_is_compress = 1;
    }

    /* Argument Processing
     * All flags are optional.
     * -D => debug
     * -V => print Version; debug verbose
     * -d => compressing == 0
     * -v => unquiet
     * -f => force overwrite of output file
     * -n => no header: useful to uncompress old files
     * -b maxbits => maxbits.  If -b is specified, then maxbits MUST be
     *	    given also.
     * -c => cat all output to stdout
     * -C => generate output compatible with compress 2.0.
     * if a string is left, must be an input filename.
     */
#ifdef DEBUG
    while ((c = getopt (argc, argv, "DVdvfnb:cC")) != EOF)
      switch(c){
	case 'D':
	   debug = 1;
	   break;
#else
    while ((c = getopt (argc, argv, "Vdvfnb:cC")) != EOF)
      switch(c){
#endif /* DEBUG */
	case 'V':
	   verbose = 1;
	   version();
	   break;
	case 'v':
	   /* if (cmd_is_zcat == 0)
	     errmsg(argv[0], c);    Why not allow zcat -v ?? pj. */
 	   quiet = 0;
	   break;
	case 'd':
	   if (cmd_is_compress == 0)
	     errmsg(argv[0], c);
	   compressing = 0;
	   break;
	case 'f':
	case 'F':
	   /* if (cmd_is_zcat == 0)
	     errmsg(argv[0], c);    Why not allow zcat -f ?? pj. */
	   overwrite = 1;
	   force = 1;
	   break;
	case 'n':
	   nomagic = 1;
	   break;
	case 'C':
	   block_compress = 0;
	   break;
	case 'b':
	  if (cmd_is_compress == 0) 
	    errmsg(argv[0], c);
	  maxbits = atoi(optarg);
	  break;
	case 'c':
	  /* if (cmd_is_zcat == 0)
	    errmsg(argv[0], c);    Why not allow zcat -c ?? pj. */
	  zcat_flg = 1;
	  break;
	case 'q':
	  quiet = 1;
	  break;
	case '?':
	  errmsg(argv[0], '\0');
      } /* end of option processing */

    comp_options_default (&comp_opt);
    comp_init (&comp_data, malloc, free, output);
    if (!block_compress) comp_opt.block_compress = 0;
    comp_opt.nomagic = nomagic;
    comp_opt.maxbits = maxbits;
    comp_opt.quiet = quiet;
    comp_opt.verbose = verbose;
    comp_opt.debug = debug;

    uncomp_options_default (&uncomp_opt);
    uncomp_init (&uncomp_data, malloc, free, output);
    uncomp_opt.quiet = quiet;
    uncomp_opt.verbose = verbose;
    uncomp_opt.debug = debug;
    if (nomagic == 0)
	uncomp_opt.uncomp_magic_disposition = required;

    if (optind == argc)
	argv[argc++] = "-";

    for ( ; optind < argc; optind++) {
	const char *ifname;	    /* input filename */
	int ifd = -1, ofd = -1;	    /* input, output file descriptors */
	struct stat64 statbuf;	    /* stat into here, to get fsize */
	off64_t fsize;		    /* for mmap, if can stat, lseek and mmap */
	char tempname[PATH_MAX];    /* build copy of ifname with .Z here */
	const char *diag_ifname;    /* input filename to show user */

	ifname = argv[optind];
	perfile_stat = 0;
	precious = 1;
	diag_ifname = ifname;

	/* setup input file */
	if (strcmp (ifname, "-") == 0) {
	    ifd = 0;
	    fsize = comp_opt.maxfilesize = -1;
	    diag_ifname = "stdin";
	} else {
	    if (compressing) {
		if (strcmp (ifname + strlen(ifname) - 2, ".Z") == 0) {
		    fprintf (stderr, "%s: already has .Z suffix -- no change\n",
			ifname);
		    perm_stat = 1;
		    goto done1;
		}
	    } else {
		/* Check for .Z suffix */
		if (strcmp(ifname + strlen(ifname) - 2, ".Z") != 0) {
		    /* No .Z: tack one on */
		    strcpy (tempname, ifname);
		    strcat (tempname, ".Z");
		    ifname = tempname;
		}
	    }

	    /* Open input file */
	    if ((ifd = open (ifname, O_RDONLY)) < 0) {
		perror (ifname);
		perm_stat = 1;
		goto done1;
	    }

	    if (fstat64 (ifd, &statbuf) == 0) {
		fsize = statbuf.st_size; 
		comp_opt.maxfilesize = statbuf.st_size;
	    }
	    else
		fsize = comp_opt.maxfilesize = -1;
	}
    
	/* setup output file */
	if (zcat_flg || strcmp (ifname, "-") == 0) {
	    ofd = 1;
	    close_ofd = 0;
	} else {
	    /* Generate output ifname */
	    strcpy (ofname, ifname);
	    if (compressing) {
		strcat (ofname, ".Z");
	    } else {
		ofname[strlen(ifname) - 2] = '\0';  /* Strip off .Z */
	    }
    
	    if (overwrite == 0 && overwrite_check (ofname) < 0)
		goto done2;
    
	    /* Open output file */
	    if ((ofd = open (ofname, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0) {
		perror (ofname);
		perm_stat = 1;
		goto done2;
	    }
	    precious = 0;
	    close_ofd = 1;
	    if (!quiet)
		fprintf (stderr, "%s: ", ifname);
	}

	if (compressing) {		    /* Actually do the compression */
	    compress_filedesc (ifd, ofd, fsize, diag_ifname);
#ifdef DEBUG
	    if (verbose)
		comp_dump_tab (&comp_data);
#endif /* DEBUG */
    
	} else {			    /* Actually do the decompression */
#ifndef DEBUG
	    uncompress_filedesc (ifd, ofd, fsize, diag_ifname);
#else
	    if (debug == 0) {
		uncompress_filedesc (ifd, ofd, fsize, diag_ifname);
	    } else {
		printcodes (ifd, ofd, fsize, diag_ifname);
	    }
	    if (verbose)
		uncomp_dump_tab (&uncomp_data);
#endif /* DEBUG */
	}
    
	if (precious == 0)
	    copystat (ifname, ofname);
    
        if (perfile_stat == 1 || !quiet)
	    putc ('\n', stderr);
    
	if (close_ofd)
	    close (ofd);
    done2:
	close (ifd);
    done1:
	continue;
    }					    /* end for-loop over files */

    exit(perm_stat ? perm_stat : perfile_stat);
    /* NOTREACHED */
}


static void
writeerr(void)
{
    perror ( ofname );
    unlink ( ofname );
    exit ( 1 );
}

static void
copystat(const char *ifname, char *ofname)
{
    struct stat64 statbuf;
    mode_t mode;
    struct utimbuf times;

    if (stat64(ifname, &statbuf)) {		/* Get stat on input file */
	perror(ifname);
	return;
    }
    if ((statbuf.st_mode & S_IFMT/*0170000*/) != S_IFREG/*0100000*/) {
	if(quiet)
	    	fprintf(stderr, "%s: ", ifname);
	fprintf(stderr, " -- not a regular file: unchanged");
	perfile_stat = 1;
	perm_stat = 1;
    } else if (statbuf.st_nlink > 1) {
	if(quiet)
	    	fprintf(stderr, "%s: ", ifname);
	fprintf(stderr, " -- has %d other links: unchanged",
		statbuf.st_nlink - 1);
	perfile_stat = 1;
	perm_stat = 1;
    } else if (perfile_stat == 2 && (!force)) { /* No compression: remove file.Z */
	if(!quiet)
		fprintf(stderr, " -- file unchanged");
    } else {			/* ***** Successful Compression ***** */
	perfile_stat = 0;
	mode = statbuf.st_mode & 07777;
	if (chmod(ofname, mode))		/* Copy modes */
	    perror(ofname);
	if (suser)
	    chown(ofname, statbuf.st_uid, statbuf.st_gid); /* Copy ownership */
	times.actime = statbuf.st_atime;
	times.modtime = statbuf.st_mtime;
	utime (ofname, &times);	/* Update last accessed and modified times */
	precious = 1;
	if (unlink(ifname))	/* Remove input file */
	    perror(ifname);
	if(!quiet)
		fprintf(stderr, " -- replaced with %s", ofname);
	return;		/* Successful return */
    }

    /* Unsuccessful return -- one of the tests failed */
    if (unlink(ofname))
	perror(ofname);
}
/*
 * This routine returns 1 if we are running in the foreground and stderr
 * is a tty.
 */
int foreground(void)
{
	if(bgnd_flag) {	/* background? */
		return(0);
	} else {			/* foreground */
		if(isatty(2)) {		/* and stderr is a tty */
			return(1);
		} else {
			return(0);
		}
	}
}

#ifdef sgi
void
#endif
onintr ( )
{
    if (!precious)
	unlink ( ofname );
    exit ( 1 );
}


static void
version(void)
{
	fprintf(stderr, "%s\n", rcs_ident);
	fprintf(stderr, "Options: ");
#ifdef vax
	fprintf(stderr, "vax, ");
#endif
#ifdef NO_UCHAR
	fprintf(stderr, "NO_UCHAR, ");
#endif
#ifdef SIGNED_COMPARE_SLOW
	fprintf(stderr, "SIGNED_COMPARE_SLOW, ");
#endif
#ifdef XENIX_16
	fprintf(stderr, "XENIX_16, ");
#endif
#ifdef COMPATIBLE
	fprintf(stderr, "COMPATIBLE, ");
#endif
#ifdef DEBUG
	fprintf(stderr, "DEBUG, ");
#endif
#ifdef BSD4_2
	fprintf(stderr, "BSD4_2, ");
#endif
	fprintf(stderr, "BITS = %d\n", MAXBITS);
}
