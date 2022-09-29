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

#ifndef	_OS_PROC_DPROC_H_
#define	_OS_PROC_DPROC_H_ 1

#include <ksys/cell/tkm.h>
#include <ksys/cell/service.h>
#include <sys/idbgentry.h>
#include <ksys/cell/handle.h>

#define VPROC_TOKEN_EXIST	0
#define	VPROC_NTOKENS		1

#define	VPROC_EXISTENCE_TOKEN	TK_MAKE_SINGLETON(VPROC_TOKEN_EXIST, TK_READ)
#define VPROC_EXISTENCE_TOKENSET TK_MAKE(VPROC_TOKEN_EXIST, TK_READ)

#define	VPROC_NO_CREATE		4	/* Must be or'able with ZNO or ZYES */

extern service_t	pid_to_service(pid_t);
extern int		pid_is_local(pid_t);

extern void		vproc_obj_init(void);

struct vproc;
extern int		dcproc_create(struct vproc *);
extern void		dcproc_alloc(struct vproc *, obj_handle_t *);
extern void		dcproc_get_handle(bhv_desc_t *, obj_handle_t *);

struct proc;
extern void		idbg_pproc_print(struct proc *);

#endif	/* _OS_PROC_DPROC_H_ */
