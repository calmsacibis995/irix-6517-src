/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.26 $
 */

#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>		/* For sysconf() */

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smt_mib.h>
#include <smt_snmp.h>
#include <smt_snmp_impl.h>
#include <smt_snmp_api.h>

#include <smtd_parm.h>
#include <smtd.h>
#include <smtd_snmp.h>
#include <smtd_svc.h>
#include <smtd_fs.h>
#include <smt_snmp_clnt.h>
#include <ma.h>
#include <sm_map.h>

extern struct sockaddr_in supersin;
static struct snmp_pdu *smtd_cur_inpdu;
/*
 * Find the tree node for a given oid string.
 *
 *	- oid str takes max 3 for tlv-mib vars: id-[subid]-type.
 *	- rid is in data not identifier.
 *	- oid list includes sub children list from
 *	  parent tree.
 */
struct tree *
find_oid(struct tree *parent, int pos, oid *subid, int oidlen)
{
    	struct tree *subtree;

	if (pos >= oidlen) 
		return(parent);

	subtree = parent->child_list;
	while (subtree) {
		if (subtree->subid == subid[pos]) {
			pos++;
			return(find_oid(subtree, pos, subid, oidlen));
		}
		subtree = subtree->next_peer;
	}
	return(NULL);
}

int
type_to_oid(register int type, register oid *subid)
{
	subid[0] = (type & 0xf000) >> 12;

	if ((type & 0x0f00) == 0) {
		subid[1] = type & 0x00ff;
		return(2);
	}

	subid[1] = (type & 0x0f00) >> 8;
	subid[2] = type & 0x00ff;
	return(3);
}

#ifdef DO_TRAP
/*
 * Signal handler to generate trap of quit.
 */
void
quit()
{
    	if (smtd_trapSession != NULL) {
	    	sm_log(LOG_EMERG, 0, "Not supported coldStart Trap yet");
		/* don't do anything */
		return;
    	}

    	sm_log(LOG_EMERG, 0, "quitting");
    	abort(0);
}
#endif

/*
 * SNMP authentication
 */
char *
snmp_auth(char *data, int *length,
	  char *community, int community_len)
{
    	if (strcmp(community,"public") == 0)
		return(data);

#ifdef DO_TRAP
	sm_log(LOG_NOTICE, 0, "community authentication failure");
	if (smtd_trapSession == NULL)
		return(NULL);
#endif

	sm_log(LOG_NOTICE, 0, "TRAP not supported yet");
	return(NULL);

}

/*
 * Callback routine that response to the requests
 */
int
snmp_trapinput(int op, struct snmp_session *session, int reqid,
				struct snmp_pdu *inpdu, void *magic)
{
	return(0);			/* do nothing */
}

/*
 * Callback routine that responds to the requests
 */
int
snmp_input(
	int op,
	struct snmp_session *session,
	int reqid,
	struct snmp_pdu *inpdu,
	void *magic)
{
    	struct snmp_pdu *outpdu;
    	struct variable_list *var,*outvar,**nextout;
    	struct tree *subtree;
    	int index = 1;

	SNMPDEBUG((LOG_DBGSNMP, 0, "snmp_input invoked"));
    	if (op == TIMED_OUT) {
		sm_log(LOG_WARNING, 0, "input timeout");
		return(0);
    	}

	session->xid = 0;
    	smtd_cmd = inpdu->command;
	/*
	 * GET_RSP_MSG, TRP_REQ_MSG not supported.
	 * and let the requestor timed out.
	 */
    	if (smtd_cmd != GET_REQ_MSG &&
	    smtd_cmd != GETNEXT_REQ_MSG &&
	    smtd_cmd != SET_REQ_MSG) {
		sm_log(LOG_NOTICE, 0, "cmd(%x) not supported", smtd_cmd);
		return(0);
	}

	smtd_cur_inpdu = inpdu;
	SNMPDEBUG((LOG_DBGSNMP, 0, "%s Request from %s:%d",
	    (smtd_cmd == GET_REQ_MSG) ? "Get" :
	    (smtd_cmd == GETNEXT_REQ_MSG) ? "GetNext" : "Set",
	    inet_ntoa(inpdu->address.sin_addr),(int)inpdu->address.sin_port));

/*jck?*/
	outpdu = snmp_pdu_create(GET_RSP_MSG);
	outpdu->reqid = inpdu->reqid;
	outpdu->address = inpdu->address;
	nextout = &outpdu->variables;

	SNMPDEBUG((LOG_DBGSNMP,0,"PDU created, inputvar=%x", inpdu->variables));
	for (var=inpdu->variables; var!=NULL; var=var->next_variable,index++) {
	    	/* Look up variable in MIB */
	    	outvar = (struct variable_list *)Malloc(sizeof *outvar);
	    	subtree = make_outvar(var, outvar);
	    	if (subtree == NULL) {
			SNMPDEBUG((LOG_DBGSNMP, 0, "make_outvar failed"));
			outpdu->errstat = SNMP_ERR_NOSUCHNAME;
			outpdu->errindex = index;
			goto sendit;
	    	}

#ifdef DEBUG
	    	if (smtd_debug >= LOG_DEBUG) {
			char buf[256];
			find_name(var, buf);
			sm_log(LOG_DEBUG, 0, "Input Variable: %s", buf);
	    	}
#endif

	    	/* Fill in value for output variable */
	    	if (smtd_cmd == GETNEXT_REQ_MSG) {
			while (1) {
		    		outpdu->errstat = find_value(subtree, outvar);
		    		if (outpdu->errstat == SNMP_ERR_NOERROR)
					break;
		    		subtree = find_next(outvar);
		    		if (subtree == NULL)
					break;
			}
	    	} else
			outpdu->errstat = find_value(subtree, outvar);

	    	if (outpdu->errstat != SNMP_ERR_NOERROR) {
			outpdu->errindex = index;
			if (smtd_debug >= LOG_INFO) {
		    		char buf[256];
		    		find_name(outvar, buf);
		    		sm_log(LOG_INFO, 0, "Can't get value of %s: %s",
					buf, snmp_errstring(outpdu->errstat));
			}
			goto sendit;
	    	}

#ifdef DEBUG
		if (smtd_debug >= LOG_DEBUG) {
			char pbuf[1025];

			find_name(outvar, pbuf);
			sm_log(LOG_DEBUG, 0, "Output Variable: %s", pbuf);
	    		if ((subtree->printer) && (outvar->val_len <= 256)) {
				(*subtree->printer)(pbuf,outvar,subtree->enums);
				sm_log(LOG_DEBUG, 0, "Value: %s", pbuf);
	    		}
		}
#endif

	    	/* Link output variable onto output PDUs list */
	    	*nextout = outvar;
	    	while (outvar->next_variable)
			outvar = outvar->next_variable;
	    	nextout = &outvar->next_variable;
	    	*nextout = NULL;
	}

sendit:
	SNMPDEBUG((LOG_DBGSNMP, 0, "responding"));
    	if (snmp_send(session, outpdu) == 0)
		sm_log(LOG_WARNING, 0, "error sending reply");

	/*first choice */
	snmp_free_pdu(outpdu);
	return(1);
}


/*
 * Make the output variable from the input variable
 */
struct tree *
make_outvar(struct variable_list *var, struct variable_list *outvar)
{
    	struct tree *subtree = smtd_mib;
    	oid outoid[128];
    	u_int size;

    	outvar->name = outoid;
    	subtree = build_variable(subtree, var, outvar, 0);
    	if (subtree != NULL) {
		size = outvar->name_length * sizeof(oid);
		outvar->name = (oid *)Malloc(size);
		bcopy(outoid, outvar->name, size);
		if (smtd_cmd == SET_REQ_MSG) {
			outvar->val.string = var->val.string;
			outvar->val_len = var->val_len;
			var->val.string = NULL;
			var->val_len = 0;
		}
    	}
    	return(subtree);
}


/*
 * Recursively walk down the MIB tree building an output variable
 */
struct tree *
build_variable(struct tree *subtree, struct variable_list *invar,
			struct variable_list *outvar, int pos)
{
    	struct tree *rtree, *last;
    	int do_next = 0;

    	if (pos == invar->name_length) {
		SNMPDEBUG((LOG_DBGSNMP, 0,
			"build_variable: END of name(len=%d)",
			(int)invar->name_length));
		return(NULL);
	}
	SNMPDEBUG((LOG_DBGSNMP, 0, "build_variable: name[%d]=%d, tree=%x",
		pos, (int)invar->name[pos], subtree));

    	/* Try to match invar->name[pos] with subid in tree */
    	for ( ; subtree != NULL; subtree = subtree->next_peer) {

		/* jck: Treat SEQUENCE node as end node */
		if (subtree->type == TYPE_SEQUENCE) {
			SNMPDEBUG((LOG_DBGSNMP, 0,
					"build_var: SEQUENCE return NULL"));
			return(NULL);
		}

		/* non leaf-node getnext -> start from 1 */
		if ((invar->name[pos] == 0) &&
		    (subtree->subid != OID_ANY) &&
		    (smtd_cmd == GETNEXT_REQ_MSG)) {
			SNMPDEBUG((LOG_DBGSNMP, 0, "build_variable: OIDANY"));
			invar->name[pos] = 1;
			do_next = 1;
		}

		/* Browse siblings */
		if (!(subtree->subid == invar->name[pos] ||
	    		subtree->subid == OID_ANY || do_next == 1)) {
			continue;
		}

		/* got matching entry */
	    	if (subtree->subid == OID_ANY)
			outvar->name[pos] = invar->name[pos];
	    	else
			outvar->name[pos] = subtree->subid;
	    	outvar->name_length = ++pos;

    		/* Recurse */
	    	rtree = build_variable(subtree->child_list,invar,outvar,pos);
	    	if (rtree != NULL)
	        	return rtree;

		/* jck: null-oid or rid gets attached iff any */
	    	/* End of the tree or variable */
	    	if ((smtd_cmd == GET_REQ_MSG) || (smtd_cmd == SET_REQ_MSG)) {
			SNMPDEBUG((LOG_DBGSNMP, 0,
				"build_variable: REQ_MSG, pos=%d, len=%d",
				pos, (int)invar->name_length));
			while (pos != invar->name_length) {
		    		outvar->name[pos] = invar->name[pos];
		    		outvar->name_length = ++pos;
			}
			return subtree;
	    	}

		/* jck: getnext overrun, just error case. */
	    	/* Find the next variable */
	    	if ((pos != invar->name_length) && (do_next != 1)) {
			/* End of the tree */
			--pos;
			do_next = 1;
			continue;
	    	}

		/* jck: make sure to get leaf node, sigle child assumed. */
		last = subtree;
		/* End of the variable, continue to descend */
		for (subtree = subtree->child_list; subtree != NULL;
			last = subtree,	subtree = subtree->child_list) {
		    	outvar->name[pos] = subtree->subid;
		    	outvar->name_length = ++pos;
		}
		if (last->subid != OID_ANY) {
		    	outvar->name[pos] = 0;
		    	outvar->name_length++;
		}
		return(last);
	}

	SNMPDEBUG((LOG_DBGSNMP, 0, "build_variable: NOT found"));
	/* Not found */
	return(NULL);
}


/*
 * Find the tree node for a variable
 */
struct tree *
find_variable(struct tree *parent, struct variable_list *var)
{
    	struct tree *subtree;
    	u_int pos = 0;

    	if (pos == var->name_length)
		return(NULL);

    	for (subtree = parent; subtree != NULL; ) {
		if (subtree->subid == var->name[pos]) {
	    		if (++pos == var->name_length)
				return(subtree);
	    		parent = subtree;
	    		subtree = subtree->child_list;
		} else
	    		subtree = subtree->next_peer;
    	}

    	return(parent);
}


/*
 * Form the name of a variable
 */
void
find_name(struct variable_list *var, char *buf)
{
    	struct tree *subtree;
    	u_int pos = 0;

    	if (pos == var->name_length) {
		*buf = '\0';
		return;
    	}

    	*buf++ = '.';
    	for (subtree = smtd_mib; subtree != NULL; ) {
		if (subtree->subid == var->name[pos]) {
	    		strcpy(buf, subtree->label);
	    		while (*buf)
				buf++;
	    		if (++pos == var->name_length)
				return;
	    		subtree = subtree->child_list;
	    		*buf++ = '.';
		} else
	    		subtree = subtree->next_peer;
    	}

    	while (pos != var->name_length) {
		sprintf(buf, "%u.", var->name[pos++]);
		while (*buf)
	    		buf++;
    	}

    	*(buf-1) = '\0';
    	return;
}


/*
 * Find the next variable given the current legal variable
 */
struct tree *
find_next(struct variable_list *var)
{
    	struct tree *st;	/* subtree */
	struct tree *p;		/* parent */
    	u_int pos = 0;

    	/* Go to the current subtree and position in the name */
    	for (p = st = smtd_mib; st != NULL; ) {
		if (st->subid == var->name[pos]) {
	    		p = st;
	    		st = st->child_list;
	    		pos++;
		} else
	    		st = st->next_peer;
    	}

    	/* Move back up until there is a next */
    	for (; (p != NULL) && (p->next_peer == NULL); p = p->parent)
		pos--;

    	/* End of tree */
    	if (p == NULL)
		return(p);
 
    	p = p->next_peer;
    	pos--;

    	/* Go back down filling in new name */
    	for (st = p; st != NULL; p = st, st = st->child_list)
		var->name[pos++] = st->subid;

    	if (p->subid != OID_ANY)
		var->name[pos++] = 0;

    	var->name_length = pos;
    	return(p);
}

static int
octet_var(struct variable_list *var, int len, void *val)
{
	var->type = ASN_OCTET_STR;
	/* snmp's magic number */
	if (len > PACKET_LENGTH)
		len = PACKET_LENGTH;
	var->val_len = len;
	var->val.string = Malloc(len);
	bcopy(val, var->val.string, len);
	return(SNMP_ERR_NOERROR);
}

static int
int_var(struct variable_list *var, int val)
{
	var->type = ASN_INTEGER;
	var->val_len = sizeof(long);
	var->val.integer = (long *)Malloc(sizeof(long));
	*(var->val.integer) = val;
	return(SNMP_ERR_NOERROR);
}

static int
fddiSMT(struct tree *subtree, int idx, struct variable_list *var)
{
	register oid	subid;

	if (idx >= var->name_length) {
		sm_log(LOG_ERR,0,"fddiSMT: idx=%d len=%d",idx,var->name_length);
    		return(SNMP_ERR_NOSUCHNAME);
	}
	subid = var->name[idx];
	idx++;
	SNMPDEBUG((LOG_DBGSNMP, 0, "fddiSMT: subid=%d", (int)subid));

    	switch(subid) {
		case 10: /* fddiSMTStationidGrp(10, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
		case 11: /* fddiSMTStationid(11, OCTETSTR */
			return(octet_var(var, sizeof(smtp->sid), &smtp->sid));
		case 13: /* fddiSMTOpVersionid(13, INTEGER */
			return(int_var(var, smtp->vers_op));
		case 14: /* fddiSMTHiVersionid(14, INTEGER */
			return(int_var(var, smtp->vers_hi));
		case 15: /* fddiSMTLoVersionid(15, INTEGER */
			return(int_var(var, smtp->vers_lo));
		case 16: /* fddiSMTManufacturerData(16, OCTETSTR */
			return(octet_var(var, sizeof(smtp->manuf),
					&smtp->manuf));
		case 17: /* fddiSMTUserData(17, OCTETSTR */
			return(octet_var(var, sizeof(smtp->userdata),
					&smtp->userdata));

		case 20: /* fddiSMTStationConfigGrp(20, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
		case 21: /* fddiSMTMac-Ct(21, INTEGER */
			return(int_var(var, smtp->mac_ct));
		case 22: /* fddiSMTNonMaster-Ct(22, INTEGER */
			return(int_var(var, smtp->nonmaster_ct));
		case 23: /* fddiSMTMaster-Ct(23, INTEGER */
			return(int_var(var, smtp->master_ct));
		case 24: /* fddiSMTPathsAvailable(24, INTEGER */
			return(int_var(var, smtp->pathavail));
		case 25: /* fddiSMTConfigurationCapabilities(25, INTEGER */
			return(int_var(var, smtp->conf_cap));
		case 26: /* fddiSMTConfigPolicy(26, INTEGER */
			return(int_var(var, smtp->conf_policy));
		case 27: /* fddiSMTConnectionPolicy(27, INTEGER */
			return(int_var(var, smtp->conn_policy));
		case 28: /* fddiSMTReportLimit(28, INTEGER */
			return(int_var(var, smtp->reportlimit));
		case 29: /* fddiSMTInvalidateHoldTime(29, INTEGER */
		case 30: return(int_var(var, smtp->srf_on));

		case 40: /* fddiSMTStatusGrp(40, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
		case 41: /* fddiSMTECMState(41, INTEGER */
			return(int_var(var, smtp->ecmstate));
		case 42: /* fddiSMTCFMState(42, INTEGER */
			return(int_var(var, smtp->cf_state));
		case 43: /* fddiSMTHoldState(43, INTEGER */
			return(int_var(var, smtp->hold_state));
		case 44: /* fddiSMTRemoteDisconnectFlag(44, INTEGER */
			return(int_var(var, smtp->rd_flag));
		
		case 50: /* fddiSMTMIBOperationGrp(50, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
		case 51: /* fddiSMTMsgTimeStamp(51, INTEGER */
			return(octet_var(var, sizeof(smtp->msg_ts),
					&smtp->msg_ts));
		case 52: /* fddiSMTTransitionTimeStamp(52, INTEGER */
			return(octet_var(var, sizeof(smtp->trans_ts),
					&smtp->trans_ts));
		case 53: /* fddiSMTSetCount(53, SEQUENCE */
			if (idx >= var->name_length) {
				sm_log(LOG_ERR, 0,
					"fddiSMT:set_count:idx=%d len=%d",
					idx, var->name_length);
    				return(SNMP_ERR_NOSUCHNAME);
			}
			subid = var->name[idx];
			SNMPDEBUG((LOG_DBGSNMP, 0,
				"fddiSMT:setcount: subid=%d", (int)subid));

			switch (subid) {
				case 1:
					return(int_var(var,
						smtp->setcount.sc_cnt));
				case 2:
					return(octet_var(var,
						sizeof(smtp->setcount.sc_ts),
						&smtp->setcount.sc_ts));
			}
    			return(SNMP_ERR_NOSUCHNAME);
		case 54: /* fddiSMTLastSetStaionid(54, OCTETSTR */
			return(octet_var(var, sizeof(smtp->last_sid),
					&smtp->last_sid));
    	}
    	return(SNMP_ERR_NOSUCHNAME);
}

static int
fddiMAC(struct tree *subtree, int idx, struct variable_list *var)
{
	register oid		subid;
	register oid		rid;
	register SMT_MAC	*mac;

	if ((idx + 1) >= var->name_length) {
		sm_log(LOG_ERR, 0, "fddiMAC: idx=%d len=%d",
					idx, var->name_length);
    		return(SNMP_ERR_NOSUCHNAME);
	}
	subid = var->name[idx];
	rid = var->name[idx+1];
	idx += 2;
	SNMPDEBUG((LOG_DBGSNMP, 0,
		"fddiMAC: subid=%d, rid=%d", (int)subid, (int)rid));

    	if (smtd_cmd == GETNEXT_REQ_MSG) {
		rid++;
		var->name[idx] = rid;
		SNMPDEBUG((LOG_DBGSNMP, 0, "fddiMAC: GETNEXT, rid=%d", (int)rid));
	}

	if ((mac = getmacbyrid(rid)) == 0)
    		return(SNMP_ERR_NOSUCHNAME);
	
    	switch(subid) {
		/* MAC capability Grp.  */
 		case 10: /* fddiMACCapabilitiesGrp(10, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
 		case 11: /* fddiMACFrameStatusCapabilities(11, INTEGER */
			return(int_var(var, mac->fsc));
 		case 12: /* fddiMACBridgeFunction(12, INTEGER */
			return(int_var(var, mac->bridge));
 		case 13: /* fddiMACT-MaxGreatestLowerBound(13, OCTETSTR */
			return(int_var(var, mac->tmax_lobound));
 		case 14: /* fddiMACTVXGreatestLowerBound(14, OCTETSTR */
			return(int_var(var, mac->tvx_lobound));

		/* MAC Config Grp */
 		case 20: /* fddiMACConfigGrp(20, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
 		case 21: /* fddiMACIndex(21, INTEGER */
			return(int_var(var, mac->rid));
 		case 22: /* fddiMACPathsAvailable(22, INTEGER */
			return(int_var(var, mac->pathavail));
 		case 23: /* fddiMACCurrentPath(23, INTEGER */
			return(int_var(var, mac->curpath));
 		case 24: /* fddiMACUpstreamNbr(24, OCTETSTR */
			return(octet_var(var, sizeof(mac->una), &mac->una));
 		case 25: /* fddiMACDownstreamNbr(25, OCTETSTR */
			return(octet_var(var, sizeof(mac->dna), &mac->dna));
 		case 26: /* fddiMACOldUpstreamNbr(26, OCTETSTR */
			return(octet_var(var, sizeof(mac->old_una),
					&mac->old_una));
 		case 27: /* fddiMACOldDownstreamNbr(27, OCTETSTR */
			return(octet_var(var, sizeof(mac->old_dna),
					&mac->old_dna));
 		case 28: /* fddiMACRootConcentratorMAC(28, INTEGER */
			return(int_var(var, mac->rootconcentrator));
 		case 29: /* fddiMACDup-Addr-Test(29, INTEGER */
			return(int_var(var, mac->dupa_test));
 		case 32: /* fddiMACPathsDesired(32, INTEGER */
			return(int_var(var, mac->pathrequested));
 		case 33: return(int_var(var, mac->dnaport));

		/* MAC Address Grp.  */
 		case 40: /* fddiMACAddressGrp(40, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
 		case 41: /* fddiMACSMTAddress(41, OCTETSTR */
			return(octet_var(var,sizeof(mac->addr),&mac->addr));
 		case 42: /* fddiMACLongAliases(42, OCTETSTR */
			return(octet_var(var, sizeof(mac->l_alias),
					mac->l_alias));
 		case 43: /* fddiMACShortAliases(43, OCTETSTR */
			return(octet_var(var, sizeof(mac->s_alias),
					mac->s_alias));
 		case 44: /* fddiMACLongGrpAddrs(44, OCTETSTR */
			return(octet_var(var, sizeof(mac->l_grpaddr),
					mac->l_grpaddr));
 		case 45: /* fddiMACShortGrpAddrs(45, OCTETSTR */
			return(octet_var(var, sizeof(mac->s_grpaddr),
					mac->s_grpaddr));

		/* MAC Operation Grp.  */
 		case 50: /* fddiMACOperationGrp(50, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
 		case 51: /* fddiMACT-Req(51, OCTETSTR */
			return(octet_var(var,sizeof(mac->treq),&mac->treq));
 		case 52: /* fddiMACT-Neg(52, OCTETSTR */
			return(octet_var(var,sizeof(mac->tneg),&mac->tneg));
 		case 53: /* fddiMACT-Max(53, OCTETSTR */
			return(octet_var(var,sizeof(mac->tmax),&mac->tmax));
 		case 54: /* fddiMACTvxValue(54, OCTETSTR */
			return(octet_var(var, sizeof(mac->tvx), &mac->tvx));
 		case 55: /* fddiMACT-Min(55, OCTETSTR */
			return(octet_var(var,sizeof(mac->tmin),&mac->tmin));
 		case 56: /* fddiMACT-Pri0(56, OCTETSTR */
			return(octet_var(var,sizeof(mac->pri0),&mac->pri0));
 		case 57: /* fddiMACT-Pri1(57, OCTETSTR */
			return(octet_var(var,sizeof(mac->pri1),&mac->pri1));
 		case 58: /* fddiMACT-Pri2(58, OCTETSTR */
			return(octet_var(var,sizeof(mac->pri2),&mac->pri2));
 		case 59: /* fddiMACT-Pri3(59, OCTETSTR */
			return(octet_var(var,sizeof(mac->pri3),&mac->pri3));
 		case 60: /* fddiMACT-Pri4(50, OCTETSTR */
			return(octet_var(var,sizeof(mac->pri4),&mac->pri4));
 		case 61: /* fddiMACT-Pri5(61, OCTETSTR */
			return(octet_var(var,sizeof(mac->pri5),&mac->pri5));
 		case 62: /* fddiMACT-Pri6(62, OCTETSTR */
			return(octet_var(var,sizeof(mac->pri6),&mac->pri6));
 		case 63: /* fddiMACCurrentFrameStatus(63, INTEGER */
			return(int_var(var, mac->curfsc));

		/* MAC Counter Grp.  */
 		case 70: /* fddiMACCounterGrp(70, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
 		case 71: /* fddiMACFrame-Ct(71, INTEGER */
			return(int_var(var, mac->frame_ct));
 		case 72: /* fddiMACReceive-Ct(72, INTEGER */
			return(int_var(var, mac->recv_ct));
 		case 73: /* fddiMACTransmit-Ct(73, INTEGER */
			return(int_var(var, mac->xmit_ct));
 		case 74: /* fddiMACToken-Ct(74, INTEGER */
			return(int_var(var, mac->token_ct));

		/* MAC Error cnt Grp.  */
 		case 80: /* fddiMACErrorCtrsGrp(80, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
 		case 81: /* fddiMACError-Ct(81, INTEGER */
			return(int_var(var, mac->err_ct));
 		case 82: /* fddiMACLost-Ct(82, INTEGER */
			return(int_var(var, mac->lost_ct));
 		case 83: /* fddiMACTvxExpired-Ct(83, INTEGER */
			return(int_var(var, mac->tvxexpired_ct));
 		case 84: /* fddiMACNotCopied-Ct(84, INTEGER */
			return(int_var(var, mac->notcopied_ct));
 		case 85: /* fddiMACLate-Ct(85, INTEGER */
			return(int_var(var, mac->late_ct));
 		case 86: /* fddiMACRingOp-Ct(86, INTEGER */
			return(int_var(var, mac->ringop_ct));

		/* MAC Frame Condition Grp.  */
 		case 90: /* fddiMACFrameConditionGrp(90, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
 		case 91: /* fddiMACBaseFrame-Ct(91, INTEGER */
			return(int_var(var, mac->bframe_ct));
 		case 92: /* fddiMACBaseError-Ct(92, INTEGER */
			return(int_var(var, mac->berr_ct));
 		case 93: /* fddiMACBaseLost-Ct(93, INTEGER */
			return(int_var(var, mac->blost_ct));
 		case 94: /* fddiMACBaseTimeMACCondition(94, INTEGER */
			return(octet_var(var, sizeof(mac->basetime),
					&mac->basetime));
 		case 95: /* fddiMACBaseFrameReportThreshold(95, INTEGER */
			return(int_var(var, mac->fr_threshold));

		/* MAC Not Copied Condition Grp.  */
 		case 100: /* fddiMACNotCopiedConditionGrp(100, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
 		case 101: /* fddiMACBaseNotCopied-Ct(101, INTEGER */
			return(int_var(var, mac->bnc_ct));
 		case 102: /* fddiMACBaseTimeNotCopiedCondition(102,INTEGER*/
			return(octet_var(var, sizeof(mac->btncc),
					&mac->btncc));
 		case 103: /* fddiMACFrameNotCopiedThreshold(103, INTEGER */
			return(int_var(var, mac->fnc_threshold));
 		case 104: return(int_var(var, mac->brcv_ct));
 		case 105: return(int_var(var, mac->nc_ratio));

		/* MAC status Grp.  */
 		case 110: /* fddiMACStatusGrp(110, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
 		case 111: /* fddiMACRMTState(111, INTEGER */
			return(int_var(var, mac->rmtstate));
 		case 112: /* fddiMACDa-Flag(112, INTEGER */
			return(int_var(var, mac->da_flag));
 		case 113: /* fddiMACUnaDa-Flag(113, INTEGER */
			return(int_var(var, mac->unada_flag));
 		case 114: /* fddiMACFrameMACCondition(114, INTEGER */
 		case 115: /* fddiMACFrameNotCopied-CtCondition(115,INTEGER*/
			return(int_var(var, mac->frm_nccond));
 		case 116: /* fddiMACLLCServiceAvailable(116, INTEGER */
			return(int_var(var, mac->llcavail));

		/* MAC Root MAC Status Grp */
		case 120: return(SNMP_ERR_NOSUCHNAME);
		case 121: return(int_var(var, mac->msloop_stat));
		case 122: return(int_var(var, mac->root_dnaport));
		case 123: return(int_var(var, mac->root_curpath));
    	}
    	return(SNMP_ERR_NOSUCHNAME);
}

static int
fddiPATH(struct tree *subtree, int idx, struct variable_list *var)
{
	register oid		subid;
	register oid		id;
	register oid		rid = 3;
	register SMT_PATH	*path;
	register SMT_PATHCLASS *pc;

	if ((idx + 1) >= var->name_length) {
		sm_log(LOG_ERR, 0, "fddiPATH: subid missing, idx=%d len=%d",
			idx, var->name_length);
    		return(SNMP_ERR_NOSUCHNAME);
	}

	subid = var->name[idx];
	id = var->name[idx+1];
	idx += 2;
	SNMPDEBUG((LOG_DBGSNMP, 0, "fddiPATH: subid=%d, id=%d", (int)subid, (int)id));

    	switch(subid) {
	case 0: /* path class */
		if (rid > 2)
			break;
		pc = &smtp->pathclass[rid-1];
		switch(id) {
			case 10: return(SNMP_ERR_NOSUCHNAME);
			case 12: return(int_var(var, pc->maxtrace));
			case 13: return(int_var(var, pc->tvx_lobound));
			case 14: return(int_var(var, pc->tmax_lobound));
		}
		break;
	case 2: /* path config grp */
		if (idx >= var->name_length) {
			sm_log(LOG_ERR, 0,
				"fddiPATH: rid missing, idx=%d len=%d",
				idx, var->name_length);
    			return(SNMP_ERR_NOSUCHNAME);
		}
		rid = var->name[idx];
		idx++;
    		if (smtd_cmd == GETNEXT_REQ_MSG) {
			rid++;
			var->name[idx-1] = rid;
		}

		if ((path = getpathbyrid(rid)) == 0)
    			return(SNMP_ERR_NOSUCHNAME);

		switch (id) {
			case 10: /* fddiPATHConfigGrp(10, SEQUENCE */
    				return(SNMP_ERR_NOSUCHNAME);
			case 11: /* fddiPATHType(11, INTEGER */
				return(int_var(var, path->type));
			case 12: /* fddiPATHPHYOrder(12, INTEGER */
				return(int_var(var, path->order));
			case 13: /* fddiPATHRingLatency(13, INTEGER */
				return(int_var(var, path->latency));
			case 14: /* fddiPATHTraceStatus(14, INTEGER */
				return(int_var(var, path->tracestatus));
			case 15: /* fddiPATHSba(15, INTEGER */
				return(int_var(var, path->sba));
			case 16: /* fddiPATHSbaOverhead(16, INTEGER */
				return(int_var(var, path->sbaoverhead));
			case 17: return(int_var(var, path->status));
			case 18: return(int_var(var, path->rtype));
			case 19: return(int_var(var, path->rmode));
		}
		break;
    	}
    	return(SNMP_ERR_NOSUCHNAME);
}

static int
fddiPHY(struct tree *subtree, int idx, struct variable_list *var)
{
	register oid		rid;
	register oid		subid;
	register SMT_PHY	*phy;

	if ((idx + 1) >= var->name_length) {
		sm_log(LOG_ERR, 0,
			"fddiPHY: idx=%d len=%d", idx, var->name_length);
    		return(SNMP_ERR_NOSUCHNAME);
	}
	subid = var->name[idx];
	rid = var->name[idx+1];
	idx += 2;
	SNMPDEBUG((LOG_DBGSNMP, 0, "fddiPHY: subid=%d, rid=%d", (int)subid, (int)rid));

    	if (smtd_cmd == GETNEXT_REQ_MSG) {
		rid++;
		var->name[idx-1] = rid;
	}

	if ((phy = getphybyrid(rid)) == 0)
    		return(SNMP_ERR_NOSUCHNAME);

    	switch(subid) {
		/* PHY Config Grp */
		case 10: /* fddiPHYConfigGrp(10, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
		case 12: /* fddiPHYPC-Type(12, INTEGER */
			return(int_var(var, phy->pctype));
		case 13: /* fddiPHYPC-Neighbor(13, INTEGER */
			return(int_var(var, phy->pcnbr));
		case 14: /* fddiPHYConnectionPolicies(14, INTEGER */
			return(int_var(var, phy->phy_connp));
		case 15: /* fddiPHYRemoteMACIndicated(15, INTEGER */
			return(int_var(var, phy->remotemac));
		case 16: /* fddiPHYCurrentPath(16, INTEGER */
		case 17: /* fddiPHYPathDesired(17, INTEGER */
		case 18: /* fddiPHYMACPlacement(18, INTEGER */
			return(int_var(var, phy->macplacement));
		case 19: /* fddiPHYAvailablePaths(19, INTEGER */
			return(int_var(var, phy->pathavail));
		case 20: /* fddiPHYMACCapability(20, INTEGER */
		case 21: return(int_var(var, phy->loop_time));
		case 22: return(int_var(var, phy->fotx));

		/* PHY Operation Grp.  */
		case 30: /* fddiPHYOperationGrp(30, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
		case 31: /* fddiPHYMaintLineState(31, INTEGER */
			return(int_var(var, phy->maint_ls));
		case 32: /* fddiPHYTB-Max(32, OCTETSTR */
			return(octet_var(var, sizeof(phy->tb_max),
					&phy->tb_max));
		case 33: /* fddiPHYBS-Flag(33, INTEGER */
			return(int_var(var, phy->bs_flag));

		/* PHY Error Cnts Grp.  */
		case 40: /* fddiPHYErrorCtrsGrp(40, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
		case 41: /* fddiPHYElasticityBfrErr-Ct(41, INTEGER */
			return(int_var(var, phy->eberr_ct));
		case 42: /* fddiPHYLCTFail-Ct(42, INTEGER */
			return(int_var(var, phy->lctfail_ct));

		/* PHY Ler Grp.  */
		case 50: /* fddiPHYLerGrp(50, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
		case 51: /* fddiPHYLer-Estimate(51, INTEGER */
			return(int_var(var, phy->ler_estimate));
		case 52: /* fddiPHYLem-Reject-Ct(52, INTEGER */
			return(int_var(var, phy->lemreject_ct));
		case 53: /* fddiPHYLem-Ct(53, INTEGER */
			return(int_var(var, phy->lem_ct));
		case 54: /* fddiPHYBaseLer-Estimate(54, INTEGER */
			return(int_var(var, phy->bler_estimate));
		case 55: /* fddiPHYBaseLer-Reject-Ct(55, INTEGER */
			return(int_var(var, phy->blemreject_ct));
		case 56: /* fddiPHYBaseLer-Ct(56, INTEGER */
			return(int_var(var, phy->blem_ct));
		case 57: /* fddiPHYBaseLer-TimeStamp(57, INTEGER */
			return(octet_var(var, sizeof(phy->bler_ts),
					&phy->bler_ts));
		case 58: /* fddiPHYLer-Cutoff(58, INTEGER */
			return(int_var(var, phy->ler_cutoff));
		case 59: /* fddiPHYLer-Alarm(59, INTEGER */
			return(int_var(var, phy->ler_alarm));

		/* PHY Status Grp.  */
		case 60: /* fddiPHYStatusGrp(60, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
		case 61: /* fddiPHYConnectState(61, INTEGER */
			return(int_var(var, phy->conn_state));
		case 62: /* fddiPHYPCMState(62, INTEGER */
			return(int_var(var, phy->pcm_state));
		case 63: /* fddiPHYPC-Withhold(63, INTEGER */
			return(int_var(var, phy->withhold));
		case 64: return(int_var(var, phy->pler_cond));
    	}
    	return(SNMP_ERR_NOSUCHNAME);
}

static int
fddiATTACHMENT(struct tree *subtree, int idx, struct variable_list *var)
{
	register oid	subid;
	register oid	rid;
	register SMT_PHY *phy;

	if ((idx + 1) >= var->name_length) {
		sm_log(LOG_ERR, 0, "fddiATTACHMENT: idx=%d len=%d",
			idx, var->name_length);
    		return(SNMP_ERR_NOSUCHNAME);
	}
	subid = var->name[idx];
	rid = var->name[idx+1];
	idx += 2;
	SNMPDEBUG((LOG_DBGSNMP, 0, "fddiATTACHMENT: subid=%d, rid = %d",
					(int)subid, (int)rid));

    	if (smtd_cmd == GETNEXT_REQ_MSG) {
		rid++;
		var->name[idx] = rid;
	}

	if ((phy = getphybyrid(rid)) == 0)
    		return(SNMP_ERR_NOSUCHNAME);

    	switch(subid) {
		case 10: /* fddiATTACHMENTConfigGrp(10, SEQUENCE */
    			return(SNMP_ERR_NOSUCHNAME);
		case 11: /* fddiATTACHMENTClass(11, INTEGER */
			return(int_var(var, phy->type));
		case 12: /* fddiATTACHMENTOpticalBypassPresent(12, INTEGER */
			return(int_var(var, phy->obs));
		case 13: /* fddiATTACHMENTI-MaxExpiration(13, OCTETSTR */
			return(octet_var(var, sizeof(phy->imax),
					&phy->imax));
		case 14: /* fddiATTACHMENTInsertedStatus(14, INTEGER */
			return(int_var(var, phy->insert));
		case 15: /* fddiATTACHMENTInsertPolicy(15, INTEGER */
			return(int_var(var, phy->ipolicy));
    	}
    	return(SNMP_ERR_NOSUCHNAME);
}

static int
fddiSGIstation(int idx, struct variable_list *var)
{
	oid	subid;
	oid	cx;
	SMT_MAC	*mac;
	int sts;

	subid = var->name[idx]; /* action */
	cx = var->name[idx+1];/* context */
	if (cx >= nsmtd)
    		return(SNMP_ERR_NOSUCHNAME);
	smtd_setcx((int)cx);

	SNMPDEBUG((LOG_DBGSNMP, 0, "fddiSGIstation: cmd=%d", (int)subid));

    	switch(subid) {
		case FDDI_SNMP_INFO: { /* interface info */
			IPHASE_INFO iph;
			int i;

			for (i = 0, mac=smtp->mac; mac; i++, mac=mac->next) {
				SNMPDEBUG((LOG_DBGSNMP,0,"%s added",mac->name));
				strcpy(iph.names[i], mac->name);
			}
			iph.mac_ct = i;
			return(octet_var(var, sizeof(iph), &iph));
		}

		case FDDI_SNMP_UP: /* auto iface up */
		case FDDI_SNMP_DOWN: /* iface down */
		case FDDI_SNMP_THRUA: /* force thru A */
		case FDDI_SNMP_THRUB: /* force thru B */
		case FDDI_SNMP_WRAPA: /* force wrap A */
		case FDDI_SNMP_WRAPB: /* force wrap B */
			break;
		case FDDI_SNMP_STAT: {
			static SMT_FSSTAT fsqstat;
			fsqstat.num = -1;
			fsq_stat(&fsqstat);
			return(octet_var(var,sizeof(fsqstat),(char*)&fsqstat));
		}

		case FDDI_SNMP_SINGLE: /* force single attach */
		case FDDI_SNMP_DUAL: /* force dual attach */
			break;
		case FDDI_SNMP_DUMP:
			sts = octet_var(var, sizeof(*smtp), (char*)smtp);
			SNMPDEBUG((LOG_DBGSNMP, 0,
				"dump smtd %d bytes", sizeof(*smtp)));
			return(sts);
		case FDDI_SNMP_REGFS: { /* register fs */
			SMT_FS_INFO *r;

			r = (SMT_FS_INFO *)(var->val.string);
			/* enq_fs will consumes "r" if op is successful */
			if ((sts = enq_fs(r)) == SNMP_ERR_NOERROR) {
				sts = octet_var(var, sizeof(*r), (char *)r);
			} else if (r)
				free(r);
			return(sts);
		}
		case FDDI_SNMP_UNREGFS: { /* unregister fs */
			SMT_FS_INFO *r = (SMT_FS_INFO *)(var->val.string);

			if ((sts = unq_fs(r)) == SNMP_ERR_NOERROR)
				sts = octet_var(var, sizeof(*r), (char *)r);
			if (r) free(r);
			return(sts);
		}

		case FDDI_SNMP_TRACE:
		case FDDI_SNMP_MAINT:
			break;

		case FDDI_SNMP_SENDFRAME: {
			SMT_FS_INFO *r;
			SMT_MAC *mac;

			SNMPDEBUG((LOG_DBGSNMP, 0, "SendFrame invoked"));
			if ((r = (SMT_FS_INFO *)(var->val.string)) == 0)
				return(SNMP_ERR_NOSUCHNAME);

			sts = fs_request(smtp->mac, r->fc, &r->fsi_da,
					r+1, (var->val_len - sizeof(*r)));
			if (sts == RC_SUCCESS)
				sts = octet_var(var, sizeof(*r), (char *)r);
			else
				sts = SNMP_ERR_NOSUCHNAME;
			if (r) free(r);
			return(sts);
		}

		case FDDI_SNMP_FSSTAT: { /* Frame svc status */
			static SMT_FSSTAT fsqstat;
			fsq_stat(&fsqstat);
			return(octet_var(var,sizeof(fsqstat),(char*)&fsqstat));
		}

		case FDDI_SNMP_FSDELETE: { /* force Frame svc delete */
			SMT_FSSTAT *stat;

			SNMPDEBUG((LOG_DBGSNMP, 0, "DelFSQ invoked"));
			if ((stat = (SMT_FSSTAT *)(var->val.string)) == 0)
				return(SNMP_ERR_NOSUCHNAME);
			fsq_del(stat);
			sts = 0;
			sts = octet_var(var, sizeof(sts), (char *)&sts);
			if (stat) free(stat);
			return(sts);
		}

		case FDDI_SNMP_VERSION:
			return(int_var(var, SMTD_VERS));

		default:
			break;
    	}
    	return(SNMP_ERR_NOSUCHNAME);
}

static int
fddiSGIiphase(int idx, struct variable_list *var, char *ifname)
{
	oid	subid;
	oid	unit;
	SMT_MAC	*mac;
	int	sts;
	static	char lz = 0;

	subid = var->name[idx]; /* action */
	unit = var->name[idx+1];/* unit */
	SNMPDEBUG((LOG_DBGSNMP,0,"fddiSGI%s%d: cmd=%d",ifname,unit,(int)subid));

	/* FDDI_SNMP_SENDFRAME is only exception!!! */
	if ((smtd_cmd == SET_REQ_MSG) &&
	    (subid != FDDI_SNMP_SENDFRAME) &&
	    (smtd_cur_inpdu->address.sin_family != AF_UNIX) &&
	    (smtd_cur_inpdu->address.sin_addr.s_addr
	     != supersin.sin_addr.s_addr)) {
		sm_log(LOG_ERR, 0, "Illegal SET attempt by %s:%d",
		       inet_ntoa(smtd_cur_inpdu->address.sin_addr),
		       (int)smtd_cur_inpdu->address.sin_port);
		return(SNMP_ERR_NOSUCHNAME);
	}

	switch(subid) {
		case FDDI_SNMP_INFO: { /* interface info */
			PORT_INFO p;
			char name[10];

			sprintf(name, "%s%d", ifname, unit);
			p.phy_ct = 0;
			if ((mac = getmacbyname(ifname, unit)) == 0)
    				return(SNMP_ERR_NOSUCHNAME);
			if ((mac->primary != 0) &&
			    (sm_getconf(name,0,&p.conf_0)==SNMP_ERR_NOERROR))
				p.phy_ct++;
			if ((mac->secondary) && 
			    (sm_getconf(name,1,&p.conf_1)==SNMP_ERR_NOERROR))
				p.phy_ct++;
			return(octet_var(var, sizeof(p), (char *)&p));
		}

		case FDDI_SNMP_UP: /* auto iface up */
		case FDDI_SNMP_SINGLE: /* force single attach */
		case FDDI_SNMP_DUAL: /* force dual attach */
			sts = upiphase(ifname, unit);
			if (sts != SNMP_ERR_NOERROR)
    				return(SNMP_ERR_NOSUCHNAME);
			if (var->val.string) free(var->val.string);
			return(octet_var(var, sizeof(lz), &lz));

		case FDDI_SNMP_DOWN: /* iface down */
			sts = downiphase(ifname, (int)unit);
			if (sts != SNMP_ERR_NOERROR)
    				return(SNMP_ERR_NOSUCHNAME);
			if (var->val.string) free(var->val.string);
			return(octet_var(var, sizeof(lz), &lz));

		case FDDI_SNMP_THRUA: /* force thru A */
		case FDDI_SNMP_THRUB: /* force thru B */
		case FDDI_SNMP_WRAPA: /* force wrap A */
		case FDDI_SNMP_WRAPB: /* force wrap B */
			break;

		case FDDI_SNMP_STAT: {
			struct smt_st *st;
			oid phyidx;

			phyidx = var->name[idx+2];/* phyidx */
			if ((mac = getmacbyname(ifname, unit)) == 0)
    				return(SNMP_ERR_NOSUCHNAME);
			st = (phyidx == 0) ? mac->st : mac->st1;
			return(octet_var(var, sizeof(*st), (char*)st));
		}

		case FDDI_SNMP_DUMP: { /* interface dump - smtd thinks */
			MAC_INFO m;

			if ((mac = getmacbyname(ifname, unit)) == 0)
    				return(SNMP_ERR_NOSUCHNAME);
			m.mac = *mac;
			if (mac->primary)
				m.phy_0 = *(mac->primary);
			if (mac->secondary)
				m.phy_1 = *(mac->secondary);

			SNMPDEBUG((LOG_DBGSNMP,0,
					"mac dump %d bytes", sizeof(m)));
			return(octet_var(var, sizeof(m), (u_char *)&m));
		}

		case FDDI_SNMP_REGFS: /* register fs */
		case FDDI_SNMP_UNREGFS: /* unregister fs */
			break;

		case FDDI_SNMP_TRACE: {
			char name[10];
			oid phyidx;

			phyidx = var->name[idx+2];/* phyidx */
			sprintf(name, "%s%d", ifname, (int)unit);
			SNMPDEBUG((LOG_DBGSNMP,0,
					"%s:%d: trace",name,(int)phyidx));
			if (sm_trace(name, (int)phyidx) != SNMP_ERR_NOERROR)
				return(SNMP_ERR_NOSUCHNAME);
			if (var->val.string) free(var->val.string);
			return(octet_var(var, sizeof(lz), &lz));
		}

		case FDDI_SNMP_MAINT: {
			char name[10];
			oid phyidx;
			enum pcm_ls *ls;

			phyidx = var->name[idx+2];/* phyidx */
			sprintf(name, "%s%d", ifname, (int)unit);
			ls = (enum pcm_ls *)var->val.string;
			if (!ls) {
				sm_log(LOG_ERR, 0, "maint: no value");
				return(SNMP_ERR_NOSUCHNAME);
			}
			SNMPDEBUG((LOG_DBGSNMP, 0,
				   "%s: maint_ls=%d", name, *ls));
			if ((mac = getmacbyname(ifname, unit)) != 0)
				mac->changed = 1;
			if (sm_maint(name, (int)phyidx, *ls)
				!= SNMP_ERR_NOERROR)
				return(SNMP_ERR_NOSUCHNAME);
			if (var->val.string) free(var->val.string);
			return(octet_var(var, sizeof(lz), &lz));
		}

		case FDDI_SNMP_SENDFRAME: {
			SMT_FS_INFO *r;
			SMT_MAC *mac;

			if ((mac = getmacbyname(ifname, unit)) == 0)
    				return(SNMP_ERR_NOSUCHNAME);
			if ((r = (SMT_FS_INFO *)(var->val.string)) == 0)
				return(SNMP_ERR_NOSUCHNAME);

			sts = fs_request(mac, r->fc, &r->fsi_da,
					r+1, (var->val_len - sizeof(*r)));
			if (sts == RC_SUCCESS)
				sts = octet_var(var, sizeof(*r), (char *)r);
			else
				sts = SNMP_ERR_NOSUCHNAME;
			if (r) free(r);
			return(sts);
		}

		case FDDI_SNMP_FSSTAT: /* Frame svc status */
		case FDDI_SNMP_FSDELETE: /* force Frame svc delete */
		case FDDI_SNMP_VERSION:
		default:
			break;
    	}

    	return(SNMP_ERR_NOSUCHNAME);
}

static int
sgi_var(struct tree *subtree, int idx, struct variable_list *var)
{
	oid subid;

	SNMPDEBUG((LOG_DBGSNMP, 0, "sgi_var: %d %d %d", (int)var->name[idx],
			(int)var->name[idx+1], (int)var->name[idx+2]));

	if (idx > var->name_length) {
		sm_log(LOG_ERR, 0, "SGI_VAR: idx=%d, len=%d",
			idx, var->name_length);
		return(SNMP_ERR_NOSUCHNAME);
	}

	subid = var->name[idx];
	idx++;

    	switch(subid) {
		case 1: /* station */
			return(fddiSGIstation(idx, var));
#ifdef undef
		case 2: /* xxx */
			return(fddiSGIiphase(idx, var, "xxx"));
		case 3: /* xxx */
			return(fddiSGIiphase(idx, var, "xxx"));
#endif
		case 4: /* ipg */
			return(fddiSGIiphase(idx, var, "ipg"));
		case 5: /* xpi */
			return(fddiSGIiphase(idx, var, "xpi"));
		case 6: /* rns */
			return(fddiSGIiphase(idx, var, "rns"));
    	}
    	return(SNMP_ERR_NOSUCHNAME);
}

/*
 * Find the value of a variable
 */
int
find_value(struct tree *subtree, struct variable_list *var)
{
    	register oid subid;
	register int i;

	if (smtd_cmd == SET_REQ_MSG) {
		if ((subtree->access&ACCESS_WRITEONLY) == 0) {
			sm_log(LOG_INFO, 0, "%s is read only", subtree->label);
	    		return(SNMP_ERR_READONLY);
		}
	} else {
		if ((subtree->access&ACCESS_READONLY) == 0) {
			sm_log(LOG_INFO, 0,"%s is not readable",subtree->label);
	    		return(SNMP_ERR_READONLY);
		}
	}

	if ((i = underfdditree(var->name)) <= 0) {
		if ((i = undersgifdditree(var->name)) <= 0)
	    		return(SNMP_ERR_NOSUCHNAME);
		return(sgi_var(subtree, i, var));
    	}
	
	if (var->name_length <= i) {
		sm_log(LOG_ERR, 0, "find_value: name too short");
	    	return(SNMP_ERR_NOSUCHNAME);
	}

	if ((smtd_cmd == SET_REQ_MSG) &&
	    (smtd_cur_inpdu->address.sin_family != AF_UNIX) &&
	    (smtd_cur_inpdu->address.sin_addr.s_addr
	     != supersin.sin_addr.s_addr)) {
		sm_log(LOG_ERR, 0, "Illegal SET attempt by %s:%d",
		       inet_ntoa(smtd_cur_inpdu->address.sin_addr),
		       (int)smtd_cur_inpdu->address.sin_port);
		return(SNMP_ERR_NOSUCHNAME);
	}

	subid = var->name[i];
	i++;
    	switch(subid) {
		case 1: /* fddiSMT */
			return(fddiSMT(subtree, i, var));
		case 2: /* fddiMAC */
			return(fddiMAC(subtree, i, var));
		case 3: /* fddiPATH */
			return(fddiPATH(subtree, i, var));
		case 4: /* fddiPHY */
			return(fddiPHY(subtree, i, var));
		case 6: /* fddiATTACHMENT */
			return(fddiATTACHMENT(subtree, i, var));
    	}
    	return(SNMP_ERR_NOSUCHNAME);
}
