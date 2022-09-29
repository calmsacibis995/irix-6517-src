/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "pmcd.h"

/*
 * Diagnostic event tracing support
 */

typedef struct {
    time_t	t_stamp;	/* timestamp */
    int		t_type;		/* event type */
    int		t_who;		/* originator or principal identifier */
    int		t_p1;		/* optional event parameters */
    int		t_p2;
} tracebuf;

static tracebuf		*trace;
static unsigned int	next;

/*
 * by default, circular buffer last 20 events -- change by modify
 * pmcd.control.tracebufs
 * by default, tracing is disabled -- change by setting the following
 * to 1, pmcd.control.traceconn (trace connections) and/or
 * pmcd.control.tracepdu (trace PDUs)
 */
int		_pmcd_trace_nbufs = 20;
int		_pmcd_trace_mask;

void
pmcd_init_trace(int n)
{
    if (trace != NULL)
	free(trace);
    if ((trace = (tracebuf *)malloc(n * sizeof(tracebuf))) == NULL) {
	 __pmNoMem("pmcd_init_trace", n * sizeof(tracebuf), PM_RECOV_ERR);
	 return;
    }
    _pmcd_trace_nbufs = n;
    next = 0;
}

void
pmcd_trace(int type, int who, int p1, int p2)
{
    int		p;

    if (trace == NULL) {
	pmcd_init_trace(_pmcd_trace_nbufs);
	if (trace == NULL)
	    return;
    }

    switch (type) {
	case TR_XMIT_PDU:
	case TR_RECV_PDU:
	    if ((_pmcd_trace_mask & TR_MASK_PDU) == 0)
		return;
	default:
	    if ((_pmcd_trace_mask & TR_MASK_CONN) == 0)
		return;
    }

    p = (next++) % _pmcd_trace_nbufs;

    time(&trace[p].t_stamp);
    trace[p].t_type = type;
    trace[p].t_who = who;
    trace[p].t_p1 = p1;
    trace[p].t_p2 = p2;

    if (_pmcd_trace_mask & TR_MASK_NOBUF)
	/* unbuffered, dump it now */
	pmcd_dump_trace(stderr);
}

void
pmcd_dump_trace(FILE *f)
{
    int			i;
    int			p;
    struct tm		last;
    struct tm		this;
    struct in_addr	addr;	/* internet address */
    struct hostent	*hp;

    if ((_pmcd_trace_mask & TR_MASK_NOBUF) == 0)
	fprintf(f, "\n->PMCD event trace: ");
    if (trace != NULL && next != 0) {
	if (next < _pmcd_trace_nbufs)
	    i = 0;
	else
	    i = next - _pmcd_trace_nbufs;
	if ((_pmcd_trace_mask & TR_MASK_NOBUF) == 0) {
	    fprintf(f, "starting at %s", ctime(&trace[i % _pmcd_trace_nbufs].t_stamp));
	    last.tm_hour = -1;
	}
	else
	    last.tm_hour = -2;

	for ( ; i < next; i++) {
	    fprintf(f, "->");
	    p = i % _pmcd_trace_nbufs;
	    localtime_r(&trace[p].t_stamp, &this);
	    if (this.tm_hour != last.tm_hour ||
		this.tm_min != last.tm_min ||
		this.tm_sec != last.tm_sec) {
		if (last.tm_hour == -1)
		    fprintf(f, "         ");
		else
		    fprintf(f, "%02d:%02d:%02d ", this.tm_hour, this.tm_min, this.tm_sec);
		last = this;
	    }
	    else
		fprintf(f, "         ");

	    switch (trace[p].t_type) {

		case TR_ADD_CLIENT:
		    fprintf(f, "New client: from=");
		    addr.s_addr = trace[p].t_who;
		    hp = gethostbyaddr((void *)&addr.s_addr, sizeof(addr.s_addr), AF_INET);
		    if (hp == NULL) {
			char	*p = (char *)&addr.s_addr;
			int	k;
			for (k = 0; k < 4; k++) {
			    if (k > 0)
				fputc('.', f);
			    fprintf(f, "%d", p[k] & 0xff);
			}
		    }
		    else {
			fprintf(f, "%s", hp->h_name);
		    }
		    fprintf(f, ", fd=%d\n", trace[p].t_p1);
		    break;

		case TR_DEL_CLIENT:
		    fprintf(f, "End client: fd=%d", trace[p].t_who);
		    if (trace[p].t_p1 != 0)
			fprintf(f, ", err=%d: %s", trace[p].t_p1, pmErrStr(trace[p].t_p1));
		    fputc('\n', f);
		    break;

		case TR_ADD_AGENT:
		    fprintf(f, "Add PMDA: domain=%d, ", trace[p].t_who);
		    if (trace[p].t_p1 == -1 && trace[p].t_p2 == -1)
			fprintf(f, "DSO\n");
		    else
			fprintf(f, "infd=%d, outfd=%d\n", trace[p].t_p1, trace[p].t_p2);
		    break;

		case TR_DEL_AGENT:
		    fprintf(f, "Drop PMDA: domain=%d, ", trace[p].t_who);
		    if (trace[p].t_p1 == -1 && trace[p].t_p2 == -1)
			fprintf(f, "DSO\n");
		    else
			fprintf(f, "infd=%d, outfd=%d\n", trace[p].t_p1, trace[p].t_p2);
		    break;

		case TR_EOF:
		    fprintf(f, "Premature EOF: expecting %s PDU, fd=%d\n",
			trace[p].t_p1 == -1 ? "NO" : __pmPDUTypeStr(trace[p].t_p1),
			trace[p].t_who);
		    break;

		case TR_WRONG_PDU:
		    if (trace[p].t_p2 > 0) {
			fprintf(f, "Wrong PDU type: expecting %s PDU, fd=%d, got %s PDU\n",
			    trace[p].t_p1 == -1 ? "NO" : __pmPDUTypeStr(trace[p].t_p1),
			    trace[p].t_who,
			    trace[p].t_p2 == -1 ? "NO" : __pmPDUTypeStr(trace[p].t_p2));
		    }
		    else if (trace[p].t_p2 == 0) {
			fprintf(f, "Wrong PDU type: expecting %s PDU, fd=%d, got EOF\n",
			    trace[p].t_p1 == -1 ? "NO" : __pmPDUTypeStr(trace[p].t_p1),
			    trace[p].t_who);
		    }
		    else {
			fprintf(f, "Wrong PDU type: expecting %s PDU, fd=%d, got err=%d: %s\n",
			    trace[p].t_p1 == -1 ? "NO" : __pmPDUTypeStr(trace[p].t_p1),
			    trace[p].t_who,
			    trace[p].t_p2, pmErrStr(trace[p].t_p2));

		    }
		    break;

		case TR_XMIT_ERR:
		    fprintf(f, "Send %s PDU failed: fd=%d, err=%d: %s\n",
			__pmPDUTypeStr(trace[p].t_p1), trace[p].t_who,
			trace[p].t_p2, pmErrStr(trace[p].t_p2));
		    break;

		case TR_RECV_TIMEOUT:
		    fprintf(f, "Recv timeout: expecting %s PDU, fd=%d\n",
			__pmPDUTypeStr(trace[p].t_p1), trace[p].t_who);
		    break;

		case TR_RECV_ERR:
		    fprintf(f, "Recv error: expecting %s PDU, fd=%d, err=%d: %s\n",
			__pmPDUTypeStr(trace[p].t_p1), trace[p].t_who,
			trace[p].t_p2, pmErrStr(trace[p].t_p2));
		    break;

		case TR_XMIT_PDU:
		    fprintf(f, "Xmit: %s PDU, fd=%d",
			__pmPDUTypeStr(trace[p].t_p1), trace[p].t_who);
		    if (trace[p].t_p1 == PDU_ERROR)
			fprintf(f, ", err=%d: %s",
			    trace[p].t_p2, pmErrStr(trace[p].t_p2));
		    else if (trace[p].t_p1 == PDU_RESULT)
			fprintf(f, ", numpmid=%d", trace[p].t_p2);
		    else if (trace[p].t_p1 == PDU_TEXT_REQ ||
			     trace[p].t_p1 == PDU_TEXT)
			fprintf(f, ", id=0x%x", trace[p].t_p2);
		    else if (trace[p].t_p1 == PDU_DESC_REQ ||
			     trace[p].t_p1 == PDU_DESC)
			fprintf(f, ", pmid=%s", pmIDStr((pmID)trace[p].t_p2));
		    else if (trace[p].t_p1 == PDU_INSTANCE_REQ ||
			     trace[p].t_p1 == PDU_INSTANCE)
			fprintf(f, ", indom=%s", pmInDomStr((pmInDom)trace[p].t_p2));
		    else if (trace[p].t_p1 == PDU_PMNS_NAMES)
			fprintf(f, ", numpmid=%d", trace[p].t_p2);
		    else if (trace[p].t_p1 == PDU_PMNS_IDS)
			fprintf(f, ", numpmid=%d", trace[p].t_p2);
		    else if (trace[p].t_p1 == PDU_CREDS)
			fprintf(f, ", numcreds=%d", trace[p].t_p2);
		    fputc('\n', f);
		    break;

		case TR_RECV_PDU:
		    fprintf(f, "Recv: %s PDU, fd=%d, pdubuf=0x",
			__pmPDUTypeStr(trace[p].t_p1), trace[p].t_who);
#if (_MIPS_SZPTR != _MIPS_SZINT)
		    fprintf(f, "...");
#endif
		    fprintf(f, "%x\n", trace[p].t_p2);
		    break;

		default:
		    fprintf(f, "Type=%d who=%d p1=%d p2=%d\n",
			trace[p].t_type, trace[p].t_who, trace[p].t_p1,
			trace[p].t_p2);
		    break;
	    }
	}
    }
    else
	fprintf(f, "<empty>\n");

    if ((_pmcd_trace_mask & TR_MASK_NOBUF) == 0)
	fputc('\n', f);
    next = 0;			/* empty the circular buffer */
}
