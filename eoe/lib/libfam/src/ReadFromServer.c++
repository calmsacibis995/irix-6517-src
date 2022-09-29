#include "Client.h"
#include <stdio.h>
#include <errno.h>
#include "libc.h"
extern int errno;

// ReadFromServer(sock,msg,nbytes)
//
// ReadFromServer reads one message from the server who's socket number
// is sock.  ReadFromServer will block if no message is available to
// be read.  You may use the ClientSelect call to determine if there is
// a message available for ReadFromServer to read.
//
// The socket number of the server.
// A pointer to an array of characters in which to return the message.
// The maximum number of bytes in the message.  If the message contains more
//   bytes than nbytes, it will be truncated to nbytes.
//
// Returns:
//   ReadFromServer returns the number of bytes in the message, or -1 if
// an error occurs.  If an error occurs, the external variable, int errno,
// will indicate the type of the error as described below.  (See also
// errno(3c).
//
// [ENOENT]   If the socket is not in the client list.
// [EBADF]    If the socket is not open.
// [EINTR]    If the read was interupted by a signal.
// [ENOLINK]  If the client closed the connection.


int Client::ReadFromServer(int sock,char *msg,int nbytes)
{
	ServerEntry *server;
	int	i, n;
	long	nextmsglen, msglen;

	// Find the ServerEntry struct for the socket.

	for (server = Slist; server; server = server->next)
		if (server->sock == sock)
			break;

	// If there is no Server struct for the socket, return an error.

	if (server == 0) {
		errno = ENOENT;
		return(-1);
	}

	// If there is no message in the buffer, try to get one from
        // ClientSelect 

	if (!server->inbuf_has_msg) {
	    DoClientIn(server);
#if 0
	fd_set t_rdfds;
		FD_ZERO(&t_rdfds);
		FD_SET(sock, &t_rdfds);
		stat = ClientSelect(FD_SETSIZE, &t_rdfds, 0, 0, 0);
		if (stat == -1) {
			/* At this point, errno==EBADF | errno==EINTR */
			return(-1);
		}
#endif
	}

	// Figure out the length of the message.

	if (server->inend - server->instart >= sizeof(long)) {
		getword(server->instart, &msglen);
	} else {
		msglen = 0;
	}

	// If there is no message, the connection must be gone!

	if (server->inend - server->instart < msglen + sizeof(long)) {
		errno = ENOLINK;
		return(-1);
	}

	// Transfer the message to the caller.

	n = (msglen < nbytes) ? msglen : nbytes; // Figure out how much to xfer
	server->instart += 4;                    // Skip over count.
	for (i = 0; i < n; i++)
		msg[i] = server->instart[i];     // Transfer.
	server->instart += msglen;               // Skip over message.

	// Determine if there is still a message in the buffer.

	if (server->inend - server->instart >= sizeof(long)) {
		getword(server->instart, &nextmsglen);
		if (server->inend - server->instart >=
                    nextmsglen + sizeof(long)) {
			server->inbuf_has_msg = 1;
		} else {
			server->inbuf_has_msg = 0;
		}
	} else {
		server->inbuf_has_msg = 0;
	}

	// The buffer is no longer full.

	server->inbuf_full = 0;

	// Return the length of the message.

	return(msglen);
}
