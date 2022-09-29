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
 * process_db - edit a text database file.
 */

#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <sys/stat.h>
#include "dbedit.h"

/*
 * Allocate a new line structure
 */
line_t *
newline(int state)
{
	line_t *gnu;

	gnu = (line_t *)malloc(sizeof(line_t));
	gnu->l_prev = NULL;
	gnu->l_next = NULL;
	gnu->l_state = state;
	gnu->l_change_from = 0;
	gnu->l_change_to = 0;
	gnu->l_text[0] = '\0';
	return (gnu);
}

/*
 * Return the previous line which has not been deleted.
 * If they all have been, return NULL.
 */
static line_t *
prev_not_deleted(line_t *lp)
{
	if (lp == NULL || lp->l_prev == NULL)
		return (NULL);

	do {
		lp = lp->l_prev;
		if ((lp->l_state & L_DELETED) == 0)
			return (lp);
	} while (lp->l_prev != NULL);

	return (NULL);
}

/*
 * Return the next line which has not been deleted.
 * If they all have been, return NULL.
 */
static line_t *
next_not_deleted(line_t *lp)
{
	if (lp == NULL || lp->l_next == NULL)
		return (NULL);

	do {
		lp = lp->l_next;
		if ((lp->l_state & L_DELETED) == 0)
			return (lp);
	} while (lp->l_next != NULL);

	return (NULL);
}

line_t *
process_db(line_t *head, char *dbname)
{

	char *cp;
	char command[256];	/* command buffer */
	line_t *current = head;	/* current element of the line list */
	line_t *temp;
	line_t yank;		/* yank buffer */
	int i;
	int done = 0;
	int chid = 0;		/* Change # */

	yank.l_state = L_YANKED;
	yank.l_next = NULL;

	command[0] = '.';
	do {
		switch(command[0]) {
		case 'f':
			/*
			 * Print the database name.
			 */
			printf("Database is \"%s\"\n", dbname);
			break;
		case 's':
			/*
			 * Print a change summary.
			 */
			change_summary(head);
			break;
		case 'y':
			/*
			 * 'Yank' this line into a buffer
			 */
			yank = *current;
			yank.l_state = L_NEW;
			yank.l_change_from = 0;
			yank.l_change_to = 0;
			break;
		case 'p':
			if (yank.l_state == L_YANKED) {
				printf("No buffer Yanked. 'p' Ignored\n");
				break;
			}
			/*
			 * Append the yank buffer after this line.
			 */
			temp = newline(L_NEW);
			*temp = yank;

			if (current->l_next)
				current->l_next->l_prev = temp;
			temp->l_prev = current;
			temp->l_next = current->l_next;
			current->l_next = temp;
			current = temp;
			break;
		case 'a':
			/*
			 * Append after this line.
			 */
			temp = newline(L_NEW);
			if (fgets(temp->l_text, 256, stdin) == NULL)
				break;
			if ((cp = index(temp->l_text, '\n')) != NULL)
				*cp = '\0';
			if (current->l_next)
				current->l_next->l_prev = temp;
			temp->l_prev = current;
			temp->l_next = current->l_next;
			current->l_next = temp;
			current = temp;
			break;
		case 'i':
			/*
			 * Insert before this line.
			 */
			temp = newline(L_NEW);
			if (fgets(temp->l_text, 256, stdin) == NULL)
				break;
			if ((cp = index(temp->l_text, '\n')) != NULL)
				*cp = '\0';
			if (current == head) {
				temp->l_next = head;
				head->l_prev = temp;
				head = temp;
			}
			else {
				temp->l_prev = current->l_prev;
				temp->l_next = current;
				current->l_prev->l_next = temp;
				current->l_prev = temp;
			}
			current = temp;
			break;
		case 'c':
			/*
			 * Change this line.
			 */
			current->l_state |= L_DELETED | L_CHANGED_FROM;
			temp = newline(L_NEW | L_CHANGED_TO);
			chid++;
			current->l_change_from = chid;
			temp->l_change_to = chid;

			if (fgets(temp->l_text, 256, stdin) == NULL)
				break;
			if ((cp = index(temp->l_text, '\n')) != NULL)
				*cp = '\0';
			if (current->l_next)
				current->l_next->l_prev = temp;
			temp->l_prev = current;
			temp->l_next = current->l_next;
			current->l_next = temp;
			current = temp;
			break;
		case 'd':
			/*
			 * Mark this line deleted.
			 */
			current->l_state |= L_DELETED;
			if ((temp = next_not_deleted(current)) == NULL) {
				if ((temp=prev_not_deleted(current)) == NULL) {
					temp = newline(L_DUMMY);
					temp->l_prev = current;
					temp->l_next = current->l_next;
					if (current->l_next)
						current->l_next->l_prev = temp;
					current->l_next = temp;
				}
			}
			current = temp;
			break;
		case '\n':
			/*
			 * Next Line.
			 */
			if ((temp = next_not_deleted(current)) != NULL)
				current = temp;
			break;
		case '.':
			/*
			 * This Line.
			 */
			break;
		case '-':
			/*
			 * Previous Line.
			 */
			if ((temp = prev_not_deleted(current)) != NULL)
				current = temp;
			break;
		case '1':
			/*
			 * First Line.
			 */
			current = head;
			if (head->l_state & L_DELETED)
				current = next_not_deleted(head);
			break;
		case 'q':
			/*
			 * Done processing.
			 */
			done = 1;
			break;
		case '$':
			/*
			 * Last Line.
			 */
			while (current->l_next)
				current = current->l_next;
			if (current->l_state & L_DELETED)
				if ((temp = prev_not_deleted(current)) != NULL)
					current = temp;
			break;
		}
		/*
		 * Shortcut out for any of the done conditions
		 */
		if (done) 
			break;
		/*
		 * Print the prompt
		 */
		printf("%s > ", current->l_text);
	} while (fgets(command, 256, stdin) != NULL);

	return(head);
}
