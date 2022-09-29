#ifndef __RPCSVC_BOOTPARAM_H__
#define __RPCSVC_BOOTPARAM_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.6 $"
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
/*	@(#)bootparam.h	1.3 88/05/08 4.0NFSSRC SMI	*/

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 1.2
 */

#ifndef _KERNEL
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/errno.h>
#include <sys/fs/nfs.h>
#endif
#define	MAX_MACHINE_NAME 255
#define	MAX_PATH_LEN	1024
#define	MAX_FILEID	32
#define	IP_ADDR_TYPE	1

typedef char *bp_machine_name_t;


typedef char *bp_path_t;


typedef char *bp_fileid_t;


struct ip_addr_t {
	char net;
	char host;
	char lh;
	char impno;
};
typedef struct ip_addr_t ip_addr_t;


struct bp_address {
	int address_type;
	union {
		ip_addr_t ip_addr;
	} bp_address;
};
typedef struct bp_address bp_address;


struct bp_whoami_arg {
	bp_address client_address;
};
typedef struct bp_whoami_arg bp_whoami_arg;


struct bp_whoami_res {
	bp_machine_name_t client_name;
	bp_machine_name_t domain_name;
	bp_address router_address;
};
typedef struct bp_whoami_res bp_whoami_res;


struct bp_getfile_arg {
	bp_machine_name_t client_name;
	bp_fileid_t file_id;
};
typedef struct bp_getfile_arg bp_getfile_arg;


struct bp_getfile_res {
	bp_machine_name_t server_name;
	bp_address server_address;
	bp_path_t server_path;
};
typedef struct bp_getfile_res bp_getfile_res;


#define BOOTPARAMPROG 100026
#define BOOTPARAMVERS 1
#define BOOTPARAMPROC_WHOAMI 1
#define BOOTPARAMPROC_GETFILE 2

extern bool_t xdr_bp_machine_name_t(XDR *, bp_machine_name_t *);
extern bool_t xdr_bp_path_t(XDR *, bp_path_t *);
extern bool_t xdr_bp_fileid_t(XDR *, bp_fileid_t *);
extern bool_t xdr_ip_addr_t(XDR *, ip_addr_t *);
extern bool_t xdr_bp_address(XDR *, bp_address *);
extern bool_t xdr_bp_whoami_arg(XDR *, bp_whoami_arg *);
extern bool_t xdr_bp_whoami_res(XDR *, bp_whoami_res *);
extern bool_t xdr_bp_getfile_arg(XDR *, bp_getfile_arg *);
extern bool_t xdr_bp_getfile_res(XDR *, bp_getfile_res *);
#ifdef __cplusplus
}
#endif
#endif /* !__RPCSVC_BOOTPARAM_H__ */
