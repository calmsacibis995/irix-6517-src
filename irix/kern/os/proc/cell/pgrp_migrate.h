/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_PGRP_MIGRATE_H_
#define	_PGRP_MIGRATE_H_

#ident "$Id: pgrp_migrate.h,v 1.4 1997/02/21 19:59:10 cp Exp $"

#include <ksys/cell/relocation.h>
#include "pproc_private.h"

/*
 * Routines supporting process migration:
 */
extern int	vpgrp_proc_emigrate_start(vpgrp_t *, cell_t, obj_manifest_t *);
extern int	vpgrp_proc_immigrate_start(obj_manifest_t *, void **);
extern int	vpgrp_proc_emigrate(vpgrp_t *, cell_t, proc_t *, obj_bag_t);
extern int	vpgrp_proc_immigrate(vpgrp_t *, proc_t *, obj_bag_t);
extern int	vpgrp_proc_emigrate_end(vpgrp_t *, proc_t *);
extern int	vpgrp_proc_emigrate_abort(vpgrp_t *, proc_t *);
extern int	vpgrp_proc_immigrate_abort(vpgrp_t *, proc_t *);

extern void	vpgrp_obj_init(void);

#endif	/* _PGRP_MIGRATE_H_ */
