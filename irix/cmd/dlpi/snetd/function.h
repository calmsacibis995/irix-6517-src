#ident "@(#)function.h	1.6	11/13/92"
/*
 *  SpiderTCP Network Daemon
 *
 *      Control Interface Function definitions
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.function.h
 *	@(#)function.h	1.6
 *
 *	Last delta created	17:06:09 2/4/91
 *	This file extracted	14:52:24 11/13/92
 *
 */

struct function		/* control action description */
{
	char	*fname;			/* action name */
	int	(*fcall)();		/* function call */
};

typedef struct function	*function_t;	/* action description pointer */

#define F_NULL	((struct function *) 0)

extern struct function	cftab[];	/* control function table */

extern function_t flookup();		/* function table lookup */
