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
 * #ident "$Revision: 1.2 $"
 *
 * macplanged - directly edit an existing Plan G database.
 *
 *
 * macplanged label path
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <strings.h>
#include <mls.h>

#define	BUFFERS	512
	
main(argc, argv)
	int argc;
	char *argv[];
{
	eag_mac_index_t *indexs;	/* The in-core index file */
	eag_mac_index_t element;	/* Value of the index file entry */
	char *path;			/* File to update */
	char *cp;
	char index_file[PATH_MAX];	/* Name of the index file */
	char index_tmp_file[PATH_MAX];	/* Name of the index tmp file */
	char index_old_file[PATH_MAX];	/* Name of the index old file */
	char label_file[PATH_MAX];	/* Name of the label file */
	FILE *index_fp;			/* the index file */
	FILE *index_tmp_fp;		/* the index tmp file */
	FILE *label_fp;			/* the label file */
	FILE *fp;			/* For popen of df(1) */
	struct stat statbuf;		/* return from stat(2) */
	char buffer[BUFFERS];		/* Read buffer */
	int inode;			/* inode # */
	mac_label *lp;			/* Binary value of label */
	int i;

	if (argc != 3) {
		fprintf(stderr, "macplanged: Usage: macplanged label path\n");
		exit(1);
	}

	if ((lp = mac_strtolabel(argv[1])) == NULL) {
		fprintf(stderr, "macplanged: label is invalid.\n");
		exit(1);
	}

	path = argv[2];
	/*
	 * Get the inode number of the file.
	 */
	if (lstat(path, &statbuf) == -1) {
		perror("macplanged: stat");
		exit(1);
	}
	inode = statbuf.st_ino;
	
	/*
	 * Invoke df(1) as the most convenient way to get the name of
	 * the file system upon which the file resides.
	 * NOTE: This code assumes an unchanging format of output from
	 * df(1). There may come a time when this assumption turns out
	 * to be inappropriate.
	 */
	sprintf(buffer, "/bin/df %s", path);
	if ((fp = popen(buffer, "r")) == NULL) {
		perror("macplanged: popen");
		exit(1);
	}
	/*
	 * Toss aside the first line, as it's the header.
	 */
	if (fgets(buffer, BUFFERS, fp) == NULL) {
		perror("macplanged: initial fgets");
		exit(1);
	}
	/*
	 * There'd better be a second line.
	 */
	if (fgets(buffer, BUFFERS, fp) == NULL) {
		perror("macplanged: initial fgets");
		exit(1);
	}
	pclose(fp);
	/*
	 * If the file system is mounted by NFS then we don't want to do
	 * this.
	 */
	if ((cp = strtok(buffer, "\n\t ")) == NULL) {
		fprintf(stderr, "macplanged: output from df mangled\n");
		exit(1);
	}
	if ((cp = strtok(NULL, "\n\t ")) == NULL) {
		fprintf(stderr, "macplanged: output from df mangled\n");
		exit(1);
	}
	if (strncmp(cp, "efs", 3) != 0) {
		fprintf(stderr,
		    "macplanged: Will only update EFS file systems\n");
		exit(1);
	}
	/*
	 * Skip 'blocks' field
	 */
	if ((cp = strtok(NULL, "\n\t ")) == NULL) {
		fprintf(stderr, "macplanged: output from df mangled\n");
		exit(1);
	}
	/*
	 * Skip 'use' field
	 */
	if ((cp = strtok(NULL, "\n\t ")) == NULL) {
		fprintf(stderr, "macplanged: output from df mangled\n");
		exit(1);
	}
	/*
	 * Skip 'avail' field
	 */
	if ((cp = strtok(NULL, "\n\t ")) == NULL) {
		fprintf(stderr, "macplanged: output from df mangled\n");
		exit(1);
	}
	/*
	 * Skip '%use' field
	 */
	if ((cp = strtok(NULL, "\n\t ")) == NULL) {
		fprintf(stderr, "macplanged: output from df mangled\n");
		exit(1);
	}
	/*
	 * Get to 'Mounted on' field
	 */
	if ((cp = strtok(NULL, "\n\t ")) == NULL) {
		fprintf(stderr, "macplanged: output from df mangled\n");
		exit(1);
	}

	/*
	 * Open the index file and a temporary to copy it into.
	 */
	sprintf(label_file, "%s/%s", cp, EAG_MAC_LABEL);
	sprintf(index_file, "%s/%s", cp, EAG_MAC_INDEX);
	sprintf(index_tmp_file, "%s/%s.TMP", cp, EAG_MAC_INDEX);
	sprintf(index_old_file, "%s/%s.OLD", cp, EAG_MAC_INDEX);
	if ((index_fp = fopen(index_file, "r")) == NULL) {
		perror("macplanged: opening index file");
		exit(1);
	}
	if ((index_tmp_fp = fopen(index_tmp_file, "w")) == NULL) {
		perror("macplanged: opening index tmp file");
		exit(1);
	}
	/*
	 * Copy the preceeding parts of the index file.
	 */
	for (i = 0; i < inode; i++) {
		if (fread(&element, sizeof(element), 1, index_fp) < 0) {
			fprintf(stderr, "macplanged: premature EOF.\n");
			exit(1);
		}
		if (fwrite(&element,sizeof(element),1,index_tmp_fp) < 0) {
			fprintf(stderr, "macplanged: Write failure.\n");
			exit(1);
		}
	}
	/*
	 * Read in the current value for this record.
	 */
	if (fread(&element, sizeof(element), 1, index_fp) < 0) {
		fprintf(stderr, "macplanged: premature EOF.\n");
		exit(1);
	}
	if ((i = mac_size(lp)) <= 2 * sizeof(int)) {
		/*
		 * If the new value fits in a direct entry do so.
		 */
		bcopy(lp, &element, i);
		element.emi_flags = EAG_MAC_DIRECT;
	}
	else {
		element.emi_size = i;
		element.emi_flags = 0;
		element.emi_index = -1;
		/*
		 * Since the new value does not fit in a direct entry
		 * look for its value in the labels file, adding it if
		 * it's not there.
		 */
		if ((label_fp = fopen(label_file, "r")) == NULL) {
			perror("macplanged: opening label file");
			exit(1);
		}
		while (fread(&i, sizeof(int), 1, label_fp) > 0) {
			element.emi_index = ftell(label_fp) - sizeof(int);
			if (i > BUFFERS) {
				fprintf(stderr,
				    "macplanged: oversized label entry.\n");
				exit(1);
			}
			if (fread(buffer, i, 1, label_fp) < 0) {
				perror("macplanged: reading label file");
				exit(1);
			}
			/*
			 * If the label is found then store the index and
			 * exit the loop.
			 */
			if (i != element.emi_size ||
			    bcmp(lp, buffer, element.emi_size) != 0)
				element.emi_index = -1;
		}
		/*
		 * If the label is not already in the labels file append it.
		 */
		if (element.emi_index == -1) {
			element.emi_index = ftell(label_fp);
			fclose(label_fp);
			if ((label_fp = fopen(label_file, "a")) == NULL) {
				perror("macplanged: appending label file");
				exit(1);
			}
			if (fwrite(&element.emi_size, sizeof(int), 1,
			    label_fp) < 0) {
				perror("macplanged: Append failure");
				exit(1);
			}
			if (fwrite(lp, element.emi_size, 1, label_fp) < 0) {
				perror("macplanged: Append failure");
				exit(1);
			}
		}
		fclose(label_fp);
	}
	if (fwrite(&element,sizeof(element),1,index_tmp_fp) < 0) {
		fprintf(stderr, "macplanged: Write failure.\n");
		exit(1);
	}
	/*
	 * Copy the subsequent parts of the index file.
	 */
	while (fread(&element, sizeof(element), 1, index_fp) > 0) {
		if (fwrite(&element,sizeof(element),1,index_tmp_fp) < 0) {
			fprintf(stderr, "macplanged: Write failure.\n");
			exit(1);
		}
	}
	if (fclose(index_fp) < 0) {
		perror("macplanged: closing index file");
		exit(1);
	}
	if (fclose(index_tmp_fp) < 0) {
		perror("macplanged: closing index tmp file");
		exit(1);
	}
	/*
	 * Move the old file away and the new in.
	 */
	if (rename(index_file, index_old_file) < 0) {
		perror("macplanged: saving old index file");
		exit(1);
	}
	if (rename(index_tmp_file, index_file) < 0) {
		perror("macplanged: establishing new index file");
		exit(1);
	}
	exit(0);
}
