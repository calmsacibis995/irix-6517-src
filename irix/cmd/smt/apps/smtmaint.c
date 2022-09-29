/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.7 $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smtd.h>
#include <smt_snmp_clnt.h>
#include <sm_map.h>

char	name[30];

main(argc, argv)
	int argc;
	char *argv[];
{
	int phyidx;
	enum pcm_ls pls;

	if (argc != 4) {
		fprintf(stderr, 
"usage: smtmaint interface phyindex pcm-line-state\n\
       smtmaint interface phyindex -t\n");
		exit(1);
	}
	argc--, argv++;

	/* get interface name */
	strncpy(name, *argv, sizeof(name));
	argc--, argv++;

	/* get phy index */
	phyidx = atoi(*argv);
	if ((phyidx != 0) && (phyidx != 1)) {
		fprintf(stderr, "%d: invalid PHY index\n", phyidx);
		exit(1);
	}
	argc--, argv++;

	if (geteuid() != 0) {
		fprintf(stderr, "You are not superuser\n");
		exit(1);
	}
	sm_openlog(SM_LOGON_STDERR, LOG_ERR, 0, 0, 0);

	/* trace */
	if (!strcmp(*argv, "-t")) {
		(void)sm_trace(name, (int)phyidx);
		map_exit(0);
	}
	
	/* Maint ls */
	if (isdigit(**argv)) {
		pls = (enum pcm_ls) atoi(*argv);
	} else {
		register char *cp = *argv;

		if (!strcasecmp("QLS", cp))
			pls = PCM_QLS;
		else if (!strcasecmp("ILS", cp))
			pls = PCM_ILS;
		else if (!strcasecmp("MLS", cp))
			pls = PCM_MLS;
		else if (!strcasecmp("HLS", cp))
			pls = PCM_HLS;
		else if (!strcasecmp("ALS", cp))
			pls = PCM_ALS;
		else if (!strcasecmp("ULS", cp))
			pls = PCM_ULS;
		else if (!strcasecmp("NLS", cp))
			pls = PCM_NLS;
		else if (!strcasecmp("LSU", cp))
			pls = PCM_LSU;
		else {
			fprintf(stderr, "%s: invalid PCM line state\n", cp);
			exit(1);
		}
	}

#ifdef STASH
	map_smt(name, FDDI_SNMP_MAINT, (char*)&pls, sizeof(pls), phyidx);
#endif
	(void)sm_maint(name, phyidx, pls);
	map_exit(0);
}
