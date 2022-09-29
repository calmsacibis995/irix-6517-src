#include "TCP_Client.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Cred.h"
#include "Event.h"
#include "Interest.h"
#include "Log.h"
#include "Scanner.h"

//////////////////////////////////////////////////////////////////////////////
//  Construction/destruction

TCP_Client::TCP_Client(int fd)
    : my_scanner(NULL),
      conn(fd, input_handler, unblock_handler, this)
{
    assert(fd >= 0);

    // Set client's name.

    char namebuf[20];
    sprintf(namebuf, "client %d", fd);
    name(namebuf);
    Log::debug("new connection from %s", name());
}

//////////////////////////////////////////////////////////////////////////////
//  Input

void
TCP_Client::input_handler(const char *msg, unsigned nbytes, void *closure)
{
    TCP_Client *client = (TCP_Client *) closure;
    if (msg)
	client->input_msg(msg, nbytes);
    else
    {   Log::debug("lost connection from %s", client->name());
	delete client;			// NULL msg means connection closed.
    }
}

void
TCP_Client::input_msg(const char *msg, int size)
{
    // Parse the first message.
    // The first message is:
    //	    opcode, request number, uid, gid, file name.

    char *p = (char *) msg;
    char opcode = *p++;
    Request reqnum = strtol(p, &p, 10);
    uid_t uid = strtol(p, &p, 10);
    gid_t gid = strtol(p, &p, 10);
    while (isascii(*p) && isspace(*p))
	p++;
    char filename[MAXPATHLEN];
    int i;
    for (i = 0; *p; i++)
    {   if (i >= MAXPATHLEN)
	{   Log::error("%s path name too long (%d chars)", name(), i);
	    return;
	}
	filename[i] = *p++;
    }
    filename[i] = '\0';
    if (i > 0 && filename[i - 1] == '\n')
	filename[i - 1] = '\0';		// strip the trailing newline
    p++;

    // Parse the second message, if any.
    // The second message is:
    //	    ngroups, group, group, group ...

    int ngroups =  1;
    gid_t grouplist[NGROUPS_UMAX] = { gid };
    if (p < msg + size - 1) {
        ngroups += strtol(p, &p, 10);
        char *q;
	for (int i = 1; i < ngroups; i++) {   
	    grouplist[i] = strtol(p, &q, 10);
	    if (p == q) {
                ngroups = i;
		break;
            }
            p = q;
        }
    }

    // Process the message.

    switch (opcode)
    {
    case 'W':				// Monitor File
	Log::debug("%s said: request %d monitor file \"%s\"",
		   name(), reqnum, filename);
	monitor_file(reqnum, filename, Cred(uid, ngroups, grouplist));
	break;

    case 'M':				// Monitor Directory
	Log::debug("%s said: request %d monitor dir \"%s\"",
		   name(), reqnum, filename);
	monitor_dir(reqnum, filename, Cred(uid, ngroups, grouplist));
	break;

    case 'C':				// Cancel
	Log::debug("%s said: cancel request %d", name(), reqnum);
	cancel(reqnum);
	break;

    case 'S':				// Suspend
	Log::debug("%s said: suspend request %d", name(), reqnum);
	MxClient::suspend(reqnum);
	break;

    case 'U':				// Resume
	Log::debug("%s said: resume request %d", name(), reqnum);
	MxClient::resume(reqnum);
	break;

    case 'N':				// Client Name
	Log::debug("%s said: %s is %s", name(), name(), filename);
	if (*filename && strcmp(filename, "test"))
	    name(filename);		// set this client's name
	break;

    case 'D':				// Log Debug
	Log::debug();
	break;

    case 'V':				// Log Verbose
	Log::info();
	break;

    case 'E':				// Log Error
	Log::error();
	break;

    default:
	Log::error("%s said unknown request '%c' ('\\%3o')",
		   name(), opcode, opcode & 0377);
	break;
    }
}

//////////////////////////////////////////////////////////////////////////////
//  Output

void
TCP_Client::unblock_handler(void *closure)
{
    TCP_Client *client = (TCP_Client *) closure;

    //  Continue scanner, if any.

    Scanner *scanner = client->my_scanner;
    if (scanner)
    {	if (scanner->done())
	    client->my_scanner = NULL;
	else
	    return;
    }

    //  After scanner has run, scan more interests.

    Interest *ip;
    while (client->ready_for_events() && (ip = client->to_be_scanned.first()))
    {   client->to_be_scanned.remove(ip);
	ip->scan();
    }

    //  Enable input if all enqueued work is done.

    if (client->ready_for_events())
	client->conn.ready_for_input(true);
}

Boolean
TCP_Client::ready_for_events()
{
    return conn.ready_for_output();
}

void
TCP_Client::enqueue_for_scan(Interest *ip)
{
    if (!to_be_scanned.size())
	conn.ready_for_input(false);
    to_be_scanned.insert(ip);
}

void
TCP_Client::dequeue_from_scan(Interest *ip)
{
    to_be_scanned.remove(ip);
    if (!to_be_scanned.size())
	conn.ready_for_input(true);
}

void
TCP_Client::enqueue_scanner(Scanner *sp)
{
    assert(!my_scanner);
    my_scanner = sp;
    conn.ready_for_input(false);
}

void
TCP_Client::post_event(const Event& event, Request request, const char *path)
{
    conn.send_event(event, request, path);
    Log::debug("sent event to %s: request %d \"%s\" %s",
	       name(), request, path, event.name());
}
