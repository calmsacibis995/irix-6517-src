#ident	"lib/libsk/lib/saio.c: $Revision: 3.141 $"

/*
 * saio.c -- standalone io system
 * Mimics UNIX io as much as possible.
 */

#include <fault.h>	/* arcs/fault.h */
#include <arcs/types.h>
#include <arcs/restart.h>
#include <arcs/signal.h>
#include <arcs/hinv.h>
#include <arcs/folder.h>
#include <arcs/cfgtree.h>
#include <arcs/eiob.h>
#include <arcs/errno.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <setjmp.h>
#include <ctype.h>
#include <saio.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <saioctl.h>
#include <libsc.h>
#include <libsk.h>
#include <alocks.h>
#include <standcfg.h>
#include <stringlist.h>

#if EVEREST
#include <sys/cpu.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evconfig.h>
#ifndef LARGE_CPU_COUNT_EVEREST
#define MAXCPU EV_MAX_CPUS
#endif
#define	_SLOTS	EV_BOARD_MAX
#define	_SLICES	EV_MAX_CPUS_BOARD
#define MULTIPROCESSOR 1
#define DPRINT \
	if (EVCFGINFO->ecfg_debugsw & VDS_DEBUG_PROM) printf
#elif SN
#include <libkl.h>
#include <pgdrv.h>
#include <sys/cpu.h>
#include <sys/SN/addrs.h>
#ifndef MAXCPU
#define MAXCPU	MAXCPUS
#endif
#define	_SLOTS	1		/* XXX fixme XXX */
#define	_SLICES	2		/* XXX fixme XXX */
#define MULTIPROCESSOR 1
#ifdef	DEBUG
#define DPRINT	printf
#else
#define DPRINT(a)
#endif
#else /* !EVEREST && !SN */
#ifdef	DEBUG
/*#define DPRINT	if (Debug) printf*/
#define DPRINT	printf
#else
#define DPRINT(a)
#endif

#define	_SLOTS	1
#define	_SLICES	1
#endif /* EVEREST */

#if !MULTIPROCESSOR
#define MAXCPU 1
#endif

struct u u;			/* for driver portability */
int gfx_ok;			/* set by the graphics power-on diagnostic */
char *_intr_char;

extern int ignore_xoff;		/* set during autoboots */
extern int ignore_intr;		/* set by ioctl */
extern struct string_list environ_str;

#ifdef SN0
extern void kl_init_devices(void) ;
extern void sn_init_devices(void) ;
void	_kl_init_libsc_private(void);
extern void kl_check_consenv(void) ;
#endif

static void _init_devices(void);
static void _init_eiob(void);
static void _init_stdio(void);
static void init_walk(COMPONENT *);
static struct eiob *_eiobp[ARCS_FOPEN_MAX];

/*
 * Per-cpu declarations needed for stdio buffering support.
 */
char	*_fputbp[MAXCPU];
char	*_fputbb[MAXCPU];
int	_fputbs[MAXCPU];
int	_fputcn[MAXCPU];
int	_column[MAXCPU];
char	*_sprintf_cp[MAXCPU];

void	_init_signals(void);
int	issig(void);
void	psig(int);

/*
 * _init_saio -- call all prom saio initialization routines.
 *
 * _init_saio is called after aborts and after re-entry to the
 * monitor from external programs, its job is to make sure that
 * the prom data structures are consistent enough to run.  It avoids
 * trashing data structures that contain user entered info that should
 * persist across aborts; things like environment variables, enabled
 * consoles, and current uart baud rates.
 */

void
_init_saio(void)
{

	extern int _prom;

	extern int stdio_init;
	extern int envdirty;
	
	stdio_init = 0;
	ignore_xoff = 0;
	envdirty = 0;

	DPRINT("Initializing PDAs...\n");
	setup_gpdas();
	DPRINT("Initializing malloc...\n");
	_init_malloc();
	DPRINT("Initializing intrs...\n");
	_intr_init();
	DPRINT("Initializing spb...\n");
	_init_spb();			/* init system param block */
	DPRINT("Calling cpu_reset...\n");
	cpu_reset();
	/* cpu_reset initializes the spinlock subsystem */
	initlock(&arcs_ui_lock);
#if defined(_DO_MLOCK)
	initlock(&malloc_lock);
#endif
#ifndef SN0
	DPRINT("Initializing cfgtree..\n");
	init_cfgtree();
#endif

	DPRINT("Initializing libsc_private...\n");
	_init_libsc_private();

#ifdef SN0
	init_module_table() ;
#ifdef SN_PDI
	pgInit() ;
#else
	init_prom_graph() ;
#endif
#endif
	atob(getenv("DEBUG"), &Debug);	/* initialize debugging flags */
	atob(getenv("VERBOSE"), &Verbose);
	_udpcksum = 1;			/* default to checksum on */
	atob(getenv("UDPCKSUM"), &_udpcksum);

	DPRINT("Initializing memdescs...\n");
	md_init();			/* memory descriptors */
	DPRINT("Initializing load space...\n");
	load_init();			/* load mem space init */
	DPRINT("Initializing alarm...\n");
	_init_alarm();			/* alarm signal */
	
	DPRINT("Initializing restart block\n");
#if _K64PROM32
	/*  The restart block is broken on _K64PROM32 and not worth fixing
	 * due to difficulty interfacing with 32 bit prom and it's lack of
	 * significant use.
	 */
	if (_prom)
#endif
	init_rb();			/* init restart block for this cpu */
	DPRINT("Initializing eiobs...\n");
	_init_eiob();			/* extended io control blocks */
	DPRINT("Initializing devices...\n");
	_init_devices();		/* device drivers */
#ifdef SN0
#ifndef SN_PDI
	if (_prom)
	    kl_init_devices() ; 		/* SN0 device drivers */
#else
	if (_prom)
	    sn_init_devices() ; 		/* SN_PDI device drivers */
#endif
#endif
	DPRINT("Initializing fs...\n");
	fs_init();			/* file systems */
	DPRINT("Initializing mbufs...\n");
	_init_mbufs();			/* network buffers */
	DPRINT("Initializing sockets...\n");
	_init_sockets();		/* sockets buffers */
	DPRINT("Initializing boot env...\n");
	_init_bootenv();		/* Setup boot environment variables */

	DPRINT("Initializing stdio...\n");
	_init_stdio();
	_init_mouse();			/* internal mouse driver */
	_init_systemid();		/* ARCS system identifier */
	_init_signals();
	get_tod();			/* tod clock */
}

static int pass;		/* how many times _init_devices is called */

static void
init_walk(COMPONENT *c)
{
	cfgnode_t *cfg = (cfgnode_t *)c;
	register struct eiob *io;

	if (!c) return;			/* end recursion */

	/* init driver */
	if (cfg->driver) {
		if ((io = new_eiob()) == NULL)
			return;
		io->iob.FunctionCode = FC_INITIALIZE;
		io->iob.Count = pass;
 		if ((*cfg->driver)(&cfg->comp, &io->iob) != ESUCCESS) {
			c->Flags |= Failed;
		}
		free_eiob(io);
	}

	/* walk children, then peers */
	init_walk(GetChild(c));
	init_walk(GetPeer(c));
	return;
}

/*
 * _init_devices -- walk the configuration tree and call the init routine
 * for each node.  Device driver init routines should basically cleanup
 * all global storage.
 */
static void
_init_devices(void)
{
	pass++;
	init_walk (GetChild((COMPONENT *)NULL));
}

/*
 * set-up file descriptors 0 and 1
 */
static void
_init_stdio(void)
{
	static char *openerr = "Cannot open %s for %s\n";
	int retried = 0;
	extern int stdio_init;
	char *ci, *co;
	ULONG fd;
	LONG rc;

tryagain:

	Close(StandardIn);
	Close(StandardOut);

	ci = getenv("ConsoleIn");
	co = getenv("ConsoleOut");
	if (!ci || !co)
		goto error;

	if ((rc=Open((CHAR *)ci,OpenReadOnly,&fd)) || (fd != StandardIn)) {
		/*  If we failed we probably failed to open the keyboard
		 * with with cable unattached.  Try to warn the user.
		 */
		if (rc == ENODEV)
			printf("Cannot connect to keyboard -- "
			       "check the cable.\n");
		printf(openerr,ci,"input");
		goto error;
	}
	if (Open((CHAR *)co,OpenWriteOnly,&fd) || (fd != StandardOut)) {
		printf(openerr,co,"output");
		goto error;
	}

	stdio_init = 1;			/* safe to use stdio now */
	return;

error:
	/*  If we havn't retried yet, make sure all environment variables
	 * are set-up.  This can cause the multiple error messages that
	 * are the same (cannot open graphics), but a bad user setting
	 * of ConsoleIn/Out could cause us to miss graphics, or if
	 * console=d all consoles!
	 */
	if (!retried) {
		retried = 1;
		init_consenv(0);
		goto tryagain;
	}

	/*  Try alternative (tty) console one last time if we do not thin
	 * it was tried already (console!=d).  This should always work.
	 */
	ci = getenv("console");
	if (ci && (*ci != 'd') && (retried != 2)) {
		retried = 2;
		setenv("NoAutoLoad","CONSOLE OPEN FAILED.");
		init_consenv("d");		/* reset ConsoleIn/Out */
		goto tryagain;
	} 

	panic("Cannot open any console!");
}

/* Synthesize ARCS console variables from nvram backed console:
 *	console -> create ConsoleIn/ConsoleOut
 */
void
init_consenv(char *cs)
{
	static char *console = "console";
	char *ci,*co;
	char *p;
	struct standcfg *s;
	extern struct standcfg standcfg[];

	/* setup ConsoleIn/ConsoleOut from console
	 */
	p = cs ? cs : getenv(console);
	if (!p)
		replace_str(console,"g",&environ_str);

	/* Check to see if there are any graphics probe routines; if
	 * there aren't, we assume that the caller only supports serial I/O.
	 */
	for (s = standcfg; s->install; s++)
		if (s->gfxprobe)
			break;	

	if (!s->gfxprobe || (p && *p == 'd')) {
		co = ci = (char *)cpu_get_serial();
	}
	else {			/* assume graphics */
		co = (char *)cpu_get_disp_str();
		ci = (char *)cpu_get_kbd_str();
	}
	replace_str("ConsoleOut", co, &environ_str);
	replace_str("ConsoleIn", ci, &environ_str);
}

/*
 * _init_eiob -- make sure eiob's are in known state
 */
static void
_init_eiob(void)
{
	bzero(_eiobp, sizeof _eiobp);
}

/*
 * _get_iobbuf -- obtain block io buffer
 * May be called by file system open routine to obtain buffer.  If
 * fs open calls this, it's close routine MUST free this buffer.
 *
 * There are bugs in the drivers and filesystem code regarding the size
 * the buffer returned by _get_iobbuf.  In bootp.c it needs to by more
 * than 512 bytes for instance.  Until the buffer size problems are fixed,
 * _get_iobbuf returns BLKSIZE*2 which testing shows to be large enough.
 * 10/31/90 - Wiltse.
 */
char *
_get_iobbuf(void)
{
	char *cp = (char *)dmabuf_malloc(BLKSIZE*2);
	if ( cp )
		return cp;
	panic("out of io buffers");
	/*NOTREACHED*/
}

/*
 * _free_iobbuf -- free block io buffer
 */
void
_free_iobbuf(char *cp)
{
	dmabuf_free(cp);
}

/*
 * ioctl -- io control
 * A place to hide all those device and fs specific operations
 * (kinda ugly, but necessary)
 */
LONG
ioctl(ULONG fd, LONG cmd, LONG arg)
{
	register struct eiob *io = get_eiob(fd);
	register long old_arg;
	int errflg;

	if (io == NULL || io->iob.Flags == 0)
		return(-1);

	switch (cmd) {
	case TIOCINTRCHAR:
		old_arg = (long) _intr_char;
		_intr_char = (char *)arg;
		return old_arg;

	case TIOCSETXOFF:
		old_arg = (int) ignore_xoff;
		ignore_xoff = !arg;
		return old_arg;

	case TIOCISATTY:
		*((int *)arg) = isatty(fd);
		return(ESUCCESS);

	case FIOCNBLOCK:
		if (arg)
			io->iob.Flags |= F_NBLOCK;
		else
			io->iob.Flags &= ~F_NBLOCK;
		return(ESUCCESS);

#ifdef NETDBX
	case NIOCREADANY:
		if (arg)
			io->iob.Flags |= F_READANY;
		else
			io->iob.Flags &= ~F_READANY;
		return(ESUCCESS);
#endif /* NETDBX */

	case TIOCISGRAPHIC:
		*((int *)arg) = (io->dev->Type == DisplayController);
		return(ESUCCESS);
	}

	io->iob.FunctionCode = FC_IOCTL;
	io->iob.IoctlCmd = (IOCTL_CMD)cmd;
	io->iob.IoctlArg = (IOCTL_ARG)arg;
	errflg = (*io->devstrat)(io->dev, &io->iob);

	return(errflg);
}

/*
 * _scandevs -- scan all enabled getc devices for input
 * Called periodically to avoid receiver overruns
 */
void
_scandevs(void)
{
	static int scan_count;
	int i;

	/* check alarm clock 
	 */
	if ((++scan_count & 0xf) == 0)
		check_alarm();	

	cpu_scandevs();			/* service cpu depenant non fd devs */

	/* check scannable devices
	 */
	for (i = 0; i < ARCS_FOPEN_MAX; i++) {
		struct eiob *io = _eiobp[i];
		if ( io && (io->iob.Flags & F_SCAN) ) {
			io->iob.FunctionCode = FC_POLL;
			(*io->devstrat) (io->dev, &io->iob);
		}
	}

	/* check for recent signals
	 */
	while (i = issig())
		psig(i);
}

/*
 * new_eiob -- obtain a new standalone io control block
 */
struct eiob *
new_eiob(void)
{
	int i;

	/* Find a slot for new file descriptor and insert a new one. */
	for ( i = 0; i < ARCS_FOPEN_MAX; i++ )
		if ( _eiobp[i] == 0 )
			break;

	if ((i == ARCS_FOPEN_MAX) || 
	    !(_eiobp[i] = (struct eiob *)malloc(sizeof *_eiobp[0])))
		return(NULL);
	
	bzero (_eiobp[i], sizeof *_eiobp[i]);
	_eiobp[i]->fd = i;

	return _eiobp[i];
}

/*
 * get_eiob -- find io control block for fd
 */
struct eiob *
get_eiob(ULONG fd)
{
	if (fd >= ARCS_FOPEN_MAX)		/* fd is unsigned */
		return NULL;
	return _eiobp[fd];
}

void
free_eiob(struct eiob *io)
{
	if (io->filename) free(io->filename);
	_eiobp[io->fd] = 0;
	free(io);
}

/*
 * isatty -- determine if fd refers to a character device
 */
int
isatty(ULONG fd)
{
	register struct eiob *io = get_eiob(fd);

	if (io && ((io->dev->Type == SerialController) ||
		(io->dev->Type == LinePeripheral) ||
		(io->dev->Type == DisplayController) ||
		(io->dev->Type == KeyboardPeripheral) ||
		(io->dev->Type == PointerPeripheral)))
		return 1;
	else
		return 0;
}

/*
 * console_is_gfx -- determine if the console is a graphics device
 *
 * its real use is before the I/O system is initialized
 */
int
console_is_gfx(void)
{
	extern int stdio_init;

	/* if i/o system hasn't been initialized yet, do what we can
	 * to determine if we have a graphics console, ignoring fd
	 */
	if (!stdio_init) {
		char *co = getenv("console");

		return (gfx_ok && co && (*co == 'g' || *co == 'G'));
	}
	return(isgraphic(StandardOut));
}

/* for use of dma_map, and the various drivers based on the kernel drivers
*/
void
vflush(void *addr, unsigned cnt)
{
	/*ASSERT(SCACHE_ALIGNED(addr));*/
	if(IS_KSEG0(addr))
		clear_cache(addr, cnt);
}

void
_init_bootenv(void)
{
	struct volume_header dvh;
	COMPONENT *disk;
	char pathname[256];
	char eb[128];
	ulong dfd;
	char *sp, *oslp, *osl, *oslf;

        sp   = getenv("SystemPartition");
        oslp = getenv("OSLoadPartition");
        osl  = getenv("OSLoader");
        oslf = getenv("OSLoadFilename");

        /* If the systempartition or osloadpartition aren't set, try to
         * synthesize them appropriately.  We go out of our way to avoid
         * doing this, since on Challenges the following code may not produce
         * the correct results and can cause the CDROM to time out.  -jfk
         */
        if (!sp || !oslp || !oslf) {
                if ( !(disk = find_type(GetChild(0), DiskPeripheral)) )
                        return;

                strcpy(pathname, getpath(disk));

                if ( Open(pathname, OpenReadOnly , &dfd) != ESUCCESS )
                        return;

                if ( ioctl(dfd, DIOCGETVH, (long)&dvh) == -1 ) {
                        Close(dfd);
                        return;
                }

                if (dvh.vh_magic == VHMAGIC) { /* Old style disk */
                        int i;
                        /* System Partition (volume header) */
                        if ( ! sp)
                                for ( i = 0; i < NPARTAB; i++)
                                    if ( dvh.vh_pt[i].pt_nblks &&
                                        dvh.vh_pt[i].pt_type == PTYPE_VOLHDR ) {                                            sprintf(eb,"%spartition(%d)",
                                                    pathname,i);
                                            replace_str("SystemPartition",
                                                        eb, &environ_str);
                                            break;
                                    }

                        /* OS Load Partition */
                        if ( ! oslp ) {
                                sprintf(eb,"%spartition(%d)",pathname,
                                        dvh.vh_rootpt);
                                replace_str("OSLoadPartition", eb,
                                            &environ_str);
                        }

			/* OS Load Filename */
			if ( ! oslf )
				replace_str("OSLoadFilename", 
					dvh.vh_bootfile, &environ_str);

                } else
                        printf("Warning: unrecognized volume header type.\n");

                Close(dfd);
        }

        /* OS Loader */
        if ( ! osl )
                replace_str("OSLoader", "sash", &environ_str);
}

