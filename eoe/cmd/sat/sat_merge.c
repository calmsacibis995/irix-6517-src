/*
 * sat_merge.c - Combine multiple audit files into one
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.2 $"

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <values.h>
#include <malloc.h>
#include <sat.h>

int file_major;
int file_minor;

char	*pname;			/* program name */
int	debug = 0;		/* debuging option */

void merge_file_headers(struct sat_file_info *, struct sat_file_info *,
	FILE **, char **, int);
void merge_entries(const char *, struct sat_list_ent ***, int,
	struct sat_list_ent ***, int *);
void dump_entries(const char *, struct sat_list_ent ***, int,
	struct sat_list_ent **);

main (int argc, char **argv) {

	struct sat_file_info *fhead;	/* array of file headers */
	struct sat_hdr_info *head;	/* array of record headers */
	FILE **file;			/* array of file descriptors */
	char **filename;		/* array of file names */
	int nf;				/* size of all above arrays */

	int c;				/* for getopt */
	int i;

	static struct sat_file_info mergedfh;
	FILE *out;
	char *outfile = NULL;

	pname = argv[0];

		/* parse command line options	*/

	while ((c = getopt(argc, argv, "do:")) != EOF)
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'o':
			outfile = optarg;
			break;
		default:
			fprintf(stderr, "usage: %s [-d] filename ...\n",
			    pname);
			exit(1);
		}

	if (optind == argc) {
		fprintf(stderr, "%s: No filenames specified.\n", pname);
		exit(1);
	}

	if (outfile != NULL) {
		out = fopen(outfile, "w");
		if (out == NULL) {
			fprintf(stderr, "%s: Can't open output file ", pname);
			perror(outfile);
			exit(1);
		}
	} else {
		outfile = "stdout";
		out = stdout;
	}

		/* allocate arrays */

	file = malloc((argc - optind) * sizeof(FILE *));
	filename = malloc((argc - optind) * sizeof(char **));
	fhead = malloc((argc - optind) * sizeof(struct sat_file_info));
	head = malloc((argc - optind) * sizeof(struct sat_hdr_info));
	
	if (file == NULL || filename == NULL || fhead == NULL || head == NULL) {
		fprintf(stderr, "%s: Out of memory\n", pname);
		exit(1);
	}

		/* Check files on command line */

	nf = 0;
	while (optind < argc) {

			/* Open file */

		filename[nf] = argv[optind];
		if ((file[nf] = fopen(filename[nf], "r")) == NULL) {
			fprintf(stderr, "%s: Can't open %s: ",
			     pname, filename[nf]);
			perror(NULL);
			exit(1);
		}

			/* Read file header */

		if (sat_read_file_info(file[nf],NULL,&fhead[nf],SFI_ALL) !=
		    SFI_OKAY) {
			fprintf(stderr, "%s: Can't read file header of %s\n",
				pname, filename[nf]);
			sat_free_file_info(&fhead[nf]);
			exit(1);
		}
		       /*
			* Validate header.  The first file is checked against
			* this version of sat_merge, remaining files are checked
			* against first file.  We are more lienient with 
			* version 2 files.
			*/

		if (nf == 0) {
			file_major = fhead[nf].sat_major;
			file_minor = fhead[nf].sat_minor;
			if ((file_major == 2) && (SAT_VERSION_MAJOR == 2))
				NULL;
			else if ((file_major != SAT_VERSION_MAJOR) ||
			    (file_minor != SAT_VERSION_MINOR)) {
				fprintf(stderr,
					"%s: %s is version %d.%d, can only "
					"handle version %d.%d\n",
					pname, filename[i],
					file_major, file_minor,
					SAT_VERSION_MAJOR, SAT_VERSION_MINOR);
				exit(1);
			} 
		} else {
			if ((fhead[nf].sat_major != file_major) ||
			    (fhead[nf].sat_minor != file_minor)) {
				fprintf(stderr, "%s: merged files must"
					" be the same version,"
					" %s is version %d.%d\n",
					fhead[nf].sat_major,
					fhead[nf].sat_minor);
				exit(1);
			}
		}

			/* Next */

		optind++;
		nf++;
	}

	if (nf < 2) {
		fprintf(stderr, "%s: Need at least two files to merge.\n",
		    pname);
		exit(1);
	}

	       /*
		* We've read all the headers, now merge all the fields into one.
		* Things to check : major, minor, TZ, start and end times,
		* and mac_enabled all the same (or compatible).
		*/

	merge_file_headers(&mergedfh, fhead, file, filename, nf);

		/* write out the merged file header */

	if (sat_write_file_info(out, &mergedfh) != SFI_OKAY) {
		fprintf(stderr,"Failed to merged header\n");
		exit(1);
	}

	       /*
		* Now merge the records from each file into a single stream,
		* ordered by time.
		*/
	
		/* read a record header from each file (put hostid in header).*/

	for (i=0; i < nf; i++) {
		if (sat_read_header_info(file[i], &head[i], SHI_BUFFER,
			SAT_VERSION_MAJOR, SAT_VERSION_MINOR) != SFI_OKAY) {
			if (ferror(file[i])) {
				fprintf(stderr, "%s: Error reading ", pname);
				perror(filename[i]);
			}
			if (debug)
				fprintf(stderr, "debug: done %s\n", filename[i]);
			fclose(file[i]);
			file[i] = NULL;
		} else {
			struct sat_hdr *h =
				(struct sat_hdr *)head[i].sat_buffer;
			h->sat_host_id = fhead[i].sat_host_id;
		}
	}

	       /*
	 	* While there are records in any file, find the one with the
	 	* smallest time value, and write it out.
	 	*/

	for (;;) {
		time_t	mintime;
		int	minticks;
		int	ent;
		char	*body = NULL;
		int	bodysize = 0;

		mintime = MAXINT;
		minticks = 99;
		for (i=0; i < nf; i++) {
			if (file[i] == NULL)
				continue;
			if (mintime > head[i].sat_time ||
				(mintime == head[i].sat_time &&
				 minticks > head[i].sat_ticks)) {
				mintime = head[i].sat_time;
				minticks = head[i].sat_ticks;
				ent = i;
			}
		}
		/*
		 * No min?  No more files.
		 */
		if (mintime == MAXINT)
			break;

		/*
		 * Otherwise, write this header and record, and get the
		 * next one.
		 */
		if (head[ent].sat_recsize > bodysize) {
			bodysize = head[ent].sat_recsize;
			body = realloc(body, head[ent].sat_recsize);
		}
		fwrite(head[ent].sat_buffer, head[ent].sat_hdrsize, 1, out);
		fread(body, head[ent].sat_recsize, 1, file[ent]);
		fwrite(body, head[ent].sat_recsize, 1, out);
		if (feof(out) || ferror(out)) {
			fprintf(stderr, "%s: Error writing ", pname);
			perror(outfile);
			exit(1);
		}

		if (sat_read_header_info(file[ent], &head[ent], SHI_BUFFER,
			SAT_VERSION_MAJOR, SAT_VERSION_MINOR) != SFI_OKAY) {
			if (ferror(file[ent])) {
				fprintf(stderr, "%s: Error reading ", pname);
				perror(filename[ent]);
			}
			if (debug)
				fprintf(stderr, "debug: done %s\n", filename[ent]);
			fclose(file[ent]);
			file[ent] = NULL;
		} else {
			struct sat_hdr *h =
				(struct sat_hdr *)head[ent].sat_buffer;
			h->sat_host_id = fhead[ent].sat_host_id;
		}
	}

	fclose(out);
}


/*
 * Merge each file header into one common file header.
 */
void
merge_file_headers(struct sat_file_info *merged, struct sat_file_info *fhead,
	FILE **file, char **filename, int nf)
{
	int		i;
	struct sat_list_ent ***entries;

	/* initialize merged file header */

	merged->sat_major = file_major;
	merged->sat_minor = file_minor;
	merged->sat_start_time = fhead[0].sat_start_time;
	merged->sat_stop_time = fhead[0].sat_stop_time;
	merged->sat_host_id = 0;
	merged->sat_mac_enabled = fhead[0].sat_mac_enabled;
	merged->sat_cap_enabled = fhead[0].sat_cap_enabled;
	merged->sat_timezone = fhead[0].sat_timezone;
	merged->sat_hostname = NULL;
	merged->sat_domainname = NULL;
	merged->sat_buffer = NULL;
	merged->sat_user_entries = 0;
	merged->sat_group_entries = 0;
	merged->sat_host_entries = 0;

	/* check for incompatibilities; merge start and stop times */
        /* Note:  the check for mac_enabled, and timezone should be
           moved to the file validation loop */

	for (i = 0; i < nf; i++) {

		if (fhead[i].sat_mac_enabled != merged->sat_mac_enabled) {
			fprintf(stderr, "%s: all files must have the same "
				"mac_enabled setting\n", pname);
			exit(1);
		}
		if (fhead[i].sat_cap_enabled != merged->sat_cap_enabled) {
			fprintf(stderr, "%s: all files must have the same "
				"cap_enabled setting\n", pname);
			exit(1);
		}
		if (strcmp(fhead[i].sat_timezone, merged->sat_timezone) != 0) {
			fprintf(stderr, "%s: warning: timezones are not all "
				"the same -- using GMT.\n", pname);
			merged->sat_timezone = "TZ=GMT";
		}
		if (merged->sat_start_time > fhead[i].sat_start_time) {
			merged->sat_start_time = fhead[i].sat_start_time;
		}
		if (merged->sat_stop_time < fhead[i].sat_stop_time) {
			merged->sat_stop_time = fhead[i].sat_stop_time;
		}
		merged->sat_user_entries += fhead[i].sat_user_entries;
		merged->sat_group_entries += fhead[i].sat_group_entries;
		merged->sat_host_entries += fhead[i].sat_host_entries;
	}

	merged->sat_timezone += 3;	/* skip TZ= */
	merged->sat_users =
		malloc(sizeof(void **) * (merged->sat_user_entries + 1));
	merged->sat_groups =
		malloc(sizeof(void **) * (merged->sat_group_entries + 1));
	merged->sat_hosts =
		malloc(sizeof(void **) * (merged->sat_host_entries + 1));

	/* merge all the file header user/group/host entries */

	entries = malloc(nf * sizeof(void *));
	for (i=0; i < nf; i++)
		entries[i] = fhead[i].sat_users;
	merge_entries("user", entries, nf, &merged->sat_users,
			  &merged->sat_user_entries);
	for (i=0; i < nf; i++)
		entries[i] = fhead[i].sat_groups;
	merge_entries("group", entries, nf, &merged->sat_groups,
			  &merged->sat_group_entries);
	for (i=0; i < nf; i++)
		entries[i] = fhead[i].sat_hosts;
	merge_entries("host", entries, nf, &merged->sat_hosts,
			  &merged->sat_host_entries);
	free(entries);

	if (debug) {
		entries = malloc(nf * sizeof(void *));
		for (i=0; i < nf; i++) {
			entries[i] = fhead[i].sat_users;
			printf("user entries %d: %d\n", i,
				fhead[i].sat_user_entries);
		}
		printf("merged user entries: %d\n", merged->sat_user_entries);
		dump_entries("user", entries, nf, merged->sat_users);
		for (i=0; i < nf; i++) {
			entries[i] = fhead[i].sat_groups;
			printf("group entries %d: %d\n", i,
				fhead[i].sat_group_entries);
		}
		printf("merged group entries: %d\n", merged->sat_group_entries);
		dump_entries("group", entries, nf, merged->sat_groups);
		for (i=0; i < nf; i++) {
			entries[i] = fhead[i].sat_hosts;
			printf("host entries %d: %d\n", i,
				fhead[i].sat_host_entries);
		}
		printf("merged host entries: %d\n", merged->sat_host_entries);
		dump_entries("host", entries, nf, merged->sat_hosts);
		free(entries);
	}
}


/*
 * Take all the entries from each file header ('list') and
 * merge them into a sorted list in the new file header ('listp').
 * The entries in 'list' are already sorted.
 */
void
merge_entries(const char *entname, struct sat_list_ent ***list, int nlists,
	struct sat_list_ent ***listp, int *n_entp)
{
	int	i, j;
	int	ent = 0;
	int	min;
	int	listents = (*n_entp);
	struct sat_list_ent *prev = NULL;

	for (j=0; j < listents; j++) {
		min = MAXINT;
		for (i=0; i < nlists; i++) {
			if (prev && *list[i] &&
				(*list[i])->sat_id == prev->sat_id) {
				if (strcmp((*list[i])->sat_name.data,
						   prev->sat_name.data) != 0) {
					fprintf(stderr, "%s: warning: duplicate "
					"%s with different names:\n\t%s was "
					"used for %s %d, %s ignored.\n",
					pname, entname, prev->sat_name.data,
					entname, prev->sat_id,
					(*list[i])->sat_name.data);
				}
				free(*list[i]);
				list[i]++;
			}
			if ((*list[i]) == NULL)
				continue;
			if (min > (*list[i])->sat_id) {
				min = (*list[i])->sat_id;
				ent = i;
			}
		}
		if (min == MAXINT) {	/* we're done */
			break;
		}
		(*listp)[j] = *list[ent];
		prev = *list[ent];
		list[ent]++;
	}
	(*listp)[j] = NULL;
	(*n_entp) = j;
}


/*
 * Dump the contents of the list of entries given, for debugging.
 */
void
dump_entries(const char *entname, struct sat_list_ent ***list, int nlists,
	struct sat_list_ent **listp)
{
	int	i;
	struct sat_list_ent **le;

	for (i=0; i < nlists; i++) {
		printf("%ss header %d:\n", entname, i);
		le = list[i];
		while (*le != NULL) {
			printf("	%-6d %s\n", (*le)->sat_id,
				(*le)->sat_name.data);
			le++;
		}
	}
	printf("%ss merged header:\n", entname, i);
	le = listp;
	while (*le != NULL) {
		printf("	%-6d %s\n", (*le)->sat_id, (*le)->sat_name.data);
		le++;
	}
}
