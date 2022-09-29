/*
 * Copyright (c) 1980,1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * There are a number of exit codes with the new 'savecore' binary:
 * 
 * 0 = success (core dump saved)
 * 1 = fail (could not save core dump for any reason)
 * 2 = partial fail (could not save core dump due to space reasons)
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980,1986 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)savecore.c	5.8 (Berkeley) 5/26/86";
#endif /* not lint */

/*
 * savecore
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <nlist.h>
#include <sys/param.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <sys/utsname.h>
#include <sys/dump.h>
#include <syslog.h>
#include <diskinfo.h>
#include <sys/capability.h>

#define	MAXLEN	80

#define eq(a,b,l) (!bcmp(a,b,l))
#if defined(vax) || defined(mips)
#define ok(number) ((number)&0x7fffffff)
#else
#define ok(number) (number)
#endif

#if (_MIPS_SZLONG == 64)
struct nlist64 current_nl[] = {	/* namelist for currently running system */
#define X_DUMPDEV	0
	{ "dumpdev" },
#define X_DUMPLO	1
	{ "dumplo" },
#define X_TIME		2
	{ "time" },
#define	X_DUMPSIZE	3
	{ "dumpsize" },
#define X_UTSNAME	4
	{ "utsname" },
#define X_PANICSTR	5
	{ "panicstr" },
#define	X_DUMPMAG	6
	{ "dumpmag" },
#define	X_KERNEL_MAGIC	7
	{ "kernel_magic" },
#define	X_END		8
	{ "end" },
#define X_PAGECONST	9
	{ "_pageconst" },
	{ "" },
};

struct nlist64 dump_nl[] = {	/* name list for dumped system */
	{ "dumpdev" },		/* entries MUST be the same as */
	{ "dumplo" },		/*	those in current_nl[]  */
	{ "time" },
	{ "dumpsize" },
	{ "utsname" },
	{ "panicstr" },
	{ "dumpmag" },
	{ "kernel_magic" },
	{ "end" },
	{ "_pageconst" },
	{ "" },
};

#else

struct nlist current_nl[] = {	/* namelist for currently running system */
#define X_DUMPDEV	0
	{ "dumpdev" },
#define X_DUMPLO	1
	{ "dumplo" },
#define X_TIME		2
	{ "time" },
#define	X_DUMPSIZE	3
	{ "dumpsize" },
#define X_UTSNAME	4
	{ "utsname" },
#define X_PANICSTR	5
	{ "panicstr" },
#define	X_DUMPMAG	6
	{ "dumpmag" },
#define	X_KERNEL_MAGIC	7
	{ "kernel_magic" },
#define	X_END		8
	{ "end" },
#define X_PAGECONST	9
	{ "_pageconst" },
	{ "" },
};

struct nlist dump_nl[] = {	/* name list for dumped system */
	{ "dumpdev" },		/* entries MUST be the same as */
	{ "dumplo" },		/*	those in current_nl[]  */
	{ "time" },
	{ "dumpsize" },
	{ "utsname" },
	{ "panicstr" },
	{ "dumpmag" },
	{ "kernel_magic" },
	{ "end" },
	{ "_pageconst" },
	{ "" },
};
#endif /* _MIPS_SZLONG */

char	*systemfile;
char	*dirname;			/* directory to save dumps in */
char	*dd_bname, *dd_rname;		/* name of dump block and raw device */
time_t	dumptime;			/* time the dump was taken */
int	dumpsize;			/* amount of memory dumped */
int	dumpdevsize;			/* size of the dump device */
int	dumplo;				/* where dump starts on dumpdev */

unsigned long long new_dumpmag;		/* magic number in new dump headers */
unsigned int old_dumpmag;		/* magic number in old dumps */
dump_hdr_t dump_hdr;			/* Dump header */

struct	utsname cur_uts;
struct	utsname	core_uts;
int	panicstr;
char	panic_mesg[MAXLEN];
int	Force = 0;
int	Verbose = 0;
int	Avail = 0;			/* activates availability monitor */

/*
 * We get the pagesize from the /unix that we're going to save
 */
static int pagesize;
extern	int errno;

/* Function prototypes */
ssize_t Write(int, char *, ssize_t);
int Create(char *, int);
off_t Lseek(int, long, int);
ssize_t Read(int, char *, ssize_t);
int Open(char *, int);
unsigned int read_number(char *);
char * path(char *);
char * stat_dev(char *, char *, dev_t, int);
char * find_dev(char *, char *, dev_t, int);
void save_core(int);
void log_putbuf(dump_hdr_t *, int);
void logheader(dump_hdr_t *, int);
extern void get_panicstr(char *, dump_hdr_t *d, int);
int read_kmem(void);
void get_crashtime(void);
int dump_exists(int);
void clear_dump(void);
void print_dumpsize_warning(int,int);

void logavail(dump_hdr_t *dh);
void expand_header(dump_hdr_t *dump_hdr, FILE *fp);

main(int argc, char **argv)
{
	char *cp;
	int slash_unix;	/* Set when /unix is the running system */

	if (geteuid() != 0) {
		fprintf(stderr,
			"savecore: Must be root to use this program\n");
		exit(1);
	}

	argc--, argv++;
	while (argc > 0 && argv[0][0] == '-') {
		for (cp = &argv[0][1]; *cp; cp++) switch (*cp) {

		case 'f':
			Force++;
			break;

		case 'v':
			Verbose++;
			break;
		case 'a':
			Avail++;
			break;
		default:
		usage:
			fprintf(stderr,
			    "usage: savecore [-f] [-v] dirname [ system ]\n");
			exit(1);
		}
		argc--, argv++;
	}
	if (argc != 1 && argc != 2)
		goto usage;
	dirname = argv[0];
	if (argc == 2)
		systemfile = argv[1];
	openlog("savecore", LOG_ODELAY, LOG_AUTH);
	if (access(dirname, W_OK) < 0) {
		int oerrno = errno;

		perror(dirname);
		errno = oerrno;
		syslog(LOG_ERR, "%s: %m", dirname);
		exit(1);
	}

	/*
	 * If /unix isn't the running system, we need to make our best
	 * guess.  Try the beginning of /dev/swap.  These things can be changed
	 * on the kernel command line, but guessing wrong here isn't fatal.
	 */
	if (!(slash_unix = read_kmem())) {
		if (Verbose)
			fprintf(stderr, "savecore: Trying default parameters.\n");
		pagesize = getpagesize();  /* This will "usually" be correct */
		dd_bname = "/dev/swap";
		dd_rname = "/dev/rswap";
		dumplo = 0;
 	} else {
		int ncpu = sysconf(_SC_NPROC_CONF);
		/*
		 * if we have access to the symbols and we are running on a
		 * large configuration, make sure that we have enough dump
		 * space available. 
		 * The only other time where this would be checked is if we take
		 * a hit and run out of dump space resulting in an incomplete
		 * system dump .....
		 */
		if (ncpu >= 64) {
			/*
			 * our recommendation is 256Mbyte per 32CPU (8MB CPU)
			 * 
			 */
			int dump_in_mb  = ((dumpdevsize*sysconf(_SC_PAGESIZE)) 
							/ (1024 * 1024));
			if (dump_in_mb < (ncpu * 8)) 
				print_dumpsize_warning(dump_in_mb,ncpu);
			
		}

	}
	
	if (!dump_exists(Force)) {
		if (Verbose)
			fprintf(stderr, "savecore: No dump exists.\n");
		exit(1);
	} else {
		clear_dump();
	}

	/* Grab the panic string from the dump header. */
	get_panicstr(panic_mesg, &dump_hdr, MAXLEN);

	if (strlen(panic_mesg))
		syslog(LOG_CRIT, "reboot after panic: %s", panic_mesg);
	else
		syslog(LOG_CRIT, "reboot");

	/* Copy the putbuf from the dump header to syslog. */
	log_putbuf(&dump_hdr, 1);

	/* Get the crash time and log it. */
	get_crashtime();

	/*
	 * if avail monitoring, dump header to availdir
	 */
	if (Avail)
	{
		logavail(&dump_hdr);
	}
		
	save_core(slash_unix);

	exit(0);
	/*NOTREACHED*/
}

int
dump_exists(int force)
{
	register int dumpfd;
	unsigned long long word;

	dumpfd = Open(dd_bname, O_RDONLY);

	Lseek(dumpfd, (off_t) dumplo, L_SET);
	new_dumpmag = DUMP_MAGIC;
	Read(dumpfd, (char *)&word, sizeof (word));

	close(dumpfd);

	if (Verbose && word != new_dumpmag) {
		printf("dumplo = %d (%d bytes)\n", dumplo/NBPSCTR, dumplo);
		printf("Magic number mismatch: 0x%llx != 0x%llx\n", word,
								new_dumpmag);
	}

	if ((word == new_dumpmag) || force) {
		dumpfd = Open(dd_bname, O_RDONLY);
		Lseek(dumpfd, (off_t) dumplo, L_SET);
		Read(dumpfd, (char *)&dump_hdr, sizeof(dump_hdr_t));
		close(dumpfd);
	}
	
	return ((word == new_dumpmag) || force);
}


void
clear_dump(void)
{
	register int dumpfd;
	long long zero = 0LL;
	cap_t ocap;
	cap_value_t cap_mac_write = CAP_MAC_WRITE;

	ocap = cap_acquire(1, &cap_mac_write);
	dumpfd = Open(dd_bname, O_WRONLY);
	cap_surrender(ocap);

	/* Clear dump magic number */
	Lseek(dumpfd, (off_t) dumplo, L_SET);
	Write(dumpfd, (char *)&zero, sizeof (long long));

	close(dumpfd);
}


char *
stat_dev(char *dir, char *name, dev_t dev, int type)
{
	struct stat statb;
	static char devname[MAXPATHLEN + 1];
	char *dp;

	strcpy(devname, dir);
	strcat(devname, "/");
	strcat(devname, name);

	if (stat(devname, &statb)) {
		if (Verbose)
			perror(devname);
		return (NULL);
	}
	if (((statb.st_mode&S_IFMT) == type) && (dev == statb.st_rdev)) {
		dp = malloc(strlen(devname)+1);
		strcpy(dp, devname);
		return (dp);
	}

	return (NULL);
}

char *
find_dev(register char *devdir, register char *devhint,
	 register dev_t dev, register int type)
{
	register DIR *dfd;
	struct dirent *dir;
	char *dp;

	/* check hint file (i.e. /dev/swap) first */
	if (devhint != NULL) {
		if ((dp = stat_dev(devdir, devhint, dev, type)) != NULL)
			return (dp);
	}

	dfd = opendir(devdir);
	if (dfd) {
		while ((dir = readdir(dfd))) {
			if ((dp = stat_dev(devdir, dir->d_name, dev, type)) != NULL) {
				closedir(dfd);
				return (dp);
			}
		}
		closedir(dfd);
	}
	return((char *)NULL);
}

int	cursyms[] =
    { X_DUMPDEV, X_DUMPLO, -1 };

/* true == /unix is the running system.  0 == /unix isn't the running system
 * Don't copy /unix if it's not what's running...  Odds are it isn't what
 * _was_ running either.  That's what the header's uname is for.
 */
int
read_kmem(void)
{
	char *dump_sys;
	struct pageconst pageconst;
	int kmem, i;
	long kernel_magic;
	dev_t	dumpdev;			/* dump device */
	char *tmpname;
	
	dump_sys = systemfile ? systemfile : "/unix";

#if (_MIPS_SZLONG == 64)
	nlist64("/unix", current_nl);
	nlist64(dump_sys, dump_nl);
#else
	nlist("/unix", current_nl);
	nlist(dump_sys, dump_nl);
#endif
	/*
	 * Check that /unix and /dev/kmem match. If they don't match,
	 * let main() setup the default values, since incorrectly
	 * lseeking into /dev/kmem and reading (e.g. for pagesize)
	 * is far worse than the default. 
	 */
	kmem = Open("/dev/kmem", O_RDONLY);
	Lseek(kmem, (long)current_nl[X_KERNEL_MAGIC].n_value, L_SET);
	Read(kmem, (char *)&kernel_magic, sizeof(kernel_magic));
	if (kernel_magic != current_nl[X_END].n_value) {
		fprintf(stderr, "savecore: /unix is not the running system\n");
		syslog(LOG_ERR, "/unix is not the running system\n");
		
		return 0;
	}

	/*
	 * Some names we need for the currently running system,
	 * others for the system that was running when the dump was made.
	 * The values obtained from the current system are used
	 * to look for things in /dev/kmem that cannot be found
	 * in the dump_sys namelist, but are presumed to be the same
	 * (since the disk partitions are probably the same!)
	 *
	 * Actually, with our new dump format, we don't want any symbols
	 * from the dump.
	 */
	for (i = 0; cursyms[i] != -1; i++)
		if (current_nl[cursyms[i]].n_value == 0) {
			fprintf(stderr, "savecore: /unix: %s not in namelist\n",
			    current_nl[cursyms[i]].n_name);
			syslog(LOG_ERR, "/unix: %s not in namelist",
			    current_nl[cursyms[i]].n_name);
			exit(1);
		}

	Lseek(kmem, (long)current_nl[X_DUMPDEV].n_value, L_SET);
	Read(kmem, (char *)&dumpdev, sizeof (dumpdev));
	Lseek(kmem, (long)current_nl[X_DUMPLO].n_value, L_SET);
	Read(kmem, (char *)&dumplo, sizeof (dumplo));
	Lseek(kmem, (long)current_nl[X_PAGECONST].n_value, L_SET);
	Read(kmem, (char *)&pageconst, sizeof (pageconst));
	pagesize = pageconst.p_pagesz;

	Lseek(kmem, (long)current_nl[X_DUMPSIZE].n_value, L_SET);
	Read(kmem, (char *)&dumpdevsize,sizeof (dumpdevsize));


	dumplo *= NBPSCTR;

	if ((dd_bname = find_dev("/dev", "swap", dumpdev, S_IFBLK)) == NULL)
	    if ((dd_bname = find_dev("/dev/dsk", NULL, dumpdev, S_IFBLK))
		== NULL) {
		fprintf(stderr,
		   "savecore: can't find block device corresponding to %d/%d\n",
		   major(dumpdev), minor(dumpdev));
		syslog(LOG_ERR,
			"can't find block device corresponding to %d/%d\n",
			major(dumpdev), minor(dumpdev));
		exit(1);
	    }
	if ((tmpname = findrawpath(dd_bname)) == NULL) {
		fprintf(stderr,
			"savecore: can't find raw device corresponding to %s\n",
			dd_bname);

		syslog(LOG_ERR,
			"can't find raw device corresponding to %d/%d\n",
			major(dumpdev), minor(dumpdev));
		exit(1);
	    }
	dd_rname = strdup(tmpname);
	if (systemfile)
		return (kernel_magic == current_nl[X_END].n_value);
	close(kmem);
	return (kernel_magic == current_nl[X_END].n_value);
}

void
get_crashtime(void)
{
	/*
	 * Get the panic time from either the variable in the image or
	 * The dump header if it's available.
	 */
	dumptime = dump_hdr.dmp_crash_time;

	/* If we get a bogus dump time, don't print ... 1970 */
	if (dumptime == 0) {
		if (Verbose)
			printf("Dump time not found.\n");
		syslog(LOG_ERR, "Dump time not found.\n");
	} else {
		printf("System went down at %s", ctime(&dumptime));
		syslog(LOG_CRIT, "System went down at %s", ctime(&dumptime));
	}
}

char *
path(char *file)
{
	char *cp = malloc(strlen(file) + strlen(dirname) + 2);

	(void) strcpy(cp, dirname);
	(void) strcat(cp, "/");
	(void) strcat(cp, file);
	return (cp);
}

long long
check_space(void)
{
	struct stat dsb;
	long long spacefree;
	struct statfs fs;

	if (stat(dirname, &dsb) < 0) {
		int oerrno = errno;

		perror(dirname);
		errno = oerrno;
		syslog(LOG_ERR, "%s: %m", dirname);
		exit(1);
	}
	statfs(dirname, &fs, sizeof(fs), 0);
 	spacefree = (long long)fs.f_bfree * fs.f_bsize;

 	return spacefree - read_number("minfree");
}

unsigned int
read_number(char *fn)
{
	char lin[80];
	register FILE *fp;

	fp = fopen(path(fn), "r");
	if (fp == NULL)
		return (0);

	if (fgets(lin, 80, fp) == NULL) {
		fclose(fp);
		return (0);
	}
	fclose(fp);
	return (atoi(lin));
}

#define	BUFPAGES	(256*1024/pagesize)		/* 1/4 Mb */

void
save_core(int slash_unix)
{
	register int amount;
	register char *cp;
	register int ifd, ofd, bounds;
	register FILE *fp;
	long long space_to_use;
	long long dump_bytes;
	ssize_t n;

	/* space_to_use == freespace - minfree. */
	space_to_use = check_space();

	if (Verbose)
		printf("savecore: %lld bytes available for dump.\n",
			space_to_use);

	/* Don't save anything if space_to_use <= 0 -- old savecore behavior */
	if (space_to_use <= 0) {
		printf("savecore: Dump omitted, not enough space on device\n");
		syslog(LOG_WARNING,
			"dump omitted, not enough space on device");
		exit(2);
	}

	cp = malloc(BUFPAGES*pagesize);
	if (cp == 0) {
		fprintf(stderr, "savecore: Can't allocate i/o buffer.\n");
		return;
	}

	bounds = read_number("bounds");


	/* Save the dump header in /var/adm/crash/crashlog.%d */

	logheader(&dump_hdr, bounds);

	if (slash_unix) {
		ifd = Open(systemfile?systemfile:"/unix", O_RDONLY);
		sprintf(cp, "unix.%d", bounds);
		ofd = Create(path(cp), 0600);
		while((n = Read(ifd, cp, BUFSIZ)) > 0)
			Write(ofd, cp, n);
		close(ifd);
		close(ofd);
	} else {
		fprintf(stderr, "savecore: Not saving /unix.\n");
	}

	/* Get the actual dump size from the header. */
	dumpsize = dump_hdr.dmp_pages;

	dump_bytes = (long long)pagesize * dumpsize;

	if (dump_bytes > space_to_use) {
		syslog(LOG_NOTICE, "Truncating dump.  (Original size %lld)\n",
			dump_bytes);
		if (Verbose)
			printf("Truncating dump.  (Original size %lld)\n",
				dump_bytes);
		dump_bytes = space_to_use;
		dumpsize = dump_bytes / pagesize;
	}

	ifd = Open(dd_rname, O_RDONLY);
	sprintf(cp, "vmcore.%d.comp", bounds);

	ofd = Create(path(cp), 0600);
	Lseek(ifd, (off_t)dumplo, L_SET);

	printf("savecore: Saving %lld bytes of image in %s/vmcore.%d.comp\n",
		dump_bytes, dirname, bounds);
	syslog(LOG_NOTICE, "saving %lld bytes of image in %s/vmcore.%d.comp\n",
		dump_bytes, dirname, bounds);
	while (dumpsize > 0) {
		amount = (dumpsize > BUFPAGES ? BUFPAGES : dumpsize) * pagesize;
		n = Read(ifd, cp, amount);
		if (n != amount) {
			printf("savecore: warning: vmcore may be incomplete\n");
			syslog(LOG_WARNING,
				"warning: vmcore may be incomplete\n");
			break;
		}
		if (Write(ofd, cp, n) != n) {
			printf("savecore: warning: vmcore incomplete\n");
			syslog(LOG_WARNING,
				"warning: vmcore incomplete\n");
			break;
		}
		dumpsize -= n/pagesize;
	}

	close(ifd);

	/* Restore magic number! */	
	Lseek(ofd, 0, L_SET);
	Write(ofd, (char *)&(new_dumpmag), sizeof(dump_hdr.dmp_magic));
	close(ofd);

	fp = fopen(path("bounds"), "w");
	fprintf(fp, "%d\n", bounds+1);
	fclose(fp);
	free(cp);
}


void
logavail(dump_hdr_t *dh)
{
	FILE *fp;
	char *logfile = "/var/adm/avail/availlog";

	if ((fp = fopen(logfile, "ab")) == NULL) {
		printf("savecore::logavail => can't open %s (OK)\n",
			logfile);
		perror(logfile);
		return;
	}
	fprintf(fp, "CRASH|Savecore/Begin Header\n");
	expand_header(dh, fp);
	fprintf(fp, "CRASH|Savecore/End Header\n");
	fprintf(fp, "CRASH|%ld|%s", dumptime, ctime(&dumptime));
		/* newline is included in output of ctime() */
	fclose(fp);
}


/* Create a /var/adm/crashlog.%d file that contains the dump header. */
void
logheader(dump_hdr_t *dh, int bounds)
{
	FILE *fp;
	char logfile[20];

	sprintf(logfile, "crashlog.%d", bounds);

	if ((fp = fopen(path(logfile), "w")) == NULL) {
		printf("savecore: Can't create %s\n", path(logfile));
		perror("savecore");
		return;
	}

	fprintf(fp, "savecore: Created log %s", ctime(&dumptime));

	expand_header(dh, fp);

	fclose(fp);
}

/*
 * Versions of std routines that exit on error.
 */
Open(char *name, int rw)
{
	int fd;

	fd = open(name, rw);
	if (fd < 0) {
		int oerrno = errno;

		perror(name);
		errno = oerrno;
		syslog(LOG_ERR, "%s: %m", name);
		exit(1);
	}
	return (fd);
}

ssize_t
Read(int fd, char *buff, ssize_t size)
{
	ssize_t ret;

	ret = read(fd, buff, size);
	if (ret < 0) {
		int oerrno = errno;

		perror("read");
		errno = oerrno;
		syslog(LOG_ERR, "read: %m");
		exit(1);
	}
	return (ret);
}

off_t
Lseek(int fd, long off, int flag)
{
	long ret;

	if ((flag == L_SET) && (off & 0x80000000))
		off &= ~0x80000000;
	ret = lseek(fd, off, flag);
	if (ret == -1) {
		int oerrno = errno;

		perror("lseek");
		errno = oerrno;
		syslog(LOG_ERR, "lseek: %m");
		exit(1);
	}
	return (ret);
}

int
Create(char *file, int mode)
{
	register int fd;

	fd = creat(file, mode);
	if (fd < 0) {
		int oerrno = errno;

		perror(file);
		errno = oerrno;
		syslog(LOG_ERR, "%s: %m", file);
		exit(1);
	}
	return (fd);
}

ssize_t
Write(int fd, char *buf, ssize_t size)
{

	ssize_t n;

	if ((n = write(fd, buf, size)) < 0) {
		int oerrno = errno;

		perror("write");
		errno = oerrno;
		syslog(LOG_ERR, "write: %m");
		exit(1);
	}
	return (n);
}

void
print_dumpsize_warning(int dumpsize_in_mb,int ncpu)
{
	printf(
	"\nWARNING : The size of the dump device (%d MB) might be too small\n",
		dumpsize_in_mb);
	printf(
	"for a system dump. The recommended size for this machine is %d MB\n\n",
		ncpu * 8);
}
