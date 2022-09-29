#ident	"lib/libsc/cmd/mount_cmd.c:  $Revision: 1.4 $"

/*
 * mount -- mount loadable media on device
 */

#include <arcs/io.h>
#include <libsc.h>

/*ARGSUSED*/
int
mount_cmd(int argc, char **argv, char **envp)
{
    long rv;

    while (--argc) {
	++argv;
	rv = Mount ((CHAR *)*argv, LoadMedia);
	if (rv)
	    perror (rv, *argv);
    }
    return 0;
}
