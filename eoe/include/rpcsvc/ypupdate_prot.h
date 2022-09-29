#ifndef __RPCSVC_YPUPDATE_PROT_H__
#define __RPCSVC_YPUPDATE_PROT_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.7 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
/* @(#)ypupdate_prot.h	1.2 88/03/31 4.0NFSSRC; from 1.4 88/02/08 Copyr 1986, Sun Micro */

/*
 * NIS update service protocol
 */
#define	YPU_PROG	100028
#define	YPU_VERS	1
#define	YPU_CHANGE	1
#define	YPU_INSERT	2
#define	YPU_DELETE	3
#define	YPU_STORE	4

#define MAXMAPNAMELEN	255
#define	MAXYPDATALEN	1023

struct yp_buf {
	int	yp_buf_len;	/* not to exceed MAXYPDATALEN */
	char	*yp_buf_val;
};

struct ypupdate_args {
	char   *mapname;	/* not to exceed MAXMAPNAMELEN */
	struct yp_buf key;
	struct yp_buf datum;
};

struct ypdelete_args {
	char   *mapname;
	struct yp_buf key;
};

int xdr_ypupdate_args();
int xdr_ypdelete_args();
#ifdef __cplusplus
}
#endif
#endif /* !__RPCSVC_YPUPDATE_PROT_H__ */
