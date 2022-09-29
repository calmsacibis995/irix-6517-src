/*
 * pgun.c
 * 
 * User level program to dump bad parity into given bytes in memory.
 */
#include <sys/types.h>
#include <sys/parity.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <invent.h>
#include <math.h>
#include <sys/time.h>
#include <stdlib.h>
#include <syslog.h>

/* syssgi call to stuff bad parity into a byte of mem */
#define SGI_PGUN 1020

/* Global Data */
PgunData	pgd;	    /* Command structure for syssgi call. */
int		printing=0; /* Print gobs of info about what we're doing? */

/* Subroutines. */
static void quick_test(void);
static void parse_input(void);
static void parse_args(int, char**);

int pgun_cmds(int, char**);
int pgun_printing(int, char**);
int pgun_phys(int, char**);
int pgun_virt(int, char**);
int pgun_pvirt(int, char**);
int pgun_text(int, char**);
int pgun_bdest(int, char**);
int pgun_print_mem(int, char**);
int pgun_test_load(int, char**);
int pgun_shotgun(int, char**);
int pgun_data_text(int, char**);
int pgun_bd_store(int, char**);
int pgun_fp_store(int, char**);
int pgun_ktext(int, char**);
int pgun_unaligned(int, char**);
int pgun_all(int, char**);
int pgun_unaligned_data(int, char**);
int pgun_unaligned_text(int, char**);

/* Pack 2 values into a return code:
 * Args used, and a real return code.
 */
#define make_ret(rval,args) ((rval)<<16|(args)&0xffff)

char usage[]="pgun [cmd [args]] ...";

char *cmdlist[] = 
{
       "? or -h			  -Print command list",
       "printing {on|off}	  -Turn on (off) verbose printing",
       "phys <addr> <mask>	  -Dump bad parity to physical addr",
       "virt <addr> <mask>	  -   to virtual addr in current process",
       "pvirt <pid> <addr> <mask> -   to virtual addr in another process",
       "text			  -Bad parity to text segment, then execute it",
       "bdest			  -Bad parity in destination of bcopy() call",
       "print_mem		  -Print main memory size",
       "shotgun <num>		  -Put <num> errors randomly in main mem",
       "bd_store <num>		  -Store <num> in branch delay slot",
       "fp_store <val>		  -Store <val> from fp coprocessor",
       "ktext			  -Test kernel text parity error recovery",
       "unaligned		  -Test unaligned transfers w/parity errors",
       "udata                     -Test non-cacheline aligned data w/errors",
       "utext                     -Test non-cacheline aligned text w/errors",
       "all			  -Try all known recoverable errors",
       0
};

/* These functions return number of args they chew up.
 * (The one for the command itself is taken care of in the for loop.)
 */
struct functab {
    char *name;
    int (*func)(int, char**);
} ftab[] = {
    {"?", pgun_cmds},
    {"-h", pgun_cmds},
    {"printing", pgun_printing},
    {"phys", pgun_phys},
    {"virt", pgun_virt},
    {"pvirt", pgun_pvirt},
    {"text", pgun_text},
    {"bdest", pgun_bdest},
    {"print_mem", pgun_print_mem},
    {"test_load", pgun_test_load},
    {"shotgun", pgun_shotgun},
    {"data_text", pgun_data_text},
    {"bd_store", pgun_bd_store},
    {"fp_store", pgun_fp_store},
    {"ktext", pgun_ktext},
    {"udata", pgun_unaligned_data},
    {"utext", pgun_unaligned_text},
/*    {"unaligned", pgun_unaligned},*/
    {"all", pgun_all}
};

main (int argc, char **argv)
{
#ifdef DEBUG
    quick_test();
#else
    if (argc==1) {  /* No args.  Read cmds from stdin */
	parse_input();
    } else {	/* Read cmds from args */
	parse_args(argc, argv);
    }
#endif /* !DEBUG */
}

/* May want to be able to shove commands at this program via stdin
 * eventually.
 */
static void
parse_input (void)
{
    printf ("Usage: %s\n", usage);
    printf ("Input parsing not currenty implemented.");
    printf ("  Use command line args instead.\n");
    printf ("Use \042pgun -h\042 to get list of commands.\n");
    exit (1);
}

/* Read commands from the argument list, and execute them sequentially.
 */
static void
parse_args (int argc, char **argv)
{
    int funcid, nargs, ffound;
    
    for (--argc, ++argv; argc>0; --argc, ++argv) {
	ffound=0;
	for (funcid=0;funcid<sizeof(ftab)/sizeof(struct functab);funcid++) {
	    if (!strncmp(*argv, ftab[funcid].name, strlen(ftab[funcid].name))) {
		ffound=1;
		nargs=(*ftab[funcid].func)(argc, argv);
		argc-=nargs;
		argv+=nargs;
		break;	/* Back to outer for loop. */
	    }
	}
	if (!ffound) {
	    fprintf (stderr, "pgun: unrecognized cmd: %s\n", *argv);
	    exit (1);
	}
    }
}

/* Local random number generator.
 */
static long
lcl_rnd(void)
{
    static int	seeded=0;   /* Have we seeded random number generator? */
    static struct timeval tv; /* If not, use time of day as starter. */
    
    if (!seeded) {
	gettimeofday(&tv);
	srandom (tv.tv_sec);
	seeded=1;
	random();   /* Let's toss one out for fun! */
    }

    return random();
}

int
pgun_cmds (int argc, char **argv)
{
    char **cmd;

    printf ("Usage: %s\n\n", usage);
    printf ("Commands are:\n");
    for (cmd=cmdlist; *cmd; cmd++) {
	printf ("%s\n", *cmd);
    }
    printf ("\n");
    return 0;
}

int
pgun_printing (int argc, char **argv)
{
    if (argc<2) {
	printf ("Need 1 parameter to printing cmd.\n");
	exit (1);
    }
    
    if (!strcasecmp("on", argv[1])) {
	printing = 1;
	return 1;	/* Used 1 args. */
    }
    if (!strcasecmp("off", argv[1])) {
	printing = 0;
	return 1;	/* Used 1 args. */
    }

    /* Bad argument. */
    fprintf (stderr, "pgun printing: bad argument %s.\n", argv[1]);
    exit (1);
}

static int
do_phys (ulong paddr, int mask)
{
    pgd.addr = paddr;
    pgd.bitmask = mask;
    
    errno = 0;
    syssgi (SGI_PGUN, PGUN_PHYS, &pgd);
    return errno;
}

int
pgun_phys (int argc, char **argv)
{
    if (argc<3) {
	printf ("Need 2 parameters to phys cmd.\n");
	exit (1);
    }

    if (do_phys(strtoul(argv[1], NULL, 0), strtol(argv[2], NULL, 0))) {
	perror ("pgun_phys: syssgi failed.");
	exit (1);
    }
    
    return 2;	/* Used 2 args. */
}

static int
do_virt (ulong vaddr, int mask)
{
    pgd.addr = vaddr;
    pgd.bitmask = mask;
    
    errno = 0;
    syssgi (SGI_PGUN, PGUN_VIRT, &pgd);
    return errno;
}

int
pgun_virt (int argc, char **argv)
{
    if (argc<3) {
	printf ("Need 2 parameters to virt cmd.\n");
	exit (1);
    }

    if (do_virt(strtoul(argv[1], NULL, 0), strtol(argv[2], NULL, 0))) {
	perror ("pgun_virt: syssgi failed.");
	exit (1);
    }
    
    return 2;	/* Used 2 args. */
}

static int
do_pvirt (pid_t pid, ulong vaddr, int mask)
{
    pgd.pid = pid;
    pgd.addr = vaddr;
    pgd.bitmask = mask;
    
    errno = 0;
    syssgi (SGI_PGUN, PGUN_PID_VIRT, &pgd);
    return errno;
}

int
pgun_pvirt (int argc, char **argv)
{
    if (argc<4) {
	printf ("Need 3 parameters to pvirt cmd.\n");
	exit (1);
    }
    
    if (do_pvirt (strtol(argv[1], NULL, 0),
		  strtoul(argv[2], NULL, 0),
		  strtol(argv[3], NULL, 0))) {
	perror ("pgun_pvirt: syssgi failed.");
	exit (1);
    }
    
    return 3;	/* Used 3 args. */
}

/* Stupid routine.  Dump a bad byte in the text for it, then call it
 * to test recovery from parity errors in text segment.
 */
static int
small_calc (int input)
{
    return input*2+5;
}

/* Put a parity error in program text, then execute over it.
 * Return values are:
 *   0: Completely successful.  Text executed correctly.
 *  >0: Unable to put parity error in.  errno is set.
 *  <0: Executed text, but got wrong answer.
 */
static int
do_text (void)
{
    int ret_val;
    
    if (ret_val = do_virt((u_long)&small_calc, 8)) {
	return ret_val;
    }
    
    return (small_calc(53) == 53*2+5) ? 0 : -1;
}

int
pgun_text (int argc, char **argv)
{
    register int ret_val;
    
    if ((ret_val=do_text()) > 0) {
	perror ("pgun_text: syssgi failed.");
	exit (1);
    }
    
    if (ret_val<0) {
	fprintf (stderr, "Pgun text: calculation unsuccessful.\n");
	exit (1);
    }
    
    printf ("Pgun text: successful.\n");

    return 0;	/* No args. */
}

/*
 * Dump a parity error into the destination of a bcopy.
 */
int bcopy_dst[1024]; /* For now, don't worry about alignment. */
int bcopy_src[1024];

static int
do_bdest ()
{
    pgd.addr = (ulong)&bcopy_dst[lcl_rnd()%1024];
    pgd.bitmask = (1 << (lcl_rnd()%9)) & 0xff;	/* Flip a random bit */

    if (printing) {
	printf ("bdest: about to write bad bitmask 0x%x to address 0x%x.\n", 
	    pgd.bitmask, pgd.addr);
	fflush (stdout);
    }
    
    syssgi (SGI_PGUN, PGUN_VIRT, &pgd);	/* Now, the bad parity is there. */
    if (errno) {
	return (errno);
    }
    if (printing) {
	printf ("do_bdest: bad parity written.  About to bcopy over it.\n");
	fflush (stdout);
    }
    
    /* Do the copy.  Should hit error, but recover. */
    bcopy (bcopy_src, bcopy_dst, sizeof(bcopy_src));
    if (printing)
	printf ("Pgun: completed bcopy over parity error.\n");

    if (bcmp(bcopy_src, bcopy_dst, sizeof(bcopy_src))) {
	if (printing)
	    printf ("Pgun: do_bdest bcopy copied bad data.\n");
	return -1;
    }

    if (printing)
	printf ("Pgun: do_bdest succeeded.\n");
    return 0; /* Success. */
}

int
pgun_bdest (int argc, char **argv)
{
    int ret_val;
    
    if ((ret_val=do_bdest()) > 0) {
	perror ("pgun_bdest: syssgi failed.");
	exit (1);
    }
    
    if (ret_val < 0) {
	fprintf (stderr, "bdest operation failed with value %d.\n", ret_val);
	exit (1);
    }

    return 0; /* No args. */
}

int
mem_inv (inventory_t *pinvent)
{
    if (pinvent->inv_class == INV_MEMORY &&
	    pinvent->inv_type  == INV_MAIN)
	return pinvent->inv_state;
    else
	return 0;
}
int
get_mem_size(void)
{
    static int memsize = 0;
    
    if (memsize)
	return (memsize);
	
    return memsize=scaninvent( (int(*)()) mem_inv, NULL);
}

int
pgun_print_mem(int argc, char **argv)
{
    printf ("Main memory size is %d megabytes.\n", get_mem_size()/(1024*1024));
    return 0;
}

int
pgun_test_load(int argc, char **argv)
{
    syssgi (SGI_PGUN, PGUN_TEST_LOAD, &pgd);  /* Do load from bad addr. */
    if (errno) {
	perror ("pgun_test_load: syssgi failed.");
	exit (1);
    }
    return 0;
}

int
pgun_shotgun (int argc, char **argv)
{
    unsigned	mem_addr, phys_addr, mask;
    register int i, top;    /* Loop counter variables */
    
    if (argc<2) {
	printf ("Need 1 parameter to shotgun cmd.\n");
	exit (1);
    }
    
    top=strtoul(argv[1], NULL, 0);  /* How many parity errors to shoot in. */
    for (i=0; i<top; i++) {
	mem_addr = lcl_rnd() % get_mem_size();	/* Random address < mem size */
	/* Account for MC's memory addressing */
	phys_addr = mem_addr<(256*1024*1024)	?
		    mem_addr+0x08000000		:
		    mem_addr+0x10000000;
	/* Randomly pick a bit in the byte to kill.  8=>parity bit */
	mask = (1 << (lcl_rnd()%9)) & 0xff;
	printf ("Memaddr is 0x%08x, physaddr is 0x%08x, mask is 0x%02x.\n",
		mem_addr, phys_addr, mask);
    
	pgd.addr = phys_addr;
	pgd.bitmask = mask;
	

	syssgi (SGI_PGUN, PGUN_PHYS, &pgd);
	if (errno) {
	    perror ("pgun_shotgun: syssgi failed.");
	    exit (1);
	}

	/* I hate this message, but it does allow syslog to fit it on one
	 * line, and includes the word "parity" for grepping.
	 */
	syslog(LOG_INFO,
		"pgun: injected parity at 0x%08x mask 0x%02x.",
		phys_addr, mask);
    }
    
    return 1;	/* Used 1 arg. */
}

static char *text_arena = 0;
static char *data_arena = 0;

static void
setup_text_arena(void)
{
    if (text_arena) {
	return;
    }

    /* Allocate 3 pages, so we know we have 2 and can cross a
     * boundary where we want to.
     */
    text_arena = malloc (4096*3);

    if (!text_arena) {
	perror ("unable to allocate text arena.\n");
	exit(1);
    }
}

static void
setup_data_arena(void)
{
    if (data_arena) {
	return;
    }

    /* Allocate 3 pages, so we know we have 2 and can cross a
     * boundary where we want to.
     */
    data_arena = malloc (4096*3);

    if (!data_arena) {
	perror ("unable to allocate data arena.\n");
	exit(1);
    }
}

int
pgun_data_text (int argc, char **argv)
{
    register int input, output;
    extern int data_text(int);

    if (argc<2) {
	printf ("Need 1 parameter to data_text cmd.\n");
	exit (1);
    }
    
    setup_text_arena();
    
    bcopy (data_text, text_arena, 4*8);	/* Whole routine is <8instr */

    input=strtoul(argv[1], NULL, 0);

    output = data_text(input);
    printf ("data_text(%d) returned %d.\n", input, output);
    
    output = (*(int (*)(int))text_arena)(input);
    printf ("bcopied data_text(%d) returned %d.\n", input, output);
    
    return 1;	/* Used 1 arg. */
}

int
do_bd_store (int input)
{
    void *parity_addr;
    extern void bd_store(int, void *, int);
	/* Args are: take branch?, addr to store to in bd slot, value */

    setup_text_arena();
    setup_data_arena();
    
    /* Dump a parity error somewhere randomly in the data arena. */
    parity_addr = &data_arena[lcl_rnd()%4096&~3];  /* Need integer addr */

    pgd.addr = (ulong)parity_addr;
    pgd.bitmask = (1 << (lcl_rnd()%9)) & 0xff;	/* Flip a random bit */

    if (printing) {
	printf ("bd_store: about to write bad bitmask 0x%x to address 0x%x.\n", 
	    pgd.bitmask, pgd.addr);
	fflush (stdout);
    }
    
    syssgi (SGI_PGUN, PGUN_VIRT, &pgd);	/* Now, the bad parity is there. */
    if (errno) {
	return (errno);
    }
    if (printing) {
	printf ("bd_store: bad parity written.  About to store over it.\n");
	fflush (stdout);
    }
    
    if (printing) {
	printf ("Calling bd_store with %d, 0x%x, %d.\n",
		1, parity_addr, input);
	fflush (stdout);
    }

    bd_store(1, parity_addr, input);
    if (printing) {
	printf ("Pgun: bd_store completed.\n");
	fflush (stdout);
    }

    if (*(int *)parity_addr != input) {
	if (printing)
	    printf ("Pgun: do_bd_store stored bad data.\n");
	return -1;
    }
    
    if (printing)
	printf ("bd_store: successful.\n");
    
    return 0;	/* Used 1 arg. */
}


int
pgun_bd_store (int argc, char **argv)
{
    register int input,  ret_val;

    if (argc<2) {
	printf ("Need 1 parameter to bd_store cmd.\n");
	exit (1);
    }
    
    input=strtoul(argv[1], NULL, 0);

    if ((ret_val=do_bd_store(input)) > 0) {
	perror ("pgun_bd_store: syssgi failed.");
	exit (1);
    }
    
    if (ret_val < 0) {
	fprintf (stderr, "bd_store operation failed with value %d.\n", ret_val);
	exit (1);
    }

    return 1;	/* Used 1 arg. */
}

static int
do_fp_store (int val1, int val2)
{
    int *store_area;
    extern void fp_store(int, void *);
	/* Args are: value, addr to store to */
    extern void fp_dstore(int, int, void *);
	/* Args are: value1, value2, addr to store to */

    setup_text_arena();
    setup_data_arena();
    store_area = (int *)data_arena;

    store_area[0] = 0xdeadbeef;
    pgd.addr = (ulong)store_area;
    pgd.bitmask = 0;	/* Flip the parity bit. */
    syssgi (SGI_PGUN, PGUN_VIRT, &pgd);	/* Now, the bad parity is there. */
    if (errno) {
	return errno;
    }

    fp_store(val1, store_area);
    if (store_area[0] != val1) {
	return -1;
    }

    store_area[0] = 0xcafebabe;
    store_area[1] = 0xbaddad;
    pgd.addr = (ulong)store_area;
    pgd.bitmask = 0;	/* Flip the parity bit. */
    syssgi (SGI_PGUN, PGUN_VIRT, &pgd);	/* Now, the bad parity is there. */
    if (errno) {
	return errno;
    }

    fp_dstore(val1, val2, store_area);
    if (store_area[0] != val1) {
	return -2;
    }
    if (store_area[1] != val2) {
	return -3;
    }
    
    return 0;	/* Succeeded!. */
}

int
pgun_fp_store (int argc, char **argv)
{
    int input1, input2, ret_val;

    if (argc<3) {
	printf ("Need 2 parameters to fp_store cmd.\n");
	exit (1);
    }
    
    input1 = strtoul(argv[1], NULL, 0);
    input2 = strtoul(argv[2], NULL, 0);

    printf ("pgun_fp_store: input is %d, %d.\n", input1, input2);

    if ((ret_val=do_fp_store(input1, input2)) > 0) {
	perror ("pgun_fp_store: syssgi failed.");
	exit (1);
    }
    
    if (ret_val < 0) {
	fprintf (stderr, "fp_store operation failed with value %d.\n", ret_val);
	exit (1);
    }

    return 2;	/* Used 2 args. */
}

/* Put a parity error in kernel text, then execute over it. */
static int
do_ktext (void)
{
    errno = 0;
    syssgi (SGI_PGUN, PGUN_KTEXT, &pgd);  /* pgd structure is irrelevant. */
    return errno;
}

int
pgun_ktext (int argc, char **argv)
{
    if (do_ktext()) {
	perror ("pgun_ktext: syssgi failed.");
	exit (1);
    }
    
    printf ("pgun_ktext executed successfully.\n");
    
    return 0;	/* Used 0 args. */
}

/* The kitchen sink!
 */
int
pgun_all (int argc, char **argv)
{
    int ret_val;
    
    printf ("Throwing in the kitchen sink!\n");

    syslog(LOG_INFO, "pgun: testing kernel text recovery.");
    sleep(1);  /* Let's see if we can get above message in. */
    if (ret_val = do_ktext()) {
	syslog(LOG_INFO, "pgun: kernel text recovery failed because %m.");
	perror ("pgun_all ktext syssgi failed because");
	exit (1);
    }
    
    syslog(LOG_INFO, "pgun: testing user text recovery.");
    sleep(1);  /* Let's see if we can get above message in. */
    if (ret_val = do_text()) {
	syslog(LOG_INFO, "pgun: user text recovery failed because %m.");
	perror ("pgun_all text syssgi failed because");
	exit (1);
    }
    
    syslog(LOG_INFO, "pgun: testing stores over parity errors.");
    sleep(1);  /* Let's see if we can get above message in. */
    if ((ret_val = do_bdest ()) > 0) {
	syslog(LOG_INFO, "pgun: store recovery failed because %m.");
	perror ("pgun_all bdest() syssgi failed because");
	exit (1);
    }
    if (ret_val < 0) {
	syslog(LOG_INFO, "pgun: store recovery failed with cause %d.\n", 
		ret_val);
	exit (1);
    }
    
    syslog(LOG_INFO, "pgun: testing floating point stores over parity errors.");
    sleep(1);  /* Let's see if we can get above message in. */
    if ((ret_val = do_fp_store(4332, 5432)) > 0) {
	syslog(LOG_INFO, "pgun: floating point store recovery failed because %m.");
	perror ("pgun_all fp_store syssgi failed because");
	exit (1);
    }
    if (ret_val < 0) {
	syslog(LOG_INFO, "pgun: floating point store recovery failed with cause %d.\n", 
		ret_val);
	exit (1);
    }
    
    syslog(LOG_INFO, "pgun: testing stores in branch delay slot over parity errors.");
    sleep(1);  /* Let's see if we can get above message in. */
    if ((ret_val = do_bd_store(lcl_rnd())) > 0) {
	syslog(LOG_INFO, "pgun: branch delay store recovery failed because %m.");
	perror ("pgun_all bd_store syssgi failed because");
	exit (1);
    }
    if (ret_val < 0) {
	syslog(LOG_INFO, "pgun: branch delay store recovery failed with cause %d.\n", 
		ret_val);
	exit (1);
    }

    printf ("pgun all: tests completed successfully.\n");
    return 0;	/* Used no args. */
}

/* This was used to debug the kernel part of the syssgi call.
 * Don't use it if the real syscall is enabled and you don't want bad
 * things to happen.
 */
static void
quick_test (void)
{
    /* For starters, let's just try the damn call! */
    pgd.addr = 0x08000000;  /* First byte of main memory. */
    pgd.bitmask = 0;
    
    syssgi (SGI_PGUN, PGUN_PHYS, &pgd);
    if (errno) perror ("syssgi failed.");
    
    /* Put bad parity in this process */
    pgd.addr = (ulong)&pgd;
    pgd.bitmask = 0x1;
    printf ("Trying to dump a parity error on first byte of pgd struct 0x%x.\n", 
	pgd.addr);
    fflush (stdout);
    syssgi (SGI_PGUN, PGUN_VIRT, &pgd);
    if (errno) perror ("syssgi failed.");
    
    /* Quick dis of /etc/init shows this in the beginning text segment */
    pgd.addr = 0x4000c0;
    pgd.bitmask = 0x2;
    pgd.pid = 1;
    printf ("Trying to dump a parity error into init process @0x%x.\n", 
	pgd.addr);
    fflush (stdout);
    syssgi (SGI_PGUN, PGUN_PID_VIRT, &pgd);
    if (errno) perror ("syssgi failed.");
}

pgun_unaligned_data(int argc, char **argv)
{
  if (do_unaligned_data()) {
    perror("pgun_unaligned_dat: syssgi failed");
    exit(1);
  }

  printf("unaligned data error corrected successfully\n");
  return(0);
}

static int
do_cache_noee (ulong vaddr, int mask)
{
    pgd.addr = vaddr;
    pgd.bitmask = mask;
    
    errno = 0;
    syssgi (SGI_PGUN, PGUN_CACHE_NOEE, &pgd);
    return errno;
}

do_unaligned_data()
{
  char *mbase;
  char *maddr;
  char foo;
  int ret_val = 0;
  extern touch_cacheline(char *);

  /* align to cache line boundry */
  mbase = (char *)malloc(128);
  if (!mbase) {
	printf("malloc of 128 bytes failed\n");
	exit(1);
  }
  printf("mbase is 0x%x\n", mbase);
  maddr = (char *)(((uint)mbase + 64) & ~0x1f);

  if (printing) 
    printf("pgun unaligned data: writing bad parity to address 0x%x\n", 
	   maddr + 16);

  if (ret_val = do_cache_noee((uint)maddr + 16, 8)) return(ret_val);

  free(mbase);
  return(0);              /* if we get here everything worked */
}

pgun_unaligned_text(int argc, char **argv)
{
  if (do_unaligned_text()) {
    perror("pgun_unaligned_text: syssgi failed");
    exit(1);
  }

  printf("unaligned text error corrected successfully\n");
  return(0);
}
  
do_unaligned_text()
{
  
    pgd.addr = 0;
    pgd.bitmask = 0;
    
    errno = 0;
    syssgi (SGI_PGUN, PGUN_TEXT_NOEE, &pgd);
    return errno;
}
