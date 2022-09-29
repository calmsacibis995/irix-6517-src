/*
 *  SpiderTCP Network Daemon
 *
 *      Control Interface Function Table
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.control.c
 *	@(#)control.c	1.22
 *
 *	Last delta created	13:07:18 4/14/92
 *	This file extracted	14:52:21 11/13/92
 */


#ident "@(#)control.c	1.22	11/13/92"

#include "function.h"

extern int	cf_frs_setsnid();
extern int	cf_shell();
extern int	cf_ll_set_snid();
extern int	cf_x25_set_snid();
extern int	cf_echotype();
extern int	cf_ipnet();


struct function	cftab[] =		/* control function table */
{
    { "SHELL",        cf_shell },
    { "FRS_SET_SNID", cf_frs_setsnid },
    { "LL_SET_SNID",  cf_ll_set_snid },    
    { "X25_SET_SNID", cf_x25_set_snid },    
    { "ECHO_TYPE",    cf_echotype },
    { "IP_NET",       cf_ipnet },
    { 0, 0 }
};
