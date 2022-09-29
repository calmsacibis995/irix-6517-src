/*
 * PMCD routines for reading config file, creating children and
 * attaching to DSOs.
 *
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

#ident "$Id: config.c,v 2.73 1999/08/17 04:13:41 kenmcd Exp $"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/wait.h>

#if defined(sgi)
#include <sgidefs.h>
#include <rld_interface.h>
#endif

#include "pmapi.h"
#include "pmapi_dev.h"
#include "impl.h"
#include "pmcd.h"

#if defined(HAVE_DLFCN)
#include <dlfcn.h>
#elif defined(HAVE_SHL)
#include <dl.h>
#else
!bozo!
#endif


#define MIN_AGENTS_ALLOC	3	/* Number to allocate first time */
#define LINEBUF_SIZE 200

extern int	errno;
extern int	_creds_timeout;

/* Config file modification time */
#if defined(HAVE_STAT_TIMESTRUC)
static timestruc_t      configFileTime;
#elif defined(HAVE_STAT_TIMESPEC)
static timespec_t       configFileTime;
#elif defined(HAVE_STAT_TIME_T)
static time_t   	configFileTime;
#else
!bozo!
#endif

AgentInfo	*agent = NULL; /* List of agent info structs */
int		nAgents = 0;		/* Number of structs in use */
int		szAgents = 0;		/* Number currently allocated */
int		mapdom[MAXDOMID+2];	/* The DomainId-to-AgentIndex map */
					/* Don't use it during parsing! */

static FILE	*inputStream = NULL;    /* Input stream for scanner */
static int	scanInit = 0;
static int	scanError = 0;		/* Problem in scanner */
static char	*linebuf;		/* Buffer for input stream */
static int	szLineBuf = 0;		/* Allocated size of linebuf */
static char	*token = NULL;		/* Start of current token */
static int	max_seen_fd = -1;	/* Larges fd we've seen */
static char	*tokenend = NULL;	/* End of current token */
static int	nLines;			/* Current line of config file */
static int	linesize = 0;		/* Length of line in linebuf */

/* Macro to compare a string with token.  The linebuf is always null terminated
 * so there are no nasty boundary conditions.
 */
#define TokenIs(str)	((tokenend - token) == strlen(str) && \
			 !strncasecmp(token, str, strlen(str)))

/* Return the numeric value of token (or zero if token is not numeric). */
static int
TokenNumVal(void)
{
    int		val = 0;
    char	*p = token;
    while (isdigit(*p)) {
	val = val * 10 + *p - '0';
	p++;
    }
    return val;
}

/* Return true if token is a numeric value */
static int
TokenIsNumber(void)
{
    char	*p;
    if (token == tokenend)		/* Nasty end of input case */
	return 0;
    for (p = token; isdigit(*p); p++)
	;
    return p == tokenend;
}

/* Return a strdup-ed copy of the current token. */
static char*
CopyToken(void)
{
    int		len = (int)(tokenend - token);
    char	*copy = (char *)malloc(len + 1);
    if (copy != NULL) {
	strncpy(copy, token, len);
	copy[len] = '\0';
    }
    return copy;
}

/* Get the next line from the input stream into linebuf. */

static void
GetNextLine(void)
{
    char	*end;
    int		more;			/* There is more line to read */
    int		still_to_read;
    int		atEOF = 0;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	fprintf(stderr, "%d: GetNextLine()\n", nLines);
#endif

    if (szLineBuf == 0) {
	szLineBuf = LINEBUF_SIZE;
	linebuf = (char *)malloc(szLineBuf);
	if (linebuf == NULL)
	    __pmNoMem("pmcd config: GetNextLine init", szLineBuf, PM_FATAL_ERR);
	/* NOTREACHED */
    }

    linebuf[0] = '\0';
    token = linebuf;
    if (feof(inputStream))
	return;

    end = linebuf;
    more = 0;
    still_to_read = szLineBuf;
    do {
	/* Read into linebuf.  If more is set, the read is into a realloc()ed
	 * version of linebuf.  In this case, more is the number of characters
	 * at the end of the previous batch that should be overwritten
	 */
	if (fgets(end, still_to_read, inputStream) == NULL) {
	    if (!feof(inputStream)) {
		fprintf(stderr, "pmcd config: line %d, %s\n",
			     nLines, strerror(errno));
		scanError = 1;
		return;
	    }
	    atEOF = 1;
	}

	linesize = (int)strlen(linebuf);
	more = 0;
	if (linesize == 0)
	    break;
	if (linebuf[linesize - 1] != '\n') {
	    if (feof(inputStream)) {
		/* Final input line has no '\n', so add one.  If a terminating
		 * null fits after it, that's the line, so break out of loop.
		 */
		linebuf[linesize] = '\n';
		/* Add terminating null if it fits */
		if (linesize + 1 < szLineBuf) {
		    linebuf[++linesize] = '\0';
		    break;
		}
		/* If no room for null, get more buffer space */
	    }
	    more = 1;			/* More buffer space required */
	}
	/* Check for continued lines */
	else if (linesize > 1 && linebuf[linesize - 2] == '\\') {
	    linebuf[linesize - 2] = ' ';
	    linesize--;			/* Pretend the \n isn't there */
	    more = 2;			/* Overwrite \n and \0 on next read */
	}
	    
	/* Make buffer larger to accomodate more of the line. */
	if (more) {
	    if (szLineBuf > 10 * LINEBUF_SIZE) {
		fprintf(stderr,
			     "pmcd config: line %d ridiculously long\n",
			     nLines+1);
		linebuf[0] = '\0';
		scanError = 1;
		return;
	    }
	    szLineBuf += LINEBUF_SIZE;
	    if ((linebuf = realloc(linebuf, szLineBuf)) == NULL) {
		static char	fallback[2];

		__pmNoMem("pmcd config: GetNextLine", szLineBuf, PM_RECOV_ERR);
		linebuf = fallback;
		linebuf[0] = '\0';
		scanError = 1;
		return;
	    }
	    end = linebuf + linesize;
	    still_to_read = LINEBUF_SIZE + more;
	    /* *end is where the next fgets will start putting data.
	     * There is a special case if we are already at end of input:
	     * *end is the '\n' added to the line since it didn't have one.
	     * We are here because the terminating null wouldn't fit.
	     */
	    if (atEOF) {
		end[1] = '\0';
		linesize++;
		break;
	    }
	    token = linebuf;		/* We may have a new buffer */
	}
    } while (more);
    nLines++;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "\n===================NEWLINE=================\n\n");
	fprintf(stderr, "len = %d\nline = \"%s\"\n", (int)strlen(linebuf), linebuf);
    }
#endif
}

/* Advance through the input stream until either a non-whitespace character, a
 * newline, or end of input is reached.
 */
static void
SkipWhitespace(void)
{
    while (*token) {
	char	ch = *token;

	if (isspace(ch))
	    if (ch == '\n')			/* Stop at end of line */
		return;
	    else
		token++;
	else if (ch == '#') {
	    token = &linebuf[linesize-1];
	    return;
	}
	else
	    return;
    }
}

static int	scanReadOnly = 0;	/* Set => don't modify input scanner */
static int	doingAccess = 0;	/* Set => parsing [access] section */
static int	tokenQuoted = 0;	/* True when token a quoted string */

/* Print the current token on a given stream. */

static void
PrintToken(FILE *stream)
{
    char	*p;
    if (tokenQuoted)
	fputc('"', stream);
    for (p = token; p < tokenend; p++) {
	if (*p == '\n')
	    fputs("<newline>", stream);
	else if (*p == '\0')
	    fputs("<null>", stream);
	else
	    fputc(*p, stream);
    }
    if (tokenQuoted)
	fputc('"', stream);
}

/* Move to the next token in the input stream.  This is done by skipping any
 * non-whitespace characters to get to the end of the current token then
 * skipping any whitespace and newline characters to get to the next token.
 */

static void
FindNextToken(void)
{
    static char	*rawToken;		/* Used in pathological quoting case */
    char	ch;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
	fprintf(stderr, "FindNextToken()\n");
#endif

    do {
	if (scanInit) {
	    if (*token == '\0')		/* EOF is EOF and that's final */
		return;
	    if (scanError)		/* Ditto for scanner errors */
		return;

	    if (*token == '\n')		/* If at end of line, get next line */
		GetNextLine();
	    else {			/* Otherwise move past "old" token */
		if (tokenQuoted)	/* Move past last quote */
		    tokenend++;
		token = tokenend;
	    }
	    SkipWhitespace();		/* Find null, newline or non-space  */
	}
	else {
	    scanInit = 1;
	    scanError = 0;
	    GetNextLine();
	    SkipWhitespace();		/* Don't return yet, find tokenend */
	}
    } while (doingAccess && *token == '\n');

    /* Now we have the start of a token.  Find the end. */

    ch = *token;
    if (ch == '\0' || ch == '\n') {
	tokenend = token;
	return;
    }

    if (doingAccess)
	if (ch == ',' || ch == ':' || ch == ';' || ch == '[' || ch == ']') {
	    tokenend = token + 1;
	    return;
	}

    rawToken = token;			/* Save real token start in case it moves */
    tokenend = token + 1;
    if (ch == '#')			/* For comments, token is newline */
	token = tokenend = &linebuf[linesize-1];
    else {
	int	inQuotes = *token == '"';
	int	fixToken = 0;

	do {
	    int gotEnd = isspace(*tokenend);

	    while (!gotEnd) {
		switch (*tokenend) {
		    case '#':		/* \# or # in quotes does not start a comment */
			if (*(tokenend - 1) == '\\' || inQuotes)
			    fixToken = 1;
			else		/* Comments don't need whitespace in front */
			    gotEnd = 1;
			break;

		    case ',':
		    case ':':
		    case ';':
		    case '[':
		    case ']':
			gotEnd = doingAccess && !inQuotes;
			break;

		    case '"':
			if (*(tokenend - 1) == '\\')
			    fixToken = 1;
			else {
			    int	err = 1;

			    if (inQuotes) {
				inQuotes = 0;
				err = tokenend[1] && !isspace(tokenend[1]);
				gotEnd = 1;
			    }
			    if (err) {
				scanError = 1;
				*token = '\0';
				tokenend = token;
				fprintf(stderr,
					     "pmcd config: line %d, quoted string must have whitespace either side\n",
					     nLines);
				return;
			    }
			}
			break;

		    default:
			gotEnd = isspace(*tokenend);
		}
		if (gotEnd)
		    break;
		tokenend++;
	    }
	    /* Skip any whitespace if still in quotes, but stop at end of line */
	    if (inQuotes)
		while (isspace(*tokenend) && *tokenend != '\n')
		    tokenend++;
	} while (inQuotes && *tokenend != '\n');

	if (inQuotes) {
	    scanError = 1;
	    *token = 0;
	    tokenend = token;
	    fprintf(stderr,
			 "pmcd config: line %d, unterminated quoted string\n",
			 nLines);
	    return;
	}

	/* Replace any \# or \" in the token with # or " */
	if (fixToken && !scanReadOnly) {
	    char	*p, *q;

	    for (p = q = tokenend; p >= token; p--) {
		if (*p == '\\' && ( p[1] == '#' || p[1] == '"') )
		    continue;
		*q-- = *p;
	    }
	    token = q + 1;
	}
    }

    /* If token originally started with a quote, token is what's inside quotes.
     * Note that *rawToken is checked since *token will also be " if the
     * token originally started with a \" that has been changed to ".
     */
    if (*rawToken == '"') {
	token++;
	tokenQuoted = 1;
    }
    else
	tokenQuoted = 0;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fputs("TOKEN = '", stderr);
	PrintToken(stderr);
	fputs("'\n", stderr);
    }
#endif
}

/* Move to the next line of the input stream. */

static void
SkipLine(void)
{
    while (*token && *token != '\n')
	FindNextToken();
    FindNextToken();			/* Move over the newline */
}

/* From an argv, build a command line suitable for display in logs etc. */

static char *
BuildCmdLine(char **argv)
{
    int		i, cmdLen = 0;
    char	*cmdLine;
    char	*p;

    if (argv == NULL)
	return NULL;
    for (i = 0; argv[i] != NULL; i++) {
	cmdLen += strlen(argv[i]) + 1;	/* +1 for space spearator or null */
	/* any arg with whitespace appears in quotes */
	if (strpbrk(argv[i], " \t") != NULL)
	    cmdLen += 2;
	/* any quote gets a \ prepended */
	for (p = argv[i]; *p; p++)
	    if (*p == '"')
		cmdLen++;
    }

    if ((cmdLine = (char *)malloc(cmdLen)) == NULL) {
	fprintf(stderr, "pmcd config: line %d, error building command line\n",
		nLines);
	__pmNoMem("pmcd config: BuildCmdLine", cmdLen, PM_RECOV_ERR);
	return NULL;
    }
    for (i = 0, p = cmdLine; argv[i] != NULL; i++) {
	int	quote = strpbrk(argv[i], " \t") != NULL;
	char	*q;

	if (quote)
	    *p++ = '"';
	for (q = argv[i]; *q; q++) {
	    if (*q == '"')
		*p++ = '\\';
	    *p++ = *q;
	}
	if (quote)
	    *p++ = '"';
	if (argv[i+1] != NULL)
	    *p++ = ' ';
    }
    *p = '\0';
    return cmdLine;
}


/* Build an argument list suitable for an exec call from the rest of the tokens
 * on the current line.
 */
char **
BuildArgv(void)
{
    int		nArgs;
    char	**result;

    nArgs = 0;
    result = NULL;
    do {
	/* Make result big enough for new arg and terminating NULL pointer */
	result = (char **) realloc(result, (nArgs + 2) * sizeof(char *));
	if (result != NULL)
	    result[nArgs] = CopyToken();
	if (result == NULL || result[nArgs] == NULL) {
	    fprintf(stderr, "pmcd config: line %d, error building argument list\n",
		    nLines);
	    __pmNoMem("pmcd config: build argv", nArgs * sizeof(char *),
		     PM_RECOV_ERR);
	    return NULL;
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    fprintf(stderr, "argv[%d] = '%s'\n", nArgs, result[nArgs]);
#endif

	nArgs++;
	FindNextToken();
    } while (*token && *token != '\n');
    result[nArgs] = NULL;

    return result;
}

/* Return the next unused index into the agent array, extending the array
   as necessary. */
static int
GetNewAgentIndex(void)
{
    if (agent == NULL) {
	agent = (AgentInfo*) malloc(sizeof(AgentInfo) * MIN_AGENTS_ALLOC);
	if (agent == NULL) {
	    perror("GetNewAgentIndex: malloc");
	    exit(1);
	}
#ifdef MALLOC_AUDIT
	_persistent_(agent);
#endif

	szAgents = MIN_AGENTS_ALLOC;
    }
    else if (nAgents >= szAgents) {
	agent = (AgentInfo*)
	    realloc(agent, sizeof(AgentInfo) * 2 * szAgents);
	if (agent == NULL) {
	    perror("GetNewAgentIndex: realloc");
	    exit(1);
	}
	szAgents *= 2;
    }
    return nAgents++;
}

/* Free any malloc()-ed memory associated with an agent */

static void
FreeAgent(AgentInfo *ap)
{
    int		i;
    char	**argv = NULL;

    free(ap->pmDomainLabel);
    if (ap->ipcType == AGENT_DSO) {
	free(ap->ipc.dso.pathName);
	free(ap->ipc.dso.entryPoint);
    }
    else if (ap->ipcType == AGENT_SOCKET) {
	if (ap->ipc.socket.commandLine != NULL) {
	    free(ap->ipc.socket.commandLine);
	    argv = ap->ipc.socket.argv;
	}
    }
    else
	if (ap->ipc.pipe.commandLine != NULL) {
	    free(ap->ipc.pipe.commandLine);
	    argv = ap->ipc.pipe.argv;
	}
    
    if (argv != NULL) {
	for (i = 0; argv[i] != NULL; i++)
	    free(argv[i]);
	free(argv);
    }
}

/*
 * Use compilation flags to determine what format this pmcd is compiled into.
 * pmcd can only load DSOs compiled in the same MIPS format that it is.
 *
 * Each of the MIPS executable/DSO formats has a different prefix to allow
 * multiple versions of the same DSO agent to coexist in one directory.
 */

static char	*dsoPrefix =
#if   _MIPS_SIM == _MIPS_SIM_ABI32
    "mips_o32.";
#elif _MIPS_SIM == _MIPS_SIM_NABI32
    "mips_n32.";
#elif _MIPS_SIM == _MIPS_SIM_ABI64
    "mips_64.";
#else
    !!! bozo : dont know which MIPS executable format pmcd should be!!!
#endif


/* Parse a DSO specification, creating and initialising a new entry in the
 * agent table if the spec has no errors.
 */
static int
ParseDso(char *pmDomainLabel, int pmDomainId)
{
    char	*pathName;
    char	*entryPoint;
    AgentInfo	*newAgent;
    int		i;
    int		xlatePath = 0;

    FindNextToken();
    if (*token == '\n') {
	fprintf(stderr, "pmcd: line %d, expected DSO entry point\n", nLines);
	return -1;
    }
    else
	if ((entryPoint = CopyToken()) == NULL) {
	    fprintf(stderr,
			 "pmcd config: line %d, couldn't copy DSO entry point\n",
			 nLines);
	    __pmNoMem("pmcd config", tokenend - token + 1, PM_FATAL_ERR);
	    /* NOTREACHED */
	}

    FindNextToken();
    if (*token == '\n') {
	fprintf(stderr, "pmcd: line %d, expected DSO pathname\n", nLines);
	return -1;
    }

    if ((pathName = CopyToken()) == NULL) {
	fprintf(stderr,
		     "pmcd config: line %d, couldn't copy DSO pathname\n",
		     nLines);
	__pmNoMem("pmcd config", tokenend - token + 1, PM_FATAL_ERR);
	/* NOTREACHED */
    }
    /*
     * if the DSO pathname is not absolute, prepend the appropriate "well
     * known directory" where the DSOs have the same MIPS format as this
     * pmcd executable.
     */
    if (*pathName != '/' && strcmp(pathName, "-") != 0) {
	char	*newPath;
	size_t	need = strlen(pathName) + strlen(dsoPrefix) + 1;

	if ((newPath = (char *)malloc(need)) == NULL) {
	    fprintf(stderr,
			 "pmcd config: line %d, couldn't construct DSO pathname\n",
			 nLines);
	    __pmNoMem("pmcd config", need, PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	strcpy(newPath, dsoPrefix);
	strcat(newPath, pathName);
	free(pathName);
	pathName = newPath;
	xlatePath = 1;
    }

    FindNextToken();
    if (*token != '\n') {
	fprintf(stderr, "pmcd: line %d, Too many parameters for DSO\n",
		     nLines);
	return -1;
    }

    /* Now create and initialise a slot in the agents table for the new agent */

    i = GetNewAgentIndex();
    newAgent = &agent[i];

    newAgent->ipcType = AGENT_DSO;
    newAgent->pmDomainId = pmDomainId;
    newAgent->pduProtocol = PDU_BINARY;
    newAgent->inFd = -1;
    newAgent->outFd = -1;
    newAgent->profClient = NULL;
    newAgent->pmDomainLabel = pmDomainLabel;
    newAgent->status.connected = 0;
    newAgent->status.busy = 0;
    newAgent->status.isChild = 0;
    newAgent->status.notReady = 0;
    newAgent->reason = 0;
    newAgent->ipc.dso.pathName = pathName;
    newAgent->ipc.dso.xlatePath = xlatePath;
    newAgent->ipc.dso.entryPoint = entryPoint;

#ifdef MALLOC_AUDIT
    _persistent_(pmDomainLabel);
    _persistent_(pathName);
    _persistent_(entryPoint);
#endif

    return 0;
}

/* Parse a socket specification, creating and initialising a new entry in the
 * agent table if the spec has no errors.
 */
static int
ParseSocket(char *pmDomainLabel, int pmDomainId)
{
    int		i, addrDomain, port = -1;
    char	*socketName = NULL;
    AgentInfo	*newAgent;

    FindNextToken();
    if (TokenIs("inet"))
	addrDomain = AF_INET;
    else if (TokenIs("unix"))
	addrDomain = AF_UNIX;
    else {
	fprintf(stderr,
		     "pmcd: line %d, expected socket address domain (`inet' or `unix')\n",
		     nLines);
	return -1;
    }

    FindNextToken();
    if (*token == '\n') {
	fprintf(stderr, "pmcd: line %d, expected socket port name or number\n",
		     nLines);
	return -1;
    }
    else if (TokenIsNumber())
	port = TokenNumVal();
    else
	if ((socketName = CopyToken()) == NULL) {
	    fprintf(stderr, "pmcd: line %d, couldn't copy port name\n",
			 nLines);
	    __pmNoMem("pmcd config", tokenend - token + 1, PM_FATAL_ERR);
	    /* NOTREACHED */
	}
    FindNextToken();

    /* If an internet domain port name was specified, find the corresponding
     port number. */

    if (addrDomain == AF_INET && socketName) {
	struct servent *service;

	service = getservbyname(socketName, NULL);
	if (service)
	    port = service->s_port;
	else {
	    fprintf(stderr,
			 "pmcd: line %d, Error getting port number for port %s: %s\n",
			 nLines, socketName, strerror(errno));
	    free(pmDomainLabel);
	    free(socketName);
	    return -1;
	}
    }

    /* Now create and initialise a slot in the agents table for the new agent */

    i = GetNewAgentIndex();
    newAgent = &agent[i];

    newAgent->ipcType = AGENT_SOCKET;
    newAgent->pmDomainId = pmDomainId;
    newAgent->pduProtocol = PDU_BINARY;
    newAgent->inFd = -1;
    newAgent->outFd = -1;
    newAgent->profClient = NULL;
    newAgent->pmDomainLabel = pmDomainLabel;
    newAgent->status.connected = 0;
    newAgent->status.busy = 0;
    newAgent->status.isChild = 0;	/* set to one when/if child created */
    newAgent->status.notReady = 0;
    newAgent->reason = 0;
    newAgent->ipc.socket.addrDomain = addrDomain;
    newAgent->ipc.socket.name = socketName;
    newAgent->ipc.socket.port = port;
    if (*token != '\n') {
	newAgent->ipc.socket.argv = BuildArgv();
	if (newAgent->ipc.socket.argv == NULL) {
	    fprintf(stderr, "pmcd: line %d, error building argv for \"%s\" agent.\n",
			 nLines, newAgent->pmDomainLabel);
	    return -1;
	}
	newAgent->ipc.socket.commandLine = BuildCmdLine(newAgent->ipc.socket.argv);
    }
    else {
	newAgent->ipc.socket.argv = NULL;
	newAgent->ipc.socket.commandLine = NULL;
    }
    newAgent->ipc.socket.agentPid = (pid_t) -1;

#ifdef MALLOC_AUDIT
    _persistent_(pmDomainLabel);
    if (socketName != NULL) _persistent_(socketName);
    if (commandLine != NULL) {
	char	**argv = newAgent->ipc.socket.argv;

	_persistent_(commandLine);
	for (i = 0; argv[i] != NULL; i++)
	    _persistent_(argv[i]);
    }
#endif

    return 0;
}

/* Parse a pipe specification, creating and initialising a new entry in the
 * agent table if the spec has no errors.
 */
static int
ParsePipe(char *pmDomainLabel, int pmDomainId)
{
    int		i, pduProtocol;
    AgentInfo	*newAgent;

    FindNextToken();
    if (TokenIs("binary"))
	pduProtocol = PDU_BINARY;
    else if (TokenIs("text"))
	pduProtocol = PDU_ASCII;
    else {
	fprintf(stderr,
		     "pmcd: line %d, pipe PDU type expected (`binary' or `text')\n",
		     nLines);
	return -1;
    }

    FindNextToken();
    if (*token == '\n') {
	fprintf(stderr,
		     "pmcd: line %d, command to create pipe agent expected.\n",
		     nLines);
	return -1;
    }

    /* Now create and initialise a slot in the agents table for the new agent */

    i = GetNewAgentIndex();
    newAgent = &agent[i];
    newAgent->ipcType = AGENT_PIPE;
    newAgent->pmDomainId = pmDomainId;
    newAgent->pduProtocol = pduProtocol;
    newAgent->inFd = -1;
    newAgent->outFd = -1;
    newAgent->profClient = NULL;
    newAgent->pmDomainLabel = pmDomainLabel;
    newAgent->status.connected = 0;
    newAgent->status.busy = 0;
    newAgent->status.isChild = 0;	/* set to one when/if child created */
    newAgent->status.notReady = 0;
    newAgent->reason = 0;
    newAgent->ipc.pipe.argv = BuildArgv();

    if (newAgent->ipc.pipe.argv == NULL) {
	fprintf(stderr, "pmcd: line %d, error building argv for \"%s\" agent.\n",
		     nLines, newAgent->pmDomainLabel);
	return -1;
    }
    newAgent->ipc.pipe.commandLine = BuildCmdLine(newAgent->ipc.pipe.argv);

#ifdef MALLOC_AUDIT
    _persistent_(pmDomainLabel);
    if (commandLine != NULL) {
	char	**argv = newAgent->ipc.pipe.argv;

	_persistent_(commandLine);
	for (i = 0; argv[i] != NULL; i++)
	    _persistent_(argv[i]);
    }
#endif

    return 0;
}

static int
ParseAccessSpec(int allow, int *specOps, int *denyOps, int *maxCons, int recursion)
{
    int		op;			/* >0 for specific ops, 0 otherwise */
    int		haveOps = 0, haveAll = 0;
    int		haveComma = 0;

    if (*token == ';') {
	fprintf(stderr,
		     "pmcd: line %d, Empty or incomplete permissions list\n",
		     nLines);
	return -1;
    }

    if (!recursion)			/* Set maxCons to unspecified 1st time */
	*maxCons = 0;
    while (*token && *token != ';') {
	op = 0;
	if (TokenIs("fetch"))
	    op = OP_FETCH;
	else if (TokenIs("store"))
	    op = OP_STORE;
	else if (TokenIs("all")) {
	    if (haveOps) {
		fprintf(stderr,
			     "pmcd: line %d, Can't have \"all\" mixed with specific permissions\n",
			     nLines);
		return -1;
	    }
	    haveAll = 1;
	    if (recursion) {
		fprintf(stderr,
			     "pmcd: line %d, Can't have \"all\" within an \"all except\"\n",
			     nLines);
		return -1;
	    }
	    FindNextToken();

	    /* Any "all" statement specifies permissions for all operations
	    * Start off with all operations in allow/disallowed state
	    */
	    *denyOps = allow ? OP_NONE : OP_ALL;

	    if (TokenIs("except")) {
		/* Now deal with exceptions by reversing the "allow" sense */
		int sts;

		FindNextToken();
		sts = ParseAccessSpec(!allow, specOps, denyOps, maxCons, 1);
		if (sts < 0) return -1;
	    }
	    *specOps = OP_ALL;		/* Do this AFTER any recursive call */
	}
	else if (TokenIs("maximum") || TokenIs("unlimited")) {
	    int	unlimited = *token == 'u' || *token == 'U';

	    if (*maxCons) {
		fprintf(stderr,
			     "pmcd: line %d, Connection limit already specified\n",
			     nLines);
		return -1;
	    }
	    if (recursion && !haveOps) {
		fprintf(stderr,
			     "pmcd: line %d, Connection limit may not immediately follow\"all except\"\n",
			     nLines);
		return -1;
	    }

	    /* "maximum N connections" or "unlimited connections" is not
	     * allowed in a disallow statement.  This is a bit tricky, because
	     * of the recursion in "all except", which flips "allow" into
	     * !"allow" and recursion from 0 to 1 for the recursive call to
	     * this function.  The required test is !XOR: "!recursion && allow"
	     * is an "allow" with no "except".  "recursion && !allow" is an
	     * "allow" with an "except" anything else is a "disallow" (i.e. an
	     * error)
	     */
	    if (!(recursion ^ allow)) { /* disallow statement */
		fprintf(stderr,
			     "pmcd: line %d, Can't specify connection limit in a disallow statement\n",
			     nLines);
		return -1;
	    }
	    if (unlimited)
		*maxCons = -1;
	    else {
		FindNextToken();
		if (!TokenIsNumber() || TokenNumVal() <= 0) {
		    fprintf(stderr,
				 "pmcd: line %d, Maximum connection limit must be a positive number\n",
				 nLines);
		    return -1;
		}
		*maxCons = TokenNumVal();
		FindNextToken();
	    }
	    if (!TokenIs("connections")) {
		fprintf(stderr,
			     "pmcd: line %d, \"connections\" expected\n",
			     nLines);
		return -1;
	    }
	    FindNextToken();
	}
	else {
	    fprintf(stderr, "pmcd: line %d, bad access specifier\n",
			 nLines);
	    return -1;
	}

	/* If there was a specific operation mentioned, (dis)allow it */
	if (op) {
	    if (haveAll) {
		fprintf(stderr,
			     "pmcd: line %d, Can't have \"all\" mixed with specific permissions\n",
			     nLines);
		return -1;
	    }
	    haveOps = 1;
	    *specOps |= op;
	    if (allow)
		*denyOps &= (~op);
	    else
		*denyOps |= op;
	    FindNextToken();
	}
	if (*token != ',' && *token != ';') {
	    fprintf(stderr,
			 "pmcd: line %d, ',' or ';' expected in permission list\n",
			 nLines);
	    return -1;
	}
	if (*token == ',') {
	    haveComma = 1;
	    FindNextToken();
	}
	else
	    haveComma = 0;
    }
    if (haveComma) {
	fprintf(stderr,
		     "pmcd: line %d, Misplaced (trailing) ',' in permission list\n",
		     nLines);
	return -1;
    }
    return 0;
}

static int
ParseHosts(int allow)
{
    int		sts = 0;
    int		i = 0;
    int		j;
    int		ok = 0;
    int		another = 1;
    static char	**name = NULL;
    static int	szNames = 0;
    int		specOps = 0;
    int		denyOps = 0;
    int		maxCons = 0;		/* Zero=>unspecified, -1=>unlimited */

    while (*token && another && *token != ':' && *token != ';') {
	if (i == szNames) {
	    int		need;

	    szNames += 8;
	    need = szNames * (int)sizeof(char**);
	    if ((name = (char **)realloc(name, need)) == NULL)
		__pmNoMem("pmcd ParseHosts name list", need, PM_FATAL_ERR);
	}
	if ((name[i] = CopyToken()) == NULL)
	    __pmNoMem("pmcd ParseHosts name", tokenend - token, PM_FATAL_ERR);
	FindNextToken();
	if (*token != ',' && *token != ':') {
	    fprintf(stderr,
			 "pmcd: line %d, ',' or ':' expected after \"%s\"\n",
			 nLines, name[i]);
	    goto error;
	}
	if (*token == ',') {
	    FindNextToken();
	    another = 1;
	}
	else
	    another = 0;
	i++;
    }
    if (i == 0) {
	fprintf(stderr,
		     "pmcd: line %d, No hosts in allow/disallow statement\n",
		     nLines);
	goto error;
    }
    if (another) {
	fprintf(stderr, "pmcd: line %d, host expected after ','\n",
		     nLines);
	goto error;
    }
    if (*token != ':') {
	fprintf(stderr, "pmcd: line %d, ':' expected after \"%s\"\n",
		nLines, name[i]);
	goto error;
    }
    FindNextToken();
    if (ParseAccessSpec(allow, &specOps, &denyOps, &maxCons, 0) < 0)
	goto error;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	for (j = 0; j < i; j++)
	    fprintf(stderr, "ACCESS: %s specOps=%02x denyOps=%02x maxCons=%d\n",
		    name[j], specOps, denyOps, maxCons);
    }
#endif
    
    /* Make new entries for hosts in host access list */

    for (j = 0; j < i; j++) {
	if ((sts = __pmAccAddHost(name[j], specOps, denyOps, maxCons)) < 0) {
	    if (sts == -EHOSTUNREACH || -EHOSTDOWN)
		fprintf(stderr, "Warning: the following access control specification will be ignored\n");
	    fprintf(stderr,
			 "pmcd: line %d, access control error for host '%s': %s\n",
			 nLines, name[j], pmErrStr(sts));
	    if (sts == -EHOSTUNREACH || -EHOSTDOWN)
		;
	    else
		goto error;
	}
	else
	    ok = 1;
    }
    return ok;

error:
    for (j = 0; j < i; j++)
	free(name[j]);
    return -1;
}

static int
ParseAccessControls(void)
{
    int		sts = 0;
    int		tmp;
    int		allow;
    int		nHosts = 0;

    doingAccess = 1;
    /* This gets a little tricky, because the token may be "[access]", or
     * "[access" or "[".  "[" and "]" can't be made special characters until
     * the scanner knows it is in the access control section because the arg
     * lists for agents may contain them.
     */
    if (TokenIs("[access]"))
	FindNextToken();
    else {
	if (TokenIs("[")) {
	    FindNextToken();
	    if (!TokenIs("access")) {
		fprintf(stderr,
			     "pmcd: line %d, \"access\" keyword expected\n",
			     nLines);
		return -1;
	    }
	}
	else if (!TokenIs("[access")) {
	    fprintf(stderr,
			 "pmcd: line %d, \"access\" keyword expected\n",
			 nLines);
	    return -1;
	}
	FindNextToken();
	if (*token != ']') {
	    fprintf(stderr, "pmcd: line %d, ']' expected\n", nLines);
	    return -1;
	}
	FindNextToken();
    }
    while (*token && !scanError) {
	if (TokenIs("allow"))
	    allow = 1;
	else if (TokenIs("disallow"))
	    allow = 0;
	else {
	    fprintf(stderr,
			 "pmcd: line %d, allow or disallow statement expected\n",
			 nLines);
	    sts = -1;
	    while (*token && !scanError && *token != ';')
		FindNextToken();
	    if (*token && !scanError && *token == ';') {
		FindNextToken();
		continue;
	    }
	    return -1;
	}
	FindNextToken();
	if ((tmp = ParseHosts(allow)) < 0)
	    sts = -1;
	else if (tmp > 0)
	    nHosts++;
	while (*token && !scanError && *token != ';')
	    FindNextToken();
	if (!*token || scanError)
	    return -1;
	FindNextToken();
    }
    if (sts != 0)
	return sts;

    if (nHosts == 0) {
	fprintf(stderr,
		     "pmcd: line %d, No valid statements in [access] section\n",
		     nLines);
	return -1;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	__pmAccDumpHosts(stderr);
#endif

    return 0;
}

/* Parse the configuration file, creating the agent list. */
static int
ReadConfigFile(FILE *configFile)
{
    char	*pmDomainLabel;
    int		i, pmDomainId;
    int		s, sts = 0;

    inputStream = configFile;
    scanInit = 0;
    scanError = 0;
    doingAccess = 0;
    nLines = 0;
    FindNextToken();
    while (*token && !scanError) {
	if (*token == '\n')		/* It's a comment or blank line */
	    goto doneLine;

	if (*token == '[')		/* Start of access control specs */
	    break;

	if ((pmDomainLabel = CopyToken()) == NULL)
	    __pmNoMem("pmcd config: domain label", tokenend - token + 1, PM_FATAL_ERR);

	FindNextToken();
	if (TokenIsNumber()) {
	    pmDomainId = TokenNumVal();
	    FindNextToken();
	}
	else {
	    fprintf(stderr,
			 "pmcd: line %d, Expected domain number for \"%s\" agent\n",
			 nLines, pmDomainLabel);
	    sts = -1;
	    goto doneLine;
	}
	if (pmDomainId < 0 || pmDomainId > MAXDOMID) {
	    fprintf(stderr,
			 "pmcd: line %d, Illegal domain number (%d) for \"%s\" agent\n",
			 nLines, pmDomainId, pmDomainLabel);
	    sts = -1;
	    goto doneLine;
	}
	/* Can't use mapdom because it isn't built yet.  Can't build it during
	 * parsing because this might be a restart parse that fails, requiring
	 * a revert to the old mapdom.
	 */
	for (i = 0; i < nAgents; i++)
	    if (pmDomainId == agent[i].pmDomainId) {
		fprintf(stderr,
			     "pmcd: line %d, PM domain identifier for \"%s\" agent clashes with \"%s\" agent\n",
			     nLines, pmDomainLabel, agent[i].pmDomainLabel);
		sts = -1;
		goto doneLine;
	    }

	s = 0;
	if (TokenIs("dso"))
	    s = ParseDso(pmDomainLabel, pmDomainId);
	else if (TokenIs("socket"))
	    s = ParseSocket(pmDomainLabel, pmDomainId);
	else if (TokenIs("pipe"))
	    s = ParsePipe(pmDomainLabel, pmDomainId);
	else {
	    fprintf(stderr,
			 "pmcd: line %d, Expected `dso', `socket' or `pipe'.  Line ignored\n",
			 nLines);
	    sts = -1;
	}
	if (s < 0) {
	    FreeAgent(&agent[nAgents-1]);
	    nAgents--;
	    sts = -1;
	}
doneLine:
	SkipLine();
    }
    if (scanError) {
	fprintf(stderr, "pmcd config: Can't continue, giving up\n");
	sts = -1;
    }
    if (*token == '[' && sts != -1)
	if (ParseAccessControls() < 0)
	    sts = -1;
    return sts;
}

static int
DoAgentCreds(AgentInfo* aPtr, __pmPDU *pb)
{
    int			i;
    int			sts = 0;
    int			credcount = 0;
    int			sender = 0;
    int			vflag = 0;
    __pmCred		*credlist = NULL;
    __pmIPC		ipc = { UNKNOWN_VERSION, NULL };

    if ((sts = __pmDecodeCreds(pb, PDU_BINARY, &sender, &credcount, &credlist)) < 0)
	return sts;
    else if (_pmcd_trace_mask)
	pmcd_trace(TR_RECV_PDU, aPtr->outFd, sts, (int)((__psint_t)pb & 0xffffffff));

    for (i = 0; i < credcount; i++) {
	switch (credlist[i].c_type) {
	case CVERSION:
	    aPtr->pduVersion = credlist[i].c_vala;
	    ipc.version = credlist[i].c_vala;
	    vflag = 1;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "pmcd: version cred (%u)\n", aPtr->pduVersion);
#endif
	    break;
	}
    }

    if (((sts = __pmAddIPC(aPtr->inFd, ipc)) < 0) ||
	((sts = __pmAddIPC(aPtr->outFd, ipc)) < 0))
	return sts;

    if (vflag) {	/* complete the version exchange - respond to agent */
        __pmCred	handshake[1];

	handshake[0].c_type = CVERSION;
	handshake[0].c_vala = PDU_VERSION;
	handshake[0].c_valb = 0;
	handshake[0].c_valc = 0;
	if ((sts = __pmSendCreds(aPtr->inFd, PDU_BINARY, 1, handshake)) < 0)
	    return sts;
	else if (_pmcd_trace_mask)
	    pmcd_trace(TR_XMIT_PDU, aPtr->inFd, PDU_CREDS, credcount);
    }

    return 0;
}

/* version exchange - get a credentials PDU from 2.0 agents */
static int
AgentNegotiate(AgentInfo *aPtr)
{
    int		sts;
    __pmPDU	*ack;
    __pmIPC	ipc = { UNKNOWN_VERSION, NULL };

    if (aPtr->pduProtocol == PDU_BINARY) {
	sts = __pmGetPDU(aPtr->outFd, PDU_BINARY, _creds_timeout, &ack);
	if (sts == PDU_CREDS) {
	    if ((sts = DoAgentCreds(aPtr, ack)) < 0) {
		fprintf(stderr, "pmcd: version exchange failed "
		    "for \"%s\" agent: %s\n", aPtr->pmDomainLabel, pmErrStr(sts));
		return sts;
	    }
	    return 0;
	}
	else if (sts == PM_ERR_TIMEOUT) {
	    fprintf(stderr, "pmcd: no version exchange with PMDA %s: "
		    "assuming PCP 1.x PMDA.\n", aPtr->pmDomainLabel);
	    /*FALLTHROUGH*/
	}
	else {
	    if (sts > 0)
		fprintf(stderr, "pmcd: unexpected PDU type (0x%x) at initial "
			"exchange with %s PMDA\n", sts, aPtr->pmDomainLabel);
	    else if (sts == 0)
		fprintf(stderr, "pmcd: unexpected end-of-file at initial "
			"exchange with %s PMDA\n", aPtr->pmDomainLabel);
	    else
		fprintf(stderr, "pmcd: error at initial PDU exchange with "
			"%s PMDA: %s\n", aPtr->pmDomainLabel, pmErrStr(sts));
	    return PM_ERR_IPC;
	}
    }

    /*
     * Either (PDU_ASCII _is_ version 1 ONLY), or timed out in PDU exchange
     */
    aPtr->pduVersion = PDU_VERSION1;
    ipc.version = PDU_VERSION1;
    if ((sts = __pmAddIPC(aPtr->inFd, ipc)) >= 0)
	sts = __pmAddIPC(aPtr->outFd, ipc);
    return sts;
}

/* Connect to an agent's socket. */
static int
ConnectSocketAgent(AgentInfo *aPtr)
{
    int		len;
    int		sts = 0;
    int		fd;

    fd = socket(aPtr->ipc.socket.addrDomain, SOCK_STREAM, 0);
    if (fd < 0) {
	fprintf(stderr,
		     "pmcd: Error creating socket for \"%s\" agent : %s\n",
		     aPtr->pmDomainLabel, strerror(errno));
	return -1;
    }

    if (aPtr->ipc.socket.addrDomain == AF_INET) {
	struct sockaddr_in	addr;
	struct hostent		*hostInfo;
	int			i;
	struct linger		noLinger = {1, 0};

	i = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &i,
	    sizeof(i)) < 0) {
	    fprintf(stderr, "ConnectSocketAgent setsockopt(nodelay): %s\n",
			 strerror(errno));
	    goto error;
	}
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &noLinger, sizeof(noLinger)) < 0) {
	    fprintf(stderr, "ConnectSocketAgent setsockopt(nolinger): %s\n",
			 strerror(errno));
	    goto error;
	}
	hostInfo = gethostbyname("localhost");
	if (hostInfo == NULL) {
	    fputs("pmcd: Error getting inet address for localhost\n", stderr);
	    goto error;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	memcpy(&addr.sin_addr, hostInfo->h_addr, hostInfo->h_length);
	addr.sin_port = aPtr->ipc.socket.port;
	sts = connect(fd, (struct sockaddr *) &addr, sizeof(addr));
    }
    else {
	struct sockaddr_un	addr;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, aPtr->ipc.socket.name);
	len = (int)sizeof(addr.sun_family) + (int)strlen(addr.sun_path);
	sts = connect(fd, (struct sockaddr *) &addr, len);
    }
    if (sts < 0) {
	fprintf(stderr, "pmcd: Error connecting to \"%s\" agent : %s\n",
		     aPtr->pmDomainLabel, strerror(errno));
	goto error;
    }
    aPtr->outFd = aPtr->inFd = fd;	/* Sockets are bi-directional */
    if (fd > max_seen_fd)
	max_seen_fd = fd;

    PMCD_OPENFDS_SETHI(fd);

    if ((sts = AgentNegotiate(aPtr)) < 0)
	goto error;

    return 0;

error:
    close(fd);
    return -1;
}

/* Create the specified agent running at the end of a pair of pipes. */
static int
CreateAgent(AgentInfo *aPtr)
{
    int		i;
    int		sts;
    int		inPipe[2];	/* Pipe for input to child */
    int		outPipe[2];	/* For output to child */
    pid_t	childPid;
    char	**argv;

    if (aPtr->ipcType == AGENT_PIPE) {
	argv = aPtr->ipc.pipe.argv;
	if (pipe(inPipe) < 0) {
	    fprintf(stderr,
			 "pmcd: input pipe create failed for \"%s\" agent: %s\n",
			 aPtr->pmDomainLabel, strerror(errno));
	    return -1;
	}

	if (pipe(outPipe) < 0) {
	    fprintf(stderr,
			 "pmcd: output pipe create failed for \"%s\" agent: %s\n",
			 aPtr->pmDomainLabel, strerror(errno));
	    close(inPipe[0]);
	    close(inPipe[1]);
	    return -1;
	}
	if (outPipe[1] > max_seen_fd)
	    max_seen_fd = outPipe[1];
	
	PMCD_OPENFDS_SETHI(outPipe[1]);

    }
    else if (aPtr->ipcType == AGENT_SOCKET)
	argv = aPtr->ipc.socket.argv;

    if (argv != NULL) {			/* Start a new agent if required */
	fflush(stderr);
	fflush(stdout);
	if ((childPid = fork()) == (pid_t) -1) {
	    fprintf(stderr, "pmcd: fork failed for \"%s\" agent: %s\n",
			 aPtr->pmDomainLabel, strerror(errno));
	    if (aPtr->ipcType == AGENT_PIPE) {
		close(inPipe[0]);
		close(inPipe[1]);
		close(outPipe[0]);
		close(outPipe[1]);
	    }
	    return -1;
	}

	if (childPid) {
	    /* This is the parent (PMCD) */
	    if (aPtr->ipcType == AGENT_PIPE) {
		close(inPipe[0]);
		close(outPipe[1]);
		aPtr->inFd = inPipe[1];
		aPtr->outFd = outPipe[0];
	    }
	}
	else {
	    /*
	     * This is the child (new agent)
	     * make sure stderr is fd 2
	     */
	    dup2(fileno(stderr), STDERR_FILENO); 
	    if (aPtr->ipcType == AGENT_PIPE) {
		/* make pipe stdin for PMDA */
		dup2(inPipe[0], STDIN_FILENO);
		/* make pipe stdout for PMDA */
		dup2(outPipe[1], STDOUT_FILENO);
	    }
	    else {
		/*
		 * not a pipe, close stdin and attach stdout to stderr
		 */
		close(STDIN_FILENO);
		dup2(STDERR_FILENO, STDOUT_FILENO);
	    }

	    for (i = 0; i <= max_seen_fd; i++) {
		/* Close all except std{in,out,err} */
		if (i == STDIN_FILENO ||
		    i == STDOUT_FILENO ||
		    i == STDERR_FILENO)
		    continue;
		close(i);
	    }

	    execvp(argv[0], argv);
	    /* botch if reach here */
	    fprintf(stderr, "pmcd: error starting %s: %s\n",
			 argv[0], strerror(errno));
	    /* avoid atexit() processing, so _exit not exit */
	    _exit(1);
	}

	/* Only pmcd (parent) executes this */

	aPtr->status.isChild = 1;
	if (aPtr->ipcType == AGENT_PIPE) {
	    aPtr->ipc.pipe.agentPid = childPid;
	    /* ready for version negotiation */
	    if ((sts = AgentNegotiate(aPtr)) < 0) {
		close(aPtr->inFd);
		close(aPtr->outFd);
		return sts;
	    }
	}
	else if (aPtr->ipcType == AGENT_SOCKET)
	    aPtr->ipc.socket.agentPid = childPid;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "pmcd: started PMDA %s (%d), pid=%d\n",
	        aPtr->pmDomainLabel, aPtr->pmDomainId, childPid);
#endif
    }
    return 0;
}

/* Print a table of all of the agent configuration info on a given stream. */
void
PrintAgentInfo(FILE *stream)
{
    int		i, ver=0;
    __pmIPC	*ipc = NULL;
    AgentInfo	*aPtr;

    fputs("\nactive agent dom   pid  in out ver protocol parameters\n", stream);
    fputs(  "============ === ===== === === === ======== ==========\n", stream);
    for (i = 0; i < nAgents; i++) {
	aPtr = &agent[i];
	if (aPtr->status.connected == 0)
	    continue;
	fprintf(stream, "%-12s", aPtr->pmDomainLabel);

	switch (aPtr->ipcType) {
	case AGENT_DSO:
	    fprintf(stream, " %3d               %3d dso i:%d",
		    aPtr->pmDomainId,
		    aPtr->ipc.dso.dispatch.comm.pmapi_version,
		    aPtr->ipc.dso.dispatch.comm.pmda_interface);
	    fprintf(stream, "  lib=%s entry=%s [0x%p]\n",
	        aPtr->ipc.dso.pathName, aPtr->ipc.dso.entryPoint,
	        aPtr->ipc.dso.initFn);
	    break;

	case AGENT_SOCKET:
	    if (__pmFdLookupIPC(aPtr->inFd, &ipc) < 0)
		ver = UNKNOWN_VERSION;
	    else
		ver = ipc->version;
	    fprintf(stream, " %3d %5d %3d %3d %3d ",
		aPtr->pmDomainId, aPtr->ipc.socket.agentPid, aPtr->inFd, aPtr->outFd, ver);
	    fputs("bin ", stream);
	    fputs("sock ", stream);
	    if (aPtr->ipc.socket.addrDomain == AF_UNIX)
		fprintf(stream, "dom=unix port=%s", aPtr->ipc.socket.name);
	    else if (aPtr->ipc.socket.addrDomain == AF_INET) {
		if (aPtr->ipc.socket.name)
		    fprintf(stream, "dom=inet port=%s (%d)",
		        aPtr->ipc.socket.name, aPtr->ipc.socket.port);
		else
		    fprintf(stream, "dom=inet port=%d", aPtr->ipc.socket.port);
	    }
	    else {
		fputs("dom=???", stream);
	    }
	    if (aPtr->ipc.socket.commandLine) {
		fputs(" cmd=", stream);
		fputs(aPtr->ipc.socket.commandLine, stream);
	    }
	    putc('\n', stream);
	    break;

	case AGENT_PIPE:
	    if (__pmFdLookupIPC(aPtr->inFd, &ipc) < 0)
		ver = UNKNOWN_VERSION;
	    else
		ver = ipc->version;
	    fprintf(stream, " %3d %5d %3d %3d %3d ",
		aPtr->pmDomainId, aPtr->ipc.pipe.agentPid, aPtr->inFd, aPtr->outFd, ver);
	    if (aPtr->pduProtocol == PDU_BINARY)
		fputs("bin ", stream);
	    else
		fputs("txt ", stream);
	    if (aPtr->ipc.pipe.commandLine) {
		fputs("pipe cmd=", stream);
		fputs(aPtr->ipc.pipe.commandLine, stream);
		putc('\n', stream);
	    }
	    break;

	default:
	    fputs("????\n", stream);
	    break;
	}
    }
    fflush(stream);			/* Ensure that it appears now */
}

/* Like a DSO, but the library is already linked into my address space */
static int
GetAgentInline(AgentInfo *aPtr)
{
    DsoInfo		*dso = &aPtr->ipc.dso;

    dso->pathName = strdup("-");
    dso->dlHandle = NULL;

    /* Get a pointer to the DSO's init function and call it to get the agent's
     dispatch table for the DSO. */

#if defined(sgi)
    dso->initFn = (void (*)(pmdaInterface*))_rld_new_interface(_RLD_NAME_TO_ADDR, dso->entryPoint);

    if (dso->initFn == NULL) {
	fprintf(stderr, "Couldn't find init function `%s' for DSO agent %s in pmcd executable\n",
		     dso->entryPoint, aPtr->pmDomainLabel);
	aPtr->status.connected = 0;
	aPtr->reason = REASON_NOSTART;
	return -1;
    }
#else
/* TODO work out how to map a name to an address in the current process? */
#endif
	
    return 0;
}

/* Load the DSO for a specified agent and initialise it. */
static int
GetAgentDso(AgentInfo *aPtr)
{
    DsoInfo		*dso = &aPtr->ipc.dso;
    const char		*name;
    unsigned int	challenge;

    aPtr->status.connected = 0;
    aPtr->reason = REASON_NOSTART;

    if (strcmp(dso->pathName, "-") == 0) {
	int	sts;
	sts = GetAgentInline(aPtr);
	if (sts < 0)
	    return sts;
	goto setup;
    }

    name = __pmFindPMDA(dso->pathName);

    if (name == NULL) {
	fprintf(stderr, "Cannot find %s DSO at \"%s\"\n", 
		     aPtr->pmDomainLabel, dso->pathName);
	fputc('\n', stderr);
	return -1;
    }

    if (name != dso->pathName) {
	/* some searching was done */
	free(dso->pathName);
	dso->pathName = strdup(name);
	if (dso->pathName == NULL) {
	    __pmNoMem("pmcd config: pathName", strlen(name), PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	dso->xlatePath = 1;
    }

#if defined(HAVE_DLFCN)
    /*
     * RTLD_NOW would be better in terms of detecting unresolved symbols
     * now, rather than taking a SEGV later ... but various combinations
     * of dynamic and static libraries used to create the DSO PMDA,
     * combined with hiding symbols in the DSO PMDA may result in benign
     * unresolved symbols remaining and the dlopen() would fail under
     * these circumstances.
     */
    dso->dlHandle = dlopen(dso->pathName, RTLD_LAZY);
#elif defined(HAVE_SHL)
    dso->dlHandle = shl_load(dso->pathName, BIND_IMMEDIATE | DYNAMIC_PATH | BIND_VERBOSE, 0);
#else
bozo
#endif
    if (dso->dlHandle == NULL) {
	fprintf(stderr, "Error attaching %s DSO at \"%s\"\n",
		     aPtr->pmDomainLabel, dso->pathName);
#if defined(HAVE_DLFCN)
	fprintf(stderr, "%s\n\n", dlerror());
#else
	fprintf(stderr, "%s\n\n", strerror(errno));
#endif
	return -1;
    }

    /* Get a pointer to the DSO's init function and call it to get the agent's
     dispatch table for the DSO. */

#if defined(HAVE_DLFCN)
    dso->initFn = (void (*)(pmdaInterface*)) dlsym(dso->dlHandle, dso->entryPoint);
    if (dso->initFn == NULL) {
	fprintf(stderr, "Couldn't find init function `%s' in %s DSO\n",
		     dso->entryPoint, aPtr->pmDomainLabel);
	dlclose(dso->dlHandle);
	return -1;
    }
#elif defined(HAVE_SHL)
    if (shl_findsym((shl_t *)&dso->dlHandle, dso->entryPoint, TYPE_PROCEDURE, &dso->initFn) != 0) {
	fprintf(stderr, "Couldn't find init function `%s' in %s DSO\n",
		     dso->entryPoint, aPtr->pmDomainLabel);
	shl_unload((shl_t) dso->dlHandle);
	return -1;
    }
#else
!bozo!
#endif

setup:
    /*
     * Pass in the expected domain id.
     * The PMDA initialization routine can (a) ignore it, (b) check it
     * is the expected value, or (c) self-adapt.
     */
    dso->dispatch.domain = aPtr->pmDomainId;

    /*
     * the PMDA interface / PMAPI version discovery as a "challenge" ...
     * for pmda_interface it is all the bits being set,
     * for pmapi_version it is the complement of the one you are using now
     */
    challenge = 0xff;
    dso->dispatch.comm.pmda_interface = challenge;
    dso->dispatch.comm.pmapi_version = ~PMAPI_VERSION;

    dso->dispatch.comm.flags = 0;
    dso->dispatch.status = 0;

    (*dso->initFn)(&dso->dispatch);

    if (dso->dispatch.status != 0) {
	/* initialization failed for some reason */
	fprintf(stderr,
		     "Initialization routine %s in %s DSO failed: %s\n", 
		     dso->entryPoint, aPtr->pmDomainLabel,
		     pmErrStr(dso->dispatch.status));
	    dlclose(dso->dlHandle);
	    return -1;
    }

    if (dso->dispatch.comm.pmda_interface == challenge) {
	/*
	 * DSO did not change pmda_interface, assume PMAPI version 1
	 * from PCP 1.x and PMDA_INTERFACE_1
	 */
	dso->dispatch.comm.pmda_interface = PMDA_INTERFACE_1;
	dso->dispatch.comm.pmapi_version = PMAPI_VERSION_1;
    }
    else {
	/*
	 * gets a bit tricky ...
	 * interface_version (8-bits) used to be version (4-bits),
	 * so it is possible that only the bottom 4 bits were
	 * changed and in this case the PMAPI version is 1 for
	 * PCP 1.x
	 */
	if ((dso->dispatch.comm.pmda_interface & 0xf0) == (challenge & 0xf0)) {
	    dso->dispatch.comm.pmda_interface &= 0x0f;
	    dso->dispatch.comm.pmapi_version = PMAPI_VERSION_1;
	}
    }

    if (dso->dispatch.comm.pmda_interface < PMDA_INTERFACE_1 ||
	dso->dispatch.comm.pmda_interface > PMDA_INTERFACE_LATEST) {
	__pmNotifyErr(LOG_ERR,
		 "Unknown PMDA interface version (%d) used by DSO %s\n",
		 dso->dispatch.comm.pmda_interface, aPtr->pmDomainLabel);
	dlclose(dso->dlHandle);
	return -1;
    }

    if (dso->dispatch.comm.pmapi_version == PMAPI_VERSION_1)
	aPtr->pduVersion = PDU_VERSION1;
    else if (dso->dispatch.comm.pmapi_version == PMAPI_VERSION_2)
	aPtr->pduVersion = PDU_VERSION2;
    else {
	__pmNotifyErr(LOG_ERR,
		 "Unsupported PMAPI version (%d) used by DSO %s\n",
		 dso->dispatch.comm.pmapi_version, aPtr->pmDomainLabel);
	dlclose(dso->dlHandle);
	return -1;
    }

    aPtr->status.connected = 1;
    aPtr->reason = 0;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "pmcd: started DSO PMDA %s (%d) using pmPMDA version=%d, "
		"PDU version=%d\n", aPtr->pmDomainLabel, aPtr->pmDomainId,
		dso->dispatch.comm.pmda_interface, aPtr->pduVersion);
#endif

    return 0;
}


/* For creating and establishing contact with agents of the PMCD. */
static void
ContactAgents(void)
{
    int		i, sts;
    int		createdSocketAgents = 0;
    AgentInfo	*aPtr;

    for (i = 0; i < nAgents; i++) {
	aPtr = &agent[i];
	if (aPtr->status.connected)
	    continue;
	switch (aPtr->ipcType) {
	case AGENT_DSO:
	    sts = GetAgentDso(aPtr);
	    break;

	case AGENT_SOCKET:
	    if (aPtr->ipc.socket.argv) { /* Create agent if required */
		sts = CreateAgent(aPtr);
		if (sts >= 0)
		    createdSocketAgents = 1;

		/* Don't attempt to connect yet, if the agent has just been
		       created, it will need time to initialise socket. */
	    }
	    else
		sts = ConnectSocketAgent(aPtr);
	    break;			/* Connect to existing agent */

	case AGENT_PIPE:
	    sts = CreateAgent(aPtr);
	    break;
	}
	aPtr->status.connected = sts == 0;
	if (aPtr->status.connected) {
	    if (aPtr->ipcType == AGENT_DSO)
		pmcd_trace(TR_ADD_AGENT, aPtr->pmDomainId, -1, -1);
	    else
		pmcd_trace(TR_ADD_AGENT, aPtr->pmDomainId, aPtr->inFd, aPtr->outFd);
	    MarkStateChanges(PMCD_ADD_AGENT);
	}
	else
	    aPtr->reason = REASON_NOSTART;
    }

    /* Allow newly created socket agents time to initialise before attempting
       to connect to them. */

    if (createdSocketAgents) {
	sleep(2);			/* Allow 2 second for startup */
	for (i = 0; i < nAgents; i++) {
	    aPtr = &agent[i];
	    if (aPtr->ipcType == AGENT_SOCKET &&
	        aPtr->ipc.socket.agentPid != (pid_t) -1) {
		sts = ConnectSocketAgent(aPtr);
		aPtr->status.connected = sts == 0;
		if (!aPtr->status.connected)
		    aPtr->reason = REASON_NOSTART;
	    }
	}
    }
}

int
ParseInitAgents(char *fileName)
{
    int		sts;
    int		i;
    FILE	*configFile;
    struct stat	statBuf;
    static int	firstTime = 1;

    memset(&configFileTime, 0, sizeof(configFileTime));
    configFile = fopen(fileName, "r");
    if (configFile == NULL)
	fprintf(stderr,
		     "ParseInitAgents: %s: %s\n", fileName,
		     strerror(errno));
    else if (stat(fileName, &statBuf) == -1)
	fprintf(stderr, "ParseInitAgents: stat(%s): %s\n",
		     fileName, strerror(errno));
    else {
#if defined(HAVE_ST_MTIME_WITH_E)
	configFileTime = statBuf.st_mtime;
#else
	configFileTime = statBuf.st_mtim; /* possible struct assignment */
#endif
    }
    if (configFile == NULL)
	return -1;

    if (firstTime)
	if (__pmAccAddOp(OP_FETCH) < 0 || __pmAccAddOp(OP_STORE) < 0) {
	    fprintf(stderr,
			 "ParseInitAgents: __pmAccAddOp: can't create access ops\n");
	    exit(1);
	}

    sts = ReadConfigFile(configFile);
    fclose(configFile);

    /* If pmcd is restarting, don't create/contact the agents until the results
     * of the parse can be compared with the previous setup to determine
     * whether anything has changed.
     */
    if (!firstTime)
	return sts;

    firstTime = 0;
    if (sts == 0) {
	ContactAgents();
	for (i = 0; i < MAXDOMID + 2; i++)
	    mapdom[i] = nAgents;
	for (i = 0; i < nAgents; i++)
	    if (agent[i].status.connected)
		mapdom[agent[i].pmDomainId] = i;
    }
    return sts;
}

static int
AgentsDiffer(AgentInfo *a1, AgentInfo *a2)
{
    int	i;

    if (a1->pmDomainId != a2->pmDomainId)
	return 1;
    if (a1->ipcType != a2->ipcType)
	return 1;
    if (a1->pduProtocol != a2->pduProtocol)
	return 1;
    if (a1->ipcType == AGENT_DSO) {
	DsoInfo	*dso1 = &a1->ipc.dso;
	DsoInfo	*dso2 = &a2->ipc.dso;
	char	*p1;
	char	*p2;
	int	pfxlen = (int)strlen(dsoPrefix);

	if (dso1 == NULL || dso2 == NULL)
		return 1;	/* should never happen */
	if (dso1->pathName == NULL || dso2->pathName == NULL)
		return 1;	/* should never happen */
	p1 = dso1->pathName;
	if (dso1->xlatePath) {
	    p1 = basename(p1);
	    if (strncmp(p1, dsoPrefix, pfxlen) == 0)
		p1 += pfxlen;
	}
	p2 = dso2->pathName;
	if (dso2->xlatePath) {
	    p2 = basename(p2);
	    if (strncmp(p2, dsoPrefix, pfxlen) == 0)
		p2 += pfxlen;
	}
	if (strcmp(p1, p2))
	    return 1;
	if (dso1->entryPoint == NULL || dso2->entryPoint == NULL)
		return 1;	/* should never happen */
	if (strcmp(dso1->entryPoint, dso2->entryPoint))
	    return 1;
    }

    else if (a1->ipcType == AGENT_SOCKET) {
	SocketInfo	*sock1 = &a1->ipc.socket;
	SocketInfo	*sock2 = &a2->ipc.socket;

	if (sock1 == NULL || sock2 == NULL)
		return 1;	/* should never happen */
	if (sock1->addrDomain != sock2->addrDomain)
	    return 1;
	/* The names don't really matter, it's the port that counts */
	if (sock1->port != sock2->port)
	    return 1;
	if ((sock1->commandLine == NULL && sock2->commandLine != NULL) ||
	    (sock1->commandLine != NULL && sock2->commandLine == NULL))
	    return 1;
	if (sock1->argv != NULL && sock2->argv != NULL) {
	    /* Don't just compare commandLines, changes may be cosmetic */
	    for (i = 0; sock1->argv[i] != NULL && sock2->argv[i] != NULL; i++)
		if (strcmp(sock1->argv[i], sock2->argv[i]))
		return 1;
	    if (sock1->argv[i] != NULL || sock2->argv[i] != NULL)
		return 1;
	}
	else if ((sock1->argv == NULL && sock2->argv != NULL) ||
		 (sock1->argv != NULL && sock2->argv == NULL))
		    return 1;
    }

    else {
	PipeInfo	*pipe1 = &a1->ipc.pipe;
	PipeInfo	*pipe2 = &a2->ipc.pipe;

	if (pipe1 == NULL || pipe2 == NULL)
		return 1;	/* should never happen */
	if ((pipe1->commandLine == NULL && pipe2->commandLine != NULL) ||
	    (pipe1->commandLine != NULL && pipe2->commandLine == NULL))
	    return 1;
	if (pipe1->argv != NULL && pipe2->argv != NULL) {
	    /* Don't just compare commandLines, changes may be cosmetic */
	    for (i = 0; pipe1->argv[i] != NULL && pipe2->argv[i] != NULL; i++)
		if (strcmp(pipe1->argv[i], pipe2->argv[i]))
		    return 1;
	    if (pipe1->argv[i] != NULL || pipe2->argv[i] != NULL)
		return 1;
	}
	else if ((pipe1->argv == NULL && pipe2->argv != NULL) ||
		 (pipe1->argv != NULL && pipe2->argv == NULL))
		    return 1;
    }
    return 0;
}

/* Make the "dest" agent the equivalent of an existing "src" agent.
 * This assumes that the agents are identical according to AgentsDiffer(), and
 * that they have distinct copies of the fields compared therein.
 * Note that only the the low level PDU I/O information is copied here.
 */
static void
DupAgent(AgentInfo *dest, AgentInfo *src)
{
    dest->inFd = src->inFd;
    dest->outFd = src->outFd;
    dest->profClient = src->profClient;
    dest->profIndex = src->profIndex;
    /* IMPORTANT: copy the status, connections stay connected */
    memcpy(&dest->status, &src->status, sizeof(dest->status));
    if (src->ipcType == AGENT_DSO) {
	dest->ipc.dso.dlHandle = src->ipc.dso.dlHandle;
	memcpy(&dest->ipc.dso.dispatch, &src->ipc.dso.dispatch,
	       sizeof(dest->ipc.dso.dispatch));
	/* initFn should never be needed */
	dest->ipc.dso.initFn = (DsoInitPtr)0;
    }
    else if (src->ipcType == AGENT_SOCKET)
	dest->ipc.socket.agentPid = src->ipc.socket.agentPid;
    else
	dest->ipc.pipe.agentPid = src->ipc.pipe.agentPid;
}

static
void harvest(void)
{
    int		i;
    int		sts;
    pid_t	pid;
    AgentInfo	*ap;

    /*
     * Check for child process termination.  Be careful, and ignore any
     * non-agent processes found.
     */
    do {
#if defined(HAVE_WAIT3)
	pid = wait3(&sts, WNOHANG, NULL);
#elif defined(HAVE_WAITPID)
	pid = waitpid((pid_t)-1, &sts, WNOHANG);
#else
	break;
#endif
	if (pid > (pid_t)0) {
	    for ( i = 0; i < nAgents; i++) {
		ap = &agent[i];
		if (!ap->status.connected)
		    continue;
		if (ap->ipcType == AGENT_SOCKET) {
		    if (pid == ap->ipc.socket.agentPid)
			break;
		}
		else
		    if (pid == ap->ipc.pipe.agentPid)
			break;
	    }
	    if (i < nAgents)		/* pid is for terminated agent[i] */
		CleanupAgent(ap, AT_EXIT, sts);
	}
    } while (pid > 0);
}

void
ParseRestartAgents(char *fileName)
{
    int		sts;
    int		i, j;
    struct stat	statBuf;
    AgentInfo	*oldAgent;
    int		oldNAgents;
    AgentInfo	*ap;
    fd_set	fds;

    /* Clean up any deceased agents.  We haven't seen an agent's death unless
     * a PDU transfer involving the agent has occurred.  This cleans up others
     * as well.
     */
    FD_ZERO(&fds);
    j = -1;
    for (i = 0; i < nAgents; i++) {
	ap = &agent[i];
	if (ap->status.connected &&
	    (ap->ipcType == AGENT_SOCKET || ap->ipcType == AGENT_PIPE)) {

	    FD_SET(ap->outFd, &fds);
	    if (ap->outFd > j)
		j = ap->outFd;
	}
    }
    if (++j) {
	/* any agent with output ready has either closed the file descriptor or
	 * sent an unsolicited PDU.  Clean up the agent in either case.
	 */
	struct timeval	timeout = {0, 0};

	sts = select(j, &fds, NULL, NULL, &timeout);
	if (sts > 0) {
	    for (i = 0; i < nAgents; i++) {
		ap = &agent[i];
		if (ap->status.connected &&
		    (ap->ipcType == AGENT_SOCKET || ap->ipcType == AGENT_PIPE) &&
		    FD_ISSET(ap->outFd, &fds)) {

		    /* try to discover more ... */
		    __pmPDU	*pb;
		    sts = __pmGetPDU(ap->outFd, ap->pduProtocol, TIMEOUT_NEVER, &pb);
		    if (sts > 0 && _pmcd_trace_mask)
			pmcd_trace(TR_RECV_PDU, ap->outFd, sts, (int)((__psint_t)pb & 0xffffffff));
		    if (sts == 0)
			pmcd_trace(TR_EOF, ap->outFd, -1, -1);
		    else
			pmcd_trace(TR_WRONG_PDU, ap->outFd, -1, sts);
		    

		    CleanupAgent(ap, AT_COMM, ap->outFd);
		}
	    }
	}
	else if (sts < 0)
	    fprintf(stderr, "pmcd: deceased agents select: %s\n",
			 strerror(errno));
    }

    /* gather and deceased children */
    harvest();

    if (stat(fileName, &statBuf) == -1) {
	fprintf(stderr, "ParseRestartAgents: stat(%s): %s\n",
		     fileName, strerror(errno));
	fprintf(stderr, "Configuration left unchanged\n");
	return;
    }


    /* If the config file's modification time hasn't changed, just try to
     * restart any deceased agents
     */
#if defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESTRUC)
    if (statBuf.st_mtim.tv_sec == configFileTime.tv_sec &&
        statBuf.st_mtim.tv_nsec == configFileTime.tv_nsec) {
#elif defined(HAVE_STAT_TIME_T)
    if (statBuf.st_mtime == configFileTime) {
#else
!bozo!
#endif
	fprintf(stderr, "Configuration file '%s' unchanged\n", fileName);
	fprintf(stderr, "Restarting any deceased agents:\n");
	j = 0;
	for (i = 0; i < nAgents; i++)
	    if (!agent[i].status.connected) {
		fprintf(stderr, "    \"%s\" agent\n",
			     agent[i].pmDomainLabel);
		j++;
	    }
	if (j == 0)
	    fprintf(stderr, "    (no agents required restarting)\n");
	else {
	    putc('\n', stderr);
	    ContactAgents();
	    for (i = 0; i < nAgents; i++)
		mapdom[agent[i].pmDomainId] =
		    agent[i].status.connected ? i : nAgents;
	    MarkStateChanges(PMCD_RESTART_AGENT);
	}
	PrintAgentInfo(stderr);
	__pmAccDumpHosts(stderr);
	return;
    }
    
    /* Save the current agent[] and host access tables, Reset the internal
     * state of the config file parser and re-parse the config file.
     */
    oldAgent = agent;
    oldNAgents = nAgents;
    agent = NULL;
    nAgents = 0;
    szAgents = 0;
    scanInit = 0;
    scanError = 0;
    if (__pmAccSaveHosts() < 0) {
	fprintf(stderr, "Error saving access controls\n");
	sts = -2;
    }
    else
	sts = ParseInitAgents(fileName);

    /* If the config file had errors or there were no valid agents in the new
     * config file, ignore it and stick with the old setup.
     */
    if (sts < 0 || nAgents == 0) {
	if (sts == -1)
	    fprintf(stderr,
			 "Configuration file '%s' has errors\n", fileName);
	else
	    fprintf(stderr,
			 "Configuration file '%s' has no valid agents\n",
			 fileName);
	fprintf(stderr, "Configuration left unchanged\n");
	agent = oldAgent;
	nAgents = oldNAgents;
	if (sts != -2 && __pmAccRestoreHosts() < 0) {
	    fprintf(stderr, "Error restoring access controls!\n");
	    exit(1);
	}
	PrintAgentInfo(stderr);
	__pmAccDumpHosts(stderr);
	return;
    }

    /* Reconcile the old and new agent tables, creating or destroying agents
     * as reqired.
     */
    for (j = 0; j < oldNAgents; j++)
	oldAgent[j].status.restartKeep = 0;

    for (i = 0; i < nAgents; i++)
	for (j = 0; j < oldNAgents; j++)
	    if (!AgentsDiffer(&agent[i], &oldAgent[j]) &&
		oldAgent[j].status.connected) {
		DupAgent(&agent[i], &oldAgent[j]);
		oldAgent[j].status.restartKeep = 1;
	    }

    for (j = 0; j < oldNAgents; j++) {
	if (oldAgent[j].status.connected && !oldAgent[j].status.restartKeep)
	    CleanupAgent(&oldAgent[j], AT_CONFIG, 0);
	FreeAgent(&oldAgent[j]);
    }
    free(oldAgent);
    __pmAccFreeSavedHosts();

    /* Allow some time for the old agents to close down.  This allows sockets
     * to be closed at the agent end, etc.
     */
    sleep(1);

    /* Start the new agents */
    ContactAgents();
    for (i = 0; i < MAXDOMID + 2; i++)
	mapdom[i] = nAgents;
    for (i = 0; i < nAgents; i++)
	if (agent[i].status.connected)
	    mapdom[agent[i].pmDomainId] = i;

    /* Now recaluculate the access controls for each client and update the
     * current connection count in the hostList entries matchine the client.
     * If the client is no longer permitted the connection because of a change
     * in permissions or connection limit, the client's connection is closed.
     */
    for (i = 0; i < nClients; i++) {
	ClientInfo	*cp = &client[i];
	int		s;

	if ((s = __pmAccAddClient(&cp->addr.sin_addr, &cp->denyOps)) < 0) {
	    /* ignore errors, the client is being terminated in any case */
	    if (_pmcd_trace_mask)
		pmcd_trace(TR_XMIT_PDU, cp->fd, PDU_ERROR, s);
	    __pmSendError(cp->fd, PDU_BINARY, s);
	    CleanupClient(cp, s);
	}
    }

    PrintAgentInfo(stderr);
    __pmAccDumpHosts(stderr);

    /*
     * gather and deceased children, some may be PMDAs that were
     * terminated by CleanupAgent or killed and had not exited
     * when the previous harvest() was done
     */
    harvest();
}

#if 0
/*
 * dynamically load libpcp_pmda.so
 */ 
int
pmcd_load_libpcp_pmda(void)
{
    int		err = 0;
    void	*handle = NULL;

#if _MIPS_SIM == _MIPS_SIM_ABI32
    char *path = "/usr/lib/libpcp_pmda.so";
#elif _MIPS_SIM == _MIPS_SIM_NABI32
    char *path = "/usr/lib32/libpcp_pmda.so";
#elif _MIPS_SIM == _MIPS_SIM_ABI64
    char *path = "/usr/lib64/libpcp_pmda.so";
#else
    char *path = "/usr/lib/libpcp_pmda.so"
#endif

    /* we expect not to find this on IRIX when PCP is not installed */
    if (access(path, F_OK) < 0) {
	/*
	 * No messages required; any pmdas wanting libpcp_pmda
	 * will generate a sufficient quantity of rld errors.
	 */
	err = errno;
    }
    else {
#if defined(HAVE_DLFCN)
	handle = dlopen(path, RTLD_NOW);
#elif defined(HAVE_SHL)
	handle = shl_load(path, BIND_IMMEDIATE | DYNAMIC_PATH | BIND_VERBOSE, 0);
#endif
	if (handle == NULL) {
	    /* this is more serious */
	    char *mess = dlerror();
	    __pmNotifyErr(LOG_ERR, "failed to dynamically load dso \"%s\" : %s\n",
		path, mess != NULL ? mess : "unknown error");
	    err = -1;
	}
	else {
	    __pmNotifyErr(LOG_INFO, "pmcd dynamically loaded dso \"%s\"\n", path);
	}
    }

    return err;
}

#endif
