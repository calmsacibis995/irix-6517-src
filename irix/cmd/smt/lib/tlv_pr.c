/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.13 $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smtd_parm.h>
#include <smtd.h>
#include <smt_subr.h>
#include <smt_snmp_clnt.h>
#include <smtd_fs.h>
#include <sm_map.h>
#include <sm_addr.h>
#include <sm_str.h>
#include <ma_str.h>
#include <tlv_sm.h>

/*
 *
 */
void
pr_parm(FILE *f, char *buf, int cc)
{
	char *info;
	u_long infolen, maxinfo;
	PARM_UNION uu;
	char str[256];
	char str2[256];
	int macs = 0;
	int phys = 0;
	register smt_header *fp;
	register SMT_FB *fb = (SMT_FB *)buf;

	if (cc > sizeof(*fb)) {
		printf("Too large packet:\n");
		goto ret_dump;
	}

	fp = &fb->smt_hdr;
	info = FBTOINFO(buf);
	infolen = MIN(ntohs(fp->len), (cc+buf-info));
	maxinfo = (u_long)((char*)fb+sizeof(*fb));
	while ((int)infolen > 0) {
		u_long type, len, peeklen, nextlen;
		char *nextparam;

		/* param boundary */
		if ((u_long)info & 0x1) {
			printf("Unaligned parameter:\n");
			goto ret_dump;
		}

		/* peek type */
		type = ntohs(*((u_short *)info));
		peeklen = ntohs(((u_short *)info)[1]);
		nextparam = info + peeklen + TLV_HDRSIZE;
		nextlen = infolen - peeklen - TLV_HDRSIZE;
		if ((u_long)nextparam >= maxinfo) {
			char hstr[64];
			printf("%s -- BAD %s len=%d\n",
				fddi_ntoa(&FBTOMH(fb)->mac_sa, hstr),
				sm_ptype_str(type), peeklen);
			return;
		}

		switch (type) {
		case PTYPE_UNA: {
			tlv_parse_una(&info, &infolen, &uu.una.una);
			if (fddi_ntohost(&uu.una.una, str))
				fddi_ntoa(&uu.una.una, str);
			fprintf(f, "UNA=%s\n", str);
			break;
		}
		case PTYPE_STD:
			tlv_parse_std(&info, &infolen, &uu.std);
			fprintf(f,
			"Station Dscrpt: %d MAC, %d masters, %d nonmaster %s\n",
				uu.std.mac_ct,
				uu.std.master_ct,
				uu.std.nonmaster_ct,
				sm_nodeclass_str(uu.std.nodeclass));
			macs = uu.std.mac_ct;
			phys = uu.std.master_ct + uu.std.nonmaster_ct;
			break;
		case PTYPE_STAT:
			tlv_parse_stat(&info, &infolen, &uu.stat);
			fprintf(f, "Station States: topology=%s, dupa=%x\n",
				sm_topology_str(uu.stat.topology, str),
				uu.stat.dupa);
			break;
		case PTYPE_MTS:
			tlv_parse_mts(&info, &infolen, &uu.mts);
			fprintf(f, "Msg Timestamp:  %d.%06u\n",
				uu.mts.tv_sec, uu.mts.tv_usec);
			break;
		case PTYPE_SP:
			tlv_parse_sp(&info, &infolen, &uu.sp);
			fprintf(f, "Station Policy: conf=%s, conn=%s\n",
				sm_conf_policy_str(uu.sp.conf_policy, str),
				sm_conn_policy_str(uu.sp.conn_policy, str2));
			break;
		case PTYPE_PL:
			tlv_parse_pl(&info, &infolen, &uu.pl);
			fprintf(f, "Path Latency: i1=%d, r1=%d, i2=%d, r2=%d\n",
				uu.pl.indx1, uu.pl.ring1,
				uu.pl.indx2, uu.pl.ring2);
			break;
		case PTYPE_MACNBRS:
			tlv_parse_macnbrs(&info, &infolen, &uu.macnbrs);
			fprintf(f, "MAC%d Nbrs:      una=%s, dna=%s\n",
				uu.macnbrs.macidx,
				fddi_ntoa(&uu.macnbrs.una,str),
				fddi_ntoa(&uu.macnbrs.dna,str2));
			break;
		case PTYPE_VERS: {
			u_long opvers, hivers, lovers;

			tlv_parse_vers(&info,&infolen,&opvers,&hivers,&lovers);
			fprintf(f, "Supported Vers: v%d-v%d, opvers=%d\n",
					lovers, hivers, opvers);
			break;
		}
		case PTYPE_PDS: {
			char tmp[512];
			u_long n, m;
			PARM_PD_PHY *pdp;
			PARM_PD_MAC *pdm;

			fprintf(f, "Path Descriptors:\n");
			tlv_parse_pds(&info,&infolen,(PARM_PD_MAC*)&tmp[0],&n);
			for (n=0, pdp=(PARM_PD_PHY*)tmp; n<phys; n++,pdp++) {
				fprintf(f, "\tport%d:  pc=%s state=%s pc_nbr=%s remotemac=%d conn_rid=%d\n",
				n+1,
				ma_pc_type_str((enum fddi_pc_type)pdp->pc_type),
				sm_conn_state_str((PHY_CONN_STATE)pdp->conn_state),
				ma_pc_type_str((enum fddi_pc_type)pdp->pc_nbr),
				pdp->rmac_indecated,
				(int)pdp->conn_rid);
			}
			for (m=0, pdm=(PARM_PD_MAC*)pdp; m<macs; m++,pdm++) {
				fprintf(f, "\tmac%d:   addr=%s conn_rid=%d\n",
					m+n+1,
					fddi_ntoa(&pdm->addr, str),
					(int)pdm->rid);
			}
			break;
		}
		case PTYPE_MAC_STAT:
			tlv_parse_mac_stat(&info, &infolen, &uu.mac_stat);
			fprintf(f, "MAC%d Status:\ttreq=%3.3f tneg=%3.3f "
				"tmax=%3.3f tvx=%3.3f tmin=%3.3f\n",
				uu.mac_stat.macidx,
				FROMBCLK(uu.mac_stat.treq),
				FROMBCLK(uu.mac_stat.tneg),
				FROMBCLK(uu.mac_stat.tmax),
				FROMBCLK(uu.mac_stat.tvx),
				FROMBCLK(uu.mac_stat.tmin));

			fprintf(f, "\t\tsba=%d frame_ct=%d err_ct=%d "
				"lost_ct=%d\n",
				uu.mac_stat.sba,
				uu.mac_stat.frame_ct,
				uu.mac_stat.error_ct,
				uu.mac_stat.lost_ct);
			break;
		case PTYPE_LER:
			tlv_parse_ler(&info, &infolen, &uu.ler);
			fprintf(f, "LER%d:\t\tcutoff=%d alarm=%d estimate=%d "
				"reject_ct=%d, lem_ct=%d\n",
				uu.ler.phyidx,
				uu.ler.ler_cutoff,
				uu.ler.ler_alarm,
				uu.ler.ler_estimate,
				uu.ler.lem_reject_ct,
				uu.ler.lem_ct);
			break;
		case PTYPE_MFC:
			tlv_parse_mfc(&info, &infolen, &uu.mfc);
			fprintf(f, "Frame Cnt MAC%d: recv_ct=%d xmit_ct=%d\n",
				uu.mfc.macidx, uu.mfc.recv_ct,
				uu.mfc.xmit_ct);
			break;
		case PTYPE_MFNCC:
			tlv_parse_mfncc(&info, &infolen, &uu.mfncc);
			fprintf(f, "Frame NCC MAC%d: notcopied_ct=%d\n",
				uu.mfncc.macidx, uu.mfncc.notcopied_ct);
			break;
		case PTYPE_MPV:
			tlv_parse_mpv(&info, &infolen, &uu.mpv);
			fprintf(f, "Priority MAC%d:\t%d %d %d %d %d %d %d\n",
				(int)uu.mpv.macidx,
				uu.mpv.pri0,
				uu.mpv.pri1,
				uu.mpv.pri2,
				uu.mpv.pri3,
				uu.mpv.pri4,
				uu.mpv.pri5,
				uu.mpv.pri6);
			break;
		case PTYPE_EBS:
			tlv_parse_ebs(&info, &infolen, &uu.ebs);
			fprintf(f, "EBS phy%d:       eberr_ct=%d\n",
				uu.ebs.phyidx, uu.ebs.eberr_ct);
			break;
		case PTYPE_MF:
			tlv_parse_mf(&info, &infolen, (SMT_MANUF_DATA*)&uu);
			fprintf(f, "Manufacturer Data: %02x-%02x-%02x:%.*s\n",
					(int)((char*)&uu)[0],
					(int)((char*)&uu)[1],
					(int)((char*)&uu)[2],
					sizeof(uu.manuf.manf_data),
					&((char*)&uu)[3]);
			break;
		case PTYPE_USR:
			tlv_parse_usr(&info, &infolen, &uu.user);
			fprintf(f, "User Data:      '%.*s'\n",
				sizeof(uu.user), (char*)&uu);
			break;
		case PTYPE_RC:
			tlv_parse_rc(&info, &infolen, &uu.rc.reason);
			fprintf(f, "Reason Code: %s\n",ma_rc_str(uu.rc.reason));
			break;
		case PTYPE_MFC_COND:
			tlv_parse_mfc_cond(&info, &infolen, &uu.mfc_cond);
			fprintf(f, "MAC%d: frame error condition %s, ratio=%d @%d.%06us\n",
				uu.mfc_cond.macidx,
				sm_cond_str(uu.mfc_cond.cs),
				uu.mfc_cond.mer,
				uu.mfc_cond.mbmts.tv_sec,
				uu.mfc_cond.mbmts.tv_usec);
			fprintf(f, "       base_frame_ct=%d frame_ct=%d\n",
					uu.mfc_cond.mbfc, uu.mfc_cond.mfc);
			fprintf(f, "       base_error_ct=%d error_ct=%d\n",
					uu.mfc_cond.mbec, uu.mfc_cond.mec);
			fprintf(f, "       base_lost_ct=%d lost_ct=%d\n",
					uu.mfc_cond.mblc, uu.mfc_cond.mlc);
			break;
		case PTYPE_PLER_COND:
			tlv_parse_pler_cond(&info, &infolen, &uu.pler_cond);
			fprintf(f, "Port%d: LER condition %s(%d.%06u)\n",
				uu.pler_cond.phyidx,
				sm_cond_str(uu.pler_cond.cs),
				uu.pler_cond.bmts.tv_sec,
				uu.pler_cond.bmts.tv_usec);
			fprintf(f, "       cutoff=%d alarm=%d\n",
				uu.pler_cond.cutoff, uu.pler_cond.alarm);
			fprintf(f, "       base_estimate=%d estimate=%d\n",
				uu.pler_cond.ble, uu.pler_cond.estimate);
			fprintf(f, "       base_ler_ct=%d ler_ct=%d\n",
				uu.pler_cond.blc, uu.pler_cond.ler_ct);
			fprintf(f, "       base_reject_ct=%d reject_ct=%d\n",
				uu.pler_cond.blrc, uu.pler_cond.ler_reject_ct);
			break;

		case PTYPE_MFNC_COND:
			tlv_parse_mfnc_cond(&info, &infolen, &uu.mfnc_cond);
			fprintf(f, "MAC%d: Not Copied condition %s, nc_ratio=%d(%d.%06u)\n",
				uu.mfnc_cond.macidx,
				sm_cond_str(uu.mfnc_cond.cs),
				uu.mfnc_cond.nc_ratio,
				uu.mfnc_cond.bmts.tv_sec,
				uu.mfnc_cond.bmts.tv_usec);
			fprintf(f, "       receive_ct=%d notcopied_ct=%d\n",
				uu.mfnc_cond.rcc, uu.mfnc_cond.ncc);
			fprintf(f, "       base_receive_ct=%d base_notcopied_ct=%d\n",
				uu.mfnc_cond.brcc, uu.mfnc_cond.bncc);
			break;

		case PTYPE_DUPA_COND:
			tlv_parse_dupa_cond(&info, &infolen, &uu.dupa_cond);
			fprintf(f, "MAC%d: DUP_ADDR %s detected\n",
					uu.dupa_cond.macidx,
					uu.dupa_cond.cs ? "": "NOT");
			fprintf(f, "       dupa=%s una_dupa=%s\n",
					fddi_ntoa(&uu.dupa_cond.dupa, str2),
					fddi_ntoa(&uu.dupa_cond.una_dupa, str));
			break;
		case PTYPE_UICA_EVNT:
			tlv_parse_uica_evnt(&info, &infolen, &uu.uica_evnt);
			fprintf(f, "PORT%d: %s to %s connect attempt %s.\n",
				uu.uica_evnt.phyidx,
				ma_pc_type_str((enum fddi_pc_type)uu.uica_evnt.pc_nbr),
				ma_pc_type_str((enum fddi_pc_type)uu.uica_evnt.pc_type),
				uu.uica_evnt.accepted?"ACCEPTED":"REJECTED");
			break;
		case PTYPE_TRAC_EVNT:
			tlv_parse_trac_evnt(&info, &infolen, &uu.trac_evnt);
			fprintf(f, "TRACE STATUS: %s%s%s.\n",
				uu.trac_evnt.trace_start ? "PT_INIT" : "",
				uu.trac_evnt.trace_prop ? "->PT_PROP" : "",
				uu.trac_evnt.trace_term ? "->PT_TERM" : "");
			break;
		case PTYPE_NBRCHG_EVNT:
			tlv_parse_nbrchg_evnt(&info, &infolen, &uu.nbrchg_evnt);
			if (uu.nbrchg_evnt.cs == 0) {
				fprintf(f, "no Nbr changed\n");
				break;
			}

			if (uu.nbrchg_evnt.cs&1) {
				fprintf(f, "Upstream Nbr changed %s -> %s\n",
					fddi_ntoa(&uu.nbrchg_evnt.old_una,str2),
					fddi_ntoa(&uu.nbrchg_evnt.una, str));
			}
			if (uu.nbrchg_evnt.cs&2) {
				fprintf(f, "Downstream Nbr changed %s -> %s\n",
					fddi_ntoa(&uu.nbrchg_evnt.old_dna,str2),
					fddi_ntoa(&uu.nbrchg_evnt.dna, str));
			}
			break;

		case PTYPE_PEB_COND:
			tlv_parse_ebs(&info, &infolen, &uu.ebs);
			fprintf(f, "EBS condition=%s  rid=%d count=%d\n",
				uu.peb_cond.cs ? "ON" : "OFF",
				uu.peb_cond.phyidx, uu.peb_cond.errcnt);
			break;

		case PTYPE_PD_EVNT:
		case PTYPE_RFB:
		case PTYPE_ECHO:
		case PTYPE_AUTH:
		case PTYPE_ESF:
		default:
			if (tlv_parse_tl(&info, &infolen, &type, &len))
				break;
			fprintf(f, "type=%x, len=%d: suppressed\n", type, len);
			info += len;
			infolen -= len;
			break;
		}

		if (info != nextparam || infolen != nextlen) {
			printf("BAD parameter type=%x len=%d\n", type, peeklen);
			infolen = nextlen;
			info = nextparam;
		}
	}
	fflush(f);
	return;

ret_dump:
	sm_hexdump(LOG_DEBUG, buf, cc);
}
