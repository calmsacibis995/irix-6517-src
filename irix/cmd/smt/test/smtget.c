/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.7 $
 */

/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

		      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/

#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smt_snmp.h>
#include <smt_snmp_impl.h>
#include <smt_snmp_api.h>
#include <smtd_parm.h>
#include <smtd.h>
#include <smt_snmp_clnt.h>

#ifdef USEUDS
static char *tfname = 0;
static char map_tmplt[] =  "/usr/tmp/.get.XXXXXX";
#endif

main(argc, argv)
    int	    argc;
    char    *argv[];
{
    struct snmp_session session, *ss;
    struct snmp_pdu *pdu, *response;
    struct variable_list *vars;
    int	arg;
    char *gateway = NULL;
    char *community = NULL;
    int	count, current_name = 0;
    char *names[128];
    oid name[MAX_NAME_LEN];
    int name_length;
    int status;
	int s = -1;
	int dbglvl = LOG_ERR;

    /*
     * usage: smtget gateway-name community-name
     */
	for(arg = 1; arg < argc; arg++) {
		if (argv[arg][0] == '-') {
			switch(argv[arg][1]) {
			  case 'd':
				dbglvl++;
				break;
			  default:
				printf("invalid option: -%c\n", argv[arg][1]);
				break;
			}
			continue;
		}
		if (gateway == NULL){
			gateway = argv[arg];
		} else if (community == NULL) {
			community = argv[arg];
		} else {
			names[current_name++] = argv[arg];
		}
	}

	sm_openlog(SM_LOGON_STDERR, dbglvl, 0, 0, 0);
	(void)init_fddimib(SMT_MFILE);

    if (!(gateway && community && current_name > 0)){
	printf("usage: smtget gateway-name community-name object-identifier [object-identifier ...]\n");
	exit(1);
    }

    bzero((char *)&session, sizeof(struct snmp_session));
    session.peername = gateway;
    session.community = community;
    session.community_len = strlen(community);
    session.retries = SNMP_DEFAULT_RETRIES;
    session.timeout = SNMP_DEFAULT_TIMEOUT;
    session.authenticator = NULL;
    snmp_synch_setup(&session);
#ifdef USEUDS
	if (!tfname && (tfname = mktemp(map_tmplt)) == NULL) {
		printf("mktemp failed\n");
		return(-1);;
	}
	ss = snmp_open(&session, &s, tfname);
#else
    ss = snmp_open(&session, &s);
#endif
    if (ss == NULL){
	printf("Couldn't open snmp\n");
	exit(-1);
    }

	pdu = snmp_pdu_create(GET_REQ_MSG);
	for(count = 0; count < current_name; count++) {
		name_length = MAX_NAME_LEN;
		if (!read_objid(names[count], name, &name_length)) {
			printf("Invalid object identifier: %s\n", names[count]);
		}
		snmp_add_null_var(pdu, name, name_length);
	}

retry:
	status = snmp_synch_response(ss, pdu, &response);
	if (status != STAT_SUCCESS) {
		if (status == STAT_TIMEOUT){
			printf("No Response from %s\n", gateway);
		} else {    /* status == STAT_ERROR */
			printf("An error occurred, Quitting\n");
		}
		goto getdone;
	}

	if (response->errstat == SNMP_ERR_NOERROR) {
		for (vars=response->variables; vars; vars=vars->next_variable)
			print_variable(vars->name, vars->name_length, vars);
		goto getdone;
	}

	printf("Error in packet: %s\n", snmp_errstring(response->errstat));
	if (response->errstat == SNMP_ERR_NOSUCHNAME) {
		printf("This name doesn't exist: ");
		count = 1;
		vars = response->variables;
		while (vars) {
			if (count == response->errindex) {
				print_objid(vars->name, vars->name_length);
			}
			vars = vars->next_variable;
			count++;
		}
		printf("\n");
	}

#ifdef STASH
	if ((pdu = snmp_fix_pdu(response, GET_REQ_MSG)) != NULL)
		goto retry;
#endif
getdone:
	if (response)
		snmp_free_pdu(response);
	snmp_close(ss);
#ifdef USEUDS
	if (tfname)
		unlink(tfname);
#endif
}
