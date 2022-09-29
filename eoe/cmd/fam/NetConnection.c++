#include "NetConnection.h"

#include <assert.h>
#include <bstring.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Log.h"
#include "Scheduler.h"

NetConnection::NetConnection(int a_fd,
			     char *inbuf, unsigned isize,
			     char *outbuf, unsigned osize,
			     UnblockHandler uhandler, void *uclosure)
    : fd(a_fd),
      iready(true), oready(true),
      ibuf(inbuf),		 obuf(outbuf),
      iend(inbuf + isize),	 oend(outbuf + osize),
      itail(inbuf),		 otail(outbuf),
      unblock_handler(uhandler), closure(uclosure)
{
    // Enable nonblocking output on socket.

    int yes = 1;
    if (ioctl(fd, FIONBIO, &yes) < 0)
    {   Log::perror("can't set NBIO on fd %d", fd);
	shutdown(false);
	return;
    }

#ifdef TINY_BUFFERS			/* This kills throughput. */

    // Reduce kernel's send and receive buffer sizes.  Useful for debugging
    // flow control.

    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &osize, sizeof osize))
	Log::perror("setsockopt(%d, SO_SNDBUF)", fd);
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &isize, sizeof isize))
	Log::perror("setsockopt(%d, SO_RCVBUF)", fd);

#endif /* !TINY_BUFFERS */

    // Wait for something to read.

    (void) Scheduler::install_read_handler(fd, read_handler, this);
}

NetConnection::~NetConnection()
{
    shutdown(false);
}

void
NetConnection::shutdown(Boolean call_input)
{
    if (fd >= 0)
    {
	Scheduler::IOHandler oldh;
	if (iready && oready)
	{   oldh = Scheduler::remove_read_handler(fd);
	    assert(oldh == read_handler);
	}
	if (!oready)
	{   oldh = Scheduler::remove_write_handler(fd);
	    assert(oldh == write_handler);
	}
	(void) close(fd);
	fd = -1;
	oready = true;
	if (call_input)
	    input_msg(NULL, 0);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Input

void
NetConnection::read_handler(int fd, void *closure)
{
    NetConnection *connection = (NetConnection *) closure;
    assert(fd == connection->fd);
    connection->input();
}

void
NetConnection::input()
{
    if (fd < 0)
	return;

    // Read from socket.

    assert(itail < iend);
    int maxbytes = iend - itail;
    int ret = recv(fd, itail, maxbytes, 0);
    if (ret < 0)
    {   if (errno != EAGAIN && errno != ECONNRESET)
	{   Log::perror("fd %d read error", fd);
	    shutdown();
	}
	return;
    }
    else if (ret == 0)
    {   shutdown();
	return;
    }
    else // (ret > 0)
    {
	itail += ret;
    }

    deliver_input();
}

void
NetConnection::deliver_input()
{
    // Find messages and process them.

    char *ihead = ibuf;
    while (iready && oready && ihead + sizeof (Length) <= itail)
    {   assert(sizeof (Length) == 4);
	Length len = (ihead[0] << 24 | ihead[1] << 16 |
		      ihead[2] <<  8 | ihead[3] <<  0);

	if (len > iend - ibuf)
	{   Log::error("fd %d message length %d bytes exceeds max of %d.",
		       fd, len, iend - ibuf);
	    shutdown();
	    break;
	}
	if (ihead + sizeof (Length) + len > itail)
	    break;

	// Call the message reader.

	input_msg(ihead + sizeof (Length), len);

	ihead += sizeof (Length) + len;
    }

    // If data remain in buffer, slide them to the left.

    assert(ihead <= itail);
    int remaining = itail - ihead;
    if (remaining && ihead != ibuf)
	bcopy(ihead, ibuf, remaining);
    itail = ibuf + remaining;
    assert(itail < iend);
}

void
NetConnection::ready_for_input(Boolean tf)
{
    set_handlers(tf, oready);
}

///////////////////////////////////////////////////////////////////////////////
//  Output

void
NetConnection::mprintf(const char *format, ...)
{
    if (fd < 0)
	return;				// if closed, do nothing.

    va_list args;
    va_start(args, format);
    char *msg = otail + sizeof (Length);
    Length len = vsprintf(msg, format, args) + 1;
    va_end(args);

    assert(len > 0);
    assert(msg + len <= oend);
    assert(sizeof (Length) == 4);
    otail[0] = len >> 24;
    otail[1] = len >> 16;
    otail[2] = len >> 8;
    otail[3] = len >> 0;
    otail = msg + len;
    flush();
}

void
NetConnection::flush()
{
    Boolean allgone = true;
    unsigned nbytes = otail - obuf;
    if (nbytes)
    {
	int ret = send(fd, obuf, nbytes, 0);
	if (ret >= 0)
	{   nbytes -= ret;
	    if (nbytes)
	    {   if (ret)
		    bcopy(obuf + ret, obuf, nbytes);
		allgone = false;
	    }
	    otail -= ret;
	}
	else if (errno == EWOULDBLOCK)
	    allgone = false;
	else
	{   Log::info("fd %d write error: %m", fd);
	    otail = obuf;		// throw it away
	}
    }
    set_handlers(iready, allgone);
}

void
NetConnection::set_handlers(Boolean new_iready, Boolean new_oready)
{
    Scheduler::IOHandler oldh;
    Boolean call_unblock = false;

    if (oready != new_oready)
    {   if (new_oready)
	{   oldh = Scheduler::remove_write_handler(fd);
	    assert(oldh == write_handler);
	    call_unblock = true;
	}
	else
	{   oldh = Scheduler::install_write_handler(fd, write_handler, this);
	    assert(oldh == NULL);
	}
    }

    //  Install read_handler iff iready && oready.

    if ((iready && oready) != (new_iready && new_oready))
    {
	if (new_iready && new_oready)
	{   oldh = Scheduler::install_read_handler(fd, read_handler, this);
	    assert(oldh == NULL);
	}
	else
	{   oldh = Scheduler::remove_read_handler(fd);
	    assert(oldh == read_handler);
	}
    }
    Boolean old_iready = iready;
    iready = new_iready;
    oready = new_oready;

    //  If we unblocked output, call the unblock handler.
    //  If there's input to deliver, deliver it.

    if (call_unblock && unblock_handler)
    {   assert(!iready || old_iready);
	(*unblock_handler)(closure);
    }
    else if (iready && !old_iready)
	deliver_input();
}

void
NetConnection::write_handler(int fd, void *closure)
{
    NetConnection *connection = (NetConnection *) closure;
    assert(connection->fd == fd);
    connection->flush();
}
