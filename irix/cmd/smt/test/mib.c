/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.8 $
 */

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <sm_log.h>
#include <sm_mib.h>
#include <smt_snmp_api.h>
#include <smt_mib.h>
#include <smt_snmp.h>

struct tree *Mib;

main(argc, argv)
	int argc;
	char *argv[];
{
	oid objid[64];
	long ptr[128];
	register int count;
	struct variable_list variable;
	int objidlen = sizeof (objid);

	sm_openlog(SM_LOGON_STDERR, LOG_DEBUG+1, 0, 0, 0);

	Mib = init_fddimib(SMT_MFILE);
	if (argc < 2) {
		print_subtree(Mib, 0);
	}
	variable.type = ASN_INTEGER;
	ptr[0] = 3;
	variable.val.integer = ptr;
	variable.val_len = 4;
	variable.next_variable = NULL;    /* NULL for last variable */
	for (argc--; argc; argc--, argv++) {
		objidlen = sizeof (objid);
		sm_log(LOG_DEBUG, 0, "read_objid(%s) = %d",
	       		argv[1], read_objid(argv[1], objid, &objidlen));
		for(count = 0; count < objidlen; count++)
	    		sm_log(LOG_DEBUG, 0, "%d.", objid[count]);
		print_variable(objid, objidlen, &variable);
	}
}
