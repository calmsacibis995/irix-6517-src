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
 * read_db - read a text database file.
 */

#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mac.h>
#include "dbedit.h"

extern int mac_enabled;

/*
 * Build a list of line structures from the desired path.
 */
line_t *
read_db(char *path)
{
	struct stat statbuf;	/* to stat(2) into */
	mac_t label;		/* to mac_get_path into */
	char *cp;
	char command[256];	/* command buffer */
	line_t *head;		/* head of the line list */
	line_t *current;	/* current element of the line list */
	FILE *fp;
	int i;

	/*
	 * Get the information about the file.
	 */
	if (stat(path, &statbuf) < 0) {
		perror(path);
		return (NULL);
	}
	if (mac_enabled && (label = mac_get_file(path)) == NULL) {
		perror(path);
		return (NULL);
	}

	/*
	 * Print the information.
	 */
	printf("\n%s: uid=%d, gid=%d, mode=%04o", path, statbuf.st_uid,
	    statbuf.st_gid, statbuf.st_mode & 07777);
	if (mac_enabled) {
		if (cp = mac_to_text(label, NULL)) {
			printf(" MAC=\"%s\"", cp);
			free(cp);
		}
		else
			printf(" MAC=\"[Unknown?]\"");
	}
	printf("\n\n");


	/*
	 * Read the database into memory.
	 */
	if ((fp = fopen(path, "r")) == NULL) {
		sprintf(command, "Cannot open \"%s\"", path);
		perror(command);
		return (NULL);
	}
	head = newline(L_ORIGINAL);
	current = head;

	if (statbuf.st_size == 0) {
		head->l_text[0] = '\n';
	}
	else if (fgets(head->l_text, 256, fp) == NULL) {
		sprintf(command, "Cannot read \"%s\"", path);
		perror(command);
		fclose(fp);
		return (NULL);
	}

	do {
		if ((cp = index(current->l_text, '\n')) != NULL)
			*cp = '\0';
		current->l_next = newline(L_ORIGINAL);
		current->l_next->l_prev = current;
		current = current->l_next;
		current->l_next = NULL;
	} while (fgets(current->l_text, 256, fp) != NULL);
	current = current->l_prev;
	free(current->l_next);
	current->l_next = NULL;

	return (head);
}
