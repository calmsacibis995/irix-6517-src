#include <stdio.h>
#include "Client.h"
#include "errno.h"
#include "osfcn.h"
extern int errno;
extern "C" {
#include <sys/syslog.h>
   void syslog( int, char*);
};



// DoClientIn()
//
// Reads from the server's socket and updates the input buffer and it's
// pointers.
//
// Returns:
//   DoClientIn returns zero if no errors occured, or -1 if
// an error occured.  If an error occurs, the external variable, int errno,
// will indicate the type of the error as described below.  (See also
// errno(3c).
//
// [EINTR]    A signal occured during the read.
// [EBADF]    The client's socket is not open.


int Client::DoClientIn(ServerEntry *server)
{
	long	msglen;
	int	stat;
	char	*p, *q;


	// If the buffer is completely full, just return.

	if (server->inbuf_full)
		return(0);

	// Shift the buffer to the left.

	if (server->instart != server->inbuf) {
		for (p = server->instart, q = server->inbuf;
                     p < server->inend; *q++ = *p++)
			/* Null Statement */;
		server->instart = server->inbuf;
		server->inend = q;
	}

	// Read as much as will fit into the buffer.

	stat = read(server->sock, server->inend,
                    &(server->inbuf[MSGBUFSIZ]) - server->inend);

	// Debuggging
#if 0
	char sysmsg[200], bufCopy[200];
	int i, len = stat;
	if (len > 100) len = 100;
	if (len < 0) len = 0;
	for (i=4; i<len; i++) bufCopy[i-4] = server->inend[i];
	bufCopy[i] = 0;
	sprintf(sysmsg, "DoClientIn - Read (len %d) (data %s)", stat, bufCopy);
        syslog(LOG_ALERT, sysmsg);
#endif

	// Handle read failure.

	if (stat == -1)
		return(-1);

	// Handle broken connection.

	if (stat == 0) {
		server->connection_closed = 1;
		return(0);
	}

	// Update pointer to end of buffer.

	server->inend += stat;

	// Determine if the buffer is now completely full.

	if (server->inend - server->inbuf >= MSGBUFSIZ)
		server->inbuf_full = 1;
	else
		server->inbuf_full = 0;

	// Determine if we have a full message yet.

	if (server->inend - server->instart < sizeof(long)) {
		server->inbuf_has_msg = 0;
	} else {
		getword(server->instart, &msglen);
		if (server->inend - server->instart >= msglen + sizeof(long)) {
			server->inbuf_has_msg = 1;
		} else
		 {
			server->inbuf_has_msg = 0;
		}
	}

	return(0);
}
