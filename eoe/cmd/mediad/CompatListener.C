#include "CompatListener.H"

#include <bstring.h>
#include <mntent.h>
#include <mediad.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Log.H"
#include "CompatServer.H"
#include "Scheduler.H"

const char CompatListener::MEDIAD_SOCKNAME[] = "/tmp/.mediad_socket";

CompatListener::CompatListener(const char *sock_path)
: _rendezvous_handler(accept_client, this)
{
    //  Register with portmapper.

    int sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
    {   Log::perror("can't create Unix domain socket for rendezvous");
	exit(1);
    }

    (void) unlink(sock_path);

    struct sockaddr_un addr;
    bzero(&addr, sizeof addr);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sock_path);
    if (bind(sock, (sockaddr *) &addr, sizeof addr) < 0)
    {   Log::perror("Unix domain bind");
	exit(1);
    }

    if (listen(sock, 5) < 0)
    {   Log::perror("Unix domain listen");
	exit(1);
    }

    _rendezvous_handler.fd(sock);
    _rendezvous_handler.activate();
    Log::debug("listening for compat clients on descriptor %d", sock);
}

CompatListener::~CompatListener()
{
    int fd = _rendezvous_handler.fd();
    if (fd >= 0)
    {   int rc = close(fd);
	if (rc < 0)
	    Log::perror("close rendezvous port(%d)", fd);
	else
	    Log::debug("compat service closed");
    }
}

void
CompatListener::accept_client(int rendezvous_fd, void *)
{
    // Get the new socket.

    struct sockaddr_un addr;
    int addrlen = sizeof addr;
    int client_fd = accept(rendezvous_fd,
			   (struct sockaddr *) &addr,
			   &addrlen);
    if (client_fd < 0)
	Log::perror("failed to accept new client");
    else
	(void) new CompatServer(client_fd);

    // Forget client.  Scheduler knows about it.
}
