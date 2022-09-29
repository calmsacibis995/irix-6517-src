#ident	"lib/libsk/cmd/reboot_cmd.c:  $Revision: 1.2 $"
#include <libsc.h>
#include <libsk.h>

/*
 * Miscellaneous commands.
 */

/*ARGSUSED*/
int
reboot_cmd(int argc, char **argv)
{
	cpu_hardreset();
	printf ("Software reset not available on this CPU.\n");
	return 0;
}
