/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: phost_private.h,v 1.1 1996/09/19 18:13:29 cp Exp $"

#ifndef _PHOST_PRIVATE_H_
#define _PHOST_PRIVATE_H_       1

#include <ksys/vhost.h>

/*
 * Rather degenerate physical host - nothing but the behavior.
 */
typedef struct phost {
	bhv_desc_t	ph_bhv;
} phost_t;

#define PHOST_TO_BHV(p)		(&(p)->ph_bhv)
#define PHOST_TO_VHOST(p)	((vhost_t *) BHV_VOBJ(PHOST_TO_BHV(p)))

extern void	phost_create(vhost_t *);

/*
 * Protos for physical layer (what's not declared in <ksys/vhost.h>.
 */
extern void	phost_register(bhv_desc_t *, int);
extern void	phost_deregister(bhv_desc_t *, int);
extern void	phost_killall(bhv_desc_t *, int, pid_t, pid_t, pid_t);
extern void	phost_reboot(bhv_desc_t *, int, char *);
extern void	phost_sync(bhv_desc_t *, int);

extern void	idbg_phost(__psint_t x);

#endif	/* _PHOST_PRIVATE_H_ */
