#include "RPC_TCP_Connector.h"

#include <bstring.h>
#include <errno.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Log.h"
#include "Scheduler.h"

RPC_TCP_Connector::RPC_TCP_Connector(ulong p, ulong v, ulong host,
				     ConnectHandler ch, void *cl)
    : state(IDLE), sockfd(-1), program(p), version(v),
      connect_handler(ch), closure(cl)
{
    bzero(&address, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = host;
}

RPC_TCP_Connector::~RPC_TCP_Connector()
{
    deactivate();
}

//////////////////////////////////////////////////////////////////////////////

void
RPC_TCP_Connector::activate()
{
    assert(state == IDLE);
    state = PMAPPING;
    address.sin_port = PMAPPORT;
    retry_interval = INITIAL_RETRY_INTERVAL;
    try_to_connect();
}

void
RPC_TCP_Connector::deactivate()
{
    if (sockfd >= 0)
    {   (void) Scheduler::remove_write_handler(sockfd);
	(void) close(sockfd);
	sockfd = -1;
    }
    if (state == PAUSING)
	(void) Scheduler::remove_onetime_task(retry_task, this);
    state = IDLE;
}

//////////////////////////////////////////////////////////////////////////////

void
RPC_TCP_Connector::try_to_connect()
{
    assert(sockfd == -1);
    assert(state == PMAPPING || state == CONNECTING);
    int fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
    {   Log::perror("socket");
	try_again();
	return;
    }
    int yes = 1;
    int rc = ioctl(fd, FIONBIO, &yes);
    if (rc < 0)
    {   Log::perror("FIONBIO");
	(void) close(fd);
	try_again();
	return;
    }
    rc = connect(fd, &address, sizeof address);
    if (rc == 0)
    {   sockfd = fd;
	write_handler(fd, this);
    }
    else if (errno == EINPROGRESS)
    {   (void) Scheduler::install_write_handler(fd, write_handler, this);
	sockfd = fd;
    }
    else
    {   Log::perror("connect");
	(void) close(fd);
	try_again();
    }
}

void
RPC_TCP_Connector::write_handler(int fd, void *closure)
{
    (void) Scheduler::remove_write_handler(fd);
    RPC_TCP_Connector *conn = (RPC_TCP_Connector *) closure;
    assert(fd == conn->sockfd);
    int rc = connect(fd, &conn->address, sizeof conn->address);
    if (rc < 0 && errno != EISCONN)
    {
	Log::perror("connect");
	(void) close(conn->sockfd);
	conn->sockfd = -1;
	conn->try_again();
	return;
    }
    switch (conn->state)
    {
    case PMAPPING:

	//  We have connected with portmapper; make a PMAP_GETPORT
	//  call.

	sockaddr_in addr;
	addr.sin_port = PMAPPORT;
	CLIENT *client = clnttcp_create(&addr, PMAPPROG, PMAPVERS, &fd,
					RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	clnt_stat rc = (clnt_stat) ~RPC_SUCCESS;
	unsigned short port = 0;
	if (client)
	{   struct pmap pmp = { conn->program, conn->version, IPPROTO_TCP, 0 };
	    timeval timeout = { PMAP_TIMEOUT, 0 };
	    rc = CLNT_CALL(client, PMAPPROC_GETPORT, (xdrproc_t) xdr_pmap,
			   &pmp, (xdrproc_t) xdr_u_short, &port, timeout);
	    if (rc != RPC_SUCCESS)
		Log::info("Portmapper call failed: %s", clnt_sperrno(rc));
	    CLNT_DESTROY(client);
	}
	else
	    Log::info("Couldn't create RPC TCP/IP client: %m");
	(void) close(fd);	
	conn->sockfd = -1;
	if (rc == RPC_SUCCESS && port != 0)
	{   conn->state = CONNECTING;
	    conn->address.sin_port = port;
	    conn->try_to_connect();
	}
	else
	    conn->try_again();
	break;

    case CONNECTING:

	conn->state = IDLE;
	conn->sockfd = -1;
	(*conn->connect_handler)(fd, conn->closure);
	break;

    default:

	int unknown_connector_state = 0; assert(unknown_connector_state);
	break;
    }
}

//////////////////////////////////////////////////////////////////////////////

//  Implement an exponential falloff.  Start trying once a second,
//  slow to once every 1024 seconds (~17 minutes).  Time required by
//  each connection attempt is added in, so if other host is down and
//  TCP has a two minute timeout, we start by polling every 2:01, and
//  slow to every 19:05.

void
RPC_TCP_Connector::try_again()
{
    assert(state == PMAPPING || state == CONNECTING);
    state = PAUSING;

    timeval next_time;
    (void) gettimeofday(&next_time, NULL);
    next_time.tv_sec += retry_interval;
    if (retry_interval < MAX_RETRY_INTERVAL)
	retry_interval *= 2;

    Scheduler::install_onetime_task(next_time, retry_task, this);
}

void
RPC_TCP_Connector::retry_task(void *closure)
{
    RPC_TCP_Connector *conn = (RPC_TCP_Connector *) closure;
    assert(conn->state == PAUSING);
    conn->state = PMAPPING;
    conn->address.sin_port = PMAPPORT;
    conn->try_to_connect();
}
