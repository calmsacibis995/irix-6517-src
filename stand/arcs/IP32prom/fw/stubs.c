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
RET0(txPrint)			/* errputs.o */
#endif
RET0(bell)			/* passwd_cmd.o, htport.o */
XCRASH(cpu_savecfg)		/* config.o */
RET0(cpu_show_fault)		/* fault.o */
XCRASH(ecc_error_decode)	/* fault.o */
/* RET0(cpu_scandevs)		   saio.o       */
/* RET0(cpu_reset)		   reboot_cmd.o */
/* XCRASH(cpu_hardreset)	   reboot_cmd.o */
/* XCRASH(wbflush)                              */

void
run_cached(void)
{}
