#ident "@(#)utils.h	1.6	11/13/92"
/*
 *  SpiderTCP Network Daemon
 *
 *      Utility Definitions
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.utils.h
 *	@(#)utils.h	1.6
 *
 *	Last delta created	17:06:20 2/4/91
 *	This file extracted	14:52:28 11/13/92
 *
 */

#define streq(s1, s2)	(strcmp((s1), (s2)) == 0)

extern char	*memory(), *mfree();
extern char	*array();
extern char	*copy();

struct list		/* generic linked list type */
{
	char		*data;		/* data pointer */
	struct list	*next;		/* next link in list */
};

typedef struct list	*list_t;	/* linked list pointer */

#define L_NULL	((struct list *) 0)

extern list_t	mklist();
