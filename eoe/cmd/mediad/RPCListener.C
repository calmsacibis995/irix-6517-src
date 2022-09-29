#include "RPCListener.H"

#include <bstring.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Log.H"
#include "MonitorClient.H"
#include "Scheduler.H"

extern "C" int bindresvport(int sd, struct sockaddr_in *);

RPCListener::RPCListener(bool all_hosts,
			 bool started_by_inetd,
			 u_long prog, u_long vers)
: _all_hosts(all_hosts),
  _started_by_inetd(started_by_inetd),
  _program(prog),
  _version(vers),
  _rendezvous_handler(accept_client, this)
{
    int sock;

    if (started_by_inetd)
    {
	//  Portmapper already knows about
	//  us, so just wait for the requests to start rolling in.

	sock = RENDEZVOUS_FD;
    }
    else
    {
	//  Register with portmapper.

	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
	{   Log::perror("can't create TCP/IP socket for rendezvous");
	    exit(1);
	}
	struct sockaddr_in addr;
	bzero(&addr, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
//	u_long host_address = all_hosts ? INADDR_ANY : INADDR_LOOPBACK;
//	addr.sin_addr.s_addr = htonl(host_address);
	addr.sin_port = 0;
	if (bindresvport(sock, &addr) < 0)
	{
	    Log::perror("can't bind to reserved port");
	    exit(1);
	}
	if (listen(sock, 1) < 0)
	{
	    Log::perror("can't listen for rendezvous");
	    exit(1);
	}
	if (!pmap_set(_program, _version, IPPROTO_TCP, addr.sin_port))
	{
	    (void) pmap_unset(_program, _version);
	    if (!pmap_set(_program, _version, IPPROTO_TCP, addr.sin_port))
	    {   Log::error("can't register with portmapper.");
		(void) close(sock);
		return;
	    }
	}
    }

    _rendezvous_handler.fd(sock);
    _rendezvous_handler.activate();
    Log::debug("listening for monitor clients on descriptor %d", sock);
}

RPCListener::~RPCListener()
{
    int fd = _rendezvous_handler.fd();
    if (fd >= 0)
    {   pmap_unset(_program, _version);
	int rc = close(fd);
	if (rc < 0)
	    Log::perror("close rendezvous port(%d)", fd);
	else
	    Log::debug("monitor service closed");
    }
}

void
RPCListener::accept_client(int rendezvous_fd, void * /* closure */ )
{
//    RPCListener *the_listener = (RPCListener *) closure;
    
    // Get the new socket.

    struct sockaddr_in addr;
    int addrlen = sizeof addr;
    int client_fd = accept(rendezvous_fd,
			   (struct sockaddr *) &addr,
			   &addrlen);
    if (client_fd < 0)
	Log::perror("failed to accept new client");
    else
    {
	bool is_remote = addr.sin_addr.s_addr != htonl(INADDR_LOOPBACK);

	// Create a MonitorClient on the heap.  Forget it --
	// the Scheduler knows about it and it will delete
	// itself when it reads EOF.

	(void) new MonitorClient(client_fd, is_remote);
    }
}
