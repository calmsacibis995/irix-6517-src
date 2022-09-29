/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.8 $"


#include <sys/dmi_kern.h>


extern int
dm_get_config(
	void		*hanp,
	size_t		hlen,
	dm_config_t	flagname,
	dm_size_t	*retvalp)
{
	return dmi(DM_GET_CONFIG, hanp, hlen, flagname, retvalp);
}


extern int
dm_get_config_events(
	void		*hanp,
	size_t		hlen,
	u_int		nelem,
	dm_eventset_t	*eventsetp,
	u_int		*nelemp)
{
	return dmi(DM_GET_CONFIG_EVENTS, hanp, hlen, nelem, eventsetp, nelemp);
}
