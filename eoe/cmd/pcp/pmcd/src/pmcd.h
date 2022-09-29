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

#ident "$Id: pmcd.h,v 2.14 1999/08/17 04:13:41 kenmcd Exp $"

#ifndef _PMCD_H
#define _PMCD_H

#include "pmapi.h"
#include "impl.h"
#include "client.h"
#include "pmda.h"

/* Structures of type-specific info for each kind of domain agent-PMCD 
 * connection (DSO, socket, pipe).
 */

typedef void (*DsoInitPtr)(pmdaInterface*);

typedef struct {
    char		*pathName;	/* Where the DSO lives */
    int			xlatePath;	/* translated pathname? */
    char		*entryPoint;	/* Name of the entry point */
    void		*dlHandle;	/* Handle for DSO */
    DsoInitPtr		initFn;		/* Function to initialise DSO */
					/* and return dispatch table */
    pmdaInterface	dispatch;      	/* Dispatch table for dso agent */
} DsoInfo;

typedef struct {
    int	  addrDomain;			/* AF_UNIX or AF_INET */
    int	  port;				/* Port number if an INET socket */
    char  *name;			/* Port name if supplied for INET */
					/* or socket name for UNIX */
    char  *commandLine;			/* Optinal command to start agent */
    char* *argv;			/* Arg list built from commandLine */
    pid_t agentPid;			/* Process is of agent if PMCD started */
} SocketInfo;

typedef struct {
    char* commandLine;			/* Command line to use for child */
    char* *argv;			/* Arg list built from command line */
    pid_t agentPid;			/* Process id of the agent */
} PipeInfo;

/* The agent table and its size. */

typedef struct {
    int        pmDomainId;		/* PMD identifier */
    int        ipcType;			/* DSO, socket or pipe */
    int        pduProtocol;		/* Agent expects binary/ascii PDUs */
    int        pduVersion;		/* PDU_VERSION for this agent */
    int        inFd, outFd;		/* For input to/output from agent */
    int	       done;			/* Set when processed for this Fetch */
    ClientInfo *profClient;		/* Last client to send profile to agent */
    int	       profIndex;		/* Index of profile that client sent */
    char       *pmDomainLabel;		/* Textual label for agent's PMD */
    struct {				/* Status of agent */
	unsigned int
	    connected : 1,		/* Agent connected to pmcd */
	    busy : 1,			/* Processing a request */
	    isChild : 1,		/* Is a child process of pmcd */
	    madeDsoResult : 1,		/* Pmcd made a "bad" pmResult (DSO only) */
	    restartKeep : 1,		/* Keep agent if set during restart */
	    notReady : 1;		/* Agent not ready to process PDUs */
    } status;
    int		reason;			/* if ! connected */
    union {				/* per-ipcType info */
	DsoInfo    dso;
	SocketInfo socket;
	PipeInfo   pipe;
    } ipc;
} AgentInfo;

extern AgentInfo *agent;		/* Array of domain agent structs */
extern int	 nAgents;		/* Number of agents in array */

/* DomainId-to-AgentIndex map */
#define MAXDOMID	254		/* 8 bits of DomainId, 255 is special */
extern int		mapdom[];	/* the map */

/* Domain agent-PMCD connection types (AgentInfo.ipcType) */

#define	AGENT_DSO	0
#define AGENT_SOCKET	1
#define AGENT_PIPE	2

/* Masks for operations used in access controls for clients. */
#define OP_FETCH	0x1
#define	OP_STORE	0x2

#define OP_NONE		0x0
#define	OP_ALL		0x3

/* Agent termination reasons */
#define AT_CONFIG	1
#define AT_COMM		2
#define AT_EXIT		3

/*
 * Agent termination reasons for "reason" in AgentInfo, and pmcd.agent.state
 * as exported by PMCD PMDA ... these encode the low byte, next byte contains
 * exit status and next byte encodes signal
 */
				/* 0 connected */
				/* 1 connected, not ready */
#define REASON_EXIT	2
#define REASON_NOSTART	4
#define REASON_PROTOCOL	8

extern AgentInfo *FindDomainAgent(int);
extern void CleanupAgent(AgentInfo *, int, int);
extern void CleanupClient(ClientInfo*, int);
extern char* FdToString(int);
extern pmResult **SplitResult(pmResult *);
extern void Shutdown(void);

/* timeout to PMDAs (secs) */
extern int	_pmcd_timeout;

/*
 * trace types
 */

#define TR_ADD_CLIENT	1
#define TR_DEL_CLIENT	2
#define TR_ADD_AGENT	3
#define TR_DEL_AGENT	4
#define TR_EOF		5
#define TR_XMIT_PDU	7
#define TR_RECV_PDU	8
#define TR_WRONG_PDU	9
#define TR_XMIT_ERR	10
#define TR_RECV_TIMEOUT	11
#define TR_RECV_ERR	12

/*
 * trace control
 */
extern int		_pmcd_trace_mask;

/*
 * trace mask bits
 */
#define TR_MASK_CONN	1
#define TR_MASK_PDU	2
#define TR_MASK_NOBUF	256

/*
 * routines
 */
extern void pmcd_init_trace(int);
extern void pmcd_trace(int, int, int, int);
extern void pmcd_dump_trace(FILE *);
extern int pmcd_load_libpcp_pmda(void);

/*
 * set state change status for clients
 */
extern void MarkStateChanges(int);

/*
 * Keep the highest know file desriptor used for a Client or an Agent connection.
 * This is reported in the pmcd.openfds metric. See Bug #660497.
 */
extern int pmcd_hi_openfds;   /* Highest known file descriptor for pmcd */

#define PMCD_OPENFDS_SETHI(x) if (x > pmcd_hi_openfds) pmcd_hi_openfds = x;

#endif /* _PMCD_H */
