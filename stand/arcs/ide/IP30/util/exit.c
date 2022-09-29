#include <sys/types.h>
#include <sys/cpu.h>
#include <arcs/spb.h>
#include <arcs/restart.h>
#include <libsk.h>
#include <ide.h>
#include <genpda.h>
#include <ide_mp.h>

/* Hook to send all CPUs back to the prom.
 */
void
EnterInteractiveMode(void)
{
	argdesc_t	acvp;
	int		id;
    	int		my_id = cpuid();

	acvp.argc = 0;
	acvp.argv = NULL;
	for (id = 0; id < MAXCPU; id++) {
		if (id != my_id && cpu_is_active(id)) {
			_do_slave_asynch(id,(volvp)__TV->EnterInteractiveMode,
					 &acvp);
		}
	}

	__TV->EnterInteractiveMode();
}
