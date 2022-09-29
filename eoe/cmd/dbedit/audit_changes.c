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
 * audit_changes - audit the fact of the changes.
 */

#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <sat.h>
#include "dbedit.h"

static void
write_record(char *db, char *op, char *arg1, char *arg2)
{
	char buffer[512];	/* Buffer to compose record into */

	switch (*op) {
	case 'a' :
	case 'd' :
		sprintf(buffer, "%s %s\n%s", db, op, arg1);
		break;
	case 'c' :
		sprintf(buffer, "%s %s\n%s\n%s", db, op, arg1, arg2);
		break;
	default:
		/*
		 * Programming error
		 */
		fprintf(stderr, "write_record programming error\n");
		exit(2);
	}
	if (satwrite(SAT_AE_DBEDIT, SAT_SUCCESS, buffer, strlen(buffer)) < 0) {
		perror("Cannot write audit records");
		exit(2);
	}
}

void
audit_changes(line_t *head, char *name)
{
	line_t *current;	/* current element of the line list */
	line_t *lp;		/* to find the other chid part */
	int chid;

	/*
	 * Report the Changes.
	 * First the Deletions.
	 */
	for (current = head; current; current = current->l_next) {
		if ((current->l_state & L_DELETED) &&
		    !(current->l_state & L_NEW) &&
		    !(current->l_state & L_CHANGED_FROM))
			write_record(name, "deleted", current->l_text, NULL);
	}
	/*
	 * Then the additions.
	 */
	for (current = head; current; current = current->l_next) {
		if (!(current->l_state & L_DELETED) &&
		    !(current->l_state & L_CHANGED_TO) &&
		    (current->l_state & L_NEW))
			write_record(name, "added", current->l_text, NULL);
	}
	/*
	 * Finally the changes.
	 */
	for (current = head; current; current = current->l_next) {
		if (!(current->l_state & L_CHANGED_FROM))
			continue;
		if (current->l_state & L_NEW)
			continue;
		/*
		 * Now track down the sum of changes.
		 */
		chid = current->l_change_from;
		lp = head;

		while (lp) {
			if (lp->l_change_to == chid) {
				if (lp->l_change_from == 0)
					break;
				chid = lp->l_change_from;
				lp = head;
				continue;
			}
			lp = lp->l_next;
		}

		if (lp->l_state & L_DELETED)
			write_record(name, "deleted", current->l_text, NULL);
		else {
			write_record(name, "changed", current->l_text,
			    lp->l_text);
		}
	}
	return;
}
