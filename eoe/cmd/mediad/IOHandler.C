#include "IOHandler.H"

#include <assert.h>
#include <stdlib.h>
#include <sys/select.h>

IOHandler::Set   ReadHandler::handlers;
IOHandler::Set  WriteHandler::handlers;
IOHandler::Set ExceptHandler::handlers;

IOHandler::IOHandler(Set& set, IOProc proc, void *closure)
: _set(set), _proc(proc), _closure(closure), _fd(-1), _active(false)
{ }

IOHandler::~IOHandler()
{
    deactivate();
}

void
IOHandler::activate()
{
    if (!_active)
    {   _active = true;
	if (_fd >= 0)
	    _set.activate(this);
    }
}

void
IOHandler::deactivate()
{
    if (_active)
    {   _active = false;
	if (_fd >= 0)
	    _set.deactivate(this);
    }
}

void
IOHandler::fd(int fd)
{
    if (_fd >= 0 && _active)
	_set.deactivate(this);
    _fd = fd;
    if (_fd >= 0 && _active)
	_set.activate(this);
}

///////////////////////////////////////////////////////////////////////////////

void
IOHandler::Set::activate(IOHandler *ioh)
{
    assert(ioh);
    int fd = ioh->fd();
    if (fd >= 0)
    {   assert(fd < FD_SETSIZE);
	use_fd(fd);
	FD_SET(fd, &_fds);
	_handlers[fd] = ioh;
    }
}

void
IOHandler::Set::deactivate(IOHandler *ioh)
{
    assert(ioh);
    int fd = ioh->fd();
    if (fd >= 0)
    {   assert(fd < FD_SETSIZE);
	FD_CLR(fd, &_fds);
	_handlers[fd] = NULL;
	forget_fd(fd);
    }
}

fd_set *
IOHandler::Set::fdset()
{
    if (_nfds == 0)
	return NULL;
    _xfds = _fds;
    return &_xfds;
}

void
IOHandler::Set::run(fd_set *fds, int nfds)
{
    if (fds)
	for (int fd = 0; fd < nfds; fd++)
	    if (FD_ISSET(fd, fds))
	    {   IOHandler *ioh = _handlers[fd];
		assert(ioh);
		if (ioh)
		    ioh->run();
	    }
}

// use_fd() is called internally when an fd is about to be used.
// It verifies that nfds is big enough, and that _handlers has
// enough entries allocated.

void
IOHandler::Set::use_fd(int fd)
{
    if (fd >= _nfds)
    {   _nfds = fd + 1;
	if (_nfds > _maxfds)
	{
	    typedef IOHandler *IOHandlerPtr;
	    IOHandlerPtr *new_handlers = new IOHandlerPtr[_nfds];
	    int i;

	    for (i = 0; i < _maxfds; i++)
		new_handlers[i] = _handlers[i];
	    for ( ; i < _nfds; i++)
		new_handlers[i] = NULL;
	    delete _handlers;
	    _handlers = new_handlers;
	    _maxfds = _nfds;
	}
    }
}

void
IOHandler::Set::forget_fd(int fd)
{
    if (fd == _nfds - 1)
	while (_nfds > 0 && _handlers[_nfds - 1] == NULL)
	   _nfds--;
}

