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
#include "dbedit.h"

void
change_summary(line_t *head)
{
	line_t *current;	/* current element of the line list */
	line_t *lp;		/* to find the other chid part */
	int ccount = 0;		/* change count */
	int chid;

	/*
	 * Report the Changes.
	 * First the Deletions.
	 */
	for (current = head; current; current = current->l_next) {
		if ((current->l_state & L_DELETED) &&
		    !(current->l_state & L_NEW) &&
		    !(current->l_state & L_CHANGED_FROM)) {
			printf("D  :%s\n", current->l_text);
			ccount++;
		}
	}
	/*
	 * Then the additions.
	 */
	for (current = head; current; current = current->l_next) {
		if (!(current->l_state & L_DELETED) &&
		    !(current->l_state & L_CHANGED_TO) &&
		    (current->l_state & L_NEW)) {
			printf("A  :%s\n", current->l_text);
			ccount++;
		}
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
			printf("D  :%s\n", current->l_text);
		else {
			printf("Was:%s\n", current->l_text);
			printf("Is :%s\n", lp->l_text);
		}
		ccount++;
	}

	if (ccount == 0)
		printf("No changes.\n");
	return;
}
