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

#ident "$Id: timecontrol.c,v 2.32 1999/05/13 00:36:34 kenmcd Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#if defined(sgi)
#include <bstring.h>
#endif
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/tcp.h>

#include "pmapi.h"
#include "impl.h"

extern int errno;

/*
 * time controller state
 */
static __pmTimeState *timestate = NULL;

/*
 * pmapi client side state
 */
static struct {
    int		connected;
    pid_t	pidServer; /* pid of pmtime, if connected */
    int		toServer;
    int		fromServer;
} client = {0, 0, -1, -1};

static int GetInitParams(int, pmTime *);
static int SendTctl(int, int, int, void *);
static int RecvTctl(int, int *, int *, void **);

/*
 * Local timestate maintenance helpers
 * Need helpers to:
 *	keep track of clients using extended interval base
 *	toggle use of extended interval base as clients connect
 *	toggle use of extended interval base as clients disconnect
 */
static int
countXTBClients(void)
{
    int	client_ix;
    int	rval = 0;

    if (timestate == NULL)
	return 0;

    for(client_ix = 0; client_ix < timestate->numclients; client_ix++) {
	if (timestate->clients[client_ix].data.vcrmode & PM_XTB_FLAG)
	    rval++;
    }

    return rval;
}

static void 
disconnectXTBCLient(void)
{
    /*
     * if all remaining clients use XTB, then set the overall mode to 
     * be XTB (if it isnt already)
     */
    if (timestate->data.vcrmode & PM_XTB_FLAG)
	return;

    /*
     * Only use Milliseconds without Extended Time Base, so make the units
     * milliseconds.
     */
    if (countXTBClients() == timestate->numclients) {
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "disconnectXTBCLient: All remaining clients are using XTB. Switching XTB on with delta of %d Milliseconds.\n", timestate->data.delta);
#endif
	timestate->data.vcrmode = (timestate->data.vcrmode & 0xffff) | PM_XTB_SET(PM_TIME_MSEC);
    }
}

/*
 * Connect to time controller in given mode, returns read fd (or err < 0).
 * If standalone or new master then use given initial state.
 */
int
pmTimeConnect(int mode, const char *port, pmTime *state)
{
    int sts = 0;
    int fds[2];
    pid_t child_pid;
    struct sockaddr_un addr;
    int newclient;
    pmTime ackState;
    char *master;
    int i;
    __pmTimeInitPDU initBuffer;

    if (client.connected) {
	sts = PM_ERR_ISCONN;
    }
    else {
	if ((timestate = (__pmTimeState *)malloc(sizeof(__pmTimeState))) == NULL)
	    return -errno;
	memset(timestate, 0, sizeof(__pmTimeState));

	timestate->mode = mode;
	memcpy(&timestate->data, state, sizeof(pmTime));
	if (port != NULL)
	    timestate->port = strdup(port);
	else
	    timestate->port = NULL;

	if (mode & PM_TCTL_MODE_STANDALONE) {
	    /*
	     * There is no control port for this case.
	     * Instead, there is exactly one client,
	     * and it's send/recv ports are a loopback pipe.
	     * In a sense, the time controller has already
	     * accepted it's client.
	     */

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "pmTimeConnect(STANDALONE)\n");
#endif

	    if ((sts = pipe(fds)) < 0) {
		free(timestate);
		timestate = NULL;
		return -errno;
	    }
	    else {
		timestate->control = -1; /* not used */
		client.fromServer = fds[0];
		client.toServer = fds[1];

		/* return client recv fd */
		sts = client.fromServer;
	    }

	    newclient = __pmTimeAddClient(client.toServer, client.fromServer);
	    if (newclient < 0) {
		free(timestate);
		timestate = NULL;
		return -errno;
	    }

	}
	else {
	    /* 
	     * Establish an IPC channel to a master time controller process.
	     */
	    if (port == NULL) {
		/* a port name must be supplied */
		free(timestate);
		timestate = NULL;
		return PM_ERR_NEEDPORT;
	    }

	    if ((master = getenv("_PM_TIME_CONTROL")) == NULL) {
		/* use default */
		master = PM_TCTL_MASTER_PATH;
	    }

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	    fprintf(stderr, "pmTimeConnect(MASTER): exec(%s -p %s)\n", master, port);
#endif

	    if (mode & PM_TCTL_MODE_NEWMASTER) {
		/*
		 * Fork/exec a new master controller before connecting to it.
		 * Use given port and init state via command line arguments.
		 */
		if ((child_pid = fork()) < 0) {
		    free(timestate);
		    timestate = NULL;
		    return -errno;
		}

		if (child_pid == 0) {
		    /*
		     * This is the child/newmaster process.
		     */

		    char Dbuf[64];
		    sprintf(Dbuf, "%d", pmDebug);
		    sts = execl(master, master, (mode & PM_TCTL_MODE_HOST) ? "-h" : "-a",
			"-p", port, "-D", Dbuf, NULL);

		    /* oops, exec failed, forked process now exits */
		    exit(errno);
		}

		if (client.pidServer != 0) {
		    int child_status;
		    /*
		     * Wait for previous pmtime child process so it doesn't
		     * become a zombie (since we didn't exit when we disconnected
		     * from it). Note: the pmtime process will be still alive if
		     * it still has other clients, in which case this is a no-op.
		     */
		    (void)waitpid(client.pidServer, &child_status, WNOHANG);
		}
		client.pidServer = child_pid;

		/*
		 * The parent continues, but pauses briefly to allow the master to start
		 */
		sginap(20);
	    }

	    /*
	     * Recv channel: connect to an existing master on given port
	     * (this may be the master just exec'ed above)
	     */
	    if ((client.fromServer = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		free(timestate);
		timestate = NULL;
		return -errno;
	    }
	    memset((void *)&addr, 0, sizeof(addr));	/* for purify */
	    strcpy(addr.sun_path, timestate->port);
	    addr.sun_family = AF_UNIX;

	    for (i=0; i < 100; i++) {
		if ((sts = connect(client.fromServer, (struct sockaddr *)&addr,
		    (int)strlen(addr.sun_path) + (int)sizeof(addr.sun_family))) < 0) {
		    /*
		     * retry to avoid race condition, since the new master
		     * may have just started and is not listening yet.
		     */
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL) {
		    struct timeval	n;
		    time_t		now;
		    gettimeofday(&n, NULL);
		    now = (time_t)n.tv_sec;
		    fprintf(stderr, "pmTimeConnect: connect \"%s\" failed on attempt %d [%.19s.%03d]\n",
			timestate->port, i, ctime(&now), (int)(n.tv_usec/1000));
}
#endif
		    /* sleep with back-off before retrying */
		    sginap(i*5);
		}
		else
		    break;
	    }

	    if (sts < 0) {
		free(timestate);
		timestate = NULL;
		return -errno;
	    }

	    /* 
	     * Used to have two channels, but we only need one.
	     * It still looks like two because all the APIs want them
	     * and the APIs cant be changed.
	     */
	    client.toServer = client.fromServer;
	}

	/* don't inherit these when the client forks/execs */
	fcntl(client.fromServer, F_SETFD, FD_CLOEXEC);

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_TIMECONTROL) {
	    fprintf(stderr, "pmTimeConnect: Requesting state:\n\tshowdialog %d\n", state->showdialog);
	    fprintf(stderr, "\tdelta %d", state->delta);
	    switch(PM_XTB_GET(state->vcrmode)) {
		case PM_TIME_NSEC:
		    fprintf(stderr, " Nanoseconds (XTB ON)\n");
		    break;
		case PM_TIME_USEC:
		    fprintf(stderr, " Microseconds (XTB ON)\n");
		    break;
		case PM_TIME_MSEC:
		    fprintf(stderr, " Milliseconds (XTB ON)\n");
		    break;
		case PM_TIME_SEC:
		    fprintf(stderr, " Seconds (XTB ON)\n");
		    break;
		case PM_TIME_MIN:
		    fprintf(stderr, " Minutes (XTB ON)\n");
		    break;
		case PM_TIME_HOUR:
		    fprintf(stderr, " Hours (XTB ON)\n");
		    break;
		default:
		    fprintf(stderr, " Milliseconds (XTB OFF)\n");
		    break;
	    }

	    fprintf(stderr, "\tvcrmode ");
	    switch(state->vcrmode & 0xffff) {
		case PM_TCTL_VCRMODE_STOP:
		    fprintf(stderr, "STOP\n");
		    break;
		case PM_TCTL_VCRMODE_PLAY:
		    fprintf(stderr, "PLAY\n");
		    break;
		case PM_TCTL_VCRMODE_FAST:
		    fprintf(stderr, "FAST\n");
		    break;
		case PM_TCTL_VCRMODE_DRAG:
		    fprintf(stderr, "DRAG\n");
		    break;
		default:
		    fprintf(stderr, "UNKNOWN\n");
		    break;
	    }
	}
#endif
	/*
	 * send the version and initial vcr state parameters
	 */
	initBuffer.version = PM_TCTL_PROTO_VERSION; /* from impl.h */
	memcpy(&initBuffer.params, state, sizeof(pmTime)); /* user supplied */
	sts = SendTctl(client.toServer, PM_TCTL_INITIALIZE, sizeof(initBuffer), &initBuffer);
	if (sts < 0)
	    return sts;
	
	/*
	 * block and wait for an ACK (signalling that initialization is complete)
	 */
	 sts = __pmTimeRecvPDU(client.fromServer, &ackState);
	 if (sts < 0)
	    return sts;
	else
	if (sts != PM_TCTL_ACK)
	    return PM_ERR_IPC;

	/* return the client's recv descriptor */
	sts = client.fromServer;
	client.connected = 1;
    }

    return sts;
}

/*
 *  disconnect from time control master or standalone, free internal state.
 */
int
pmTimeDisconnect(void)
{
    int sts = 0;

    if (client.connected == 0) {
	sts = PM_ERR_NOTCONN;
    }
    else {
	client.connected = 0;
	close(client.toServer);
	client.toServer = -1;
	client.fromServer = -1;
	free(timestate);
    }

    return sts;
}

/*
 * return port name (in static buffer), or NULL if standalone
 */
const char *
pmTimeGetPort(void)
{
    return timestate ? timestate->port : NULL;
}

/*
 * a := a + b for struct timevals
 */
static void
tvadd(struct timeval *a, struct timeval *b)
{
    a->tv_usec += b->tv_usec;
    if (a->tv_usec > 1000000) {
      a->tv_usec -= 1000000;
      a->tv_sec++;
    }
    a->tv_sec += b->tv_sec;
}

/*
 * a := a - b for struct timevals, but result is never less than zero
 */
static void
tvsub(struct timeval *a, struct timeval *b)
{
    if (a->tv_usec < b->tv_usec) {
      a->tv_usec += 1000000;
      a->tv_sec--;
    }
    a->tv_usec -= b->tv_usec;
    a->tv_sec -= b->tv_sec;
}

/*
 * add +-ms milliseconds to struct timeval
 */
static void
tvinc(struct timeval *tv, int ms, int mode)
{
    struct timeval operand;

    if (ms < 0) {
	ms = -ms;
	pmXTBdeltaToTimeval(ms, mode, &operand);
	tvsub(tv, &operand);
    }
    else {
	pmXTBdeltaToTimeval(ms, mode, &operand);
	tvadd(tv, &operand);
    }
}

/*
 * receive a time control command (may block), returns command (or err < 0)
 */
int
pmTimeRecv(pmTime *state)
{
    int sts;

    if (client.connected == 0) {
	sts = PM_ERR_NOTCONN;
    }
    else {
	sts = __pmTimeRecvPDU(client.fromServer, state);
	if (sts == PM_TCTL_STEP || sts == PM_TCTL_SKIP) {
	    /* advance by +- delta */
	    tvinc(&state->position, state->delta, state->vcrmode);
	}
    }
    return sts;
}

/*
 * send ack that PM_TCTL_STEP has been processed
 */
int
pmTimeSendAck(const struct timeval *fetchTime)
{
    pmTime buf;
    int sts = 0;

    if (client.connected == 0) {
	sts = PM_ERR_NOTCONN;
    }
    else {
	memcpy(&buf.position, fetchTime, sizeof(struct timeval));
	sts = __pmTimeSendPDU(client.toServer, PM_TCTL_ACK, &buf);
    }

    return sts;
}

/*
 * send earliest and latest archive bounds
 */
int
pmTimeSendBounds(const struct timeval *start, const struct timeval *finish)
{
    pmTime buf;
    int sts = 0;

    if (client.connected == 0) {
	sts = PM_ERR_NOTCONN;
    }
    else {
	memcpy(&buf.start, start, sizeof(struct timeval));
	memcpy(&buf.finish, finish, sizeof(struct timeval));
	sts = __pmTimeSendPDU(client.toServer, PM_TCTL_BOUNDS, &buf);
    }

    return sts;
}

/*
 * send a label and a timezone string
 */
int
pmTimeSendTimezone(const char *label, const char *tz)
{
    pmTime buf;
    int sts = 0;

    if (client.connected == 0) {
	sts = PM_ERR_NOTCONN;
    }
    else {
	memset(&buf, 0, sizeof(buf));
	/* truncate copying timezone strings if necessary */
	if (label == NULL)
	    buf.tzlabel[0] = '\0'; 
	else {
	    strncpy(buf.tzlabel, label, sizeof(buf.tzlabel));
	    buf.tzlabel[sizeof(buf.tzlabel)-1] = '\0';
	}
	if (tz == NULL)
	    buf.tz[0] = '\0'; 
	else {
	    strncpy(buf.tz, tz, sizeof(buf.tz));
	    buf.tz[sizeof(buf.tz)-1] = '\0';
	}
	sts = __pmTimeSendPDU(client.toServer, PM_TCTL_TZ, &buf);
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "pmTimeSendTimezone(\"%s\", \"%s\") -> %d\n",
	    label ? label : "", tz ? tz : "", sts);
#endif
    }

    return sts;
}

/*
 * send new archive position
 */
int
pmTimeSendPosition(const struct timeval *position, int delta)
{
    pmTime buf;
    int sts = 0;

    if (client.connected == 0) {
	sts = PM_ERR_NOTCONN;
    }
    else {
	memcpy(&buf.position, position, sizeof(struct timeval));
	buf.delta = delta;
	sts = __pmTimeSendPDU(client.toServer, PM_TCTL_SET, &buf);
    }

    return sts;
}

/*
 * send new vcr mode
 */
int
pmTimeSendMode(int mode)
{
    pmTime buf;
    int sts = 0;

    if (client.connected == 0) {
	sts = PM_ERR_NOTCONN;
    }
    else {
	buf.vcrmode = mode;
	sts = __pmTimeSendPDU(client.toServer, PM_TCTL_VCRMODE, &buf);
    }

    return sts;
}

int
pmTimeShowDialog(int show)
{
    pmTime buf;
    int sts = 0;

    if (client.connected == 0) {
	sts = PM_ERR_NOTCONN;
    }
    else {
	buf.showdialog = show;
	sts = __pmTimeSendPDU(client.toServer, PM_TCTL_SHOWDIALOG, &buf);
    }

    return sts;
}

/*
 * Create control port on the given named pipe, then bind and listen.
 * This is for use by X11 widgets and the pmTimeMaster main program.
 */
int
__pmTimeInit(const char *port, pmTime *state)
{
    int sts = 0;

    struct sockaddr_un serverAddr;

    /*
     * set up a named pipe, bind it to the port and start listening.
     * return the control socket or err.
     */
    if (port == NULL) {
	return PM_ERR_NEEDPORT;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    strcpy(serverAddr.sun_path, port);
    serverAddr.sun_family = AF_UNIX;

    if (timestate == NULL) {
	if ((timestate = (__pmTimeState *)malloc(sizeof(__pmTimeState))) == NULL)
	    return -errno;
	memset(timestate, 0, sizeof(__pmTimeState));
	memcpy(&timestate->data, state, sizeof(pmTime));
	timestate->port = strdup(port);
    }

    sts = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sts < 0)
	sts = -errno;
    else {
	timestate->control = sts;
	sts = bind(timestate->control, (struct sockaddr*)&serverAddr,
	    (int)strlen(serverAddr.sun_path) + (int)sizeof(serverAddr.sun_family));

	if (sts < 0)
	    sts = -errno;
	else {
	    sts = listen (timestate->control, 16);
	    if (sts < 0)
		sts = -errno;
	    else {
		/*
		 * return the control port.
		 * The time controller will accept clients on this 
		 */
		sts = timestate->control;
	    }
	}
    }

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
    if (sts < 0)
	fprintf(stderr, "__pmTimeInit: failed to create socket on port %s: %s\n", port, pmErrStr(sts));
    else
	fprintf(stderr, "__pmTimeInit(listening on port %s) -> %d\n", port, sts);
#endif

    return sts;
}


/*
 * close control port, close and free all clients, free internal state
 */
int
__pmTimeClose(void)
{
    int sts = 0;
    /* TODO EXCEPTION PCP 2.0 */

    return sts;
}

/*
 * get pointer to internal time control state
 */
int
__pmTimeGetState(__pmTimeState **stateptr)
{
    int sts = 0;

    if (timestate == NULL)
	sts = PM_ERR_NOTCONN;
    else {
	*stateptr = timestate;
    }

    return sts;
}

/*
 * accept a new client on the control port
 * return the protocol version number (or <0 err status)
 * Note: return -EINVAL means a down-rev client tried to connect.
 */
int
__pmTimeAcceptClient(int *toClient, int *fromClient, pmTime *initParams)
{
    int sts = 0;
    struct sockaddr_un addr;
    int addrlen = sizeof(addr);

    if (timestate == NULL || timestate->control < 0)
	sts = PM_ERR_NOTCONN;
    else {
	*toClient = accept(timestate->control, (struct sockaddr *)&addr, &addrlen);
	if (*toClient < 0)
	    sts = *toClient;
	else {
	    /*
	     * pmTime was using two channels, but only needed one.
	     * The second one has been removed, but the from/to Client
	     * fields are still needed for the APIs.
	     */
	    *fromClient = *toClient;

	    /* this will block until the client sends them */
	    sts = GetInitParams(*fromClient, initParams);
	}
    }

    return sts;
}

/*
 * register XTB data for the client
 */
void
__pmTimeRegisterClient(int fromClient, pmTime *initParams)
{
    int client_ix;
    __pmTimeClient *client;

    /*
     * NOTE that this is an optional symbol for IRIX6_5
     */

    for(client_ix = 0; client_ix < timestate->numclients; client_ix++) {
	client = &timestate->clients[client_ix];
	if (client->fromClient == fromClient) {
	    memcpy(&client->data, initParams, sizeof(client->data));
	    /* 
	     * if this client does not utilise the XTB, but XTB is currently
	     * set, then it must be switched off, and all the clients must
	     * be told about it.
	     */
	    if ((timestate->data.vcrmode & PM_XTB_FLAG) &&
		!(initParams->vcrmode & PM_XTB_FLAG)) {
		struct timeval delta_tv;
		pmXTBdeltaToTimeval(timestate->data.delta, timestate->data.vcrmode, &delta_tv);
		timestate->data.delta = (int)(delta_tv.tv_sec * 1000 
				      + delta_tv.tv_usec / 1000);
		timestate->data.vcrmode &= __PM_MODE_MASK;
		__pmTimeBroadcast(PM_TCTL_SET, &timestate->data);
	    }
	    break; /* successful completion */
	}
    }
}

#if defined(IRIX6_5) 
#pragma optional __pmTimeRegisterClient
#endif

__pmTimeClient *
__pmTimeFindClient(int fd)
{
    int i;
    __pmTimeClient *client = NULL;

    if (timestate != NULL) {
	for (i=0; i < timestate->numclients; i++) {
	    if (timestate->clients[i].fromClient == fd) {
		client = &timestate->clients[i];
		break;
	    }
	}
    }

    return client;
}


/*
 * initialize a new time control client on given sockets|pipe
 * return client number (or err < 0)
 */
int
__pmTimeAddClient(int toClient, int fromClient)
{
    int sts = 0;
    __pmTimeClient *client;

    if (timestate == NULL)
	sts = PM_ERR_NOTCONN;
    else {
	timestate->numclients++;
	timestate->clients = (__pmTimeClient *)realloc(timestate->clients, timestate->numclients * sizeof(__pmTimeClient));
	if (timestate->clients == NULL)
	    sts = -errno;
	else {
	    sts = timestate->numclients - 1;
	    client = &timestate->clients[timestate->numclients - 1];
	    memset(client, 0, sizeof(*client));
	    client->toClient = toClient;
	    client->fromClient = fromClient;
	}
    }

    return sts;
}

/*
 * delete and free a time control client (on given sndFd)
 * return number of remaining clients, or err < 0
 */
int
__pmTimeDelClient(int fromClient)
{
    int i;
    int sts = 0;
    __pmTimeClient *client;

    if (timestate == NULL)
	sts = PM_ERR_NOTCONN;
    else {
	for (i=0; i < timestate->numclients; i++) {
	    client = &timestate->clients[i];
	    if (client->fromClient == fromClient) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
		fprintf(stderr, "__pmTimeDelClient: deleting client fd=%d\n", fromClient);
#endif
		close(client->toClient);
		client->toClient = client->fromClient = -1;
		for (; i < timestate->numclients-1; i++) {
		    memcpy(&timestate->clients[i], &timestate->clients[i+1], sizeof(__pmTimeClient));
		}
		timestate->numclients--;
		break;
	    }
	}
    }

    if (sts < 0)
	return sts;
    else {
	disconnectXTBCLient();
	return timestate->numclients;
    }
}

/*
 * add a new timezone to the list, return new timezone number (or err < 0)
 */
int
__pmTimeAddTimezone(const char *label, const char *tz)
{
    int sts = 0;
    pmTimeZone *zone;

    if (timestate == NULL)
	sts = PM_ERR_NOTCONN;
    else {
	timestate->numtz++;
	timestate->tz = (pmTimeZone *)realloc(timestate->tz, timestate->numtz * sizeof(pmTimeZone));
	if (timestate->tz == NULL)
	    sts = -errno;
	else {
	    sts = timestate->numtz - 1;
	    zone = &timestate->tz[timestate->numtz - 1];
	    zone->label = strdup(label);
	    zone->tz = strdup(tz);
	    zone->handle = pmNewZone(tz);
	}
    }

    return sts;
}

int
__pmTimeGetTimezone(const char *label)
{
    int sts = -1;
    int i;
    pmTimeZone *zone;

    for (i=0; i < timestate->numtz; i++) {
	zone = &timestate->tz[i];
	if (strcmp(zone->label, label) == 0) {
	    sts = zone->handle;
	    break;
	}
    }

    return sts;
}


/*
 * send a time control command to all clients
 * return err or 0
 */
int
__pmTimeBroadcast(int cmd, pmTime *syncstate)
{
    int sts = 0;
    __pmTimeClient *client;
    int i;

    if (timestate == NULL)
	return PM_ERR_NOTCONN;

    /* check if there are any acks pending */
    for (i=0; i < timestate->numclients; i++) {
	client = &timestate->clients[i];
	if (client->wantAck == 1)
	    return PM_ERR_WANTACK;
    }

    /* broadcast the request, and set the client wantAck flags */
    for (i=0; i < timestate->numclients; i++) {
	client = &timestate->clients[i];
	if (client->toClient < 0 || client->wantAck != 0) {
	    continue;
	}
	sts = __pmTimeSendPDU(client->toClient, cmd, syncstate);
	if (sts >= 0 && cmd == PM_TCTL_STEP) {
	    client->wantAck = 1; /* ACK pending state */
	}
    }

    return sts;
}

/*
 * Promote clients which have pending acks
 * (wantAck==1) to the hung state (wantAck==2),
 * where they remain until sending an ACK.
 */
void
__pmTimeSetHungClients(void)
{
    int i;
    __pmTimeClient *client;

    for (i=0; i < timestate->numclients; i++) {
	client = &timestate->clients[i];
	if (client->wantAck == 1) {
	    client->wantAck = 2;
	}
    }
}

/*
 * Return the number of clients with outstanding acks.
 * Hung clients (wantAck==2) are not counted.
 * Do NOT block if timeout is NULL.
 */
int
__pmTimeAcksPending(struct timeval *timeout)
{
    __pmTimeClient *client;
    int count = 0;
    int i;
    int bigfd = 0;
    fd_set fdset;

    if (timeout != NULL) {
	FD_ZERO(&fdset);
    }

    for (i=0; i < timestate->numclients; i++) {
	client = &timestate->clients[i];
	if (client->wantAck == 1 && client->fromClient >= 0) {
	    if (timeout != NULL) {
		FD_SET(client->fromClient, &fdset);
		if (client->fromClient > bigfd)
		    bigfd = client->fromClient;
	    }
	    count++;
	}
    }

    if (count > 0 && timeout != NULL) {
	/* we may block here */
	select(bigfd+1, &fdset, NULL, NULL, timeout);
    }

    return count;
}

/*
 * low level time control PDU transmit
 */
int
__pmTimeSendPDU(int fd, int cmd, pmTime *state)
{
    int sts = 0;
    int len;
    int mod;
    __pmTimeval tv;
    char buf[sizeof(pmTime)];
    char tzbuf[64];

    switch (cmd) {

    case PM_TCTL_STEP: /* no data */
	len = 0;
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "__pmTimeSendPDU(pid=%d fd=%d cmd=%s)\n",
	    getpid(), fd, __pmTimeCmdStr(cmd));
#endif
	break;

    case PM_TCTL_SKIP: /* no data */
	len = 0;
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "__pmTimeSendPDU(pid=%d fd=%d cmd=%s)\n",
	    getpid(), fd, __pmTimeCmdStr(cmd));
#endif
	break;

    case PM_TCTL_ACK: /* timeval */
	tv.tv_sec = (__int32_t)state->position.tv_sec;
	tv.tv_usec = (__int32_t)state->position.tv_usec;
	memcpy(buf, &tv, sizeof(tv));
	len = sizeof(tv);
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "__pmTimeSendPDU(pid=%d fd=%d cmd=%s pos=%.24s.%03d)\n",
	    getpid(), fd, __pmTimeCmdStr(cmd),
	    pmCtime((time_t *)&state->position.tv_sec, tzbuf), (int)(state->position.tv_usec / 1000));
#endif
	break;

    case PM_TCTL_SET: /* timeval, int */
	tv.tv_sec = (__int32_t)state->position.tv_sec;
	tv.tv_usec = (__int32_t)state->position.tv_usec;
	memcpy(buf, &tv, sizeof(tv));
	len = sizeof(tv);
	memcpy(buf + len, &state->vcrmode, sizeof(state->vcrmode));
	len += sizeof(state->vcrmode);
	memcpy(buf + len, &state->delta, sizeof(state->delta));
	len += sizeof(state->delta);
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "__pmTimeSendPDU(pid=%d fd=%d cmd=%s mode=%d delta=%d pos=%.24s.%03d)\n",
	    getpid(), fd, __pmTimeCmdStr(cmd), state->vcrmode, state->delta,
	    pmCtime((time_t *)&state->position.tv_sec, tzbuf), (int)(state->position.tv_usec / 1000));
#endif
	break;

    case PM_TCTL_VCRMODE: /* int */
	len = sizeof(state->vcrmode);
	memcpy(buf, &state->vcrmode, len);
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "__pmTimeSendPDU(pid=%d fd=%d cmd=%s vcrmode=%s)\n",
	getpid(), fd, __pmTimeCmdStr(cmd), __pmTimeStateStr(state->vcrmode));
#endif
	break;

    case PM_TCTL_BOUNDS: /* timeval, timeval */
	tv.tv_sec = (__int32_t)state->start.tv_sec;
	tv.tv_usec = (__int32_t)state->start.tv_usec;
	len = sizeof(tv);
	memcpy(buf, &tv, len);
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "__pmTimeSendPDU(pid=%d fd=%d cmd=%s start=%.24s.%03d",
	    getpid(), fd, __pmTimeCmdStr(cmd),
	    pmCtime((time_t *)&state->start.tv_sec, tzbuf), (int)(state->start.tv_usec / 1000));
#endif
	tv.tv_sec = (__int32_t)state->finish.tv_sec;
	tv.tv_usec = (__int32_t)state->finish.tv_usec;
	memcpy(buf + len, &tv, sizeof(tv));
	len += sizeof(tv);
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, " finish=%.24s.%03d)\n",
	    pmCtime((time_t *)&state->finish.tv_sec, tzbuf), (int)(state->finish.tv_usec / 1000));
#endif
	break;

    case PM_TCTL_TZ: /* string, string */
	if (state->tzlabel != NULL) {
	    strcpy(buf, state->tzlabel);
	    len = (int)strlen(state->tzlabel)+1;
	}
	else {
	    buf[0] = (char)0;
	    len = 1;
	}

	if (state->tz != NULL) {
	    strcpy(buf + len, state->tz);
	    len += strlen(state->tz)+1;
	}
	else {
	    buf[len] = (char)0;
	    len++;
	}
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "__pmTimeSendPDU(pid=%d fd=%d cmd=%s tz=%s tzlabel=%s)\n",
	    getpid(), fd, __pmTimeCmdStr(cmd), state->tz, state->tzlabel);
#endif
	break;
    
    case PM_TCTL_SHOWDIALOG: /* int */
	len = sizeof(state->showdialog);
	memcpy(buf, &state->showdialog, len);
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "__pmTimeSendPDU(pid=%d fd=%d cmd=%s showdialog=%d)\n",
	    getpid(), fd, __pmTimeCmdStr(cmd), state->showdialog);
#endif
	break;

    default:
	/* eh? */
	break;
    }

    mod = len % (int)sizeof(__pmPDU);
    len += mod ? (int)sizeof(__pmPDU) - mod : 0;
    sts = SendTctl(fd, cmd, len, buf);

    return sts;
}

/*
 * low level time control PDU receive, returns recv'ed command
 */
int
__pmTimeRecvPDU(int fd, pmTime *state)
{
    char	*buf;
    void	*vp;
    int		len;
    int		sts;
    int		cmd;
    __pmTimeval	tv;

    if ((sts = RecvTctl(fd, &cmd, &len, &vp)) < 0)
	return sts;
    buf = vp;

    switch (cmd) {
    case PM_TCTL_STEP: /* no data */
	break;

    case PM_TCTL_SKIP: /* no data */
	break;

    case PM_TCTL_ACK: /* timeval */
	if (len != sizeof(tv))
	    sts = PM_ERR_IPC;
	else {
	    memcpy(&tv, buf, sizeof(tv));
	    state->position.tv_sec = tv.tv_sec;
	    state->position.tv_usec = tv.tv_usec;
	}
	break;

    case PM_TCTL_SET: /* timeval, int */
	if (len != sizeof(tv) + sizeof(state->vcrmode) + sizeof(state->delta)) {
	    sts = PM_ERR_IPC;
	}
	else {
	    memcpy(&tv, buf, sizeof(tv));
	    state->position.tv_sec = tv.tv_sec;
	    state->position.tv_usec = tv.tv_usec;
	    memcpy(&state->vcrmode, buf + sizeof(tv), sizeof(state->vcrmode));
	    memcpy(&state->delta, buf + sizeof(tv) + sizeof(state->vcrmode), sizeof(state->delta));
	}
	break;

    case PM_TCTL_VCRMODE: /* int */
	if (len != sizeof(state->vcrmode))
	    sts = PM_ERR_IPC;
	else {
	    int l_mode;
	    if (sizeof(l_mode) != len)
		sts = PM_ERR_IPC;
	    else {
		memcpy(&l_mode, buf, sizeof(state->vcrmode));
		state->vcrmode = (state->vcrmode & 0xffff0000) | 
				 (l_mode & __PM_MODE_MASK);
	    }
	}
	break;

    case PM_TCTL_BOUNDS: /* timeval, timeval */
	if (len != 2 * sizeof(tv))
	    sts = PM_ERR_IPC;
	else {
	    memcpy(&tv, buf, sizeof(tv));
	    state->start.tv_sec = tv.tv_sec;
	    state->start.tv_usec = tv.tv_usec;
	    memcpy(&tv, buf + sizeof(tv), sizeof(tv));
	    state->finish.tv_sec = tv.tv_sec;
	    state->finish.tv_usec = tv.tv_usec;
	}
	break;

    case PM_TCTL_TZ: /* string, string */
	if (len == 0)
	    sts = PM_ERR_IPC;
	else {
	    strcpy(state->tzlabel, buf);
	    strcpy(state->tz, buf+strlen(state->tzlabel)+1);
	}
	break;
    
    case PM_TCTL_SHOWDIALOG: /* int */
	if (len != sizeof(state->showdialog))
	    sts = PM_ERR_IPC;
	else
	    memcpy(&state->showdialog, buf, sizeof(state->showdialog));
	break;

    default:
	/* eh? */
	sts = PM_ERR_IPC;
    }

    return (sts >= 0) ? cmd : sts;
}

static int
GetInitParams(int fd, pmTime *initParams)
{
    void	*vp;
    int		len;
    int		sts;
    int		cmd;
    __pmTimeInitPDU *initpdu;

    if ((sts = RecvTctl(fd, &cmd, &len, &vp)) < 0)
	return sts;

    switch (cmd) {
    case PM_TCTL_INITIALIZE:
	if (len != sizeof(__pmTimeInitPDU))
	    sts = PM_ERR_IPC;
	else {
	    initpdu = (__pmTimeInitPDU *)vp;
	    sts = initpdu->version;
	    memcpy(initParams, &initpdu->params, sizeof(pmTime));
	}
	break;

    case 0x700b: /* PDU_DATA_X => down-rev client, not supported */
	sts = -EINVAL;
	break;

    default:
	/* eh? */
	sts = PM_ERR_IPC;
    }

    /* return proto version number or error */
    return sts;
}

/*
 * debugging
 */
const char *
__pmTimeCmdStr(int cmd)
{
    switch(cmd) {
    case PM_TCTL_SET:		return "SET";
    case PM_TCTL_STEP:		return "STEP";
    case PM_TCTL_SKIP:		return "SKIP";
    case PM_TCTL_VCRMODE:	return "VCRMODE";
    case PM_TCTL_TZ:		return "TZ";
    case PM_TCTL_BOUNDS:	return "BOUNDS";
    case PM_TCTL_ACK:		return "ACK";
    case PM_TCTL_SHOWDIALOG:	return "SHOWDIALOG";
    default:			return "UNKNOWN";
    }
}

const char *
__pmTimeStateStr(int s)
{
    int state = s & __PM_MODE_MASK;
    switch(state) {
    case PM_TCTL_VCRMODE_STOP:		return "STOP";
    case PM_TCTL_VCRMODE_PLAY:		return "PLAY";
    case PM_TCTL_VCRMODE_FAST:		return "FAST";
    case PM_TCTL_VCRMODE_DRAG:		return "DRAG";
    default:				return "UNKNOWN";
    }
}

static const char *
XTBStr(int mode)
{
    int timebase = PM_XTB_GET(mode);
    switch(timebase) {
	case PM_TIME_NSEC:	return "NANOSECONDS";
	case PM_TIME_USEC:	return "MICROSECONDS";
	case PM_TIME_MSEC:	return "MILLISECONDS";
	case PM_TIME_SEC:	return "SECONDS";
	case PM_TIME_MIN:	return "MINUTES";
	case PM_TIME_HOUR:	return "HOURS";
	default:		return "MILLISECONDS";
    }
}

const char *
__pmTimeStr(const pmTime *state)
{
    static char buf[1024];
    time_t tm = (time_t)state->position.tv_sec;
    sprintf(buf, "state=%-7s pos=%.24s[%+03dms] delta=%d %s show=%s",
    	    __pmTimeStateStr(state->vcrmode), ctime(&tm), 
	    (int)(state->position.tv_usec / 1000), state->delta, XTBStr(state->vcrmode),
	    state->showdialog ? "yes" : "no");
    return buf;
}



/*
 * pmTimeGetStatePixmap(int dir, int live, int record)
 * returns char **data, to use with XpmGetPixmapFromData().
 */
static char *TopHalf[] = {
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "ccccccccccccccccccdcccccccccccccccbccccccccccccccccc",
    "cccccccccccccccc##dcc##########dcc#bcccccccccccccccc",
    "ccccccccccccccc##ddcc#IIIIIIIIIdcc#bbccccccccccccccc",
    "ccccccccccccc##<<ddcc#IIIIIIIIIdcc#>>bbccccccccccccc",
    "cccccccccccc##<<<ddcc#IIIIIIIIIdcc#>>>bbcccccccccccc",
    "cccccccccc##<<<<<ddcc#IIIIIIIIIdcc#>>>>>bbcccccccccc",
    "ccccccccc##<<<<<<ddcc#IIIIIIIIIdcc#>>>>>>bbccccccccc",
    "ccccccc##<<<<<<<<ddcc#IIIIIIIIIdcc#>>>>>>>>bbccccccc",
    "cccccc#b<<<<<<<<<ddcc#IIIIIIIIIdcc#>>>>>>>>>dbcccccc",
    "cccccccbb<<<<<<<<ddcc#IIIIIIIIIdcc#>>>>>>>>ddccccccc",
    "cccccccccbb<<<<<<ddcc#IIIIIIIIIdcc#>>>>>>ddccccccccc",
    "ccccccccccbb<<<<<ddcc#IIIIIIIIIdcc#>>>>>ddcccccccccc",
    "ccccccccccccbb<<<ddcc#IIIIIIIIIdcc#>>>ddcccccccccccc",
    "cccccccccccccbb<<ddcc#IIIIIIIIIdcc#>>ddccccccccccccc",
    "cccccccccccccccbbddcc#IIIIIIIIIdcc#ddccccccccccccccc",
    "ccccccccccccccccbbdccdddddddddddcc#dcccccccccccccccc",
    "ccccccccccccccccccdcccccccccccccccdccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
};

static char *BotLive[] = {
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccRRccccccccccccccccccccccccccccccccccccccc",
    "ccccccccccRRRRcccccccc.ccccccccccccccccccccccccccccc",
    "ccccccccRRRRRRRRcccccc.ccccc.ccccccccccccccccccccccc",
    "ccccccccRRRRRRRRcccccc.ccccccccccccccccccccccccccccc",
    "cccccccRRRRRRRRRRccccc.ccccc.cc.ccc.cc...ccccccccccc",
    "cccccccRRRRRRRRRRccccc.ccccc.cc.ccc.c.ccc.cccccccccc",
    "ccccccccRRRRRRRRcccccc.ccccc.cc.ccc.c.....cccccccccc",
    "ccccccccRRRRRRRRcccccc.ccccc.cc.ccc.c.cccccccccccccc",
    "ccccccccccRRRRcccccccc.ccccc.ccc...cc.cccccccccccccc",
    "cccccccccccRRccccccccc.....c.cccc.cccc...ccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc"
};

static char *BotArchive[] = {
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "ccccccccc.ccccccccccccccc.cccccccccccccccccccccccccc",
    "cccccccc.c.cccccccccccccc.ccccc.cccccccccccccccccccc",
    "ccccccc.ccc.ccccccccccccc.cccccccccccccccccccccccccc",
    "ccccccc.ccc.c.c..ccc...cc.c..cc.c.ccc.cc...ccccccccc",
    "ccccccc.ccc.c..cc.c.ccc.c..cc.c.c.ccc.c.ccc.cccccccc",
    "ccccccc.....c.ccccc.ccccc.ccc.c.c.ccc.c.....cccccccc",
    "ccccccc.ccc.c.ccccc.ccc.c.ccc.c.c.ccc.c.cccccccccccc",
    "ccccccc.ccc.c.ccccc.ccc.c.ccc.c.cc.c.cc.cccccccccccc",
    "ccccccc.ccc.c.cccccc...cc.ccc.c.ccc.cccc...ccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccc"
    };

static char *rev_off	= "< c #9b9b9b";
static char *rev_on	= "< c #ffff00";
static char *stop_off	= "I c #9b9b9b";
static char *stop_on	= "I c #ffff00";
static char *for_off	= "> c #9b9b9b";
static char *for_on	= "> c #ffff00";
static char *rec_off	= "R s background";
static char *rec_on	= "R c #ff0000";

static char *font	= ". s foreground";
static char *b_shade	= "b c #686868";
static char *c_shade	= "c s background";
static char *d_shade	= "d c #b8b8b8";
static char *h_shade	= "# c #383838";

const char **
pmTimeGetStatePixmap(int vcrmode, int dir, int live, int record)
{
    int i;
    int row=0;
    static char *pd[64];

    memset(pd, 0, sizeof(pd));

    /*             width height num_colors chars_per_pixel */
    pd[row++] = "    52    40        9            1";
    pd[row++] = font;
    pd[row++] = b_shade;
    pd[row++] = c_shade;
    pd[row++] = d_shade;
    pd[row++] = h_shade;
    
    if (live) {
	/* live mode */
	pd[row++] = record ? rec_on : rec_off;
	pd[row++] = rev_off;
	pd[row++] = dir == 0 ? stop_on : stop_off;
	pd[row++] = dir == 0 ? for_off : for_on;
    }
    else {
	/* archive mode */
	pd[row++] = rec_off;
	if ((vcrmode & __PM_MODE_MASK) == PM_TCTL_VCRMODE_DRAG) {
	    pd[row++] = rev_on;
	    pd[row++] = stop_off;
	    pd[row++] = for_on;
	}
	else {
	    pd[row++] = dir < 0 ? rev_on : rev_off;
	    pd[row++] = dir == 0 ? stop_on : stop_off;
	    pd[row++] = dir > 0 ? for_on : for_off;
	}
    }

    for (i=0; i < sizeof(TopHalf) / sizeof(TopHalf[0]); i++) {
	pd[row++] = TopHalf[i];
    }

    if (live) {
	for (i=0; i < sizeof(BotLive) / sizeof(BotLive[0]); i++) {
	    pd[row++] = BotLive[i];
	}
    }
    else {
	for (i=0; i < sizeof(BotArchive) / sizeof(BotArchive[0]); i++) {
	    pd[row++] = BotArchive[i];
	}
    }

    return (const char **)pd;
}


/*
 * Time Control binary data exchange 
 * (over unix doamin sockets, so no byte order conv necessary)
 *
 * | len | cmd | buffer
 * 	len == length of whole pdu, including len
 *	cmd == integer tctl command
 *	buffer == depends on cmd, may be empty.
 */
static char	*tctl_buf = NULL;
static int	tctl_buflen = 0;

static int
SendTctl(int fd, int cmd, int vlen, void *vp)
{
    static int	sigpipe_done = 0;
    int		len = (int)sizeof(vlen) + (int)sizeof(cmd) + vlen;

    if (!sigpipe_done) {
	SIG_PF  user_onpipe;
	user_onpipe = signal(SIGPIPE, SIG_IGN);
	if (user_onpipe != SIG_DFL)
	    signal(SIGPIPE, user_onpipe);
	sigpipe_done = 1;
    }
    if (tctl_buflen < len) {
	tctl_buflen = len;
	if ((tctl_buf = realloc(tctl_buf, tctl_buflen)) == NULL)
	    return -errno;
	memset(tctl_buf, 0, tctl_buflen); /* for purify */
    }
    memcpy(tctl_buf, &len, sizeof(len));
    memcpy(tctl_buf + sizeof(len), &cmd, sizeof(cmd));
    memcpy(tctl_buf + sizeof(len) + sizeof(cmd), vp, vlen);
    if (send(fd, (const void *)tctl_buf, len, 0) < 0)
	return -errno;

    return len;
}

static int
RecvTctl(int fd, int *cmd, int *vlen, void **vp)
{
    int		len;
    int		rem;
    int		off;
    int		n;

    if ((n = recv(fd, (void *)&len, sizeof(len), 0)) < 0)
	return -errno;
    else
    if (n == 0)
	return PM_ERR_EOF;

    if (tctl_buflen < len) {
	tctl_buflen = len;
	if ((tctl_buf = realloc(tctl_buf, tctl_buflen * sizeof(char))) == NULL)
	    return -errno;
    }

    for (off = 0, rem = len - (int)sizeof(len); rem > 0; rem -= n, off += n) {
	if ((n = recv(fd, (void *)(tctl_buf + off), rem, 0)) < 0)
	    return -errno;
	else
	if (n == 0)
	    return PM_ERR_EOF;
    }
    memcpy(cmd, tctl_buf, sizeof(*cmd));
    *vp = (void *)(tctl_buf + sizeof(*cmd));
    /* vlen is length of buffer, NOT including len or cmd */
    *vlen = len - (int)sizeof(len) - (int)sizeof(*cmd); 

    return len;
}
