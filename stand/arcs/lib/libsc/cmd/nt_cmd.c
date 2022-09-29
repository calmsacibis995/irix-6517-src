#ident	"lib/libsc/cmd/nt_cmd.c:  $Revision: 1.1 $"

/*
 * nt_cmd.c -- special command for NT 
 */

#include "libsc.h"

int
nt_cmd(int argc, char **argv)
{
	char *cp;
	char buf[75];

	if (argc != 2 && argc != 3)
		return -1;

	++argv;
	--argc;

	if (!(cp = index(*argv, ':')))
		return -1;

	*cp++ = '\0';

	sprintf (buf, "scsi()disk(%s)rdisk()partition(%s)", *argv, cp);
	printf ("%s\n", buf);

	setenv ("SystemPartition", buf);
	setenv ("OSLoadPartition", buf);

	++argv;
	--argc;

	if (argc) {
		if (**argv == 'n')
		    setenv ("OSLoadOptions", "nodebug");
		else if (**argv == 'd')
		    setenv ("OSLoadOptions", "");
		else
		    printf ("Illegal option: %s\n", *argv);
	}

	return 0;
}
