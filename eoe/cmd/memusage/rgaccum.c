/*
 * This program does a symbolic dump of the number of pages consumed by each
 * mapped region in the system across all processes.  For instance, if libc.so
 * is mapped into seven processes, rgaccum will add all the in core counts
 * from those processes that pertain to libc.so and display the totals.
 *
 * usage: rgaccum [-Dbcdfstz] [process-id|name]
 *		-D	Don't collect any Data/Stack/Break/Etc region counts
 *		-a	Don't collect Anonymous region counts
 *		-b	Don't collect Break region counts
 *		-c	Don't collect Cow region counts
 *		-d	Don't collect Data region counts
 *		-f	Don't collect File region counts
 *		-s	Don't collect Stack region counts
 *		-t	Don't collect Text region counts
 *		-z	Don't report regions with total of zero
 *		id	Optional process ID (in decimal)
 *		name	Optional process base name (as in Xsgi, etc...)
 */

#include <stdio.h>
#include <stddef.h>
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

/*
 * Path to /proc filesystem
 */
char prpathname[BUFSIZ];

/*
 * process information structure from /proc
 */
struct prpsinfo info;

/*
 * process region map information from /proc
 */
#define MAXMAPS         256
struct prmap_sgi maps[MAXMAPS];

/*
 * Table to correlate region data
 */
static struct sregion {
	dev_t dev;
	ino_t ino;
	u_long vsize, psize, rsize, msize, rss;
	int use;
	struct sregion *next;
} *sregion, *fregion;
static int nsrel;
#define FIXUP(val) ((val) = ((val) + 1) / MA_WSIZE_FRAC)

static void
add_region(struct prmap_sgi *map)
{
	register u_long rss, refcnt, multi;
	struct sregion *new;

	/* Compute weighted rss from wsize and region reference count */
	refcnt = map->pr_mflags >> MA_REFCNT_SHIFT;
	multi = MA_WSIZE_FRAC / refcnt;

	/* Search existing list */
	for (new = sregion; new != NULL; new = new->next) {
		if (new->dev == map->pr_dev && new->ino == map->pr_ino) {
			new->vsize += map->pr_vsize * multi;
			new->psize += map->pr_psize * multi;
			new->rsize += map->pr_rsize * multi;
			new->msize += map->pr_msize * multi;
			new->rss += map->pr_wsize / refcnt;
			new->use++;
			return;
		}
	}

	/* Nothing found, add new entry to sregion list */
	new = (struct sregion *)malloc(sizeof (struct sregion));
	new->next = sregion;
	new->dev = map->pr_dev;
	new->ino = map->pr_ino;
	new->vsize = map->pr_vsize * multi;
	new->psize = map->pr_psize * multi;
	new->rsize = map->pr_rsize * multi;
	new->msize = map->pr_msize * multi;
	new->rss = map->pr_wsize / refcnt;
	new->use = 1;
	sregion = new;
	++nsrel;
}

compare(const void *a, const void *b)
{
	register struct sregion *s = (struct sregion *)a;
	register struct sregion *t = (struct sregion *)b;

	if (s->rss < t->rss)
		return 1;
	else
		return -1;
}

static void
sort_regions()
{
	register struct sregion *new, *tbl;
	register int i;

	/* Create new table for sorting */
	fregion = (struct sregion *)malloc(nsrel * sizeof (struct sregion));

	/* Copy existing list */
	for (tbl = fregion, new = sregion; new != NULL; new = new->next) {
		*tbl = *new;
		tbl++;
	}

	/* Sort the table */
	qsort(fregion, nsrel, sizeof (struct sregion), compare);

	/* Fix table next pointers */
	for (tbl = fregion, i = nsrel; --i > 0; tbl++) {
		tbl->next = &tbl[1];
	}
	tbl->next = NULL;

	/* install new table */
	sregion = fregion;
}

/*
 * Table to correlate inode numbers to path names
 */
struct inrec {
	struct inrec *next;
	ino_t inode;
	dev_t rdev;
	char name[256];
};

#define _PATH_INODES "/var/tmp/memusage.inodes"
static struct inrec *inodes;

static void
add_inode(dev_t rdev, ino_t inode, char *str)
{
	struct inrec *new;

	new = (struct inrec *)malloc(sizeof (struct inrec));
	new->next = inodes;
	new->inode = inode;
	new->rdev = rdev;
	strcpy (new->name, str);
	inodes = new;
}

static
find_inode(char *str, dev_t *rdev, ino_t *ino)
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
char *
prdevino(dev_t rdev, ino_t inode, int use)
{
	static long first_call = 1;
	static char str[128];
	struct inrec *current;
	FILE *fp;

	if (first_call) {
		first_call = 0;
		if ((fp = fopen (_PATH_INODES, "r")) != NULL) {
			while (fscanf(fp,"%d %llu %s\n",&rdev,&inode,str) == 3)
				add_inode (rdev, inode, str);
			fclose(fp);
		}
	}

	for (current = inodes; current; current = current->next)
		if (current->inode == inode && current->rdev == rdev) {
			if (use > 1)
				sprintf(str, "%s (%d)", current->name, use);
			else
				strcpy(str, current->name);
			return str;
		}

	sprintf(str, "Dev # = %x, Inode # = %llu", rdev, inode);

	return str;
}

/*
 * Add dynamic paths into dev/ino map list
 */
static void
prdynpaths()
{
	static char *paths[] = { "/var/tmp/.Xshmtrans0",
				 "/tmp/.cadminOSSharedArena",
				 "/var/tmp/.rtmond_shm_file",
				 "/var/tmp/.XsgiShmInfo0",
				 NULL };
	register int i;
	struct stat statd;

	/* Add elements to the list */
	for (i = 0; paths[i] != NULL; ++i) {
		if (stat(paths[i], &statd) < 0)
			continue;
		add_inode (statd.st_dev, statd.st_ino, paths[i]);
	}
}

/*
 * Get page table data
 */
static prpgd_sgi_t *pgd = NULL, *pgds = NULL;

void
getpgdinfo(int fd, struct prmap_sgi *map)
{
	register pgd_t *src, *dst;
	register int i;

        /* Allocate array(s) to hold data */
	if (pgd == NULL) {
		register int size;

		size = sizeof (prpgd_sgi_t);
		size += sizeof (pgd_t) * (map->pr_size / getpagesize());

	        pgd = (prpgd_sgi_t *)calloc(1, size);
	        pgds = (prpgd_sgi_t *)calloc(1, size);
	}

        /* Get page table data */
        pgd->pr_vaddr = map->pr_vaddr;
        if (ioctl(fd, PIOCPGD_SGI, pgd) < 0) {
                return;
        }

	/* Sum page table data */
	pgds->pr_pglen = pgd->pr_pglen;
        src = pgd->pr_data; dst = pgds->pr_data;
        for (i = 0; i < pgd->pr_pglen; ++i, ++dst, ++src) {
		dst->pr_flags |= src->pr_flags;
	}
}

void
prpgdinfo()
{
	register pgd_t *data;
	register int i;

        /* Dump RLD page table info */
        printf("RLD [DATA] page table - (%d)\n", pgd->pr_pglen);
        for (i = 0, data = pgds->pr_data; i < pgds->pr_pglen; ++i, ++data) {
                printf("%02x ", data->pr_flags);
        }
        printf("\n");
}

/*
 * Identify PROGRAM text extent
 */
isprgtxt(struct prmap_sgi *map)
{
	if ((map->pr_mflags & (MA_EXEC|MA_PRIMARY)) == (MA_PRIMARY|MA_EXEC))
		return 1;
	return 0;
}

/*
 * Identify RLD text extent
 */
isrldtxt(struct prmap_sgi *map)
{
	if ((ptrdiff_t)map->pr_vaddr == 0x0fb60000)
		return 1;
	return 0;
}

/*
 * Identify RLD bss extents
 */
isrldbss(struct prmap_sgi *map)
{
	if (((ptrdiff_t)map->pr_vaddr >= 0x0fbc0000) &&
	    ((ptrdiff_t)map->pr_vaddr < 0x0fc40000))
		return 1;
	return 0;
}

/*
 * Identify RLD bss extents
 */
isdevzero(struct prmap_sgi *map)
{
	static dev_t zdev;
	static ino_t zino;

	/* Lookup /dev/zero */
	if (!zdev) {
		find_inode("/dev/zero", &zdev, &zino);
	}

	/* Match? */
	if (map->pr_dev == zdev && map->pr_ino == zino)
		return 1;
	return 0;
}

static void
usage()
{
	fprintf(stderr, "usage: rgaccum [-DMabcdfrtv] [process-id|name]\n");
	exit(0);
}

main(int argc, char **argv)
{
	register struct prmap_sgi *map, *amap, *rmap;
	register int i, flags, fd;
	register struct sregion *rgn;
	register DIR *dirp;
        prmap_sgi_arg_t maparg;
	struct dirent *direntp;
	int c, nmaps, me, pid = 0;
	int Dflg = 1, Sflg = 1, aflg = 1, bflg = 1, cflg = 1, dflg = 1;
	int fflg = 1, sflg = 1, tflg = 1, Mflg = 0, zflg = 0;
	pgno_t tvsize = 0, tpsize = 0, trss = 0, tmsize = 0;
	dev_t mdev = 0, fdev = 0;
	ino_t mino = 0, fino = 0;

	/* Setup region symbol table */
	(void) prdevino(0, 0, 0);
	(void) prdynpaths();
	
	/* Check for command line options */
	while ((c = getopt(argc, argv, "DM:Sabcdfstzr:")) != EOF) {
		switch (c) {
			case 'D':
				Dflg = 0;
				break;
			case 'M':
				switch (optarg[0]) {
				case 'N': Mflg = MA_NOTCACHED; break;
				case 'P': Mflg = MA_PRIMARY; break;
				case 'S': Mflg = MA_SREGION; break;
				case 'W': Mflg = MA_WRITE; break;
				case 'b': Mflg = MA_BREAK; break;
				case 'd': Mflg = MA_COW; break;
				case 'h': Mflg = MA_SHMEM; break;
				case 'p': Mflg = MA_PHYS; break;
				case 's': Mflg = MA_STACK; break;
				case 't': Mflg = MA_EXEC; break;
				default : printf("unknown -M suffix '%c'\n",
						optarg[0]); exit(0);
				}
				break;
			case 'S':
				Sflg = 0;
				break;
			case 'a':
				aflg = 0;
				break;
			case 'b':
				bflg = 0;
				break;
			case 'c':
				cflg = 0;
				break;
			case 'd':
				dflg = 0;
				break;
			case 'f':
				fflg = 0;
				break;
			case 's':
				sflg = 0;
				break;
			case 't':
				tflg = 0;
				break;
			case 'z':
				zflg = 1;
				break;
			case 'r':
				if (!find_inode(optarg, &fdev, &fino)) {
					printf("'%s' not found\n", optarg);
				}
				break;
			case '?':
				usage();
				break;
		}
	}

	/* Look for optional process id argument */
	if (optind != argc) {
		if (isdigit(argv[optind][0])) {
			pid = strtol(argv[optind], &optarg, 10);
			if (*optarg != NULL) {
			    fprintf(stderr, "ERROR: pid is not an integer\n");
			    usage();
			}
		} else if (isalpha(argv[optind][0])) {
			optarg = argv[optind];
		} else
			usage();
	} else {
		pid = 0; optarg = NULL;
	}

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
		me = 0;
		if (ioctl(fd, PIOCPSINFO, &info) < 0) {
			perror("ioctl(PIOCPSINFO)");
			exit(-3);
		}
		if (pid && info.pr_pid != pid)
			continue;
		if (optarg && strcmp(info.pr_fname, optarg))
			continue;
		if (strcmp(info.pr_fname, "rgaccum") == 0) {
			me = 1;
		}
	
		/* Get process map structures */
		maparg.pr_vaddr = (caddr_t)maps;
		maparg.pr_size = sizeof maps;
		if ((nmaps = ioctl(fd, PIOCMAP_SGI, &maparg)) < 0) {
			(void) close(fd);
			continue;
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
					if (me) {
						mdev = map->pr_dev;
						mino = map->pr_ino;
					}
				}

				/* Remember RLD region */
				if (!rmap && isrldtxt(map)) {
					rmap = map;
				}
			}
		}

		/* Try and bill /dev/zero regions to real owners */
		for (map = maps, i = nmaps; i-- > 0; ++map) {
			/* Hack for rld's /dev/zero regions */
			if (isdevzero(map)) {
				if (rmap && isrldbss(map)) {
					map->pr_dev = rmap->pr_dev;
					map->pr_ino = rmap->pr_ino;
					map->pr_mflags |= MA_BREAK;
				}
			}
		}

		/* Scan the table */
		for (map = maps, i = nmaps; i-- > 0; ++map) {
			if (Mflg && !(map->pr_mflags & Mflg))
				continue;
#define MA_COREDATA (MA_SHMEM|MA_COW|MA_WRITE)
#define MA_DATA (MA_COREDATA|MA_BREAK|MA_STACK)
			if (!Dflg && (map->pr_mflags & MA_DATA))
				continue;
			if (!Sflg && (map->pr_mflags & MA_SHMEM))
				continue;
			if (!aflg && !map->pr_dev && !map->pr_ino)
				continue;
			if (!bflg && (map->pr_mflags & MA_BREAK))
				continue;
			if (!cflg && (map->pr_mflags & MA_COW))
				continue;
			if (!dflg && (map->pr_mflags & MA_COREDATA))
				continue;
			if (!sflg && (map->pr_mflags & MA_STACK))
				continue;
			if (!tflg && (map->pr_mflags & MA_EXEC))
				continue;
#define MA_FILE (MA_STACK|MA_BREAK|MA_SHMEM|MA_EXEC|MA_COW)
			if (!fflg && map->pr_dev && map->pr_ino &&
			    (map->pr_mflags & MA_FILE) == 0)
				continue;
			if (map->pr_ino) {
				/* Region filter */ 
				if (fdev && fino) {
					/* Reject if no match */
					if (fdev != map->pr_dev ||
					    fino != map->pr_ino)
						continue;

					/* Use app name for instance */
					if (!amap)
						continue;
					map->pr_dev = amap->pr_dev;
					map->pr_ino = amap->pr_ino;
				}

				add_region(map);
			} else if (amap && !fdev) {
				/* add stack & junk into app region */
				map->pr_dev = amap->pr_dev;
				map->pr_ino = amap->pr_ino;
				add_region(map);
			}
		}

		(void) close(fd);
	}
	closedir(dirp);

	/* Any data */
	if (sregion == NULL)
		exit(0);

	/* Sort the table by weighted-rss sizes */
	sort_regions();

	/* Dump out the region data */
	printf("\tMapped Objects\t\t\t\t\t  Page Counts\n");
	printf("Pathname\t\t\t\t\t  Valid\tPrivate\t Weight\tWritten\n");
	for (rgn = sregion; rgn != NULL; rgn = rgn->next) {
		if (!zflg && rgn->vsize == 0)
			continue;
		if (rgn->dev == mdev && rgn->ino == mino) 
			printf(" %-46s", "<rgaccum - this program>");
		else
			printf(" %-46s", prdevino(rgn->dev,rgn->ino,rgn->use));
		FIXUP(rgn->vsize);
			printf("\t%7d", rgn->vsize);
			tvsize += rgn->vsize;
		FIXUP(rgn->psize);
			printf("\t%7d", rgn->psize);
			tpsize += rgn->psize;
		FIXUP(rgn->rss);
			printf("\t%7d", rgn->rss);
			trss += rgn->rss;
		FIXUP(rgn->msize);
			printf("\t%7d", rgn->msize);
			tmsize += rgn->msize;
		printf("\n");
	}
	printf("Totals\t\t\t\t\t\t%7d\t%7d\t%7d\t%7d\n",
		tvsize, tpsize, trss, tmsize);
	
	exit(0);
}
