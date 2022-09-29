#include "Listener.h"

#include <assert.h>
#include <bstring.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "Log.h"
#include "RemoteClient.h"
#include "Scheduler.h"
#include "TCP_Client.h"

extern "C" bindresvport(int sd, struct sockaddr_in *);

Listener::Listener(Boolean sbi, unsigned long p, unsigned long v)
: program(p),
  version(v),
  rendezvous_fd(-1),
  started_by_inetd(sbi),
  _ugly_sock(-1)
{
    if (started_by_inetd)
    {
	//  Portmapper already knows about
	//  us, so just wait for the requests to start rolling in.

	set_rendezvous_fd(RENDEZVOUS_FD);

	dirty_ugly_hack();
    }
    else
    {
	//  Need to register with portmapper.
	//  Unless we're debugging, fork and close all descriptors.

	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{   Log::perror("can't create TCP/IP socket for rendezvous");
	    exit(1);
	}
	struct sockaddr_in addr;
	bzero(&addr, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0);
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
	(void) pmap_unset(program, version);
	if (!pmap_set(program, version, IPPROTO_TCP, addr.sin_port))
	{
	    Log::error("can't register with portmapper.");
	    exit(1);
	}
	set_rendezvous_fd(sock);
    }
}

Listener::~Listener()
{
    if (rendezvous_fd >= 0)
    {   if (!started_by_inetd)
	    pmap_unset(program, version);
	int rc = close(rendezvous_fd);
	if (rc < 0)
	    Log::perror("close rendezvous port(%d)", rendezvous_fd);
	else
	    Log::debug("fam service closed");
	(void) Scheduler::remove_read_handler(rendezvous_fd);
    }
    if (_ugly_sock >= 0)
    {   (void) close(_ugly_sock);
	(void) unlink("/tmp/.fam_socket");
    }
}

void
Listener::set_rendezvous_fd(int newfd)
{
    if (rendezvous_fd >= 0)
    {   (void) Scheduler::remove_read_handler(rendezvous_fd);
	(void) close(rendezvous_fd);
    }
    rendezvous_fd = newfd;
    if (rendezvous_fd >= 0)
    {   (void) Scheduler::install_read_handler(rendezvous_fd,
					       accept_client, NULL);
	Log::debug("listening for clients on descriptor %d", rendezvous_fd);
    }
}

void
Listener::accept_client(int rendezvous_fd, void *)
{
    // Get the new socket.

    struct sockaddr_in addr;
    int addrlen = sizeof addr;
    int client_fd = accept(rendezvous_fd, (struct sockaddr *) &addr, &addrlen);
    if (client_fd < 0)
    {
	Log::perror("failed to accept new client");
	return;
    }

    TCP_Client *client;
    if (1 || addr.sin_addr.s_addr == INADDR_LOOPBACK)
    {
	// Client is local.

	client = new TCP_Client(client_fd);
    }
    else
    {
	// Check that client is superuser.

	if (addr.sin_port >= IPPORT_RESERVED)
	{
	    Log::error("remote client does not have a reserved port.");
	    (void) close(client_fd);
	    return;
	}

	client = new RemoteClient(client_fd, addr.sin_addr);

//	// If client is remote, verify that we export to that host.
//
//	if (!host_can_import(&addr.sin_addr))
//	{
//	    Log::error("remote client cannot import filesystems.");
//	    (void) close(client_fd);
//	    return;
//	}

	client = new RemoteClient(client_fd, addr.sin_addr);
    }

    // Forget client pointer.
}

//  Here's the problem.  If the network is stopped and restarted,
//  inetd and portmapper are killed and new ones are launched in their
//  place.  Since they have no idea that fam is already running, inetd
//  open a new TCP/IP socket and registers it with the new portmapper.
//  Meanwhile, the old fam has /dev/imon open, and any new fam launched
//  by the new inetd won't be able to use imon.  The old fam can't just
//  quit; it has all the state of all its existing clients to contend with.

//  So here's the dirty, ugly, hack solution.  The first fam to run is
//  the master.  The master opens the UNIX domain socket,
//  /tmp/.fam_socket, and listens for connections from slaves.  Each
//  subsequent fam is a slave.  It knows it's a slave because it can
//  connect to the master.  The slave passes its rendezvous descriptor
//  (the one that fam clients will connect to) through to the master
//  using the access rights mechanism of Unix domain sockets.  The
//  master receives the new descriptor and starts accepting clients on
//  it.  Meanwhile, the slave blocks, waiting for the master to close
//  the connection on /tmp/.fam_socket.  That happens when the master
//  exits.

//  This master/slave stuff has nothing to do with fam forwarding
//  requests to fams on other hosts.  That's called client/server
//  fams.

//  Why not just kill fam when we kill the rest of the network
//  services?  Because too many programs need it.  The desktop, the
//  desktop sysadmin tools, MediaMail...  None of those should go down
//  when the network goes down, and none of them are designed to
//  handle fam dying.

void
Listener::dirty_ugly_hack()
{
    static sockaddr_un sun = { AF_UNIX, "/tmp/.fam_socket" };

    int sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
    {   Log::perror("socket(PF_UNIX, SOCK_STREAM, 0)");
	exit(1);
    }
    
    struct stat sb;
    if (lstat(sun.sun_path, &sb) == 0 &&
	sb.st_uid == 0 && S_ISSOCK(sb.st_mode))
    {
	if (connect(sock, (sockaddr *) &sun, sizeof sun) == 0)
	{   
	    // Another fam is listening to /tmp/.fam_socket.
	    // Pass our rendezvous fd to the other fam and
	    // sleep until that fam exits.

	    int rights[1] = { rendezvous_fd };
	    msghdr msg = { NULL, 0, NULL, 0, (caddr_t) rights, sizeof rights };
	    if (sendmsg(sock, &msg, 0) < 0)
	    {   Log::perror("sendmsg");
		exit(1);
	    }
	    Log::debug("fam (process %d) enslaved to master fam", getpid());
	    char data[1];
	    int nb = read(sock, &data, sizeof data);
	    assert(nb == 0);
	    exit(0);
	}
	else
	    Log::debug("could not enslave myself: %m");
    }

    //  We were unable to connect to another fam.
    //	We'll create our own dirty ugly hack socket and accept connections.

    (void) unlink(sun.sun_path);
    if (bind(sock, (sockaddr *) &sun, sizeof sun) != 0)
    {   Log::perror("bind");
	exit(1);
    }

    if (chmod(sun.sun_path, 0700) != 0)
    {   Log::perror("chmod");
	exit(1);
    }

    if (listen(sock, 0) != 0)
    {   Log::perror("listen");
	exit(1);
    }

    (void) Scheduler::install_read_handler(sock, accept_ugly_hack, this);
    _ugly_sock = sock;
    Log::debug("fam (process %d) is master fam.", getpid());
}

void
Listener::accept_ugly_hack(int ugly, void *closure)
{
    Listener *listener = (Listener *) closure;
    assert(ugly == listener->_ugly_sock);
    
    //  Accept a new ugly connection.

    struct sockaddr_un sun;
    int sunlen = sizeof sun;
    int sock = accept(ugly, &sun, &sunlen);
    if (sock < 0)
    {   Log::perror("accept");
	return;
    }

    (void) Scheduler::install_read_handler(sock, read_ugly_hack, listener);
}

void
Listener::read_ugly_hack(int sock, void *closure)
{
    Listener *listener = (Listener *) closure;

    int rights[1] = { -1 };
    msghdr msg = { NULL, 0, NULL, 0, (caddr_t) rights, sizeof rights };
    if (recvmsg(sock, &msg, 0) >= 0)
    {
	Log::debug("master fam (process %d) has new slave", getpid());

	assert(rights[0] >= 0);
	listener->set_rendezvous_fd(rights[0]);

	//  Turn the inactivity timeout all the way down.  We want to
	//  clean up this dirty ugly hack as soon as possible.

	Activity::timeout(0);

#if 1
	// Forget socket -- it will be closed on exit.
	// Slave is blocked waiting for socket to close, so slave
	// will exit when master exits.
#else
  	// Allow slave fam to exit now.

  	(void) close(sock);
#endif
	(void) Scheduler::remove_read_handler(sock);
    }
    else
    {	Log::perror("recvmsg");
	(void) Scheduler::remove_read_handler(sock);
	(void) close(sock);
    }
}
