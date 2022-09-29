/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident  "$Revision: 1.6 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/stream.h>
#include <sys/strids.h>

static struct module_info ld_info = {
	STRID_LDTERM,			/* module ID */
	"ldterm",			/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* infinite maximum packet size	*/
	1,				/* hi-water mark */
	0,				/* lo-water mark */
};


extern int st_open(queue_t*, dev_t *, int, int, cred_t *);
extern int st_rput(queue_t*, mblk_t*);
extern int st_rsrv(queue_t*);
extern int st_close(queue_t*, int, cred_t *);
extern int st_wsrv(queue_t *);
extern int st_wput(queue_t *, mblk_t *);

static struct qinit ld_rinit = {
	st_rput, st_rsrv, st_open, st_close, NULL, &ld_info, NULL
};

static struct qinit ld_winit = {
	st_wput, st_wsrv, NULL, NULL, NULL, &ld_info, NULL
};

struct streamtab ldterminfo = {&ld_rinit, &ld_winit, NULL, NULL};

int ldtermdevflag =  0;


