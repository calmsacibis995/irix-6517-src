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

#ident "$Revision: 1.1 $"
/*
 * Initialize Plan G databases.
 */

#include <sys/types.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/syssgi.h>
#include <sys/mac_label.h>
#include <sys/inf_label.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <sys/eag.h>

#define	DIRECT_LABEL_MAX	8

mac_label mac_def_label = { MSEN_LOW_LABEL, MINT_EQUAL_LABEL, 0, 0, 0, 0, };
inf_label inf_def_label = { 0, 0, };

char *program;

char *
build_name(char *path, char *file)
{
	char *result = malloc(PATH_MAX);

	strcpy(result, path);
	strcat(result, "/");
	strcat(result, file);
	return (result);
}

int
create_file(char *path, char *file, int existok)
{
	int fd;
	struct stat statbuf;		/* Data returned by stat(2) */

	path = build_name(path, file);

	if (stat(path, &statbuf) < 0) {
		if ((fd = open(path, O_CREAT, 0400)) < 0) {
			perror("open");
			fprintf(stderr, "%s: cannot open %s\n", program, path);
			return (1);
		}
		close(fd);
	}
	else if (!existok) {
		fprintf(stderr, "%s: %s exists\n", program, path);
		return (1);
	}
	return (0);
}

static int
inode_count(char *file)
{
	struct statfs fsbuf;		/* Data returned by statfs(2) */
	
	if (statfs(file, &fsbuf, sizeof(struct statfs), 0) < 0)
		return (-1);
	return (fsbuf.f_files);
}

static int
fill_file(char *path, char *file, char *data, int size)
{
	int fd;
	int i;

	file = build_name(path, file);

	if ((fd = open(file, O_RDWR)) < 0) {
		perror(file);
		return (1);
	}

	if ((i = inode_count(file)) < 0) {
		perror(file);
		return (1);
	}
	/*
	 * make one entry for each inode in the file system
	 */
	while (i--) {
		if (write(fd, data, size) != size) {
			perror(file);
			close(fd);
			return (1);
		}
	}

	close(fd);
	return (0);
}

int
create_directory(char *mountpoint, char *dirname)
{
	struct stat statbuf;		/* Data returned by stat(2) */
	char *directory = build_name(mountpoint, dirname);

	if (stat(mountpoint, &statbuf) < 0) {
		perror(mountpoint);
		fprintf(stderr, "%s: cannot access %s\n", program, mountpoint);
		return (1);
	}
	/*
	 * Needs to be at a mount point.
	 */
	if (statbuf.st_ino != 2) {
		fprintf(stderr,"%s: %s not a mount point\n",program,mountpoint);
		return (1);
	}
	/*
	 * Make attribute directory. It's OKay if the mkdir fails because
	 * the directory exists.
	 */
	if (mkdir(directory, 0755) < 0 && errno != EEXIST) {
		perror(directory);
		fprintf(stderr, "%s: cannot create %s\n", program, directory);
		return (1);
	}
	return (0);
}

int
create_mac_files(char *path, mac_label *lp, int existok)
{
	mac_eag_index_t element;	/* Value of the default label */
	char *directory = build_name(path, MAC_EAG_DIR);

	/*
	 * Make sure the directory exists.
	 */
	if (create_directory(path, MAC_EAG_DIR))
		return (1);

	bzero(&element, sizeof(element));
	bcopy(lp, &element, mac_size(lp));
	element.mei_flags = MAC_EAG_DIRECT;

	/*
	 * Create attribute/mac_label
	 */
	if (create_file(directory, MAC_EAG_LABEL_FILE, existok))
		return (1);
	/*
	 * Create and fill attribute/mac_index
	 */
	if (create_file(directory, MAC_EAG_INDEX_FILE, existok))
		return (1);
	if (fill_file(path, MAC_EAG_INDEX, (char *)&element, sizeof(element)))
		return (1);
	
	return (0);
}

int
create_inf_files(char *path, inf_label *lp, int existok)
{
	inf_eag_index_t element;	/* Value of the default label */
	char *directory = build_name(path, INF_EAG_DIR);

	/*
	 * Make sure the directory exists.
	 */
	if (create_directory(path, INF_EAG_DIR))
		return (1);

	bzero(&element, sizeof(element));
	bcopy(lp, &element, inf_size(lp));
	element.iei_flags = INF_EAG_DIRECT;

	/*
	 * Create attribute/inf_label
	 */
	if (create_file(directory, INF_EAG_LABEL_FILE, existok))
		return (1);
	/*
	 * Create and fill attribute/inf_index
	 */
	if (create_file(directory, INF_EAG_INDEX_FILE, existok))
		return (1);
	if (fill_file(path, INF_EAG_INDEX, (char *)&element, sizeof(element)))
		return (1);
	
	return (0);
}

int
create_acl_files(char *path, int existok)
{
	acl_eag_index_t acl_index = { -1, -1 };
	int fd = -1;
	int inodes;
	int i;
	char *directory = build_name(path, ACL_EAG_DIR);

	/*
	 * Make sure the directory exists.
	 */
	if (create_directory(path, ACL_EAG_DIR))
		return (1);
	/*
	 * Create attribute/acl_acl.
	 */
	if (create_file(directory, ACL_EAG_ACL_FILE, existok))
		return (1);
	/*
	 * Create and fill attribute/acl_index
	 */
	if (create_file(directory, ACL_EAG_INDEX_FILE, existok))
		return (1);

	acl_index.aei_acl = -1;
	acl_index.aei_defacl = -1;

	if (fill_file(path, ACL_EAG_INDEX,(char *)&acl_index,sizeof(acl_index)))
		return (1);

	return (0);
}

/*
 * Note: No way to set a "default" value.
 */
int
create_cap_files(char *path, int existok)
{
	char *directory = build_name(path, CAP_EAG_DIR);
	cap_eag_index_t dex = -1;
	
	/*
	 * Make sure the directory exists.
	 */
	if (create_directory(path, CAP_EAG_DIR))
		return (1);

	/*
	 * Create attribute/cap_set and attribute/cap_index
	 */
	if (create_file(directory, CAP_EAG_SET_FILE, existok))
		return (1);
	if (create_file(directory, CAP_EAG_INDEX_FILE, existok))
		return (1);
	if (fill_file(path, CAP_EAG_INDEX, (char *)&dex, sizeof(dex)))
		return (1);
	
	return (0);
}

void
arg_error(char *arg, char *value)
{
	fprintf(stderr, "%s: argument %s cannot use value \"%s\".\n",
	    program, arg, value);
	exit(1);
}

main(int argc, char *argv[])
{
	int i;
	int create_flag = 0;
	int apply_flag = 1;
	int existok_flag = 0;
	int acl_flag = 0;
	int cap_flag = 0;
	mac_label *mac = NULL;
	inf_label *inf = NULL;
	char *value;

	program = argv[0];

	for (i = 1; i < argc; i++) {
		/*
		 * No '-' means it's a file system name, and the option
		 * list is complete.
		 */
		if (argv[i][0] != '-')
			break;
		if (value = strchr(argv[i], '='))
			*value++ = '\0';
		/*
		 * Flag values.
		 */
		if (!strcmp(argv[i], "-create")) {
			if (!value || !strcmp(value, "true"))
				create_flag = 1;
			else if (!strcmp(value, "false"))
				create_flag = 0;
			else
				arg_error(argv[i], value);
			continue;
		}
		if (!strcmp(argv[i], "-apply")) {
			if (!value || !strcmp(value, "true"))
				apply_flag = 1;
			else if (!strcmp(value, "false"))
				apply_flag = 0;
			else
				arg_error(argv[i], value);
			continue;
		}
		if (!strcmp(argv[i], "-exists")) {
			if (!value || !strcmp(value, "true") ||
			    !strcmp(value, "ok"))
				existok_flag = 1;
			else if (!strcmp(value, "false"))
				existok_flag = 0;
			else
				arg_error(argv[i], value);
			continue;
		}
		/*
		 * Database types.
		 */
		if (!strcmp(argv[i], "-acl")) {
			acl_flag = 1;
			if (value) {
				fprintf(stderr,
				    "%s: Default ACL inappropriate\n",
				    program);
				arg_error(argv[i], value);
			}
			continue;
		}
		if (!strcmp(argv[i], "-capabilities")) {
			cap_flag = 1;
			if (value) {
				fprintf(stderr,
				    "%s: Default capability inappropriate\n",
				    program);
				arg_error(argv[i], value);
			}
			continue;
		}
		if (!strcmp(argv[i], "-inf")) {
			if (!value)
				inf = &inf_def_label;
			else if ((inf = inf_strtolabel(value)) == NULL) 
				arg_error(argv[i], value);
			/*
			 * The binary version of the label must fit in a
			 * "direct" entry.
			 */
			if (inf_size(inf) > DIRECT_LABEL_MAX) {
				fprintf(stderr,
				    "%s: information label %s is too big\n",
				    program, value);
				exit(1);
			}
			continue;
		}
		if (!strcmp(argv[i], "-mac")) {
			if (!value)
				mac = &mac_def_label;
			else if ((mac = mac_strtolabel(value)) == NULL) 
				arg_error(argv[i], value);
			/*
			 * The binary version of the label must fit in a
			 * "direct" entry.
			 */
			if (mac_size(mac) > DIRECT_LABEL_MAX) {
				fprintf(stderr, "%s: MAC label %s is too big\n",
				    program, value);
				exit(1);
			}
			continue;
		}
		/*
		 * Not something we know about.
		 */
		fprintf(stderr, "%s: unknown argument \"%s\".\n",
		    program, argv[i]);
		exit(1);
	}

	/*
	 * Now loop through the desired mount points.
	 */
	for (; i < argc; i++) {
		if (create_flag) {
			if (mac && (create_mac_files(argv[i],mac,existok_flag)))
				exit(1);
			if (inf && (create_inf_files(argv[i],inf,existok_flag)))
				exit(1);
			if (acl_flag && create_acl_files(argv[i],existok_flag))
				exit(1);
			if (cap_flag &&
			    create_cap_files(argv[i],existok_flag))
				exit(1);
		}
		if (apply_flag && syssgi(SGI_LOADATTR, argv[i]) < 0) {
			perror(argv[i]);
			fprintf(stderr, "%s: database attachment failed\n",
			    program);
			exit(1);
		}
	}

	exit(0);
}
