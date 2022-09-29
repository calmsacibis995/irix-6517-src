/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * - test integrity of smt config file.
 *
 *	$Revision: 1.10 $
 */

#include <stdio.h>
#include <sys/types.h>

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>

#include <smt_snmp.h>
#include <smtd.h>
#include <cf.h>

static int smtdnum = 0;
static SMTD smtd[NSMTD];

SMTD *smtp;
SMT_MAC *smtd_config;
LFDDI_ADDR smtd_sr_mid;

static void
print_phy(SMT_PHY *phy)
{
	if (phy == 0)
		return;

	printf("\tPHY:\n");
	printf("\t\ttype = %d\n", phy->type);
	printf("\t\tpctype = %d\n", phy->pctype);
	printf("\t\ttb_max = %d\n", phy->tb_max);
	printf("\t\tler_cutoff = %d\n", phy->ler_cutoff);
	printf("\t\tler_alarm = %d\n", phy->ler_alarm);
	printf("\t\tdebug = %d\n", phy->debug);
	printf("\t\tip_pri = %d\n", phy->ip_pri);
	printf("\t\tpcm_tgt = %d\n", phy->pcm_tgt);
	printf("\tENDPHY\n");
}

static void
print_config(void)
{
	int iid;
	SMT_MAC *mac;

	for (iid = 0; iid < smtdnum; iid++) {
		smtp = &smtd[iid];
		printf("STATION:\n");
		printf("\tStationId = %s\n", sidtoa((char *)&smtp->sid));
		printf("\tStationType = %d\n", smtp->nodeclass);
		printf("\tphy_ct = %d\n", smtp->phy_ct);
		printf("\tmac_ct = %d\n", smtp->mac_ct);
		printf("\tmaster_ct = %d\n", smtp->master_ct);
		printf("\tpathavail = %x\n", smtp->pathavail);
		printf("\tconf_cap = %x\n", smtp->conf_cap);
		printf("\tconf_policy = %x\n", smtp->conf_policy);
		printf("\tconn_policy = %x\n", smtp->conn_policy);
		printf("\treportlimit = %d\n", smtp->reportlimit);
		printf("\tt_notify = %d\n", smtp->t_notify);
		printf("ENDSTATION:\n");
	}

	for (mac = smtd_config; mac; mac = mac->next) {
		printf("MAC:\n");
		printf("\tname = %s\n", mac->name);
		printf("\tphy_ct = %d\n", mac->phy_ct);
		printf("\tfsc = %d\n", mac->fsc);
		printf("\tbridge = %d\n", mac->bridge);
		printf("\tpathavail = %d\n", mac->pathavail);
		printf("\tpathrequested = %d\n", mac->pathrequested);
		printf("\taddr = %s\n", midtoa((u_char *)&mac->addr));
		printf("\ttreq = %d\n", mac->treq);
		printf("\ttmax = %d\n", mac->tmax);
		printf("\ttmin = %d\n", mac->tmin);
		printf("\ttvx = %d\n", mac->tvx);
		printf("\ttmax_lobound = %d\n", mac->tmax_lobound);
		printf("\ttvx_lobound = %d\n", mac->tvx_lobound);
		printf("\tfr_threshold = %d\n", mac->fr_threshold);
		printf("\tfnc_threshold = %d\n", mac->fnc_threshold);
		print_phy(mac->primary);
		print_phy(mac->secondary);
		printf("ENDMAC\n");
	}
}

main()
{
	FILE *fp;
	char line[LINESIZE];
	char name[CFBUFSIZE];
	char **lp;

	printf("smtd_config:\n");
	if ((fp = fopen(SMT_CFILE, "r")) == NULL) {
		printf("can,t open %s\n", SMT_CFILE);
		exit(1);
	}
	while (lp = getline(line, sizeof(line), fp)) {
		skipblank(lp);
		if ((**lp == '#') || (**lp == '\n'))
			continue;

		if (sscanf(*lp, "%s", name) != 1) {
			printf("bad config format(%s)\n", *lp);
			exit(2);
		}

		if (strcmp(name, "STATION:") == 0) {
			smtp = &smtd[smtdnum];
			smtdnum++;
			conf_station(fp);
		} else if (strcmp(name, "MAC:") == 0) {
			conf_mac(fp);
		} else {
			printf("Illegal KEY: %s\n", name);
			exit(3);
		}
	}
	fclose(fp);
	print_config();
}
