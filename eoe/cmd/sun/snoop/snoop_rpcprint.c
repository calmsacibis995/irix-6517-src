/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_rpcprint.c,v 1.1 1996/06/19 20:34:12 nn Exp $"

#include <sys/types.h>
#include <sys/tiuser.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include "snoop.h"

char *nameof_prog();

void
protoprint(flags, type, xid, prog, vers, proc, data, len)
	u_long xid;
	int flags, type, prog, vers, proc;
	char *data;
	int len;
{
	char *name;
	void (*interpreter)();
	extern void interpret_pmap();
	extern void interpret_rstat();
	extern void interpret_nfs();
	extern void interpret_nis();
	extern void interpret_mount();
	extern void interpret_nisbind();
	extern void interpret_nlm();
	extern void interpret_bparam();
	extern void interpret_rquota();
	extern void interpret_nisplus();
	extern void interpret_nisp_cb();
	extern void interpret_nfs_acl();
	extern void interpret_solarnet_fw();

	switch (prog) {
	case 100000:	interpreter = interpret_pmap;		break;
	case 100001:	interpreter = interpret_rstat;		break;
	case 100003:	interpreter = interpret_nfs;		break;
	case 100004:	interpreter = interpret_nis;		break;
	case 100005:	interpreter = interpret_mount;		break;
	case 100007:	interpreter = interpret_nisbind;	break;
	case 100011:	interpreter = interpret_rquota;		break;
	case 100021:	interpreter = interpret_nlm;		break;
	case 100026:	interpreter = interpret_bparam;		break;
	case 100227:	interpreter = interpret_nfs_acl;	break;
	case 100300:	interpreter = interpret_nisplus;	break;
	case 100302:	interpreter = interpret_nisp_cb;	break;
	case 150006:	interpreter = interpret_solarnet_fw;	break;
	default:	interpreter = NULL;
	}

	if (interpreter == NULL) {
		if (!(flags & F_SUM))
			return;
		name = nameof_prog(prog);
		if (*name == '?' || strcmp(name, "transient") == 0)
			return;
		(void) sprintf(get_sum_line(), "%s %c",
			name,
			type == CALL ? 'C' : 'R');
	} else {
		(*interpreter) (flags, type, xid, vers, proc, data, len);
	}
}
