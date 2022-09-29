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

#ident "$Id: err.c,v 2.24 1998/02/25 21:44:07 kenmcd Exp $"

#include <stdio.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"

/*
 * if you modify this table at all, be sure to remake qa/006
 */
static const struct {
    int  	err;
    char	*symb;
    char	*errmess;
} errtab[] = {
    { PM_ERR_GENERIC,		"PM_ERR_GENERIC",
	"Generic error, already reported above" },
    { PM_ERR_PMNS,		"PM_ERR_PMNS",
	"Problems parsing PMNS definitions" },
    { PM_ERR_NOPMNS,		"PM_ERR_NOPMNS",
	"PMNS not accessible" },
    { PM_ERR_DUPPMNS,		"PM_ERR_DUPPMNS",
	"Attempt to reload the PMNS" },
    { PM_ERR_TEXT,		"PM_ERR_TEXT",
	"Oneline or help text is not available" },
    { PM_ERR_APPVERSION,	"PM_ERR_APPVERSION",
	"Metric not supported by this version of monitored application" },
    { PM_ERR_VALUE,		"PM_ERR_VALUE",
	"Missing metric value(s)" },
    { PM_ERR_LICENSE,		"PM_ERR_LICENSE",
	"Current PCP license does not permit this operation" },
    { PM_ERR_TIMEOUT,		"PM_ERR_TIMEOUT",
	"Timeout waiting for a response from PMCD" },
    { PM_ERR_NODATA,		"PM_ERR_NODATA",
	"Empty archive log file" },
    { PM_ERR_RESET,		"PM_ERR_RESET",
	"PMCD reset or configuration change" },
    { PM_ERR_FILE,		"PM_ERR_FILE",
	"Cannot locate a file" },
    { PM_ERR_NAME,		"PM_ERR_NAME",
	"Unknown metric name" },
    { PM_ERR_PMID,		"PM_ERR_PMID",
	"Unknown or illegal metric identifier" },
    { PM_ERR_INDOM,		"PM_ERR_INDOM",
	"Unknown or illegal instance domain identifier" },
    { PM_ERR_INST,		"PM_ERR_INST",
	"Unknown or illegal instance identifier" },
    { PM_ERR_UNIT,		"PM_ERR_UNIT",
	"Illegal pmUnits specification" },
    { PM_ERR_CONV,		"PM_ERR_CONV",
	"Impossible value or scale conversion" },
    { PM_ERR_TRUNC,		"PM_ERR_TRUNC",
	"Truncation in value conversion" },
    { PM_ERR_SIGN,		"PM_ERR_SIGN",
	"Negative value in conversion to unsigned" },
    { PM_ERR_PROFILE,		"PM_ERR_PROFILE",
	"Explicit instance identifier(s) required" },
    { PM_ERR_IPC,		"PM_ERR_IPC",
	"IPC protocol failure" },
    { PM_ERR_NOASCII,		"PM_ERR_NOASCII",
	"ASCII format not supported for this PDU" },
    { PM_ERR_EOF,		"PM_ERR_EOF",
	"IPC channel closed" },
    { PM_ERR_NOTHOST,		"PM_ERR_NOTHOST",
	"Operation requires context with host source of metrics" },
    { PM_ERR_EOL,		"PM_ERR_EOL",
	"End of PCP archive log" },
    { PM_ERR_MODE,		"PM_ERR_MODE",
	"Illegal mode specification" },
    { PM_ERR_LABEL,		"PM_ERR_LABEL",
	"Illegal label record at start of a PCP archive log file" },
    { PM_ERR_LOGREC,		"PM_ERR_LOGREC",
	"Corrupted record in a PCP archive log" },
    { PM_ERR_NOTARCHIVE,	"PM_ERR_NOTARCHIVE",
	"Operation requires context with archive source of metrics" },
    { PM_ERR_NOCONTEXT,		"PM_ERR_NOCONTEXT",
	"Attempt to use an illegal context" },
    { PM_ERR_PROFILESPEC,	"PM_ERR_PROFILESPEC",
	"NULL pmInDom with non-NULL instlist" },
    { PM_ERR_PMID_LOG,		"PM_ERR_PMID_LOG",
	"Metric not defined in the PCP archive log" },
    { PM_ERR_INDOM_LOG,		"PM_ERR_INDOM_LOG",
	"Instance domain identifier not defined in the PCP archive log" },
    { PM_ERR_INST_LOG,		"PM_ERR_INST_LOG",
	"Instance identifier not defined in the PCP archive log" },
    { PM_ERR_NOPROFILE,		"PM_ERR_NOPROFILE",
	"Missing profile - protocol botch" },
    { PM_ERR_NOAGENT,		"PM_ERR_NOAGENT",
	"No PMCD agent for domain of request" },
    { PM_ERR_PERMISSION,	"PM_ERR_PERMISSION",
	"No permission to perform requested operation" },
    { PM_ERR_CONNLIMIT,		"PM_ERR_CONNLIMIT",
	"PMCD connection limit for this host exceeded" },
    { PM_ERR_AGAIN,		"PM_ERR_AGAIN",
	"Try again. Information not currently available" },
    { PM_ERR_ISCONN,		"PM_ERR_ISCONN",
	"Already Connected" },
    { PM_ERR_NOTCONN,		"PM_ERR_NOTCONN",
	"Not Connected" },
    { PM_ERR_NEEDPORT,		"PM_ERR_NEEDPORT",
	"A non-null port name is required" },
    { PM_ERR_WANTACK,		"PM_ERR_WANTACK",
	"Cannot send due to pending acknowledgements" },
    { PM_ERR_NONLEAF,		"PM_ERR_NONLEAF",
	"Metric name is not a leaf in PMNS" },
    { PM_ERR_PMDANOTREADY,	"PM_ERR_PMDANOTREADY",
	"PMDA is not yet ready to respond to requests" },
    { PM_ERR_PMDAREADY,		"PM_ERR_PMDAREADY",
	"PMDA is now responsive to requests" },
    { PM_ERR_OBJSTYLE,		"PM_ERR_OBJSTYLE",
	"Caller does not match object style of running kernel" },
    { PM_ERR_PMCDLICENSE,	"PM_ERR_PMCDLICENSE",
	"PMCD is not licensed to accept client connections" },
    { PM_ERR_TOOSMALL,		"PM_ERR_TOOSMALL",
	"Insufficient elements in list" },
    { PM_ERR_TOOBIG,		"PM_ERR_TOOBIG",
	"Result size exceeded" },
    { PM_ERR_NYI,		"PM_ERR_NYI",
	"Functionality not yet implemented" },
    { 0,			"",
	"" }
};


#define BADCODE "No such PMAPI error code (%d)"
static char	barf[45];

const char *
pmErrStr(int code)
{
    if ((code < 0) && (code > -sys_nerr))
	/* intro(2) errors */
	return strerror(-code);
    else if (code == 0)
	return "No error";
    else {
	int	i;
	for (i = 0; errtab[i].err; i++)
	    if (errtab[i].err == code)
		return errtab[i].errmess;

	sprintf(barf, BADCODE,  code);
	return barf;
    }
}

void
__pmDumpErrTab(FILE *f)
{
    int	i;

    fprintf(f, "  Code  Symbolic Name        Message\n");
    for (i = 0; errtab[i].err; i++)
	fprintf(f, "%6d  %-20s %s\n",
	    errtab[i].err, errtab[i].symb, errtab[i].errmess);
}
