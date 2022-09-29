#ident	"lib/libsc/cmd/cat_cmd.c:  $Revision: 1.13 $"

/*
 * cat -- cat files
 */

#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>

/*ARGSUSED*/
int
cat(int argc, char **argv, char **envp, struct cmd_table *bunk2)
{
    register char *bp;
    char buf[512];
    ULONG fd, cc;
    long rc;

    if (argc < 2)
	return(1);

    while (--argc > 0) {
	argv++;

	if ((rc = Open ((CHAR *)*argv, OpenReadOnly, &fd)) != ESUCCESS) {
	    perror(rc,*argv);
	    continue;
	}

	while (((rc = Read(fd, buf, sizeof(buf), &cc))==ESUCCESS) && cc)
	    for (bp = buf; bp < &buf[cc]; bp++)
		putchar(*bp);
	if (rc != ESUCCESS)
	    printf("*** error termination on read\n");
	Close(fd);
    }
    return(0);
}
