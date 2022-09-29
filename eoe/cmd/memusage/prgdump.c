/*
 * This program does a symbolic dump of a processes regions using /proc
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ustat.h>
#include <ftw.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <mntent.h>
#include <dirent.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/procfs.h>
#include <paths.h>
#include <libw.h>
#include <locale.h>
#include <fmtmsg.h>
#include <stdarg.h>
#include <sgi_nl.h>

/* Path to /proc filesystem */
#define _PATH_DBGDIR "/proc"
char prpathname[BUFSIZ];

/* process information structure from /proc */
struct prpsinfo info;

/* macro to print and clear a set of flags */
#define FLAGSPRF(maskt, maskc, name) { \
	if (flags & (maskt)) { \
		printf(name); \
		flags &= ~(maskc); \
		if (flags) printf("/"); \
	} \
}

/*
 * Table to correlate inode numbers to path names
 */
struct inrec {
	struct inrec *next;
	long inode;
	dev_t rdev;
	char name[128];
};

#define _PATH_INODES "/var/tmp/memusage.inodes"
static struct inrec *inodes;

static void
praddinode(long inode, dev_t rdev, char *str)
{
	struct inrec *new;

	new = (struct inrec *)malloc(sizeof (struct inrec));
	new->next = inodes;
	new->inode = inode;
	new->rdev = rdev;
	strcpy(new->name, str);
	inodes = new;
}

static
prfindinode(char *str, dev_t *rdev, ino_t *ino)
{
	struct inrec *current;

	for (current = inodes; current; current = current->next)
		if (strcmp(current->name, str) == NULL) {
			if (rdev) *rdev = current->rdev;
			if (ino) *ino = current->inode;
			return 1;
		}
	return 0;
}

/*
 * Given a vnode at address vloc, find a corresponding path name
 */
static char *
prdevino(int rdev, int inode)
{
	static int first_call = 1;
	struct inrec *current;
	char str[BUFSIZ];

	if (first_call) {
		FILE *fp;

		first_call = 0;
		if ((fp = fopen(_PATH_INODES, "r")) != NULL) {
			while (fscanf(fp,"%d %d %s\n",&rdev,&inode,str) == 3)
				praddinode(inode, rdev, str);
			fclose(fp);
		}
	}

	for (current = inodes; current; current = current->next)
		if (current->inode == inode && current->rdev == rdev)
			return current->name;

	sprintf(str, "Dev # = %x, Inode # = %d", rdev, inode);

	return str;
}

/*
 * Add dynamic paths into dev/ino map list
 */
static
prdynpaths()
{
	static char *paths[] = { "/var/tmp/.Xshmtrans0",
				 "/tmp/.cadminOSSharedArena",
				 NULL };
	register int i;
	struct stat statd;

	/* Add elements to the list */
	for (i = 0; paths[i] != NULL; ++i) {
		if (stat(paths[i], &statd) < 0)
			continue;
		praddinode(statd.st_ino, statd.st_dev, paths[i]);
	}
}

/*
 * Indentify PROGRAM text extent
 */
isprgtxt(struct prmap_sgi *map)
{
	if ((map->pr_mflags & (MA_EXEC|MA_PRIMARY)) == (MA_PRIMARY|MA_EXEC))
		return 1;
	return 0;
}

/*
 * Indentify RLD text extent
 */
isrldtxt(struct prmap_sgi *map)
{
	if ((unsigned)map->pr_vaddr == 0x0fb60000)
		return 1;
	return 0;
}

/*
 * Indentify RLD bss extents
 */
isrldbss(struct prmap_sgi *map)
{
	if (((unsigned)map->pr_vaddr >= 0x0fbe0000) &&
	    ((unsigned)map->pr_vaddr < 0x0fc40000))
		return 1;
	return 0;
}

/*
 * Indentify RLD bss extents
 */
isdevzero(struct prmap_sgi *map)
{
	static dev_t zdev;
	static ino_t zino;

	/* Lookup /dev/zero */
	if (!zdev) {
		prfindinode("/dev/zero", &zdev, &zino);
	}

	/* Match? */
	if (map->pr_dev == zdev && map->pr_ino == zino)
		return 1;
	return 0;
}

static void
prdumpinfo(int pid)
{
	register int i, flags, fd;
	register struct prmap_sgi *map, *rmap, *amap;
	static struct prmap_sgi maps[256];
	prmap_sgi_arg_t maparg;
	pgno_t vsize = 0;
	unsigned refcnt, nmaps;
	double wrss, awrss, rss;
	dev_t pdev;
	ino_t pino;

	/* Create process path name */
	sprintf(prpathname, "%s/%05d", _PATH_DBGDIR, pid);

	/* Open process */
	if ((fd = open(prpathname, 0)) < 0) {
		fprintf(stderr, "%s - ", prpathname);
		perror("open");
		exit(-2);
	}

	/* Get process info structure */
	if (ioctl(fd, PIOCPSINFO, &info) < 0) {
		perror("ioctl(PIOCPSINFO)");
		exit(-3);
	}

	/* Get process map structures */
	maparg.pr_vaddr = (caddr_t)maps;
	maparg.pr_size = sizeof maps;
	if ((nmaps = ioctl(fd, PIOCMAP_SGI, &maparg)) < 0) {
		perror("ioctl(PIOCMAP_SGI)");
		exit(-4);
	}

	/* Search regions to find app segment & rld segment */
	rmap = amap = NULL;
	for (map = maps, i = nmaps; i-- > 0; ++map) {
		if (map->pr_ino) {
			/* Hack for app region */
			if (!amap && isprgtxt(map)) {
				amap = map;
			}

			/* Use BRK as app region marker */
			if (map->pr_mflags & MA_BREAK) {
				amap = map;
			}

			/* Remember RLD region */
			if (!rmap && isrldtxt(map)) {
				rmap = map;
			}
		}
	}

	/* Compute weighted rss from wsize and region reference count */
	for (wrss = awrss = 0.0, map = maps, i = nmaps; i-- > 0; ++map) {
		/* Skip physical regions */
		if (map->pr_mflags & MA_PHYS)
			continue;

		/* Entire app */
		rss = (double)map->pr_wsize / MA_WSIZE_FRAC;
		rss /= map->pr_mflags >> MA_REFCNT_SHIFT;
		wrss += rss;

		/* Only local app objects */
		if (amap && (((amap->pr_dev == map->pr_dev) &&
			      (amap->pr_ino == map->pr_ino)) ||
			     (map->pr_mflags & MA_STACK))) {
			vsize += map->pr_size;
			awrss += rss;
		}
	}

	/* Dump table */
	printf("Process: %s, pid %d\n", info.pr_fname, pid);
	printf("\tAddress space segments: %d\n", nmaps);
	printf("Total virtual size: %d, rss size: %d (psinfo's rss %d)\n",
		info.pr_size, (int)(wrss + 0.5), info.pr_rssize);
	printf("\tApplication virtual size: %d, rss size: %d\n",
		vsize / getpagesize(), (int)(awrss + 0.5));
	printf("Segment table -\n");
	printf("    VADDR       SIZE        OFFSET      FLAGS\n");
	for (map = maps; nmaps-- > 0; ++map) {
		flags = map->pr_mflags;
		if (flags & (MA_PRIMARY|MA_STACK)) {
			printf("  %s", info.pr_fname);
		} else if (map->pr_ino) {
			/* start with maps dev & ino values */
			pdev = map->pr_dev;
			pino = map->pr_ino;

			/* try to charge rld for it's /dev/zero segments */
			if (isdevzero(map) && isrldbss(map)) {
				flags |= MA_BREAK;
				pdev = rmap->pr_dev, pino = rmap->pr_ino;
			}

			/* try to symbolic print dev & ino information */
			printf("  %s", prdevino(pdev, pino));
		} else {
			printf("  anonymous");
		}
		refcnt = flags >> MA_REFCNT_SHIFT;
		if (refcnt > 1)
			printf(" (region refcnt=%d)", refcnt);
		printf("\n");

		printf("    ");
		printf("0x%08X  ", map->pr_vaddr);
		printf("0x%08X  ", map->pr_size);
		printf("0x%08X  ", map->pr_off);
		printf("<");
		flags &= (1 << MA_REFCNT_SHIFT) - 1;
		FLAGSPRF(MA_PRIMARY, MA_PRIMARY, "PRIMARY");
		FLAGSPRF(MA_SREGION, MA_SREGION, "SREGION");
		FLAGSPRF(MA_COW, MA_COW, "COW");
		FLAGSPRF(MA_READ, MA_READ, "READ");
		FLAGSPRF(MA_WRITE, MA_WRITE, "WRITE");
		FLAGSPRF(MA_EXEC, MA_EXEC, "EXEC");
		FLAGSPRF(MA_SHMEM, MA_SHMEM|MA_SHARED, "SHMEM");
		FLAGSPRF(MA_SHARED, MA_SHARED, "SHARED");
		FLAGSPRF(MA_BREAK, MA_BREAK, "BREAK");
		FLAGSPRF(MA_STACK, MA_STACK, "STACK");
		FLAGSPRF(MA_PHYS, MA_PHYS, "PHYS");
		FLAGSPRF(MA_NOTCACHED, MA_NOTCACHED, "NOTCACHED");
		printf(">\n");
		printf("      ");
		printf("Valid = %d, ", map->pr_vsize);
		printf("Private = %d, ", map->pr_psize);
		if (!(map->pr_mflags & MA_PHYS))
			printf("Weighted-RSS = %d, ",
				map->pr_wsize / MA_WSIZE_FRAC);
		if (map->pr_mflags & MA_WRITE)
			printf("Modified = %d", map->pr_msize);
		else
			printf("Referenced = %d", map->pr_rsize);
		printf("\n");
	}
}

main(int argc, char **argv)
{
	register int i, flags, fd;
	register DIR *dirp;
	struct dirent *direntp;
	char prpathname[32];

	/* Usage */
	if (argc < 2) {
		fprintf(stderr, "Usage: prgdump process-id|name\n");
		exit(-1);
	}
	argv++;

	/* Setup symbol table */
	(void) prdevino(0, 0);
	(void) prdynpaths();

	/* Dump process info [id or name] */
	if (isdigit(**argv)) {
		prdumpinfo(atoi(*argv));
	} else {
		/* Open /proc directory */
		if ((dirp = opendir("/proc")) == NULL) {
			perror("opendir");
			exit(-1);
		}

		/* Scan the entire directory */
		while ((direntp = readdir(dirp)) != NULL) {
			/* Create process path name */
			sprintf(prpathname, "/proc/%s", direntp->d_name);

			/* Skip . and .. */
			if (direntp->d_name[0] == '.')
				continue;
	
			/* Skip junk in /proc directory */
			if (!isdigit(direntp->d_name[0]))
				continue;

			/* Open process */
			if ((fd = open(prpathname, 0)) < 0) {
				continue;
			}

			/* Get process info structure */
			if (ioctl(fd, PIOCPSINFO, &info) < 0) {
				perror("ioctl(PIOCPSINFO)");
				exit(-3);
			}
			close(fd);

			/* Dump proc(s) with matching name */
			if (strcmp(info.pr_fname, *argv) != 0) {
				continue;
			}
			prdumpinfo(atoi(direntp->d_name));
		}
		closedir(dirp);
	}

	exit(0);
}
