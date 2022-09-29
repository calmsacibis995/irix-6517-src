/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.19 $
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <sys/time.h>
#include <sys/signal.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <ctype.h>
#include <netdb.h>

#include <sys/types.h>
#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smt_mib.h>
#include <smt_snmp.h>
#include <smt_snmp_impl.h>
#include <smt_snmp_api.h>
#include <smtd_parm.h>
#include <smtd.h>
#include <smt_subr.h>
#include <smt_snmp_clnt.h>
#include <smtd_fs.h>
#include <sm_map.h>

static struct snmp_session *map_ss = 0;

/* Must be at most (14-1) chars long -- used as Unix socket name */
static char map_tmplt[] =  "/tmp/.MXXXXXX";
static char *tfname = 0;

static char *peername = "localhost";
void
map_setpeer(char *peer)
{
	static char peerary[MAXHOSTNAMELEN];

	if (!peer || *peer == '\0') {
		peername = "localhost";
		return;
	}
	strcpy(peerary, peer);
	peername = peerary;
}

char *
map_getpeer()
{
	return(peername);
}

static int maprtry = 0;
static int maptimo = SNMP_DEFAULT_TIMEOUT;
void
map_settimo(int timo, int rtry)
{
	maprtry = rtry;
	maptimo = timo*1000000L;
}

void
map_resettimo(void)
{
	maprtry = 0;
	maptimo = SNMP_DEFAULT_TIMEOUT;
}

int
map_open(char *community, char *(*auth)())
{
	struct snmp_session session;
	int s = -1;

	if (map_ss) {
		sm_log(LOG_INFO, 0, "map_open: redundant open request ignored");
		return(-1);
	}

	if ((!tfname) && !strcmp(peername, "localhost")) {
		if ((tfname = mktemp(map_tmplt)) == NULL) {
			sm_log(LOG_ERR, SM_ISSYSERR, "map_open: mktemp failed");
			return(-1);
		}
	}

	bzero((char *)&session, sizeof(session));
	session.peername = peername;
	session.community = community;
	session.community_len = strlen(community);
	session.retries = maprtry;
	session.timeout = maptimo;
	session.authenticator = auth;
	snmp_synch_setup(&session);
	if ((map_ss = snmp_open(&session, &s, tfname)) == 0)
		return(-1);
	return(0);
}

void
map_close(void)
{
	if (!map_ss)
		return;
	snmp_close(map_ss);
	if (tfname) {
		unlink(tfname);
		tfname = 0;
	}
	map_setpeer(0);
	map_ss = 0;
}

static char *
action2name(action)
{
#ifdef __STDC__
#define ACASE(x)	case FDDI_SNMP_ ## x: return #x
#else
#define ACASE(x)	case FDDI_SNMP_/**/x: return "x"
#endif
	switch (action) {
		ACASE(INFO);
		ACASE(UP);
		ACASE(DOWN);
		ACASE(THRUA);
		ACASE(THRUB);
		ACASE(WRAPA);
		ACASE(WRAPB);
		ACASE(STAT);
		ACASE(SINGLE);
		ACASE(DUAL);
		ACASE(DUMP);
		ACASE(REGFS);
		ACASE(UNREGFS);
		ACASE(TRACE);
		ACASE(MAINT);
		ACASE(SENDFRAME);
		ACASE(FSSTAT);
		ACASE(FSDELETE);
		ACASE(VERSION);

		default:
			return "???";
	}
}
#undef ACASE

/*
 * Get 1 variable each time.
 */
int
map_smt(char *ifname, int action, void *buf, int buflen, int phyidx)
{
	oid subid;
	int unit;
	struct snmp_pdu *pdu, *response = 0;
	struct variable_list *vars;
	oid name[MAX_NAME_LEN];
	int name_length;
	int status;
	int pos;
	int rtry;

	if (!ifname) {
		subid = 1;	/* SMT */
		unit = phyidx;
	} else if (strncmp(ifname, "rns", 3) == 0) {
		subid = 6;
		unit = (int)(ifname[3] - '0');
	} else if (strncmp(ifname, "xpi", 3) == 0) {
		subid = 5;
		unit = (int)(ifname[3] - '0');
	} else if (strncmp(ifname, "ipg", 3) == 0) {
		subid = 4;
		unit = (int)(ifname[3] - '0');
#ifdef undef
	} else if (strncmp(ifname, "xxx", 3) == 0) {
		subid = 3;
		unit = (int)(ifname[3] - '0');
	} else if (strncmp(ifname, "xxx", 2) == 0) {
		subid = 2;
		unit = (int)(ifname[2] - '0');
#endif
	} else {
		sm_log(LOG_ERR,0,"map_smt: FDDI interface %s not found",ifname);
		return(STAT_ERROR);
	}

	if (!map_ss) {
		status = map_open("public", NULL);
		if (status)
			return(STAT_ERROR);
	}

	pos = get_sgifddimib((char *)name);
	name[pos  ] = (oid)subid;
	name[pos+1] = (oid)action;
	name[pos+2] = (oid)(unit);
	pos += 3;
	if ((action == FDDI_SNMP_STAT) ||
	    (action == FDDI_SNMP_MAINT)) {
		name[pos] = (oid)(phyidx);
		pos++;
	}
	name_length = pos;
	name[pos] = 0;
	rtry = map_ss->retries;

retry:
	switch (action) {
		case FDDI_SNMP_INFO:
		case FDDI_SNMP_STAT:
		case FDDI_SNMP_DUMP:
		case FDDI_SNMP_FSSTAT:
		case FDDI_SNMP_VERSION:
			pdu = snmp_pdu_create(GET_REQ_MSG);
			snmp_add_null_var(pdu, name, name_length);
			break;
		case FDDI_SNMP_UP:
		case FDDI_SNMP_DOWN:
		case FDDI_SNMP_THRUA:
		case FDDI_SNMP_THRUB:
		case FDDI_SNMP_WRAPA:
		case FDDI_SNMP_WRAPB:
		case FDDI_SNMP_SINGLE:
		case FDDI_SNMP_DUAL:
		case FDDI_SNMP_TRACE:
			pdu = snmp_pdu_create(SET_REQ_MSG);
			vars = (struct variable_list *)Malloc(sizeof(*vars));
			pdu->variables = vars;
			vars->next_variable = NULL;
			vars->name = (oid *)Malloc(name_length * sizeof(oid));
			bcopy((char *)name, (char *)vars->name,
						name_length * sizeof(oid));
			vars->name_length = name_length;
			vars->type = ASN_OCTET_STR;
			vars->val.string = Malloc(2);
			bcopy("on", vars->val.string, 2);
			vars->val_len = 2;
			break;
		case FDDI_SNMP_REGFS:
		case FDDI_SNMP_UNREGFS:
		case FDDI_SNMP_MAINT:
		case FDDI_SNMP_SENDFRAME:
		case FDDI_SNMP_FSDELETE:
			pdu = snmp_pdu_create(SET_REQ_MSG);
			vars = (struct variable_list *)Malloc(sizeof(*vars));
			pdu->variables = vars;
			vars->next_variable = NULL;
			vars->name = (oid *)Malloc(name_length * sizeof(oid));
			bcopy((char *)name, (char *)vars->name,
						name_length * sizeof(oid));
			vars->name_length = name_length;
			vars->type = ASN_OCTET_STR;
			vars->val.string = Malloc(buflen);
			bcopy(buf, vars->val.string, buflen);
			vars->val_len = buflen;
			break;
		default:
			sm_log(LOG_EMERG,0,"map_smt: BAD command %d", action);
			exit(99);
	}

	status = snmp_synch_response(map_ss, pdu, &response);
	if (!response || status != STAT_SUCCESS) {
		/* XXX routine should return useful error status codes */
		sm_log(LOG_ERR, 0,
		       ((status == STAT_TIMEOUT)
			? "No response from daemon: timed-out"
			: "Can't get response from daemon"));
		goto snmp_stat_ret;
	}

	if (response->errstat != SNMP_ERR_NOERROR) {
		sm_log(LOG_ERR,0,"Error: %s.%d %s: %s",
			ifname ? ifname: "smt", phyidx, action2name(action),
			snmp_errstring(response->errstat));
		if (response->errstat == SNMP_ERR_NOSUCHNAME &&
		    (vars = response->variables) != 0) {
			char loc_buf[256];

			sprint_objid(loc_buf, vars->name, vars->name_length);
			sm_log(LOG_ERR, 0, "map_smt: %s doesn't exist",
			       loc_buf);
		}
		snmp_free_pdu(response);
		response = 0;
		rtry--;
		if (rtry > 0)
			goto retry;
		status = STAT_ERROR;
		goto snmp_stat_ret;
	}

	vars = response->variables;
	switch (action) {
	case FDDI_SNMP_INFO:
	case FDDI_SNMP_STAT:
	case FDDI_SNMP_DUMP:
	case FDDI_SNMP_FSSTAT:
	case FDDI_SNMP_VERSION:
		if (buf) {
			if (buflen > response->variables->val_len)
				buflen = response->variables->val_len;
			bcopy(response->variables->val.string, buf, buflen);
		}
		break;
	case FDDI_SNMP_UP:
	case FDDI_SNMP_DOWN:
	case FDDI_SNMP_THRUA:
	case FDDI_SNMP_THRUB:
	case FDDI_SNMP_WRAPA:
	case FDDI_SNMP_WRAPB:
	case FDDI_SNMP_SINGLE:
	case FDDI_SNMP_DUAL:
	case FDDI_SNMP_REGFS:
	case FDDI_SNMP_UNREGFS:
	case FDDI_SNMP_MAINT:
	case FDDI_SNMP_TRACE:
	case FDDI_SNMP_SENDFRAME:
	case FDDI_SNMP_FSDELETE:
		break;
	default:
		sm_log(LOG_EMERG, 0, "map_smt: Invalid action: %d", action);
		exit(99);
	}

snmp_stat_ret:
	if (response)
		snmp_free_pdu(response);
	return(status);
}

void
map_exit(int es)
{
	map_close();
	exit(es);
}
