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
 */

/*
 * List structure for processing lines.
 */
struct line_s {
	struct line_s	*l_next;	/* Next line */
	struct line_s	*l_prev;	/* Previous line */
	char		l_text[256];	/* text of the line */
	int		l_state;	/* line state */
	int		l_change_from;	/* Change ID number */
	int		l_change_to;	/* Change ID number */
};
typedef struct line_s line_t;

#define	L_ORIGINAL	0x0	/* Original line as read */
#define	L_NEW		0x1	/* Added to the file */
#define	L_DELETED	0x2	/* Deleted from the file */
#define	L_CHANGED_FROM	0x4	/* Changed From */
#define	L_CHANGED_TO	0x8	/* Changed To */
#define	L_DUMMY		0x10	/* Not really a data line in the file */
#define	L_YANKED	0x20	/* An empty yank buffer */

#define DBEDIT_CONF	"/etc/dbedit.conf"

/*
 * Local function prototypes
 */
line_t *newline(int state);
line_t *read_db(char *path);
line_t *process_db(line_t *head, char *dbname);
void save_db(line_t *head, char *path);
void audit_changes(line_t *head, char *name);
void change_summary(line_t *head);
int confirm(char *prompt, char *choices);
char *find_database(char *);
