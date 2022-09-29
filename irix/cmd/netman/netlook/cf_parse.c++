/*
 * Copyright 1991-1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Parser and Lexical Analyzer for Network Configuration Files
 *
 *	$Revision: 1.5 $
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

#include "cf.h"

extern "C" {
#include <netinet/in.h>
#include <stdio.h>
#include <strings.h>
#include <ctype.h>
}

static HostNode *
lookupHost(CfNode *root, char *name)
{
	for (NetworkNode *n = (NetworkNode *) root->child;
			n != 0; n = (NetworkNode *) n->next) {
		for (SegmentNode *s = (SegmentNode *) n->child;
				s != 0; s = (SegmentNode *) s->next) {
			for (InterfaceNode *i = (InterfaceNode *) s->child;
					i != 0; i = (InterfaceNode *) i->next) {
				HostNode *h = i->getHost();
				if (h->name != 0 && strcmp(h->name, name) == 0)
					return h;
			}
		}
	}
	return 0;
}

int
cf_parse(FILE *fp, CfNode *root, int *line, char *e)
{
	int shift;
	enum cftoken token;
	char *value, *emsg;
	NetworkNode *network;
	SegmentNode *segment;
	HostNode *host;
	InterfaceNode *interface;
	enum lexstate {
		STATE_TOP,
		STATE_NETWORK,
		STATE_NETWORKID,
		STATE_NETWORKL,
		STATE_NETWORKIP,
		STATE_NETWORKMASK,
		STATE_SEGMENT,
		STATE_SEGMENTID,
		STATE_SEGMENTL,
		STATE_SEGMENTTYPE,
		STATE_SEGMENTIP,
		STATE_SEGMENTDN,
		STATE_HOST,
		STATE_HOSTID,
		STATE_HOSTL,
		STATE_INTERFACE,
		STATE_INTERFACEID,
		STATE_INTERFACEL,
		STATE_INTERFACEPA,
		STATE_INTERFACEIP,
		STATE_INTERFACEDN,
		STATE_ERROR,
	} state;

	emsg = 0;
	token = TOKEN_NULL;
	state = STATE_TOP;
	shift = 1;

	for ( ; ; ) {
		if (shift)
			cf_lex(fp, line, &token, &value);

		switch (state) {
		    case STATE_TOP:
			switch (token) {
			    case TOKEN_EOF:
				return 1;
			    case TOKEN_NETWORK:
				network = new NetworkNode(root);
				state = STATE_NETWORK;
				shift = 1;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_NETWORK:
			switch (token) {
			    case TOKEN_ID:
				network->setName(value);
				state = STATE_NETWORKID;
				shift = 1;
				break;
			    case TOKEN_LBRACE:
				state = STATE_NETWORKID;
				shift = 0;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_NETWORKID:
			switch (token) {
			    case TOKEN_LBRACE:
				state = STATE_NETWORKL;
				shift = 1;
				break;
			    case TOKEN_EOF:
				state = STATE_TOP;
				shift = 0;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_NETWORKL:
			switch (token) {
			    case TOKEN_SEGMENT:
				segment = new SegmentNode(network);
				state = STATE_SEGMENT;
				shift = 1;
				break;
			    case TOKEN_IPNET:
				if (network->ipnum.isValid()) {
					emsg =
					  "Two Internet numbers for a network";
					value = 0;
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_NETWORKIP;
					shift = 1;
				}
				break;
			    case TOKEN_IPMASK:
				if (network->ipmask.isValid()) {
					emsg = "Two IP netmasks for a network";
					value = 0;
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_NETWORKMASK;
					shift = 1;
				}
				break;
			    case TOKEN_RBRACE:
				network->complete();
				state = STATE_TOP;
				shift = 1;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_NETWORKIP:
			switch (token) {
			    case TOKEN_ID:
				network->ipnum = value;
				if (!network->ipnum.isValid()) {
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_NETWORKL;
					shift = 1;
				}
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_NETWORKMASK:
			switch (token) {
			    case TOKEN_ID:
				network->ipmask = value;
				if (!network->ipmask.isValid()) {
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_NETWORKL;
					shift = 1;
				}
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_SEGMENT:
			switch (token) {
			    case TOKEN_ID:
				segment->setName(value);
				state = STATE_SEGMENTID;
				shift = 1;
				break;
			    case TOKEN_LBRACE:
				state = STATE_SEGMENTID;
				shift = 0;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_SEGMENTID:
			switch (token) {
			    case TOKEN_LBRACE:
				state = STATE_SEGMENTL;
				shift = 1;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_SEGMENTL:
			switch (token) {
			    case TOKEN_TYPE:
				if (segment->getType() != SEG_NULL) {
					emsg = "Two types for a segment";
					value = 0;
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_SEGMENTTYPE;
					shift = 1;
				}
				break;
			    case TOKEN_IPNET:
				if (segment->ipnum.isValid()) {
					emsg =
					  "Two Internet numbers for a segment";
					value = 0;
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_SEGMENTIP;
					shift = 1;
				}
				break;
			    case TOKEN_DNAREA:
				if (segment->dnarea.isValid()) {
					emsg = "Two DECnet areas for a segment";
					value = 0;
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_SEGMENTDN;
					shift = 1;
				}
				break;
			    case TOKEN_HOST:
				state = STATE_HOST;
				shift = 1;
				break;
			    case TOKEN_RBRACE:
				segment->complete();
				state = STATE_NETWORKL;
				shift = 1;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_SEGMENTTYPE:
			switch (token) {
			    case TOKEN_ID:
				if (segment->setType(value) == SEG_NULL) {
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_SEGMENTL;
					shift = 1;
				}
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_SEGMENTIP:
			switch (token) {
			    case TOKEN_ID:
				segment->ipnum = value;
				if (!segment->ipnum.isValid()) {
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_SEGMENTL;
					shift = 1;
				}
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_SEGMENTDN:
			switch (token) {
			    case TOKEN_ID:
				segment->dnarea = value;
				if (!segment->dnarea.isValid()) {
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_SEGMENTL;
					shift = 1;
				}
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_HOST:
			switch (token) {
			    case TOKEN_ID:
				// XXX - look up the host in a table
				host = lookupHost(root, value);
				if (host == 0) {
					host = new HostNode();
					host->setName(value);
				}
				state = STATE_HOSTID;
				shift = 1;
				break;
			    case TOKEN_LBRACE:
				host = new HostNode();
				state = STATE_HOSTID;
				shift = 0;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_HOSTID:
			switch (token) {
			    case TOKEN_LBRACE:
				state = STATE_HOSTL;
				shift = 1;
				break;
			    case TOKEN_RBRACE:
			    case TOKEN_HOST:
				{
					char *n = host->getName();
					interface = new InterfaceNode(segment);
					interface->setHost(host);
					interface->setName(n);
					interface->complete();
					state = STATE_SEGMENTL;
					shift = 0;
				}
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_HOSTL:
			switch (token) {
			    case TOKEN_NISSERVER:
				host->setNISserver(1);
				shift = 1;
				break;
			    case TOKEN_NISMASTER:
				host->setNISmaster(1);
				shift = 1;
				break;
			    case TOKEN_INTERFACE:
				interface = new InterfaceNode(segment);
				interface->setHost(host);
				state = STATE_INTERFACE;
				shift = 1;
				break;
			    case TOKEN_RBRACE:
				if (interface == 0) {
					char *n = host->getName();
					if (n == 0) {
						emsg = "Empty host";
						value = 0;
						state = STATE_ERROR;
						shift = 0;
						break;
					}
					interface = new InterfaceNode(segment);
					interface->setHost(host);
					interface->setName(n);
					interface->complete();
				}
				state = STATE_SEGMENTL;
				shift = 1;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_INTERFACE:
			switch (token) {
			    case TOKEN_ID:
				interface->setName(value);
				state = STATE_INTERFACEID;
				shift = 1;
				break;
			    case TOKEN_LBRACE:
				state = STATE_INTERFACEID;
				shift = 0;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_INTERFACEID:
			switch (token) {
			    case TOKEN_LBRACE:
				state = STATE_INTERFACEL;
				shift = 1;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_INTERFACEL:
			switch (token) {
			    case TOKEN_PHYSADDR:
				if (interface->physaddr.isValid()) {
					emsg =
				      "two physical addresses for an interface";
					value = 0;
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_INTERFACEPA;
					shift = 1;
				}
				break;
			    case TOKEN_IPADDR:
				if (interface->ipaddr.isValid()) {
					emsg =
					    "two IP addresses for an interface";
					value = 0;
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_INTERFACEIP;
					shift = 1;
				}
				break;
			    case TOKEN_DNADDR:
				if (interface->dnaddr.isValid()) {
					emsg =
					"two DECnet addresses for an interface";
					value = 0;
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_INTERFACEDN;
					shift = 1;
				}
				break;
			    case TOKEN_RBRACE:
				interface->complete();
				state = STATE_HOSTL;
				shift = 1;
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_INTERFACEPA:
			switch (token) {
			    case TOKEN_ID:
				interface->physaddr = value;
				if (!interface->physaddr.isValid()) {
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_INTERFACEL;
					shift = 1;
				}
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_INTERFACEIP:
			switch (token) {
			    case TOKEN_ID:
				interface->ipaddr = value;
				if (!interface->ipaddr.isValid()) {
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_INTERFACEL;
					shift = 1;
				}
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_INTERFACEDN:
			switch (token) {
			    case TOKEN_ID:
				interface->dnaddr = value;
				if (!interface->dnaddr.isValid()) {
					state = STATE_ERROR;
					shift = 0;
				} else {
					state = STATE_INTERFACEL;
					shift = 1;
				}
				break;
			    default:
				state = STATE_ERROR;
				shift = 0;
			}
			break;
		    case STATE_ERROR:
		    default:
			if (emsg == 0) {
				if (token == TOKEN_EOF) {
					emsg = "unexpected end of file";
					(*line)--;
				} else
					emsg = "syntax error";
			}
			if (value != 0 && isprint(*value))
				sprintf(e, "line %d: %s near \"%s\"",
					   *line, emsg, value);
			else
				sprintf(e, "line %d: %s", *line, emsg);
			return 0;
		}
	}
}
