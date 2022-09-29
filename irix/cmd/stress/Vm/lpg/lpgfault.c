#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include "sys/syssgi.h"
#include "sys/sysmp.h"
#include "sys/pmo.h"
#include "sys/procfs.h"
#include <values.h>
#include <sys/prctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define	NBPP		16384
#define	PAGE_SIZE	65536
#define	NUMPAGES	120
#define	BUFSIZE		NUMPAGES*NBPP
#define	NUMITER		2
#define	MAXCPUS		4
#define	MAXITER		100

#define	FAULT	0
#define	FORK 	1
#define	MUNMAP	2
#define	MSYNC	3
#define	SPROC	4
#define	IO	5
#define	SHM	6
#define STACK	7
#define	MAP_LOCAL_TST	8
#define	MAPFILE	9
#define	INVALID_OP	-1

#define	PAGE_ALLOC_FREE	3
#define	NUMSPROCS	5
#define	DMADEVICE	"/dev/rswap"

char	buf[BUFSIZE+PAGE_SIZE];
volatile char	*mapaddr, *bssaddr;
int	numiter;
size_t	page_size;
size_t	bufsize;
int	randfault;

int	set_page_size(int, volatile char *, int);
int	set_stack_pagesize(int , volatile char *);
int 	ctlb_on(int);
void 	get_utlbmiss_count(int, int);
void 	alloc_addr(volatile char **, size_t, volatile char **, size_t);
void	usage(void);

void 	do_io(volatile char *, int);
void 	do_shmtst(int);
void 	do_stacktest(int);
void	do_fault(volatile char *addr, int len, int numiter);
void	do_sproc_fault();
void	do_map_local(int);
void	do_mapfile(char *, int);
void	do_sproc_maplocal();

char	*progname;

struct cmd {
	char	*str;
	int	cmd_op;
} cmd_list[] = {
	"fault", FAULT,
	"fork", FORK,
	"mmap", MUNMAP,
	"msync", MSYNC,
	"sproc", SPROC,
	"IO", IO,
	"shm", SHM,
	"stack", STACK,
	"map_local", MAP_LOCAL_TST,
	"mapfile", MAPFILE,
	0, 0
};

policy_set_t policy = {
	"PlacementDefault", (void *)1,
	"FallbackDefault", NULL,
	"ReplicationDefault", NULL,
	"MigrationDefault", NULL,	
	"PagingDefault", NULL,
	PAGE_SIZE
};

void
usage()
{
	fprintf(stderr, "Usage %s -o <cmd> -i <numiter> -c\n",progname);
	fprintf(stderr, "Where cmd can be\n");
	fprintf(stderr, "fork : fork tests\n");
	fprintf(stderr, "fault : fault test\n");
	fprintf(stderr, "mmap : mmap/munmap test\n");
	fprintf(stderr, "sproc : sproc test\n");
	fprintf(stderr, "msync : msync test\n");
	fprintf(stderr, "IO : IO test\n");
	fprintf(stderr, "stack : Stack test\n");
	fprintf(stderr, "shm : SHM test\n");
	fprintf(stderr, "map_local : map local test\n");
	fprintf(stderr, "mapfile : mmap file test\n");
	fprintf(stderr,"-c enables counting tlbmisses\n");
	exit(1);
}

int
get_cmd_op(char *str)
{
	struct cmd *p = cmd_list;
	
	while (p->str) {
		if (!strcmp(p->str, str))
			return p->cmd_op;
		p++;
	}
	fprintf(stderr,"Invalid cmd option\n");
	usage();
	/* NOTREACHED */
}

main(int argc, char **argv)
{

	int	op;
	int	c;
	extern	char 	*optarg;
	int	numsprocs;
	int	procfd;
	int	enable_counting = 0;
	char	*filename;

	op = FAULT;
	numiter = NUMITER;
	progname = argv[0];
	page_size = PAGE_SIZE;
	bufsize = BUFSIZE;
	filename = NULL;

	while ((c = getopt(argc, argv, "b:f:p:o:i:cr")) != EOF) {
		switch (c) {
		case 'o' :
			op = get_cmd_op(optarg);
			break;

		case 'i' :
			numiter = atoi(optarg);
			break;

		case 'c' :
			enable_counting++;
			break;

		case 'p':
			page_size = (size_t)strtol(optarg, NULL, 0);
			break;

		case 'b':
			bufsize = (size_t)strtol(optarg, NULL, 0);
			if (bufsize < BUFSIZE) {
				printf("bufsize too small, min %d\n",
				       BUFSIZE);
				exit(1);
			}
			break;

		case 'r':
			randfault = 1;
			break;

		case 'f':
			filename = optarg;
			break;

		default :
			usage();
			break;
		}
	}

	if ( numiter < 0) numiter = MAXINT;

	printf("Executing %s test for %d iterations\n",cmd_list[op].str, 
					numiter);
	if (enable_counting) {
		printf("Tlbmiss counting turned on\n");
		procfd = ctlb_on(getpid());
	}

	switch (op) {

	case	FAULT :
			printf("Fault of large page Numiter %x\n", numiter);
			alloc_addr(&bssaddr, BUFSIZE, &mapaddr, bufsize);
			do_fault(bssaddr, BUFSIZE, numiter);
			do_fault(mapaddr, bufsize, numiter);
			break;
	case 	MUNMAP :
			alloc_addr(&bssaddr, BUFSIZE, &mapaddr, bufsize);
			do_fault(mapaddr, bufsize, numiter);
			munmap((void *)(mapaddr + 15*NBPP), 23*NBPP);
			do_fault(mapaddr, 15*NBPP, numiter);
			do_fault(mapaddr + 38*NBPP, (40 - 38)*NBPP, numiter);
			break;
	case	FORK :
			alloc_addr(&bssaddr, BUFSIZE, &mapaddr, bufsize);
			do_fault(mapaddr, bufsize, numiter);
			do_fault(bssaddr, BUFSIZE, numiter);
			if ( fork() == 0) {
				if( set_page_size(NBPP, mapaddr, 
							bufsize) == -1) {
					exit(1);
				}
				do_fault(mapaddr, bufsize, numiter);
				do_fault(bssaddr, BUFSIZE, numiter);
				exit(0);
			} else wait(0);
			break;
	case	SPROC :
			numsprocs = NUMSPROCS;
			alloc_addr(&bssaddr, BUFSIZE, &mapaddr, bufsize);
			while (numsprocs--) {
				if (sproc(do_sproc_fault, PR_SADDR) == -1) {
					perror("sproc");
					exit(1);
				}
			}
			do_sproc_fault();
			wait(0);
			break;

	case	IO :
			alloc_addr(&bssaddr, BUFSIZE, &mapaddr, bufsize);
			do_io(bssaddr, BUFSIZE);
			break;

	case	SHM:
			do_shmtst(numiter);
			break;

	case	STACK :
			do_stacktest(numiter);
			break;

	case	MAP_LOCAL_TST:
			do_map_local(numiter);
			break;
	case	MAPFILE:
			do_mapfile(filename, numiter);
			break;
	default:
			break;
	}
	if (enable_counting)
		get_utlbmiss_count(procfd, getpid());
}


int
set_page_size(int size, volatile char *vaddr, int len)
{
	pmo_handle_t	pm;

	policy.page_size = size;
	pm = pm_create( &policy);

	if ( pm < 0) {
		perror("pm_create");
		return -1;
	}
	if (pm_attach(pm, (void *)vaddr, len) < 0) {
		perror("pm_attach");
		return -1;
	}
	return 0;
}

int
set_stack_pagesize(int size, volatile char *vaddr)
{
	pmo_handle_t	pm;
	pmo_handle_list_t pm_list;

	pm_list.handles = &pm;
	pm_list.length = 1;

	if ( pm_getall((void *)vaddr, NBPP, &pm_list) == -1) {
		perror("pm_getall");
		exit(1);
	}

	if (pm_setpagesize(pm, size) == -1) {
		perror("pm_setpagesize");
		exit(1);
	}
	return 0;
}

void
alloc_addr(volatile char **bssaddrp, size_t bsssize,
	   volatile char **mapaddrp, size_t mapsize)
{
	int fd = open("/dev/zero",O_RDWR);	
	volatile char	*bssaddr, *mapaddr;

	if ( fd == -1) {
		perror("open /dev/zero");
		exit(1);
	}

	mapaddr = mmap(0,mapsize+NBPP,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
	if ( mapaddr == (volatile char *)-1) {
		perror("mmap");
		exit(1);
	}

	printf("Mmap succeeded addr %x\n",mapaddr);
	mapaddr = (char *)((__psunsigned_t)mapaddr &~(NBPP-1));
	if( set_page_size(page_size, mapaddr, mapsize) == -1) {
		exit(1);
	}

	bssaddr = (char *)(((__psunsigned_t)buf + PAGE_SIZE - 1) 
					&~(PAGE_SIZE-1));
	if( set_page_size(PAGE_SIZE, bssaddr, bsssize) == -1) {
		exit(1);
	}

	printf("Bss addr %x\n",bssaddr);
	*bssaddrp = bssaddr;
	*mapaddrp = mapaddr;
	close(fd);
}




int
ctlb_on(int pid)
{
	int	cmd;
	int fd;
	prusage_t pru;
	char	procfile[20];

	sprintf(procfile,"/proc/%05d",pid);
	fd = open(procfile, O_RDWR);
	if ( fd == -1) {
		perror(procfile);
		exit(1);
	}
	cmd = TLB_COUNT;
	if ( ioctl(fd,PIOCTLBMISS,&cmd)  == -1) {
		perror("ioctl:PIOCTLBMISS:TLB_COUNT");
		exit(1);
	}
	return(fd);
}

void
get_utlbmiss_count(int fd,int pid)
{
	prusage_t pru;

	pru.pu_utlb = 0;

	if ( ioctl(fd,PIOCUSAGE,&pru)  == 0) {
		printf("Tlbmiss count 0x%x\n",pru.pu_utlb);
	}
	else perror("get_utlbmiss_count");
}

void
do_fault(volatile char *addr, int len, int numiter)
{
	volatile char	*eaddr;
	volatile __psint_t	*x;
	__psint_t	tmp;
	int	i;

	len = len & (~(NBPP - 1));
	eaddr = addr + len;
	for (i = 0; i < numiter; i++) {
		addr = eaddr - len;
		for (;addr < eaddr; addr += NBPP) {
			x = (__psint_t *)addr; /* Force a vfault and a pfault */
			tmp = *x;
			if ( i == 0) {
				*x = (__psint_t)addr;
			}
			tmp = *x;
			if  (tmp != (__psint_t)addr) {
				fprintf(stderr,"Fatal error:Data does not match addr %x x %x iter %d\n", addr, tmp, i);
				/*
				if (syssgi(SGI_DEBUGLPAGE, PAGE_ALLOC_FREE,
					tmp, addr) == -1)
					perror("SGI_DEBUGLPAGE");
				 */
				exit();
			}
		}
		if ((numiter <= 20) || ((i) && ((i %1000000) == 0)))
			printf("Iteration %d x %x %x\n",i,x,*(x));
	}
}


void
do_random_fault(volatile char *addr, int len, int numiter)
{
	volatile char	*eaddr;
	volatile __psint_t	*x;
	__psint_t	tmp;
	int		i, j;
	int		numpages;
	volatile char	*touchaddr;
	char		*touched;
	int		pgno;

	len = len & (~(NBPP - 1));
	eaddr = addr + len;
	numpages = len / NBPP;

	touched = (char *)malloc(numpages);
	bzero(touched, numpages);
	for (i = 0; i < numiter; i++) {
		for (j = 0; j < numpages; j++) {
			pgno = random() % numpages;
			touchaddr = addr + (pgno * NBPP);
			x = (__psint_t *)touchaddr; /* Force a vfault and a pfault */
			tmp = *x;
			if (!touched[pgno]) {
				*x = (__psint_t)touchaddr;
				touched[pgno] = 1;
			}
			tmp = *x;
			if (tmp != (__psint_t)touchaddr) {
				fprintf(stderr,"Fatal error:Data does not match addr %x x %x iter %d\n", touchaddr, tmp, i);
				/*
				if (syssgi(SGI_DEBUGLPAGE, PAGE_ALLOC_FREE,
					*x,addr) == -1)
					perror("SGI_DEBUGLPAGE");
				*/
				exit();
			}
		}
		if ((numiter <= 20) || ((i) && ((i %1000000) == 0)))
			printf("Iteration %d x %x %x\n",i,x,*(x));
	}
}

void 
do_sproc_fault()
{
	int	i;
	int	pass = 0;
	printf("sproc test pid %d\n",getpid());
	for (pass = 0; pass < numiter; pass++) {
		do_fault(mapaddr, bufsize, MAXITER);
		do_fault(bssaddr, BUFSIZE, MAXITER);
		if ((pass % 1000000) == 0)
			printf("sproc test pid %d %d pass completed\n",getpid(), pass);
	}
	exit(0);
}

void
do_io(volatile char *addr, int size)
{
	int fd = open(DMADEVICE, O_RDONLY);
	volatile char	*eaddr;
	int fd1 = open("./junk", O_RDWR|O_CREAT, 0666);
	

	if ( fd == -1) {
		perror("open dmadevice");
		exit(1);
	}

	if (fd1 == -1) {
		perror("Cannot open junk");
		exit(1);
	}

	printf("Starting io from %s to addr 0x%x\n",DMADEVICE, addr);
	eaddr = addr + size;
	for (; addr < eaddr; addr += NBPP) {
		if (read(fd, addr, NBPP) == -1) {
			perror("read");
			exit(1);
		}
		if (write(fd1, addr, NBPP) == -1) {
			perror("write");
			exit(1);
		}
	}
	printf("Read completed\n");
	close(fd);
	close(fd1);
}

void
do_shmtst(int numiter)
{
	int	id;
	char	*shmaddr;
	
	id = shmget(0xbacd, bufsize, IPC_CREAT|0666);

	if (id == -1) {
		perror("shmget");
		exit(1);
	}

	if (fork() == 0) {
		id = shmget(0xbacd, bufsize, 0);

		if (id == -1) {
			perror("shmget");
			exit(1);
		}

		sleep(60);
		shmaddr = shmat(id, 0, 0);

		if (shmaddr == (char *)-1L) {
			perror("shmat");
			exit(1);
		}

		shmaddr = (char *)(((__psint_t)shmaddr + page_size -1 ) &~(page_size-1));

		printf("Child shmaddr %x pid %d\n",shmaddr, getpid());
		do_fault(shmaddr, bufsize, numiter);
		exit(0);
	}

	shmaddr = shmat(id, 0, 0);

	if (shmaddr == (char *)-1L) {
		perror("shmat");
		exit(1);
	}

	shmaddr = (char *)(((__psint_t)shmaddr + page_size -1 ) &~(page_size-1));

	if(set_page_size(page_size, shmaddr, bufsize) == -1) {
		exit(1);
	}

	printf("Parent shmaddr %x pid %x\n", shmaddr, getpid());
	do_fault(shmaddr, bufsize, numiter);
	wait(0);
	
}

void
do_stacktest(int numiter)
{
	int	i;
	char stkbuf[BUFSIZE+PAGE_SIZE];
	char	*stkaddr = stkbuf;


	stkaddr = (char *)(((__psint_t)stkaddr + PAGE_SIZE -1 ) &~(PAGE_SIZE-1));

	if( set_stack_pagesize(PAGE_SIZE, stkaddr) == -1) {
		exit(1);
	}

	printf("Running Stack tests\n");
	printf("Stkaddr %x pid %x\n", stkaddr, getpid());
	do_fault(stkaddr, BUFSIZE, numiter);
}

#define	SHARED_ADDR	(0x4000000)
#define	LOCAL_ADDR	(SHARED_ADDR + 2*page_size)
#define	LOCAL_BUFSIZE	(10*page_size)


void
do_map_local(int numiter)
{
	int fd = open("/dev/zero",O_RDWR);	
	volatile char	*sharedaddr;

	if ( fd == -1) {
		perror("open /dev/zero");
		exit(1);
	}

	sharedaddr = mmap((void *)SHARED_ADDR, bufsize, PROT_READ|PROT_WRITE, 
						MAP_SHARED|MAP_FIXED ,fd,0);
	if ( sharedaddr == (volatile char *)-1) {
		perror("mmap");
		exit(1);
	}

	printf("Shared mmap succeeded addr %x\n",sharedaddr);
	sharedaddr = (char *)((__psint_t)sharedaddr &~(NBPP-1));
	if( set_page_size(page_size, sharedaddr, bufsize) == -1) {
		exit(1);
	}

	close(fd);

	if (sproc(do_sproc_maplocal, PR_SADDR) == -1) {
		perror("sproc");
		exit(1);
	}

	printf("%d running shared address faults\n", getpid());

	do_fault(sharedaddr, bufsize, numiter);
}

void
do_sproc_maplocal()
{
	int fd = open("/dev/zero",O_RDWR);	
	volatile char	*localaddr;

	if ( fd == -1) {
		perror("open /dev/zero");
		exit(1);
	}

	localaddr = mmap((void *)LOCAL_ADDR, LOCAL_BUFSIZE,PROT_READ|PROT_WRITE,
				 MAP_PRIVATE|MAP_FIXED|MAP_LOCAL,fd,0);

	if (localaddr == (volatile char *)-1) {
		perror("mmap");
		exit(1);
	}

	printf("Local mmap succeeded addr %x\n",localaddr);
	localaddr = (char *)((__psint_t)localaddr &~(NBPP-1));
	if( set_page_size(page_size, localaddr, LOCAL_BUFSIZE) == -1) {
		exit(1);
	}
	close(fd);
	printf("%d running local address faults\n", getpid());
	do_fault((volatile char *)SHARED_ADDR, bufsize, numiter);
}


void
do_mapfile(char *filename, int numiter)
{
	int		fd;
	int		error;
	char		*addr;

	if (filename == NULL) {
		filename = tempnam(NULL, "lpgmmap");
		fd = open(filename, O_CREAT | O_TRUNC | O_RDWR, 0777);
	} else {
		fd = open(filename, O_CREAT | O_RDWR, 0777);
	}
	if (fd == -1) {
		perror("open temp file");
		exit(1);
	}

	error = ftruncate(fd, (off_t)bufsize);
	if (error) {
		perror("ftruncate");
		exit(1);
	}

	addr = mmap((void *)SHARED_ADDR, bufsize, PROT_READ|PROT_WRITE, 
		    MAP_SHARED|MAP_FIXED, fd, 0);
	if (addr == (volatile char *)-1) {
		perror("mmap");
		exit(1);
	}

	printf("Mmap succeeded addr %x\n", addr);
	addr = (char *)((__psint_t)addr & ~(NBPP - 1));
	if (set_page_size(page_size, addr, bufsize) == -1) {
		exit(1);
	}

	close(fd);

	printf("%d running mmap faults\n", getpid());

	if (randfault) {
		do_random_fault(addr, bufsize, numiter);
	} else {
		do_fault(addr, bufsize, numiter);
	}
}
