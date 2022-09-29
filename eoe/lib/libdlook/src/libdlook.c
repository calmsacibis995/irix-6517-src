#include <signal.h>
#include <sys/pmo.h>
#include <sys/types.h>
#include <sys/attributes.h>
#include <sys/conf.h>
#include <sys/hwgraph.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <invent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <abi_mutex.h>
#include <procfs/procfs.h>
#include <fcntl.h>
#include <sys/iograph.h>
#include <sys/times.h>
#include <sys/syssgi.h>
#include <unistd.h>

extern int _fdata[], _ftext[], _etext[];

static void dlook_handler(int sig, int code , sigcontext_t *sc);

#if _MIPS_SZPTR == 32
#define TOPOFSTACK 0x7fff8000L
#endif

#if _MIPS_SZPTR == 64
#define TOPOFSTACK 0x10000000000
#endif

extern int
__pm_get_page_info(void* base_addr, size_t length, pm_pginfo_t* pginfo_buf,
		   int buf_len);

#define d __dlook_d

struct dlook_info {
	abilock_t is_busy;
	long period;
	FILE *outputfile;
	int verbose;
	int print_physical;
	int print_color;
	int waysize;
	clock_t t0;
} *d;

/* log2
 *
 * Determine the integral part of the base-2 logarithm of a number
 * i.e. log2 = (unsigned long) floor(log(num)/log(2))
 */
static unsigned long
log2(unsigned long num) {
	unsigned long log = 0;

	if (num < 2) {
		return 0;
	}

	while (num > 1) {
		log++;
		num >>=1;
	}

	return log;
}

/* va2pa
 *
 * Translate a virtual address to a physical address
 */
static unsigned long long
va2pa(void *va, int pagesize) {
	unsigned int pfn;
	int pgsz;

	if (syssgi(SGI_PHYSP,va,&pfn) != 0) {
		perror("Virtual to physical mapping failed");
		exit(1);
	}

	pgsz = getpagesize();
	return ( ((unsigned long long) pfn << log2(pgsz)) |
	         ((unsigned long long) va & (pgsz-1)) );
}

/* get_color
 *
 * Determine the color of a page
 */
static unsigned long long
get_color(unsigned long long addr, int pagesize) {
	if(pagesize >= d->waysize) {
		return 0LL;
	}
  
	return ((addr & ((unsigned long long)(d->waysize) - 1)) >> log2(getpagesize()));
}

/* print_page_info
 *
 * Print information about a particular memory page
 */
static void
print_page_info(void *va, int pagesize) {
	unsigned long long pa;
  
	pa = va2pa((void*)va, pagesize);
  
	if(d->print_physical && !(d->print_color)) {
		fprintf(d->outputfile, "\t0x%011llx -> 0x%011llx\n",
			(unsigned long long) va, pa);
	}

	if(d->print_color) {
		fprintf(d->outputfile, "\t0x%011llx (color %03llx) -> 0x%011llx (color %03llx) (%d color bits)\n",
			(unsigned long long) va, 
			get_color((unsigned long long) va, pagesize), 
			pa, get_color(pa, pagesize),
			(pagesize < d->waysize) ? (int) (log2(d->waysize)-log2(pagesize)) : 0);
	}
}
 
/* do_memory
 *
 * Memory-map a page-sized portion of memory
 */
static void 
do_memory(void) {
	int		fd;
	unsigned int	*mapped;

	if ((fd = open("/dev/zero", O_RDWR)) < 0) {
		perror("Can't open /dev/zero");
		exit(1);
	}

	mapped = (unsigned int *) mmap(NULL,getpagesize(),PROT_READ|PROT_WRITE,
				       MAP_SHARED|MAP_AUTORESRV,fd,0);
	if (NULL == mapped) {
		perror("mmap of /dev/zero failed");
		exit(1);
	}
  
	d = (struct dlook_info *) mapped;
}

/* do_args
 *
 * Process environment variables
 */
static void
do_args(void) {
	char *c;
  
	/* Determine the output file and open it if needed */
	c = getenv("__DLOOK_OUTPUTFILE_");
	if (!c) {
		c = "stderr";
	}

	if (!strcmp(c,"stdout")) {
		d->outputfile = stdout;
	} else if (!strcmp(c,"stderr")) {
		d->outputfile = stderr;
	} else {
		d->outputfile = fopen(c,"a");
		if (NULL == d->outputfile) {
			fprintf(stderr,"dlook: cannot open '%s' for writing\n",c);
			exit(1);
		}
		if (fcntl(fileno(d->outputfile), F_SETFD, FD_CLOEXEC) == -1) {
			fprintf(stderr,"dlook: cannot open '%s' for writing\n",c);
			exit(1);
		}
	}

	/* Do we want verbose output? */
	c = getenv("__DLOOK_VERBOSE_");
	if (c) {
		d->verbose = 1;
	} else {
		d->verbose = 0 ;
	}

	/* Determine the sampling period */
	c = getenv("__DLOOK_PERIOD_");
	if (c) {
		d->period = strtol(c,(char **)NULL,0);
	} else {
		d->period = 0 ;
	}
	if(d->verbose) {
		fprintf(d->outputfile,"dlook: period = %ld\n",d->period);
	}
    
	/* Print physical virtual to physical mappings? */
	c = getenv("__DLOOK_PRINT_PHYSICAL_");
	if (c) {
		d->print_physical = 1;
	} else {
		d->print_physical = 0;
	}
	if(d->verbose) {
		fprintf(d->outputfile,"dlook: print_physical = %d\n", d->print_physical);
	}

	/* Print page colors? */
	c = getenv("__DLOOK_PRINT_COLOR_");
	if(c) {
		d->print_color = 1;
	} else {
		d->print_color = 0;
	}
	if(d->verbose) {
		fprintf(d->outputfile,"dlook: print_color = %d\n", d->print_color);
	}

	{
		struct tms tms;
		d->t0 = times(&tms);
	}  
}

/* do_waysize
 *
 * Determine the size of one way of the secondary cache
 */
static void
do_waysize(void) {
	int setassoc;
	int scachesize;

	if (syssgi(SGI_CONST, SGICONST_SCACHE_SETASSOC, &setassoc) != 0) {
		perror("Secondary cache set associativity query failed");
		exit(1);
	}
	if (syssgi(SGI_CONST, SGICONST_SCACHE_SIZE, &scachesize) != 0) {
		perror("Secondary cache size query failed");
		exit(1);
	}

	d->waysize = scachesize / setassoc;

	if (d->verbose) {
		fprintf(d->outputfile, "dlook: waysize = %d\n", d->waysize);
	}
}

/* do_trigger
 *
 * Arm the sampling period trigger
 */
static void 
do_trigger(void) {
	if ( d->period ) {
		struct itimerval timer;
		timer.it_interval.tv_sec  = (int) (d->period / 1000000);
		timer.it_interval.tv_usec = (int) (d->period % 1000000);
		timer.it_value = timer.it_interval;
		sigset(SIGPROF,dlook_handler);
		setitimer(ITIMER_PROF,&timer,0);
	}
}

/* memory_summary
 *
 * Summarize the address mappings for a section of memory
 */
static void 
memory_summary(void* start, size_t size) {
	pm_pginfo_t		node, last_node;
	int			n;
	void			*vmin = 0, *end = 0, *stop;
	char			devname[160], *c;
	int			length, count = 0;
	unsigned long long	va, pa;
  
#if _MIPS_SZPTR == 32
	if(size <= 0) {
		size = 0x7fffffff;
	}
#else
	if(size <= 0) {
		size = 0x7fffffffff;
	}
#endif

	if(d->verbose) {
		fprintf(d->outputfile, "0x%llx - 0x%llx\n",
			(long long) start, (long long) start + (long long) size);
	}

	end = start;
	n = __pm_get_page_info(end, size, &node, 1);
	end = vmin = node.vaddr;
	last_node = node;
	last_node.vaddr = last_node.vaddr - last_node.page_size; /* For 1st it. */
	if (n == 0) {
		return ;
	}

	stop = (void*) ((long long) start + (long long) size);
	while (size && (n = __pm_get_page_info(end, size, &node, 1)) > 0 ) {
		if (last_node.page_size != node.page_size ||
		    last_node.node_dev != node.node_dev || 
		    last_node.vaddr + last_node.page_size != node.vaddr) {
			length = sizeof (devname);
			c = dev_to_devname(last_node.node_dev, devname, &length);
			fprintf(d->outputfile,
				"[0x%011llx,0x%011llx] %5d %dK pages on %s\n",
				(long long)vmin,
				(long long)last_node.vaddr + last_node.page_size,
				count, last_node.page_size >> 10,
				c ? devname:"/hw/mem");
			if (d->print_physical || d->print_color) {
				for (va = (unsigned long long) vmin; 
				     va < (unsigned long long) last_node.vaddr + last_node.page_size;
				     va += last_node.page_size) {
					print_page_info((void *) va, last_node.page_size);
				}
			}
			vmin = node.vaddr;
			count = 0;
		}
    		end = node.vaddr + node.page_size;
		last_node = node;
		count++;
		size = (long long) stop - (long long) end;
	}

	length = sizeof (devname);
	c = dev_to_devname(last_node.node_dev, devname, &length);
	fprintf(d->outputfile, "[0x%011llx,0x%011llx] %5d %dK pages on %s\n",
		(long long) vmin,
		(long long) last_node.vaddr + last_node.page_size,
		count, last_node.page_size >> 10,
		c ? devname : "/hw/mem"); 
	if (d->print_physical || d->print_color) {
		for (va = (unsigned long long) vmin; 
		     va < (unsigned long long) last_node.vaddr + last_node.page_size;
		     va +=  last_node.page_size) {
			print_page_info((void *) va, last_node.page_size);
		}
	}
}

/* show_all
 *
 * Show address mapping summary for all segments attached to a process
 */
static void
show_all(void) {
	fprintf(d->outputfile, "all memory:\n");
	memory_summary(0,0);
}

/* show_text
 *
 * Show address mappings for the text segment
 */
static void
show_text(void) {
	void	*start;
	size_t	size;
	size_t	pgsz = getpagesize();

	fprintf(d->outputfile, "text:\n");
	start = (void*) _ftext;
	size = (long long) _etext - (long long) start;
	size = pgsz * ((size+pgsz-1)/pgsz);
	memory_summary(start, size);
}

/* show_mapped
 *
 * Show address mappings for memory mapped segments
 */  
static void
show_mapped(void) {
	void *start;
	size_t size;
	size_t pgsz = getpagesize();
	struct rlimit rlp;

	fprintf(d->outputfile, "mapped:\n");
	start = (void*) 0;
	size = (long long) _ftext - (long long) start;
	size = pgsz * ((size)/pgsz);
	memory_summary(start, size);
  
	if (getrlimit(RLIMIT_STACK, &rlp) < 0) {
		perror("getrlimit(RLIMIT_STACK, &rlp) failed\n");
		exit(1);
	}

	start = (void*) (pgsz * (((size_t) sbrk(0)+pgsz-1)/pgsz));
	size = TOPOFSTACK - rlp.rlim_cur - (size_t) start;
	size = pgsz * ((size+pgsz-1)/pgsz);
	memory_summary(start, size);
}
  
/* show_data
 *
 * Show address mappings for data segments
 */
static void
show_data(void) {
	void *start;
	size_t size;
	int pgsz = getpagesize();

	fprintf(d->outputfile, "data/heap:\n");
	start = (void*) _fdata;
	size = (long long) sbrk(0) - (long long) start;
	size = pgsz * ((size+pgsz-1)/pgsz);
	memory_summary(start, size);
}

/* show_stack
 *
 * Show address mappings for stack segments
 */
static void
show_stack(void) {
	void   *start;
	size_t size;
	struct rlimit rlp;

	if (getrlimit(RLIMIT_STACK, &rlp) < 0) {
		perror("getrlimit(RLIMIT_STACK, &rlp) failed\n");
		exit(1);
	}
	start = (void*) (TOPOFSTACK - rlp.rlim_cur);
	size = rlp.rlim_cur;
	fprintf(d->outputfile, "stack:\n");
	memory_summary(start, size);
}

/* get_cpu_path
 *
 *
 */
int
get_cpu_path(int cpu , char *path) {
	int i,j,k;
	int rc;
	invent_cpuinfo_t cpu_invent;
	int len = sizeof(cpu_invent);
	struct stat sinfo;

	for (i=1;i<=40;i++)
		for (j=1;j<=4;j++)
			for (k=0;k<2;k++) {
				sprintf(path,"/hw/module/%d/slot/n%d/node/cpu/%c",i,j,"ab"[k]);
				if(!stat(path, &sinfo)) {
					rc = attr_get(path, INFO_LBL_DETAIL_INVENT,
								  (char *)&cpu_invent, &len, 0);
					if(rc == 0 && cpu == cpu_invent.ic_cpuid) {
						return 0;
					}
				}
			}

	for (i=1;i<=2;i++)
		for (k=0;k<2;k++) {
			sprintf(path,"/hw/module/%d/slot/MotherBoard/node/cpu/%c",i,"ab"[k]);
			if (!stat(path, &sinfo)) {
				rc = attr_get(path, INFO_LBL_DETAIL_INVENT, 
					      (char *)&cpu_invent, &len, 0);
				if(rc == 0 && cpu == cpu_invent.ic_cpuid) {
					return 0;
				}
			}
		}

	sprintf(path,"/hw/cpunum/%d",cpu);
	return 0;
}

/* show_cpuinfo
 *
 *
 */
void
show_cpuinfo(void) {
	int fd;
	char pfile[64];
	char hwpath[128];
	prpsinfo_t pi;
	int pid = getpid();
	struct tms tms;
	clock_t t;

	sprintf(pfile, "/proc/pinfo/%05d", pid);
	if ((fd = open(pfile, O_RDONLY)) < 0) {
		fprintf(d->outputfile,"Can't open '%s'\n",pfile);
		exit(1);
	}
	if (ioctl(fd, PIOCPSINFO, &pi) < 0) {
		fprintf(d->outputfile,"ioctl failed\n");
		exit(1);
	}
	close(fd);
	if (get_cpu_path(pi.pr_sonproc, hwpath ) < 0) {
		fprintf(d->outputfile,"get_cpu_path failed\n");
		exit(1);
	}
	t = times(&tms);
	fprintf(d->outputfile,
		"\"%s\" is process %d and at ~%g second is running on:\n\tcpu %d or %s .\n", 
		pi.pr_fname, pid, (double) (t-d->t0) / (double) CLK_TCK ,
		pi.pr_sonproc, hwpath);
}

/* showit
 *
 * Main function to show address mappings
 */
static void
showit(void) {
	sigset(SIGPROF,SIG_IGN);
	flockfile(d->outputfile);
	fprintf(d->outputfile,"________________________________________________________________\n");
	fprintf(d->outputfile,"Exit  : ");
	show_cpuinfo();
	if (getenv("__DLOOK_ALL_")) {
		show_all();
	} else {
		if (getenv("__DLOOK_MAPPED_")) {
			show_mapped();
		}
		if (getenv("__DLOOK_TEXT_")) {
			show_text();
		}
		show_data();
		show_stack();
	}
	fprintf(d->outputfile,"________________________________________________________________\n");
	fflush(d->outputfile);
	funlockfile(d->outputfile);
}
 
extern void __ateachexit(void(*)(void));

/* __init_dlook
 *
 * Initialize the library for this process
 */
void __init_dlook(void) {
	do_memory();
	do_args();
	do_waysize();
	init_lock(&d->is_busy);
	__ateachexit(showit);
	do_trigger();
}

/* dlook_handler
 *
 * Main function to show address mappings
 */
static void 
dlook_handler(int sig, int code , sigcontext_t *sc) {
	sigset(SIGPROF,SIG_IGN);
	flockfile(d->outputfile);
	fprintf(d->outputfile,"________________________________________________________________\n");

	fprintf(d->outputfile,"Sample: ");
	show_cpuinfo();
	if (getenv("__DLOOK_ALL_")) {
		show_all();
	} else {
		if(getenv("__DLOOK_MAPPED_")) {
			show_mapped();
		}
		if(getenv("__DLOOK_TEXT_")) {
			show_text();
		}
		show_data();
		show_stack();
	}
	fprintf(d->outputfile,"________________________________________________________________\n");
	funlockfile(d->outputfile);
	sigset(SIGPROF,dlook_handler);
}
