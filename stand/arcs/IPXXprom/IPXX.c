#ident	"IP12prom/IP12.c:  $Revision: 1.20 $"

/*
 * IPXX.c -- machine dependent prom routines
 */

#include <setjmp.h>
#include <saioctl.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <pon.h>
#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/folder.h>
#include <arcs/cfgtree.h>
#include <arcs/restart.h>
#include <arcs/spb.h>
#include <arcs/time.h>
#include <gfxgui.h>
#include <style.h>
#include <libsk.h>
#include <libsc.h>

#include "ipxx.h"

extern jmp_buf restart_buf;

static void alloc_memdesc(void);
static unsigned int clear_memory(int);
int real_main(void);

void init_prom_soft(int);
void sysinit(void);

int ioserial,fullscreen,autoload,vdiagmode,faildiags,setdl,failgdiags;
int altres_x, altres_y;
char **uenviron;

int
main(int argc, char **argv, char **envp)
{
	char *p;
	int i;

	uenviron = envp;		/* save env for fork! */

	/* incomplete arg checking for -s for serial output only */
	/*  Incomplete arg checking:
	 *	-s	serial output only (do not bring up gl)
	 *	-h	high resolution (1280x1024)
	 *	-r XxY	set alternate resolutions (test VGA, etc)
	 *	-f	full screen
	 *	-a	set autoload
	 *	-v	diagmode == 'v'
	 *	-F	fail diagnostics	(but graphics)
	 *	-g	fail graphics diagnostics
	 *	-D	setenv diskless=1
	 */
	for (i=1 ; i < argc; i++) {
		if (*argv[i] == '-') {
			for (p=argv[i]; *(++p) ; ) {
				switch (*p) {
				case 's':
					ioserial = 1;
					break;
				case 'h':
					altres_x = 1280;
					altres_y = 1024;
					break;
				case 'r':
					i++;
					if (*(p+1) || ((i+1) >= argc))
						goto usage;
					altres_x = atoi(argv[i]);
					altres_y = atoi(argv[i+1]);
					i += 2;
					break;
				case 'f':
					fullscreen = 1;
					break;
				case 'a':
					autoload = 1;
					break;
				case 'v':
					vdiagmode = 1;
					break;
				case 'F':
					faildiags = 1;
					break;
				case 'g':
					failgdiags = 1;
					break;
				case 'D':
					setdl = 1;
					break;
				default:
usage:
					l_write(0,"usage: prom [-shfavFgD] [-r X Y]\n",
						33);
					l_exit(1);
				}
			}
		}
	}

	sysinit();
}

/*
 * sysinit - finish system initialization and run diags
 */
void
sysinit(void)
{
    extern __scunsigned_t dmabuf_linemask;
    int diagsfailed;

    dmabuf_linemask = 4;

    _init_promgl();

    _init_malloc();     /* do it now such that diags can use dmabuf_malloc */

    init_env();

    diagsfailed = pon_morediags();
    init_prom_soft(1);

    if (diagsfailed) {
	    setTpButtonAction(real_main,TPBCONTINUE,WBNOW);
	    printf("\nDiagnostics failed.\n[Press any key to continue.]");
	    getchar();
	    real_main();
    }
    else {
	    startup ();
    }
}

/* 
 * init_prom_soft - configure prom software 
 */
void
init_prom_soft(int noenv)
{
    if (noenv == 0)
    	init_env();		/*  initialize environment */
    _init_saio();		/*  initialize saio library */
    alloc_memdesc();		/*  allocate memory descriptors for prom */

}

/* _halt(): work needed by various halt-like routines.
 */
static void
_halt(void)
{
    if (doGui()) setGuiMode(1,GUI_NOLOGO);
    popupDialog("\n\nOkay to power off the system now.\n\n"
		"Press RESET button to restart.",
		0,DIALOGINFO,
		DIALOGCENTER|DIALOGBIGFONT);
}

/*
 * halt - bring the system to a quiescent state
 */
void
halt(void)
{
	_halt();
}

/*
 * powerdown - attempt a soft power down of the system, alternatively halt
 */
void
powerdown(void)
{
    cpu_soft_powerdown();
    _halt();
}

/*
 * restart - attempt to restart
 */
void
restart(void)
{
    static int restart_buttons[] = {DIALOGRESTART, -1};

    if (doGui()) setGuiMode(1,GUI_NOLOGO);
    popupDialog("\n\nOkay to power off the system now.\n\n"
		"Press any key to restart.",
		restart_buttons,DIALOGINFO,
		DIALOGCENTER|DIALOGBIGFONT);
    if (doGui()) setGuiMode(0,GUI_RESET);
    startup();
}

/*
 * reboot - attempt a reboot
 *
 * XXX This is supposed to try to reproduce the last system boot
 * command, but it doesn't.
 */
void
reboot(void)
{
    extern int Verbose;

    if (!Verbose && doGui()) {
	    setGuiMode(1,0);
    }
    else {
	    p_panelmode();
	    p_cursoff();
    }
    if (ESUCCESS != autoboot(0, 0, 0)) {
	putchar('\n');
	setTpButtonAction(EnterInteractiveMode,TPBCONTINUE,WBNOW);
	p_curson();
	p_printf("Unable to boot; press any key to continue: ");
	getchar();
	putchar('\n');
    }

    /* If reboot fails, reinitialize software in case a partially
     * booted program has messed up the prom state.
     */
    EnterInteractiveMode();
}

void
enter_imode(void)
{
    _hook_exceptions();
    _init_saio();		/*  initialize saio library */
    alloc_memdesc();		/*  allocate memory descriptors for prom */
    wbflush();
    real_main();
    EnterInteractiveMode();	/* shouldn't get here */
}

static void
alloc_memdesc(void)
{
#define STKSZ	8192 		/* 8K stack */

    /*  Add what we know to be the dprom's text, data as Firmwareermanent,
     * and bss (rounded up from data page) + stack as Firmware as
     * FirmwareTemporary.
     */
    extern _ftext[], _etext[], _edata[], _end[];
    unsigned bsspage1 = ((unsigned)_edata & ~(ARCS_NBPP-1)) + ARCS_NBPP;
    unsigned etext = (unsigned) _etext;
    unsigned ftext = (unsigned) _ftext;

    md_alloc (KDM_TO_PHYS(_ftext), 
	      arcs_btop(KDM_TO_PHYS(etext - ftext)),
	      FirmwarePermanent);
#if 0		/* ld is putting bss at a high address -- let's skip it */
    md_alloc (KDM_TO_PHYS(bsspage1),
	      arcs_btop(KDM_TO_PHYS((unsigned)_end - (unsigned)bsspage1)),
	      FirmwareTemporary);
#endif
}

/*
 * clear_memory - clear remaining untouched locations of memory
 * The area of bss and stack (PROM_BSS to PROM_STACK) has already
 * been cleared by init_memconfig on power-on.  This clears
 * between 0 and PROM_BSS and between PROM_STACK and top-of-memory.
 */
#define CLRSIZE 0x40		/* matches size in lmem_conf.s */
static unsigned int
clear_memory(int uncached)
{
    return (4*0x100000);
}

static RestartBlock restartb;
static char spbpage[4096];
static spb_t *spb;

RestartBlock *
get_cpu_rb(int cpu)
{
	SPB->RestartBlock = (void *)&restartb;
	return(&restartb);
}

void cpu_get_tod(TIMEINFO *at)
{
	static TIMEINFO t;

	l_sginap(1);

	if ((t.Milliseconds += 3) >= 100) {
		t.Milliseconds = 0;
		if ((++t.Seconds) == 60) {
			t.Seconds = 0;
			if ((++t.Minutes) == 60) {
				t.Minutes = 0;
				if ((++t.Hour) == 24) {
					t.Hour = 0;
					if ((++t.Day) == 30) {
						t.Day = 0;
						if ((++t.Month) == 12) {
							t.Month = 0;
							t.Year++;
						}
					}
				}
			}
		}
	}
	bcopy(&t,at,sizeof(TIMEINFO));
	return;
}

spb_t *
get_spb()
{
	spb=(spb_t *)spbpage;
	return(spb);
}

static int tpfd[2];
static int kbfd[2];
static int msfd[2];
int gltpfd, glmsfd, glkbfd;

void
_init_promgl(void)
{
	char *initav[2];

	l_pipe(tpfd);
	l_pipe(kbfd);
	l_pipe(msfd);

	if (l_fork() == 0) {
		l_close(0);
		l_dup(tpfd[0]);
		l_close(tpfd[0]);
		l_close(tpfd[1]);
		l_close(3);
		l_dup(kbfd[1]);
		l_close(kbfd[0]);
		l_close(kbfd[1]);
		l_close(4);
		l_dup(msfd[1]);
		l_close(msfd[0]);
		l_close(msfd[1]);
		bzero(initav,sizeof(initav));
		initav[0] = "promgl";
		l_execve("./promgl",initav,uenviron);
		printf("exec failed!\n");
		l_exit(1);
	}

	l_close(tpfd[0]);
	l_close(kbfd[1]);
	l_close(msfd[1]);

	gltpfd = tpfd[1];
	glkbfd = kbfd[0];
	glmsfd = msfd[0];
}

/* may need to clear important data structures to simulate .bss clear.
 */
void
clear_prom_ram()
{
	extern int gfx_ok;

	gfx_ok = 0;
}
