/***********************************************************************\
 * This is a main program stub for the flash commands.
 * Main goal is to identify the platform on which the command is
 * running, and jump to appropriate routines..
 * Even the command parsing is done by the routines in the respective
 * file (i.e. flashio_everest.c flashio_sn0.c
 * 
 * Use inventory to figure out the system type.
 ***********************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/systeminfo.h>

extern int flashio_racer(int argc, char **argv);
extern int flashio_everest(int, char **);
extern int flashio_sn0(int, char **);

main(int argc, char *argv[])
{

	char	name[16];


	sysinfo(SI_MACHINE, name, 16);

	if ((strcmp(name, "IP19") == 0) ||
	    (strcmp(name, "IP21") == 0) ||
	    (strcmp(name, "IP25") == 0))
		return flashio_everest(argc, argv);

	if (strcmp(name, "IP27") == 0)
		return flashio_sn0(argc, argv);

	if (strcmp(name, "IP30") == 0)
		return flashio_racer(argc, argv);

	fprintf(stderr, "%s: Not supported on %s platform \n",
			argv[0], name);

	return 0;
}
