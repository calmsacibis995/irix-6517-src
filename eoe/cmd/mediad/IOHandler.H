#ifndef IOHandler_included
#define IOHandler_included

#include <sys/select.h>
#include "bool.H"

//  An IOHandler is a procedure that is called when I/O is possible on
//  a particular descriptor.  IOHandler itself is not instantiated;
//  instead one of the three subclasses, ReadHandler, WriteHandler or
//  ExceptHandler, is.
//  
//  To arrange for an IOHandler to be called, its descriptor must be
//  set using fd(), and it must be activated by calling activate().

class IOHandler {

protected:

    class Set {

    public:
	void         activate(IOHandler *);
	void         deactivate(IOHandler *);
	unsigned int nfds()		{ return _nfds; }
	fd_set      *fdset();
	void         run(fd_set *, int nfds);

    private:
	int _nfds;
	int _maxfds;
	IOHandler **_handlers;
	fd_set _fds;
	fd_set _xfds;			// fd_set for export

	void use_fd(int);
	void forget_fd(int);

    };

public:

    typedef void (*IOProc)(int fd, void *closure);

    IOHandler(IOHandler::Set&, IOProc, void *closure);
    virtual ~IOHandler();

    void activate();
    void deactivate();
    bool active() const			{ return _active && _fd >= 0; }

    void fd(int);
    int fd(void) const			{ return _fd; }

    void run()				{ (*_proc)(_fd, _closure); }

private:

    //  Instance Variables

    Set& _set;
    IOProc _proc;
    void *_closure;
    int _fd;
    bool _active;

    IOHandler(const IOHandler&);	// Do not copy
    void operator = (const IOHandler&);	//  or assign.

};

#define IOHANDLER_SUBCLASS(classname)					\
	class classname : public IOHandler {				\
	public:								\
	    classname(IOProc p, void *c): IOHandler(handlers, p, c) { }	\
	    static int nfds()		{ return handlers.nfds(); }	\
	    static fd_set *fdset()	{ return handlers.fdset(); }	\
	    static void run_handlers(fd_set *fds, int nfds)		\
					{ handlers.run(fds, nfds); }	\
	private:							\
	    static Set handlers;					\
	}

IOHANDLER_SUBCLASS(ReadHandler);
IOHANDLER_SUBCLASS(WriteHandler);
IOHANDLER_SUBCLASS(ExceptHandler);

#endif /* !IOHandler_included */
