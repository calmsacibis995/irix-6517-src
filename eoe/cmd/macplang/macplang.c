/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * #ident "$Revision: 1.5 $"
 *
 * Initialize attribute/mac_label and attribute/mac_index files
 * (all files system low, equal integrity).
 *
 * pdc, July 19, 1990
 *
 */

#include <sys/types.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syssgi.h>
#include <sys/mac_label.h>
#include <sys/eag.h>

#define	DEFAULT_SIZE	8

mac_label label = { MSEN_LOW_LABEL, MINT_EQUAL_LABEL, 0, 0, 0, 0 };

void create_files(char *path, char *index_path, char *label_path, int existok);
char *build_name(char *path, char *file);

main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	int c;
	int create_flag = 0;
	int apply_flag = 1;
	int existok_flag = 0;
	char *index_path;
	char *label_path;
	struct stat statbuf;		/* Data returned by stat(2) */

	while ((c = getopt(argc, argv, "bce")) != -1) {
		switch (c) {
		case 'b':
			create_flag = 1;
			break;
		case 'c':
			create_flag = 1;
			apply_flag = 0;
			break;
		case 'e':
			existok_flag = 1;
			break;
		default:
			break;
		}
	}

	/*
	 * A directory to put the databases into is the expected parameter.
	 * The passed directory must exist.
	 */
	if (optind >= argc) {
		fprintf( stderr, "usage: macplang mount_directory\n");
		exit(1);
	}

	index_path = build_name(argv[optind], EAG_MAC_INDEX);
	label_path = build_name(argv[optind], EAG_MAC_LABEL);

	if (create_flag)
		create_files(argv[optind], index_path, label_path,
		    existok_flag);

	if (stat(index_path, &statbuf) < 0 ||
	    stat(label_path, &statbuf) < 0) {
		perror("macplang: Inaccessible attributes");
		exit(1);
	}

	if (apply_flag)
		if (syssgi(SGI_LOADATTR, argv[optind]) < 0) {
			perror("macplang: Cannot apply attributes");
			exit(1);
		}

	exit(0);
}

char *
build_name(char *path, char *file)
{
	char *result = malloc(PATH_MAX);

	strcpy(result, path);
	strcat(result, "/");
	strcat(result, file);
	return (result);
}

void
create_files(char *path, char *index_path, char *label_path, int existok)
{
	eag_mac_index_t element;	/* Value of the default label */
	mac_label *lp;			/* Binary value of default label */
	int i, ret;
	struct stat statbuf;		/* Data returned by stat(2) */
	struct statfs fsbuf;		/* Data returned by statfs(2) */
	char file[PATH_MAX];		/* Name of the new file */
	FILE *fd;			/* For writing to the file */

	/*
	 * Get the binary version of the label.
	 * The length must be the 'proper' size, else something's wrong.
	 * Put the label directly into the element and set the flag field.
	 */
	if (mac_size(&label) != DEFAULT_SIZE) {
		fprintf(stderr, "macplang: Size is not what was expected\n");
		exit(1);
	}
	bcopy(&label, &element, mac_size(&label));
	element.emi_flags = EAG_MAC_DIRECT;

	ret = stat( path, &statbuf );
	if (ret == -1) {
		perror("stat");
		fprintf( stderr, "usage: macplang mount_directory\n");
		exit(1);
	}

	/*
	 * "attribute" needs to be at a mount point.
	 */
	if ( statbuf.st_ino != 2 ) {
		fprintf( stderr, "%s not a mount point\n", path);
		fprintf( stderr, "usage: macplang mount_directory\n");
		exit(1);
	}

	/*
	 * make attribute directory
	 */
	strcpy(file, path);
	strcat(file, "/attribute");
	ret = mkdir(file, 0755);
	if (ret == -1) {
		/*
		 * If "-e" was specified it's OKay that the files exist.
		 * In that case, simply exit.
		 */
		if (existok)
			exit(0);
		perror("mkdir");
		exit(1);
	}

	/*
	 * touch attribute/mac_label'
	 */
	ret = open(label_path, O_CREAT, 0400);
	if (ret == -1) {
		perror("touch mac_label");
		exit(1);
	}
	close( ret );

	/*
	 * make attribute/mac_index
	 */
	ret = open(index_path, O_WRONLY | O_CREAT | O_EXCL, 0400);
	if (ret == -1) {
		perror("create mac_index");
		exit(1);
	}

	fd = fdopen( ret, "w" );
	if (fd == 0) {
		fprintf( stderr, "fdopen error.\n" );
		exit(1);
	}

	/*
	 * stat the file system to find out how many inodes
	 */
	ret = statfs (path, &fsbuf, sizeof(struct statfs), 0);
	if (ret == -1) {
		perror("statfs");
		exit(1);
	}

	/*
	 * make one entry for each inode in the file system
	 */
	for (i = fsbuf.f_files; i > 0; i--)
		fwrite(&element, sizeof(element), 1, fd);

	fclose(fd);
}
