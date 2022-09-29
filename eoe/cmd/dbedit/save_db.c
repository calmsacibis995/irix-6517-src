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
 * #ident "$Revision: 1.3 $"
 *
 * save_db - save text database file.
 */

#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <sys/stat.h>
#include "dbedit.h"

/*
 * Free the line list.
 */
static void
free_lines(line_t *head)
{
	line_t *current;	/* current element of the line list */

	while (head) {
		current = head;
		head = head->l_next;
		free(current);
	}
}

#define PROMPT	"Do you wish to save these changes (yes, no, or summary): "

void
save_db(line_t *head, char *path)
{
	struct stat statbuf;	/* to stat(2) into */
	char dotnew[256];	/* dotnew buffer */
	char dotold[256];	/* dotnew buffer */
	line_t *current;	/* current element of the line list */
	line_t *lp;		/* to find the other chid part */
	FILE *fp;
	int i;

	/*
	 * Ask the user if changes should be saved or tossed.
	 */
	for (i = confirm(PROMPT, "yYnNsS");
	    i != 'y' && i != 'Y';
	    i = confirm(PROMPT, "yYnNsS")) {
		switch (i) {
		case 'n':
		case 'N':
			free_lines(head);
			return;
		case 's':
		case 'S':
			change_summary(head);
		}
	}

	/*
	 * Save the changes.
	 * Create a file like the old one.
	 */
	sprintf(dotnew, "%s.NEW", path);
	sprintf(dotold, "%s.OLD", path);

	if ((fp = fopen(dotnew, "w")) == NULL) {
		perror(dotnew);
		return;
	}

	/*
	 * Write each non-deleted line, in order.
	 */
	for (current = head; current; current = current->l_next)
		if (!(current->l_state & L_DELETED))
			if (fprintf(fp, "%s\n", current->l_text) < 0) {
				perror("fprintf");
				fclose(fp);
				return;
			}

	if (stat(path, &statbuf) < 0) {
		perror(path);
		return;
	}
	if (rename(path, dotold) < 0) {
		perror(dotold);
		return;
	}
	if (rename(dotnew, path) < 0) {
		perror(dotnew);
		return;
	}
	if (fchmod(fileno(fp), statbuf.st_mode & 0777) < 0) {
		perror(path);
		return;
	}
	if (fchown(fileno(fp), statbuf.st_uid, statbuf.st_gid) < 0) {
		perror(path);
		return;
	}

	if (fclose(fp) < 0)
		perror(path);

	return;
}
