/*
 * smtstat.c
 *
 *	Show SMT statistics.
 *
 *
 * Copyright 1990, Silicon Graphics, Inc.
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

#ident "$Revision: 1.56 $"

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <curses.h>
#include <termios.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/times.h>
#include <string.h>
#include <bstring.h>
#include <getopt.h>

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smtd.h>
#include <smt_snmp_clnt.h>
#include <smt_snmp_api.h>
#include <smt_subr.h>
#include <sm_map.h>
#include <sm_addr.h>
#include <ma_str.h>
#include <sm_str.h>

#define SLCTR(x) LCTR(st.x)

static void spr(void);
static void smtpr(void);
static void prnt(char *, ...);

static WINDOW *win;

#define MAX_MACS 2

#define	BOS (LINES-1)			/* Index of last line on screen */

SMTD smtd;
IPHASE_INFO ifi;

int	sflag = 0;			/* continual screen display */
int	vflag = 0;			/* verbose */
int	lflag = 0;			/* show label (-v or -vv) */
int	dflag = 0;			/* dump MAC info (-vv) */
int	Iflag = 0;			/* -I specified */
int	iflag = 0;			/* -i specified */
int	nsflag = 0;			/* don't bit swap address */
int	nflag = 0;			/* don't translate numbers */

int	scmd = 0;			/* default scmd = MAC */
int	unit = 0;
int	interval = 0;
int	devnum = 0;
int	fail_cnt = 0;
#define NO_FAIL 5
char	*devname = NULL;		/* Device name only */
char	interface[16];			/* Device name & unit # -- ipg0 */
char	str1[128], str2[128], str3[128], hname[128];

char	usage[] = "[ -nvsbi ] [-I interface] [interval]";

char	hostname[MAXHOSTNAMELEN];


static time_t base_sec;			/* when the clock said that */
static time_t base_tick;		/* lbolt said that */

static int needhead;
static int first = 2;			/* 2=clear counters, 1=clear screen */
static int Zflag = 0;
static int Dflag = 0;

static void
quit(int stat)
{
	if (sflag) {
		move(BOS, 0);
		clrtoeol();
		refresh();
		endwin();
	}
	map_exit(stat);
}

static void
sig_winch(int sig)
{
	if (sflag) {
		(void)endwin();
		win = initscr();
		if (first < 1)
			first = 1;
	}

	/* re-arm in case we have System V signals */
	(void)signal(sig, sig_winch);
}

static void
fail(char *fmt, ...)
{
	va_list args;
	char msg[200];
	int i;

	va_start(args, fmt);
	i = vsprintf(msg, fmt, args);
	va_end(args);

	if (sflag) {
		if (msg[i-1] == '\n')
			msg[i-1] = '\0';
		move(BOS, 0);
		clrtoeol();
		printw("smtstat: %s", msg);
		refresh();
	} else {
		fprintf(stderr,"smtstat: %s", msg);
	}
	if (--fail_cnt < 0) {
		if (sflag) {
			endwin();
			putc('\n',stderr);
		}
		map_exit(1);
	} else {
		needhead = 1;
	}
}

main(argc, argv)
	int argc;
	char *argv[];
{
	char *cp, *prog;
	int vers;
	int i;

	prog = argv[0];
	while ((i = getopt(argc, argv, "bsCnvI:i")) != EOF)
		switch (i) {
		case 'b':
			nsflag++;
			break;
		case 's':
		case 'C':
			sflag++;
			break;
		case 'n':
			nflag++;
			break;
		case 'v':
			vflag++;
			break;
		case 'I':
			Iflag++;
			devname = optarg;
			if (*devname == '\0')
				goto use;
			strcpy(interface, devname);
			cp = devname+strlen(devname)-1;
			if (!isdigit(*cp))
				goto use;
			while (isdigit(*cp))
			       cp--;
			unit = atoi(cp++);
			*cp = 0;
			break;
		case 'i':
			iflag++;
			break;
		default:
use:
			printf("usage: %s %s\n", prog, usage);
			map_exit(1);
	}

	if (vflag > 0) {
		lflag++;
		if (vflag > 1)
			dflag++;
	}

	if (argc != optind) {
		interval = strtol(argv[optind], &cp, 10);
		if (interval <= 0 || *cp != '\0')
			goto use;
	}

	(void) gethostname(hostname, sizeof(hostname));
	if ((cp = strchr(hostname, '.')) != NULL)
		*cp = '\0';

	sm_openlog(sflag ? SM_LOGON_SYSLOG : SM_LOGON_STDERR,
		   LOG_ERR, "smtstat", 0, LOG_USER);

	vers = -1;
	if (map_smt(0, FDDI_SNMP_VERSION, &vers, sizeof(vers), devnum)
		!= STAT_SUCCESS) {
		printf("Can't get smtd version\n");
		map_exit(1);
	}
	if (vers != SMTD_VERS) {
		printf("smtstat version %d doesn't match daemon's (%d)\n",
			SMTD_VERS, vers);
		map_exit(1);
	}

	if (iflag && !sflag && !Iflag) {
		/*
		 * non-interative output of all interfaces
		 */
		for (devnum = 0; devnum < NSMTD; devnum++) {
			sm_openlog(sflag ? SM_LOGON_SYSLOG : SM_LOGON_STDERR,
				   0, "smtstat", 0, LOG_USER);
			if (map_smt(0,FDDI_SNMP_INFO, &ifi,sizeof(ifi),devnum)
			    != STAT_SUCCESS) {
				break;
			}
			sm_openlog(sflag ? SM_LOGON_SYSLOG : SM_LOGON_STDERR,
				   LOG_ERR, "smtstat", 0, LOG_USER);
			if (devnum != 0)
				prnt("\n");
			smtpr();
		}
		map_exit(1);

	} else if (Iflag) {
		/*
		 * a single, named interface
		 */
		for (devnum = 0; devnum < NSMTD; devnum++) {
			if (map_smt(0,FDDI_SNMP_INFO, &ifi,sizeof(ifi),devnum)
			    != STAT_SUCCESS) {
				break;
			}
			for (i = 0; i < ifi.mac_ct; i++) {
				if (!strcmp(ifi.names[i], interface))
					goto devfound;
			}
		}
		printf("Invalid device name: %s\n", interface);
		map_exit(1);
devfound:;

	} else if (map_smt(0,FDDI_SNMP_INFO, &ifi,sizeof(ifi), devnum)
		   != STAT_SUCCESS) {
		printf("Station info failed\n");
		map_exit(2);
	}

	if (map_smt(0, FDDI_SNMP_DUMP, &smtd, sizeof(smtd), devnum)
		!= STAT_SUCCESS) {
		printf("smt status failed\n");
		map_exit(1);
	}

	signal(SIGINT, quit);
	signal(SIGTERM, quit);
	signal(SIGHUP, quit);
	(void)signal(SIGWINCH, sig_winch);

	if (sflag)
		spr();
	else
		smtpr();

	quit(0);
	/*NOTREACHED*/
}

static void
pr_time(char *tstr, time_t secs)
{
	time_t now = time(0);

	if (secs == 0)
		(void)strcpy(tstr,"never");
	else if (now - 12*60*60 <= secs && secs < now + 12*60*60)
		(void)cftime(tstr, "%T", &secs);
	else
		(void)cftime(tstr, "%x", &secs);
}


static void
pr_phy(char *name, int phyidx, SMT_PHY *phy)
{
	struct smt_st st;
	register u_long m, n;
	register struct smt_log *lp;
	char lslog[SMT_LOG_LEN][20];
	static char *tstr = "DDD MMM DD HH:MM:SS";
	static char *tstr2 = "DDD MMM DD HH:MM:SS";
	time_t first_sec, last_sec,last_tick;
	u_long first_ms10, last_ms10;
	int rows;

	bzero(&st, sizeof(st));
	if (map_smt(name, FDDI_SNMP_STAT, &st, sizeof(st), phyidx)
		!= STAT_SUCCESS) {
		prnt("port status for %s:%d failed\n", name, phyidx);
		return;
	}

	prnt("%-4.4s ", ((phyidx==0)||lflag) ? name : "    ");
	if (nsflag)
		strcpy(hname, midtoa(&st.addr));
	else
		(void)fddi_ntoa(&st.addr, hname);

	if (nflag) {
		if (phyidx == 0 || phy->type == DM_DAS)
			prnt("%-17.17s %-3x ", hname, (int)st.mac_state);
		else
			prnt("%-17.17s %-3s ", "", "");
		prnt("%-4x %-4x ", (int)phy->pctype, (int)st.npc_type);
		prnt("%-6.6x %-5x %-3x ", (int)st.pcm_state,
			(int)st.tls, (int)st.rls);
		prnt("%x\n", (int)st.flags);
	} else {
		if (phyidx == 0 || phy->type == DM_DAS)
			prnt("%-17.17s %-3.3s ",
			     hname, ma_mac_state_str(st.mac_state,0));
		else
			prnt("%-17.17s %-3.3s ", "", "");
		prnt("%-4.4s %-4.4s ",
		     ma_pc_type_str(phy->pctype),
		     ma_pc_type_str(st.npc_type));
		prnt("%-6.6s %-5s %-3s ", ma_pcm_state_str(st.pcm_state),
		     ma_rt_ls_str2(st.tls), ma_pcm_ls_str(st.rls));
		prnt("%s\n", ma_pcm_flag_str(st.flags, str1));
	}

	if ((!lflag) || interval)
		return;

#define Pr(lab)	prnt("\t%-12s %-10d", #lab, st.lab);

	prnt("\n**** PCM Status ****\n\n");
	prnt("\tn = %d\n", st.n);
	if (nflag) {
		prnt("\tr_val  %#x\n", st.r_val);
		prnt("\tt_val  %#x\n", st.t_val);
	} else {
		prnt("\tr_val  %s\n", ma_pc_signal_str(st.r_val, st.n, str1));
		prnt("\tt_val  %s\n", ma_pc_signal_str(st.t_val, st.n, str2));
	}
	prnt("\t%-7s %3.3f msec", "t_neg", FROMBCLK(st.t_neg));
	Pr(t_next);
	Pr(alarm);
	prnt("\n");
	Pr(lct_fail);
	Pr(lct_tot_fail);
	Pr(connects);
	prnt("\n");
	Pr(lem_count);
	Pr(ler);
	Pr(ler2);
	prnt("\n");
#undef Pr

	bzero(lslog, sizeof(lslog));
	lp = &st.log[st.log_ptr];
	last_tick = 0;
	for (m = 0, n = 0; m < SMT_LOG_LEN; m++, lp++) {
		int delta;
		u_long sec, ms10;

		if (lp >= &st.log[SMT_LOG_LEN])
			lp = &st.log[0];
		if (lp->lbolt == 0)
			continue;

		sec = base_sec - (base_tick-lp->lbolt+HZ-1)/HZ;
		ms10 = HZ - (base_tick-lp->lbolt)%HZ;

		if (n == 0) {
			first_sec = sec;
			first_ms10 = ms10;
		}
		if (last_tick < lp->lbolt) {
			last_sec = sec;
			last_ms10 = ms10;
		}

		if (nflag)
			sprintf(lslog[n], "%6ld.%02ld: %-7.2#x",
				(long)sec%60, ms10, (u_long)lp->ls);
		else
			sprintf(lslog[n], "%6ld.%02ld: %-7s",
				(long)sec%60, ms10,
				ma_rt_ls_str((enum rt_ls)lp->ls));
		n++;
	}

	(void)cftime(tstr, "%a %b %d %T", &first_sec);
	(void)cftime(tstr2,"%a %b %d %T", &last_sec);
	prnt("\n**** %s PHY %d Line State Log\n",
	     name, phyidx);
	prnt("\tfrom %s.%02d to %s.%02d ****\n",
	     tstr,first_ms10, tstr2,last_ms10);

	rows = (n+3)/4;
	for (n = 0; n < rows; n++) {
		prnt("%-s  %-s  %-s  %-s\n",
		lslog[n], lslog[n+rows], lslog[n+2*rows], lslog[n+3*rows]);
	}

	prnt("\n**** Kernel Statistics ****\n\n");

#define Pr(lab, val)	prnt("\t%-10s %-12.0f", #lab, SLCTR(val))
#define PrD(lab)	Pr(lab, lab)

	PrD(frame_ct);
	PrD(tok_ct);
	prnt("\n");

	Pr(A-bit, a);
	Pr(C-bit, c);
	Pr(E-bit, e);
	prnt("\n");

	PrD(rngop);
	PrD(rngbroke);
	PrD(pos_dup);
	prnt("\n");

	PrD(tkerr);
	PrD(clm);
	PrD(bec);
	prnt("\n");

	PrD(tvxexp);
	PrD(trtexp);
	PrD(tkiss);
	prnt("\n");

	PrD(myclm);
	PrD(loclm);
	PrD(hiclm);
	prnt("\n");

	PrD(mybec);
	PrD(otrbec);
	PrD(dup_mac);
	prnt("\n");

	PrD(eovf);
	PrD(noise);
	PrD(xmtabt);
	prnt("\n");

	PrD(tx_under);
	PrD(err_ct);
	PrD(lost_ct);
	prnt("\n");

	PrD(flsh);
	PrD(abort);
	PrD(miss);
	prnt("\n");

	PrD(tot_junk);
	PrD(junk_void);
	PrD(junk_bec);
	prnt("\n");

	PrD(error);
	PrD(shorterr);
	PrD(longerr);
	prnt("\n");

	PrD(rx_ovf);
	PrD(buf_ovf);
	prnt("\n");
}

static void
dump_phy(register SMT_PHY *phy)
{
	prnt("\n  phy%d:\ttype=%s\tpctype=%s\tpcnbr=%s\n",
			phy->rid,
			ma_st_type_str(phy->type),
			ma_pc_type_str(phy->pctype),
			ma_pc_type_str(phy->pcnbr));

	prnt("\tloop_time=%d\tfotx=%s\n",
			phy->loop_time,
			sm_fotx_str(phy->fotx));
	prnt("\tmaint_ls=%s\ttb_max=%d\tbs_flag=%s\n",
			ma_pcm_ls_str(phy->maint_ls),
			phy->tb_max,
			sm_flag_str(phy->bs_flag));
	prnt("\tdebug=%d\tip_pri=%d\tuica_flag=%s\tpcm_tgt=%s\n",
			phy->debug, phy->ip_pri,
			sm_flag_str(phy->uica_flag),
			ma_pcm_tgt_str(phy->pcm_tgt));
	prnt("\tobs=%d\timax=%d\tinsert=%d\tipolicy=%d\n",
			phy->obs, phy->imax, phy->insert, phy->ipolicy);
	prnt("\tler_estimate=%d\tler_alarm=%d\tler_cutoff=%d\tpler_cond=%s\n",
			phy->ler_estimate, phy->ler_alarm, phy->ler_cutoff,
			sm_flag_str(phy->pler_cond));
	prnt("\tremotemac=%s\tmacplacement=%d\tconn_policy=%s\n",
			sm_flag_str(phy->remotemac),
			phy->macplacement,
			sm_phy_connp_str(phy->phy_connp, str1));
	prnt("\tce_state=%s\tpathrequested=%s\tpathavail=%s\n",
			sm_ce_state_str(phy->ce_state),
			sm_pathtype_str(phy->pathrequested, str1),
			sm_pathtype_str(phy->pathavail, str2));
	prnt("\tconn_state=%s\twithhold=%s\tpcm_state=%s\n",
			sm_conn_state_str(phy->conn_state),
			sm_withhold_str(phy->withhold),
			ma_pcm_state_str(phy->pcm_state));
}

static void
dump_mac(MAC_INFO *mi)
{
	char tnn_tstr[10], tuv_tstr[10], tdv_tstr[10];
	register SMT_MAC *mac = &mi->mac;

	prnt("\n*** MAC Dump for %s (resid=%d, addr=%s, ringop=%s)\n\n",
			mac->name, mac->rid, fddi_ntoa(&mac->addr, str1),
			sm_flag_str(mac->ringop));
	prnt("\trmtstate=%s\tllcavail=%s\n",
			sm_rmtstate_str(mac->rmtstate),
			sm_flag_str(mac->llcavail));
	prnt("\trootconcentrator=%x\tdnaport=%s\n",
			mac->rootconcentrator,
			ma_pc_type_str((enum fddi_pc_type)mac->dnaport));
	prnt("\tuna=%s\tolduna=%s\n",
			fddi_ntoa(&mac->una, str1),
			fddi_ntoa(&mac->old_una, str2));
	prnt("\tdna=%s\told_dna=%s\n",
			fddi_ntoa(&mac->dna, str1),
			fddi_ntoa(&mac->old_dna, str2));
	pr_time(tnn_tstr, mi->mac.tnn);
	pr_time(tuv_tstr, mi->mac.tuv);
	pr_time(tdv_tstr, mi->mac.tdv);
	prnt("\ttnn=%s:%d\tnn_xid=%#x\ttuv=%s\ttdv=%s\n",
			tnn_tstr, mac->nn_xid, tuv_tstr, tdv_tstr);
	prnt("\tnn_state=%s\t\tuv_flag=%s\t\tdv_flag=%s\n",
			sm_nn_state_str(mac->nn_state),
			sm_flag_str(mac->uv_flag),
			sm_flag_str(mac->dv_flag));
	prnt("\tdupa_test=%s\tdupa_flag=%s\t\tunda_flag=%s\n",
			sm_dupa_test_str(mac->dupa_test),
			sm_flag_str(mac->dupa_flag),
			sm_flag_str(mac->unda_flag));
	prnt("\tbridge=%s\ttmax_lobound=%.3f\ttvx_lobound=%.3f\n",
			sm_bridge_str(mac->bridge),
			FROMBCLK(mac->tmax_lobound),
			FROMBCLK(mac->tvx_lobound));
	prnt("\tpathavail=%s\tpathrequested=%s\tcurpath=%s\n",
			sm_pathtype_str(mac->pathavail, str1),
			sm_pathtype_str(mac->pathrequested, str2),
			sm_pathtype_str(mac->curpath, str3));
	prnt("\tfr_threshold=%d\t\tfr_errratio=%d\t\tfrm_errcond=%s\n",
			mac->fr_threshold, mac->fr_errratio,
			sm_flag_str(mac->frm_errcond));
	prnt("\tfnc_threshold=%d\tnc_ratio=%d\t\tfrm_nccond=%s\n",
			mac->fnc_threshold, mac->nc_ratio,
			sm_flag_str(mac->frm_nccond));
	prnt("\tmsloop_stat=%s\troot_dnaport=%s\troot_curpath=%s\n",
			sm_msloop_str(mac->msloop_stat),
			ma_pc_type_str((enum fddi_pc_type)mac->root_dnaport),
			sm_pathtype_str(mac->root_curpath, str1));
	prnt("\ttmax=%-.0f\ttmin=%-.0f\ttreq=%-.0f\ttvx=%-.0f\ttneg=%-.0f\n",
			FROMBCLK(mac->tmax),
			FROMBCLK(mac->tmin),
			FROMBCLK(mac->treq),
			FROMBCLK(mac->tvx),
			FROMBCLK(mac->tneg));
	if (mac->primary)
		dump_phy(&mi->phy_0);
	if (mac->secondary)
		dump_phy(&mi->phy_1);
}


/*
 * Print a description of the station.
 */
static void
smtpr()
{
	register int i,j;
	int lines;
	MAC_INFO mi;
	struct smt_log now_log;
	struct tms dum_tms;
	struct timeval now;

	if (map_smt(0, FDDI_SNMP_DUMP, &smtd,sizeof(smtd),
		    devnum) != STAT_SUCCESS) {
		prnt("smt status for %s failed\n", ifi.names[0]);
		quit(1);
	}

	if (nflag)
		prnt("%d MAC Node (%d)",smtd.mac_ct, smtd.nodeclass);
	else
		prnt("%s: ", ma_st_type_str(smtd.st_type));

	prnt("Station ID=%s SMT Version %d (%d-%d)\n",
	     sidtoa(&smtd.sid),
	     smtd.vers_op, smtd.vers_lo, smtd.vers_hi);

	if (dflag) {
		prnt("\txid=%#x\tlast_sid=%s\n",
		     smtd.xid,
		     sidtoa(&smtd.last_sid));
		prnt("\tsetcount=%d\tsettime=%d:%d\n",
		     smtd.setcount.sc_cnt,
		     smtd.setcount.sc_ts.tv_sec,
		     smtd.setcount.sc_ts.tv_usec);
		prnt("\ttopology=%s\trd_flag=%s\treportlimit=%d\n",
		     sm_topology_str(smtd.topology, str1),
		     sm_flag_str(smtd.rd_flag),
		     smtd.reportlimit);
		prnt("\tpmf_on=%s\ttrace_on=%s\tsrf_on=%s\n",
		     sm_flag_str(smtd.pmf_on),
		     sm_flag_str(smtd.trace_on),
		     sm_flag_str(smtd.srf_on));
		prnt("\tmsg_ts=%d.%06u\ttrans_ts=%d.%06u\n",
		     smtd.msg_ts.tv_sec, smtd.msg_ts.tv_usec,
		     smtd.trans_ts.tv_sec,smtd.trans_ts.tv_usec);
		prnt("\tconf_cap=%s\tconn_policy=%s\n",
		     sm_conf_policy_str(smtd.conf_policy, str1),
		     sm_conn_policy_str(smtd.conn_policy, str2));
		prnt("\tecmstate=%s\tcfmstate=%s\tholdstate=%s\n",
		     sm_ecm_state_str(smtd.ecmstate),
		     sm_cfm_state_str(smtd.cf_state),
		     sm_hold_state_str(smtd.hold_state));
		prnt("\tmanuf data=%02x:%02x:%02x:%s\n",
		     (int)smtd.manuf.manf_oui[0],
		     (int)smtd.manuf.manf_oui[1],
		     (int)smtd.manuf.manf_oui[2],
		     (char*)&smtd.manuf.manf_data[0]);
		prnt("\tuser data=%s\n",  smtd.userdata.b);
	}


	for (lines = 0; ; lines++) {
#define BANNER "\n%-4.4s %-17.17s %-3.3s %-4.4s %-4.4s %-6.6s %-5s %-3s %-5s\n", \
		"Name","Address","MAC","port","nbr","PCM","tls","rls","flags"

		if (iflag || (lines % 21) == 0)
			prnt(BANNER);

		for (i=0; i < ifi.mac_ct; i++) {
			char *name = ifi.names[i];

			bzero(&mi, sizeof(mi));
			if (map_smt(name,FDDI_SNMP_DUMP, &mi,sizeof(mi),0)
				!= STAT_SUCCESS) {
				prnt("mac dump for %s failed\n", name);
				return;
			}

			/* Get base for lbolt, trimmed to 26 bits, by
			 * storing it in a dummy log structure.
			 * Avoid the start of a second, since beating
			 * between the time-of-day adjustments and lbolt
			 * would otherwise make the timestamps jiggle.
			 */
			sginap(1);
			now_log.lbolt = times(&dum_tms);
			(void)gettimeofday(&now,0);
			base_sec = now.tv_sec;
			base_tick = (now_log.lbolt
				     -(now.tv_usec+1000000/HZ/2)/(1000000/HZ));

			for (j = 0; j < mi.mac.phy_ct; j++) {
				if (j != 0 && lflag && !interval)
					prnt(BANNER);
				pr_phy(name, j,
				       j == 0 ? &mi.phy_0 : &mi.phy_1);
			}

			if (dflag)
				dump_mac(&mi);
		}

		if (!interval)
			return;

		sleep(interval);
	}
}

/*----------------------------------------------------------------------*/

#define NSLCTR(x) LCTR(st->x)

static MAC_INFO cmi[MAX_MACS], zmi[MAX_MACS], dmi[MAX_MACS];
static struct smt_st st0[MAX_MACS*2], zst0[MAX_MACS*2], dst0[MAX_MACS*2];

static void
sprzero()
{
	if (Zflag) {
		bcopy(cmi, zmi, sizeof(zmi));
		bcopy(st0, zst0, sizeof(zst0));
		return;
	}
	bzero(zmi, sizeof(zmi));
	bzero(zst0, sizeof(zst0));
}

static void
sprdelta()
{
	if (Dflag) {
		bcopy(cmi, dmi, sizeof(dmi));
		bcopy(st0, dst0, sizeof(dst0));
		return;
	}

	bzero(dmi, sizeof(dmi));
	bzero(dst0, sizeof(dst0));
}

#define LABEL_LEN 14
#define NUM_LEN	  12
#define P(lab,fmt,val) {					\
	if (first) {move(y,xlab); prnt("%-.*s", LABEL_LEN, lab); } \
	move(y,x);						\
	prnt(fmt,(val));					\
	y++;							\
}
#define PC(index,cond,lab,fmt,val,noval) {			\
	if (index == 0 || (cond)) {				\
		move(y,xlab);					\
		prnt("%-*.*s", LABEL_LEN,LABEL_LEN,		\
		     ((cond) ? lab : " "));			\
	}							\
	move(y,x); prnt(fmt,((cond)?(val):(noval)));		\
	y++;							\
}

#define PL(lab) {					\
	if (first) { move(y,xlab); prnt("%-s",lab); }	\
	    y++;					\
}

#define NEWCOL(old) {y=(old); x += 40; xlab += 40;}
#define ADJX(i)	(x = LABEL_LEN + i * (NUM_LEN + 1))

#define PM(lab, fmt, val) P(lab,fmt,(mi->val - mip->val))
#define PS(lab, fmt, val) P(lab,fmt,(st->val - stp->val))

#define DST(val)	(LCTR(st->val)-LCTR(stp->val))
#define PSL(lab,fmt,val) P(lab,fmt,DST(val))

/* print (on screen) highlighted if condition is true */
#define PH(lab,fmt,val, cond) {					\
	if (cond) {						\
		if (first) {move(y,xlab); prnt("%-.*s", LABEL_LEN, lab); } \
		standout(); move(y,x); prnt(fmt,(val)); y++; standend(); \
	} else {						\
		P(lab,fmt,val);					\
	}							\
}

#define PHS(lab,fmt,val) {					\
	if (st->val != stp->val) {				\
		if (first) {move(y,xlab); prnt("%-.*s", LABEL_LEN, lab); } \
		standout(); move(y,x); prnt(fmt,st->val-stp->val); \
		y++; standend();				\
	} else {						\
		PS(lab,fmt,val);				\
	}							\
}


static char delta_date[] = "mon dd hh:mm:ss";
#define BANNER_DPAT "%b %d %T"

static void
banner(char *mode, char* tmb)
{
	move(0,0);
	if (Dflag) {
		prnt("%-11s %s %-50.50s D: %s",
		     mode, ifi.names[0], hostname,
		     tmb);

	} else if (Zflag) {
		prnt("%-11s %s %-26.26s Z: from %s to %s",
		     mode, ifi.names[0], hostname,
		     delta_date, tmb);
	} else {
		prnt("%-11s %s %-26.26s R: from %s to %s",
		     mode, ifi.names[0], hostname,
		     delta_date, tmb);
	}
	move(1,0);
}


/*
 * print mac info
 */
static void
spr_mac(register int y, char *tmb)
{
	char *name;
	register int i;
	register int x, xlab = 0;
	int ysave = ++y;
	double tmp, latency, ringload;

	register MAC_INFO *mi, *mip;
	register struct smt_st *st, *stp;

	double diffclock, usecpertok, tokens, dtok;
	static clock_t prevclock[MAX_MACS];
	static double  prevtoks[MAX_MACS];
	struct tms dummy;
	clock_t curclock;

	banner("1:MAC",tmb);

	for (i = 0; i < ifi.mac_ct; i++) {
		mi = &cmi[i];
		st = &st0[i*2];
		if (Dflag) {
			mip = &dmi[i];
			stp = &dst0[i*2];
		} else {
			mip = &zmi[i];
			stp = &zst0[i*2];
		}

		curclock = times(&dummy);;
		diffclock = (double)(curclock - prevclock[i]) / (double)HZ;

		name = ifi.names[i];
		if (map_smt(name, FDDI_SNMP_DUMP, mi, sizeof(*mi), 0)
			!= STAT_SUCCESS) {
			fail("mac dump for %s failed\n", name);
			return;
		}
		if (map_smt(name, FDDI_SNMP_STAT, st, sizeof(*st), 0)
			!= STAT_SUCCESS) {
			fail("port status for %s:%d failed\n", name, i);
			return;
		}
		fail_cnt = NO_FAIL;

		y = ysave;
		ADJX(i);
		xlab=0;

		P("",	"%10s", name);
		PH("MAC state", "%10s", ma_mac_state_str(st->mac_state, 1),
		   st->mac_state == MAC_OFF);

		PM("packets xmit",	"%10u  ", mac.xmit_ct);
		PM("packets rcvd",	"%10u  ", mac.recv_ct);

		PSL("frames",		"%10.0f  ", frame_ct);
		PSL("A bit",		"%10.0f  ", a);
		PSL("C bit",		"%10.0f  ", c);
		PSL("void frames",	"%10.0f  ", junk_void);
		PSL("total junk",	"%10.0f  ", tot_junk);
		PSL("tokens issued",	"%10.0f  ", tkiss);
		PSL("tokens",		"%10.0f  ", tok_ct);

		/* token latency */
		tokens = LCTR(st->tok_ct);
		dtok = tokens-prevtoks[i];
		if (dtok != 0.) {
			/* get ring latency in usec */
			latency = st->ring_latency / 12.5;
			P("ring latency", "%6.0fusec", latency);

			latency /= 1000000.0;
			ringload = 100.0*(1.0-latency*dtok/diffclock);
			if (ringload < 0.0 && ringload > -10.0)
				ringload = 0.0;
			if (latency > 0.0 && ringload >= 0.0
			    && !first && ringload < 101.0) {
				P("ring load", "%6.0f%%", ringload);
			} else {
				P("ring load",     "%10s", "     ?%   ");
			}

			usecpertok = 1000000.0*diffclock/dtok;
			P("token latency", "%10.3f", usecpertok/1000.0);
		} else {
			P("ring latency",  "%10s", "     ?usec");
			P("ring load",     "%10s", "     ?%   ");
			P("token latency", "%10s", "     ?.?  ");
		}

		P("t_neg", "%10.3f", FROMBCLK(st->t_neg));
		P("t_max", "%10.3f", FROMBCLK(mi->mac.tmax));
		P("t_min", "%10.3f", FROMBCLK(mi->mac.tmin));
		P("t_req", "%10.3f", FROMBCLK(mi->mac.treq));
		P("tvx",   "%10.3f", FROMBCLK(mi->mac.tvx));

		NEWCOL(ysave);
		P("",	"%10s", name);

		PL("Transmit Errors:");
		PSL(" underflow",	"%10.0f", tx_under);
		PSL(" abort",		"%10.0f", xmtabt);

		PL("Receive Errors:");
		PSL(" E bit rcvd",	"%10.0f", e);
		PSL(" set E bit",	"%10.0f", err_ct);
		PSL(" bad CRC,len",	"%10.0f", error);
		PSL(" missed",		"%10.0f", misfrm);
		tmp = (LCTR(st->a)-LCTR(stp->a))-(LCTR(st->c)-LCTR(stp->c));
		P(" others' miss",	"%10.0f", tmp);
		PSL(" no host bufs",	"%10.0f", buf_ovf);
		PSL(" lost",		"%10.0f", lost_ct);
		PSL(" flushed",		"%10.0f", flsh);
		PSL(" aborted",		"%10.0f", abort);
		PSL(" small gap",	"%10.0f", miss);
		PSL(" too short",	"%10.0f", shorterr);
		PSL(" too long",	"%10.0f", longerr);
		/* IPG boards confuse multicast packets as evidence of dup
		 * addresses.
		 */
		PH(" poss dup addr",	"%10.0f", DST(pos_dup),
		   DST(pos_dup) != 0 && strncmp("ipg",name,3) != 0);
		PSL(" FIFO overflow",	"%10.0f", rx_ovf);
		PSL(" stray tokens",	"%10.0f", tkerr);

		prevtoks[i] = tokens;
		prevclock[i] = curclock;
	}
	if (Dflag) {
		bcopy(cmi, dmi, sizeof(dmi));
		bcopy(st0, dst0, sizeof(dst0));
	}
}

/*
 * Print  port info.
 */
static void
spr_phy(register int y, char *tmb)
{
	char *name;
	register int i, j;
	register SMT_PHY *phy;
	register int x, xlab;
	int ysave = ++y;
	static int prev_phy_ct;
	register MAC_INFO *mi;
	register struct smt_st *st, *stp;

	banner("2:Port",tmb);

	for (i = 0; i < ifi.mac_ct; i++) {
		mi = &cmi[i];
		st = &st0[i*2];
		if (Dflag) {
			stp = &dst0[i*2];
		} else {
			stp = &zst0[i*2];
		}

		name = ifi.names[i];
		if (map_smt(name, FDDI_SNMP_DUMP, mi, sizeof(*mi), 0)
			!= STAT_SUCCESS) {
			fail("mac dump for %s failed\n", name);
			return;
		}
		fail_cnt = NO_FAIL;
		if (mi->mac.phy_ct < 1 || mi->mac.phy_ct != prev_phy_ct) {
			first = 2;
			if (mi->mac.phy_ct < 1) {
				fail("Information not available");
				break;
			}
			prev_phy_ct = mi->mac.phy_ct;
		}

		ADJX(i);
		xlab = 0;

		for (j = 0; j < mi->mac.phy_ct; j++, st++, stp++) {
			char *pctype;
			phy = (j==0) ? &mi->phy_0 : &mi->phy_1;

			y = ysave;
			if (i == 0 && j > 0)
				ADJX(j);

			if (map_smt(name, FDDI_SNMP_STAT, st,
				    sizeof(*st), j) != STAT_SUCCESS) {
				fail("port status for %s:%d failed\n",
					name, j);
				return;
			}

			pctype = ma_pc_type_str(phy->pctype);
			P("",	"    Port %s",  pctype);
			if (nflag) {
				P("neighbor",	"%10ld", st->npc_type);
				P("PCM state",	"%10ld", st->pcm_state);
				P("PC withhold","%10ld", phy->withhold);
				P("conn state",	"%10ld", phy->conn_state);
				P("tx line state", "%10ld", st->tls);
				P("rx line state", "%10ld", st->rls);
			} else {
				P("neighbor",	"%10s",
					ma_pc_type_str(st->npc_type));
				P("PCM state",	"%10s",
					ma_pcm_state_str(st->pcm_state));
				P("PC withhold","%10s",
					sm_withhold_str(phy->withhold));
				P("conn state",	"%10s",
					sm_conn_state_str(phy->conn_state));
				P("tx line state", "%10s",
					ma_rt_ls_str2(st->tls));
				P("rx line state", "%10s",
					ma_pcm_ls_str(st->rls));
			}
			PHS("LCT failures",	"%10u", lct_tot_fail);
			PS("connects",		"%10u", connects);

			if (j <= i) {
				P("Frame errors","%10.0f",
				  DST(err_ct)+DST(lost_ct));
				P("  threshold", "%10.3f%%",
				  (double)mi->mac.fr_threshold/655.36);
				PH("  ratio", "%10.3f%%",
				   (double)mi->mac.fr_errratio/655.36,
				   mi->mac.frm_errcond);
			} else {
				y += 3;
			}

			NEWCOL(ysave);
			P("", "    Port %s", ma_pc_type_str(phy->pctype));

			PSL("noise",	"%10.0f", noise);
			PSL("elasticity ovf",	"%10.0f", eovf);
			if (j <= i) {
				PSL("Not copied err",  "%10.0f", misfrm);
				P("  threshold", "%10.3f%%",
				  (double)mi->mac.fnc_threshold/655.36);
				PH("  ratio", "%10.3f%%",
				   (double)mi->mac.nc_ratio/655.36,
				   mi->mac.frm_nccond);
			} else {
				y += 3;
			}

			PS("Link errors", "%10u", lem_count);
			PH("  estimate", "%10u", phy->ler_estimate,
			   phy->pler_cond);
			P("  alarm",	"%10u", phy->ler_alarm);
			P("  cutoff",	"%10u", phy->ler_cutoff);
			P("  long-term", "%10u", st->ler);

			y += 2;		/* skip 2 lines */
			xlab=0;
			ADJX(i);
			if (i == 0 && j > 0)
				ADJX(j);

			x = LABEL_LEN;
			if (i == 1 || j == 1) {
				y+=1;
				x+=NUM_LEN;
			}
			if (first) {
				move(y,0);
				prnt("%s flags", pctype);
			}
			move(y,x);
			clrtoeol();
			if (nflag) {
				prnt("%#10x", st->flags);
			} else {
				prnt("%s", ma_pcm_flag_str(st->flags, str1));
			}
			if (i != 1 && j != 1)
				y++;

			y++;
			ADJX(i);
			if (i == 0 && j > 0)
				ADJX(j);
			P("# signal bits", "%10u", st->n);
			x = LABEL_LEN;
			if (i == 1 || j == 1) {
				y+=2;
				x+=NUM_LEN;
			}
			if (first) {
				move(y,0);
				prnt("%s r_val", pctype);
			}
			move(y,x);
			clrtoeol();
			if (nflag) {
				prnt("%#10x", st->r_val);
			} else {
				prnt("%s", ma_pc_signal_str(st->r_val,
							    st->n, str1));
			}
			y++;
			if (first) {
				move(y,0);
				prnt("%s t_val", pctype);
			}
			move(y,x);
			clrtoeol();
			if (nflag) {
				prnt("%#10x", st->t_val);
			} else {
				prnt("%s",
				     ma_pc_signal_str(st->t_val,
						      st->n, str1));
			}
		}
	}
	if (Dflag) {
	    bcopy(cmi, dmi, sizeof(dmi));
	    bcopy(st0, dst0, sizeof(dst0));
	}
}

/*
 * Print ring management info.
 */
static void
spr_rmt(register int y, char *tmb)
{
	char *name;
	char tstr[5+1+8+1];
	int i;
	int x, xlab = 0;
	int ysave = ++y;

	register MAC_INFO *mi;
	register struct smt_st *st, *stp;

	banner("3:RMT",tmb);

	for (i = 0; i < ifi.mac_ct; i++) {
		mi = &cmi[i];
		st = &st0[i*2];
		if (Dflag) {
			stp = &dst0[i*2];
		} else {
			stp = &zst0[i*2];
		}

		name = ifi.names[i];

		ADJX(i);
		xlab = 0;
		y = ysave;

		if (map_smt(name, FDDI_SNMP_DUMP, mi, sizeof(*mi), 0)
			!= STAT_SUCCESS) {
			fail("mac dump for %s failed\n", name);
			return;
		}
		if (map_smt(name, FDDI_SNMP_STAT, st, sizeof(*st), i)
			!= STAT_SUCCESS) {
			fail("mac status for %s:%d failed\n", name, i);
			return;
		}
		fail_cnt = NO_FAIL;

		P("", "%10s", name);
		PH("ring ok",	"%10s", sm_flag_str(mi->mac.ringop),
		   !mi->mac.ringop);
		PSL("ring up cnt",	"%10.0f", rngop);
		PSL("TRT expires",	"%10.0f", trtexp);
		PSL("TVX expires",	"%10.0f", tvxexp);
		PSL("dup MAC cnt",	"%10.0f", dup_mac);

		y++;
		if (i == 0) {
			(void)cftime(tstr, "%m/%d %T", &mi->mac.boottime);
			P("started", "%-20s", tstr);
		} else {
			y++;
		}

		y++;
		(void)cftime(tstr, "%T", &mi->mac.trm_base);
		PC(i, mi->mac.trm_ck!=SMT_NEVER, "RMT timer","%10s",tstr,"");
		PH("RMT state",	"%10s", sm_rmtstate_str(mi->mac.rmtstate),
		   mi->mac.rmtstate == RM_RINGOP_DUP
		   || mi->mac.rmtstate == RM_NONOP_DUP);

		NEWCOL(ysave);
		P("", "%10s", name);
		PL("Claims received:");
		PSL("  mine",		"%10.0f", myclm);
		PSL("  lower",		"%10.0f", loclm);
		PSL("  higher",		"%10.0f", hiclm);
		PL("Beacons received:");
		PSL("  mine",		"%10.0f", mybec);
		PSL("  from others",	"%10.0f", otrbec);
		PSL("  promisc drop",	"%10.0f", junk_bec);
		PSL("Claim state",	"%10.0f", clm);
		PSL("Beacon state",	"%10.0f", bec);

		xlab=0;
		PL("RMT flags:");
		if (i == 1)
		    y++;
		if (first) {
			move(y,0);
			prnt(" %s", name);
		}
		move(y,LABEL_LEN);
		clrtoeol();
		prnt("%s", sm_rmt_flag_str(mi->mac.rmt_flag, str1));
	}
	if (Dflag) {
	    bcopy(cmi, dmi, sizeof(dmi));
	    bcopy(st0, dst0, sizeof(dst0));
	}
}

/*
 * Print config. management info.
 */
static void
spr_cfm(register int y, char *tmb)
{
	char *name;
	register int i, j;
	register SMT_PHY *phy;
	register int x, xlab;
	int ysave = ++y, save;
	static int prev_phy_ct;
	register MAC_INFO *mi;

	banner("4:Config",tmb);

	for (i = 0; i < ifi.mac_ct; i++) {
		mi = &cmi[i];

		name = ifi.names[i];
		if (map_smt(name, FDDI_SNMP_DUMP, mi, sizeof(*mi), 0)
			!= STAT_SUCCESS) {
			fail("mac dump for %s failed\n", name);
			return;
		}
		fail_cnt = NO_FAIL;
		if (mi->mac.phy_ct < 1 || mi->mac.phy_ct != prev_phy_ct) {
			first = 2;
			if (mi->mac.phy_ct < 1) {
				fail("Information not available");
				break;
			}
			prev_phy_ct = mi->mac.phy_ct;
		}

		ADJX(i);
		xlab = 0;
		y = ysave;

		P("",		   "%12s", name);
		P("path avail",	   "%12s",
			sm_pathtype_str(mi->mac.pathavail, str1));
		P("path requested", "%12s",
			sm_pathtype_str(mi->mac.pathrequested, str2));
		P("cur path",	   "%12s",
			sm_pathtype_str(mi->mac.curpath, str3));
		P("root concent",   "%12s",
			mi->mac.rootconcentrator ? "TRUE" : "FALSE");
		NEWCOL(ysave);
		P("",		   "%12s",name);
		P("DNA port",	   "%12s",
		  ma_pc_type_str((enum fddi_pc_type)mi->mac.dnaport));
		P("msloop status", "%12s",sm_msloop_str(mi->mac.msloop_stat));
		P("root DNA port", "%12s",
		  ma_pc_type_str((enum fddi_pc_type)mi->mac.root_dnaport));
		P("root cur path",   "%12s",
			sm_pathtype_str(mi->mac.root_curpath, str1));

		ADJX(i);
		save = ++y;

		for (j = 0; j < mi->mac.phy_ct; j++) {
			char *pctype;
			phy = (j==0) ? &mi->phy_0 : &mi->phy_1;

			xlab = 0;
			y = save;
			if (i == 0 && j > 0)
				ADJX(j);

			pctype = ma_pc_type_str(phy->pctype);
			P("", "    Port %s", pctype);
			P("undes. conn", "%10s", sm_flag_str(phy->uica_flag));

			P("remote MAC",	"%10s", sm_flag_str(phy->remotemac));
			P("CE state","%10s",
				sm_ce_state_str(phy->ce_state));
			P("path request","%10s",
				sm_pathtype_str(phy->pathrequested, str1));
			P("MAC placemt","%10u", phy->macplacement);
			P("path avail",	"%10s",
				sm_pathtype_str(phy->pathavail, str2));

			P("loop time",	"%10u", phy->loop_time);
			P("fotx",	"%10s", sm_fotx_str(phy->fotx));
			P("llc priority",	"%10u", phy->ip_pri);

			if (i || j)
				y++;
			if (first) {
				move(y,0);
				prnt("%s conn policy", pctype);
			}
			move(y,x);
			clrtoeol();
			prnt("%s", sm_phy_connp_str(phy->phy_connp, str1));

			NEWCOL(save);
			P("", "    Port %s", pctype);
			P("PCM target",	"%10s", ma_pcm_tgt_str(phy->pcm_tgt));
			P("maint line st",	"%10s",
				ma_pcm_ls_str(phy->maint_ls));
			P("TB max",	"%10u", phy->tb_max);
			P("break state", "%10s", sm_flag_str(phy->bs_flag));

			P("optical bypass",	"%10s",
				phy->obs ? "present":"none");
			P("OB insert max",	"%10u", phy->imax);
			P("inserted",		"%10s",
				phy->insert ? "yes" : "no");
			P("insert policy",	"%10s",
				phy->ipolicy ? "ok" : "don't");
			P("debug level",	"%10u", phy->debug);
		}
	}
}


/*
 * Print neighbor notification info.
 */
static void
spr_nn(register int y, char *tmb)
{
	int i;
	char tstr[10];
	char *name;
	register int x, xlab;
	int ysave = ++y;
	static int prev_phy_ct;
	register MAC_INFO *mi;

	banner("5:Neighbors",tmb);

	for (i = 0; i < ifi.mac_ct; i++) {
		mi = &cmi[i];

		xlab = 0;
		ADJX(i);
		y = ysave;

		name = ifi.names[i];
		if (map_smt(name, FDDI_SNMP_DUMP, mi, sizeof(*mi), 0)
			!= STAT_SUCCESS) {
			fail("NN dump for %s failed\n", name);
			return;
		}
		fail_cnt = NO_FAIL;
		if (mi->mac.phy_ct < 1 || mi->mac.phy_ct != prev_phy_ct) {
			first = 2;
			if (mi->mac.phy_ct < 1) {
				fail("Information not available");
				break;
			}
			prev_phy_ct = mi->mac.phy_ct;
		}

		P("",	"%9s", mi->mac.name);
		P("state",	"%9s", sm_nn_state_str(mi->mac.nn_state));
		P("xid",	"%#9x", mi->mac.nn_xid);
		P("UNA valid",	"%9s", mi->mac.uv_flag ? "yes" : "no");
		P("DNA valid",	"%9s", mi->mac.dv_flag ? "yes" : "no");
		PH("dup addr test","%9s",
		   sm_dupa_test_str(mi->mac.dupa_test),
		   mi->mac.dupa_test == DUPA_FAIL);
		NEWCOL(ysave);
		P("",	"%9s", mi->mac.name);
		PH("dup addr seen", "%9s", mi->mac.dupa_flag ? "yes":"no",
		   mi->mac.dupa_flag);
		PH("Upstr is dup",  "%9s", mi->mac.unda_flag ? "yes":"no",
		   mi->mac.unda_flag);
		pr_time(tstr, mi->mac.tnn);
		P("next NIF",	"%9s", tstr);
		pr_time(tstr, mi->mac.tuv);
		P("Upstr seen",	"%9s", tstr);
		pr_time(tstr, mi->mac.tdv);
		P("Dnstr seen",	"%9s", tstr);

		y += 1;
#define AFMT  "%-10s %-18.18s %-18.18s %-30.30s"
		if (i != 0)
			y+=8;		/* skip first section */

		if (first) {
			move(y,0);
			prnt(AFMT, name, "FDDI Order",
				"Canonical Order", "Host name");
		}

		y++; move(y,0);
		prnt(AFMT, "Local",
		     midtoa(&mi->mac.addr),
		     fddi_ntoa(&mi->mac.addr, str1),
		     fddi_ntohost(&mi->mac.addr, hname) == 0 ? hname : "");
		y += 2; move(y,0);
		prnt(AFMT, "Upstream",
		     midtoa(&mi->mac.una),
		     fddi_ntoa(&mi->mac.una, str1),
		     fddi_ntohost(&mi->mac.una, hname) == 0 ? hname : "");
		y++; move(y,0);
		prnt(AFMT, "  Old",
		     midtoa(&mi->mac.old_una),
		     fddi_ntoa(&mi->mac.old_una, str1),
		     fddi_ntohost(&mi->mac.old_una, hname) == 0 ? hname : "");
		y += 2; move(y,0);
		prnt(AFMT, "Downstream",
		     midtoa(&mi->mac.dna),
		     fddi_ntoa(&mi->mac.dna, str1),
		     fddi_ntohost(&mi->mac.dna, hname) == 0 ? hname : "");
		y++; move(y,0);
		prnt(AFMT, "  Old",
		     midtoa(&mi->mac.old_dna),
		     fddi_ntoa(&mi->mac.old_dna, str1),
		     fddi_ntohost(&mi->mac.old_dna, hname) == 0 ? hname : "");
	}
}

static void
spr_smt(register int y, char *tmb)
{
	register int x, xlab;
	int ysave = ++y;

	banner("6:SMT",tmb);

	if (map_smt(0, FDDI_SNMP_DUMP, &smtd,
		sizeof(smtd), devnum) != STAT_SUCCESS) {
		fail("smt status failed\n");
		return;
	}
	fail_cnt = NO_FAIL;
	xlab = 0;
	ADJX(0);

	P("Station ID", "%18s", sidtoa(&smtd.sid));
	P("cur version",  "%10u", smtd.vers_op);
	P("low version",  "%10u", smtd.vers_lo);
	P("high version",  "%10u", smtd.vers_hi);
	P("station type", "%10s", ma_st_type_str(smtd.st_type));
	P("XID",	"%#10x", smtd.xid);

	P("ECM state", "%10s", sm_ecm_state_str(smtd.ecmstate));
	P("CFM state", "%10s", sm_cfm_state_str(smtd.cf_state));
	P("hold state", "%10s", sm_hold_state_str(smtd.hold_state));
	P("rem disconn", "%10s", sm_flag_str(smtd.rd_flag));
	PH("topology", "%10s", sm_topology_str(smtd.topology, str1),
	   smtd.topology & (SMT_AA_TWIST|SMT_AA_TWIST));
	PL("Manufacturer data:");
	sprintf(str1, "%02x:%02x:%02x",
			(int)smtd.manuf.manf_oui[0],
			(int)smtd.manuf.manf_oui[1],
			(int)smtd.manuf.manf_oui[2]);
	P(" OUI",	"%10s", str1);
	P(" data",	"%10s", (char*)&smtd.manuf.manf_data[0]);
	P("User data",  "%10s", smtd.userdata.b);

	NEWCOL(ysave);
	P("MAC count",	"%10u",	smtd.mac_ct);
	P("nonmaster ct",	"%10u",	smtd.nonmaster_ct);
	P("master ct",	"%10u",	smtd.master_ct);
	P("path avail",	"%10s",	sm_pathtype_str(smtd.pathavail, str1));
	P("config cap", "%10s", sm_conf_policy_str(smtd.conf_cap, str1));
	P("config policy", "%10s", sm_conf_policy_str(smtd.conf_policy,str1));
	P("conn policy", "%10s", sm_conn_policy_str(smtd.conn_policy, str2));
	P("report limit","%10u", smtd.reportlimit);
	P("t_notify",	"%10u",	smtd.t_notify);
	P("status report","%10s", sm_flag_str(smtd.srf_on));
}


/* change device */
static void
next_dev(int delta) {
	int j;

	sm_openlog(sflag ? SM_LOGON_SYSLOG : SM_LOGON_STDERR,
		   0, "smtstat", 0, LOG_USER);
	for (j = 0; j < NSMTD; j++) {
		devnum += delta;
		if (devnum < 0)
			devnum = NSMTD-1;
		else if (devnum >= NSMTD)
			devnum = 0;
		if (map_smt(0,FDDI_SNMP_INFO, &ifi,sizeof(ifi), devnum)
		    == STAT_SUCCESS)
			break;
	}

	sm_openlog(sflag ? SM_LOGON_SYSLOG : SM_LOGON_STDERR,
		   LOG_ERR, "smtstat", 0, LOG_USER);

	first = 2;
	bzero(cmi, sizeof(cmi));
	bzero(st0, sizeof(st0));
	bzero(zmi, sizeof(zmi));
	bzero(zst0, sizeof(zst0));
	bzero(dmi, sizeof(dmi));
	bzero(dst0, sizeof(dst0));
}


#define NUMFUNCS 6
static void (*sprfuncs[NUMFUNCS])(int, char *) = {
	spr_mac,
	spr_phy,
	spr_rmt,
	spr_cfm,
	spr_nn,
	spr_smt,
};

const char min_cmd_num = '1';
const char max_cmd_num = '1' + NUMFUNCS - 1;

static void
prnt(char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (sflag) {
		if (win->_cury < BOS)
			(void)vwprintw(win, fmt, args);
	} else {
		(void)vprintf(fmt, args);
	}
	va_end(args);
}


/*
 * Print a description of the station.
 */
static void
spr(void)
{
	static char tmb[]  = "MMM DD HH:MM:SS";
	time_t t;
	struct tms dummy;
	struct timeval wait;
	fd_set rmask;
	struct termio  tb;
	int c, n;
	int intrchar;       /* interrupt character */
	int suspchar;       /* job control character */

	if (interval == 0)
		interval = 1;	/* default to 1 sec interval */

	bzero(cmi, sizeof(cmi));
	bzero(st0, sizeof(st0));
	bzero(zmi, sizeof(zmi));
	bzero(zst0, sizeof(zst0));
	bzero(dmi, sizeof(dmi));
	bzero(dst0, sizeof(dst0));

	win = initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	leaveok(stdscr, FALSE);
	nonl();
	intrflush(stdscr,FALSE);
	move(0, 0);

	(void) ioctl(0, TCGETA, &tb);
	intrchar = tb.c_cc[VINTR];
	suspchar = tb.c_cc[VSUSP];

	switch (smtd.st_type) {
	    case SM_DAS:
		ifi.mac_ct = 1;
		if (Iflag)		/* user specified interface */
			strcpy(ifi.names[0], interface);
		break;
	    case DM_DAS:
		ifi.mac_ct = 2;
		if (Iflag) {
			unit &= ~1;	/* treat devices 0 & 1 as same */
			sprintf(ifi.names[0], "%s%d", devname, unit);
			sprintf(ifi.names[1], "%s%d", devname, unit+1);
		}
		break;
	}

	/* default = delta */
	Dflag = 1;
	Zflag = 0;
	FD_ZERO(&rmask);

	for (;;) {
		if (first) {
			clear();

			if (map_smt(0, FDDI_SNMP_DUMP, &smtd,
				sizeof(smtd), devnum) != STAT_SUCCESS) {
				fail("smt status failed\n");
				sginap(HZ);
				continue;
			}
			fail_cnt = NO_FAIL;
		}

		if (needhead || first) {
			move(BOS, 0);
			standout();
			printw(" 1:MAC  2:Port  3:RMT  4:Config  5:Neighbors"
			       "  6:SMT"
			       "   +/-:Interface   DZRN:Mode");
			standend();
			needhead = 0;
		}

		t = time(0);
		(void)cftime(tmb,"%T",&t);
		move(0, 0);
		sprfuncs[scmd](1, tmb);

		if (first > 1) {
			if (!Dflag) {
				if (Zflag)
					t = time(0);
				else
					t = time(0) - times(&dummy)/HZ;
				(void)cftime(delta_date, BANNER_DPAT, &t);
			}
			if (Dflag || Zflag)
				sprdelta();
			if (Zflag)
				sprzero();
		}
		first = 0;
		move(0, 0);
		refresh();

		/* Try to delay for exactly the right interval, adjusted
		 * for time spent in displaying things.  This is to
		 * make the token count and ring load calculations
		 * more accurate.  Do it the simplest way by waiting
		 * until the start of the next second, unless we are within
		 * a tenth of the next second.
		 */
		(void)gettimeofday(&wait, 0);
		wait.tv_sec = interval;
		wait.tv_usec += 250000;
		if (wait.tv_usec > 1000000)
			wait.tv_usec -= 1000000;
		if (wait.tv_usec != 0) {
			wait.tv_usec = 1000000 - wait.tv_usec;
			if (wait.tv_usec >= 1000000/5)
				wait.tv_sec--;
		}
		FD_SET(0, &rmask);
		n = select(1, &rmask, NULL, NULL, &wait);
		if (n < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fail("select: %s", strerror(errno));
			break;
		} else if (n == 1) {
			c = getch();
			switch (c) {
			case 'q':
			case 'Q':
				quit(0);
				break;
			case 'd':
			case 'D':
				first = 2;
				Dflag = 1; sprdelta();
				Zflag = 0; sprzero();
				break;
			case 'z':
			case 'Z':
				first = 2;
				Dflag = 0;
				Zflag = 1; sprzero();
				break;
			case 'r':
			case 'R':
				first = 2;
				Dflag = 0;
				Zflag = 0; sprzero();
				break;
			case '\f':
				first = 1;
				break;
			case 'n':
			case 'N':
				nflag = !nflag;
				break;
			case '+':
			case '=':	/* in case of typos */
				next_dev(1);
				break;
			case '-':
			case '_':	/* in case of typos */
				next_dev(-1);
				break;
			default:
				if (c == intrchar) {
					quit(0);
				} else if (c == suspchar) {
					reset_shell_mode();
					kill(getpid(), SIGTSTP);
					reset_prog_mode();
				} else if (c >= min_cmd_num
					   && c <= max_cmd_num) {
					scmd = c - '1';
					first = 1;
				}
			}
		}
	}
}
