#include <sys/types.h>
#include <sys/timers.h>
#include <sys/ktime.h>
#include <sys/immu.h>
#include <sys/pfdat.h>
#include <sys/pda.h>
#include <sys/callo.h>
#include <sys/cpu.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/invent.h>
#include <sys/arcs/spb.h>
#include <sys/syssgi.h>
#include <sys/debug.h>

static char *tmp_eaddr = "08:00:11:22:33:44";

char *
flash_getenv(char *var)
{
    if (!strncmp("eaddr", var, 5))
	return(tmp_eaddr);
    else
	return(NULL);
}

/*ARGSUSED*/
void
flash_nvram_tab(char *tab, int size)
{
	return;
}

/*ARGSUSED*/
int
vice_err(_crmreg_t addr, _crmreg_t stat)
{
	return(0);
}
