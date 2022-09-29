#if defined(SN0) || defined(SN0PROM) || defined(SN0XXLPROM)

#define FIXME(foo)	printf("fixme: "); printf(foo); printf("\n"); return 0;

void printf(char *, ...) ;

_ticksper1024inst(void) { FIXME("_ticksper1024inst"); }
bell(void) {}
_z8530_strat(void) { FIXME("_z8530_strat"); }
kbd_install(void) { FIXME("kbd_install"); }

int multi_io6 = 0;

/* Graph stubs */

mrlock(){return 0 ;}
mrunlock(){return 0 ;}
mrlock_init(){return 0 ;}
mrinit(){return 0 ;}
mrfree(){return 0 ;}
replace_in_inventory(){return 0 ;}

#if !(defined(SN0PROM) || defined(SN0XXLPROM))
/*
 * Stubs for Graphics Cards
 * These are linked in for the SN0 base (server) product which is used
 * to build the IP27 prom (including the embedded copy of the
 * IO6 prom).  The IO6 prom image should be built using the SN0PROM
 * product.
 */
void kl_graphics_install() { }
#endif /* !SN0PROM */

#elif IP32

/* Stubs for various "missing" modules. */


/*
 * NOTE: A lot of these stubs were put in here originally to keep a 
 * degree of INDY compatability.  The code has basically be MH specific
 * now so the INDY compatability stubs are being turned off and removed.
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <flash.h>
#include <libsc.h>

extern void _errputs(char*);

#define XCRASH(x) void x() { _errputs("CRASH calling " #x "\n"); while (1);}
#define RET0(x)   int  x(){ return 0; }
#define RET1(x)   int  x(){ return 1; }

#if 0
XCRASH(txPrint)			/* errputs.o */
#endif
RET0(bell)			/* passwd_cmd.o, htport.o */
XCRASH(cpu_savecfg)		/* config.o */
RET0(cpu_show_fault)		/* fault.o */
RET0(cpuid)			/* badaddr.o, fault.o, genpda.o, spb.o,... */
RET0(eerom_read)		/* r4kcache.s */
RET0(cpu_nv_lock)		/* setenv.c */
RET0(cpu_set_nvram)		/* setenv.c */
#if 0
XCRASH(ecc_error_decode)	/* fault.o */
#endif
RET0(ecc_error_decode)	/* fault.o */

#endif /* IP32 */
