/*
 * process.c
 *
 * Code to collect information about memory usage in the system
 *
 */
#define TILES_TO_LPAGES
#define _KMEMUSER
#include <sys/types.h>
#include <sys/procfs.h>
#include <sys/param.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/pfdat.h>
#include <sys/proc.h>
#include <sys/syssgi.h>
#include <sys/tcpipstats.h>
#include <sys/capability.h>

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include "process.h"
#include "inode.h"
#include "pfdat3264.h"

#define PROCDIR "/proc"

/*
 * dirp is for opening /proc
 */
static DIR *dirp = NULL;
static unsigned long kpstart, ktext, ketext, kend, kpbase, kpfdat, kpfdatsz;
static unsigned long kpagesize;

static int pgsize;  	/* page size in 1000 of bytes (ie. 4, 16, etc...) */
static int usize;	/* number of pages for uarea */

/*
 *  static void
 *  KernelInfo()
 *
 *  Description:
 *      Initialize a few constant global kernel variables
 */
static void
KernelInfo(void)
{
	register long k0mask;
	register int fd;

	/* Only do once */
	if (!kend) {
		/* Handle transparent pointer sizing */
		if (sysconf(_SC_KERN_POINTERS) > 32) {
			k0mask = 0;	/* Not required on 64bit kernels. */
			kpfdatsz = sizeof (pfd64_t);
		} else {
			k0mask = 0x80000000L;
			kpfdatsz = sizeof (pfd32_t);
		}

		/* Get some info from sysmp() */
		if ((kpstart = sysmp(MP_KERNADDR, MPKA_PSTART)) == -1)
			perror("sysmp(MP_KERNADDR, MPKA_PSTART)");
		kpstart |= k0mask;
		if ((ktext = sysmp(MP_KERNADDR, MPKA_TEXT)) == -1)
			perror("sysmp(MP_KERNADDR, MPKA_TEXT)");
		if ((ketext = sysmp(MP_KERNADDR, MPKA_ETEXT)) == -1)
			perror("sysmp(MP_KERNADDR, MPKA_ETEXT)");
		if ((kend = sysmp(MP_KERNADDR, MPKA_END)) == -1)
			perror("sysmp(MP_KERNADDR, MPKA_END)");
		if ((kpfdat = sysmp(MP_KERNADDR, MPKA_PFDAT)) == -1)
			perror("sysmp(MP_KERNADDR, MPKA_PFDAT)");
		if ((kpbase = sysmp(MP_KERNADDR, MPKA_KPBASE)) == -1)
			perror("sysmp(MP_KERNADDR, MPKA_KPBASE)");
		
		/* Read some stuff from /dev/kmem */
		fd = open("/dev/kmem", 0);
		if (fd != -1) {
			/* Get value of KPBASE */
			if (lseek(fd, kpbase, SEEK_SET) == -1)
				perror("lseek");
			if (read(fd, (caddr_t)&kpbase, sizeof kpbase) < 0)
				perror("read");
			kpbase |= k0mask;
			close(fd);
		}
	}
}

/*
 *  static PROGRAM *
 *  AddElem(PROGRAM *head, char *progName, char *mapName, char *mapType,
 *          long privSize, long weightSize, long resSize, long size,
 *          pid_t pid, void *vaddr, int markNProc)
 *
 *  Description:
 *      Add an element to our program list.
 *
 *  Parameters:
 *      head	     The list to add this element to
 *      progName     Name of program to add
 *	privSize     Size of private pages
 *      weightSize   Weighted memusage size
 *      resSize	     resident size of program to add
 *      size	     total size of program to add
 *      pid	     process id of program to add
 *      vaddr        virtual address of region if this is a region
 *      markNProc    1 if we care about keeping nProc up to date
 *
 *  Returns:
 *      A new list (the element being added may go at the beginning)
 */

static PROGRAM *
AddElem(PROGRAM *head, char *progName, char *mapName, char *mapType,
	long privSize, long weightSize, long resSize,
	long size, pid_t pid, void *vaddr, int markNProc)
{
    PROGRAM *elem, *prev, *pidPlace;
    PIDLIST *pids;
    int cmp;

    elem = head;
    prev = NULL;
    pidPlace = NULL;

    /*
     * Look for another copy of the same program already running
     */
    while (elem && (cmp = strcmp(progName, elem->progName)) != 0) {
	/*
	 * Remember where the program should go (we sort by pid) in
	 * case this is the first of its kind that we've found
	 */
	if (pid < elem->pid && !pidPlace) {
	    pidPlace = prev;
	}
	prev = elem;
	elem = elem->next;
    }
    
    if (elem && cmp == 0) {
	/*
	 * Another copy is running; just add the new one's stats to
	 * the old one
	 */
	elem->privSize += privSize;
	elem->weightSize += weightSize;
	elem->resSize += resSize;
	elem->size += size;
	if (markNProc) {
	    elem->nProc++;
	}
	/*
	 * Add the new pid to the end of the pidlist for this program
	 * name.
	 */
	pids = elem->pids;
	while (pids->next) {
	    pids = pids->next;
	}
	pids->next = malloc(sizeof *pids);
	pids->next->pid = pid;
	pids->next->prev = pids;
	pids->next->next = NULL;
    } else {
	/*
	 * A new program is encountered.  Allocate a new element, and
	 * insert it into the list.
	 */
	elem = malloc(sizeof *elem);

	/*
	 * progName and mapName get strduped because the point to
	 * things like prinfo structs that change.  mapType points to
	 * static storage that does not change.  Be very careful if
	 * you change this, because FreeBloat (in this file) knows
	 * about this, AND SO DOES DrawSetup (in draw.c)!!!
	 */
	elem->progName = strdup(progName);
	elem->mapName = mapName ? strdup(mapName) : NULL;
	elem->mapType = mapType; /* Don't need to dup this; it points */
				 /* to static storage that won't change */
	elem->privSize = privSize;
	elem->weightSize = weightSize;
	elem->resSize = resSize;
	elem->size = size;
	elem->vaddr = vaddr;
	elem->nProc = 1;
	elem->print = 1;
	
	/*
	 * Set elem->pid for sorting and identification and posterity,
	 * and initialize the pidlist.
	 */
	elem->pid = pid;
	elem->pids = malloc(sizeof *elem->pids);
	elem->pids->prev = NULL;
	elem->pids->next = NULL;
	elem->pids->pid = pid;

	if (!prev) {
	    elem->prev = NULL;
	    elem->next = head;
	    head = elem;
	} else {
	    if (pidPlace) {
		prev = pidPlace;
	    }
	    elem->next = prev->next;
	    elem->prev = prev;
	    prev->next = elem;
	    if (elem->next) {
		elem->next->prev = elem;
	    }
	}
    }
    return head;
}

/*
 *  static PROGRAM *
 *  SortList(PROGRAM *head, int flag)
 *
 *  Description:
 *	Sort the element list by physical size (largest first)
 *
 *  Parameters:
 *      head	     Start of the double linked list of elements
 *
 *  Returns:
 *	New head of the list
 */

static PROGRAM *
SortList(PROGRAM *head, int flag)
{
	PROGRAM *new = NULL, *prev, *walk, *next, *scan;

	/* Walk the original list */
	for (walk = head; walk != NULL; walk = next) {
		/* Correct weightSize values */
		if (flag != 0) {
			walk->weightSize += MA_WSIZE_FRAC - 1;
			walk->weightSize /= MA_WSIZE_FRAC;
		}

		/* Get it now, will be rewritten below */
		next = walk->next;

		/* No sort, just scale */
		if (flag < 0) {
			new = head;
			continue;
		}

		/* Put into new sorted list */
		for (prev = NULL, scan = new; scan != NULL;
		     scan = scan->next) {
			if (walk->weightSize > scan->weightSize)
				break;
			prev = scan;
		}
		
		/* Insert into new list before current element */
		if (scan) {
			walk->next = scan;
			walk->prev = scan->prev;
			scan->prev = walk;
			if (prev)
				prev->next = walk;
			else
				new = walk;
		} else if (new) {
			walk->prev = prev;
			prev->next = walk;
			walk->next = NULL;
		} else {
			new = walk;
			walk->prev = NULL;
			walk->next = NULL;
		}
	}
	
	return new;
}

/*
 * Indentify PROGRAM text extent
 */
static int
isprgtxt(struct prmap_sgi *map)
{
	if ((map->pr_mflags & (MA_EXEC|MA_PRIMARY)) == (MA_PRIMARY|MA_EXEC))
		return 1;
	return 0;
}

/*
 * Indentify RLD text extent
 */
static int
isrldtxt(struct prmap_sgi *map)
{
	if ((unsigned)map->pr_vaddr == 0x0fb60000)
		return 1;
	return 0;
}

/*
 * Indentify RLD bss extents
 */
static isrldbss(struct prmap_sgi *map)
{
	if (((unsigned)map->pr_vaddr >= 0x0fbc0000) &&
	    ((unsigned)map->pr_vaddr < 0x0fc40000))
		return 1;
	return 0;
}

/*
 *  static char *
 *  MapFlags(prmap_sgi_t *map)
 *
 *  Description:
 *      Translate the flags for a region into a meaningful word that
 *      describes what type of region it is
 *
 *  Parameters:
 *      map  region pointer
 *
 *  Returns:
 *      string describing the type of region
 */

static char *
MapFlags(prmap_sgi_t *map)
{
    if (map->pr_mflags & MA_PHYS) {
	return "Physical Device";
    } else if (map->pr_mflags & MA_STACK) {
	return "Stack";
    } else if (map->pr_mflags & MA_BREAK) {
	return "Break";
    } else if (map->pr_mflags & MA_EXEC) {
	return "Text";
    } else if (map->pr_mflags & MA_COW) {
	return "Data";
    } else if (map->pr_mflags & MA_SHMEM) {
	return "Shmem";
    } else if ((map->pr_mflags & (MA_READ | MA_WRITE)) ==
	       (MA_READ | MA_WRITE)) {
	return "RW";
    } else if (map->pr_mflags & MA_READ) {
	return "RO";
    }
    return "Other";
}

/*
 *  static int
 *  PteAccounting(prmap_sgi_t *maps, int nmaps)
 *
 *  Description:
 *	Try and figure out how many page tables are in core
 *
 *  Parameters:
 *      maps  region pointer
 *	nmaps number of maps in region list
 */
static PteAccounting(prmap_sgi_t *maps, int nmaps)
{
	unsigned pteblksize = (kpagesize / sizeof (caddr_t)) * kpagesize;
	prmap_sgi_t *map;
	long segno, refcnt, lrefcnt = 0, space = 0, ssize;
	caddr_t vaddr;
	int i, last = -1;

	/* Search all the segments */
	for (map = maps, i = 0; i < nmaps; ++map, ++i) {
		/* Skip empty ones */
		if (map->pr_vsize == 0)
			continue;

		/* Walk segment one segment at a time (XXX fix me) */
		vaddr = map->pr_vaddr; 
		refcnt = map->pr_mflags >> MA_REFCNT_SHIFT;
		for (ssize = map->pr_size; ssize > 0; ssize -= pteblksize) {
			/* If not same as last time & incore, then bill me */
			segno = (long)vaddr / pteblksize;
			if (last == -1)
				last = segno;
			if (last != segno) {
				space += kpagesize / lrefcnt;
				last = segno;
				lrefcnt = 0;
			}
			vaddr += pteblksize;
			if (!lrefcnt || (refcnt < lrefcnt)) {
				lrefcnt = refcnt;
			}
		}
	}
	if (lrefcnt) {
		space += kpagesize / lrefcnt;
	}

	return space / 1024;
}

/*
 *  static void OpenProcDir(void)
 *
 *  Description:
 *	Open /proc if it's not open, and rewind.  dirp is a global
 *	variable.
 */

static void OpenProcDir(void)
{
    struct paramconst p;

    /* Get system page size */
    if (!kpagesize) {
	kpagesize = getpagesize();

	if (syssgi(SGI_CONST, SGICONST_PARAM, &p, sizeof(p), 0) == -1) {
		usize = 1; /* guess */
        } else {
		usize = p.p_usize + p.p_extusize;
        }
        pgsize = kpagesize/1024;
    }


    if (!dirp) {
	if ((dirp = opendir(PROCDIR)) == NULL) {
	    perror(PROCDIR);
	    exit(1);
	}
    }

    rewinddir(dirp);
}     

/*
 *  static void GetObjInfoPriv(char *objName, PROGRAM **all, PROGRAM **objp)
 *
 *  Description:
 *      Get physical memory usage information for all mapped objects
 *      in the system.
 *
 *  Parameters:
 *      objName	name of object to get detailed information for
 *      all     gets bloat for all objectss
 *      objp    gets bloat for a specific object
 */

static void GetObjInfoPriv(char *objName, PROGRAM **all, PROGRAM **objp)
{
    struct dirent *dent;
    char procFile[MAXPATHLEN];
    int status, fd, nmaps, i, pid = 1, ppid = 1;
    register struct prmap_sgi *map, *amap, *rmap, *bmap, *maps;
    prmap_sgi_arg_t maparg;
    PROGRAM *list = NULL;
    PROGRAM *plist = NULL;
    unsigned long refCount;
    double rss;
    char *mname, *flagName;
    struct prpsinfo info;
    long kstack = 0, ptespace = 0;


    OpenProcDir();

    while ((dent = readdir(dirp)) != NULL) {
	if (!isdigit(dent->d_name[0]))
	    continue;

	snprintf(procFile, sizeof procFile, "%s/%s", PROCDIR,
		 dent->d_name);

	status = (fd = open(procFile, O_RDONLY)) != -1
	    && ioctl(fd, PIOCPSINFO, &info) != -1;

	if (!status) {
		close(fd);
		continue;
	}

	if (ioctl(fd, PIOCNMAP, &nmaps) == -1) {
		close(fd);
		continue;
	}

	/*
	 * Dynamically determine the number of entries for the process and
	 * allocate that many map entries.
	 */
	if (nmaps) {
		nmaps++; /* Allocate an extra entry for termination */
		maps = malloc(sizeof(struct prmap_sgi) * nmaps);
		maparg.pr_vaddr = (caddr_t)maps;
		maparg.pr_size = sizeof(struct prmap_sgi)*nmaps;

		if ((nmaps = ioctl(fd, PIOCMAP_SGI, &maparg)) == -1) {
			free(maps);
			close(fd);
			continue;
		}
	}
	close(fd);

	/* charge for kernel stack */
	kstack += (KSTKSIZE * kpagesize / 1024);

	/* Try and figure out how many page tables are resident in memory */
	ptespace += PteAccounting(maps, nmaps);

	amap = NULL;
	rmap = NULL;

	/* Search regions to find app segment & rld segment */
	for (map = maps, i = nmaps; i-- > 0; ++map) {
	    /* Hack for app region */
	    if (!amap && isprgtxt(map)) {
		amap = map;
	    }

	    /* Remember primary BRK region */
	    if ((map->pr_mflags & (MA_PRIMARY|MA_BREAK)) ==
				  (MA_PRIMARY|MA_BREAK)) {
		bmap = map;
	    }
	    
	    /* Remember RLD region */
	    if (!rmap && isrldtxt(map)) {
		rmap = map;
	    }
	}
	
	/* Scan the table */
	for (map = maps, i = nmaps; i-- > 0; ++map) {
	    if (map->pr_mflags & MA_PHYS) {
		continue;
	    }

	    /* Try and bill /dev/zero regions to real owners */
	    if (map->pr_mflags & MA_MAPZERO) {
	        /* Hack for rld's /dev/zero regions */
		if (rmap && isrldbss(map)) {
		    map->pr_dev = rmap->pr_dev;
		    map->pr_ino = rmap->pr_ino;
		    map->pr_mflags |= MA_BREAK;
		}
		/* Otherwise bill to apps primary break region */
		else if (bmap) {
		    map->pr_dev = bmap->pr_dev;
		    map->pr_ino = bmap->pr_ino;
		    map->pr_mflags |= MA_BREAK;
		}
	    } else if (amap && ((map->pr_mflags & MA_STACK) ||
				(map->pr_mflags & MA_BREAK))) {
		map->pr_dev = amap->pr_dev;
		map->pr_ino = amap->pr_ino;
	    }

	    if (amap &&
		map->pr_dev == amap->pr_dev && map->pr_ino == amap->pr_ino) {
		mname = info.pr_fname;
	    } else {
		mname = map->pr_ino ?
		    InodeLookup(map->pr_dev, map->pr_ino) : NULL;
	    }

	    flagName = MapFlags(map);
	    refCount = map->pr_mflags >> MA_REFCNT_SHIFT;
	    rss = (double)map->pr_wsize;
	    rss *= pgsize;			/* use 1KB resolution */
	    rss /= (double)refCount;
	    list = AddElem(list, mname ? mname : flagName,
			   mname, flagName,
			   (map->pr_psize * pgsize) / refCount, rss,
			   0, 0, pid++, NULL, 0);
	    if (objName && mname && strcmp(objName, mname) == 0) {
	    	plist = AddElem(plist, flagName, mname, flagName,
			   (map->pr_psize * pgsize) / refCount, rss,
			   0, 0, ppid++, NULL, 0);
	    }
	}
	if (nmaps)
		free(maps);
    }
    list = SortList(list, 1);
    if (plist) plist = SortList(plist, -1);

    list = AddElem(list, "Kernel Stacks", "Kernel Stacks", IRIX,
		   kstack, kstack, 0, 0, pid++, NULL, 0);

    list = AddElem(list, "PTEs", "PTEs", IRIX,
		   ptespace, ptespace, 0, 0, pid++, NULL, 0);

    *all = list;
    *objp = plist;
}

/*
 *  static void
 *  GetProcInfoPriv(char *procName, pid_t pid, PROGRAM **all, PROGRAM **proc)
 *
 *  Description:
 *      Get memory usage information for all processes in the system.
 *      In addition, collect detailed information for procName if it
 *      is non-NULL, or collect detailed information for pid if it is
 *      not -1.
 *
 *  Parameters:
 *      procName  name of program to get detailed information for
 *      pid       process id to get detailed information for
 *      all       gets bloat for all processes
 *      proc      gets bloat for a specific process
 */

static void
GetProcInfoPriv(char *procName, pid_t pid, PROGRAM **all, PROGRAM **proc)
{
    struct dirent *dent;
    char procFile[MAXPATHLEN];
    int fd, i;
    struct prpsinfo info;
    register prmap_sgi_t *map, *amap, *maps;
    long *physUse;
    prmap_sgi_arg_t maparg;
    unsigned nmaps;
    PROGRAM *list = NULL;
    PROGRAM *plist = NULL;
    char mapObjName[MAXPATHLEN + 20], *mname, *flagName;
    int status;
    struct tileinfo tinfo;
    struct rminfo rminfo;
    struct kna kna;
    struct minfo minfo;
    long userTotal, pteTotal, kstack, ptespace, wrss, kmem, mem, privSize;
    long weighted, refCount;
    long resident, total;


    OpenProcDir();

    userTotal = pteTotal = 0;
    while ((dent = readdir(dirp)) != NULL) {
	if (!isdigit(dent->d_name[0]))
	    continue;

	snprintf(procFile, sizeof procFile, "%s/%s", PROCDIR,
		 dent->d_name);

	status = (fd = open(procFile, O_RDONLY)) != -1
	    && ioctl(fd, PIOCPSINFO, &info) != -1;

	if (!status) {
		close(fd);
		continue;
	}

	/*
	 * Dynamically determine the number of entries for the process and
	 * allocate that many map entries.
	 */

	if (ioctl(fd, PIOCNMAP, &nmaps) == -1) {
		close(fd);
		continue;
	}

	maps = NULL;
	physUse = NULL;
	if (nmaps) {
		nmaps++; /* Allocate an extra entry for termination */
		maps = malloc(sizeof(struct prmap_sgi) * nmaps);
		physUse = malloc(sizeof(long) *nmaps);
		maparg.pr_vaddr = (caddr_t)maps;
		maparg.pr_size = sizeof(struct prmap_sgi)*nmaps;

		if ((nmaps = ioctl(fd, PIOCMAP_SGI, &maparg)) == -1) {
			free(maps);
			free(physUse);
			close(fd);
			continue;
		}
	}
	close(fd);

	wrss = 0;
	privSize = 0;
	/* Compute weighted rss from valid and usage counts */
	for (map = maps, i = nmaps; i-- > 0; ++map) {
	    if ((map->pr_mflags & MA_PHYS) == 0) {
		refCount = map->pr_mflags >> MA_REFCNT_SHIFT;
		weighted = map->pr_wsize;
		weighted *= pgsize;		/* use 1KB resolution */
		weighted /= MA_WSIZE_FRAC;
		weighted /= refCount;
		physUse[i] = (long)weighted;
		wrss += weighted;
		if ((map->pr_mflags & MA_SHMEM) == 0) {
		    privSize += (map->pr_psize * pgsize) / refCount;
		}
	    } else {
		physUse[i] = 0;
	    }
	}
	userTotal += wrss;

	/* charge for kernel stack */
	kstack = (KSTKSIZE * kpagesize / 1024);
	userTotal += kstack;
	privSize += kstack;

	/* Try and figure out how many page tables are resident in memory */
	ptespace = PteAccounting(maps, nmaps);
	pteTotal += ptespace;
	privSize += ptespace;

	list = AddElem(list, info.pr_fname, NULL, NULL, privSize,
		       (long)wrss + kstack + ptespace,
		       info.pr_rssize * pgsize + kstack + ptespace,
		       info.pr_size * pgsize + kstack + ptespace,
		       info.pr_pid, NULL, 1);

	if (pid == -1 && procName && strcmp(procName, info.pr_fname) == 0
	    || pid != -1 && pid == info.pr_pid) {
	    amap = 0;
	    /*
 	     * Find program text region, so we can get good names for
	     * the regions associated with each executable file even
	     * if that file isn't in our inode map.
	     */
	    for (map = maps, i = nmaps; i-- > 0; ++map) {
		if (isprgtxt(map)) {
		    amap = map;
		    break;
		}
	    }
		
	    /* List all the address space segments in virtual order */
	    for (map = maps, i = nmaps; i-- > 0; ++map) {
		if ((map->pr_mflags & MA_MAPZERO) && isrldbss(map)) {
		    mname = "rld";
		    map->pr_mflags |= MA_BREAK;
		} else if ((map->pr_mflags & MA_STACK) ||
			   (map->pr_mflags & MA_BREAK) ||
			   (amap &&
			    map->pr_dev == amap->pr_dev && map->pr_ino ==
			    amap->pr_ino)) {
		    mname = procName;
		} else {
		    mname = map->pr_ino ?
			InodeLookup(map->pr_dev, map->pr_ino) : NULL;
		}
		flagName = MapFlags(map);
		if (mname) {
		    snprintf(mapObjName, sizeof mapObjName, "%s (%s)",
			     flagName, mname);
		    if (strlen(mapObjName) > 15) {
			mapObjName[15] = ')';
			mapObjName[16] = '\0';
		    }
		}
		refCount = map->pr_mflags >> MA_REFCNT_SHIFT;
		if (map->pr_mflags & MA_PHYS) {
		    resident = 0;
		    total = 0;
		} else {
		    resident = map->pr_vsize * pgsize;
		    total = (map->pr_size + 1023) / 1024;
		}
		plist = AddElem(plist, mname ? mapObjName : flagName,
				mname, flagName,
				((map->pr_mflags & MA_PHYS) ||
				 (map->pr_mflags & MA_SHMEM)) ?
				0 : (map->pr_psize * pgsize) / refCount,
				physUse[i], resident, total, nmaps - i,
				map->pr_vaddr, 0);
	    }
	
	    plist = AddElem(plist, "Kernel Stack", "Kernel Stack",
			    IRIX, kstack, kstack, kstack, kstack,
			    nmaps+1, NULL, 0);
	    plist = AddElem(plist, "PTEs", "PTEs",
			    IRIX, ptespace, ptespace, ptespace, ptespace,
			    nmaps+2, NULL, 0);
	}
	if (nmaps) {
		free(maps);
		free(physUse);
	}
    }

    /*
     * Special case if procName is "Irix".  We get info from the sysmp
     * call.
     */
    if (procName && strcmp(procName, IRIX) == 0) {
	KernelInfo();
	if (sysmp(MP_SAGET, MPSA_RMINFO, &rminfo, sizeof rminfo) == -1
	    ||sysmp(MP_SAGET, MPSA_MINFO, &minfo, sizeof minfo) == -1
	    ||sysmp(MP_SAGET, MPSA_TCPIPSTATS, (char *)&kna, 
	      sizeof(kna)) == -1) {
	    perror("sysmp");
	} else {
	    i = 0;

	    /*
	     * Non-filesystem parts of kernel are as follows -
	     */
	    kmem = (rminfo.physmem-(rminfo.availrmem+rminfo.bufmem)) * pgsize;

	    /*
	     * Subtract out PTEs found attached to user processes (see PMAP)
	     */
	    kmem -= pteTotal;

	    /*
	     * Filesystem data & metadata -
	     */
	    mem = ((rminfo.chunkpages + rminfo.dpages) +
		  rminfo.dchunkpages) * pgsize;

	    plist = AddElem(plist, "FS Cache", "FS Cache", IRIX, mem, mem,
			    0, 0, i++, NULL, 0);

	    mem = rminfo.bufmem * pgsize;
	    plist = AddElem(plist, "FS Control", "FS Control", IRIX,
			    mem, mem, 0, 0, i++, NULL, 0);

	    /*
	     * Subtract streams from heap, since streams memory is
	     * part of the heap
	     */
	    mem = minfo.heapmem / 1024 - rminfo.strmem * pgsize;
	    plist = AddElem(plist, "Heap", "Heap", IRIX,
			    mem, mem, 0, 0, i++, NULL, 0);
	    kmem -= mem;

	    mem = rminfo.strmem * pgsize;
	    plist = AddElem(plist, "Streams", "Streams", IRIX,
			    mem, mem, 0, 0, i++, NULL, 0);
	    kmem -= mem;

	    mem = kna.mbstat.m_clusters * pgsize;
	    plist = AddElem(plist, "BSD Networking",
			    "BSD Networking", IRIX,
			    mem, mem, 0, 0, i++, NULL, 0);
	    kmem -= mem;

	    mem = ketext - kpstart;		/* Kernel Text */
	    mem += kend - ketext;		/* Kernel Data */
	    if (kpbase > kend) {
		/* pfdat is backwards from kpbase relative to physmem */
	        kpfdat = kpbase - (rminfo.physmem -
			   ((kpbase - kpstart) / (pgsize * 1024))) * 
				kpfdatsz;
		mem += kpbase - kpfdat;		/* Page Frames */
		mem += kpfdat - kend;		/* Kernel Tables */
	    }
	    mem /= 1024;
	    kmem -= mem;

	    /*
	     * (PMAP) Subtract pmap memory since we charge it to processes
	     */
	    mem = rminfo.pmapmem * pgsize - pteTotal;
	    if (mem > 0) {
		plist = AddElem(plist, "Pmap Overhead", "Pmap Overhead",
				IRIX, mem, mem, 0, 0, i++, NULL, 0);
		kmem -= mem;
	    }

#ifdef NOTYET
	    /*
	     * Digital media data structures
	     */
	    mem = minfo.dmedia * pgsize;
	    if (mem > 0) {
		plist = AddElem(plist, "Digital media", "Digital media",
				IRIX, mem, mem, 0, 0, i++, NULL, 0);
		kmem -= mem;
	    }
#endif
#ifdef TILES_TO_LPAGES
	    /*
	     * Tiles (Physically contiguous 64k chunks)
	     */
	    if (sysmp(MP_SAGET, MPSA_TILEINFO, &tinfo, sizeof tinfo) != -1) {
		mem = tinfo.ttile * pgsize;
		if (mem > 0) {
	    	    plist = AddElem(plist, "Tiles", "Tiles", IRIX,
				mem, mem, 0, 0, i++, NULL, 0);
		    kmem -= mem;
		}
	    }
#endif

	    plist = AddElem(plist, "Other", "Other", IRIX,
			    kmem, kmem, 0, 0, i++, NULL, 0);

	    if (kpbase > kend) {
		if (kpbase > kpfdat && kpfdat > kend) {
		    mem = (kpbase - kpfdat) / 1024;
		    plist = AddElem(plist, "Page Frame Data",
				    "Page Frame Data",
				    IRIX, mem, mem, 0, 0, i++, NULL, 0);
		    mem = (kpfdat - kend) / 1024;
		    plist = AddElem(plist, "Kernel Tables", "Kernel Tables",
				    IRIX, mem, mem, 0, 0, i++, NULL, 0);
		} else {
		    mem = (kpbase - kend) / 1024;
		    plist = AddElem(plist, "Kernel Tables", "Kernel Tables",
				    IRIX, mem, mem, 0, 0, i++, NULL, 0);
		}
	    }
	    mem = (kend - ketext) / 1024;
	    plist = AddElem(plist, "Unix Data Space", "Unix Data Space",
			    IRIX, mem, mem, 0, 0, i++, NULL, 0);
	    if ((ktext - kpstart) > 2*kpagesize) {
		mem = (ketext - ktext) / 1024;
		plist = AddElem(plist, "Unix Code Space", "Unix Code Space",
				IRIX, mem, mem, 0, 0, i++, NULL, 0);
		mem = (ktext - kpstart) / 1024;
		plist = AddElem(plist, "Symmon", "Symmon", IRIX,
				mem, mem, 0, 0, i++, NULL, 0);
	    } else {
		mem = (ketext - kpstart) / 1024;
		plist = AddElem(plist, "Unix Code Space", "Unix Code Space",
				IRIX, mem, mem, 0, 0, i++, NULL, 0);
	    }
	}
    }
    *all = list;
    *proc = plist;
}

/*
 *  void GetObjInfo(char *objName, PROGRAM **all, PROGRAM **objp)
 *
 *  Description:
 *      Wrapper for GetObjInfoPriv; aquire capabilities first, drop
 *      them after.
 *
 *  Parameters:
 *      objName	name of object to get detailed information for
 *      all     gets bloat for all objectss
 *      objp    gets bloat for a specific object
 */

void GetObjInfo(char *objName, PROGRAM **all, PROGRAM **objp)
{
    cap_t ocap;
    const cap_value_t cap[3] = { CAP_DAC_WRITE,
				 CAP_DAC_READ_SEARCH,
				 CAP_FOWNER };
    ocap = cap_acquire(sizeof cap/sizeof cap[0], cap);

    GetObjInfoPriv(objName, all, objp);

    cap_surrender(ocap);
}

/*
 *  void
 *  GetProcInfo(char *procName, pid_t pid, PROGRAM **all, PROGRAM **proc)
 *
 *  Description:
 *      Wrapper for GetProcInfoPriv.  Acquire capabilities before
 *      calling, drop them after.
 *
 *  Parameters:
 *      procName  name of program to get detailed information for
 *      pid       process id to get detailed information for
 *      all       gets bloat for all processes
 *      proc      gets bloat for a specific process
 */
void
GetProcInfo(char *procName, pid_t pid, PROGRAM **all, PROGRAM **proc)
{
    cap_t ocap;
    const cap_value_t cap[3] = { CAP_DAC_WRITE,
				 CAP_DAC_READ_SEARCH,
				 CAP_FOWNER };
    ocap = cap_acquire(sizeof cap/sizeof cap[0], cap);
    GetProcInfoPriv(procName, pid, all, proc);
    cap_surrender(ocap);
}

/*
 *  void
 *  FreeBloat(PROGRAM *bloat)
 *
 *  Description:
 *      Free all the memory used by a bloat list, so we don't
 *      ourselves become too bloated :-)
 *
 *  Parameters:
 *      bloat
 */

void
FreeBloat(PROGRAM *bloat)
{
    PROGRAM *next;
    PIDLIST *pids, *npid;

    while (bloat) {
	pids = bloat->pids;
	while (pids) {
	    npid = pids->next;
	    free(pids);
	    pids = npid;
	}

	next = bloat->next;
	free(bloat->progName);
	if (bloat->mapName) {
	    free(bloat->mapName);
	}
	free(bloat);
	bloat = next;
    }
}
