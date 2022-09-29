/*
 * User level program to translate NIS client map names
 * into actual database file names.
 *
 * This is for use by the 'make.script' that builds
 * the initial copies of the database files.
 */
#include <stdio.h>
#include <sys/param.h>

main(argc, argv)
int argc;
char **argv;
{
	char *path[MAXPATHLEN];

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <map-name>\n", *argv);
		exit(1);
	}

	yp_make_dbname(*++argv, path, sizeof path);
	printf("%s", path);
}
