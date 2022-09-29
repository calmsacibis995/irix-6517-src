/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_OS_PROC_CELL_VPROC_MIGRATE_H_
#define	_OS_PROC_CELL_VPROC_MIGRATE_H_ 1

#ident "$Id: vproc_migrate.h,v 1.6 1997/07/30 20:24:27 cp Exp $"

#include <ksys/fdt_private.h>

typedef struct {
	int		mi_call;
	void		*mi_arg;
	size_t		mi_argsz;
	uthread_t	*mi_threads;
} migrate_info_t;

extern int vproc_migrate(vproc_t *, cell_t);

extern void rexec_immigrate_bootstrap(migrate_info_t *);

extern int exec_complete(struct uarg *);

#define	VPROC_OBJ_TAG_SRC_PREP		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 0)
#define	VPROC_OBJ_TAG_MI_ARGS		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 1)
#define	VPROC_OBJ_TAG_PPROC		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 2)
#define VPROC_EXEC_TAG_ARGS		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 3)
#define VPROC_EXEC_TAG_STACK		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 4)
#define VPROC_EXEC_TAG_PHDRBASE		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 5)
#define VPROC_EXEC_TAG_EHDRP		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 6)
#define VPROC_EXEC_TAG_IDATA		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 7)
#define VPROC_PPROC_TAG_NOFILES		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 8)
#define VPROC_PPROC_TAG_FDINUSE		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 9)
#define VPROC_PPROC_TAG_FDFLAGS		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 10)
#define	VPROC_PPROC_TAG_CHILD		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 12)
#define	VPROC_PPROC_TAG_EXITFUNC	OBJ_SVC_TAG(SVC_PROCESS_MGMT, 13)
#define	VPROC_PPROC_TAG_UTHREAD		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 14)
#define	VPROC_UT_TAG_EXCEPTION		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 15)
#define	VPROC_PPROC_TAG_TRMASKS		OBJ_SVC_TAG(SVC_PROCESS_MGMT, 16)

#endif	/* _OS_PROC_CELL_VPROC_MIGRATE_H_ */
