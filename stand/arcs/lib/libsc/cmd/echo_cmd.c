#ident	"lib/libsc/cmd/echo_cmd.c:  $Revision: 1.7 $"

/*
 * echo_cmd - echo args to output device
 */

#include <parser.h>
#include <saioctl.h>
#include <arcs/io.h>
#include <libsc.h>
#include <libsc_internal.h>

/*ARGSUSED*/
int
echo_cmd(int argc, char **argv, char **envp)
{
    char *cmdname = *argv;
    char outstring[LINESIZE], *nextchar, *nextarg;
    int devnext = 0;
    char *devname = 0;
    ULONG fd, count;
    long rc;

    nextchar = outstring;

    while (--argc) {
	++argv;
	nextarg = expand (*argv, 0);

	if (devnext) {
	    devname = nextarg;
	    break;	/* drop any following arguments */
	}

	if (!strcmp (nextarg, ">")) {
	    devnext = 1;
	    continue;
	}

	if (strlen(nextarg) + nextchar + 4 >= &outstring[LINESIZE]) {
	    printf ("%s: too many arguments\n", cmdname);
	    return 0;
	}

	strcpy (nextchar, nextarg);
	nextchar += strlen(nextarg);
	*nextchar++ = ' ';
    }
    *nextchar++ = '\r';
    *nextchar++ = '\n';
    *nextchar++ = '\0';
    
    if (!devname) {
	fd = StandardOut;
	devname = "Standard Out";
    } else if (rc = Open((CHAR *)devname, OpenWriteOnly, &fd)) {
	perror(rc,devname);
	return 0;
    }

    if (!isatty(fd)) {
	printf ("%s: %s is not a terminal\n", cmdname, devname);
	Close(fd);
	return 0;
    }

    if (rc = Write(fd, (void *)outstring, strlen(outstring), &count))
	perror (rc, devname);

    if (fd != StandardOut)
	Close(fd);
    return 0;
}
