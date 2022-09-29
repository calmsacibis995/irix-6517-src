#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include <fcntl.h>

static char *OLDprefix = "OLD";

static void rotate (char *fname) {
	char *Ofname;

	if ((Ofname = malloc (strlen (fname) + strlen (OLDprefix) + 1)) == NULL) {
		perror ("rotatelogs: out of memory");
		exit (2);
	}
	strcpy (Ofname, OLDprefix);
	strcat (Ofname, fname);
	rename (fname, Ofname);
	free (Ofname);
}

main (int argc, char *argv[]) {
	int i;

	for (i=1; i<argc; i++)
		rotate (argv[i]);
	exit (0);
	/* NOTREACHED */
}
