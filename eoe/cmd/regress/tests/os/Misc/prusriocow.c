
/*
 * Program to test shared-memory with COW on a
 * region attached with prusrio() [ /proc interface ] 
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/schedctl.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * My data segment begins at address _fdata
 * Partner's data segment is mapped into my address space
 *      at address (_fdata + partner_datap)
 * The macro XADDR(va) yields the address (relative to my address space)
 *      of partner's variable va
 */

/*
 * Two types of data will be allocated in exec image:
 *	1. unitialized data (BSS) - udata.
 *	2. Initialized data - idata
 * Both types will be bracketed by buffers of 128 K long, to ensure
 * that [u,i]data will be accessed by a separate page in a controlled manner.
 *
 * Mmaped region will be dymanically 'allocated' at 'SADDR'.
 */
  
extern char 		_fdata[];
char 			ubuffer[128 * 1024];
char 			ibuffer[128 * 1024] = { "This is buffer1" };
#define SADDR		0x40000

void 	mmapit();
void 	dotest(unsigned long *datap, int sprocopts);
void 	parent(int cpid, volatile unsigned long *datap);
void 	child(void *);
int 	openproc(pid_t pid);
void    debugger(unsigned long *gp, unsigned long *dp);

typedef void (*PVF)();

typedef struct {
	char 		id[256];
	PVF		pretestfunc;
	int		sprocopts;
	unsigned long  	*datap;
	unsigned long  	*basep;
} TestInfo;


/*
 * List of tests, terminated by null id field.
 */
TestInfo ti[] = {
	{"Unitialized Data (BSS)", (PVF) NULL, 	0,
		(unsigned long *) &ubuffer[64*1024], (unsigned long *) _fdata},

	{"Initialized Data", 	(PVF) NULL, 	PR_SADDR,
		(unsigned long *) &ibuffer[64*1024], (unsigned long *) _fdata},

	{"Mmaped Region", 	(PVF) mmapit, 	0,
		(unsigned long *) SADDR,   (unsigned long *) SADDR},

	{"", 			(PVF) NULL, 	0, 
		0,     0 }
};



static ptrdiff_t  	partner_datap;
static unsigned long   	*gbase;
static int		verbose = 0;

#define XADDR(va) (((char *)va) + partner_datap)

#define Vprintf		if (verbose) printf
#define PrintLine \
Vprintf("-------------------------------------------------------------------\n")



static
void
die(char * format, ...)
{
	va_list ap;
	char string[1000];

	strcpy(string, sys_errlist[errno]);
	strcat(string, ": ");
	va_start(ap, format);
	strcat(string, format);
	strcat(string, "\n");
	vfprintf(stderr, string, ap);
	va_end(ap);

	exit(1);
}

void
mmapit()
{
	int fd;

	if ((fd = open("./mmap.file", O_RDWR | O_CREAT, 0644)) < 0)
		die("mmap.file open");
	if (mmap((void *) SADDR, 0x8000, PROT_READ|PROT_WRITE,
		MAP_PRIVATE|MAP_FIXED|MAP_AUTOGROW,
			fd, 0) == (void *) -1)
		die("mmap");
	Vprintf("Done mmap\n");
}


/*
 * Fork and exec; we don't want any state from the previous tests
 * to affect this one ...
 */
int
forkandexec(char *cmd, TestInfo *tip)
{
	pid_t 	cpid;
	int 	winfo;

	Vprintf("Fork and exec: %s\n", tip->id);
	if ((cpid = fork()) == (pid_t) -1)
		die("Failed Fork: %s\n", tip->id);

	if (cpid != 0) {
		/* parent */

		if (waitpid(cpid, &winfo, 0) != cpid)
			die("Wait for child %d", cpid);

		Vprintf("P: Wait returned 0x%x\n", winfo);
		
		if (WIFEXITED(winfo) == 0)
			die("Child %d did not exit gracefully (wstat = 0x%x)",
				cpid, winfo);
		return(WEXITSTATUS(winfo));
	} else {
		char stip[32];

		sprintf(stip, "%d", tip);

		if (execl(cmd, cmd, stip, NULL) == -1)
			die("Failed exec: %s\n", tip->id);
		/* NOT REACHED */
	}
}



/*
 * Main program:
 */
main(int argc, char **argv)
{
	if (argc == 1) {
		TestInfo 	*tip;
		int		nfailed = 0;
		int		npassed = 0;

		for (tip = ti; tip->id[0] != NULL; tip++) {
			PrintLine;
			Vprintf("Test: %s\n", tip->id);
			if (forkandexec(argv[0], tip) == 0) {
				Vprintf("PASSED: %s\n", tip->id);
				npassed++;
			}
			else {
				Vprintf("FAILED: %s\n", tip->id);
				nfailed++;
			}
			PrintLine;
		}
		printf("%s: %d tests passed; %d failed\n",
			argv[0], npassed, nfailed);
		exit(nfailed);
	} else if (argc == 2) {
		TestInfo   	*tip;
		unsigned long	*datap;

		Vprintf("Called with 2 args (%s %s)\n", argv[0], argv[1]);
		tip = (TestInfo *) atoi(argv[1]);

		if (tip->pretestfunc != NULL)
			tip->pretestfunc();

		gbase = tip->basep;
		Vprintf("Dotest: %s datap=0x%x basep=0x%x sprocopts=0x%x\n", 
			tip->id, tip->datap, tip->basep, tip->sprocopts);
		dotest(tip->datap, tip->sprocopts);
	} else if (argc == 3) {
		unsigned long  *datap, *gbasep;

		datap = (unsigned long *) atoi(argv[1]);
		gbasep = (unsigned long *) atoi(argv[2]);
		debugger(datap, gbasep);
	} else {
		int i;

		for (i = 0; i < argc; i++)
			printf("%s ", argv[i]);
		printf("\n");
		die("Unknown no. of arguments %d\n", argc);
	}
}



/*
 *      create two processes executing run(0) and run(1)
 */
void
dotest(unsigned long *datap, int sprocopts)
{
	pid_t 		cpid;
	unsigned long 	junk;

	Vprintf("G: data @ 0x%x\n", datap);

	junk = *datap; /* junk is used as a tmp var to read from datap */
	Vprintf("G: read data = 0x%x\n", junk);

        cpid = fork();
	if (cpid == -1) {
                perror("dotest:fork");
                exit(1);
        }
	if (cpid)
		parent(cpid, datap);
	else 
		child(datap);
}

#define TESTDATA	0x783

void
parent(int cpid, volatile unsigned long *datap)
{
	int 		winfo;
	unsigned long	junk;

        if (sysmp(MP_MUSTRUN, 0) == -1)
                perror("sysmp(MP_MUSTRUN)");
	/*
	 * Block self until child starts, does an attach and reads data.
	 */
	blockproc(getpid());

	/*
	 * Prepare to write new data.
	 */
	junk = *datap;
	Vprintf("P: read data = 0x%x\n", *datap); 

	*datap = TESTDATA;
	Vprintf("P: write data = 0x%x\n", *datap);
	/*
	 * Wake up child, so it can examine new data.
	 */
	unblockproc(cpid);

	if (waitpid(cpid, &winfo, 0) != cpid)
		die("Wait for child %d", cpid);

	Vprintf("P: Wait returned 0x%x\n", winfo);
	
	if (WIFEXITED(winfo) == 0)
		die("Child %d did not exit gracefully (wstat = 0x%x)", cpid, winfo);
	exit(WEXITSTATUS(winfo));
}



/*
 *      exec the 'debugger' thread.
 */
void
child(void *dp)
{
	char sdatap[32], sgbasep[32];
	char *cmd = "./prusriocow";

        /* force processes onto different processors */
        if (sysmp(MP_MUSTRUN, 1) == -1)
                perror("sysmp(MP_MUSTRUN)");

	sprintf(sdatap, "%d", dp);
	sprintf(sgbasep, "%d", gbase);

	if (execl(cmd, cmd, sdatap, sgbasep, NULL) == -1)
		die("Child: exec for debugger\n");
	/* NOT REACHED */
}


void
debugger(unsigned long *dp, unsigned long *gp)
{
        register 		victim, fd;
	volatile unsigned long	junk;

	victim = getppid();
	if ((fd = openproc(victim)) == -1)
		die("Openproc %d", victim);

	if (lseek(fd, (off_t) dp, SEEK_SET) < 0)
		die("lseek /proc/%d, to 0x%x", victim, dp);

	if (read(fd, &junk, sizeof(junk)) != sizeof(junk))
		die("read /proc/%d", victim);

	Vprintf("D: Read Xdata = 0x%x\n", junk);

	/*
	 * signal parent() that attach and a read has been performed
	 */
	unblockproc(victim);

	/*
	 * Block self, and Wait for parent() to write new data.
	 */
	blockproc(getpid());

	/*
	 * Examine parent's write ...
	 */
	if (lseek(fd, (off_t) dp, SEEK_SET) < 0)
		die("lseek /proc/%d, to 0x%x", victim, dp);
	if (read(fd, &junk, sizeof(junk)) != sizeof(junk))
		die("read /proc/%d", victim);

	Vprintf("D: Read Xdata = 0x%x\n", junk);
	if (junk == TESTDATA) {
		Vprintf("D: Attached region is memory-consistent\n");
		exit(0);
	}
	else {
		printf("D: ERRROR:Attached region is NOT memory-consistent\n");
		exit(-1);
	}
}

int
openproc(pid_t pid)
{
	char pname[32];
	int pfd;

	sprintf(pname, "/debug/%05d", pid);
	if ((pfd = open(pname, O_RDWR)) < 0) {
		fprintf(stderr, "Cannot open %s\n", pname);
		return(-1);
	}
	Vprintf("Opened /proc file %s\n", pname);
	return(pfd);
}
