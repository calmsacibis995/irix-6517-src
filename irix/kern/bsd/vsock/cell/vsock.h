/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef __VSOCKDATA_H__
#define __VSOCKDATA_H__
#ident	"$Revision: 1.13 $"

#include <ksys/behavior.h>
#include <ksys/cell/handle.h>
#include <ksys/cell/tkm.h>
#include <ksys/cell/wp.h>

#define	VSOCK_TOKEN_EXIST	0

#define	VSOCK_EXISTENCE_TOKENSET	TK_MAKE(VSOCK_TOKEN_EXIST, TK_READ)

struct vsock_statistics {
	int     vsocket_count;
	int	dcvsock_count;
	int	vsocktab_total;
	int	vsocktab_lookups;
	int	vsocket_release;
	int	dsvsock_count;
	int	vsocket_frees;
	int	vsocket_allocs;
};

#if defined(_KERNEL) || defined(_KMEMUSER)
struct	vsockinfo {
	int	vsock_max;	/* max vsockets */
	int	vsock_i;	/* current # of vsockets */
};

#endif /* _KERNEL _KMEMUSER  */

extern struct vsocktable	*vsocktab;
extern	service_t		vsock_service_id;
extern struct zone		*dsvsock_zone;
extern struct zone		*dcvsock_zone;
extern struct vsock_statistics	vsock_statistics;
extern service_t		dssock_service_id;

extern	void vsock_client_quiesce(vsock_t  *);
extern	vsock_gen_t	vsock_newid (void);
extern	void	vsock_lookup_id (vsock_handle_t *, vsock_t **);
extern	void	vsock_enter_id (vsock_handle_t *, vsock_t *);
extern	int	vsock_hremove (vsock_t *, vsock_handle_t *);
extern	void vsocket_free(struct vsocket *);
#endif /* __VSOCKDATA_H__ */
