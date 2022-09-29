/**************************************************************************
 *                                                                        *
 * Copyright (C) 1991-1996 Silicon Graphics, Inc.                         *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Header"

/* 
 * prototypes for data pipe interface 
 */

#ifndef _DPIPE_H
#define _DPIPE_H

#include <sys/dpipe.h>

typedef struct dpipe_lib_hdl_s {
    dpipe_end_trans_ctx_hdl_t (*bind_transfer)(struct dpipe_lib_hdl_s *, 
					       int, /* pipe id */
					       __int64_t, /* transfer_id */
					       int /* src or sink*/);
} *dpipe_lib_hdl_t;

int         dpipeCreate(int  /* src_fd */, int  /* sink_fd */);
int         dpipeDestroy(int /* pipefd */);
__int64_t   dpipeTransfer(int /* pipefd */, 
			  dpipe_lib_hdl_t /* src_context */, 
			  dpipe_lib_hdl_t /* sink_conext */);
int         dpipeStop(int /* pipefd */, __int64_t /* transfer_id */);
int         dpipeStatus(int /* pipefd */, __int64_t /*  transfer_id */);
int         dpipeReset(int /* pipefd */);
int         dpipeFlush(int /* pipefd */);


/* 
 * The following functions play the file system pipe end role.
 */

typedef struct dpipe_fspe_ctx_s {
	int                          iovcnt;
	struct dpipe_fspe_bind_list  *iov;         /* see sys/dpipe.h */
} dpipe_fspe_ctx_t;

typedef struct dpipe_fspe_hdl_s {
	dpipe_end_trans_ctx_hdl_t   (*bind_transfer)(dpipe_lib_hdl_t, int,
						     __int64_t, int);
	int                         fd;
	dpipe_fspe_ctx_t            *ctx;
	struct dpipe_fspe_hdl_s     *next;
} dpipe_fspe_hdl_t;

dpipe_lib_hdl_t dpipe_fspe_get_hdl(int /* fd */);
int             dpipe_fspe_set_ctx(dpipe_lib_hdl_t, dpipe_fspe_ctx_t);

#endif /* _DPIPE_H */
